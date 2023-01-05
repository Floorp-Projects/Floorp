/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Diagnostic class template that helps finding dangling pointers.

#ifndef mozilla_CheckedUnsafePtr_h
#define mozilla_CheckedUnsafePtr_h

#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/DataMutex.h"
#include "mozilla/StackWalk.h"
#include "mozilla/dom/quota/QuotaCommon.h"
#include "nsContentUtils.h"
#include "nsTArray.h"
#include "nsString.h"

#include <cstddef>
#include <type_traits>
#include <utility>

#if defined(DEBUG) || defined(FUZZING) || defined(MOZ_ASAN) || defined(MOZ_TSAN)
#  define MOZ_RECORD_STACKS
#endif

namespace mozilla {
enum class CheckingSupport {
  Disabled,
  Enabled,
};

template <typename T>
class CheckedUnsafePtr;

namespace detail {

static inline void CheckedUnsafePtrStackCallback(uint32_t aFrameNumber,
                                                 void* aPC, void* aSP,
                                                 void* aClosure) {
  auto* stack = static_cast<nsCString*>(aClosure);
  MozCodeAddressDetails details;
  MozDescribeCodeAddress(aPC, &details);
  char buf[1025];
  Unused << MozFormatCodeAddressDetails(buf, sizeof(buf), aFrameNumber, aPC,
                                        &details);
  stack->Append(buf);
  stack->Append("\n");
}

class CheckedUnsafePtrBaseCheckingEnabled;

struct CheckedUnsafePtrCheckData {
  using Data = nsTArray<CheckedUnsafePtrBaseCheckingEnabled*>;

  DataMutex<Data> mPtrs{"mozilla::SupportsCheckedUnsafePtr"};
};

class CheckedUnsafePtrBaseCheckingEnabled {
  friend class CheckedUnsafePtrBaseAccess;

 protected:
  CheckedUnsafePtrBaseCheckingEnabled() = delete;
  CheckedUnsafePtrBaseCheckingEnabled(
      const CheckedUnsafePtrBaseCheckingEnabled& aOther) = default;
  CheckedUnsafePtrBaseCheckingEnabled(const char* aFunction, const char* aFile,
                                      const int aLine)
      : mFunctionName(aFunction), mSourceFile(aFile), mLineNo(aLine) {}

  // When copying an CheckedUnsafePtr, its mIsDangling member must be copied as
  // well; otherwise the new copy might try to dereference a dangling pointer
  // when destructed.
  void CopyDanglingFlagIfAvailableFrom(
      const CheckedUnsafePtrBaseCheckingEnabled& aOther) {
    mIsDangling = aOther.mIsDangling;
  }

  template <typename Ptr>
  using DisableForCheckedUnsafePtr = std::enable_if_t<
      !std::is_base_of<CheckedUnsafePtrBaseCheckingEnabled, Ptr>::value>;

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

  void DumpDebugMsg() {
    fprintf(stderr, "CheckedUnsafePtr [%p]\n", this);
    fprintf(stderr, "Location of creation: %s, %s:%d\n", mFunctionName.get(),
            mozilla::dom::quota::detail::MakeSourceFileRelativePath(mSourceFile)
                .BeginReading(),
            mLineNo);
#ifdef MOZ_RECORD_STACKS
    fprintf(stderr, "Stack of creation:\n%s\n", mCreationStack.get());
    fprintf(stderr, "Stack of last assignment\n%s\n\n",
            mLastAssignmentStack.get());
#endif
  }

  nsCString mFunctionName{EmptyCString()};
  nsCString mSourceFile{EmptyCString()};
  int32_t mLineNo{-1};
#ifdef MOZ_RECORD_STACKS
  nsCString mCreationStack{EmptyCString()};
  nsCString mLastAssignmentStack{EmptyCString()};
#endif

 private:
  bool mIsDangling = false;
};

class CheckedUnsafePtrBaseAccess {
 protected:
  static void SetDanglingFlag(CheckedUnsafePtrBaseCheckingEnabled& aBase) {
    aBase.mIsDangling = true;
    aBase.DumpDebugMsg();
  }
};

template <typename T, CheckingSupport = T::SupportsChecking::value>
class CheckedUnsafePtrBase;

template <typename T, typename U, typename S = std::nullptr_t>
using EnableIfCompatible = std::enable_if_t<
    std::is_base_of<
        T, std::remove_reference_t<decltype(*std::declval<U>())>>::value,
    S>;

template <typename T>
class CheckedUnsafePtrBase<T, CheckingSupport::Enabled>
    : detail::CheckedUnsafePtrBaseCheckingEnabled {
 public:
  MOZ_IMPLICIT constexpr CheckedUnsafePtrBase(
      const std::nullptr_t = nullptr,
      const char* aFunction = __builtin_FUNCTION(),
      const char* aFile = __builtin_FILE(),
      const int32_t aLine = __builtin_LINE())
      : detail::CheckedUnsafePtrBaseCheckingEnabled(aFunction, aFile, aLine),
        mRawPtr(nullptr) {
#ifdef MOZ_RECORD_STACKS
    MozStackWalk(CheckedUnsafePtrStackCallback, CallerPC(), 0, &mCreationStack);
#endif
  }

  template <typename U, typename = EnableIfCompatible<T, U>>
  MOZ_IMPLICIT CheckedUnsafePtrBase(
      const U& aPtr, const char* aFunction = __builtin_FUNCTION(),
      const char* aFile = __builtin_FILE(),
      const int32_t aLine = __builtin_LINE())
      : detail::CheckedUnsafePtrBaseCheckingEnabled(aFunction, aFile, aLine) {
#ifdef MOZ_RECORD_STACKS
    MozStackWalk(CheckedUnsafePtrStackCallback, CallerPC(), 0, &mCreationStack);
#endif
    Set(aPtr);
  }

  CheckedUnsafePtrBase(const CheckedUnsafePtrBase& aOther,
                       const char* aFunction = __builtin_FUNCTION(),
                       const char* aFile = __builtin_FILE(),
                       const int32_t aLine = __builtin_LINE())
      : detail::CheckedUnsafePtrBaseCheckingEnabled(aFunction, aFile, aLine) {
#ifdef MOZ_RECORD_STACKS
    MozStackWalk(CheckedUnsafePtrStackCallback, CallerPC(), 0, &mCreationStack);
#endif
    Set(aOther.Downcast());
  }

  ~CheckedUnsafePtrBase() { Reset(); }

  CheckedUnsafePtr<T>& operator=(const std::nullptr_t) {
    // Assign to nullptr, no need to record the last assignment stack.
#ifdef MOZ_RECORD_STACKS
    mLastAssignmentStack.Truncate();
#endif
    Reset();
    return Downcast();
  }

  template <typename U>
  EnableIfCompatible<T, U, CheckedUnsafePtr<T>&> operator=(const U& aPtr) {
#ifdef MOZ_RECORD_STACKS
    mLastAssignmentStack.Truncate();
    MozStackWalk(CheckedUnsafePtrStackCallback, CallerPC(), 0,
                 &mLastAssignmentStack);
#endif
    Replace(aPtr);
    return Downcast();
  }

  CheckedUnsafePtrBase& operator=(const CheckedUnsafePtrBase& aOther) {
#ifdef MOZ_RECORD_STACKS
    mLastAssignmentStack.Truncate();
    MozStackWalk(CheckedUnsafePtrStackCallback, CallerPC(), 0,
                 &mLastAssignmentStack);
#endif
    if (&aOther != this) {
      Replace(aOther.Downcast());
    }
    return Downcast();
  }

  constexpr T* get() const { return mRawPtr; }

 private:
  template <typename U, CheckingSupport>
  friend class CheckedUnsafePtrBase;

  CheckedUnsafePtr<T>& Downcast() {
    return static_cast<CheckedUnsafePtr<T>&>(*this);
  }
  const CheckedUnsafePtr<T>& Downcast() const {
    return static_cast<const CheckedUnsafePtr<T>&>(*this);
  }

  using Base = detail::CheckedUnsafePtrBaseCheckingEnabled;

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

template <typename T>
class CheckedUnsafePtrBase<T, CheckingSupport::Disabled> {
 public:
  MOZ_IMPLICIT constexpr CheckedUnsafePtrBase(const std::nullptr_t = nullptr)
      : mRawPtr(nullptr) {}

  template <typename U, typename = EnableIfCompatible<T, U>>
  MOZ_IMPLICIT constexpr CheckedUnsafePtrBase(const U& aPtr) : mRawPtr(aPtr) {}

  constexpr CheckedUnsafePtr<T>& operator=(const std::nullptr_t) {
    mRawPtr = nullptr;
    return Downcast();
  }

  template <typename U>
  constexpr EnableIfCompatible<T, U, CheckedUnsafePtr<T>&> operator=(
      const U& aPtr) {
    mRawPtr = aPtr;
    return Downcast();
  }

  constexpr T* get() const { return mRawPtr; }

 private:
  constexpr CheckedUnsafePtr<T>& Downcast() {
    return static_cast<CheckedUnsafePtr<T>&>(*this);
  }

  T* mRawPtr;
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
  static constexpr bool ShouldCheck() {
    static_assert(
        std::is_base_of<CheckCheckedUnsafePtrs, Derived>::value,
        "cannot instantiate with a type that's not a subclass of this class");
    return true;
  }

  void Check(detail::CheckedUnsafePtrCheckData::Data& aCheckedUnsafePtrs) {
    if (!aCheckedUnsafePtrs.IsEmpty()) {
      for (auto* const aCheckedUnsafePtrBase : aCheckedUnsafePtrs) {
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
class CheckedUnsafePtr : public detail::CheckedUnsafePtrBase<T> {
  static_assert(
      std::is_base_of<detail::SupportsCheckedUnsafePtrTag, T>::value,
      "type T must be derived from instantiation of SupportsCheckedUnsafePtr");

 public:
  using detail::CheckedUnsafePtrBase<T>::CheckedUnsafePtrBase;
  using detail::CheckedUnsafePtrBase<T>::get;

  constexpr T* operator->() const { return get(); }

  constexpr T& operator*() const { return *get(); }

  MOZ_IMPLICIT constexpr operator T*() const { return get(); }

  template <typename U>
  constexpr bool operator==(
      detail::EnableIfCompatible<T, U, const U&> aRhs) const {
    return get() == aRhs.get();
  }

  template <typename U>
  friend constexpr bool operator==(
      detail::EnableIfCompatible<T, U, const U&> aLhs,
      const CheckedUnsafePtr& aRhs) {
    return aRhs == aLhs;
  }

  template <typename U>
  constexpr bool operator!=(
      detail::EnableIfCompatible<T, U, const U&> aRhs) const {
    return !(*this == aRhs);
  }

  template <typename U>
  friend constexpr bool operator!=(
      detail::EnableIfCompatible<T, U, const U&> aLhs,
      const CheckedUnsafePtr& aRhs) {
    return aRhs != aLhs;
  }
};

}  // namespace mozilla

// nsTArray<T> requires by default that T can be safely moved with std::memmove.
// Since CheckedUnsafePtr<T> has a non-trivial copy constructor, it has to opt
// into nsTArray<T> using them.
template <typename T>
struct nsTArray_RelocationStrategy<mozilla::CheckedUnsafePtr<T>> {
  using Type = std::conditional_t<
      T::SupportsChecking::value == mozilla::CheckingSupport::Enabled,
      nsTArray_RelocateUsingMoveConstructor<mozilla::CheckedUnsafePtr<T>>,
      nsTArray_RelocateUsingMemutils>;
};

template <typename T>
struct nsTArray_RelocationStrategy<
    mozilla::NotNull<mozilla::CheckedUnsafePtr<T>>> {
  using Type =
      std::conditional_t<T::SupportsChecking::value ==
                             mozilla::CheckingSupport::Enabled,
                         nsTArray_RelocateUsingMoveConstructor<
                             mozilla::NotNull<mozilla::CheckedUnsafePtr<T>>>,
                         nsTArray_RelocateUsingMemutils>;
};

#endif  // mozilla_CheckedUnsafePtr_h
