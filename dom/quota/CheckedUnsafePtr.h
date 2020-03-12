/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Diagnostic class template that helps finding dangling pointers.

#ifndef mozilla_CheckedUnsafePtr_h
#define mozilla_CheckedUnsafePtr_h

#include "mozilla/DataMutex.h"
#include "nsTArray.h"

#include <cstddef>
#include <type_traits>
#include <utility>

namespace mozilla {
enum class CheckingSupport {
  Enabled,
  Disabled,
};

namespace detail {
template <CheckingSupport>
class CheckedUnsafePtrBase;

struct CheckedUnsafePtrCheckData {
  using Data = nsTArray<CheckedUnsafePtrBase<CheckingSupport::Enabled>*>;

  DataMutex<Data> mPtrs{"mozilla::SupportsCheckedUnsafePtr"};
};

template <>
class CheckedUnsafePtrBase<CheckingSupport::Disabled> {
 protected:
  template <typename Ptr>
  void CopyDanglingFlagIfAvailableFrom(const Ptr&) {}

  template <typename Ptr, typename F>
  void WithCheckedUnsafePtrsImpl(Ptr, F&&) {}
};

template <>
class CheckedUnsafePtrBase<CheckingSupport::Enabled> {
  friend class CheckedUnsafePtrBaseAccess;

 protected:
  CheckedUnsafePtrBase() = default;
  CheckedUnsafePtrBase(const CheckedUnsafePtrBase& aOther) = default;

  // When copying an CheckedUnsafePtr, its mIsDangling member must be copied as
  // well; otherwise the new copy might try to dereference a dangling pointer
  // when destructed.
  void CopyDanglingFlagIfAvailableFrom(
      const CheckedUnsafePtrBase<CheckingSupport::Enabled>& aOther) {
    mIsDangling = aOther.mIsDangling;
  }

  template <typename Ptr>
  using DisableForCheckedUnsafePtr =
      std::enable_if_t<!std::is_base_of<CheckedUnsafePtrBase, Ptr>::value>;

  // When constructing an CheckedUnsafePtr from a different kind of pointer it's
  // not possible to determine whether it's dangling; therefore it's undefined
  // behavior to construct one from a dangling pointer, and we assume that any
  // CheckedUnsafePtr thus constructed is not dangling.
  template <typename Ptr>
  DisableForCheckedUnsafePtr<Ptr> CopyDanglingFlagIfAvailableFrom(const Ptr&) {}

  template <typename F>
  void WithCheckedUnsafePtrsImpl(CheckedUnsafePtrCheckData* const aRawPtr,
                                 F&& aClosure) {
    if (!mIsDangling && aRawPtr) {
      const auto CheckedUnsafePtrs = aRawPtr->mPtrs.Lock();
      aClosure(this, *CheckedUnsafePtrs);
    }
  }

 private:
  bool mIsDangling = false;
};

class CheckedUnsafePtrBaseAccess {
 protected:
  static void SetDanglingFlag(
      CheckedUnsafePtrBase<CheckingSupport::Enabled>& aBase) {
    aBase.mIsDangling = true;
  }
};
}  // namespace detail

class CheckingPolicyAccess {
 protected:
  template <typename CheckingPolicy>
  static void NotifyCheckFailure(CheckingPolicy& aPolicy) {
    aPolicy.NotifyCheckFailure();
  }
};

template <typename Derived>
class CheckCheckedUnsafePtrs : private CheckingPolicyAccess,
                               private detail::CheckedUnsafePtrBaseAccess {
 public:
  using SupportsChecking =
      std::integral_constant<CheckingSupport, CheckingSupport::Enabled>;

 protected:
  CheckCheckedUnsafePtrs() {
    static_assert(
        std::is_base_of<CheckCheckedUnsafePtrs, Derived>::value,
        "cannot instantiate with a type that's not a subclass of this class");
  }

  bool ShouldCheck() const { return true; }

  void Check(detail::CheckedUnsafePtrCheckData::Data& aCheckedUnsafePtrs) {
    if (!aCheckedUnsafePtrs.IsEmpty()) {
      for (const auto aCheckedUnsafePtrBase : aCheckedUnsafePtrs) {
        SetDanglingFlag(*aCheckedUnsafePtrBase);
      }
      NotifyCheckFailure(*static_cast<Derived*>(this));
    }
  }
};

class CrashOnDanglingCheckedUnsafePtr
    : public CheckCheckedUnsafePtrs<CrashOnDanglingCheckedUnsafePtr> {
  friend class mozilla::CheckingPolicyAccess;
  void NotifyCheckFailure() { MOZ_CRASH("Found dangling CheckedUnsafePtr"); }
};

struct DoNotCheckCheckedUnsafePtrs {
  using SupportsChecking =
      std::integral_constant<CheckingSupport, CheckingSupport::Disabled>;
};

namespace detail {
// Template parameter CheckingSupport controls the inclusion of
// CheckedUnsafePtrCheckData as a subobject of instantiations of
// SupportsCheckedUnsafePtr, ensuring that choosing a policy without checking
// support incurs no size overhead.
template <typename CheckingPolicy,
          CheckingSupport = CheckingPolicy::SupportsChecking::value>
class SupportCheckedUnsafePtrImpl;

template <typename CheckingPolicy>
class SupportCheckedUnsafePtrImpl<CheckingPolicy, CheckingSupport::Disabled>
    : public CheckingPolicy {
 protected:
  template <typename... Args>
  explicit SupportCheckedUnsafePtrImpl(Args&&... aArgs)
      : CheckingPolicy(std::forward<Args>(aArgs)...) {}
};

template <typename CheckingPolicy>
class SupportCheckedUnsafePtrImpl<CheckingPolicy, CheckingSupport::Enabled>
    : public CheckedUnsafePtrCheckData, public CheckingPolicy {
  template <typename T>
  friend class CheckedUnsafePtr;

 protected:
  template <typename... Args>
  explicit SupportCheckedUnsafePtrImpl(Args&&... aArgs)
      : CheckingPolicy(std::forward<Args>(aArgs)...) {}

  ~SupportCheckedUnsafePtrImpl() {
    if (this->ShouldCheck()) {
      const auto ptrs = mPtrs.Lock();
      this->Check(*ptrs);
    }
  }
};

struct SupportsCheckedUnsafePtrTag {};
}  // namespace detail

template <typename Condition,
          typename CheckingPolicy = CrashOnDanglingCheckedUnsafePtr>
using CheckIf = std::conditional_t<Condition::value, CheckingPolicy,
                                   DoNotCheckCheckedUnsafePtrs>;

using DiagnosticAssertEnabled = std::integral_constant<bool,
#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
                                                       true
#else
                                                       false
#endif
                                                       >;

// A T class that publicly inherits from an instantiation of
// SupportsCheckedUnsafePtr and its subclasses can be pointed to by smart
// pointers of type CheckedUnsafePtr<T>. Whenever such a smart pointer is
// created, its existence is tracked by the pointee according to its
// CheckingPolicy. When the pointee goes out of scope it then uses the its
// CheckingPolicy to verify that no CheckedUnsafePtr pointers are left pointing
// to it.
//
// The CheckingPolicy type is used to control the kind of verification that
// happen at the end of the object's lifetime. By default, debug builds always
// check for dangling CheckedUnsafePtr pointers and assert that none are found,
// while release builds forgo all checks. (Release builds incur no size or
// runtime penalties compared to bare pointers.)
template <typename CheckingPolicy>
class SupportsCheckedUnsafePtr
    : public detail::SupportCheckedUnsafePtrImpl<CheckingPolicy>,
      public detail::SupportsCheckedUnsafePtrTag {
 public:
  template <typename... Args>
  explicit SupportsCheckedUnsafePtr(Args&&... aArgs)
      : detail::SupportCheckedUnsafePtrImpl<CheckingPolicy>(
            std::forward<Args>(aArgs)...) {}
};

// CheckedUnsafePtr<T> is a smart pointer class that helps detect dangling
// pointers in cases where such pointers are not allowed. In order to use it,
// the pointee T must publicly inherit from an instantiation of
// SupportsCheckedUnsafePtr. An CheckedUnsafePtr<T> can be used anywhere a T*
// can be used, has the same size, and imposes no additional thread-safety
// restrictions.
template <typename T>
class CheckedUnsafePtr
    : private detail::CheckedUnsafePtrBase<T::SupportsChecking::value> {
  static_assert(
      std::is_base_of<detail::SupportsCheckedUnsafePtrTag, T>::value,
      "type T must be derived from instantiation of SupportsCheckedUnsafePtr");

  template <typename U>
  friend class CheckedUnsafePtr;

  template <typename U, typename S = std::nullptr_t>
  using EnableIfCompatible =
      std::enable_if_t<std::is_base_of<T, std::remove_reference_t<decltype(
                                              *std::declval<U>())>>::value,
                       S>;

 public:
  MOZ_IMPLICIT CheckedUnsafePtr(const std::nullptr_t = nullptr)
      : mRawPtr(nullptr){};

  template <typename U>
  MOZ_IMPLICIT CheckedUnsafePtr(const U& aPtr,
                                EnableIfCompatible<U> = nullptr) {
    Set(aPtr);
  }

  CheckedUnsafePtr(const CheckedUnsafePtr& aOther) { Set(aOther); }

  ~CheckedUnsafePtr() { Reset(); }

  CheckedUnsafePtr& operator=(const std::nullptr_t) {
    Reset();
    return *this;
  }

  template <typename U>
  EnableIfCompatible<U, CheckedUnsafePtr&> operator=(const U& aPtr) {
    Replace(aPtr);
    return *this;
  }

  CheckedUnsafePtr& operator=(const CheckedUnsafePtr& aOther) {
    if (&aOther != this) {
      Replace(aOther);
    }
    return *this;
  }

  T* operator->() const { return mRawPtr; }

  T& operator*() const { return *operator->(); }

  MOZ_IMPLICIT operator T*() const { return operator->(); }

  template <typename U>
  bool operator==(EnableIfCompatible<U, const U&> aRhs) const {
    return mRawPtr == &*aRhs;
  }

  template <typename U>
  friend bool operator==(EnableIfCompatible<U, const U&> aLhs,
                         const CheckedUnsafePtr& aRhs) {
    return aRhs == aLhs;
  }

  template <typename U>
  bool operator!=(EnableIfCompatible<U, const U&> aRhs) const {
    return !(*this == aRhs);
  }

  template <typename U>
  friend bool operator!=(EnableIfCompatible<U, const U&> aLhs,
                         const CheckedUnsafePtr& aRhs) {
    return aRhs != aLhs;
  }

 private:
  using Base = detail::CheckedUnsafePtrBase<CheckingSupport::Enabled>;

  template <typename U>
  void Replace(const U& aPtr) {
    Reset();
    Set(aPtr);
  }

  void Reset() {
    WithCheckedUnsafePtrs(
        [](Base* const aSelf,
           detail::CheckedUnsafePtrCheckData::Data& aCheckedUnsafePtrs) {
          const auto index = aCheckedUnsafePtrs.IndexOf(aSelf);
          aCheckedUnsafePtrs.UnorderedRemoveElementAt(index);
        });
    mRawPtr = nullptr;
  }

  template <typename U>
  void Set(const U& aPtr) {
    this->CopyDanglingFlagIfAvailableFrom(aPtr);
    mRawPtr = &*aPtr;
    WithCheckedUnsafePtrs(
        [](Base* const aSelf,
           detail::CheckedUnsafePtrCheckData::Data& aCheckedUnsafePtrs) {
          aCheckedUnsafePtrs.AppendElement(aSelf);
        });
  }

  template <typename F>
  void WithCheckedUnsafePtrs(F&& aClosure) {
    this->WithCheckedUnsafePtrsImpl(mRawPtr, std::forward<F>(aClosure));
  }

  T* mRawPtr;
};

}  // namespace mozilla

// nsTArray<T> requires by default that T can be safely moved with std::memmove.
// Since CheckedUnsafePtr<T> has a non-trivial copy constructor, it has to opt
// into nsTArray<T> using them.
DECLARE_USE_COPY_CONSTRUCTORS_FOR_TEMPLATE(mozilla::CheckedUnsafePtr)

#endif  // mozilla_CheckedUnsafePtr_h
