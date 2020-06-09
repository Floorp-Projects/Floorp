/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_saferefptr_h__
#define mozilla_saferefptr_h__

#include "mozilla/Maybe.h"
#include "mozilla/NotNull.h"
#include "mozilla/RefCounted.h"
#include "mozilla/RefPtr.h"
#include "nsCOMPtr.h"

namespace mozilla {
template <typename T>
class SafeRefPtr;

template <typename T, typename... Args>
SafeRefPtr<T> MakeSafeRefPtr(Args&&... aArgs);

namespace detail {
struct InitialConstructionTag {};

class SafeRefCountedBase {
  template <typename U, typename... Args>
  friend SafeRefPtr<U> mozilla::MakeSafeRefPtr(Args&&... aArgs);

  template <typename T>
  friend class SafeRefPtr;

  void* operator new(size_t aSize) { return ::operator new(aSize); }

 protected:
  void operator delete(void* aPtr) { ::operator delete(aPtr); }

 public:
  void* operator new[](size_t) = delete;
};

// SafeRefCounted is similar to RefCounted, but they differ in their initial
// refcount (here 1), and the visibility of operator new (here private). The
// rest is mostly a copy of RefCounted.
template <typename T, RefCountAtomicity Atomicity>
class SafeRefCounted : public SafeRefCountedBase {
 protected:
  SafeRefCounted() = default;
#ifdef DEBUG
  ~SafeRefCounted() { MOZ_ASSERT(mRefCnt == detail::DEAD); }
#endif

 public:
  // Compatibility with nsRefPtr.
  MozRefCountType AddRef() const {
    // Note: this method must be thread safe for AtomicRefCounted.
    MOZ_ASSERT(int32_t(mRefCnt) >= 0);
    const MozRefCountType cnt = ++mRefCnt;
#ifdef MOZ_REFCOUNTED_LEAK_CHECKING
    const char* const type = static_cast<const T*>(this)->typeName();
    const uint32_t size = static_cast<const T*>(this)->typeSize();
    const void* const ptr = static_cast<const T*>(this);
    detail::RefCountLogger::logAddRef(ptr, cnt, type, size);
#endif
    return cnt;
  }

  MozRefCountType Release() const {
    // Note: this method must be thread safe for AtomicRefCounted.
    MOZ_ASSERT(int32_t(mRefCnt) > 0);
    const MozRefCountType cnt = --mRefCnt;
#ifdef MOZ_REFCOUNTED_LEAK_CHECKING
    const char* const type = static_cast<const T*>(this)->typeName();
    const void* const ptr = static_cast<const T*>(this);
    // Note: it's not safe to touch |this| after decrementing the refcount,
    // except for below.
    detail::RefCountLogger::logRelease(ptr, cnt, type);
#endif
    if (0 == cnt) {
      // Because we have atomically decremented the refcount above, only
      // one thread can get a 0 count here, so as long as we can assume that
      // everything else in the system is accessing this object through
      // RefPtrs, it's safe to access |this| here.
#ifdef DEBUG
      mRefCnt = detail::DEAD;
#endif
      delete static_cast<const T*>(this);
    }
    return cnt;
  }

  // Compatibility with wtf::RefPtr.
  void ref() { AddRef(); }
  void deref() { Release(); }
  MozRefCountType refCount() const { return mRefCnt; }
  bool hasOneRef() const {
    MOZ_ASSERT(mRefCnt > 0);
    return mRefCnt == 1;
  }

 protected:
  SafeRefPtr<T> SafeRefPtrFromThis();

 private:
  mutable RC<MozRefCountType, Atomicity> mRefCnt =
      RC<MozRefCountType, Atomicity>{1};
};
}  // namespace detail

template <typename T>
class SafeRefCounted
    : public detail::SafeRefCounted<T, detail::NonAtomicRefCount> {
 public:
  ~SafeRefCounted() {
    static_assert(std::is_base_of<SafeRefCounted, T>::value,
                  "T must derive from SafeRefCounted<T>");
  }
};

template <typename T>
class AtomicSafeRefCounted
    : public detail::SafeRefCounted<T, detail::AtomicRefCount> {
 public:
  ~AtomicSafeRefCounted() {
    static_assert(std::is_base_of<AtomicSafeRefCounted, T>::value,
                  "T must derive from AtomicSafeRefCounted<T>");
  }
};

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
template <typename T>
class MOZ_IS_REFPTR MOZ_TRIVIAL_ABI SafeRefPtr {
  template <typename U>
  friend class SafeRefPtr;

  template <typename U, typename... Args>
  friend SafeRefPtr<U> mozilla::MakeSafeRefPtr(Args&&... aArgs);

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

  SafeRefPtr(T* aRawPtr, mozilla::detail::InitialConstructionTag);

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
    static_assert(!std::is_copy_constructible_v<T>);
    static_assert(!std::is_copy_assignable_v<T>);
    static_assert(!std::is_move_constructible_v<T>);
    static_assert(!std::is_move_assignable_v<T>);

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

  template <typename U, typename = std::enable_if_t<std::is_base_of_v<T, U>>>
  SafeRefPtr<U> downcast() && {
    SafeRefPtr<U> res;
    res.mRawPtr = static_cast<U*>(mRawPtr);
    mRawPtr = nullptr;
    return res;
  }

  template <typename U>
  friend RefPtr<U> AsRefPtr(SafeRefPtr<U>&& aSafeRefPtr);
};

template <typename T>
SafeRefPtr(RefPtr<T> &&) -> SafeRefPtr<T>;

template <typename T>
SafeRefPtr(already_AddRefed<T> &&) -> SafeRefPtr<T>;

template <typename T>
SafeRefPtr<T>::SafeRefPtr(T* aRawPtr, detail::InitialConstructionTag)
    : mRawPtr(aRawPtr) {
  if (!std::is_base_of_v<detail::SafeRefCountedBase, T> && mRawPtr) {
    ConstRemovingRefPtrTraits<T>::AddRef(mRawPtr);
  }
}

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
  return SafeRefPtr{new T(std::forward<Args>(aArgs)...),
                    detail::InitialConstructionTag{}};
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

namespace detail {

template <typename T, RefCountAtomicity Atomicity>
SafeRefPtr<T> SafeRefCounted<T, Atomicity>::SafeRefPtrFromThis() {
  // this actually is safe
  return {static_cast<T*>(this), AcquireStrongRefFromRawPtr{}};
}

template <typename T>
struct CopyablePtr<SafeRefPtr<T>> {
  SafeRefPtr<T> mPtr;

  explicit CopyablePtr(SafeRefPtr<T> aPtr) : mPtr{std::move(aPtr)} {}

  CopyablePtr(const CopyablePtr& aOther) : mPtr{aOther.mPtr.clonePtr()} {}
  CopyablePtr& operator=(const CopyablePtr& aOther) {
    if (this != &aOther) {
      mPtr = aOther.mPtr.clonePtr();
    }
    return *this;
  }
  CopyablePtr(CopyablePtr&&) = default;
  CopyablePtr& operator=(CopyablePtr&&) = default;
};

}  // namespace detail

namespace dom {
/// XXX Move this to BindingUtils.h later on
template <class T, class S>
inline RefPtr<T> StrongOrRawPtr(SafeRefPtr<S>&& aPtr) {
  return AsRefPtr(std::move(aPtr));
}

}  // namespace dom

}  // namespace mozilla

// Use MOZ_INLINE_DECL_SAFEREFCOUNTING_INHERITED in a 'Class' derived from a
// 'Super' class which derives from (Atomic)SafeRefCounted, and from some other
// class using NS_INLINE_DECL_PURE_VIRTUAL_REFCOUNTING.
#if defined(NS_BUILD_REFCNT_LOGGING)
#  define MOZ_INLINE_DECL_SAFEREFCOUNTING_INHERITED(Class, Super)         \
    template <typename T, ::mozilla::detail::RefCountAtomicity Atomicity> \
    friend class ::mozilla::detail::SafeRefCounted;                       \
    NS_IMETHOD_(MozExternalRefCountType) AddRef() override {              \
      NS_IMPL_ADDREF_INHERITED_GUTS(Class, Super);                        \
    }                                                                     \
    NS_IMETHOD_(MozExternalRefCountType) Release() override {             \
      NS_IMPL_RELEASE_INHERITED_GUTS(Class, Super);                       \
    }
#else  // NS_BUILD_REFCNT_LOGGING
#  define MOZ_INLINE_DECL_SAFEREFCOUNTING_INHERITED(Class, Super)         \
    template <typename T, ::mozilla::detail::RefCountAtomicity Atomicity> \
    friend class ::mozilla::detail::SafeRefCounted;                       \
    NS_IMETHOD_(MozExternalRefCountType) AddRef() override {              \
      return Super::AddRef();                                             \
    }                                                                     \
    NS_IMETHOD_(MozExternalRefCountType) Release() override {             \
      return Super::Release();                                            \
    }
#endif

#endif
