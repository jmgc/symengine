/**
 *  \file uintpoly_piranha.h
 *  Class for Polynomial: UIntPolyPiranha
 **/
#ifndef SYMENGINE_UINTPOLY_PIRANHA_H
#define SYMENGINE_UINTPOLY_PIRANHA_H

#include <symengine/polys/upolybase.h>
#include <symengine/expression.h>
#include <symengine/dict.h>
#include <memory>

#ifdef HAVE_SYMENGINE_PIRANHA
#include <piranha/monomial.hpp>
#include <piranha/polynomial.hpp>
#include <piranha/mp_rational.hpp>
#include <piranha/mp_integer.hpp>
#include <piranha/math.hpp>
#include <piranha/type_traits.hpp>

#if SYMENGINE_INTEGER_CLASS != SYMENGINE_PIRANHA
namespace piranha
{

// overloading pow for pirahna::math::evaluate
namespace math
{

template <typename U>
struct pow_impl<SymEngine::integer_class, U,
                SymEngine::enable_if_t<std::is_integral<U>::value>> {
    template <typename T2>
    SymEngine::integer_class operator()(const SymEngine::integer_class &r,
                                        const T2 &x) const
    {
        SymEngine::integer_class res;
        mp_pow_ui(res, r, x);
        return res;
    }
};

template <>
struct gcd_impl<SymEngine::integer_class, SymEngine::integer_class> {
    SymEngine::integer_class operator()(const SymEngine::integer_class &r,
                                        const SymEngine::integer_class &x) const
    {
        SymEngine::integer_class res;
        mp_gcd(res, r, x);
        return res;
    }
};

template <>
struct divexact_impl<SymEngine::integer_class> {
    void operator()(SymEngine::integer_class &r,
                    const SymEngine::integer_class &x,
                    const SymEngine::integer_class &y) const
    {
        SymEngine::integer_class rem;
        mp_tdiv_qr(r, rem, x, y);
        if (rem != SymEngine::integer_class(0)) {
            piranha_throw(inexact_division);
        }
    }
};
}

template <>
struct has_exact_ring_operations<SymEngine::integer_class> {
    static const bool value = true;
};
}
#endif

namespace SymEngine
{
using pmonomial = piranha::monomial<unsigned int>;
using pintpoly = piranha::polynomial<integer_class, pmonomial>;
using pratpoly = piranha::polynomial<rational_class, pmonomial>;

template<typename C>
class PiranhaForIter
{
    pintpoly::container_type::const_iterator ptr_;

public:
    PiranhaForIter(pintpoly::container_type::const_iterator ptr) : ptr_{ptr}
    {
    }

    bool operator==(const PiranhaForIter &rhs)
    {
        return (ptr_ == rhs.ptr_);
    }

    bool operator!=(const PiranhaForIter &rhs)
    {
        return (ptr_ != rhs.ptr_);
    }

    PiranhaForIter operator++()
    {
        ptr_++;
        return *this;
    }

    std::pair<unsigned int, const C &> operator*()
    {
        return std::make_pair(*(ptr_->m_key.begin()), ptr_->m_cf);
    }

    std::shared_ptr<std::pair<unsigned int, const C &>> operator->()
    {
        return std::make_shared<std::pair<unsigned int, const C &>>(
            *(ptr_->m_key.begin()), ptr_->m_cf);
    }
};

template <typename D, template <typename X, typename Y> class BaseType,
          typename P>
class UPiranhaPoly : public BaseType<D, P>
{
public:
    using C = typename BaseType<D, P>::coef_type;
    using term = typename D::term_type;

    UPiranhaPoly(const RCP<const Basic> &var, D &&dict)
        : BaseType<D, P>(var, std::move(dict))
    {
    }

    int compare(const Basic &o) const
    {
        SYMENGINE_ASSERT(is_a<P>(o))
        const P &s = static_cast<const P &>(o);
        int cmp = this->var_->compare(*s.var_);
        if (cmp != 0)
            return cmp;
        if (this->poly_ == s.poly_)
            return 0;
        return (this->poly_.hash() < s.poly_.hash()) ? -1 : 1;
    }

    static D cont_from_dict(const RCP<const Basic> &var,
                            std::map<unsigned, C> &&d)
    {
        D p;
        piranha::symbol_set ss({{piranha::symbol(detail::poly_print(var))}});
        p.set_symbol_set(ss);
        for (auto &it : d)
            p.insert(term(it.second, pmonomial{it.first}));

        return std::move(p);
    }

    static RCP<const P> from_vec(const RCP<const Basic> &var,
                                 const std::vector<C> &v)
    {
        D p;
        piranha::symbol_set ss({{piranha::symbol(detail::poly_print(var))}});
        p.set_symbol_set(ss);
        for (unsigned int i = 0; i < v.size(); i++) {
            if (v[i] != 0) {
                p.insert(term(v[i], pmonomial{i}));
            }
        }
        return make_rcp<const P>(var, std::move(p));
    }

    C eval(const C &x) const
    {
        const std::unordered_map<std::string, C> t = {{detail::poly_print(this->var_), x}};
        return piranha::math::evaluate<C, D>(this->poly_, t);
    }

    std::vector<C> multieval(const std::vector<C> &v) const
    {
        std::vector<C> res(v.size());
        for (unsigned int i = 0; i < v.size(); ++i)
            res[i] = eval(v[i]);
        return res;
    }

    C get_coeff(unsigned int x) const
    {
        return this->poly_.find_cf(pmonomial{x});
    }

    const C &get_coeff_ref(unsigned int x) const
    {
        static C PZERO(0);

        term temp = term(0, pmonomial{x});
        auto it = this->poly_._container().find(temp);
        if (it == this->poly_._container().end())
            return PZERO;
        return it->m_cf;
    }

    unsigned int size() const
    {
        if (this->poly_.size() == 0)
            return 0;
        return this->get_degree() + 1;
    }

    // begin() and end() are unordered
    // obegin() and oend() are ordered, from highest degree to lowest
    typedef PiranhaForIter<C> iterator;
    typedef ContainerRevIter<P, const C &> r_iterator;
    iterator begin() const
    {
        return iterator(this->poly_._container().begin());
    }
    iterator end() const
    {
        return iterator(this->poly_._container().end());
    }
    r_iterator obegin() const
    {
        return r_iterator(this->template rcp_from_this_cast<P>(),
                          (long)size() - 1);
    }
    r_iterator oend() const
    {
        return r_iterator(this->template rcp_from_this_cast<P>(), -1);
    }
};

class UIntPolyPiranha
    : public UPiranhaPoly<pintpoly, UIntPolyBase, UIntPolyPiranha>
{
public:
    IMPLEMENT_TYPEID(UINTPOLYPIRANHA)
    //! Constructor of UIntPolyPiranha class
    UIntPolyPiranha(const RCP<const Basic> &var, pintpoly &&dict);
    //! \return size of the hash
    std::size_t __hash__() const;

}; // UIntPolyPiranha

class URatPolyPiranha : public UPiranhaPoly<pratpoly, URatPolyBase, URatPolyPiranha>
{    
public:
    IMPLEMENT_TYPEID(URATPOLYPIRANHA)
    //! Constructor of UIntPolyPiranha class
    URatPolyPiranha(const RCP<const Basic> &var, pratpoly &&dict);
    //! \return size of the hash
    std::size_t __hash__() const;

};

inline RCP<const UIntPolyPiranha> gcd_upoly(const UIntPolyPiranha &a,
                                            const UIntPolyPiranha &b)
{
    if (!(a.get_var()->__eq__(*b.get_var())))
        throw std::runtime_error("Error: variables must agree.");

    pintpoly gcdx(std::get<0>(pintpoly::gcd(a.get_poly(), b.get_poly())));
    // following the convention, that leading coefficient should be positive
    if (gcdx.find_cf(pmonomial{gcdx.degree()}) < 0)
        piranha::math::negate(gcdx);
    return make_rcp<const UIntPolyPiranha>(a.get_var(), std::move(gcdx));
}

inline RCP<const UIntPolyPiranha> lcm_upoly(const UIntPolyPiranha &a,
                                            const UIntPolyPiranha &b)
{
    if (!(a.get_var()->__eq__(*b.get_var())))
        throw std::runtime_error("Error: variables must agree.");

    pintpoly gcdx(std::get<0>(pintpoly::gcd(a.get_poly(), b.get_poly())));
    if (gcdx.find_cf(pmonomial{gcdx.degree()}) < 0)
        piranha::math::negate(gcdx);
    pintpoly mulx(a.get_poly() * b.get_poly());
    return make_rcp<const UIntPolyPiranha>(
        a.get_var(), std::move(pintpoly::udivrem(mulx, gcdx)).first);
}

inline RCP<const UIntPolyPiranha> pow_upoly(const UIntPolyPiranha &a,
                                            unsigned int p)
{
    return make_rcp<const UIntPolyPiranha>(
        a.get_var(), std::move(piranha::math::pow(a.get_poly(), p)));
}

inline bool divides_upoly(const UIntPolyPiranha &a, const UIntPolyPiranha &b,
                          const Ptr<RCP<const UIntPolyPiranha>> &res)
{
    if (!(a.get_var()->__eq__(*b.get_var())))
        throw std::runtime_error("Error: variables must agree.");

    try {
        pintpoly z;
        piranha::math::divexact(z, b.get_poly(), a.get_poly());
        *res = UIntPolyPiranha::from_container(a.get_var(), std::move(z));
        return true;
    } catch (const piranha::math::inexact_division &) {
        return false;
    }
}
}

#endif // HAVE_SYMENGINE_PIRANHA

#endif // SYMENGINE_UINTPOLY_PIRANHA_H
