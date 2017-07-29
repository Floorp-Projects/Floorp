/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_WEBRENDERTYPES_H
#define GFX_WEBRENDERTYPES_H

#include "mozilla/webrender/webrender_ffi.h"
#include "mozilla/Maybe.h"
#include "mozilla/gfx/Matrix.h"
#include "mozilla/gfx/Types.h"
#include "mozilla/gfx/Tools.h"
#include "mozilla/layers/LayersTypes.h"
#include "mozilla/Range.h"
#include "Units.h"
#include "RoundedRect.h"
#include "nsStyleConsts.h"

//#define ENABLE_FRAME_LATENCY_LOG

namespace mozilla {
namespace wr {

typedef wr::WrWindowId WindowId;
typedef wr::WrPipelineId PipelineId;
typedef wr::WrImageKey ImageKey;
typedef wr::WrFontKey FontKey;
typedef wr::WrEpoch Epoch;
typedef wr::WrExternalImageId ExternalImageId;

typedef mozilla::Maybe<mozilla::wr::WrImageMask> MaybeImageMask;
typedef Maybe<ExternalImageId> MaybeExternalImageId;

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

inline Maybe<wr::ImageFormat>
SurfaceFormatToImageFormat(gfx::SurfaceFormat aFormat) {
  switch (aFormat) {
    case gfx::SurfaceFormat::R8G8B8X8:
      // TODO: use RGBA + opaque flag
      return Some(wr::ImageFormat::BGRA8);
    case gfx::SurfaceFormat::B8G8R8X8:
      // TODO: WebRender will have a BGRA + opaque flag for this but does not
      // have it yet (cf. issue #732).
    case gfx::SurfaceFormat::B8G8R8A8:
      return Some(wr::ImageFormat::BGRA8);
    case gfx::SurfaceFormat::B8G8R8:
      return Some(wr::ImageFormat::RGB8);
    case gfx::SurfaceFormat::A8:
      return Some(wr::ImageFormat::A8);
    case gfx::SurfaceFormat::R8G8:
      return Some(wr::ImageFormat::RG8);
    case gfx::SurfaceFormat::UNKNOWN:
      return Some(wr::ImageFormat::Invalid);
    default:
      return Nothing();
  }
}

inline gfx::SurfaceFormat
ImageFormatToSurfaceFormat(ImageFormat aFormat) {
  switch (aFormat) {
    case ImageFormat::BGRA8:
      return gfx::SurfaceFormat::B8G8R8A8;
    case ImageFormat::A8:
      return gfx::SurfaceFormat::A8;
    case ImageFormat::RGB8:
      return gfx::SurfaceFormat::B8G8R8;
    default:
      return gfx::SurfaceFormat::UNKNOWN;
  }
}

struct ImageDescriptor: public wr::WrImageDescriptor {
  ImageDescriptor(const gfx::IntSize& aSize, gfx::SurfaceFormat aFormat)
  {
    format = wr::SurfaceFormatToImageFormat(aFormat).value();
    width = aSize.width;
    height = aSize.height;
    stride = 0;
    is_opaque = gfx::IsOpaqueFormat(aFormat);
  }

  ImageDescriptor(const gfx::IntSize& aSize, uint32_t aByteStride, gfx::SurfaceFormat aFormat)
  {
    format = wr::SurfaceFormatToImageFormat(aFormat).value();
    width = aSize.width;
    height = aSize.height;
    stride = aByteStride;
    is_opaque = gfx::IsOpaqueFormat(aFormat);
  }
};

// Whenever possible, use wr::WindowId instead of manipulating uint64_t.
inline uint64_t AsUint64(const WindowId& aId) {
  return static_cast<uint64_t>(aId.mHandle);
}

// Whenever possible, use wr::ImageKey instead of manipulating uint64_t.
inline uint64_t AsUint64(const ImageKey& aId) {
  return (static_cast<uint64_t>(aId.mNamespace.mHandle) << 32)
        + static_cast<uint64_t>(aId.mHandle);
}

inline ImageKey AsImageKey(const uint64_t& aId) {
  ImageKey imageKey;
  imageKey.mNamespace.mHandle = aId >> 32;
  imageKey.mHandle = aId;
  return imageKey;
}

// Whenever possible, use wr::FontKey instead of manipulating uint64_t.
inline uint64_t AsUint64(const FontKey& aId) {
  return (static_cast<uint64_t>(aId.mNamespace.mHandle) << 32)
        + static_cast<uint64_t>(aId.mHandle);
}

inline FontKey AsFontKey(const uint64_t& aId) {
  FontKey fontKey;
  fontKey.mNamespace.mHandle = aId >> 32;
  fontKey.mHandle = aId;
  return fontKey;
}

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

static inline MixBlendMode ToMixBlendMode(gfx::CompositionOp compositionOp)
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

static inline wr::ColorF ToColorF(const gfx::Color& color)
{
  wr::ColorF c;
  c.r = color.r;
  c.g = color.g;
  c.b = color.b;
  c.a = color.a;
  return c;
}

template<class T>
static inline wr::LayoutPoint ToLayoutPoint(const gfx::PointTyped<T>& point)
{
  wr::LayoutPoint p;
  p.x = point.x;
  p.y = point.y;
  return p;
}

template<class T>
static inline wr::LayoutPoint ToLayoutPoint(const gfx::IntPointTyped<T>& point)
{
  return ToLayoutPoint(IntPointToPoint(point));
}

template<class T>
static inline wr::LayoutVector2D ToLayoutVector2D(const gfx::PointTyped<T>& point)
{
  wr::LayoutVector2D p;
  p.x = point.x;
  p.y = point.y;
  return p;
}

template<class T>
static inline wr::LayoutVector2D ToLayoutVector2D(const gfx::IntPointTyped<T>& point)
{
  return ToLayoutVector2D(IntPointToPoint(point));
}

template<class T>
static inline wr::LayoutRect ToLayoutRect(const gfx::RectTyped<T>& rect)
{
  wr::LayoutRect r;
  r.origin.x = rect.x;
  r.origin.y = rect.y;
  r.size.width = rect.width;
  r.size.height = rect.height;
  return r;
}

static inline wr::LayoutRect ToLayoutRect(const gfxRect rect)
{
  wr::LayoutRect r;
  r.origin.x = rect.x;
  r.origin.y = rect.y;
  r.size.width = rect.width;
  r.size.height = rect.height;
  return r;
}

template<class T>
static inline wr::LayoutRect ToLayoutRect(const gfx::IntRectTyped<T>& rect)
{
  return ToLayoutRect(IntRectToRect(rect));
}

template<class T>
static inline wr::LayoutSize ToLayoutSize(const gfx::SizeTyped<T>& size)
{
  wr::LayoutSize ls;
  ls.width = size.width;
  ls.height = size.height;
  return ls;
}

static inline wr::WrComplexClipRegion ToWrComplexClipRegion(const RoundedRect& rect)
{
  wr::WrComplexClipRegion ret;
  ret.rect               = ToLayoutRect(rect.rect);
  ret.radii.top_left     = ToLayoutSize(rect.corners.radii[mozilla::eCornerTopLeft]);
  ret.radii.top_right    = ToLayoutSize(rect.corners.radii[mozilla::eCornerTopRight]);
  ret.radii.bottom_left  = ToLayoutSize(rect.corners.radii[mozilla::eCornerBottomLeft]);
  ret.radii.bottom_right = ToLayoutSize(rect.corners.radii[mozilla::eCornerBottomRight]);
  return ret;
}

template<class T>
static inline wr::LayoutSize ToLayoutSize(const gfx::IntSizeTyped<T>& size)
{
  return ToLayoutSize(IntSizeToSize(size));
}

template<class S, class T>
static inline wr::LayoutTransform ToLayoutTransform(const gfx::Matrix4x4Typed<S, T>& m)
{
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

static inline wr::BorderStyle ToBorderStyle(const uint8_t& style)
{
  switch (style) {
  case NS_STYLE_BORDER_STYLE_NONE:
    return wr::BorderStyle::None;
  case NS_STYLE_BORDER_STYLE_SOLID:
    return wr::BorderStyle::Solid;
  case NS_STYLE_BORDER_STYLE_DOUBLE:
    return wr::BorderStyle::Double;
  case NS_STYLE_BORDER_STYLE_DOTTED:
    return wr::BorderStyle::Dotted;
  case NS_STYLE_BORDER_STYLE_DASHED:
    return wr::BorderStyle::Dashed;
  case NS_STYLE_BORDER_STYLE_HIDDEN:
    return wr::BorderStyle::Hidden;
  case NS_STYLE_BORDER_STYLE_GROOVE:
    return wr::BorderStyle::Groove;
  case NS_STYLE_BORDER_STYLE_RIDGE:
    return wr::BorderStyle::Ridge;
  case NS_STYLE_BORDER_STYLE_INSET:
    return wr::BorderStyle::Inset;
  case NS_STYLE_BORDER_STYLE_OUTSET:
    return wr::BorderStyle::Outset;
  default:
    MOZ_ASSERT(false);
  }
  return wr::BorderStyle::None;
}

static inline wr::BorderSide ToBorderSide(const gfx::Color& color, const uint8_t& style)
{
  wr::BorderSide bs;
  bs.color = ToColorF(color);
  bs.style = ToBorderStyle(style);
  return bs;
}

static inline wr::BorderRadius ToUniformBorderRadius(const mozilla::LayerSize& aSize)
{
  wr::BorderRadius br;
  br.top_left = ToLayoutSize(aSize);
  br.top_right = ToLayoutSize(aSize);
  br.bottom_left = ToLayoutSize(aSize);
  br.bottom_right = ToLayoutSize(aSize);
  return br;
}

static inline wr::BorderRadius ToBorderRadius(const mozilla::LayerSize& topLeft, const mozilla::LayerSize& topRight,
                                              const mozilla::LayerSize& bottomLeft, const mozilla::LayerSize& bottomRight)
{
  wr::BorderRadius br;
  br.top_left = ToLayoutSize(topLeft);
  br.top_right = ToLayoutSize(topRight);
  br.bottom_left = ToLayoutSize(bottomLeft);
  br.bottom_right = ToLayoutSize(bottomRight);
  return br;
}

static inline wr::BorderWidths ToBorderWidths(float top, float right, float bottom, float left)
{
  wr::BorderWidths bw;
  bw.top = top;
  bw.right = right;
  bw.bottom = bottom;
  bw.left = left;
  return bw;
}

static inline wr::NinePatchDescriptor ToNinePatchDescriptor(uint32_t width, uint32_t height,
                                                            const wr::SideOffsets2D_u32& slice)
{
  NinePatchDescriptor patch;
  patch.width = width;
  patch.height = height;
  patch.slice = slice;
  return patch;
}

static inline wr::SideOffsets2D_u32 ToSideOffsets2D_u32(uint32_t top, uint32_t right, uint32_t bottom, uint32_t left)
{
  SideOffsets2D_u32 offset;
  offset.top = top;
  offset.right = right;
  offset.bottom = bottom;
  offset.left = left;
  return offset;
}

static inline wr::SideOffsets2D_f32 ToSideOffsets2D_f32(float top, float right, float bottom, float left)
{
  SideOffsets2D_f32 offset;
  offset.top = top;
  offset.right = right;
  offset.bottom = bottom;
  offset.left = left;
  return offset;
}

static inline wr::RepeatMode ToRepeatMode(uint8_t repeatMode)
{
  switch (repeatMode) {
  case NS_STYLE_BORDER_IMAGE_REPEAT_STRETCH:
    return wr::RepeatMode::Stretch;
  case NS_STYLE_BORDER_IMAGE_REPEAT_REPEAT:
    return wr::RepeatMode::Repeat;
  case NS_STYLE_BORDER_IMAGE_REPEAT_ROUND:
    return wr::RepeatMode::Round;
  case NS_STYLE_BORDER_IMAGE_REPEAT_SPACE:
    return wr::RepeatMode::Space;
  default:
    MOZ_ASSERT(false);
  }

  return wr::RepeatMode::Stretch;
}

template<class S, class T>
static inline wr::WrTransformProperty ToWrTransformProperty(uint64_t id,
                                                            const gfx::Matrix4x4Typed<S, T>& transform)
{
  wr::WrTransformProperty prop;
  prop.id = id;
  prop.transform = ToLayoutTransform(transform);
  return prop;
}

static inline wr::WrOpacityProperty ToWrOpacityProperty(uint64_t id, const float opacity)
{
  wr::WrOpacityProperty prop;
  prop.id = id;
  prop.opacity = opacity;
  return prop;
}

static inline wr::WrComplexClipRegion ToWrComplexClipRegion(const wr::LayoutRect& rect,
                                                            const mozilla::LayerSize& size)
{
  wr::WrComplexClipRegion complex_clip;
  complex_clip.rect = rect;
  complex_clip.radii = wr::ToUniformBorderRadius(size);
  return complex_clip;
}

template<class T>
static inline wr::WrComplexClipRegion ToWrComplexClipRegion(const gfx::RectTyped<T>& rect,
                                                            const mozilla::LayerSize& size)
{
  return ToWrComplexClipRegion(wr::ToLayoutRect(rect), size);
}

// Whenever possible, use wr::ExternalImageId instead of manipulating uint64_t.
inline uint64_t AsUint64(const ExternalImageId& aId) {
  return static_cast<uint64_t>(aId.mHandle);
}

static inline ExternalImageId ToExternalImageId(uint64_t aID)
{
  ExternalImageId Id;
  Id.mHandle = aID;
  return Id;
}

static inline wr::WrExternalImage RawDataToWrExternalImage(const uint8_t* aBuff,
                                                           size_t size)
{
  return wr::WrExternalImage {
    wr::WrExternalImageType::RawData,
    0, 0.0f, 0.0f, 0.0f, 0.0f,
    aBuff, size
  };
}

static inline wr::WrExternalImage NativeTextureToWrExternalImage(uint32_t aHandle,
                                                                 float u0, float v0,
                                                                 float u1, float v1)
{
  return wr::WrExternalImage {
    wr::WrExternalImageType::NativeTexture,
    aHandle, u0, v0, u1, v1,
    nullptr, 0
  };
}

struct Vec_u8 {
  wr::WrVecU8 inner;
  Vec_u8() {
    SetEmpty();
  }
  Vec_u8(Vec_u8&) = delete;
  Vec_u8(Vec_u8&& src) {
    inner = src.inner;
    src.SetEmpty();
  }

  Vec_u8&
  operator=(Vec_u8&& src) {
    inner = src.inner;
    src.SetEmpty();
    return *this;
  }

  wr::WrVecU8
  Extract() {
    wr::WrVecU8 ret = inner;
    SetEmpty();
    return ret;
  }

  void
  SetEmpty() {
    inner.data = (uint8_t*)1;
    inner.capacity = 0;
    inner.length = 0;
  }

  ~Vec_u8() {
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

inline wr::ByteSlice RangeToByteSlice(mozilla::Range<uint8_t> aRange) {
  return wr::ByteSlice { aRange.begin().get(), aRange.length() };
}

inline mozilla::Range<const uint8_t> ByteSliceToRange(wr::ByteSlice aWrSlice) {
  return mozilla::Range<const uint8_t>(aWrSlice.buffer, aWrSlice.len);
}

inline mozilla::Range<uint8_t> MutByteSliceToRange(wr::MutByteSlice aWrSlice) {
  return mozilla::Range<uint8_t>(aWrSlice.buffer, aWrSlice.len);
}

struct BuiltDisplayList {
  wr::VecU8 dl;
  wr::BuiltDisplayListDescriptor dl_desc;
};

static inline wr::WrFilterOpType ToWrFilterOpType(const layers::CSSFilterType type) {
  switch (type) {
    case layers::CSSFilterType::BLUR:
      return wr::WrFilterOpType::Blur;
    case layers::CSSFilterType::BRIGHTNESS:
      return wr::WrFilterOpType::Brightness;
    case layers::CSSFilterType::CONTRAST:
      return wr::WrFilterOpType::Contrast;
    case layers::CSSFilterType::GRAYSCALE:
      return wr::WrFilterOpType::Grayscale;
    case layers::CSSFilterType::HUE_ROTATE:
      return wr::WrFilterOpType::HueRotate;
    case layers::CSSFilterType::INVERT:
      return wr::WrFilterOpType::Invert;
    case layers::CSSFilterType::OPACITY:
      return wr::WrFilterOpType::Opacity;
    case layers::CSSFilterType::SATURATE:
      return wr::WrFilterOpType::Saturate;
    case layers::CSSFilterType::SEPIA:
      return wr::WrFilterOpType::Sepia;
  }
  MOZ_ASSERT_UNREACHABLE("Tried to convert unknown filter type.");
  return wr::WrFilterOpType::Grayscale;
}

static inline wr::WrFilterOp ToWrFilterOp(const layers::CSSFilter& filter) {
  return {
    ToWrFilterOpType(filter.type),
    filter.argument,
  };
}

// Corresponds to an "internal" webrender clip id. That is, a
// ClipId::Clip(x,pipeline_id) maps to a WrClipId{x}. We use a struct wrapper
// instead of a typedef so that this is a distinct type from FrameMetrics::ViewID
// and the compiler will catch accidental conversions between the two.
struct WrClipId {
  uint64_t id;
};

} // namespace wr
} // namespace mozilla

#endif /* GFX_WEBRENDERTYPES_H */
