/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_idbresult_h__
#define mozilla_dom_indexeddb_idbresult_h__

#include <mozilla/ErrorResult.h>
#include <mozilla/Variant.h>

#include <type_traits>
#include <utility>

namespace mozilla {
namespace dom {
namespace indexedDB {

// IDBSpecialValue represents two special return values, distinct from any other
// value, used in several places in the IndexedDB spec.
enum class IDBSpecialValue {
  Failure,
  Invalid,
};

namespace detail {
template <typename T>
struct OkType final {
  T mValue;
};

template <>
struct OkType<void> final {};

template <IDBSpecialValue Value>
using SpecialConstant = std::integral_constant<IDBSpecialValue, Value>;
using FailureType = detail::SpecialConstant<IDBSpecialValue::Failure>;
using InvalidType = detail::SpecialConstant<IDBSpecialValue::Invalid>;
struct ExceptionType final {};
struct VoidType final {};
}  // namespace detail

namespace {
template <typename T>
constexpr inline detail::OkType<std::remove_reference_t<T>> Ok(T&& aValue) {
  return {std::forward<T>(aValue)};
}

constexpr inline detail::OkType<void> Ok() { return {}; }

constexpr const detail::FailureType Failure;
constexpr const detail::InvalidType Invalid;
constexpr const detail::ExceptionType Exception;
}  // namespace

namespace detail {
template <IDBSpecialValue... Elements>
struct IsSortedSet;

template <IDBSpecialValue First, IDBSpecialValue Second,
          IDBSpecialValue... Rest>
struct IsSortedSet<First, Second, Rest...>
    : std::integral_constant<bool, IsSortedSet<First, Second>::value &&
                                       IsSortedSet<Second, Rest...>::value> {};

template <IDBSpecialValue First, IDBSpecialValue Second>
struct IsSortedSet<First, Second>
    : std::integral_constant<bool, (First < Second)> {};

template <IDBSpecialValue First>
struct IsSortedSet<First> : std::true_type {};

template <>
struct IsSortedSet<> : std::true_type {};

// IDBResultBase contains the bulk of the implementation of IDBResult, namely
// functionality that's applicable to all values of T.
template <typename T, IDBSpecialValue... S>
class IDBResultBase {
  // This assertion ensures that permutations of the set of possible special
  // values don't create distinct types.
  static_assert(detail::IsSortedSet<S...>::value,
                "special value list must be sorted and unique");

  template <typename R, IDBSpecialValue... U>
  friend class IDBResultBase;

 protected:
  using ValueType = detail::OkType<T>;

 public:
  // Construct a normal result. Use the Ok function to create an object of type
  // ValueType.
  MOZ_IMPLICIT IDBResultBase(const ValueType& aValue) : mVariant(aValue) {}

  MOZ_IMPLICIT IDBResultBase(detail::ExceptionType)
      : mVariant(detail::ExceptionType{}) {}

  template <IDBSpecialValue Special>
  MOZ_IMPLICIT IDBResultBase(detail::SpecialConstant<Special>)
      : mVariant(detail::SpecialConstant<Special>{}) {}

  // Construct an IDBResult from another IDBResult whose set of possible special
  // values is a subset of this one's.
  template <IDBSpecialValue... U>
  MOZ_IMPLICIT IDBResultBase(const IDBResultBase<T, U...>& aOther)
      : mVariant(aOther.mVariant.match(
            [](auto& aVariant) { return VariantType{aVariant}; })) {}

  // Test whether the result is a normal return value. The choice of the first
  // parameter's type makes it possible to write `result.Is(Ok, rv)`, promoting
  // readability and uniformity with other functions in the overload set.
  bool Is(detail::OkType<void> (*)(), const ErrorResult& aRv) const {
    AssertConsistency(aRv);
    return mVariant.template is<ValueType>();
  }

  bool Is(detail::ExceptionType, const ErrorResult& aRv) const {
    AssertConsistency(aRv);
    return mVariant.template is<detail::ExceptionType>();
  }

  template <IDBSpecialValue Special>
  bool Is(detail::SpecialConstant<Special>, const ErrorResult& aRv) const {
    AssertConsistency(aRv);
    return mVariant.template is<detail::SpecialConstant<Special>>();
  }

 protected:
  void AssertConsistency(const ErrorResult& aRv) const {
    MOZ_ASSERT(aRv.Failed() == mVariant.template is<detail::ExceptionType>());
  }

  using VariantType =
      Variant<ValueType, detail::ExceptionType, detail::SpecialConstant<S>...>;

  VariantType mVariant;
};
}  // namespace detail

// Represents a return value of an IndexedDB algorithm. T is the type of the
// regular return value, while S is a list of special values that can be
// returned by the particular algorithm.
template <typename T, IDBSpecialValue... S>
class MOZ_MUST_USE_TYPE IDBResult : public detail::IDBResultBase<T, S...> {
 public:
  using IDBResult::IDBResultBase::IDBResultBase;

  // Get a reference to the regular return value, asserting that this object
  // is indeed a regular return value.
  T& Unwrap(const ErrorResult& aRv) {
    return const_cast<T&>(static_cast<const IDBResult*>(this)->Unwrap(aRv));
  }

  const T& Unwrap(const ErrorResult& aRv) const {
    this->AssertConsistency(aRv);
    return this->mVariant.template as<typename IDBResult::ValueType>().mValue;
  }
};

template <IDBSpecialValue... S>
class MOZ_MUST_USE_TYPE IDBResult<void, S...>
    : public detail::IDBResultBase<void, S...> {
 public:
  using IDBResult::IDBResultBase::IDBResultBase;
};

}  // namespace indexedDB
}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_indexeddb_idbresult_h__
