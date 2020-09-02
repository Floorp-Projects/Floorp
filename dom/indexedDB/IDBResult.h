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
using FailureType = SpecialConstant<IDBSpecialValue::Failure>;
using InvalidType = SpecialConstant<IDBSpecialValue::Invalid>;
struct ExceptionType final {};
struct VoidType final {};
}  // namespace detail

template <typename T>
constexpr inline detail::OkType<std::remove_reference_t<T>> Ok(T&& aValue) {
  return {std::forward<T>(aValue)};
}

constexpr inline detail::OkType<void> Ok() { return {}; }

// Put these in a subnamespace to avoid conflicts from the combination of 1.
// using namespace mozilla::dom::indexedDB; in cpp files, 2. the unified build
// and 3. mozilla::dom::Exception
namespace SpecialValues {
constexpr const detail::FailureType Failure;
constexpr const detail::InvalidType Invalid;
constexpr const detail::ExceptionType Exception;
}  // namespace SpecialValues

template <typename T, IDBSpecialValue... S>
class MOZ_MUST_USE_TYPE IDBResult;

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
  static_assert(IsSortedSet<S...>::value,
                "special value list must be sorted and unique");

  template <typename R, IDBSpecialValue... U>
  friend class IDBResultBase;

 protected:
  using ValueType = OkType<T>;

 public:
  // Construct a normal result. Use the Ok function to create an object of type
  // ValueType.
  MOZ_IMPLICIT IDBResultBase(const ValueType& aValue) : mVariant(aValue) {}
  MOZ_IMPLICIT IDBResultBase(ValueType&& aValue)
      : mVariant(std::move(aValue)) {}

  MOZ_IMPLICIT IDBResultBase(ExceptionType, ErrorResult&& aErrorResult)
      : mVariant(std::move(aErrorResult)) {}

  template <IDBSpecialValue Special>
  MOZ_IMPLICIT IDBResultBase(SpecialConstant<Special>)
      : mVariant(SpecialConstant<Special>{}) {}

  IDBResultBase(IDBResultBase&&) = default;
  IDBResultBase& operator=(IDBResultBase&&) = default;

  // Construct an IDBResult from another IDBResult whose set of possible special
  // values is a subset of this one's.
  template <IDBSpecialValue... U>
  MOZ_IMPLICIT IDBResultBase(IDBResultBase<T, U...>&& aOther)
      : mVariant(aOther.mVariant.match(
            [](auto& aVariant) { return VariantType{std::move(aVariant)}; })) {}

  // Test whether the result is a normal return value. The choice of the first
  // parameter's type makes it possible to write `result.Is(Ok, rv)`, promoting
  // readability and uniformity with other functions in the overload set.
  bool Is(OkType<void> (*)()) const {
    return mVariant.template is<ValueType>();
  }

  bool Is(ExceptionType) const { return mVariant.template is<ErrorResult>(); }

  template <IDBSpecialValue Special>
  bool Is(SpecialConstant<Special>) const {
    return mVariant.template is<SpecialConstant<Special>>();
  }

  ErrorResult& AsException() { return mVariant.template as<ErrorResult>(); }

  template <typename... SpecialValueMappers>
  ErrorResult ExtractErrorResult(SpecialValueMappers... aSpecialValueMappers) {
    return mVariant.match(
        [](const ValueType&) -> ErrorResult {
          MOZ_CRASH("non-value expected");
        },
        [](ErrorResult& aException) { return std::move(aException); },
        aSpecialValueMappers...);
  }

  template <typename NewValueType>
  auto PropagateNotOk();

 protected:
  using VariantType = Variant<ValueType, ErrorResult, SpecialConstant<S>...>;

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

  // Move the regular return value, asserting that this object
  // is indeed a regular return value.
  T Unwrap() {
    return std::move(
        this->mVariant.template as<typename IDBResult::ValueType>().mValue);
  }

  // Get a reference to the regular return value, asserting that this object
  // is indeed a regular return value.
  const T& Inspect() const {
    return this->mVariant.template as<typename IDBResult::ValueType>().mValue;
  }
};

template <IDBSpecialValue... S>
class MOZ_MUST_USE_TYPE IDBResult<void, S...>
    : public detail::IDBResultBase<void, S...> {
 public:
  using IDBResult::IDBResultBase::IDBResultBase;
};

template <nsresult E>
ErrorResult InvalidMapsTo(const indexedDB::detail::InvalidType&) {
  return ErrorResult{E};
}

namespace detail {
template <typename T, IDBSpecialValue... S>
template <typename NewValueType>
auto IDBResultBase<T, S...>::PropagateNotOk() {
  using ResultType = IDBResult<NewValueType, S...>;
  MOZ_ASSERT(!Is(Ok));

  return mVariant.match(
#if defined(__clang__) || (defined(__GNUC__) && __GNUC__ >= 8)
      [](const ValueType&) -> ResultType { MOZ_CRASH("non-value expected"); },
      [](ErrorResult& aException) -> ResultType {
        return {SpecialValues::Exception, std::move(aException)};
      },
      [](SpecialConstant<S> aSpecialValue) -> ResultType {
        return aSpecialValue;
      }...
#else
      // gcc 7 doesn't accept the kind of parameter pack expansion above,
      // probably due to https://gcc.gnu.org/bugzilla/show_bug.cgi?id=47226
      [](auto& aParam) -> ResultType {
        if constexpr (std::is_same_v<ValueType&, decltype(aParam)>) {
          MOZ_CRASH("non-value expected");
        } else if constexpr (std::is_same_v<ErrorResult&, decltype(aParam)>) {
          return {SpecialValues::Exception, std::move(aParam)};
        } else {
          return aParam;
        }
      }
#endif
  );
}

}  // namespace detail

}  // namespace indexedDB
}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_indexeddb_idbresult_h__
