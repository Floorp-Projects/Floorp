/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Generated with cbindgen:0.6.8 */

/* DO NOT MODIFY THIS MANUALLY! This file was generated using cbindgen.
 * To generate this file:
 *   1. Get the latest cbindgen using `cargo install --force cbindgen`
 *      a. Alternatively, you can clone `https://github.com/eqrion/cbindgen` and use a tagged release
 *   2. Run `rustup run nightly cbindgen toolkit/library/rust/ --lockfile Cargo.lock --crate webrender_bindings -o gfx/webrender_bindings/webrender_ffi_generated.h`
 */

#include <cstdarg>
#include <cstdint>
#include <cstdlib>

namespace mozilla {
namespace wr {

/// Whether a border should be antialiased.
enum class AntialiasBorder {
  No = 0,
  Yes,

  Sentinel /* this must be last for serialization purposes. */
};

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

enum class Checkpoint : uint32_t {
  SceneBuilt,
  FrameBuilt,
  FrameTexturesUpdated,
  FrameRendered,
  /// NotificationRequests get notified with this if they get dropped without having been
  /// notified. This provides the guarantee that if a request is created it will get notified.
  TransactionDropped,

  Sentinel /* this must be last for serialization purposes. */
};

enum class ClipMode {
  Clip,
  ClipOut,

  Sentinel /* this must be last for serialization purposes. */
};

/// Specifies the color depth of an image. Currently only used for YUV images.
enum class ColorDepth : uint8_t {
  /// 8 bits image (most common)
  Color8,
  /// 10 bits image
  Color10,
  /// 12 bits image
  Color12,
  /// 16 bits image
  Color16,

  Sentinel /* this must be last for serialization purposes. */
};

enum class ExtendMode : uint8_t {
  Clamp,
  Repeat,

  Sentinel /* this must be last for serialization purposes. */
};

#if !(defined(XP_MACOSX) || defined(XP_WIN))
enum class FontHinting : uint8_t {
  None,
  Mono,
  Light,
  Normal,
  LCD,

  Sentinel /* this must be last for serialization purposes. */
};
#endif

#if !(defined(XP_MACOSX) || defined(XP_WIN))
enum class FontLCDFilter : uint8_t {
  None,
  Default,
  Light,
  Legacy,

  Sentinel /* this must be last for serialization purposes. */
};
#endif

enum class FontRenderMode : uint32_t {
  Mono = 0,
  Alpha,
  Subpixel,

  Sentinel /* this must be last for serialization purposes. */
};

/// Specifies the format of a series of pixels, in driver terms.
enum class ImageFormat : uint32_t {
  /// One-channel, byte storage. The "red" doesn't map to the color
  /// red per se, and is just the way that OpenGL has historically referred
  /// to single-channel buffers.
  R8 = 1,
  /// One-channel, short storage
  R16 = 2,
  /// Four channels, byte storage.
  BGRA8 = 3,
  /// Four channels, float storage.
  RGBAF32 = 4,
  /// Two-channels, byte storage. Similar to `R8`, this just means
  /// "two channels" rather than "red and green".
  RG8 = 5,
  /// Four channels, signed integer storage.
  RGBAI32 = 6,
  /// Four channels, byte storage.
  RGBA8 = 7,

  Sentinel /* this must be last for serialization purposes. */
};

enum class ImageRendering : uint32_t {
  Auto = 0,
  CrispEdges = 1,
  Pixelated = 2,

  Sentinel /* this must be last for serialization purposes. */
};

enum class LineOrientation : uint8_t {
  Vertical,
  Horizontal,

  Sentinel /* this must be last for serialization purposes. */
};

enum class LineStyle : uint8_t {
  Solid,
  Dotted,
  Dashed,
  Wavy,

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

/// Used to indicate if an image is opaque, or has an alpha channel.
enum class OpacityType : uint8_t {
  Opaque = 0,
  HasAlphaChannel = 1,

  Sentinel /* this must be last for serialization purposes. */
};

enum class RepeatMode : uint32_t {
  Stretch,
  Repeat,
  Round,
  Space,

  Sentinel /* this must be last for serialization purposes. */
};

enum class TelemetryProbe {
  SceneBuildTime = 0,
  SceneSwapTime = 1,
  RenderTime = 2,

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

enum class WrExternalImageBufferType {
  TextureHandle = 0,
  TextureRectHandle = 1,
  TextureArrayHandle = 2,
  TextureExternalHandle = 3,
  ExternalBuffer = 4,

  Sentinel /* this must be last for serialization purposes. */
};

enum class WrExternalImageType : uint32_t {
  RawData,
  NativeTexture,
  Invalid,

  Sentinel /* this must be last for serialization purposes. */
};

enum class YuvColorSpace : uint32_t {
  Rec601 = 0,
  Rec709 = 1,

  Sentinel /* this must be last for serialization purposes. */
};

template<typename T>
struct Arc;

struct Device;

/// Geometry in the coordinate system of the render target (screen or intermediate
/// surface) in physical pixels.
struct DevicePixel;

struct DocumentHandle;

/// Geometry in a stacking context's local coordinate space (logical pixels).
struct LayoutPixel;

/// The renderer is responsible for submitting to the GPU the work prepared by the
/// RenderBackend.
/// We have a separate `Renderer` instance for each instance of WebRender (generally
/// one per OS window), and all instances share the same thread.
struct Renderer;

/// Unit for tile coordinates.
struct TileCoordinate;

/// Represents the work associated to a transaction before scene building.
struct Transaction;

/// The default unit.
struct UnknownUnit;

template<typename T>
struct Vec;

/// Geometry in the document's coordinate space (logical pixels).
struct WorldPixel;

struct WrProgramCache;

struct WrShaders;

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

struct FontInstanceKey {
  IdNamespace mNamespace;
  uint32_t mHandle;

  bool operator==(const FontInstanceKey& aOther) const {
    return mNamespace == aOther.mNamespace &&
           mHandle == aOther.mHandle;
  }
};

using WrFontInstanceKey = FontInstanceKey;

struct FontKey {
  IdNamespace mNamespace;
  uint32_t mHandle;

  bool operator==(const FontKey& aOther) const {
    return mNamespace == aOther.mNamespace &&
           mHandle == aOther.mHandle;
  }
};

using WrFontKey = FontKey;

/// Represents RGBA screen colors with one byte per channel.
/// If the alpha value `a` is 255 the color is opaque.
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

struct SyntheticItalics {
  int16_t angle;

  bool operator==(const SyntheticItalics& aOther) const {
    return angle == aOther.angle;
  }
};

struct FontInstanceOptions {
  FontRenderMode render_mode;
  FontInstanceFlags flags;
  /// When bg_color.a is != 0 and render_mode is FontRenderMode::Subpixel,
  /// the text will be rendered with bg_color.r/g/b as an opaque estimated
  /// background color.
  ColorU bg_color;
  SyntheticItalics synthetic_italics;

  bool operator==(const FontInstanceOptions& aOther) const {
    return render_mode == aOther.render_mode &&
           flags == aOther.flags &&
           bg_color == aOther.bg_color &&
           synthetic_italics == aOther.synthetic_italics;
  }
};

#if defined(XP_WIN)
struct FontInstancePlatformOptions {
  uint16_t gamma;
  uint16_t contrast;

  bool operator==(const FontInstancePlatformOptions& aOther) const {
    return gamma == aOther.gamma &&
           contrast == aOther.contrast;
  }
};
#endif

#if defined(XP_MACOSX)
struct FontInstancePlatformOptions {
  uint32_t unused;

  bool operator==(const FontInstancePlatformOptions& aOther) const {
    return unused == aOther.unused;
  }
};
#endif

#if !(defined(XP_MACOSX) || defined(XP_WIN))
struct FontInstancePlatformOptions {
  FontLCDFilter lcd_filter;
  FontHinting hinting;

  bool operator==(const FontInstancePlatformOptions& aOther) const {
    return lcd_filter == aOther.lcd_filter &&
           hinting == aOther.hinting;
  }
};
#endif

struct FontVariation {
  uint32_t tag;
  float value;

  bool operator==(const FontVariation& aOther) const {
    return tag == aOther.tag &&
           value == aOther.value;
  }
};

using VecU8 = Vec<uint8_t>;

using ArcVecU8 = Arc<VecU8>;

using WrIdNamespace = IdNamespace;

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

/// This type carries no valuable semantics for WR. However, it reflects the fact that
/// clients (Servo) may generate pipelines by different semi-independent sources.
/// These pipelines still belong to the same `IdNamespace` and the same `DocumentId`.
/// Having this extra Id field enables them to generate `PipelineId` without collision.
using PipelineSourceId = uint32_t;

/// From the point of view of WR, `PipelineId` is completely opaque and generic as long as
/// it's clonable, serializable, comparable, and hashable.
struct PipelineId {
  PipelineSourceId mNamespace;
  uint32_t mHandle;

  bool operator==(const PipelineId& aOther) const {
    return mNamespace == aOther.mNamespace &&
           mHandle == aOther.mHandle;
  }
};

using WrPipelineId = PipelineId;

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

using WrEpoch = Epoch;

struct WrPipelineEpoch {
  WrPipelineId pipeline_id;
  WrEpoch epoch;

  bool operator==(const WrPipelineEpoch& aOther) const {
    return pipeline_id == aOther.pipeline_id &&
           epoch == aOther.epoch;
  }
};

template<typename T>
struct FfiVec {
  const T *data;
  uintptr_t length;
  uintptr_t capacity;

  bool operator==(const FfiVec& aOther) const {
    return data == aOther.data &&
           length == aOther.length &&
           capacity == aOther.capacity;
  }
};

struct WrPipelineInfo {
  FfiVec<WrPipelineEpoch> epochs;
  FfiVec<PipelineId> removed_pipelines;

  bool operator==(const WrPipelineInfo& aOther) const {
    return epochs == aOther.epochs &&
           removed_pipelines == aOther.removed_pipelines;
  }
};

/// Collection of heap sizes, in bytes.
struct MemoryReport {
  uintptr_t clip_stores;
  uintptr_t gpu_cache_metadata;
  uintptr_t gpu_cache_cpu_mirror;
  uintptr_t render_tasks;
  uintptr_t hit_testers;
  uintptr_t fonts;
  uintptr_t images;
  uintptr_t rasterized_blobs;
  uintptr_t shader_cache;
  InterningMemoryReport interning;
  uintptr_t gpu_cache_textures;
  uintptr_t vertex_data_textures;
  uintptr_t render_target_textures;
  uintptr_t texture_cache_textures;
  uintptr_t depth_target_textures;
  uintptr_t swap_chain;
};

/// A 2d size tagged with a unit.
template<typename T, typename U>
struct TypedSize2D {
  T width;
  T height;

  bool operator==(const TypedSize2D& aOther) const {
    return width == aOther.width &&
           height == aOther.height;
  }
};

using DeviceIntSize = TypedSize2D<int32_t, DevicePixel>;

using LayoutSize = TypedSize2D<float, LayoutPixel>;

/// Describes the memory layout of a display list.
/// A display list consists of some number of display list items, followed by a number of display
/// items.
struct BuiltDisplayListDescriptor {
  /// The first IPC time stamp: before any work has been done
  uint64_t builder_start_time;
  /// The second IPC time stamp: after serialization
  uint64_t builder_finish_time;
  /// The third IPC time stamp: just before sending
  uint64_t send_start_time;
  /// The amount of clipping nodes created while building this display list.
  uintptr_t total_clip_nodes;
  /// The amount of spatial nodes created while building this display list.
  uintptr_t total_spatial_nodes;

  bool operator==(const BuiltDisplayListDescriptor& aOther) const {
    return builder_start_time == aOther.builder_start_time &&
           builder_finish_time == aOther.builder_finish_time &&
           send_start_time == aOther.send_start_time &&
           total_clip_nodes == aOther.total_clip_nodes &&
           total_spatial_nodes == aOther.total_spatial_nodes;
  }
};

struct WrVecU8 {
  uint8_t *data;
  uintptr_t length;
  uintptr_t capacity;

  bool operator==(const WrVecU8& aOther) const {
    return data == aOther.data &&
           length == aOther.length &&
           capacity == aOther.capacity;
  }
};

/// A 2d Point tagged with a unit.
template<typename T, typename U>
struct TypedPoint2D {
  T x;
  T y;

  bool operator==(const TypedPoint2D& aOther) const {
    return x == aOther.x &&
           y == aOther.y;
  }
};

using WorldPoint = TypedPoint2D<float, WorldPixel>;

struct WrDebugFlags {
  uint32_t mBits;

  bool operator==(const WrDebugFlags& aOther) const {
    return mBits == aOther.mBits;
  }
};

struct WrClipId {
  uintptr_t id;

  bool operator==(const WrClipId& aOther) const {
    return id == aOther.id;
  }
};

struct WrSpatialId {
  uintptr_t id;

  bool operator==(const WrSpatialId& aOther) const {
    return id == aOther.id;
  }
};

struct WrSpaceAndClip {
  WrSpatialId space;
  WrClipId clip;

  bool operator==(const WrSpaceAndClip& aOther) const {
    return space == aOther.space &&
           clip == aOther.clip;
  }
};

/// A 2d Rectangle optionally tagged with a unit.
template<typename T, typename U>
struct TypedRect {
  TypedPoint2D<T, U> origin;
  TypedSize2D<T, U> size;

  bool operator==(const TypedRect& aOther) const {
    return origin == aOther.origin &&
           size == aOther.size;
  }
};

using LayoutRect = TypedRect<float, LayoutPixel>;

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
  /// The boundaries of the rectangle.
  LayoutRect rect;
  /// Border radii of this rectangle.
  BorderRadius radii;
  /// Whether we are clipping inside or outside
  /// the region.
  ClipMode mode;

  bool operator==(const ComplexClipRegion& aOther) const {
    return rect == aOther.rect &&
           radii == aOther.radii &&
           mode == aOther.mode;
  }
};

/// An opaque identifier describing an image registered with WebRender.
/// This is used as a handle to reference images, and is used as the
/// hash map key for the actual image storage in the `ResourceCache`.
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

using WrImageKey = ImageKey;

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

struct WrSpaceAndClipChain {
  WrSpatialId space;
  uint64_t clip_chain;

  bool operator==(const WrSpaceAndClipChain& aOther) const {
    return space == aOther.space &&
           clip_chain == aOther.clip_chain;
  }
};

/// The minimum and maximum allowable offset for a sticky frame in a single dimension.
struct StickyOffsetBounds {
  /// The minimum offset for this frame, typically a negative value, which specifies how
  /// far in the negative direction the sticky frame can offset its contents in this
  /// dimension.
  float min;
  /// The maximum offset for this frame, typically a positive value, which specifies how
  /// far in the positive direction the sticky frame can offset its contents in this
  /// dimension.
  float max;

  bool operator==(const StickyOffsetBounds& aOther) const {
    return min == aOther.min &&
           max == aOther.max;
  }
};

/// A 2d Vector tagged with a unit.
template<typename T, typename U>
struct TypedVector2D {
  T x;
  T y;

  bool operator==(const TypedVector2D& aOther) const {
    return x == aOther.x &&
           y == aOther.y;
  }
};

using LayoutVector2D = TypedVector2D<float, LayoutPixel>;

/// A group of side offsets, which correspond to top/left/bottom/right for borders, padding,
/// and margins in CSS, optionally tagged with a unit.
template<typename T, typename U>
struct TypedSideOffsets2D {
  T top;
  T right;
  T bottom;
  T left;

  bool operator==(const TypedSideOffsets2D& aOther) const {
    return top == aOther.top &&
           right == aOther.right &&
           bottom == aOther.bottom &&
           left == aOther.left;
  }
};

using LayoutSideOffsets = TypedSideOffsets2D<float, LayoutPixel>;

/// Represents RGBA screen colors with floating point numbers.
/// All components must be between 0.0 and 1.0.
/// An alpha value of 1.0 is opaque while 0.0 is fully transparent.
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

struct BorderSide {
  ColorF color;
  BorderStyle style;

  bool operator==(const BorderSide& aOther) const {
    return color == aOther.color &&
           style == aOther.style;
  }
};

/// The default side offset type with no unit.
template<typename T>
using SideOffsets2D = TypedSideOffsets2D<T, UnknownUnit>;

using LayoutPoint = TypedPoint2D<float, LayoutPixel>;

struct GradientStop {
  float offset;
  ColorF color;

  bool operator==(const GradientStop& aOther) const {
    return offset == aOther.offset &&
           color == aOther.color;
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

struct WrStackingContextClip {
  enum class Tag {
    None,
    ClipId,
    ClipChain,

    Sentinel /* this must be last for serialization purposes. */
  };

  struct ClipId_Body {
    WrClipId _0;

    bool operator==(const ClipId_Body& aOther) const {
      return _0 == aOther._0;
    }
  };

  struct ClipChain_Body {
    uint64_t _0;

    bool operator==(const ClipChain_Body& aOther) const {
      return _0 == aOther._0;
    }
  };

  Tag tag;
  union {
    ClipId_Body clip_id;
    ClipChain_Body clip_chain;
  };

  static WrStackingContextClip None() {
    WrStackingContextClip result;
    result.tag = Tag::None;
    return result;
  }

  static WrStackingContextClip ClipId(const WrClipId &a0) {
    WrStackingContextClip result;
    result.clip_id._0 = a0;
    result.tag = Tag::ClipId;
    return result;
  }

  static WrStackingContextClip ClipChain(const uint64_t &a0) {
    WrStackingContextClip result;
    result.clip_chain._0 = a0;
    result.tag = Tag::ClipChain;
    return result;
  }

  bool IsNone() const {
    return tag == Tag::None;
  }

  bool IsClipId() const {
    return tag == Tag::ClipId;
  }

  bool IsClipChain() const {
    return tag == Tag::ClipChain;
  }

  bool operator==(const WrStackingContextClip& aOther) const {
    if (tag != aOther.tag) {
      return false;
    }
    switch (tag) {
      case Tag::ClipId: return clip_id == aOther.clip_id;
      case Tag::ClipChain: return clip_chain == aOther.clip_chain;
      default: return true;
    }
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

/// A 3d transform stored as a 4 by 4 matrix in row-major order in memory.
/// Transforms can be parametrized over the source and destination units, to describe a
/// transformation from a space to another.
/// For example, `TypedTransform3D<f32, WorldSpace, ScreenSpace>::transform_point3d`
/// takes a `TypedPoint3D<f32, WorldSpace>` and returns a `TypedPoint3D<f32, ScreenSpace>`.
/// Transforms expose a set of convenience methods for pre- and post-transformations.
/// A pre-transformation corresponds to adding an operation that is applied before
/// the rest of the transformation, while a post-transformation adds an operation
/// that is applied after.
template<typename T, typename Src, typename Dst>
struct TypedTransform3D {
  T m11;
  T m12;
  T m13;
  T m14;
  T m21;
  T m22;
  T m23;
  T m24;
  T m31;
  T m32;
  T m33;
  T m34;
  T m41;
  T m42;
  T m43;
  T m44;

  bool operator==(const TypedTransform3D& aOther) const {
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

using LayoutTransform = TypedTransform3D<float, LayoutPixel, LayoutPixel>;

struct PropertyBindingId {
  IdNamespace namespace_;
  uint32_t uid;

  bool operator==(const PropertyBindingId& aOther) const {
    return namespace_ == aOther.namespace_ &&
           uid == aOther.uid;
  }
};

/// A unique key that is used for connecting animated property
/// values to bindings in the display list.
template<typename T>
struct PropertyBindingKey {
  PropertyBindingId id;

  bool operator==(const PropertyBindingKey& aOther) const {
    return id == aOther.id;
  }
};

/// A binding property can either be a specific value
/// (the normal, non-animated case) or point to a binding location
/// to fetch the current value from.
/// Note that Binding has also a non-animated value, the value is
/// used for the case where the animation is still in-delay phase
/// (i.e. the animation doesn't produce any animation values).
template<typename T>
struct PropertyBinding {
  enum class Tag {
    Value,
    Binding,

    Sentinel /* this must be last for serialization purposes. */
  };

  struct Value_Body {
    T _0;

    bool operator==(const Value_Body& aOther) const {
      return _0 == aOther._0;
    }
  };

  struct Binding_Body {
    PropertyBindingKey<T> _0;
    T _1;

    bool operator==(const Binding_Body& aOther) const {
      return _0 == aOther._0 &&
             _1 == aOther._1;
    }
  };

  Tag tag;
  union {
    Value_Body value;
    Binding_Body binding;
  };

  static PropertyBinding Value(const T &a0) {
    PropertyBinding result;
    result.value._0 = a0;
    result.tag = Tag::Value;
    return result;
  }

  static PropertyBinding Binding(const PropertyBindingKey<T> &a0,
                                 const T &a1) {
    PropertyBinding result;
    result.binding._0 = a0;
    result.binding._1 = a1;
    result.tag = Tag::Binding;
    return result;
  }

  bool IsValue() const {
    return tag == Tag::Value;
  }

  bool IsBinding() const {
    return tag == Tag::Binding;
  }

  bool operator==(const PropertyBinding& aOther) const {
    if (tag != aOther.tag) {
      return false;
    }
    switch (tag) {
      case Tag::Value: return value == aOther.value;
      case Tag::Binding: return binding == aOther.binding;
      default: return true;
    }
  }
};

struct FilterOp {
  enum class Tag {
    /// Filter that does no transformation of the colors, needed for
    /// debug purposes only.
    Identity,
    Blur,
    Brightness,
    Contrast,
    Grayscale,
    HueRotate,
    Invert,
    Opacity,
    Saturate,
    Sepia,
    DropShadow,
    ColorMatrix,
    SrgbToLinear,
    LinearToSrgb,

    Sentinel /* this must be last for serialization purposes. */
  };

  struct Blur_Body {
    float _0;

    bool operator==(const Blur_Body& aOther) const {
      return _0 == aOther._0;
    }
  };

  struct Brightness_Body {
    float _0;

    bool operator==(const Brightness_Body& aOther) const {
      return _0 == aOther._0;
    }
  };

  struct Contrast_Body {
    float _0;

    bool operator==(const Contrast_Body& aOther) const {
      return _0 == aOther._0;
    }
  };

  struct Grayscale_Body {
    float _0;

    bool operator==(const Grayscale_Body& aOther) const {
      return _0 == aOther._0;
    }
  };

  struct HueRotate_Body {
    float _0;

    bool operator==(const HueRotate_Body& aOther) const {
      return _0 == aOther._0;
    }
  };

  struct Invert_Body {
    float _0;

    bool operator==(const Invert_Body& aOther) const {
      return _0 == aOther._0;
    }
  };

  struct Opacity_Body {
    PropertyBinding<float> _0;
    float _1;

    bool operator==(const Opacity_Body& aOther) const {
      return _0 == aOther._0 &&
             _1 == aOther._1;
    }
  };

  struct Saturate_Body {
    float _0;

    bool operator==(const Saturate_Body& aOther) const {
      return _0 == aOther._0;
    }
  };

  struct Sepia_Body {
    float _0;

    bool operator==(const Sepia_Body& aOther) const {
      return _0 == aOther._0;
    }
  };

  struct DropShadow_Body {
    LayoutVector2D _0;
    float _1;
    ColorF _2;

    bool operator==(const DropShadow_Body& aOther) const {
      return _0 == aOther._0 &&
             _1 == aOther._1 &&
             _2 == aOther._2;
    }
  };

  struct ColorMatrix_Body {
    float _0[20];
  };

  Tag tag;
  union {
    Blur_Body blur;
    Brightness_Body brightness;
    Contrast_Body contrast;
    Grayscale_Body grayscale;
    HueRotate_Body hue_rotate;
    Invert_Body invert;
    Opacity_Body opacity;
    Saturate_Body saturate;
    Sepia_Body sepia;
    DropShadow_Body drop_shadow;
    ColorMatrix_Body color_matrix;
  };

  static FilterOp Identity() {
    FilterOp result;
    result.tag = Tag::Identity;
    return result;
  }

  static FilterOp Blur(const float &a0) {
    FilterOp result;
    result.blur._0 = a0;
    result.tag = Tag::Blur;
    return result;
  }

  static FilterOp Brightness(const float &a0) {
    FilterOp result;
    result.brightness._0 = a0;
    result.tag = Tag::Brightness;
    return result;
  }

  static FilterOp Contrast(const float &a0) {
    FilterOp result;
    result.contrast._0 = a0;
    result.tag = Tag::Contrast;
    return result;
  }

  static FilterOp Grayscale(const float &a0) {
    FilterOp result;
    result.grayscale._0 = a0;
    result.tag = Tag::Grayscale;
    return result;
  }

  static FilterOp HueRotate(const float &a0) {
    FilterOp result;
    result.hue_rotate._0 = a0;
    result.tag = Tag::HueRotate;
    return result;
  }

  static FilterOp Invert(const float &a0) {
    FilterOp result;
    result.invert._0 = a0;
    result.tag = Tag::Invert;
    return result;
  }

  static FilterOp Opacity(const PropertyBinding<float> &a0,
                          const float &a1) {
    FilterOp result;
    result.opacity._0 = a0;
    result.opacity._1 = a1;
    result.tag = Tag::Opacity;
    return result;
  }

  static FilterOp Saturate(const float &a0) {
    FilterOp result;
    result.saturate._0 = a0;
    result.tag = Tag::Saturate;
    return result;
  }

  static FilterOp Sepia(const float &a0) {
    FilterOp result;
    result.sepia._0 = a0;
    result.tag = Tag::Sepia;
    return result;
  }

  static FilterOp DropShadow(const LayoutVector2D &a0,
                             const float &a1,
                             const ColorF &a2) {
    FilterOp result;
    result.drop_shadow._0 = a0;
    result.drop_shadow._1 = a1;
    result.drop_shadow._2 = a2;
    result.tag = Tag::DropShadow;
    return result;
  }

  static FilterOp ColorMatrix(const float (&a0)[20]) {
    FilterOp result;
    for (int i = 0; i < 20; i++) {result.color_matrix._0[i] = a0[i];}
    result.tag = Tag::ColorMatrix;
    return result;
  }

  static FilterOp SrgbToLinear() {
    FilterOp result;
    result.tag = Tag::SrgbToLinear;
    return result;
  }

  static FilterOp LinearToSrgb() {
    FilterOp result;
    result.tag = Tag::LinearToSrgb;
    return result;
  }

  bool IsIdentity() const {
    return tag == Tag::Identity;
  }

  bool IsBlur() const {
    return tag == Tag::Blur;
  }

  bool IsBrightness() const {
    return tag == Tag::Brightness;
  }

  bool IsContrast() const {
    return tag == Tag::Contrast;
  }

  bool IsGrayscale() const {
    return tag == Tag::Grayscale;
  }

  bool IsHueRotate() const {
    return tag == Tag::HueRotate;
  }

  bool IsInvert() const {
    return tag == Tag::Invert;
  }

  bool IsOpacity() const {
    return tag == Tag::Opacity;
  }

  bool IsSaturate() const {
    return tag == Tag::Saturate;
  }

  bool IsSepia() const {
    return tag == Tag::Sepia;
  }

  bool IsDropShadow() const {
    return tag == Tag::DropShadow;
  }

  bool IsColorMatrix() const {
    return tag == Tag::ColorMatrix;
  }

  bool IsSrgbToLinear() const {
    return tag == Tag::SrgbToLinear;
  }

  bool IsLinearToSrgb() const {
    return tag == Tag::LinearToSrgb;
  }
};

/// Configure whether the contents of a stacking context
/// should be rasterized in local space or screen space.
/// Local space rasterized pictures are typically used
/// when we want to cache the output, and performance is
/// important. Note that this is a performance hint only,
/// which WR may choose to ignore.
union RasterSpace {
  enum class Tag : uint32_t {
    Local,
    Screen,

    Sentinel /* this must be last for serialization purposes. */
  };

  struct Local_Body {
    Tag tag;
    float _0;

    bool operator==(const Local_Body& aOther) const {
      return _0 == aOther._0;
    }
  };

  struct {
    Tag tag;
  };
  Local_Body local;

  static RasterSpace Local(const float &a0) {
    RasterSpace result;
    result.local._0 = a0;
    result.tag = Tag::Local;
    return result;
  }

  static RasterSpace Screen() {
    RasterSpace result;
    result.tag = Tag::Screen;
    return result;
  }

  bool IsLocal() const {
    return tag == Tag::Local;
  }

  bool IsScreen() const {
    return tag == Tag::Screen;
  }

  bool operator==(const RasterSpace& aOther) const {
    if (tag != aOther.tag) {
      return false;
    }
    switch (tag) {
      case Tag::Local: return local == aOther.local;
      default: return true;
    }
  }
};

using GlyphIndex = uint32_t;

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
  FontInstanceFlags flags;

  bool operator==(const GlyphOptions& aOther) const {
    return render_mode == aOther.render_mode &&
           flags == aOther.flags;
  }
};

using WrColorDepth = ColorDepth;

using WrYuvColorSpace = YuvColorSpace;

struct ByteSlice {
  const uint8_t *buffer;
  uintptr_t len;

  bool operator==(const ByteSlice& aOther) const {
    return buffer == aOther.buffer &&
           len == aOther.len;
  }
};

using TileOffset = TypedPoint2D<int32_t, TileCoordinate>;

using LayoutIntRect = TypedRect<int32_t, LayoutPixel>;

struct MutByteSlice {
  uint8_t *buffer;
  uintptr_t len;

  bool operator==(const MutByteSlice& aOther) const {
    return buffer == aOther.buffer &&
           len == aOther.len;
  }
};

/// A C function that takes a pointer to a heap allocation and returns its size.
/// This is borrowed from the malloc_size_of crate, upon which we want to avoid
/// a dependency from WebRender.
using VoidPtrToSizeFn = uintptr_t(*)(const void *ptr);

struct RendererStats {
  uintptr_t total_draw_calls;
  uintptr_t alpha_target_count;
  uintptr_t color_target_count;
  uintptr_t texture_upload_kb;
  uint64_t resource_upload_time;
  uint64_t gpu_cache_upload_time;

  bool operator==(const RendererStats& aOther) const {
    return total_draw_calls == aOther.total_draw_calls &&
           alpha_target_count == aOther.alpha_target_count &&
           color_target_count == aOther.color_target_count &&
           texture_upload_kb == aOther.texture_upload_kb &&
           resource_upload_time == aOther.resource_upload_time &&
           gpu_cache_upload_time == aOther.gpu_cache_upload_time;
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
  uintptr_t size;

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

using LockExternalImageCallback = WrExternalImage(*)(void*, WrExternalImageId, uint8_t, ImageRendering);

using UnlockExternalImageCallback = void(*)(void*, WrExternalImageId, uint8_t);

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

/// An opaque identifier describing a blob image registered with WebRender.
/// This is used as a handle to reference blob images, and can be used as an
/// image in display items.
struct BlobImageKey {
  ImageKey _0;

  bool operator==(const BlobImageKey& aOther) const {
    return _0 == aOther._0;
  }
};

struct WrImageDescriptor {
  ImageFormat format;
  int32_t width;
  int32_t height;
  int32_t stride;
  OpacityType opacity;

  bool operator==(const WrImageDescriptor& aOther) const {
    return format == aOther.format &&
           width == aOther.width &&
           height == aOther.height &&
           stride == aOther.stride &&
           opacity == aOther.opacity;
  }
};

using DeviceIntRect = TypedRect<int32_t, DevicePixel>;

struct WrTransformProperty {
  uint64_t id;
  LayoutTransform transform;
};

struct WrOpacityProperty {
  uint64_t id;
  float opacity;

  bool operator==(const WrOpacityProperty& aOther) const {
    return id == aOther.id &&
           opacity == aOther.opacity;
  }
};

extern "C" {

extern void AddBlobFont(WrFontInstanceKey aInstanceKey,
                        WrFontKey aFontKey,
                        float aSize,
                        const FontInstanceOptions *aOptions,
                        const FontInstancePlatformOptions *aPlatformOptions,
                        const FontVariation *aVariations,
                        uintptr_t aNumVariations);

extern void AddFontData(WrFontKey aKey,
                        const uint8_t *aData,
                        uintptr_t aSize,
                        uint32_t aIndex,
                        const ArcVecU8 *aVec);

extern void AddNativeFontHandle(WrFontKey aKey,
                                void *aHandle,
                                uint32_t aIndex);

extern void ClearBlobImageResources(WrIdNamespace aNamespace);

extern void DeleteBlobFont(WrFontInstanceKey aKey);

extern void DeleteFontData(WrFontKey aKey);

#if defined(ANDROID)
extern int __android_log_write(int aPrio,
                               const char *aTag,
                               const char *aText);
#endif

extern void apz_deregister_sampler(WrWindowId aWindowId);

extern void apz_deregister_updater(WrWindowId aWindowId);

extern void apz_post_scene_swap(WrWindowId aWindowId,
                                WrPipelineInfo aPipelineInfo);

extern void apz_pre_scene_swap(WrWindowId aWindowId);

extern void apz_register_sampler(WrWindowId aWindowId);

extern void apz_register_updater(WrWindowId aWindowId);

extern void apz_run_updater(WrWindowId aWindowId);

extern void apz_sample_transforms(WrWindowId aWindowId,
                                  Transaction *aTransaction);

extern void gecko_profiler_end_marker(const char *aName);

extern void gecko_profiler_register_thread(const char *aName);

extern void gecko_profiler_start_marker(const char *aName);

extern void gecko_profiler_unregister_thread();

extern void gfx_critical_error(const char *aMsg);

extern void gfx_critical_note(const char *aMsg);

extern bool gfx_use_wrench();

extern const char *gfx_wr_resource_path_override();

extern bool is_glcontext_angle(void *aGlcontextPtr);

extern bool is_glcontext_egl(void *aGlcontextPtr);

extern bool is_in_compositor_thread();

extern bool is_in_main_thread();

extern bool is_in_render_thread();

extern void record_telemetry_time(TelemetryProbe aProbe,
                                  uint64_t aTimeNs);

WR_INLINE
bool remove_program_binary_disk_cache(const nsAString *aProfPath)
WR_FUNC;

WR_INLINE
const VecU8 *wr_add_ref_arc(const ArcVecU8 *aArc)
WR_FUNC;

WR_INLINE
void wr_api_accumulate_memory_report(DocumentHandle *aDh,
                                     MemoryReport *aReport)
WR_FUNC;

WR_INLINE
void wr_api_capture(DocumentHandle *aDh,
                    const char *aPath,
                    uint32_t aBitsRaw)
WR_FUNC;

WR_INLINE
void wr_api_clear_all_caches(DocumentHandle *aDh)
WR_DESTRUCTOR_SAFE_FUNC;

WR_INLINE
void wr_api_clone(DocumentHandle *aDh,
                  DocumentHandle **aOutHandle)
WR_FUNC;

WR_INLINE
void wr_api_create_document(DocumentHandle *aRootDh,
                            DocumentHandle **aOutHandle,
                            DeviceIntSize aDocSize,
                            int8_t aLayer)
WR_FUNC;

WR_INLINE
void wr_api_delete(DocumentHandle *aDh)
WR_DESTRUCTOR_SAFE_FUNC;

WR_INLINE
void wr_api_delete_document(DocumentHandle *aDh)
WR_DESTRUCTOR_SAFE_FUNC;

WR_INLINE
void wr_api_finalize_builder(WrState *aState,
                             LayoutSize *aContentSize,
                             BuiltDisplayListDescriptor *aDlDescriptor,
                             WrVecU8 *aDlData)
WR_FUNC;

WR_INLINE
void wr_api_flush_scene_builder(DocumentHandle *aDh)
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
void wr_api_notify_memory_pressure(DocumentHandle *aDh)
WR_FUNC;

WR_INLINE
void wr_api_send_external_event(DocumentHandle *aDh,
                                uintptr_t aEvt)
WR_DESTRUCTOR_SAFE_FUNC;

WR_INLINE
void wr_api_send_transaction(DocumentHandle *aDh,
                             Transaction *aTransaction,
                             bool aIsAsync)
WR_FUNC;

WR_INLINE
void wr_api_set_debug_flags(DocumentHandle *aDh,
                            WrDebugFlags aFlags)
WR_FUNC;

WR_INLINE
void wr_api_shut_down(DocumentHandle *aDh)
WR_DESTRUCTOR_SAFE_FUNC;

WR_INLINE
void wr_api_wake_scene_builder(DocumentHandle *aDh)
WR_FUNC;

WR_INLINE
void wr_clear_item_tag(WrState *aState)
WR_FUNC;

WR_INLINE
void wr_dec_ref_arc(const VecU8 *aArc)
WR_DESTRUCTOR_SAFE_FUNC;

WR_INLINE
void wr_device_delete(Device *aDevice)
WR_DESTRUCTOR_SAFE_FUNC;

WR_INLINE
void wr_dp_clear_save(WrState *aState)
WR_FUNC;

WR_INLINE
WrClipId wr_dp_define_clip_with_parent_clip(WrState *aState,
                                            const WrSpaceAndClip *aParent,
                                            LayoutRect aClipRect,
                                            const ComplexClipRegion *aComplex,
                                            uintptr_t aComplexCount,
                                            const WrImageMask *aMask)
WR_FUNC;

WR_INLINE
WrClipId wr_dp_define_clip_with_parent_clip_chain(WrState *aState,
                                                  const WrSpaceAndClipChain *aParent,
                                                  LayoutRect aClipRect,
                                                  const ComplexClipRegion *aComplex,
                                                  uintptr_t aComplexCount,
                                                  const WrImageMask *aMask)
WR_FUNC;

WR_INLINE
uint64_t wr_dp_define_clipchain(WrState *aState,
                                const uint64_t *aParentClipchainId,
                                const WrClipId *aClips,
                                uintptr_t aClipsCount)
WR_FUNC;

WR_INLINE
WrSpaceAndClip wr_dp_define_scroll_layer(WrState *aState,
                                         uint64_t aExternalScrollId,
                                         const WrSpaceAndClip *aParent,
                                         LayoutRect aContentRect,
                                         LayoutRect aClipRect)
WR_FUNC;

WR_INLINE
WrSpatialId wr_dp_define_sticky_frame(WrState *aState,
                                      WrSpatialId aParentSpatialId,
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
void wr_dp_pop_stacking_context(WrState *aState,
                                bool aIsReferenceFrame)
WR_FUNC;

WR_INLINE
void wr_dp_push_border(WrState *aState,
                       LayoutRect aRect,
                       LayoutRect aClip,
                       bool aIsBackfaceVisible,
                       const WrSpaceAndClipChain *aParent,
                       AntialiasBorder aDoAa,
                       LayoutSideOffsets aWidths,
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
                                const WrSpaceAndClipChain *aParent,
                                LayoutSideOffsets aWidths,
                                int32_t aWidth,
                                int32_t aHeight,
                                SideOffsets2D<int32_t> aSlice,
                                LayoutPoint aStartPoint,
                                LayoutPoint aEndPoint,
                                const GradientStop *aStops,
                                uintptr_t aStopsCount,
                                ExtendMode aExtendMode,
                                SideOffsets2D<float> aOutset)
WR_FUNC;

WR_INLINE
void wr_dp_push_border_image(WrState *aState,
                             LayoutRect aRect,
                             LayoutRect aClip,
                             bool aIsBackfaceVisible,
                             const WrSpaceAndClipChain *aParent,
                             LayoutSideOffsets aWidths,
                             WrImageKey aImage,
                             int32_t aWidth,
                             int32_t aHeight,
                             SideOffsets2D<int32_t> aSlice,
                             SideOffsets2D<float> aOutset,
                             RepeatMode aRepeatHorizontal,
                             RepeatMode aRepeatVertical)
WR_FUNC;

WR_INLINE
void wr_dp_push_border_radial_gradient(WrState *aState,
                                       LayoutRect aRect,
                                       LayoutRect aClip,
                                       bool aIsBackfaceVisible,
                                       const WrSpaceAndClipChain *aParent,
                                       LayoutSideOffsets aWidths,
                                       LayoutPoint aCenter,
                                       LayoutSize aRadius,
                                       const GradientStop *aStops,
                                       uintptr_t aStopsCount,
                                       ExtendMode aExtendMode,
                                       SideOffsets2D<float> aOutset)
WR_FUNC;

WR_INLINE
void wr_dp_push_box_shadow(WrState *aState,
                           LayoutRect aRect,
                           LayoutRect aClip,
                           bool aIsBackfaceVisible,
                           const WrSpaceAndClipChain *aParent,
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
                           LayoutRect aRect,
                           LayoutRect aClip,
                           const WrSpaceAndClipChain *aParent)
WR_FUNC;

WR_INLINE
void wr_dp_push_clear_rect_with_parent_clip(WrState *aState,
                                            LayoutRect aRect,
                                            LayoutRect aClip,
                                            const WrSpaceAndClip *aParent)
WR_FUNC;

WR_INLINE
void wr_dp_push_iframe(WrState *aState,
                       LayoutRect aRect,
                       LayoutRect aClip,
                       bool aIsBackfaceVisible,
                       const WrSpaceAndClipChain *aParent,
                       WrPipelineId aPipelineId,
                       bool aIgnoreMissingPipeline)
WR_FUNC;

WR_INLINE
void wr_dp_push_image(WrState *aState,
                      LayoutRect aBounds,
                      LayoutRect aClip,
                      bool aIsBackfaceVisible,
                      const WrSpaceAndClipChain *aParent,
                      LayoutSize aStretchSize,
                      LayoutSize aTileSpacing,
                      ImageRendering aImageRendering,
                      WrImageKey aKey,
                      bool aPremultipliedAlpha,
                      ColorF aColor)
WR_FUNC;

WR_INLINE
void wr_dp_push_line(WrState *aState,
                     const LayoutRect *aClip,
                     bool aIsBackfaceVisible,
                     const WrSpaceAndClipChain *aParent,
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
                                const WrSpaceAndClipChain *aParent,
                                LayoutPoint aStartPoint,
                                LayoutPoint aEndPoint,
                                const GradientStop *aStops,
                                uintptr_t aStopsCount,
                                ExtendMode aExtendMode,
                                LayoutSize aTileSize,
                                LayoutSize aTileSpacing)
WR_FUNC;

WR_INLINE
void wr_dp_push_radial_gradient(WrState *aState,
                                LayoutRect aRect,
                                LayoutRect aClip,
                                bool aIsBackfaceVisible,
                                const WrSpaceAndClipChain *aParent,
                                LayoutPoint aCenter,
                                LayoutSize aRadius,
                                const GradientStop *aStops,
                                uintptr_t aStopsCount,
                                ExtendMode aExtendMode,
                                LayoutSize aTileSize,
                                LayoutSize aTileSpacing)
WR_FUNC;

WR_INLINE
void wr_dp_push_rect(WrState *aState,
                     LayoutRect aRect,
                     LayoutRect aClip,
                     bool aIsBackfaceVisible,
                     const WrSpaceAndClipChain *aParent,
                     ColorF aColor)
WR_FUNC;

WR_INLINE
void wr_dp_push_rect_with_parent_clip(WrState *aState,
                                      LayoutRect aRect,
                                      LayoutRect aClip,
                                      bool aIsBackfaceVisible,
                                      const WrSpaceAndClip *aParent,
                                      ColorF aColor)
WR_FUNC;

WR_INLINE
void wr_dp_push_shadow(WrState *aState,
                       LayoutRect aBounds,
                       LayoutRect aClip,
                       bool aIsBackfaceVisible,
                       const WrSpaceAndClipChain *aParent,
                       Shadow aShadow)
WR_FUNC;

WR_INLINE
WrSpatialId wr_dp_push_stacking_context(WrState *aState,
                                        LayoutRect aBounds,
                                        WrSpatialId aSpatialId,
                                        const WrStackingContextClip *aClip,
                                        const WrAnimationProperty *aAnimation,
                                        const float *aOpacity,
                                        const LayoutTransform *aTransform,
                                        TransformStyle aTransformStyle,
                                        const LayoutTransform *aPerspective,
                                        MixBlendMode aMixBlendMode,
                                        const FilterOp *aFilters,
                                        uintptr_t aFilterCount,
                                        bool aIsBackfaceVisible,
                                        RasterSpace aGlyphRasterSpace)
WR_FUNC;

WR_INLINE
void wr_dp_push_text(WrState *aState,
                     LayoutRect aBounds,
                     LayoutRect aClip,
                     bool aIsBackfaceVisible,
                     const WrSpaceAndClipChain *aParent,
                     ColorF aColor,
                     WrFontInstanceKey aFontKey,
                     const GlyphInstance *aGlyphs,
                     uint32_t aGlyphCount,
                     const GlyphOptions *aGlyphOptions)
WR_FUNC;

/// Push a 2 planar NV12 image.
WR_INLINE
void wr_dp_push_yuv_NV12_image(WrState *aState,
                               LayoutRect aBounds,
                               LayoutRect aClip,
                               bool aIsBackfaceVisible,
                               const WrSpaceAndClipChain *aParent,
                               WrImageKey aImageKey0,
                               WrImageKey aImageKey1,
                               WrColorDepth aColorDepth,
                               WrYuvColorSpace aColorSpace,
                               ImageRendering aImageRendering)
WR_FUNC;

/// Push a yuv interleaved image.
WR_INLINE
void wr_dp_push_yuv_interleaved_image(WrState *aState,
                                      LayoutRect aBounds,
                                      LayoutRect aClip,
                                      bool aIsBackfaceVisible,
                                      const WrSpaceAndClipChain *aParent,
                                      WrImageKey aImageKey0,
                                      WrColorDepth aColorDepth,
                                      WrYuvColorSpace aColorSpace,
                                      ImageRendering aImageRendering)
WR_FUNC;

/// Push a 3 planar yuv image.
WR_INLINE
void wr_dp_push_yuv_planar_image(WrState *aState,
                                 LayoutRect aBounds,
                                 LayoutRect aClip,
                                 bool aIsBackfaceVisible,
                                 const WrSpaceAndClipChain *aParent,
                                 WrImageKey aImageKey0,
                                 WrImageKey aImageKey1,
                                 WrImageKey aImageKey2,
                                 WrColorDepth aColorDepth,
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
uintptr_t wr_dump_display_list(WrState *aState,
                               uintptr_t aIndent,
                               const uintptr_t *aStart,
                               const uintptr_t *aEnd)
WR_FUNC;

extern void wr_finished_scene_build(WrWindowId aWindowId,
                                    WrPipelineInfo aPipelineInfo);

extern bool wr_moz2d_render_cb(ByteSlice aBlob,
                               int32_t aWidth,
                               int32_t aHeight,
                               ImageFormat aFormat,
                               const uint16_t *aTileSize,
                               const TileOffset *aTileOffset,
                               const LayoutIntRect *aDirtyRect,
                               MutByteSlice aOutput);

extern void wr_notifier_external_event(WrWindowId aWindowId,
                                       uintptr_t aRawEvent);

extern void wr_notifier_new_frame_ready(WrWindowId aWindowId);

extern void wr_notifier_nop_frame_done(WrWindowId aWindowId);

extern void wr_notifier_wake_up(WrWindowId aWindowId);

WR_INLINE
void wr_pipeline_info_delete(WrPipelineInfo aInfo)
WR_DESTRUCTOR_SAFE_FUNC;

WR_INLINE
void wr_program_cache_delete(WrProgramCache *aProgramCache)
WR_DESTRUCTOR_SAFE_FUNC;

WR_INLINE
WrProgramCache *wr_program_cache_new(const nsAString *aProfPath,
                                     WrThreadPool *aThreadPool)
WR_FUNC;

WR_INLINE
uintptr_t wr_program_cache_report_memory(const WrProgramCache *aCache,
                                         VoidPtrToSizeFn aSizeOfOp)
WR_FUNC;

WR_INLINE
void wr_renderer_accumulate_memory_report(Renderer *aRenderer,
                                          MemoryReport *aReport)
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
WrPipelineInfo wr_renderer_flush_pipeline_info(Renderer *aRenderer)
WR_FUNC;

WR_INLINE
void wr_renderer_readback(Renderer *aRenderer,
                          int32_t aWidth,
                          int32_t aHeight,
                          uint8_t *aDstBuffer,
                          uintptr_t aBufferSize)
WR_FUNC;

WR_INLINE
bool wr_renderer_render(Renderer *aRenderer,
                        int32_t aWidth,
                        int32_t aHeight,
                        bool aHadSlowFrame,
                        RendererStats *aOutStats)
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
void wr_resource_updates_add_blob_image(Transaction *aTxn,
                                        BlobImageKey aImageKey,
                                        const WrImageDescriptor *aDescriptor,
                                        WrVecU8 *aBytes)
WR_FUNC;

WR_INLINE
void wr_resource_updates_add_external_image(Transaction *aTxn,
                                            WrImageKey aImageKey,
                                            const WrImageDescriptor *aDescriptor,
                                            WrExternalImageId aExternalImageId,
                                            WrExternalImageBufferType aBufferType,
                                            uint8_t aChannelIndex)
WR_FUNC;

WR_INLINE
void wr_resource_updates_add_font_descriptor(Transaction *aTxn,
                                             WrFontKey aKey,
                                             WrVecU8 *aBytes,
                                             uint32_t aIndex)
WR_FUNC;

WR_INLINE
void wr_resource_updates_add_font_instance(Transaction *aTxn,
                                           WrFontInstanceKey aKey,
                                           WrFontKey aFontKey,
                                           float aGlyphSize,
                                           const FontInstanceOptions *aOptions,
                                           const FontInstancePlatformOptions *aPlatformOptions,
                                           WrVecU8 *aVariations)
WR_FUNC;

WR_INLINE
void wr_resource_updates_add_image(Transaction *aTxn,
                                   WrImageKey aImageKey,
                                   const WrImageDescriptor *aDescriptor,
                                   WrVecU8 *aBytes)
WR_FUNC;

WR_INLINE
void wr_resource_updates_add_raw_font(Transaction *aTxn,
                                      WrFontKey aKey,
                                      WrVecU8 *aBytes,
                                      uint32_t aIndex)
WR_FUNC;

WR_INLINE
void wr_resource_updates_clear(Transaction *aTxn)
WR_FUNC;

WR_INLINE
void wr_resource_updates_delete_blob_image(Transaction *aTxn,
                                           BlobImageKey aKey)
WR_FUNC;

WR_INLINE
void wr_resource_updates_delete_font(Transaction *aTxn,
                                     WrFontKey aKey)
WR_FUNC;

WR_INLINE
void wr_resource_updates_delete_font_instance(Transaction *aTxn,
                                              WrFontInstanceKey aKey)
WR_FUNC;

WR_INLINE
void wr_resource_updates_delete_image(Transaction *aTxn,
                                      WrImageKey aKey)
WR_FUNC;

WR_INLINE
void wr_resource_updates_set_blob_image_visible_area(Transaction *aTxn,
                                                     BlobImageKey aKey,
                                                     const DeviceIntRect *aArea)
WR_FUNC;

WR_INLINE
void wr_resource_updates_update_blob_image(Transaction *aTxn,
                                           BlobImageKey aImageKey,
                                           const WrImageDescriptor *aDescriptor,
                                           WrVecU8 *aBytes,
                                           LayoutIntRect aDirtyRect)
WR_FUNC;

WR_INLINE
void wr_resource_updates_update_external_image(Transaction *aTxn,
                                               WrImageKey aKey,
                                               const WrImageDescriptor *aDescriptor,
                                               WrExternalImageId aExternalImageId,
                                               WrExternalImageBufferType aImageType,
                                               uint8_t aChannelIndex)
WR_FUNC;

WR_INLINE
void wr_resource_updates_update_external_image_with_dirty_rect(Transaction *aTxn,
                                                               WrImageKey aKey,
                                                               const WrImageDescriptor *aDescriptor,
                                                               WrExternalImageId aExternalImageId,
                                                               WrExternalImageBufferType aImageType,
                                                               uint8_t aChannelIndex,
                                                               DeviceIntRect aDirtyRect)
WR_FUNC;

WR_INLINE
void wr_resource_updates_update_image(Transaction *aTxn,
                                      WrImageKey aKey,
                                      const WrImageDescriptor *aDescriptor,
                                      WrVecU8 *aBytes)
WR_FUNC;

WR_INLINE
WrClipId wr_root_clip_id()
WR_FUNC;

WR_INLINE
WrSpatialId wr_root_scroll_node_id()
WR_FUNC;

extern void wr_schedule_render(WrWindowId aWindowId);

WR_INLINE
void wr_set_item_tag(WrState *aState,
                     uint64_t aScrollId,
                     uint16_t aHitInfo)
WR_FUNC;

WR_INLINE
void wr_shaders_delete(WrShaders *aShaders,
                       void *aGlContext)
WR_DESTRUCTOR_SAFE_FUNC;

WR_INLINE
WrShaders *wr_shaders_new(void *aGlContext,
                          WrProgramCache *aProgramCache)
WR_FUNC;

WR_INLINE
void wr_state_delete(WrState *aState)
WR_DESTRUCTOR_SAFE_FUNC;

WR_INLINE
WrState *wr_state_new(WrPipelineId aPipelineId,
                      LayoutSize aContentSize,
                      uintptr_t aCapacity)
WR_FUNC;

WR_INLINE
void wr_thread_pool_delete(WrThreadPool *aThreadPool)
WR_DESTRUCTOR_SAFE_FUNC;

WR_INLINE
WrThreadPool *wr_thread_pool_new()
WR_FUNC;

WR_INLINE
void wr_transaction_append_transform_properties(Transaction *aTxn,
                                                const WrTransformProperty *aTransformArray,
                                                uintptr_t aTransformCount)
WR_FUNC;

WR_INLINE
void wr_transaction_clear_display_list(Transaction *aTxn,
                                       WrEpoch aEpoch,
                                       WrPipelineId aPipelineId)
WR_FUNC;

WR_INLINE
void wr_transaction_delete(Transaction *aTxn)
WR_DESTRUCTOR_SAFE_FUNC;

WR_INLINE
void wr_transaction_generate_frame(Transaction *aTxn)
WR_FUNC;

WR_INLINE
void wr_transaction_invalidate_rendered_frame(Transaction *aTxn)
WR_FUNC;

WR_INLINE
bool wr_transaction_is_empty(const Transaction *aTxn)
WR_FUNC;

WR_INLINE
Transaction *wr_transaction_new(bool aDoAsync)
WR_FUNC;

extern void wr_transaction_notification_notified(uintptr_t aHandler,
                                                 Checkpoint aWhen);

WR_INLINE
void wr_transaction_notify(Transaction *aTxn,
                           Checkpoint aWhen,
                           uintptr_t aEvent)
WR_FUNC;

WR_INLINE
void wr_transaction_pinch_zoom(Transaction *aTxn,
                               float aPinchZoom)
WR_FUNC;

WR_INLINE
void wr_transaction_remove_pipeline(Transaction *aTxn,
                                    WrPipelineId aPipelineId)
WR_FUNC;

WR_INLINE
bool wr_transaction_resource_updates_is_empty(const Transaction *aTxn)
WR_FUNC;

WR_INLINE
void wr_transaction_scroll_layer(Transaction *aTxn,
                                 WrPipelineId aPipelineId,
                                 uint64_t aScrollId,
                                 LayoutPoint aNewScrollOrigin)
WR_FUNC;

WR_INLINE
void wr_transaction_set_display_list(Transaction *aTxn,
                                     WrEpoch aEpoch,
                                     ColorF aBackground,
                                     float aViewportWidth,
                                     float aViewportHeight,
                                     WrPipelineId aPipelineId,
                                     LayoutSize aContentSize,
                                     BuiltDisplayListDescriptor aDlDescriptor,
                                     WrVecU8 *aDlData)
WR_FUNC;

WR_INLINE
void wr_transaction_set_low_priority(Transaction *aTxn,
                                     bool aLowPriority)
WR_FUNC;

WR_INLINE
void wr_transaction_set_root_pipeline(Transaction *aTxn,
                                      WrPipelineId aPipelineId)
WR_FUNC;

WR_INLINE
void wr_transaction_set_window_parameters(Transaction *aTxn,
                                          const DeviceIntSize *aWindowSize,
                                          const DeviceIntRect *aDocRect)
WR_FUNC;

WR_INLINE
void wr_transaction_update_dynamic_properties(Transaction *aTxn,
                                              const WrOpacityProperty *aOpacityArray,
                                              uintptr_t aOpacityCount,
                                              const WrTransformProperty *aTransformArray,
                                              uintptr_t aTransformCount)
WR_FUNC;

WR_INLINE
void wr_transaction_update_epoch(Transaction *aTxn,
                                 WrPipelineId aPipelineId,
                                 WrEpoch aEpoch)
WR_FUNC;

WR_INLINE
void wr_try_load_shader_from_disk(WrProgramCache *aProgramCache)
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
                   int32_t aWindowWidth,
                   int32_t aWindowHeight,
                   bool aSupportLowPriorityTransactions,
                   bool aEnablePictureCaching,
                   void *aGlContext,
                   WrProgramCache *aProgramCache,
                   WrShaders *aShaders,
                   WrThreadPool *aThreadPool,
                   VoidPtrToSizeFn aSizeOfOp,
                   VoidPtrToSizeFn aEnclosingSizeOfOp,
                   DocumentHandle **aOutHandle,
                   Renderer **aOutRenderer,
                   int32_t *aOutMaxTextureSize)
WR_FUNC;

} // extern "C"

} // namespace wr
} // namespace mozilla
