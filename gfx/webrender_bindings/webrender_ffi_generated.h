/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Generated with cbindgen:0.2.2 */

/* DO NOT MODIFY THIS MANUALLY! This file was generated using cbindgen.
 * To generate this file:
 *   1. Get the latest cbindgen using `cargo install --force cbindgen`
 *      a. Alternatively, you can clone `https://github.com/rlhunt/cbindgen` and use a tagged release
 *   2. Run `rustup run nightly cbindgen toolkit/library/rust/ --crate webrender_bindings -o gfx/webrender_bindings/webrender_ffi_generated.h`
 */

#include <cstdint>
#include <cstdlib>

extern "C" {

namespace mozilla {
namespace wr {

enum class BorderStyle : uint32_t {
  None = 0,
  Solid = 1,
  Double = 2,
  Dotted = 3,
  Dashed = 4,
  Hidden = 5,
  Groove = 6,
  Ridge = 7,
  Inset = 8,
  Outset = 9,

  Sentinel /* this must be last for serialization purposes. */
};

enum class BoxShadowClipMode : uint32_t {
  Outset = 0,
  Inset = 1,

  Sentinel /* this must be last for serialization purposes. */
};

enum class ClipMode {
  Clip = 0,
  ClipOut = 1,

  Sentinel /* this must be last for serialization purposes. */
};

enum class ExtendMode : uint32_t {
  Clamp = 0,
  Repeat = 1,

  Sentinel /* this must be last for serialization purposes. */
};

enum class ExternalImageType : uint32_t {
  Texture2DHandle = 0,
  Texture2DArrayHandle = 1,
  TextureRectHandle = 2,
  TextureExternalHandle = 3,
  ExternalBuffer = 4,

  Sentinel /* this must be last for serialization purposes. */
};

#if !(defined(XP_MACOSX) || defined(XP_WIN))
enum class FontHinting : uint8_t {
  None = 0,
  Mono = 1,
  Light = 2,
  Normal = 3,
  LCD = 4,

  Sentinel /* this must be last for serialization purposes. */
};
#endif

#if !(defined(XP_MACOSX) || defined(XP_WIN))
enum class FontLCDFilter : uint8_t {
  None = 0,
  Default = 1,
  Light = 2,
  Legacy = 3,

  Sentinel /* this must be last for serialization purposes. */
};
#endif

enum class FontRenderMode : uint32_t {
  Mono = 0,
  Alpha = 1,
  Subpixel = 2,
  Bitmap = 3,

  Sentinel /* this must be last for serialization purposes. */
};

enum class ImageFormat : uint32_t {
  Invalid = 0,
  A8 = 1,
  RGB8 = 2,
  BGRA8 = 3,
  RGBAF32 = 4,
  RG8 = 5,

  Sentinel /* this must be last for serialization purposes. */
};

enum class ImageRendering : uint32_t {
  Auto = 0,
  CrispEdges = 1,
  Pixelated = 2,

  Sentinel /* this must be last for serialization purposes. */
};

enum class LineOrientation : uint8_t {
  Vertical = 0,
  Horizontal = 1,

  Sentinel /* this must be last for serialization purposes. */
};

enum class LineStyle : uint8_t {
  Solid = 0,
  Dotted = 1,
  Dashed = 2,
  Wavy = 3,

  Sentinel /* this must be last for serialization purposes. */
};

// An enum representing the available verbosity level filters of the logging
// framework.
//
// A `LogLevelFilter` may be compared directly to a [`LogLevel`](enum.LogLevel.html).
// Use this type to [`get()`](struct.MaxLogLevelFilter.html#method.get) and
// [`set()`](struct.MaxLogLevelFilter.html#method.set) the
// [`MaxLogLevelFilter`](struct.MaxLogLevelFilter.html), or to match with the getter
// [`max_log_level()`](fn.max_log_level.html).
enum class LogLevelFilter : uintptr_t {
  // A level lower than all log levels.
  Off = 0,
  // Corresponds to the `Error` log level.
  Error = 1,
  // Corresponds to the `Warn` log level.
  Warn = 2,
  // Corresponds to the `Info` log level.
  Info = 3,
  // Corresponds to the `Debug` log level.
  Debug = 4,
  // Corresponds to the `Trace` log level.
  Trace = 5,

  Sentinel /* this must be last for serialization purposes. */
};

enum class MixBlendMode : uint32_t {
  Normal = 0,
  Multiply = 1,
  Screen = 2,
  Overlay = 3,
  Darken = 4,
  Lighten = 5,
  ColorDodge = 6,
  ColorBurn = 7,
  HardLight = 8,
  SoftLight = 9,
  Difference = 10,
  Exclusion = 11,
  Hue = 12,
  Saturation = 13,
  Color = 14,
  Luminosity = 15,

  Sentinel /* this must be last for serialization purposes. */
};

enum class RepeatMode : uint32_t {
  Stretch = 0,
  Repeat = 1,
  Round = 2,
  Space = 3,

  Sentinel /* this must be last for serialization purposes. */
};

enum class SubpixelDirection : uint32_t {
  None = 0,
  Horizontal = 1,
  Vertical = 2,

  Sentinel /* this must be last for serialization purposes. */
};

enum class TransformStyle : uint32_t {
  Flat = 0,
  Preserve3D = 1,

  Sentinel /* this must be last for serialization purposes. */
};

enum class WrAnimationType : uint32_t {
  Transform = 0,
  Opacity = 1,

  Sentinel /* this must be last for serialization purposes. */
};

enum class WrExternalImageType : uint32_t {
  NativeTexture = 0,
  RawData = 1,

  Sentinel /* this must be last for serialization purposes. */
};

enum class WrFilterOpType : uint32_t {
  Blur = 0,
  Brightness = 1,
  Contrast = 2,
  Grayscale = 3,
  HueRotate = 4,
  Invert = 5,
  Opacity = 6,
  Saturate = 7,
  Sepia = 8,

  Sentinel /* this must be last for serialization purposes. */
};

enum class YuvColorSpace : uint32_t {
  Rec601 = 0,
  Rec709 = 1,

  Sentinel /* this must be last for serialization purposes. */
};

struct Arc_VecU8;

struct DocumentHandle;

// The renderer is responsible for submitting to the GPU the work prepared by the
// RenderBackend.
struct Renderer;

// The resource updates for a given transaction (they must be applied in the same frame).
struct ResourceUpdates;

struct Vec_u8;

struct WrProgramCache;

struct WrRenderedEpochs;

struct WrState;

struct WrThreadPool;

struct IdNamespace {
  uint32_t mHandle;

  bool operator==(const IdNamespace& aOther) const {
    return mHandle == aOther.mHandle;
  }
  bool operator!=(const IdNamespace& aOther) const {
    return mHandle != aOther.mHandle;
  }
  bool operator<(const IdNamespace& aOther) const {
    return mHandle < aOther.mHandle;
  }
  bool operator<=(const IdNamespace& aOther) const {
    return mHandle <= aOther.mHandle;
  }
};

struct FontKey {
  IdNamespace mNamespace;
  uint32_t mHandle;

  bool operator==(const FontKey& aOther) const {
    return mNamespace == aOther.mNamespace &&
           mHandle == aOther.mHandle;
  }
};

typedef FontKey WrFontKey;

typedef Arc_VecU8 ArcVecU8;

typedef Vec_u8 VecU8;

struct Epoch {
  uint32_t mHandle;

  bool operator==(const Epoch& aOther) const {
    return mHandle == aOther.mHandle;
  }
  bool operator<(const Epoch& aOther) const {
    return mHandle < aOther.mHandle;
  }
  bool operator<=(const Epoch& aOther) const {
    return mHandle <= aOther.mHandle;
  }
};

typedef Epoch WrEpoch;

// This type carries no valuable semantics for WR. However, it reflects the fact that
// clients (Servo) may generate pipelines by different semi-independent sources.
// These pipelines still belong to the same `IdNamespace` and the same `DocumentId`.
// Having this extra Id field enables them to generate `PipelineId` without collision.
typedef uint32_t PipelineSourceId;

// From the point of view of WR, `PipelineId` is completely opaque and generic as long as
// it's clonable, serializable, comparable, and hashable.
struct PipelineId {
  PipelineSourceId mNamespace;
  uint32_t mHandle;

  bool operator==(const PipelineId& aOther) const {
    return mNamespace == aOther.mNamespace &&
           mHandle == aOther.mHandle;
  }
};

typedef PipelineId WrPipelineId;

struct TypedSize2D_f32__LayerPixel {
  float width;
  float height;

  bool operator==(const TypedSize2D_f32__LayerPixel& aOther) const {
    return width == aOther.width &&
           height == aOther.height;
  }
};

typedef TypedSize2D_f32__LayerPixel LayerSize;

typedef LayerSize LayoutSize;

// Describes the memory layout of a display list.
//
// A display list consists of some number of display list items, followed by a number of display
// items.
struct BuiltDisplayListDescriptor {
  // The first IPC time stamp: before any work has been done
  uint64_t builder_start_time;
  // The second IPC time stamp: after serialization
  uint64_t builder_finish_time;
  // The third IPC time stamp: just before sending
  uint64_t send_start_time;

  bool operator==(const BuiltDisplayListDescriptor& aOther) const {
    return builder_start_time == aOther.builder_start_time &&
           builder_finish_time == aOther.builder_finish_time &&
           send_start_time == aOther.send_start_time;
  }
};

struct WrVecU8 {
  uint8_t *data;
  size_t length;
  size_t capacity;

  bool operator==(const WrVecU8& aOther) const {
    return data == aOther.data &&
           length == aOther.length &&
           capacity == aOther.capacity;
  }
};

struct WrOpacityProperty {
  uint64_t id;
  float opacity;

  bool operator==(const WrOpacityProperty& aOther) const {
    return id == aOther.id &&
           opacity == aOther.opacity;
  }
};

// A 3d transform stored as a 4 by 4 matrix in row-major order in memory.
//
// Transforms can be parametrized over the source and destination units, to describe a
// transformation from a space to another.
// For example, `TypedTransform3D<f32, WordSpace, ScreenSpace>::transform_point3d`
// takes a `TypedPoint3D<f32, WordSpace>` and returns a `TypedPoint3D<f32, ScreenSpace>`.
//
// Transforms expose a set of convenience methods for pre- and post-transformations.
// A pre-transformation corresponds to adding an operation that is applied before
// the rest of the transformation, while a post-transformation adds an operation
// that is applied after.
struct TypedTransform3D_f32__LayoutPixel__LayoutPixel {
  float m11;
  float m12;
  float m13;
  float m14;
  float m21;
  float m22;
  float m23;
  float m24;
  float m31;
  float m32;
  float m33;
  float m34;
  float m41;
  float m42;
  float m43;
  float m44;

  bool operator==(const TypedTransform3D_f32__LayoutPixel__LayoutPixel& aOther) const {
    return m11 == aOther.m11 &&
           m12 == aOther.m12 &&
           m13 == aOther.m13 &&
           m14 == aOther.m14 &&
           m21 == aOther.m21 &&
           m22 == aOther.m22 &&
           m23 == aOther.m23 &&
           m24 == aOther.m24 &&
           m31 == aOther.m31 &&
           m32 == aOther.m32 &&
           m33 == aOther.m33 &&
           m34 == aOther.m34 &&
           m41 == aOther.m41 &&
           m42 == aOther.m42 &&
           m43 == aOther.m43 &&
           m44 == aOther.m44;
  }
};

typedef TypedTransform3D_f32__LayoutPixel__LayoutPixel LayoutTransform;

struct WrTransformProperty {
  uint64_t id;
  LayoutTransform transform;
};

typedef IdNamespace WrIdNamespace;

// A 2d Point tagged with a unit.
struct TypedPoint2D_f32__WorldPixel {
  float x;
  float y;

  bool operator==(const TypedPoint2D_f32__WorldPixel& aOther) const {
    return x == aOther.x &&
           y == aOther.y;
  }
};

typedef TypedPoint2D_f32__WorldPixel WorldPoint;

// Represents RGBA screen colors with floating point numbers.
//
// All components must be between 0.0 and 1.0.
// An alpha value of 1.0 is opaque while 0.0 is fully transparent.
struct ColorF {
  float r;
  float g;
  float b;
  float a;

  bool operator==(const ColorF& aOther) const {
    return r == aOther.r &&
           g == aOther.g &&
           b == aOther.b &&
           a == aOther.a;
  }
};

// A 2d Point tagged with a unit.
struct TypedPoint2D_f32__LayerPixel {
  float x;
  float y;

  bool operator==(const TypedPoint2D_f32__LayerPixel& aOther) const {
    return x == aOther.x &&
           y == aOther.y;
  }
};

// A 2d Rectangle optionally tagged with a unit.
struct TypedRect_f32__LayerPixel {
  TypedPoint2D_f32__LayerPixel origin;
  TypedSize2D_f32__LayerPixel size;

  bool operator==(const TypedRect_f32__LayerPixel& aOther) const {
    return origin == aOther.origin &&
           size == aOther.size;
  }
};

typedef TypedRect_f32__LayerPixel LayerRect;

typedef LayerRect LayoutRect;

struct BorderRadius {
  LayoutSize top_left;
  LayoutSize top_right;
  LayoutSize bottom_left;
  LayoutSize bottom_right;

  bool operator==(const BorderRadius& aOther) const {
    return top_left == aOther.top_left &&
           top_right == aOther.top_right &&
           bottom_left == aOther.bottom_left &&
           bottom_right == aOther.bottom_right;
  }
};

struct ComplexClipRegion {
  // The boundaries of the rectangle.
  LayoutRect rect;
  // Border radii of this rectangle.
  BorderRadius radii;
  // Whether we are clipping inside or outside
  // the region.
  ClipMode mode;

  bool operator==(const ComplexClipRegion& aOther) const {
    return rect == aOther.rect &&
           radii == aOther.radii &&
           mode == aOther.mode;
  }
};

struct ImageKey {
  IdNamespace mNamespace;
  uint32_t mHandle;

  bool operator==(const ImageKey& aOther) const {
    return mNamespace == aOther.mNamespace &&
           mHandle == aOther.mHandle;
  }
  bool operator!=(const ImageKey& aOther) const {
    return mNamespace != aOther.mNamespace ||
           mHandle != aOther.mHandle;
  }
};

typedef ImageKey WrImageKey;

struct WrImageMask {
  WrImageKey image;
  LayoutRect rect;
  bool repeat;

  bool operator==(const WrImageMask& aOther) const {
    return image == aOther.image &&
           rect == aOther.rect &&
           repeat == aOther.repeat;
  }
};

// The minimum and maximum allowable offset for a sticky frame in a single dimension.
struct StickyOffsetBounds {
  // The minimum offset for this frame, typically a negative value, which specifies how
  // far in the negative direction the sticky frame can offset its contents in this
  // dimension.
  float min;
  // The maximum offset for this frame, typically a positive value, which specifies how
  // far in the positive direction the sticky frame can offset its contents in this
  // dimension.
  float max;

  bool operator==(const StickyOffsetBounds& aOther) const {
    return min == aOther.min &&
           max == aOther.max;
  }
};

// A 2d Vector tagged with a unit.
struct TypedVector2D_f32__LayerPixel {
  float x;
  float y;

  bool operator==(const TypedVector2D_f32__LayerPixel& aOther) const {
    return x == aOther.x &&
           y == aOther.y;
  }
};

typedef TypedVector2D_f32__LayerPixel LayerVector2D;

typedef LayerVector2D LayoutVector2D;

struct BorderWidths {
  float left;
  float top;
  float right;
  float bottom;

  bool operator==(const BorderWidths& aOther) const {
    return left == aOther.left &&
           top == aOther.top &&
           right == aOther.right &&
           bottom == aOther.bottom;
  }
};

struct BorderSide {
  ColorF color;
  BorderStyle style;

  bool operator==(const BorderSide& aOther) const {
    return color == aOther.color &&
           style == aOther.style;
  }
};

typedef TypedPoint2D_f32__LayerPixel LayerPoint;

typedef LayerPoint LayoutPoint;

struct GradientStop {
  float offset;
  ColorF color;

  bool operator==(const GradientStop& aOther) const {
    return offset == aOther.offset &&
           color == aOther.color;
  }
};

// The default side offset type with no unit.
struct SideOffsets2D_f32 {
  float top;
  float right;
  float bottom;
  float left;

  bool operator==(const SideOffsets2D_f32& aOther) const {
    return top == aOther.top &&
           right == aOther.right &&
           bottom == aOther.bottom &&
           left == aOther.left;
  }
};

// The default side offset type with no unit.
struct SideOffsets2D_u32 {
  uint32_t top;
  uint32_t right;
  uint32_t bottom;
  uint32_t left;

  bool operator==(const SideOffsets2D_u32& aOther) const {
    return top == aOther.top &&
           right == aOther.right &&
           bottom == aOther.bottom &&
           left == aOther.left;
  }
};

struct NinePatchDescriptor {
  uint32_t width;
  uint32_t height;
  SideOffsets2D_u32 slice;

  bool operator==(const NinePatchDescriptor& aOther) const {
    return width == aOther.width &&
           height == aOther.height &&
           slice == aOther.slice;
  }
};

struct Shadow {
  LayoutVector2D offset;
  ColorF color;
  float blur_radius;

  bool operator==(const Shadow& aOther) const {
    return offset == aOther.offset &&
           color == aOther.color &&
           blur_radius == aOther.blur_radius;
  }
};

struct WrAnimationProperty {
  WrAnimationType effect_type;
  uint64_t id;

  bool operator==(const WrAnimationProperty& aOther) const {
    return effect_type == aOther.effect_type &&
           id == aOther.id;
  }
};

struct WrFilterOp {
  WrFilterOpType filter_type;
  float argument;

  bool operator==(const WrFilterOp& aOther) const {
    return filter_type == aOther.filter_type &&
           argument == aOther.argument;
  }
};

struct FontInstanceKey {
  IdNamespace mNamespace;
  uint32_t mHandle;

  bool operator==(const FontInstanceKey& aOther) const {
    return mNamespace == aOther.mNamespace &&
           mHandle == aOther.mHandle;
  }
};

typedef FontInstanceKey WrFontInstanceKey;

typedef uint32_t GlyphIndex;

struct GlyphInstance {
  GlyphIndex index;
  LayoutPoint point;

  bool operator==(const GlyphInstance& aOther) const {
    return index == aOther.index &&
           point == aOther.point;
  }
};

struct GlyphOptions {
  FontRenderMode render_mode;

  bool operator==(const GlyphOptions& aOther) const {
    return render_mode == aOther.render_mode;
  }
};

typedef YuvColorSpace WrYuvColorSpace;

typedef LogLevelFilter WrLogLevelFilter;

struct ByteSlice {
  const uint8_t *buffer;
  size_t len;

  bool operator==(const ByteSlice& aOther) const {
    return buffer == aOther.buffer &&
           len == aOther.len;
  }
};

// A 2d Point tagged with a unit.
struct TypedPoint2D_u16__Tiles {
  uint16_t x;
  uint16_t y;

  bool operator==(const TypedPoint2D_u16__Tiles& aOther) const {
    return x == aOther.x &&
           y == aOther.y;
  }
};

typedef TypedPoint2D_u16__Tiles TileOffset;

struct MutByteSlice {
  uint8_t *buffer;
  size_t len;

  bool operator==(const MutByteSlice& aOther) const {
    return buffer == aOther.buffer &&
           len == aOther.len;
  }
};

struct WrWindowId {
  uint64_t mHandle;

  bool operator==(const WrWindowId& aOther) const {
    return mHandle == aOther.mHandle;
  }
  bool operator<(const WrWindowId& aOther) const {
    return mHandle < aOther.mHandle;
  }
  bool operator<=(const WrWindowId& aOther) const {
    return mHandle <= aOther.mHandle;
  }
};

struct WrDebugFlags {
  uint32_t mBits;

  bool operator==(const WrDebugFlags& aOther) const {
    return mBits == aOther.mBits;
  }
};

struct WrExternalImage {
  WrExternalImageType image_type;
  uint32_t handle;
  float u0;
  float v0;
  float u1;
  float v1;
  const uint8_t *buff;
  size_t size;

  bool operator==(const WrExternalImage& aOther) const {
    return image_type == aOther.image_type &&
           handle == aOther.handle &&
           u0 == aOther.u0 &&
           v0 == aOther.v0 &&
           u1 == aOther.u1 &&
           v1 == aOther.v1 &&
           buff == aOther.buff &&
           size == aOther.size;
  }
};

struct WrExternalImageId {
  uint64_t mHandle;

  bool operator==(const WrExternalImageId& aOther) const {
    return mHandle == aOther.mHandle;
  }
};

typedef WrExternalImage (*LockExternalImageCallback)(void*, WrExternalImageId, uint8_t);

typedef void (*UnlockExternalImageCallback)(void*, WrExternalImageId, uint8_t);

struct WrExternalImageHandler {
  void *external_image_obj;
  LockExternalImageCallback lock_func;
  UnlockExternalImageCallback unlock_func;

  bool operator==(const WrExternalImageHandler& aOther) const {
    return external_image_obj == aOther.external_image_obj &&
           lock_func == aOther.lock_func &&
           unlock_func == aOther.unlock_func;
  }
};

struct WrImageDescriptor {
  ImageFormat format;
  uint32_t width;
  uint32_t height;
  uint32_t stride;
  bool is_opaque;

  bool operator==(const WrImageDescriptor& aOther) const {
    return format == aOther.format &&
           width == aOther.width &&
           height == aOther.height &&
           stride == aOther.stride &&
           is_opaque == aOther.is_opaque;
  }
};

typedef ExternalImageType WrExternalImageBufferType;

// Represents RGBA screen colors with one byte per channel.
//
// If the alpha value `a` is 255 the color is opaque.
struct ColorU {
  uint8_t r;
  uint8_t g;
  uint8_t b;
  uint8_t a;

  bool operator==(const ColorU& aOther) const {
    return r == aOther.r &&
           g == aOther.g &&
           b == aOther.b &&
           a == aOther.a;
  }
};

struct FontInstanceOptions {
  FontRenderMode render_mode;
  SubpixelDirection subpx_dir;
  bool synthetic_italics;
  // When bg_color.a is != 0 and render_mode is FontRenderMode::Subpixel,
  // the text will be rendered with bg_color.r/g/b as an opaque estimated
  // background color.
  ColorU bg_color;

  bool operator==(const FontInstanceOptions& aOther) const {
    return render_mode == aOther.render_mode &&
           subpx_dir == aOther.subpx_dir &&
           synthetic_italics == aOther.synthetic_italics &&
           bg_color == aOther.bg_color;
  }
};

#if defined(XP_WIN)
struct FontInstancePlatformOptions {
  bool use_embedded_bitmap;
  bool force_gdi_rendering;

  bool operator==(const FontInstancePlatformOptions& aOther) const {
    return use_embedded_bitmap == aOther.use_embedded_bitmap &&
           force_gdi_rendering == aOther.force_gdi_rendering;
  }
};
#endif

#if defined(XP_MACOSX)
struct FontInstancePlatformOptions {
  bool font_smoothing;

  bool operator==(const FontInstancePlatformOptions& aOther) const {
    return font_smoothing == aOther.font_smoothing;
  }
};
#endif

#if !(defined(XP_MACOSX) || defined(XP_WIN))
struct FontInstancePlatformOptions {
  uint16_t flags;
  FontLCDFilter lcd_filter;
  FontHinting hinting;

  bool operator==(const FontInstancePlatformOptions& aOther) const {
    return flags == aOther.flags &&
           lcd_filter == aOther.lcd_filter &&
           hinting == aOther.hinting;
  }
};
#endif

// A 2d Point tagged with a unit.
struct TypedPoint2D_u32__DevicePixel {
  uint32_t x;
  uint32_t y;

  bool operator==(const TypedPoint2D_u32__DevicePixel& aOther) const {
    return x == aOther.x &&
           y == aOther.y;
  }
};

struct TypedSize2D_u32__DevicePixel {
  uint32_t width;
  uint32_t height;

  bool operator==(const TypedSize2D_u32__DevicePixel& aOther) const {
    return width == aOther.width &&
           height == aOther.height;
  }
};

// A 2d Rectangle optionally tagged with a unit.
struct TypedRect_u32__DevicePixel {
  TypedPoint2D_u32__DevicePixel origin;
  TypedSize2D_u32__DevicePixel size;

  bool operator==(const TypedRect_u32__DevicePixel& aOther) const {
    return origin == aOther.origin &&
           size == aOther.size;
  }
};

typedef TypedRect_u32__DevicePixel DeviceUintRect;

/* DO NOT MODIFY THIS MANUALLY! This file was generated using cbindgen.
 * To generate this file:
 *   1. Get the latest cbindgen using `cargo install --force cbindgen`
 *      a. Alternatively, you can clone `https://github.com/rlhunt/cbindgen` and use a tagged release
 *   2. Run `rustup run nightly cbindgen toolkit/library/rust/ --crate webrender_bindings -o gfx/webrender_bindings/webrender_ffi_generated.h`
 */

extern void AddFontData(WrFontKey aKey,
                        const uint8_t *aData,
                        size_t aSize,
                        uint32_t aIndex,
                        const ArcVecU8 *aVec);

extern void AddNativeFontHandle(WrFontKey aKey,
                                void *aHandle,
                                uint32_t aIndex);

extern void DeleteFontData(WrFontKey aKey);

extern void gecko_printf_stderr_output(const char *aMsg);

extern void gecko_profiler_register_thread(const char *aName);

extern void gecko_profiler_unregister_thread();

extern void gfx_critical_error(const char *aMsg);

extern void gfx_critical_note(const char *aMsg);

extern bool gfx_use_wrench();

extern const char *gfx_wr_resource_path_override();

extern bool is_glcontext_egl(void *aGlcontextPtr);

extern bool is_in_compositor_thread();

extern bool is_in_main_thread();

extern bool is_in_render_thread();

WR_INLINE
const VecU8 *wr_add_ref_arc(const ArcVecU8 *aArc)
WR_FUNC;

WR_INLINE
void wr_api_clear_display_list(DocumentHandle *aDh,
                               WrEpoch aEpoch,
                               WrPipelineId aPipelineId)
WR_FUNC;

WR_INLINE
void wr_api_clone(DocumentHandle *aDh,
                  DocumentHandle **aOutHandle)
WR_FUNC;

WR_INLINE
void wr_api_delete(DocumentHandle *aDh)
WR_DESTRUCTOR_SAFE_FUNC;

WR_INLINE
void wr_api_finalize_builder(WrState *aState,
                             LayoutSize *aContentSize,
                             BuiltDisplayListDescriptor *aDlDescriptor,
                             WrVecU8 *aDlData)
WR_FUNC;

WR_INLINE
void wr_api_generate_frame(DocumentHandle *aDh)
WR_FUNC;

WR_INLINE
void wr_api_generate_frame_with_properties(DocumentHandle *aDh,
                                           const WrOpacityProperty *aOpacityArray,
                                           size_t aOpacityCount,
                                           const WrTransformProperty *aTransformArray,
                                           size_t aTransformCount)
WR_FUNC;

WR_INLINE
WrIdNamespace wr_api_get_namespace(DocumentHandle *aDh)
WR_FUNC;

WR_INLINE
bool wr_api_hit_test(DocumentHandle *aDh,
                     WorldPoint aPoint,
                     WrPipelineId *aOutPipelineId,
                     uint64_t *aOutScrollId,
                     uint16_t *aOutHitInfo)
WR_FUNC;

WR_INLINE
void wr_api_remove_pipeline(DocumentHandle *aDh,
                            WrPipelineId aPipelineId)
WR_FUNC;

WR_INLINE
void wr_api_send_external_event(DocumentHandle *aDh,
                                size_t aEvt)
WR_DESTRUCTOR_SAFE_FUNC;

WR_INLINE
void wr_api_set_display_list(DocumentHandle *aDh,
                             ColorF aColor,
                             WrEpoch aEpoch,
                             float aViewportWidth,
                             float aViewportHeight,
                             WrPipelineId aPipelineId,
                             LayoutSize aContentSize,
                             BuiltDisplayListDescriptor aDlDescriptor,
                             WrVecU8 *aDlData,
                             ResourceUpdates *aResources)
WR_FUNC;

WR_INLINE
void wr_api_set_root_pipeline(DocumentHandle *aDh,
                              WrPipelineId aPipelineId)
WR_FUNC;

WR_INLINE
void wr_api_set_window_parameters(DocumentHandle *aDh,
                                  int32_t aWidth,
                                  int32_t aHeight)
WR_FUNC;

WR_INLINE
void wr_api_update_pipeline_resources(DocumentHandle *aDh,
                                      WrPipelineId aPipelineId,
                                      WrEpoch aEpoch,
                                      ResourceUpdates *aResources)
WR_FUNC;

WR_INLINE
void wr_api_update_resources(DocumentHandle *aDh,
                             ResourceUpdates *aResources)
WR_FUNC;

WR_INLINE
void wr_clear_item_tag(WrState *aState)
WR_FUNC;

WR_INLINE
void wr_dec_ref_arc(const VecU8 *aArc)
WR_FUNC;

WR_INLINE
void wr_dp_clear_save(WrState *aState)
WR_FUNC;

WR_INLINE
uint64_t wr_dp_define_clip(WrState *aState,
                           const uint64_t *aAncestorScrollId,
                           const uint64_t *aAncestorClipId,
                           LayoutRect aClipRect,
                           const ComplexClipRegion *aComplex,
                           size_t aComplexCount,
                           const WrImageMask *aMask)
WR_FUNC;

WR_INLINE
void wr_dp_define_scroll_layer(WrState *aState,
                               uint64_t aScrollId,
                               const uint64_t *aAncestorScrollId,
                               const uint64_t *aAncestorClipId,
                               LayoutRect aContentRect,
                               LayoutRect aClipRect)
WR_FUNC;

WR_INLINE
uint64_t wr_dp_define_sticky_frame(WrState *aState,
                                   LayoutRect aContentRect,
                                   const float *aTopMargin,
                                   const float *aRightMargin,
                                   const float *aBottomMargin,
                                   const float *aLeftMargin,
                                   StickyOffsetBounds aVerticalBounds,
                                   StickyOffsetBounds aHorizontalBounds,
                                   LayoutVector2D aAppliedOffset)
WR_FUNC;

WR_INLINE
void wr_dp_pop_all_shadows(WrState *aState)
WR_FUNC;

WR_INLINE
void wr_dp_pop_clip(WrState *aState)
WR_FUNC;

WR_INLINE
void wr_dp_pop_clip_and_scroll_info(WrState *aState)
WR_FUNC;

WR_INLINE
void wr_dp_pop_scroll_layer(WrState *aState)
WR_FUNC;

WR_INLINE
void wr_dp_pop_stacking_context(WrState *aState)
WR_FUNC;

WR_INLINE
void wr_dp_push_border(WrState *aState,
                       LayoutRect aRect,
                       LayoutRect aClip,
                       bool aIsBackfaceVisible,
                       BorderWidths aWidths,
                       BorderSide aTop,
                       BorderSide aRight,
                       BorderSide aBottom,
                       BorderSide aLeft,
                       BorderRadius aRadius)
WR_FUNC;

WR_INLINE
void wr_dp_push_border_gradient(WrState *aState,
                                LayoutRect aRect,
                                LayoutRect aClip,
                                bool aIsBackfaceVisible,
                                BorderWidths aWidths,
                                LayoutPoint aStartPoint,
                                LayoutPoint aEndPoint,
                                const GradientStop *aStops,
                                size_t aStopsCount,
                                ExtendMode aExtendMode,
                                SideOffsets2D_f32 aOutset)
WR_FUNC;

WR_INLINE
void wr_dp_push_border_image(WrState *aState,
                             LayoutRect aRect,
                             LayoutRect aClip,
                             bool aIsBackfaceVisible,
                             BorderWidths aWidths,
                             WrImageKey aImage,
                             NinePatchDescriptor aPatch,
                             SideOffsets2D_f32 aOutset,
                             RepeatMode aRepeatHorizontal,
                             RepeatMode aRepeatVertical)
WR_FUNC;

WR_INLINE
void wr_dp_push_border_radial_gradient(WrState *aState,
                                       LayoutRect aRect,
                                       LayoutRect aClip,
                                       bool aIsBackfaceVisible,
                                       BorderWidths aWidths,
                                       LayoutPoint aCenter,
                                       LayoutSize aRadius,
                                       const GradientStop *aStops,
                                       size_t aStopsCount,
                                       ExtendMode aExtendMode,
                                       SideOffsets2D_f32 aOutset)
WR_FUNC;

WR_INLINE
void wr_dp_push_box_shadow(WrState *aState,
                           LayoutRect aRect,
                           LayoutRect aClip,
                           bool aIsBackfaceVisible,
                           LayoutRect aBoxBounds,
                           LayoutVector2D aOffset,
                           ColorF aColor,
                           float aBlurRadius,
                           float aSpreadRadius,
                           BorderRadius aBorderRadius,
                           BoxShadowClipMode aClipMode)
WR_FUNC;

WR_INLINE
void wr_dp_push_clear_rect(WrState *aState,
                           LayoutRect aRect)
WR_FUNC;

WR_INLINE
void wr_dp_push_clip(WrState *aState,
                     uint64_t aClipId)
WR_FUNC;

WR_INLINE
void wr_dp_push_clip_and_scroll_info(WrState *aState,
                                     uint64_t aScrollId,
                                     const uint64_t *aClipId)
WR_FUNC;

WR_INLINE
void wr_dp_push_iframe(WrState *aState,
                       LayoutRect aRect,
                       bool aIsBackfaceVisible,
                       WrPipelineId aPipelineId)
WR_FUNC;

WR_INLINE
void wr_dp_push_image(WrState *aState,
                      LayoutRect aBounds,
                      LayoutRect aClip,
                      bool aIsBackfaceVisible,
                      LayoutSize aStretchSize,
                      LayoutSize aTileSpacing,
                      ImageRendering aImageRendering,
                      WrImageKey aKey)
WR_FUNC;

WR_INLINE
void wr_dp_push_line(WrState *aState,
                     const LayoutRect *aClip,
                     bool aIsBackfaceVisible,
                     const LayoutRect *aBounds,
                     float aWavyLineThickness,
                     LineOrientation aOrientation,
                     const ColorF *aColor,
                     LineStyle aStyle)
WR_FUNC;

WR_INLINE
void wr_dp_push_linear_gradient(WrState *aState,
                                LayoutRect aRect,
                                LayoutRect aClip,
                                bool aIsBackfaceVisible,
                                LayoutPoint aStartPoint,
                                LayoutPoint aEndPoint,
                                const GradientStop *aStops,
                                size_t aStopsCount,
                                ExtendMode aExtendMode,
                                LayoutSize aTileSize,
                                LayoutSize aTileSpacing)
WR_FUNC;

WR_INLINE
void wr_dp_push_radial_gradient(WrState *aState,
                                LayoutRect aRect,
                                LayoutRect aClip,
                                bool aIsBackfaceVisible,
                                LayoutPoint aCenter,
                                LayoutSize aRadius,
                                const GradientStop *aStops,
                                size_t aStopsCount,
                                ExtendMode aExtendMode,
                                LayoutSize aTileSize,
                                LayoutSize aTileSpacing)
WR_FUNC;

WR_INLINE
void wr_dp_push_rect(WrState *aState,
                     LayoutRect aRect,
                     LayoutRect aClip,
                     bool aIsBackfaceVisible,
                     ColorF aColor)
WR_FUNC;

WR_INLINE
void wr_dp_push_scroll_layer(WrState *aState,
                             uint64_t aScrollId)
WR_FUNC;

WR_INLINE
void wr_dp_push_shadow(WrState *aState,
                       LayoutRect aBounds,
                       LayoutRect aClip,
                       bool aIsBackfaceVisible,
                       Shadow aShadow)
WR_FUNC;

WR_INLINE
void wr_dp_push_stacking_context(WrState *aState,
                                 LayoutRect aBounds,
                                 const WrAnimationProperty *aAnimation,
                                 const float *aOpacity,
                                 const LayoutTransform *aTransform,
                                 TransformStyle aTransformStyle,
                                 const LayoutTransform *aPerspective,
                                 MixBlendMode aMixBlendMode,
                                 const WrFilterOp *aFilters,
                                 size_t aFilterCount,
                                 bool aIsBackfaceVisible)
WR_FUNC;

WR_INLINE
void wr_dp_push_text(WrState *aState,
                     LayoutRect aBounds,
                     LayoutRect aClip,
                     bool aIsBackfaceVisible,
                     ColorF aColor,
                     WrFontInstanceKey aFontKey,
                     const GlyphInstance *aGlyphs,
                     uint32_t aGlyphCount,
                     const GlyphOptions *aGlyphOptions)
WR_FUNC;

// Push a 2 planar NV12 image.
WR_INLINE
void wr_dp_push_yuv_NV12_image(WrState *aState,
                               LayoutRect aBounds,
                               LayoutRect aClip,
                               bool aIsBackfaceVisible,
                               WrImageKey aImageKey0,
                               WrImageKey aImageKey1,
                               WrYuvColorSpace aColorSpace,
                               ImageRendering aImageRendering)
WR_FUNC;

// Push a yuv interleaved image.
WR_INLINE
void wr_dp_push_yuv_interleaved_image(WrState *aState,
                                      LayoutRect aBounds,
                                      LayoutRect aClip,
                                      bool aIsBackfaceVisible,
                                      WrImageKey aImageKey0,
                                      WrYuvColorSpace aColorSpace,
                                      ImageRendering aImageRendering)
WR_FUNC;

// Push a 3 planar yuv image.
WR_INLINE
void wr_dp_push_yuv_planar_image(WrState *aState,
                                 LayoutRect aBounds,
                                 LayoutRect aClip,
                                 bool aIsBackfaceVisible,
                                 WrImageKey aImageKey0,
                                 WrImageKey aImageKey1,
                                 WrImageKey aImageKey2,
                                 WrYuvColorSpace aColorSpace,
                                 ImageRendering aImageRendering)
WR_FUNC;

WR_INLINE
void wr_dp_restore(WrState *aState)
WR_FUNC;

WR_INLINE
void wr_dp_save(WrState *aState)
WR_FUNC;

WR_INLINE
void wr_dump_display_list(WrState *aState)
WR_FUNC;

WR_INLINE
void wr_init_external_log_handler(WrLogLevelFilter aLogFilter)
WR_FUNC;

extern bool wr_moz2d_render_cb(ByteSlice aBlob,
                               uint32_t aWidth,
                               uint32_t aHeight,
                               ImageFormat aFormat,
                               const uint16_t *aTileSize,
                               const TileOffset *aTileOffset,
                               MutByteSlice aOutput);

extern void wr_notifier_external_event(WrWindowId aWindowId,
                                       size_t aRawEvent);

extern void wr_notifier_new_frame_ready(WrWindowId aWindowId);

extern void wr_notifier_new_scroll_frame_ready(WrWindowId aWindowId,
                                               bool aCompositeNeeded);

WR_INLINE
void wr_program_cache_delete(WrProgramCache *aProgramCache)
WR_DESTRUCTOR_SAFE_FUNC;

WR_INLINE
WrProgramCache *wr_program_cache_new()
WR_FUNC;

WR_INLINE
void wr_rendered_epochs_delete(WrRenderedEpochs *aPipelineEpochs)
WR_DESTRUCTOR_SAFE_FUNC;

WR_INLINE
bool wr_rendered_epochs_next(WrRenderedEpochs *aPipelineEpochs,
                             WrPipelineId *aOutPipeline,
                             WrEpoch *aOutEpoch)
WR_FUNC;

WR_INLINE
bool wr_renderer_current_epoch(Renderer *aRenderer,
                               WrPipelineId aPipelineId,
                               WrEpoch *aOutEpoch)
WR_FUNC;

WR_INLINE
void wr_renderer_delete(Renderer *aRenderer)
WR_DESTRUCTOR_SAFE_FUNC;

WR_INLINE
WrRenderedEpochs *wr_renderer_flush_rendered_epochs(Renderer *aRenderer)
WR_FUNC;

WR_INLINE
WrDebugFlags wr_renderer_get_debug_flags(Renderer *aRenderer)
WR_FUNC;

WR_INLINE
void wr_renderer_readback(Renderer *aRenderer,
                          uint32_t aWidth,
                          uint32_t aHeight,
                          uint8_t *aDstBuffer,
                          size_t aBufferSize)
WR_FUNC;

WR_INLINE
bool wr_renderer_render(Renderer *aRenderer,
                        uint32_t aWidth,
                        uint32_t aHeight)
WR_FUNC;

WR_INLINE
void wr_renderer_set_debug_flags(Renderer *aRenderer,
                                 WrDebugFlags aFlags)
WR_FUNC;

WR_INLINE
void wr_renderer_set_external_image_handler(Renderer *aRenderer,
                                            WrExternalImageHandler *aExternalImageHandler)
WR_FUNC;

WR_INLINE
void wr_renderer_update(Renderer *aRenderer)
WR_FUNC;

WR_INLINE
void wr_renderer_update_program_cache(Renderer *aRenderer,
                                      WrProgramCache *aProgramCache)
WR_FUNC;

WR_INLINE
void wr_resource_updates_add_blob_image(ResourceUpdates *aResources,
                                        WrImageKey aImageKey,
                                        const WrImageDescriptor *aDescriptor,
                                        WrVecU8 *aBytes)
WR_FUNC;

WR_INLINE
void wr_resource_updates_add_external_image(ResourceUpdates *aResources,
                                            WrImageKey aImageKey,
                                            const WrImageDescriptor *aDescriptor,
                                            WrExternalImageId aExternalImageId,
                                            WrExternalImageBufferType aBufferType,
                                            uint8_t aChannelIndex)
WR_FUNC;

WR_INLINE
void wr_resource_updates_add_font_descriptor(ResourceUpdates *aResources,
                                             WrFontKey aKey,
                                             WrVecU8 *aBytes,
                                             uint32_t aIndex)
WR_FUNC;

WR_INLINE
void wr_resource_updates_add_font_instance(ResourceUpdates *aResources,
                                           WrFontInstanceKey aKey,
                                           WrFontKey aFontKey,
                                           float aGlyphSize,
                                           const FontInstanceOptions *aOptions,
                                           const FontInstancePlatformOptions *aPlatformOptions,
                                           WrVecU8 *aVariations)
WR_FUNC;

WR_INLINE
void wr_resource_updates_add_image(ResourceUpdates *aResources,
                                   WrImageKey aImageKey,
                                   const WrImageDescriptor *aDescriptor,
                                   WrVecU8 *aBytes)
WR_FUNC;

WR_INLINE
void wr_resource_updates_add_raw_font(ResourceUpdates *aResources,
                                      WrFontKey aKey,
                                      WrVecU8 *aBytes,
                                      uint32_t aIndex)
WR_FUNC;

WR_INLINE
void wr_resource_updates_clear(ResourceUpdates *aResources)
WR_FUNC;

WR_INLINE
void wr_resource_updates_delete(ResourceUpdates *aUpdates)
WR_DESTRUCTOR_SAFE_FUNC;

WR_INLINE
void wr_resource_updates_delete_font(ResourceUpdates *aResources,
                                     WrFontKey aKey)
WR_FUNC;

WR_INLINE
void wr_resource_updates_delete_font_instance(ResourceUpdates *aResources,
                                              WrFontInstanceKey aKey)
WR_FUNC;

WR_INLINE
void wr_resource_updates_delete_image(ResourceUpdates *aResources,
                                      WrImageKey aKey)
WR_FUNC;

WR_INLINE
ResourceUpdates *wr_resource_updates_new()
WR_FUNC;

WR_INLINE
void wr_resource_updates_update_blob_image(ResourceUpdates *aResources,
                                           WrImageKey aImageKey,
                                           const WrImageDescriptor *aDescriptor,
                                           WrVecU8 *aBytes,
                                           DeviceUintRect aDirtyRect)
WR_FUNC;

WR_INLINE
void wr_resource_updates_update_external_image(ResourceUpdates *aResources,
                                               WrImageKey aKey,
                                               const WrImageDescriptor *aDescriptor,
                                               WrExternalImageId aExternalImageId,
                                               WrExternalImageBufferType aImageType,
                                               uint8_t aChannelIndex)
WR_FUNC;

WR_INLINE
void wr_resource_updates_update_image(ResourceUpdates *aResources,
                                      WrImageKey aKey,
                                      const WrImageDescriptor *aDescriptor,
                                      WrVecU8 *aBytes)
WR_FUNC;

WR_INLINE
void wr_scroll_layer_with_id(DocumentHandle *aDh,
                             WrPipelineId aPipelineId,
                             uint64_t aScrollId,
                             LayoutPoint aNewScrollOrigin)
WR_FUNC;

WR_INLINE
void wr_set_item_tag(WrState *aState,
                     uint64_t aScrollId,
                     uint16_t aHitInfo)
WR_FUNC;

WR_INLINE
void wr_shutdown_external_log_handler()
WR_FUNC;

WR_INLINE
void wr_state_delete(WrState *aState)
WR_DESTRUCTOR_SAFE_FUNC;

WR_INLINE
WrState *wr_state_new(WrPipelineId aPipelineId,
                      LayoutSize aContentSize,
                      size_t aCapacity)
WR_FUNC;

WR_INLINE
void wr_thread_pool_delete(WrThreadPool *aThreadPool)
WR_DESTRUCTOR_SAFE_FUNC;

WR_INLINE
WrThreadPool *wr_thread_pool_new()
WR_FUNC;

WR_INLINE
void wr_vec_u8_free(WrVecU8 aV)
WR_FUNC;

WR_INLINE
void wr_vec_u8_push_bytes(WrVecU8 *aV,
                          ByteSlice aBytes)
WR_FUNC;

WR_INLINE
bool wr_window_new(WrWindowId aWindowId,
                   uint32_t aWindowWidth,
                   uint32_t aWindowHeight,
                   void *aGlContext,
                   WrThreadPool *aThreadPool,
                   DocumentHandle **aOutHandle,
                   Renderer **aOutRenderer,
                   uint32_t *aOutMaxTextureSize)
WR_FUNC;

} // namespace wr
} // namespace mozilla

} // extern "C"

/* DO NOT MODIFY THIS MANUALLY! This file was generated using cbindgen.
 * To generate this file:
 *   1. Get the latest cbindgen using `cargo install --force cbindgen`
 *      a. Alternatively, you can clone `https://github.com/rlhunt/cbindgen` and use a tagged release
 *   2. Run `rustup run nightly cbindgen toolkit/library/rust/ --crate webrender_bindings -o gfx/webrender_bindings/webrender_ffi_generated.h`
 */
