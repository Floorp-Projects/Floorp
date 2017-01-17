/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_WEBRENDERTYPES_H
#define GFX_WEBRENDERTYPES_H

#include "mozilla/gfx/webrender.h"
#include "mozilla/Maybe.h"

typedef mozilla::Maybe<WRImageMask> MaybeImageMask;

namespace mozilla {
namespace layers {

static inline WRColor ToWRColor(const gfx::Color& color)
{
  WRColor c;
  c.r = color.r;
  c.g = color.g;
  c.b = color.b;
  c.a = color.a;
  return c;
}

static inline WRBorderSide ToWRBorderSide(const LayerCoord width, const gfx::Color& color)
{
  WRBorderSide bs;
  bs.width = width;
  bs.color = ToWRColor(color);
  bs.style = WRBorderStyle::Solid;
  return bs;
}

static inline WRLayoutSize ToWRLayoutSize(const LayerSize size)
{
  WRLayoutSize ls;
  ls.width = size.width;
  ls.height = size.height;
  return ls;
}

template<class T>
static inline WRRect ToWRRect(const gfx::RectTyped<T>& rect)
{
  WRRect r;
  r.x = rect.x;
  r.y = rect.y;
  r.width = rect.width;
  r.height = rect.height;
  return r;
}

template<class T>
static inline WRRect ToWRRect(const gfx::IntRectTyped<T>& rect)
{
  return ToWRRect(IntRectToRect(rect));
}

} // namespace layers
} // namespace mozilla

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

  bool operator==(const ByteBuffer& other) const {
    return mLength == other.mLength &&
          !(memcmp(mData, other.mData, mLength));
  }

  size_t mLength;
  uint8_t* mData;
  bool mOwned;
};

struct WindowId {
  explicit WindowId(WRWindowId aHandle) : mHandle(aHandle) {}
  bool operator<(const WindowId& aOther) const { return mHandle < aOther.mHandle; }
  bool operator==(const WindowId& aOther) const { return mHandle == aOther.mHandle; }

  WRWindowId mHandle;
};

struct PipelineId {
  explicit PipelineId(WRPipelineId aHandle) : mHandle(aHandle) {}
  bool operator<(const PipelineId& aOther) const { return mHandle < aOther.mHandle; }
  bool operator==(const PipelineId& aOther) const { return mHandle == aOther.mHandle; }

  WRPipelineId mHandle;
};

// TODO: We need to merge this with the notion of transaction id.
struct Epoch {
  explicit Epoch(WREpoch aHandle) : mHandle(aHandle) {}
  bool operator<(const Epoch& aOther) const { return mHandle < aOther.mHandle; }
  bool operator==(const Epoch& aOther) const { return mHandle == aOther.mHandle; }

  WREpoch mHandle;
};

struct FontKey {
  explicit FontKey(WRFontKey aHandle) : mHandle(aHandle) {}
  bool operator<(const FontKey& aOther) const { return mHandle < aOther.mHandle; }
  bool operator==(const FontKey& aOther) const { return mHandle == aOther.mHandle; }

  WRFontKey mHandle;
};

struct ImageKey {
  explicit ImageKey(WRImageKey aHandle) : mHandle(aHandle) {}
  bool operator<(const ImageKey& aOther) const { return mHandle < aOther.mHandle; }
  bool operator==(const ImageKey& aOther) const { return mHandle == aOther.mHandle; }

  WRImageKey mHandle;
};


} // namespace gfx
} // namespace mozilla

#endif /* GFX_WEBRENDERTYPES_H */
