/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=2 sw=2 et tw=78:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/* smart pointer for strong references to objects through pointer-like
 * "handle" objects */

#include <algorithm>
#include "mozilla/Assertions.h"

#ifndef mozilla_HandleRefPtr_h
#define mozilla_HandleRefPtr_h

namespace mozilla {

/**
 * A class for holding strong references to handle-managed objects.
 *
 * This is intended for use with objects like RestyleManagerHandle,
 * where the handle type is not a pointer but which can still have
 * ->AddRef() and ->Release() called on it.
 */
template<typename T>
class HandleRefPtr
{
public:
  HandleRefPtr() {}

  HandleRefPtr(HandleRefPtr<T>& aRhs)
  {
    assign(aRhs.mHandle);
  }

  HandleRefPtr(HandleRefPtr<T>&& aRhs)
  {
    std::swap(mHandle, aRhs.mHandle);
  }

  MOZ_IMPLICIT HandleRefPtr(T aRhs)
  {
    assign(aRhs);
  }

  HandleRefPtr<T>& operator=(HandleRefPtr<T>& aRhs)
  {
    assign(aRhs.mHandle);
    return *this;
  }

  HandleRefPtr<T>& operator=(T aRhs)
  {
    assign(aRhs);
    return *this;
  }

  ~HandleRefPtr() { assign(nullptr); }

  explicit operator bool() const { return !!mHandle; }
  bool operator!() const { return !mHandle; }

  operator T() const { return mHandle; }
  T operator->() const { return mHandle; }

  void swap(HandleRefPtr<T>& aOther)
  {
    std::swap(mHandle, aOther.mHandle);
  }

private:
  void assign(T aPtr)
  {
    // AddRef early so |aPtr| can't disappear underneath us.
    if (aPtr) {
      aPtr->AddRef();
    }

    // Don't release |mHandle| yet: if |mHandle| indirectly owns |this|,
    // releasing would invalidate |this|.  Swap |aPtr| for |mHandle| so that
    // |aPtr| lives as long as |this|, then release the new |aPtr| (really the
    // original |mHandle|) so that if |this| is invalidated, we're not using it
    // any more.
    std::swap(mHandle, aPtr);

    if (aPtr) {
      aPtr->Release();
    }
  }

  T mHandle;
};

template<typename T>
inline bool operator==(const HandleRefPtr<T>& aLHS, const HandleRefPtr<T>& aRHS)
{
  return static_cast<T>(aLHS) == static_cast<T>(aRHS);
}

template<typename T>
inline bool operator==(const HandleRefPtr<T>& aLHS, T aRHS)
{
  return static_cast<T>(aLHS) == aRHS;
}

template<typename T>
inline bool operator==(T aLHS, const HandleRefPtr<T>& aRHS)
{
  return aLHS == static_cast<T>(aRHS);
}

template<typename T>
inline bool operator!=(const HandleRefPtr<T>& aLHS, const HandleRefPtr<T>& aRHS)
{
  return !(aLHS == aRHS);
}

template<typename T>
inline bool operator!=(const HandleRefPtr<T>& aLHS, T aRHS)
{
  return !(aLHS == aRHS);
}

template<typename T>
inline bool operator!=(T aLHS, const HandleRefPtr<T>& aRHS)
{
  return !(aLHS == aRHS);
}

} // namespace mozilla

#endif // mozilla_HandleRefPtr_h
