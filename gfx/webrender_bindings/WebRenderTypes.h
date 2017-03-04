/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_WEBRENDERTYPES_H
#define GFX_WEBRENDERTYPES_H

#include "mozilla/webrender/webrender_ffi.h"
#include "mozilla/Maybe.h"
#include "mozilla/gfx/Types.h"
#include "mozilla/gfx/Tools.h"
#include "mozilla/Range.h"
#include "Units.h"
#include "nsStyleConsts.h"

typedef mozilla::Maybe<WrImageMask> MaybeImageMask;

namespace mozilla {
namespace wr {

typedef WrGradientExtendMode GradientExtendMode;
typedef WrMixBlendMode MixBlendMode;
typedef WrImageRendering ImageRendering;
typedef WrImageFormat ImageFormat;
typedef WrWindowId WindowId;
typedef WrPipelineId PipelineId;
typedef WrImageKey ImageKey;
typedef WrFontKey FontKey;
typedef WrEpoch Epoch;

inline WindowId NewWindowId(uint64_t aId) {
  WindowId id;
  id.mHandle = aId;
  return id;
}

inline Epoch NewEpoch(uint32_t aEpoch) {
  Epoch e;
  e.mHandle = aEpoch;
  return e;
}

inline Maybe<WrImageFormat>
SurfaceFormatToWrImageFormat(gfx::SurfaceFormat aFormat) {
  switch (aFormat) {
    case gfx::SurfaceFormat::B8G8R8X8:
      // TODO: WebRender will have a BGRA + opaque flag for this but does not
      // have it yet (cf. issue #732).
    case gfx::SurfaceFormat::B8G8R8A8:
      return Some(WrImageFormat::RGBA8);
    case gfx::SurfaceFormat::B8G8R8:
      return Some(WrImageFormat::RGB8);
    case gfx::SurfaceFormat::A8:
      return Some(WrImageFormat::A8);
    case gfx::SurfaceFormat::UNKNOWN:
      return Some(WrImageFormat::Invalid);
    default:
      return Nothing();
  }
}

struct ImageDescriptor: public WrImageDescriptor {
  ImageDescriptor(const gfx::IntSize& aSize, gfx::SurfaceFormat aFormat)
  {
    format = SurfaceFormatToWrImageFormat(aFormat).value();
    width = aSize.width;
    height = aSize.height;
    stride = 0;
    is_opaque = gfx::IsOpaqueFormat(aFormat);
  }

  ImageDescriptor(const gfx::IntSize& aSize, uint32_t aByteStride, gfx::SurfaceFormat aFormat)
  {
    format = SurfaceFormatToWrImageFormat(aFormat).value();
    width = aSize.width;
    height = aSize.height;
    stride = aByteStride;
    is_opaque = gfx::IsOpaqueFormat(aFormat);
  }
};

// Whenever possible, use wr::PipelineId instead of manipulating uint64_t.
inline uint64_t AsUint64(const PipelineId& aId) {
  return (static_cast<uint64_t>(aId.mNamespace) << 32)
        + static_cast<uint64_t>(aId.mHandle);
}

inline PipelineId AsPipelineId(const uint64_t& aId) {
  PipelineId pipeline;
  pipeline.mNamespace = aId >> 32;
  pipeline.mHandle = aId;
  return pipeline;
}

inline ImageRendering ToImageRendering(gfx::SamplingFilter aFilter)
{
  return aFilter == gfx::SamplingFilter::POINT ? ImageRendering::Pixelated
                                               : ImageRendering::Auto;
}

static inline MixBlendMode ToWrMixBlendMode(gfx::CompositionOp compositionOp)
{
  switch (compositionOp)
  {
      case gfx::CompositionOp::OP_MULTIPLY:
        return MixBlendMode::Multiply;
      case gfx::CompositionOp::OP_SCREEN:
        return MixBlendMode::Screen;
      case gfx::CompositionOp::OP_OVERLAY:
        return MixBlendMode::Overlay;
      case gfx::CompositionOp::OP_DARKEN:
        return MixBlendMode::Darken;
      case gfx::CompositionOp::OP_LIGHTEN:
        return MixBlendMode::Lighten;
      case gfx::CompositionOp::OP_COLOR_DODGE:
        return MixBlendMode::ColorDodge;
      case gfx::CompositionOp::OP_COLOR_BURN:
        return MixBlendMode::ColorBurn;
      case gfx::CompositionOp::OP_HARD_LIGHT:
        return MixBlendMode::HardLight;
      case gfx::CompositionOp::OP_SOFT_LIGHT:
        return MixBlendMode::SoftLight;
      case gfx::CompositionOp::OP_DIFFERENCE:
        return MixBlendMode::Difference;
      case gfx::CompositionOp::OP_EXCLUSION:
        return MixBlendMode::Exclusion;
      case gfx::CompositionOp::OP_HUE:
        return MixBlendMode::Hue;
      case gfx::CompositionOp::OP_SATURATION:
        return MixBlendMode::Saturation;
      case gfx::CompositionOp::OP_COLOR:
        return MixBlendMode::Color;
      case gfx::CompositionOp::OP_LUMINOSITY:
        return MixBlendMode::Luminosity;
      default:
        return MixBlendMode::Normal;
  }
}

static inline WrColor ToWrColor(const gfx::Color& color)
{
  WrColor c;
  c.r = color.r;
  c.g = color.g;
  c.b = color.b;
  c.a = color.a;
  return c;
}

static inline WrBorderStyle ToWrBorderStyle(const uint8_t& style)
{
  switch (style) {
  case NS_STYLE_BORDER_STYLE_NONE:
    return WrBorderStyle::None;
  case NS_STYLE_BORDER_STYLE_SOLID:
    return WrBorderStyle::Solid;
  case NS_STYLE_BORDER_STYLE_DOUBLE:
    return WrBorderStyle::Double;
  case NS_STYLE_BORDER_STYLE_DOTTED:
    return WrBorderStyle::Dotted;
  case NS_STYLE_BORDER_STYLE_DASHED:
    return WrBorderStyle::Dashed;
  case NS_STYLE_BORDER_STYLE_HIDDEN:
    return WrBorderStyle::Hidden;
  case NS_STYLE_BORDER_STYLE_GROOVE:
    return WrBorderStyle::Groove;
  case NS_STYLE_BORDER_STYLE_RIDGE:
    return WrBorderStyle::Ridge;
  case NS_STYLE_BORDER_STYLE_INSET:
    return WrBorderStyle::Inset;
  case NS_STYLE_BORDER_STYLE_OUTSET:
    return WrBorderStyle::Outset;
  default:
    MOZ_ASSERT(false);
  }
  return WrBorderStyle::None;
}

static inline WrBorderSide ToWrBorderSide(const LayerCoord width, const gfx::Color& color, const uint8_t& style)
{
  WrBorderSide bs;
  bs.width = width;
  bs.color = ToWrColor(color);
  bs.style = ToWrBorderStyle(style);
  return bs;
}

static inline WrPoint ToWrPoint(const LayerPoint point)
{
  WrPoint lp;
  lp.x = point.x;
  lp.y = point.y;
  return lp;
}

static inline WrLayoutSize ToWrLayoutSize(const LayerSize size)
{
  WrLayoutSize ls;
  ls.width = size.width;
  ls.height = size.height;
  return ls;
}

static inline WrBorderRadius ToWrBorderRadius(const LayerSize& topLeft, const LayerSize& topRight,
                                              const LayerSize& bottomLeft, const LayerSize& bottomRight)
{
  WrBorderRadius br;
  br.top_left = ToWrLayoutSize(topLeft);
  br.top_right = ToWrLayoutSize(topRight);
  br.bottom_left = ToWrLayoutSize(bottomLeft);
  br.bottom_right = ToWrLayoutSize(bottomRight);
  return br;
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

static inline WrPoint ToWrPoint(const gfx::Point& point)
{
  WrPoint p;
  p.x = point.x;
  p.y = point.y;
  return p;
}

static inline WrExternalImageId ToWrExternalImageId(uint64_t aID)
{
  WrExternalImageId id;
  id.id = aID;
  return id;
}

struct VecU8 {
  WrVecU8 inner;
  VecU8() {
    inner.data = nullptr;
    inner.capacity = 0;
  }
  VecU8(VecU8&) = delete;
  VecU8(VecU8&& src) {
    inner = src.inner;
    src.inner.data = nullptr;
    src.inner.capacity = 0;
  }

  ~VecU8() {
    if (inner.data) {
      wr_vec_u8_free(inner);
    }
  }
};

struct ByteBuffer
{
  ByteBuffer(size_t aLength, uint8_t* aData)
    : mLength(aLength)
    , mData(aData)
    , mOwned(false)
  {}

  // XXX: this is a bit of hack that assumes
  // the allocators are the same
  explicit ByteBuffer(VecU8&& vec)
  {
    if (vec.inner.capacity) {
      mLength = vec.inner.length;
      mData = vec.inner.data;
      vec.inner.data = nullptr;
      vec.inner.capacity = 0;
      mOwned = true;
    } else {
      mOwned = false;
      mData = nullptr;
      mLength = 0;
    }
  }

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

  const Range<uint8_t> AsSlice() const { return Range<uint8_t>(mData, mLength); }

  Range<uint8_t> AsSlice() { return Range<uint8_t>(mData, mLength); }

  bool operator==(const ByteBuffer& other) const {
    return mLength == other.mLength &&
          !(memcmp(mData, other.mData, mLength));
  }

  size_t mLength;
  uint8_t* mData;
  bool mOwned;
};

struct BuiltDisplayList {
  VecU8 dl;
  WrBuiltDisplayListDescriptor dl_desc;
  VecU8 aux;
  WrAuxiliaryListsDescriptor aux_desc;
};

} // namespace wr
} // namespace mozilla

#endif /* GFX_WEBRENDERTYPES_H */
