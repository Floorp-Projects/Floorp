/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DataMutex_h__
#define DataMutex_h__

#include "mozilla/Mutex.h"

namespace mozilla {

// A template to wrap a type with a mutex so that accesses to the type's
// data are required to take the lock before accessing it. This ensures
// that a mutex is explicitly associated with the data that it protects,
// and makes it impossible to access the data without first taking the
// associated mutex.
//
// This is based on Rust's std::sync::Mutex, which operates under the
// strategy of locking data, rather than code.
//
// Examples:
//
//    DataMutex<uint32_t> u32DataMutex(1, "u32DataMutex");
//    auto x = u32DataMutex.Lock();
//    *x = 4;
//    assert(*x, 4u);
//
//    DataMutex<nsTArray<uint32_t>> arrayDataMutex("arrayDataMutex");
//    auto a = arrayDataMutex.Lock();
//    auto& x = a.ref();
//    x.AppendElement(1u);
//    assert(x[0], 1u);
//
template<typename T>
class DataMutex
{
private:
  class MOZ_STACK_CLASS AutoLock
  {
  public:
    T* operator->() const { return &ref(); }

    T& operator*() const { return ref(); }

    // Like RefPtr, make this act like its underlying raw pointer type
    // whenever it is used in a context where a raw pointer is expected.
    operator T*() const & { return &ref(); }

    // Like RefPtr, don't allow implicit conversion of temporary to raw pointer.
    operator T*() const && = delete;

    T& ref() const
    {
      MOZ_ASSERT(mOwner);
      return mOwner->mValue;
    }

    AutoLock(AutoLock&& aOther)
      : mOwner(aOther.mOwner)
    {
      aOther.mOwner = nullptr;
    }

    ~AutoLock()
    {
      if (mOwner) {
        mOwner->mMutex.Unlock();
        mOwner = nullptr;
      }
    }

  private:
    friend class DataMutex;

    AutoLock(const AutoLock& aOther) = delete;

    explicit AutoLock(DataMutex<T>* aDataMutex)
      : mOwner(aDataMutex)
    {
      MOZ_ASSERT(!!mOwner);
      mOwner->mMutex.Lock();
    }

    DataMutex<T>* mOwner;
  };

public:
  explicit DataMutex(const char* aName)
    : mMutex(aName)
  {
  }

  DataMutex(T&& aValue, const char* aName)
    : mMutex(aName)
    , mValue(aValue)
  {
  }

  AutoLock Lock() { return AutoLock(this); }

private:
  Mutex mMutex;
  T mValue;
};

} // namespace mozilla

#endif // DataMutex_h__
