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

// A restricted variant of mozilla::RefPtr<T>, which prohibits some unsafe or
// unperformant misuses, in particular:
// * It is not implicitly convertible from a raw pointer. Unsafe acquisitions
//   from a raw pointer must be made using the verbose
//   AcquireStrongRefFromRawPtr. To create a new object on the heap, use
//   MakeSafeRefPtr.
// * It does not implicitly decay to a raw pointer. get() must be called
//   explicitly.
// * It is not copyable, but must be explicitly copied using clonePtr().
// * Temporaries cannot be dereferenced using operator* or operator->.
template <class T>
class MOZ_IS_REFPTR SafeRefPtr {
  template <class U>
  friend class SafeRefPtr;

  RefPtr<T> mRefPtr;

 public:
  SafeRefPtr() = default;

  template <typename U,
            typename = std::enable_if_t<std::is_convertible_v<U*, T*>>>
  MOZ_IMPLICIT SafeRefPtr(SafeRefPtr<U>&& aSrc)
      : mRefPtr(std::move(aSrc.mRefPtr)) {}

  explicit SafeRefPtr(RefPtr<T>&& aRefPtr) : mRefPtr(std::move(aRefPtr)) {}

  // To prevent implicit conversion of raw pointer to RefPtr and then
  // calling the previous overload.
  SafeRefPtr(T* const aRawPtr) = delete;

  SafeRefPtr(T* const aRawPtr, const AcquireStrongRefFromRawPtr&)
      : mRefPtr(aRawPtr) {}

  MOZ_IMPLICIT SafeRefPtr(std::nullptr_t) {}

  // Prevent implicit copying, use clonePtr() instead.
  SafeRefPtr(const SafeRefPtr&) = delete;
  SafeRefPtr& operator=(const SafeRefPtr&) = delete;

  // Allow moving.
  SafeRefPtr(SafeRefPtr&&) = default;
  SafeRefPtr& operator=(SafeRefPtr&&) = default;

  typedef T element_type;

  explicit operator bool() const { return mRefPtr; }
  bool operator!() const { return !mRefPtr; }

  T& operator*() const&& = delete;

  T& operator*() const& { return mRefPtr.operator*(); }

  T* operator->() const&& = delete;

  T* operator->() const& MOZ_NO_ADDREF_RELEASE_ON_RETURN {
    return mRefPtr.operator->();
  }

  Maybe<T&> maybeDeref() const {
    return mRefPtr ? SomeRef(*mRefPtr) : Nothing();
  }

  T* unsafeGetRawPtr() const { return mRefPtr.get(); }

  SafeRefPtr<T> clonePtr() const { return SafeRefPtr{RefPtr{mRefPtr}}; }

  already_AddRefed<T> forget() { return mRefPtr.forget(); }

  bool operator==(const SafeRefPtr<T>& aOther) const {
    return mRefPtr == aOther.mRefPtr;
  }

  bool operator!=(const SafeRefPtr<T>& aOther) const {
    return mRefPtr != aOther.mRefPtr;
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
  return std::move(aSafeRefPtr.mRefPtr);
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
