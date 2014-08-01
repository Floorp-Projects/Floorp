/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* A class for non-null strong pointers to reference-counted objects. */

#ifndef mozilla_dom_OwningNonNull_h
#define mozilla_dom_OwningNonNull_h

#include "nsAutoPtr.h"
#include "nsCycleCollectionNoteChild.h"

namespace mozilla {
namespace dom {

template<class T>
class OwningNonNull
{
public:
  OwningNonNull()
#ifdef DEBUG
    : mInited(false)
#endif
  {}

  // This is no worse than get() in terms of const handling.
  operator T&() const
  {
    MOZ_ASSERT(mInited);
    MOZ_ASSERT(mPtr, "OwningNonNull<T> was set to null");
    return *mPtr;
  }

  operator T*() const
  {
    MOZ_ASSERT(mInited);
    MOZ_ASSERT(mPtr, "OwningNonNull<T> was set to null");
    return mPtr;
  }

  void operator=(T* aValue)
  {
    init(aValue);
  }

  void operator=(T& aValue)
  {
    init(&aValue);
  }

  void operator=(const already_AddRefed<T>& aValue)
  {
    init(aValue);
  }

  already_AddRefed<T> forget()
  {
#ifdef DEBUG
    mInited = false;
#endif
    return mPtr.forget();
  }

  // Make us work with smart pointer helpers that expect a get().
  T* get() const
  {
    MOZ_ASSERT(mInited);
    MOZ_ASSERT(mPtr);
    return mPtr;
  }

protected:
  template<typename U>
  void init(U aValue)
  {
    mPtr = aValue;
    MOZ_ASSERT(mPtr);
#ifdef DEBUG
    mInited = true;
#endif
  }

  nsRefPtr<T> mPtr;
#ifdef DEBUG
  bool mInited;
#endif
};

template <typename T>
inline void
ImplCycleCollectionTraverse(nsCycleCollectionTraversalCallback& aCallback,
                            OwningNonNull<T>& aField,
                            const char* aName,
                            uint32_t aFlags = 0)
{
  CycleCollectionNoteChild(aCallback, aField.get(), aName, aFlags);
}

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_OwningNonNull_h
