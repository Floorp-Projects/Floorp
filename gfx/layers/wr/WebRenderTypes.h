/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_BYTEBUFFER_H
#define GFX_BYTEBUFFER_H

#include "chrome/common/ipc_message_utils.h"
#include "mozilla/gfx/webrender.h"
#include "mozilla/Maybe.h"

typedef mozilla::Maybe<WRImageMask> MaybeImageMask;

namespace mozilla {
namespace gfx {

struct ByteBuffer
{
  ByteBuffer(size_t aLength, uint8_t* aData)
    : mLength(aLength)
    , mData(aData)
    , mOwned(false)
  {}

  ByteBuffer()
    : mLength(0)
    , mData(nullptr)
    , mOwned(false)
  {}

  bool
  Allocate(size_t aLength)
  {
    MOZ_ASSERT(mData == nullptr);
    mData = (uint8_t*)malloc(aLength);
    if (!mData) {
      return false;
    }
    mLength = aLength;
    mOwned = true;
    return true;
  }

  ~ByteBuffer()
  {
    if (mData && mOwned) {
      free(mData);
    }
  }

  size_t mLength;
  uint8_t* mData;
  bool mOwned;
};

} // namespace gfx
} // namespace mozilla

namespace IPC {

template<>
struct ParamTraits<mozilla::gfx::ByteBuffer>
{
  typedef mozilla::gfx::ByteBuffer paramType;

  static void
  Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mLength);
    aMsg->WriteBytes(aParam.mData, aParam.mLength);
  }

  static bool
  Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    size_t length;
    return ReadParam(aMsg, aIter, &length)
        && aResult->Allocate(length)
        && aMsg->ReadBytesInto(aIter, aResult->mData, length);
  }
};

template<>
struct ParamTraits<WRImageFormat>
  : public ContiguousEnumSerializer<
        WRImageFormat,
        WRImageFormat::Invalid,
        WRImageFormat::RGBAF32>
{
};

template<>
struct ParamTraits<WRImageKey>
{
  static void
  Write(Message* aMsg, const WRImageKey& aParam)
  {
    WriteParam(aMsg, aParam.a);
    WriteParam(aMsg, aParam.b);
  }

  static bool
  Read(const Message* aMsg, PickleIterator* aIter, WRImageKey* aResult)
  {
    return ReadParam(aMsg, aIter, &aResult->a)
        && ReadParam(aMsg, aIter, &aResult->b);
  }
};

template<>
struct ParamTraits<WRRect>
{
  static void
  Write(Message* aMsg, const WRRect& aParam)
  {
    WriteParam(aMsg, aParam.x);
    WriteParam(aMsg, aParam.y);
    WriteParam(aMsg, aParam.width);
    WriteParam(aMsg, aParam.height);
  }

  static bool
  Read(const Message* aMsg, PickleIterator* aIter, WRRect* aResult)
  {
    return ReadParam(aMsg, aIter, &aResult->x)
        && ReadParam(aMsg, aIter, &aResult->y)
        && ReadParam(aMsg, aIter, &aResult->width)
        && ReadParam(aMsg, aIter, &aResult->height);
  }
};

template<>
struct ParamTraits<WRImageMask>
{
  static void
  Write(Message* aMsg, const WRImageMask& aParam)
  {
    WriteParam(aMsg, aParam.image);
    WriteParam(aMsg, aParam.rect);
    WriteParam(aMsg, aParam.repeat);
  }

  static bool
  Read(const Message* aMsg, PickleIterator* aIter, WRImageMask* aResult)
  {
    return ReadParam(aMsg, aIter, &aResult->image)
        && ReadParam(aMsg, aIter, &aResult->rect)
        && ReadParam(aMsg, aIter, &aResult->repeat);
  }
};

} // namespace IPC

#endif /* GFX_BYTEBUFFER_H */
