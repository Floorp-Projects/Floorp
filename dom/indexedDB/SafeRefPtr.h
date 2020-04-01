/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_saferefptr_h__
#define mozilla_saferefptr_h__

#include "mozilla/Maybe.h"
#include "mozilla/RefPtr.h"

namespace mozilla {

struct AcquireStrongRefFromRawPtr {};

// XXX for Apple, clang::trivial_abi is probably also supported, but we need to
// find out the correct version number
#if defined(__clang__) && !defined(__apple_build_version__) && \
    __clang_major__ >= 7
#  define MOZ_TRIVIAL_ABI [[clang::trivial_abi]]
#else
#  define MOZ_TRIVIAL_ABI
#endif

// A restricted variant of mozilla::RefPtr<T>, which prohibits some unsafe or
// unperformant misuses, in particular:
// * It is not implicitly convertible from a raw pointer. Unsafe acquisitions
//   from a raw pointer must be made using the verbose
//   AcquireStrongRefFromRawPtr. To create a new object on the heap, use
//   MakeSafeRefPtr.
// * It does not implicitly decay to a raw pointer. unsafeGetRawPtr() must be
// called
//   explicitly.
// * It is not copyable, but must be explicitly copied using clonePtr().
// * Temporaries cannot be dereferenced using operator* or operator->.
template <class T>
class MOZ_IS_REFPTR MOZ_TRIVIAL_ABI SafeRefPtr {
  template <class U>
  friend class SafeRefPtr;

  T* MOZ_OWNING_REF mRawPtr = nullptr;

  // BEGIN Some things copied from RefPtr.
  // We cannot simply use a RefPtr member because we want to be trivial_abi,
  // which RefPtr is not.
  void assign_with_AddRef(T* aRawPtr) {
    if (aRawPtr) {
      ConstRemovingRefPtrTraits<T>::AddRef(aRawPtr);
    }
    assign_assuming_AddRef(aRawPtr);
  }

  void assign_assuming_AddRef(T* aNewPtr) {
    T* oldPtr = mRawPtr;
    mRawPtr = aNewPtr;
    if (oldPtr) {
      ConstRemovingRefPtrTraits<T>::Release(oldPtr);
    }
  }

  template <class U>
  struct ConstRemovingRefPtrTraits {
    static void AddRef(U* aPtr) { mozilla::RefPtrTraits<U>::AddRef(aPtr); }
    static void Release(U* aPtr) { mozilla::RefPtrTraits<U>::Release(aPtr); }
  };
  template <class U>
  struct ConstRemovingRefPtrTraits<const U> {
    static void AddRef(const U* aPtr) {
      mozilla::RefPtrTraits<U>::AddRef(const_cast<U*>(aPtr));
    }
    static void Release(const U* aPtr) {
      mozilla::RefPtrTraits<U>::Release(const_cast<U*>(aPtr));
    }
  };
  // END Some things copied from RefPtr.

 public:
  SafeRefPtr() = default;

  template <typename U,
            typename = std::enable_if_t<std::is_convertible_v<U*, T*>>>
  MOZ_IMPLICIT SafeRefPtr(SafeRefPtr<U>&& aSrc) : mRawPtr(aSrc.mRawPtr) {
    aSrc.mRawPtr = nullptr;
  }

  explicit SafeRefPtr(RefPtr<T>&& aRefPtr) : mRawPtr(aRefPtr.forget().take()) {}

  // To prevent implicit conversion of raw pointer to RefPtr and then
  // calling the previous overload.
  SafeRefPtr(T* const aRawPtr) = delete;

  SafeRefPtr(T* const aRawPtr, const AcquireStrongRefFromRawPtr&) {
    assign_with_AddRef(aRawPtr);
  }

  MOZ_IMPLICIT SafeRefPtr(std::nullptr_t) {}

  // Prevent implicit copying, use clonePtr() instead.
  SafeRefPtr(const SafeRefPtr&) = delete;
  SafeRefPtr& operator=(const SafeRefPtr&) = delete;

  // Allow moving.
  SafeRefPtr(SafeRefPtr&& aOther) noexcept : mRawPtr(aOther.mRawPtr) {
    aOther.mRawPtr = nullptr;
  }
  SafeRefPtr& operator=(SafeRefPtr&& aOther) noexcept {
    assign_assuming_AddRef(aOther.mRawPtr);
    aOther.mRawPtr = nullptr;
    return *this;
  }

  ~SafeRefPtr() {
    if (mRawPtr) {
      ConstRemovingRefPtrTraits<T>::Release(mRawPtr);
    }
  }

  typedef T element_type;

  explicit operator bool() const { return mRawPtr; }
  bool operator!() const { return !mRawPtr; }

  T& operator*() const&& = delete;

  T& operator*() const& {
    MOZ_ASSERT(mRawPtr);
    return *mRawPtr;
  }

  T* operator->() const&& = delete;

  T* operator->() const& MOZ_NO_ADDREF_RELEASE_ON_RETURN {
    MOZ_ASSERT(mRawPtr);
    return mRawPtr;
  }

  Maybe<T&> maybeDeref() const {
    return mRawPtr ? SomeRef(*mRawPtr) : Nothing();
  }

  T* unsafeGetRawPtr() const { return mRawPtr; }

  SafeRefPtr<T> clonePtr() const {
    return SafeRefPtr{mRawPtr, AcquireStrongRefFromRawPtr{}};
  }

  already_AddRefed<T> forget() {
    auto* const res = mRawPtr;
    mRawPtr = nullptr;
    return dont_AddRef(res);
  }

  bool operator==(const SafeRefPtr<T>& aOther) const {
    return mRawPtr == aOther.mRawPtr;
  }

  bool operator!=(const SafeRefPtr<T>& aOther) const {
    return mRawPtr != aOther.mRawPtr;
  }

  template <typename U>
  friend RefPtr<U> AsRefPtr(SafeRefPtr<U>&& aSafeRefPtr);
};

template <typename T>
SafeRefPtr(RefPtr<T> &&)->SafeRefPtr<T>;

template <typename T>
bool operator==(std::nullptr_t aLhs, const SafeRefPtr<T>& aRhs) {
  return !aRhs;
}

template <typename T>
bool operator!=(std::nullptr_t aLhs, const SafeRefPtr<T>& aRhs) {
  return static_cast<bool>(aRhs);
}

template <typename T>
bool operator==(const SafeRefPtr<T>& aLhs, std::nullptr_t aRhs) {
  return !aLhs;
}

template <typename T>
bool operator!=(const SafeRefPtr<T>& aLhs, std::nullptr_t aRhs) {
  return static_cast<bool>(aLhs);
}

template <typename T, typename U, typename = std::common_type_t<T*, U*>>
bool operator==(T* const aLhs, const SafeRefPtr<U>& aRhs) {
  return aLhs == aRhs.unsafeGetRawPtr();
}

template <typename T, typename U, typename = std::common_type_t<T*, U*>>
bool operator!=(T* const aLhs, const SafeRefPtr<U>& aRhs) {
  return !(aLhs == aRhs);
}

template <typename T, typename U, typename = std::common_type_t<T*, U*>>
bool operator==(const SafeRefPtr<T>& aLhs, U* const aRhs) {
  return aRhs == aLhs;
}

template <typename T, typename U, typename = std::common_type_t<T*, U*>>
bool operator!=(const SafeRefPtr<T>& aLhs, U* const aRhs) {
  return aRhs != aLhs;
}

template <typename T, typename U, typename = std::common_type_t<T*, U*>>
bool operator==(const Maybe<T&> aLhs, const SafeRefPtr<U>& aRhs) {
  return &aLhs.ref() == aRhs.unsafeGetRawPtr();
}

template <typename T, typename U, typename = std::common_type_t<T*, U*>>
bool operator!=(const Maybe<T&> aLhs, const SafeRefPtr<U>& aRhs) {
  return !(aLhs == aRhs);
}

template <typename T, typename U, typename = std::common_type_t<T*, U*>>
bool operator==(const SafeRefPtr<T>& aLhs, const Maybe<U&> aRhs) {
  return aRhs == aLhs;
}

template <typename T, typename U, typename = std::common_type_t<T*, U*>>
bool operator!=(const SafeRefPtr<T>& aLhs, const Maybe<U&> aRhs) {
  return aRhs != aLhs;
}

template <typename T>
RefPtr<T> AsRefPtr(SafeRefPtr<T>&& aSafeRefPtr) {
  return aSafeRefPtr.forget();
}

template <typename T, typename... Args>
SafeRefPtr<T> MakeSafeRefPtr(Args&&... aArgs) {
  return SafeRefPtr{MakeRefPtr<T>(std::forward<Args>(aArgs)...)};
}

template <typename T>
void ImplCycleCollectionTraverse(nsCycleCollectionTraversalCallback& aCallback,
                                 const SafeRefPtr<T>& aField, const char* aName,
                                 uint32_t aFlags = 0) {
  CycleCollectionNoteChild(aCallback, aField.unsafeGetRawPtr(), aName, aFlags);
}

template <typename T>
void ImplCycleCollectionUnlink(SafeRefPtr<T>& aField) {
  aField = nullptr;
}

}  // namespace mozilla

#endif
