/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_WEBRENDERTYPES_H
#define GFX_WEBRENDERTYPES_H

#include "mozilla/webrender/webrender_ffi.h"
#include "mozilla/Maybe.h"

typedef mozilla::Maybe<WrImageMask> MaybeImageMask;

namespace mozilla {
namespace wr {

static inline WrColor ToWrColor(const gfx::Color& color)
{
  WrColor c;
  c.r = color.r;
  c.g = color.g;
  c.b = color.b;
  c.a = color.a;
  return c;
}

static inline WrBorderSide ToWrBorderSide(const LayerCoord width, const gfx::Color& color)
{
  WrBorderSide bs;
  bs.width = width;
  bs.color = ToWrColor(color);
  bs.style = WrBorderStyle::Solid;
  return bs;
}

static inline WrLayoutSize ToWrLayoutSize(const LayerSize size)
{
  WrLayoutSize ls;
  ls.width = size.width;
  ls.height = size.height;
  return ls;
}

template<class T>
static inline WrRect ToWrRect(const gfx::RectTyped<T>& rect)
{
  WrRect r;
  r.x = rect.x;
  r.y = rect.y;
  r.width = rect.width;
  r.height = rect.height;
  return r;
}

template<class T>
static inline WrRect ToWrRect(const gfx::IntRectTyped<T>& rect)
{
  return ToWrRect(IntRectToRect(rect));
}

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

  bool operator==(const ByteBuffer& other) const {
    return mLength == other.mLength &&
          !(memcmp(mData, other.mData, mLength));
  }

  size_t mLength;
  uint8_t* mData;
  bool mOwned;
};

struct WindowId {
  explicit WindowId(WrWindowId aHandle) : mHandle(aHandle) {}
  bool operator<(const WindowId& aOther) const { return mHandle < aOther.mHandle; }
  bool operator==(const WindowId& aOther) const { return mHandle == aOther.mHandle; }

  WrWindowId mHandle;
};

struct PipelineId {
  explicit PipelineId(WrPipelineId aHandle) : mHandle(aHandle) {}
  bool operator<(const PipelineId& aOther) const { return mHandle < aOther.mHandle; }
  bool operator==(const PipelineId& aOther) const { return mHandle == aOther.mHandle; }

  WrPipelineId mHandle;
};

// TODO: We need to merge this with the notion of transaction id.
struct Epoch {
  explicit Epoch(WrEpoch aHandle) : mHandle(aHandle) {}
  bool operator<(const Epoch& aOther) const { return mHandle < aOther.mHandle; }
  bool operator==(const Epoch& aOther) const { return mHandle == aOther.mHandle; }

  WrEpoch mHandle;
};

struct FontKey {
  explicit FontKey(WrFontKey aHandle) : mHandle(aHandle) {}
  bool operator<(const FontKey& aOther) const { return mHandle < aOther.mHandle; }
  bool operator==(const FontKey& aOther) const { return mHandle == aOther.mHandle; }

  WrFontKey mHandle;
};

struct ImageKey {
  explicit ImageKey(WrImageKey aHandle) : mHandle(aHandle) {}
  bool operator<(const ImageKey& aOther) const { return mHandle < aOther.mHandle; }
  bool operator==(const ImageKey& aOther) const { return mHandle == aOther.mHandle; }

  WrImageKey mHandle;
};


} // namespace wr
} // namespace mozilla

#endif /* GFX_WEBRENDERTYPES_H */
