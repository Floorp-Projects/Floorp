/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_gfx_2d_RwAssert_h
#define mozilla_gfx_2d_RwAssert_h

#include "mozilla/Attributes.h"
#include "mozilla/Mutex.h"

namespace mozilla {
namespace gfx {

class RwAssert {
public:
  RwAssert()
    : mLock("RwAssert::mLock")
    , mReaders(0)
    , mWriting(false)
  { }

  ~RwAssert()
  {
    MOZ_RELEASE_ASSERT(!mReaders && !mWriting);
  }

  class MOZ_RAII Writer {
  public:
    MOZ_IMPLICIT Writer(RwAssert& aAssert)
      : mAssert(aAssert)
    {
      mAssert.BeginWriting();
    }
    ~Writer()
    {
      mAssert.EndWriting();
    }

  private:
    RwAssert& mAssert;
  };

  class MOZ_RAII Reader {
  public:
    MOZ_IMPLICIT Reader(RwAssert& aAssert)
      : mAssert(aAssert)
    {
      mAssert.BeginReading();
    }
    ~Reader()
    {
      mAssert.EndReading();
    }

  private:
    RwAssert& mAssert;
  };

  void BeginWriting()
  {
    MutexAutoLock lock(mLock);
    MOZ_RELEASE_ASSERT(!mReaders && !mWriting);
    mWriting = true;
  }

  void EndWriting()
  {
    MutexAutoLock lock(mLock);
    MOZ_ASSERT(mWriting);
    mWriting = false;
  }

  void BeginReading()
  {
    MutexAutoLock lock(mLock);
    MOZ_RELEASE_ASSERT(!mWriting);
    mReaders += 1;
  }

  void EndReading()
  {
    MutexAutoLock lock(mLock);
    MOZ_ASSERT(mReaders > 0);
    mReaders -= 1;
  }

private:
  Mutex mLock;
  uint32_t mReaders;
  bool mWriting;
};

} // namespace gfx
} // namespace mozilla

#endif // mozilla_gfx_2d_RwAssert_h
