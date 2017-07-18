/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=2 sw=2 et tw=78:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/* smart pointer for strong references to nsPresArena-allocated objects
   that might be held onto until the arena's destruction */

#include "mozilla/Assertions.h"
#include "mozilla/RefPtr.h"

#ifndef mozilla_ArenaRefPtr_h
#define mozilla_ArenaRefPtr_h

class nsPresArena;

namespace mozilla {

// Most consumers of ArenaRefPtr will unconditionally use the arena
// However, nsStyleContext only uses the arena if it is a GeckoStyleContext
// so we need a mechanism to override this
template<class U>
struct ArenaRefPtrTraits
{
  static bool UsesArena(U* aPtr) {
    return true;
  }
};

/**
 * A class for holding strong references to nsPresArena-allocated
 * objects.
 *
 * Since the arena's lifetime is not related to the refcounts
 * of the objects allocated within it, it is possible to have a strong
 * reference to an arena-allocated object that lives until the
 * destruction of the arena.  An ArenaRefPtr acts like a weak reference
 * in that it will clear its referent if the arena is about to go away.
 *
 * T must be a class that has these two methods:
 *
 *   static mozilla::ArenaObjectID ArenaObjectID();
 *   U* Arena();
 *
 * where U is a class that has these two methods:
 *
 *   void RegisterArenaRefPtr(ArenaRefPtr<T>*);
 *   void DeregisterArenaRefPtr(ArenaRefPtr<T>*);
 *
 * Currently, both nsPresArena and nsIPresShell can be used as U.
 *
 * The ArenaObjectID method must return the mozilla::ArenaObjectID that
 * uniquely identifies T, and the Arena method must return the nsPresArena
 * (or a proxy for it) in which the object was allocated.
 */
template<typename T>
class ArenaRefPtr
{
  friend class ::nsPresArena;

public:
  ArenaRefPtr()
  {
    AssertValidType();
  }

  template<typename I>
  MOZ_IMPLICIT ArenaRefPtr(already_AddRefed<I>& aRhs)
  {
    AssertValidType();
    assign(aRhs);
  }

  template<typename I>
  MOZ_IMPLICIT ArenaRefPtr(already_AddRefed<I>&& aRhs)
  {
    AssertValidType();
    assign(aRhs);
  }

  MOZ_IMPLICIT ArenaRefPtr(T* aRhs)
  {
    AssertValidType();
    assign(aRhs);
  }

  template<typename I>
  ArenaRefPtr<T>& operator=(already_AddRefed<I>& aRhs)
  {
    assign(aRhs);
    return *this;
  }

  template<typename I>
  ArenaRefPtr<T>& operator=(already_AddRefed<I>&& aRhs)
  {
    assign(aRhs);
    return *this;
  }

  ArenaRefPtr<T>& operator=(T* aRhs)
  {
    assign(aRhs);
    return *this;
  }

  ~ArenaRefPtr() { assign(nullptr); }

  operator T*() const & { return get(); }
  operator T*() const && = delete;
  explicit operator bool() const { return !!mPtr; }
  bool operator!() const { return !mPtr; }
  T* operator->() const { return mPtr.operator->(); }
  T& operator*() const { return *get(); }

  T* get() const { return mPtr; }

private:
  void AssertValidType();

  /**
   * Clears the pointer to the arena-allocated object but skips the usual
   * step of deregistering the ArenaRefPtr from the nsPresArena.  This
   * method is called by nsPresArena when clearing all registered ArenaRefPtrs
   * so that it can deregister them all at once, avoiding hash table churn.
   */
  void ClearWithoutDeregistering()
  {
    mPtr = nullptr;
  }

  template<typename I>
  void assign(already_AddRefed<I>& aSmartPtr)
  {
    RefPtr<T> newPtr(aSmartPtr);
    assignFrom(newPtr);
  }

  template<typename I>
  void assign(already_AddRefed<I>&& aSmartPtr)
  {
    RefPtr<T> newPtr(aSmartPtr);
    assignFrom(newPtr);
  }

  void assign(T* aPtr) { assignFrom(aPtr); }

  template<typename I>
  void assignFrom(I& aPtr)
  {
    if (aPtr == mPtr) {
      return;
    }
    bool sameArena = mPtr && aPtr && mPtr->Arena() == aPtr->Arena();
    if (mPtr && !sameArena && ArenaRefPtrTraits<T>::UsesArena(mPtr)) {
      MOZ_ASSERT(mPtr->Arena());
      mPtr->Arena()->DeregisterArenaRefPtr(this);
    }
    mPtr = Move(aPtr);
    if (mPtr && !sameArena && ArenaRefPtrTraits<T>::UsesArena(aPtr)) {
      MOZ_ASSERT(mPtr->Arena());
      mPtr->Arena()->RegisterArenaRefPtr(this);
    }
  }

  RefPtr<T> mPtr;
};

} // namespace mozilla

#endif // mozilla_ArenaRefPtr_h
