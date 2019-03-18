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
#include <limits>

#include "DrawCommand.h"
#include "Logging.h"

namespace mozilla {
namespace gfx {

class CaptureCommandList {
 public:
  CaptureCommandList() : mLastCommand(nullptr) {}
  CaptureCommandList(CaptureCommandList&& aOther)
      : mStorage(std::move(aOther.mStorage)),
        mLastCommand(aOther.mLastCommand) {
    aOther.mLastCommand = nullptr;
  }
  ~CaptureCommandList();

  CaptureCommandList& operator=(CaptureCommandList&& aOther) {
    mStorage = std::move(aOther.mStorage);
    mLastCommand = aOther.mLastCommand;
    aOther.mLastCommand = nullptr;
    return *this;
  }

  template <typename T>
  T* Append() {
    static_assert(sizeof(T) + sizeof(uint16_t) + sizeof(uint16_t) <=
                      std::numeric_limits<uint16_t>::max(),
                  "encoding is too small to contain advance");
    const uint16_t kAdvance = sizeof(T) + sizeof(uint16_t) + sizeof(uint16_t);

    size_t size = mStorage.size();
    mStorage.resize(size + kAdvance);

    uint8_t* current = &mStorage.front() + size;
    *(uint16_t*)(current) = kAdvance;
    current += sizeof(uint16_t);
    *(uint16_t*)(current) = ~kAdvance;
    current += sizeof(uint16_t);

    T* command = reinterpret_cast<T*>(current);
    mLastCommand = command;
    return command;
  }

  template <typename T>
  T* ReuseOrAppend() {
    {  // Scope lock
      if (mLastCommand != nullptr && mLastCommand->GetType() == T::Type) {
        return reinterpret_cast<T*>(mLastCommand);
      }
    }
    return Append<T>();
  }

  bool IsEmpty() const { return mStorage.empty(); }

  template <typename T>
  bool BufferWillAlloc() const {
    const uint16_t kAdvance = sizeof(T) + sizeof(uint16_t) + sizeof(uint16_t);
    return mStorage.size() + kAdvance > mStorage.capacity();
  }

  size_t BufferCapacity() const { return mStorage.capacity(); }

  void Clear();

  class iterator {
   public:
    explicit iterator(CaptureCommandList& aParent)
        : mParent(aParent), mCurrent(nullptr), mEnd(nullptr) {
      if (!mParent.mStorage.empty()) {
        mCurrent = &mParent.mStorage.front();
        mEnd = mCurrent + mParent.mStorage.size();
      }
    }

    bool Done() const { return mCurrent >= mEnd; }
    void Next() {
      MOZ_ASSERT(!Done());
      uint16_t advance = *reinterpret_cast<uint16_t*>(mCurrent);
      uint16_t redundant =
          ~*reinterpret_cast<uint16_t*>(mCurrent + sizeof(uint16_t));
      MOZ_RELEASE_ASSERT(advance == redundant);
      mCurrent += advance;
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

  void Log(TreeLog<>& aStream) {
    for (iterator iter(*this); !iter.Done(); iter.Next()) {
      DrawingCommand* cmd = iter.Get();
      cmd->Log(aStream);
      aStream << "\n";
    }
  }

 private:
  CaptureCommandList(const CaptureCommandList& aOther) = delete;
  void operator=(const CaptureCommandList& aOther) = delete;

 private:
  std::vector<uint8_t> mStorage;
  DrawingCommand* mLastCommand;
};

}  // namespace gfx
}  // namespace mozilla

#endif  // mozilla_gfx_2d_CaptureCommandList_h
