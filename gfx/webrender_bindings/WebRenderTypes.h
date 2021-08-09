/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_WEBRENDERTYPES_H
#define GFX_WEBRENDERTYPES_H

#include "ImageTypes.h"
#include "mozilla/webrender/webrender_ffi.h"
#include "mozilla/EnumSet.h"
#include "mozilla/Maybe.h"
#include "mozilla/gfx/Matrix.h"
#include "mozilla/gfx/Types.h"
#include "mozilla/gfx/Tools.h"
#include "mozilla/gfx/Rect.h"
#include "mozilla/layers/LayersTypes.h"
#include "mozilla/PodOperations.h"
#include "mozilla/Range.h"
#include "mozilla/TypeTraits.h"
#include "Units.h"

namespace mozilla {

enum class StyleBorderStyle : uint8_t;
enum class StyleBorderImageRepeat : uint8_t;

namespace ipc {
class ByteBuf;
}  // namespace ipc

namespace wr {

// Using uintptr_t in C++ code for "size" types seems weird, so let's use a
// better-sounding typedef. The name comes from the fact that we generally
// have to deal with uintptr_t because that's what rust's usize maps to.
typedef uintptr_t usize;

typedef wr::WrWindowId WindowId;
typedef wr::WrRemovedPipeline RemovedPipeline;

class RenderedFrameIdType {};
typedef layers::BaseTransactionId<RenderedFrameIdType> RenderedFrameId;

typedef mozilla::Maybe<mozilla::wr::IdNamespace> MaybeIdNamespace;
typedef mozilla::Maybe<mozilla::wr::ImageMask> MaybeImageMask;
typedef Maybe<ExternalImageId> MaybeExternalImageId;

typedef Maybe<FontInstanceOptions> MaybeFontInstanceOptions;
typedef Maybe<FontInstancePlatformOptions> MaybeFontInstancePlatformOptions;

struct ExternalImageKeyPair {
  ImageKey key;
  ExternalImageId id;
};

/* Generate a brand new window id and return it. */
WindowId NewWindowId();

inline DebugFlags NewDebugFlags(uint32_t aFlags) { return {aFlags}; }

inline Maybe<wr::ImageFormat> SurfaceFormatToImageFormat(
    gfx::SurfaceFormat aFormat) {
  switch (aFormat) {
    case gfx::SurfaceFormat::R8G8B8X8:
      // WebRender not support RGBX8. Assert here.
      MOZ_ASSERT(false);
      return Nothing();
    case gfx::SurfaceFormat::R8G8B8A8:
      return Some(wr::ImageFormat::RGBA8);
    case gfx::SurfaceFormat::B8G8R8X8:
      // TODO: WebRender will have a BGRA + opaque flag for this but does not
      // have it yet (cf. issue #732).
    case gfx::SurfaceFormat::B8G8R8A8:
      return Some(wr::ImageFormat::BGRA8);
    case gfx::SurfaceFormat::A8:
      return Some(wr::ImageFormat::R8);
    case gfx::SurfaceFormat::A16:
      return Some(wr::ImageFormat::R16);
    case gfx::SurfaceFormat::R8G8:
      return Some(wr::ImageFormat::RG8);
    case gfx::SurfaceFormat::R16G16:
      return Some(wr::ImageFormat::RG16);
    case gfx::SurfaceFormat::UNKNOWN:
    default:
      return Nothing();
  }
}

inline gfx::SurfaceFormat ImageFormatToSurfaceFormat(ImageFormat aFormat) {
  switch (aFormat) {
    case ImageFormat::BGRA8:
      return gfx::SurfaceFormat::B8G8R8A8;
    case ImageFormat::R8:
      return gfx::SurfaceFormat::A8;
    case ImageFormat::R16:
      return gfx::SurfaceFormat::A16;
    default:
      return gfx::SurfaceFormat::UNKNOWN;
  }
}

struct ImageDescriptor : public wr::WrImageDescriptor {
  // We need a default constructor for ipdl serialization.
  ImageDescriptor() {
    format = (ImageFormat)0;
    width = 0;
    height = 0;
    stride = 0;
    opacity = OpacityType::HasAlphaChannel;
    prefer_compositor_surface = false;
  }

  ImageDescriptor(const gfx::IntSize& aSize, gfx::SurfaceFormat aFormat,
                  bool aPreferCompositorSurface = false) {
    format = wr::SurfaceFormatToImageFormat(aFormat).value();
    width = aSize.width;
    height = aSize.height;
    stride = 0;
    opacity = gfx::IsOpaque(aFormat) ? OpacityType::Opaque
                                     : OpacityType::HasAlphaChannel;
    prefer_compositor_surface = aPreferCompositorSurface;
  }

  ImageDescriptor(const gfx::IntSize& aSize, uint32_t aByteStride,
                  gfx::SurfaceFormat aFormat,
                  bool aPreferCompositorSurface = false) {
    format = wr::SurfaceFormatToImageFormat(aFormat).value();
    width = aSize.width;
    height = aSize.height;
    stride = aByteStride;
    opacity = gfx::IsOpaque(aFormat) ? OpacityType::Opaque
                                     : OpacityType::HasAlphaChannel;
    prefer_compositor_surface = aPreferCompositorSurface;
  }

  ImageDescriptor(const gfx::IntSize& aSize, uint32_t aByteStride,
                  gfx::SurfaceFormat aFormat, OpacityType aOpacity,
                  bool aPreferCompositorSurface = false) {
    format = wr::SurfaceFormatToImageFormat(aFormat).value();
    width = aSize.width;
    height = aSize.height;
    stride = aByteStride;
    opacity = aOpacity;
    prefer_compositor_surface = aPreferCompositorSurface;
  }
};

inline uint64_t AsUint64(const NativeSurfaceId& aId) {
  return static_cast<uint64_t>(aId._0);
}

// Whenever possible, use wr::WindowId instead of manipulating uint64_t.
inline uint64_t AsUint64(const WindowId& aId) {
  return static_cast<uint64_t>(aId.mHandle);
}

// Whenever possible, use wr::ImageKey instead of manipulating uint64_t.
inline uint64_t AsUint64(const ImageKey& aId) {
  return (static_cast<uint64_t>(aId.mNamespace.mHandle) << 32) +
         static_cast<uint64_t>(aId.mHandle);
}

inline ImageKey AsImageKey(const uint64_t& aId) {
  ImageKey imageKey;
  imageKey.mNamespace.mHandle = aId >> 32;
  imageKey.mHandle = aId;
  return imageKey;
}

// Whenever possible, use wr::FontKey instead of manipulating uint64_t.
inline uint64_t AsUint64(const FontKey& aId) {
  return (static_cast<uint64_t>(aId.mNamespace.mHandle) << 32) +
         static_cast<uint64_t>(aId.mHandle);
}

inline FontKey AsFontKey(const uint64_t& aId) {
  FontKey fontKey;
  fontKey.mNamespace.mHandle = aId >> 32;
  fontKey.mHandle = aId;
  return fontKey;
}

// Whenever possible, use wr::FontInstanceKey instead of manipulating uint64_t.
inline uint64_t AsUint64(const FontInstanceKey& aId) {
  return (static_cast<uint64_t>(aId.mNamespace.mHandle) << 32) +
         static_cast<uint64_t>(aId.mHandle);
}

inline FontInstanceKey AsFontInstanceKey(const uint64_t& aId) {
  FontInstanceKey instanceKey;
  instanceKey.mNamespace.mHandle = aId >> 32;
  instanceKey.mHandle = aId;
  return instanceKey;
}

// Whenever possible, use wr::PipelineId instead of manipulating uint64_t.
inline uint64_t AsUint64(const PipelineId& aId) {
  return (static_cast<uint64_t>(aId.mNamespace) << 32) +
         static_cast<uint64_t>(aId.mHandle);
}

inline PipelineId AsPipelineId(const uint64_t& aId) {
  PipelineId pipeline;
  pipeline.mNamespace = aId >> 32;
  pipeline.mHandle = aId;
  return pipeline;
}

inline mozilla::layers::LayersId AsLayersId(const PipelineId& aId) {
  return mozilla::layers::LayersId{AsUint64(aId)};
}

inline PipelineId AsPipelineId(const mozilla::layers::LayersId& aId) {
  return AsPipelineId(uint64_t(aId));
}

inline ImageRendering ToImageRendering(gfx::SamplingFilter aFilter) {
  return aFilter == gfx::SamplingFilter::POINT ? ImageRendering::Pixelated
                                               : ImageRendering::Auto;
}

static inline FontRenderMode ToFontRenderMode(gfx::AntialiasMode aMode,
                                              bool aPermitSubpixelAA = true) {
  switch (aMode) {
    case gfx::AntialiasMode::NONE:
      return FontRenderMode::Mono;
    case gfx::AntialiasMode::GRAY:
      return FontRenderMode::Alpha;
    case gfx::AntialiasMode::SUBPIXEL:
    default:
      return aPermitSubpixelAA ? FontRenderMode::Subpixel
                               : FontRenderMode::Alpha;
  }
}

static inline MixBlendMode ToMixBlendMode(gfx::CompositionOp compositionOp) {
  switch (compositionOp) {
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

static inline wr::ColorF ToColorF(const gfx::DeviceColor& color) {
  wr::ColorF c;
  c.r = color.r;
  c.g = color.g;
  c.b = color.b;
  c.a = color.a;
  return c;
}

static inline wr::ColorU ToColorU(const gfx::DeviceColor& color) {
  wr::ColorU c;
  c.r = uint8_t(color.r * 255.0f);
  c.g = uint8_t(color.g * 255.0f);
  c.b = uint8_t(color.b * 255.0f);
  c.a = uint8_t(color.a * 255.0f);
  return c;
}

static inline wr::LayoutPoint ToLayoutPoint(
    const mozilla::LayoutDevicePoint& point) {
  wr::LayoutPoint p;
  p.x = point.x;
  p.y = point.y;
  return p;
}

static inline wr::LayoutPoint ToLayoutPoint(
    const mozilla::LayoutDeviceIntPoint& point) {
  return ToLayoutPoint(LayoutDevicePoint(point));
}

static inline wr::WorldPoint ToWorldPoint(const mozilla::ScreenPoint& point) {
  wr::WorldPoint p;
  p.x = point.x;
  p.y = point.y;
  return p;
}

static inline wr::LayoutVector2D ToLayoutVector2D(
    const mozilla::LayoutDevicePoint& point) {
  wr::LayoutVector2D p;
  p.x = point.x;
  p.y = point.y;
  return p;
}

static inline wr::LayoutVector2D ToLayoutVector2D(
    const mozilla::LayoutDeviceIntPoint& point) {
  return ToLayoutVector2D(LayoutDevicePoint(point));
}

static inline wr::LayoutRect ToLayoutRect(
    const mozilla::LayoutDeviceRect& rect) {
  wr::LayoutRect r;
  r.min.x = rect.X();
  r.min.y = rect.Y();
  r.max.x = rect.X() + rect.Width();
  r.max.y = rect.Y() + rect.Height();
  return r;
}

static inline wr::LayoutRect ToLayoutRect(const gfx::Rect& rect) {
  wr::LayoutRect r;
  r.min.x = rect.X();
  r.min.y = rect.Y();
  r.max.x = rect.X() + rect.Width();
  r.max.y = rect.Y() + rect.Height();
  return r;
}

static inline wr::DeviceIntRect ToDeviceIntRect(
    const mozilla::ImageIntRect& rect) {
  wr::DeviceIntRect r;
  r.min.x = rect.X();
  r.min.y = rect.Y();
  r.max.x = rect.X() + rect.Width();
  r.max.y = rect.Y() + rect.Height();
  return r;
}

// TODO: should be const LayoutDeviceIntRect instead of ImageIntRect
static inline wr::LayoutIntRect ToLayoutIntRect(
    const mozilla::ImageIntRect& rect) {
  wr::LayoutIntRect r;
  r.min.x = rect.X();
  r.min.y = rect.Y();
  r.max.x = rect.X() + rect.Width();
  r.max.y = rect.Y() + rect.Height();
  return r;
}

static inline wr::LayoutRect ToLayoutRect(
    const mozilla::LayoutDeviceIntRect& rect) {
  return ToLayoutRect(IntRectToRect(rect));
}

static inline wr::LayoutRect IntersectLayoutRect(const wr::LayoutRect& aRect,
                                                 const wr::LayoutRect& aOther) {
  wr::LayoutRect r;
  r.min.x = std::max(aRect.min.x, aOther.min.x);
  r.min.y = std::max(aRect.min.y, aOther.min.y);
  r.max.x = std::min(aRect.max.x, aOther.max.x);
  r.max.y = std::min(aRect.max.y, aOther.max.y);

  if (r.max.x < r.min.x || r.max.y < r.min.y) {
    r.max.x = r.min.x;
    r.max.y = r.min.y;
  }
  return r;
}

static inline wr::LayoutSize ToLayoutSize(
    const mozilla::LayoutDeviceSize& size) {
  wr::LayoutSize ls;
  ls.width = size.width;
  ls.height = size.height;
  return ls;
}

static inline wr::ComplexClipRegion ToComplexClipRegion(
    const gfx::RoundedRect& rect) {
  wr::ComplexClipRegion ret;
  ret.rect = ToLayoutRect(rect.rect);
  ret.radii.top_left = ToLayoutSize(LayoutDeviceSize::FromUnknownSize(
      rect.corners.radii[mozilla::eCornerTopLeft]));
  ret.radii.top_right = ToLayoutSize(LayoutDeviceSize::FromUnknownSize(
      rect.corners.radii[mozilla::eCornerTopRight]));
  ret.radii.bottom_left = ToLayoutSize(LayoutDeviceSize::FromUnknownSize(
      rect.corners.radii[mozilla::eCornerBottomLeft]));
  ret.radii.bottom_right = ToLayoutSize(LayoutDeviceSize::FromUnknownSize(
      rect.corners.radii[mozilla::eCornerBottomRight]));
  ret.mode = wr::ClipMode::Clip;
  return ret;
}

static inline wr::ComplexClipRegion SimpleRadii(const wr::LayoutRect& aRect,
                                                float aRadii) {
  wr::ComplexClipRegion ret;
  wr::LayoutSize radii{aRadii, aRadii};
  ret.rect = aRect;
  ret.radii.top_left = radii;
  ret.radii.top_right = radii;
  ret.radii.bottom_left = radii;
  ret.radii.bottom_right = radii;
  ret.mode = wr::ClipMode::Clip;
  return ret;
}

static inline wr::LayoutSize ToLayoutSize(
    const mozilla::LayoutDeviceIntSize& size) {
  return ToLayoutSize(LayoutDeviceSize(size));
}

template <class S, class T>
static inline wr::LayoutTransform ToLayoutTransform(
    const gfx::Matrix4x4Typed<S, T>& m) {
  wr::LayoutTransform transform;
  transform.m11 = m._11;
  transform.m12 = m._12;
  transform.m13 = m._13;
  transform.m14 = m._14;
  transform.m21 = m._21;
  transform.m22 = m._22;
  transform.m23 = m._23;
  transform.m24 = m._24;
  transform.m31 = m._31;
  transform.m32 = m._32;
  transform.m33 = m._33;
  transform.m34 = m._34;
  transform.m41 = m._41;
  transform.m42 = m._42;
  transform.m43 = m._43;
  transform.m44 = m._44;
  return transform;
}

wr::BorderStyle ToBorderStyle(StyleBorderStyle style);

static inline wr::BorderSide ToBorderSide(const gfx::DeviceColor& color,
                                          StyleBorderStyle style) {
  wr::BorderSide bs;
  bs.color = ToColorF(color);
  bs.style = ToBorderStyle(style);
  return bs;
}

static inline wr::BorderRadius EmptyBorderRadius() {
  wr::BorderRadius br;
  PodZero(&br);
  return br;
}

static inline wr::BorderRadius ToBorderRadius(
    const LayoutDeviceSize& topLeft, const LayoutDeviceSize& topRight,
    const LayoutDeviceSize& bottomLeft, const LayoutDeviceSize& bottomRight) {
  wr::BorderRadius br;
  br.top_left = ToLayoutSize(topLeft);
  br.top_right = ToLayoutSize(topRight);
  br.bottom_left = ToLayoutSize(bottomLeft);
  br.bottom_right = ToLayoutSize(bottomRight);
  return br;
}

static inline wr::BorderRadius ToBorderRadius(
    const gfx::RectCornerRadii& aRadii) {
  return ToBorderRadius(LayoutDeviceSize::FromUnknownSize(aRadii[0]),
                        LayoutDeviceSize::FromUnknownSize(aRadii[1]),
                        LayoutDeviceSize::FromUnknownSize(aRadii[3]),
                        LayoutDeviceSize::FromUnknownSize(aRadii[2]));
}

static inline wr::ComplexClipRegion ToComplexClipRegion(
    const nsRect& aRect, const nscoord* aRadii, int32_t aAppUnitsPerDevPixel) {
  wr::ComplexClipRegion ret;
  ret.rect =
      ToLayoutRect(LayoutDeviceRect::FromAppUnits(aRect, aAppUnitsPerDevPixel));
  ret.radii = ToBorderRadius(
      LayoutDeviceSize::FromAppUnits(
          nsSize(aRadii[eCornerTopLeftX], aRadii[eCornerTopLeftY]),
          aAppUnitsPerDevPixel),
      LayoutDeviceSize::FromAppUnits(
          nsSize(aRadii[eCornerTopRightX], aRadii[eCornerTopRightY]),
          aAppUnitsPerDevPixel),
      LayoutDeviceSize::FromAppUnits(
          nsSize(aRadii[eCornerBottomLeftX], aRadii[eCornerBottomLeftY]),
          aAppUnitsPerDevPixel),
      LayoutDeviceSize::FromAppUnits(
          nsSize(aRadii[eCornerBottomRightX], aRadii[eCornerBottomRightY]),
          aAppUnitsPerDevPixel));
  ret.mode = ClipMode::Clip;
  return ret;
}

static inline wr::LayoutSideOffsets ToBorderWidths(float top, float right,
                                                   float bottom, float left) {
  wr::LayoutSideOffsets bw;
  bw.top = top;
  bw.right = right;
  bw.bottom = bottom;
  bw.left = left;
  return bw;
}

static inline wr::DeviceIntSideOffsets ToDeviceIntSideOffsets(int32_t top,
                                                              int32_t right,
                                                              int32_t bottom,
                                                              int32_t left) {
  wr::DeviceIntSideOffsets offset;
  offset.top = top;
  offset.right = right;
  offset.bottom = bottom;
  offset.left = left;
  return offset;
}

static inline wr::LayoutSideOffsets ToLayoutSideOffsets(float top, float right,
                                                        float bottom,
                                                        float left) {
  wr::LayoutSideOffsets offset;
  offset.top = top;
  offset.right = right;
  offset.bottom = bottom;
  offset.left = left;
  return offset;
}

wr::RepeatMode ToRepeatMode(StyleBorderImageRepeat);

template <class S, class T>
static inline wr::WrTransformProperty ToWrTransformProperty(
    uint64_t id, const gfx::Matrix4x4Typed<S, T>& transform) {
  wr::WrTransformProperty prop;
  prop.id = id;
  prop.value = ToLayoutTransform(transform);
  return prop;
}

static inline wr::WrOpacityProperty ToWrOpacityProperty(uint64_t id,
                                                        const float opacity) {
  wr::WrOpacityProperty prop;
  prop.id = id;
  prop.value = opacity;
  return prop;
}

static inline wr::WrColorProperty ToWrColorProperty(
    uint64_t id, const gfx::DeviceColor& color) {
  wr::WrColorProperty prop;
  prop.id = id;
  prop.value = ToColorF(color);
  return prop;
}

// Whenever possible, use wr::ExternalImageId instead of manipulating uint64_t.
inline uint64_t AsUint64(const ExternalImageId& aId) {
  return static_cast<uint64_t>(aId._0);
}

static inline ExternalImageId ToExternalImageId(uint64_t aID) {
  ExternalImageId Id;
  Id._0 = aID;
  return Id;
}

static inline wr::WrExternalImage RawDataToWrExternalImage(const uint8_t* aBuff,
                                                           size_t size) {
  return wr::WrExternalImage{
      wr::WrExternalImageType::RawData, 0, 0.0f, 0.0f, 0.0f, 0.0f, aBuff, size};
}

static inline wr::WrExternalImage NativeTextureToWrExternalImage(
    uint32_t aHandle, float u0, float v0, float u1, float v1) {
  return wr::WrExternalImage{wr::WrExternalImageType::NativeTexture,
                             aHandle,
                             u0,
                             v0,
                             u1,
                             v1,
                             nullptr,
                             0};
}

static inline wr::WrExternalImage InvalidToWrExternalImage() {
  return wr::WrExternalImage{
      wr::WrExternalImageType::Invalid, 0, 0, 0, 0, 0, nullptr, 0};
}

inline wr::ByteSlice RangeToByteSlice(mozilla::Range<uint8_t> aRange) {
  return wr::ByteSlice{aRange.begin().get(), aRange.length()};
}

inline mozilla::Range<const uint8_t> ByteSliceToRange(wr::ByteSlice aWrSlice) {
  return mozilla::Range<const uint8_t>(aWrSlice.buffer, aWrSlice.len);
}

inline mozilla::Range<uint8_t> MutByteSliceToRange(wr::MutByteSlice aWrSlice) {
  return mozilla::Range<uint8_t>(aWrSlice.buffer, aWrSlice.len);
}

void Assign_WrVecU8(wr::WrVecU8& aVec, mozilla::ipc::ByteBuf&& aOther);

template <typename T>
struct Vec;

template <>
struct Vec<uint8_t> final {
  wr::WrVecU8 inner;
  Vec() { SetEmpty(); }
  Vec(Vec&) = delete;
  Vec(Vec&& src) {
    inner = src.inner;
    src.SetEmpty();
  }

  explicit Vec(mozilla::ipc::ByteBuf&& aSrc) {
    Assign_WrVecU8(inner, std::move(aSrc));
  }

  Vec& operator=(Vec&& src) {
    inner = src.inner;
    src.SetEmpty();
    return *this;
  }

  wr::WrVecU8 Extract() {
    wr::WrVecU8 ret = inner;
    SetEmpty();
    return ret;
  }

  void SetEmpty() {
    inner.data = (uint8_t*)1;
    inner.capacity = 0;
    inner.length = 0;
  }

  uint8_t* Data() { return inner.data; }

  size_t Length() { return inner.length; }

  size_t Capacity() { return inner.capacity; }

  Range<uint8_t> GetRange() { return Range<uint8_t>(Data(), Length()); }

  void PushBytes(Range<uint8_t> aBytes) {
    wr_vec_u8_push_bytes(&inner, RangeToByteSlice(aBytes));
  }

  void Reserve(size_t aLength) { wr_vec_u8_reserve(&inner, aLength); }

  ~Vec() {
    if (inner.data) {
      wr_vec_u8_free(inner);
    }
  }
};

struct ByteBuffer {
  ByteBuffer(size_t aLength, uint8_t* aData)
      : mLength(aLength), mData(aData), mOwned(false) {}

  // XXX: this is a bit of hack that assumes
  // the allocators are the same
  explicit ByteBuffer(VecU8&& vec) {
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

  ByteBuffer(ByteBuffer&& aFrom)
      : mLength(aFrom.mLength), mData(aFrom.mData), mOwned(aFrom.mOwned) {
    aFrom.mLength = 0;
    aFrom.mData = nullptr;
    aFrom.mOwned = false;
  }

  ByteBuffer(ByteBuffer& aFrom)
      : mLength(aFrom.mLength), mData(aFrom.mData), mOwned(aFrom.mOwned) {
    aFrom.mLength = 0;
    aFrom.mData = nullptr;
    aFrom.mOwned = false;
  }

  ByteBuffer() : mLength(0), mData(nullptr), mOwned(false) {}

  bool Allocate(size_t aLength) {
    MOZ_ASSERT(mData == nullptr);
    mData = (uint8_t*)malloc(aLength);
    if (!mData) {
      return false;
    }
    mLength = aLength;
    mOwned = true;
    return true;
  }

  ~ByteBuffer() {
    if (mData && mOwned) {
      free(mData);
    }
  }

  const Range<uint8_t> AsSlice() const {
    return Range<uint8_t>(mData, mLength);
  }

  Range<uint8_t> AsSlice() { return Range<uint8_t>(mData, mLength); }

  bool operator==(const ByteBuffer& other) const {
    return mLength == other.mLength && !(memcmp(mData, other.mData, mLength));
  }

  size_t mLength;
  uint8_t* mData;
  bool mOwned;
};

struct BuiltDisplayList {
  wr::VecU8 dl_items;
  wr::VecU8 dl_cache;
  wr::BuiltDisplayListDescriptor dl_desc;
};

// Corresponds to a clip id for a clip chain in webrender. Similar to
// WrClipId but a separate struct so we don't get them mixed up in C++.
struct WrClipChainId {
  uint64_t id;

  bool operator==(const WrClipChainId& other) const { return id == other.id; }

  static WrClipChainId Empty() {
    WrClipChainId id = {0};
    return id;
  }
};

WrSpatialId RootScrollNode();
WrSpaceAndClipChain RootScrollNodeWithChain();
WrSpaceAndClipChain InvalidScrollNodeWithChain();

enum class WebRenderError : int8_t {
  INITIALIZE = 0,
  MAKE_CURRENT,
  RENDER,
  NEW_SURFACE,
  BEGIN_DRAW,
  VIDEO_OVERLAY,
  EXCESSIVE_RESETS,

  Sentinel /* this must be last for serialization purposes. */
};

static inline wr::WrYuvColorSpace ToWrYuvColorSpace(
    gfx::YUVColorSpace aYUVColorSpace) {
  switch (aYUVColorSpace) {
    case gfx::YUVColorSpace::BT601:
      return wr::WrYuvColorSpace::Rec601;
    case gfx::YUVColorSpace::BT709:
      return wr::WrYuvColorSpace::Rec709;
    case gfx::YUVColorSpace::BT2020:
      return wr::WrYuvColorSpace::Rec2020;
    case gfx::YUVColorSpace::Identity:
      return wr::WrYuvColorSpace::Identity;
    default:
      MOZ_ASSERT_UNREACHABLE("Tried to convert invalid YUVColorSpace.");
  }
  return wr::WrYuvColorSpace::Rec601;
}

// TODO: Use YUVRangedColorSpace instead of assuming ColorRange::LIMITED.
static inline wr::YuvRangedColorSpace ToWrYuvRangedColorSpace(
    gfx::YUVRangedColorSpace aFrom) {
  switch (aFrom) {
    case gfx::YUVRangedColorSpace::BT601_Narrow:
      return wr::YuvRangedColorSpace::Rec601Narrow;
    case gfx::YUVRangedColorSpace::BT601_Full:
      return wr::YuvRangedColorSpace::Rec601Full;
    case gfx::YUVRangedColorSpace::BT709_Narrow:
      return wr::YuvRangedColorSpace::Rec709Narrow;
    case gfx::YUVRangedColorSpace::BT709_Full:
      return wr::YuvRangedColorSpace::Rec709Full;
    case gfx::YUVRangedColorSpace::BT2020_Narrow:
      return wr::YuvRangedColorSpace::Rec2020Narrow;
    case gfx::YUVRangedColorSpace::BT2020_Full:
      return wr::YuvRangedColorSpace::Rec2020Full;
    case gfx::YUVRangedColorSpace::GbrIdentity:
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Tried to convert invalid YUVColorSpace.");
      break;
  }
  return wr::YuvRangedColorSpace::GbrIdentity;
}

static inline wr::WrColorDepth ToWrColorDepth(gfx::ColorDepth aColorDepth) {
  switch (aColorDepth) {
    case gfx::ColorDepth::COLOR_8:
      return wr::WrColorDepth::Color8;
    case gfx::ColorDepth::COLOR_10:
      return wr::WrColorDepth::Color10;
    case gfx::ColorDepth::COLOR_12:
      return wr::WrColorDepth::Color12;
    case gfx::ColorDepth::COLOR_16:
      return wr::WrColorDepth::Color16;
    default:
      MOZ_ASSERT_UNREACHABLE("Tried to convert invalid color depth value.");
  }
  return wr::WrColorDepth::Color8;
}

static inline wr::WrColorRange ToWrColorRange(gfx::ColorRange aColorRange) {
  switch (aColorRange) {
    case gfx::ColorRange::LIMITED:
      return wr::WrColorRange::Limited;
    case gfx::ColorRange::FULL:
      return wr::WrColorRange::Full;
    default:
      MOZ_ASSERT_UNREACHABLE("Tried to convert invalid color range value.");
      return wr ::WrColorRange::Limited;
  }
}

static inline wr::SyntheticItalics DegreesToSyntheticItalics(float aDegrees) {
  wr::SyntheticItalics synthetic_italics;
  synthetic_italics.angle =
      int16_t(std::min(std::max(aDegrees, -89.0f), 89.0f) * 256.0f);
  return synthetic_italics;
}

}  // namespace wr
}  // namespace mozilla

namespace std {
template <>
struct hash<mozilla::wr::WrSpatialId> {
  std::size_t operator()(mozilla::wr::WrSpatialId const& aKey) const noexcept {
    return std::hash<size_t>{}(aKey.id);
  }
};
}  // namespace std

#endif /* GFX_WEBRENDERTYPES_H */
