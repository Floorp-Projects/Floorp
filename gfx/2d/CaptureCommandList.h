/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_gfx_2d_CaptureCommandList_h
#define mozilla_gfx_2d_CaptureCommandList_h

#include "mozilla/Move.h"
#include "mozilla/PodOperations.h"
#include <vector>

#include "DrawCommand.h"
#include "Logging.h"
#include "RwAssert.h"

namespace mozilla {
namespace gfx {

class CaptureCommandList
{
public:
  CaptureCommandList()
    : mLastCommand(nullptr)
  {}
  CaptureCommandList(CaptureCommandList&& aOther)
    : mStorage(std::move(aOther.mStorage))
    , mLastCommand(aOther.mLastCommand)
  {
    aOther.mLastCommand = nullptr;
  }
  ~CaptureCommandList();

  CaptureCommandList& operator=(CaptureCommandList&& aOther) {
    RwAssert::Writer lock(mAssert);
    mStorage = std::move(aOther.mStorage);
    mLastCommand = aOther.mLastCommand;
    aOther.mLastCommand = nullptr;
    return *this;
  }

  template <typename T>
  T* Append() {
    RwAssert::Writer lock(mAssert);
    size_t oldSize = mStorage.size();
    mStorage.resize(mStorage.size() + sizeof(T) + sizeof(uint32_t));
    uint8_t* nextDrawLocation = &mStorage.front() + oldSize;
    *(uint32_t*)(nextDrawLocation) = sizeof(T) + sizeof(uint32_t);
    T* newCommand = reinterpret_cast<T*>(nextDrawLocation + sizeof(uint32_t));
    mLastCommand = newCommand;
    return newCommand;
  }

  template <typename T>
  T* ReuseOrAppend() {
    { // Scope lock
      RwAssert::Writer lock(mAssert);
      if (mLastCommand != nullptr &&
        mLastCommand->GetType() == T::Type) {
        return reinterpret_cast<T*>(mLastCommand);
      }
    }
    return Append<T>();
  }

  class iterator
  {
  public:
    explicit iterator(CaptureCommandList& aParent)
     : mParent(aParent),
       mCurrent(nullptr),
       mEnd(nullptr)
    {
      mParent.mAssert.BeginReading();
      if (!mParent.mStorage.empty()) {
        mCurrent = &mParent.mStorage.front();
        mEnd = mCurrent + mParent.mStorage.size();
      }
    }
    ~iterator()
    {
      mParent.mAssert.EndReading();
    }

    bool Done() const {
      return mCurrent >= mEnd;
    }
    void Next() {
      MOZ_ASSERT(!Done());
      mCurrent += *reinterpret_cast<uint32_t*>(mCurrent);
    }
    DrawingCommand* Get() {
      MOZ_ASSERT(!Done());
      return reinterpret_cast<DrawingCommand*>(mCurrent + sizeof(uint32_t));
    }

  private:
    CaptureCommandList& mParent;
    uint8_t* mCurrent;
    uint8_t* mEnd;
  };

  void Log(TreeLog& aStream)
  {
    for (iterator iter(*this); !iter.Done(); iter.Next()) {
      DrawingCommand* cmd = iter.Get();
      cmd->Log(aStream);
      aStream << "\n";
    }
  }

private:
  CaptureCommandList(const CaptureCommandList& aOther) = delete;
  void operator =(const CaptureCommandList& aOther) = delete;

private:
  std::vector<uint8_t> mStorage;
  DrawingCommand* mLastCommand;
  RwAssert mAssert;
};

} // namespace gfx
} // namespace mozilla

#endif // mozilla_gfx_2d_CaptureCommandList_h
