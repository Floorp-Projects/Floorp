/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Generated with cbindgen:0.1.19 */

/* DO NOT MODIFY THIS MANUALLY! This file was generated using cbindgen.
 * To generate this file:
 *   1. Get the latest cbindgen using `cargo install --force cbindgen`
 *      a. Alternatively, you can clone `https://github.com/rlhunt/cbindgen` and use a tagged release
 *   2. Run `cbindgen toolkit/library/rust/ --crate webrender_bindings -o gfx/webrender_bindings/webrender_ffi_generated.h`
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
  None = 0,
  Outset = 1,
  Inset = 2,

  Sentinel /* this must be last for serialization purposes. */
};

enum class ExtendMode : uint32_t {
  Clamp = 0,
  Repeat = 1,

  Sentinel /* this must be last for serialization purposes. */
};

enum class ExternalImageType : uint32_t {
  Texture2DHandle = 0,
  TextureRectHandle = 1,
  TextureExternalHandle = 2,
  ExternalBuffer = 3,

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

enum class TransformStyle : uint32_t {
  Flat = 0,
  Preserve3D = 1,

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

struct LayerPixel;

struct RenderApi;

struct Renderer;

struct Vec_u8;

struct WrRenderedEpochs;

struct WrState;

struct WrThreadPool;

typedef Vec_u8 VecU8;

typedef Arc_VecU8 ArcVecU8;

struct IdNamespace {
  uint32_t mHandle;

  bool operator==(const IdNamespace& aOther) const {
    return mHandle == aOther.mHandle;
  }
  bool operator<(const IdNamespace& aOther) const {
    return mHandle < aOther.mHandle;
  }
  bool operator<=(const IdNamespace& aOther) const {
    return mHandle <= aOther.mHandle;
  }
};

struct ImageKey {
  IdNamespace mNamespace;
  uint32_t mHandle;

  bool operator==(const ImageKey& aOther) const {
    return mNamespace == aOther.mNamespace &&
           mHandle == aOther.mHandle;
  }
};

typedef ImageKey WrImageKey;

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

struct ByteSlice {
  const uint8_t *buffer;
  size_t len;

  bool operator==(const ByteSlice& aOther) const {
    return buffer == aOther.buffer &&
           len == aOther.len;
  }
};

struct WrExternalImageId {
  uint64_t mHandle;

  bool operator==(const WrExternalImageId& aOther) const {
    return mHandle == aOther.mHandle;
  }
};

typedef ExternalImageType WrExternalImageBufferType;

struct FontKey {
  IdNamespace mNamespace;
  uint32_t mHandle;

  bool operator==(const FontKey& aOther) const {
    return mNamespace == aOther.mNamespace &&
           mHandle == aOther.mHandle;
  }
};

typedef FontKey WrFontKey;

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

struct PipelineId {
  uint32_t mNamespace;
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

struct BuiltDisplayListDescriptor {
  uint64_t builder_start_time;
  uint64_t builder_finish_time;
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

typedef LayerPixel LayoutPixel;

typedef TypedTransform3D_f32__LayoutPixel__LayoutPixel LayoutTransform;

struct WrTransformProperty {
  uint64_t id;
  LayoutTransform transform;
};

typedef IdNamespace WrIdNamespace;

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

struct TypedPoint2D_f32__LayerPixel {
  float x;
  float y;

  bool operator==(const TypedPoint2D_f32__LayerPixel& aOther) const {
    return x == aOther.x &&
           y == aOther.y;
  }
};

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

struct WrComplexClipRegion {
  LayoutRect rect;
  BorderRadius radii;

  bool operator==(const WrComplexClipRegion& aOther) const {
    return rect == aOther.rect &&
           radii == aOther.radii;
  }
};

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

struct WrFilterOp {
  WrFilterOpType filter_type;
  float argument;

  bool operator==(const WrFilterOp& aOther) const {
    return filter_type == aOther.filter_type &&
           argument == aOther.argument;
  }
};

struct GlyphInstance {
  uint32_t index;
  LayoutPoint point;

  bool operator==(const GlyphInstance& aOther) const {
    return index == aOther.index &&
           point == aOther.point;
  }
};

typedef YuvColorSpace WrYuvColorSpace;

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

/* DO NOT MODIFY THIS MANUALLY! This file was generated using cbindgen.
 * To generate this file:
 *   1. Get the latest cbindgen using `cargo install --force cbindgen`
 *      a. Alternatively, you can clone `https://github.com/rlhunt/cbindgen` and use a tagged release
 *   2. Run `cbindgen toolkit/library/rust/ --crate webrender_bindings -o gfx/webrender_bindings/webrender_ffi_generated.h`
 */

WR_INLINE
const VecU8 *wr_add_ref_arc(const ArcVecU8 *aArc)
WR_FUNC;

WR_INLINE
void wr_api_add_blob_image(RenderApi *aApi,
                           WrImageKey aImageKey,
                           const WrImageDescriptor *aDescriptor,
                           ByteSlice aBytes)
WR_FUNC;

WR_INLINE
void wr_api_add_external_image(RenderApi *aApi,
                               WrImageKey aImageKey,
                               const WrImageDescriptor *aDescriptor,
                               WrExternalImageId aExternalImageId,
                               WrExternalImageBufferType aBufferType,
                               uint8_t aChannelIndex)
WR_FUNC;

WR_INLINE
void wr_api_add_image(RenderApi *aApi,
                      WrImageKey aImageKey,
                      const WrImageDescriptor *aDescriptor,
                      ByteSlice aBytes)
WR_FUNC;

WR_INLINE
void wr_api_add_raw_font(RenderApi *aApi,
                         WrFontKey aKey,
                         uint8_t *aFontBuffer,
                         size_t aBufferSize,
                         uint32_t aIndex)
WR_FUNC;

WR_INLINE
void wr_api_clear_root_display_list(RenderApi *aApi,
                                    WrEpoch aEpoch,
                                    WrPipelineId aPipelineId)
WR_FUNC;

WR_INLINE
void wr_api_delete(RenderApi *aApi)
WR_DESTRUCTOR_SAFE_FUNC;

WR_INLINE
void wr_api_delete_font(RenderApi *aApi,
                        WrFontKey aKey)
WR_FUNC;

WR_INLINE
void wr_api_delete_image(RenderApi *aApi,
                         WrImageKey aKey)
WR_FUNC;

WR_INLINE
void wr_api_finalize_builder(WrState *aState,
                             LayoutSize *aContentSize,
                             BuiltDisplayListDescriptor *aDlDescriptor,
                             WrVecU8 *aDlData)
WR_FUNC;

WR_INLINE
void wr_api_generate_frame(RenderApi *aApi)
WR_FUNC;

WR_INLINE
void wr_api_generate_frame_with_properties(RenderApi *aApi,
                                           const WrOpacityProperty *aOpacityArray,
                                           size_t aOpacityCount,
                                           const WrTransformProperty *aTransformArray,
                                           size_t aTransformCount)
WR_FUNC;

WR_INLINE
WrIdNamespace wr_api_get_namespace(RenderApi *aApi)
WR_FUNC;

WR_INLINE
void wr_api_send_external_event(RenderApi *aApi,
                                size_t aEvt)
WR_DESTRUCTOR_SAFE_FUNC;

WR_INLINE
void wr_api_set_root_display_list(RenderApi *aApi,
                                  ColorF aColor,
                                  WrEpoch aEpoch,
                                  float aViewportWidth,
                                  float aViewportHeight,
                                  WrPipelineId aPipelineId,
                                  LayoutSize aContentSize,
                                  BuiltDisplayListDescriptor aDlDescriptor,
                                  uint8_t *aDlData,
                                  size_t aDlSize)
WR_FUNC;

WR_INLINE
void wr_api_set_root_pipeline(RenderApi *aApi,
                              WrPipelineId aPipelineId)
WR_FUNC;

WR_INLINE
void wr_api_set_window_parameters(RenderApi *aApi,
                                  int32_t aWidth,
                                  int32_t aHeight)
WR_FUNC;

WR_INLINE
void wr_api_update_blob_image(RenderApi *aApi,
                              WrImageKey aImageKey,
                              const WrImageDescriptor *aDescriptor,
                              ByteSlice aBytes)
WR_FUNC;

WR_INLINE
void wr_api_update_external_image(RenderApi *aApi,
                                  WrImageKey aKey,
                                  const WrImageDescriptor *aDescriptor,
                                  WrExternalImageId aExternalImageId,
                                  WrExternalImageBufferType aImageType,
                                  uint8_t aChannelIndex)
WR_FUNC;

WR_INLINE
void wr_api_update_image(RenderApi *aApi,
                         WrImageKey aKey,
                         const WrImageDescriptor *aDescriptor,
                         ByteSlice aBytes)
WR_FUNC;

WR_INLINE
void wr_dec_ref_arc(const VecU8 *aArc)
WR_FUNC;

WR_INLINE
void wr_dp_begin(WrState *aState,
                 uint32_t aWidth,
                 uint32_t aHeight)
WR_FUNC;

WR_INLINE
void wr_dp_end(WrState *aState)
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
                           LayoutRect aBoxBounds,
                           LayoutVector2D aOffset,
                           ColorF aColor,
                           float aBlurRadius,
                           float aSpreadRadius,
                           float aBorderRadius,
                           BoxShadowClipMode aClipMode)
WR_FUNC;

WR_INLINE
void wr_dp_push_built_display_list(WrState *aState,
                                   BuiltDisplayListDescriptor aDlDescriptor,
                                   WrVecU8 *aDlData)
WR_FUNC;

WR_INLINE
uint64_t wr_dp_push_clip(WrState *aState,
                         LayoutRect aRect,
                         const WrComplexClipRegion *aComplex,
                         size_t aComplexCount,
                         const WrImageMask *aMask)
WR_FUNC;

WR_INLINE
void wr_dp_push_clip_and_scroll_info(WrState *aState,
                                     uint64_t aScrollId,
                                     const uint64_t *aClipId)
WR_FUNC;

WR_INLINE
void wr_dp_push_iframe(WrState *aState,
                       LayoutRect aRect,
                       WrPipelineId aPipelineId)
WR_FUNC;

WR_INLINE
void wr_dp_push_image(WrState *aState,
                      LayoutRect aBounds,
                      LayoutRect aClip,
                      LayoutSize aStretchSize,
                      LayoutSize aTileSpacing,
                      ImageRendering aImageRendering,
                      WrImageKey aKey)
WR_FUNC;

WR_INLINE
void wr_dp_push_linear_gradient(WrState *aState,
                                LayoutRect aRect,
                                LayoutRect aClip,
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
                     ColorF aColor)
WR_FUNC;

WR_INLINE
void wr_dp_push_scroll_layer(WrState *aState,
                             uint64_t aScrollId,
                             LayoutRect aContentRect,
                             LayoutRect aClipRect)
WR_FUNC;

WR_INLINE
void wr_dp_push_stacking_context(WrState *aState,
                                 LayoutRect aBounds,
                                 uint64_t aAnimationId,
                                 const float *aOpacity,
                                 const LayoutTransform *aTransform,
                                 TransformStyle aTransformStyle,
                                 MixBlendMode aMixBlendMode,
                                 const WrFilterOp *aFilters,
                                 size_t aFilterCount)
WR_FUNC;

WR_INLINE
void wr_dp_push_text(WrState *aState,
                     LayoutRect aBounds,
                     LayoutRect aClip,
                     ColorF aColor,
                     WrFontKey aFontKey,
                     const GlyphInstance *aGlyphs,
                     uint32_t aGlyphCount,
                     float aGlyphSize)
WR_FUNC;

WR_INLINE
void wr_dp_push_yuv_NV12_image(WrState *aState,
                               LayoutRect aBounds,
                               LayoutRect aClip,
                               WrImageKey aImageKey0,
                               WrImageKey aImageKey1,
                               WrYuvColorSpace aColorSpace,
                               ImageRendering aImageRendering)
WR_FUNC;

WR_INLINE
void wr_dp_push_yuv_interleaved_image(WrState *aState,
                                      LayoutRect aBounds,
                                      LayoutRect aClip,
                                      WrImageKey aImageKey0,
                                      WrYuvColorSpace aColorSpace,
                                      ImageRendering aImageRendering)
WR_FUNC;

WR_INLINE
void wr_dp_push_yuv_planar_image(WrState *aState,
                                 LayoutRect aBounds,
                                 LayoutRect aClip,
                                 WrImageKey aImageKey0,
                                 WrImageKey aImageKey1,
                                 WrImageKey aImageKey2,
                                 WrYuvColorSpace aColorSpace,
                                 ImageRendering aImageRendering)
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
void wr_renderer_readback(Renderer *aRenderer,
                          uint32_t aWidth,
                          uint32_t aHeight,
                          uint8_t *aDstBuffer,
                          size_t aBufferSize)
WR_FUNC;

WR_INLINE
void wr_renderer_render(Renderer *aRenderer,
                        uint32_t aWidth,
                        uint32_t aHeight)
WR_FUNC;

WR_INLINE
void wr_renderer_set_external_image_handler(Renderer *aRenderer,
                                            WrExternalImageHandler *aExternalImageHandler)
WR_FUNC;

WR_INLINE
void wr_renderer_set_profiler_enabled(Renderer *aRenderer,
                                      bool aEnabled)
WR_FUNC;

WR_INLINE
void wr_renderer_update(Renderer *aRenderer)
WR_FUNC;

WR_INLINE
void wr_scroll_layer_with_id(RenderApi *aApi,
                             WrPipelineId aPipelineId,
                             uint64_t aScrollId,
                             LayoutPoint aNewScrollOrigin)
WR_FUNC;

WR_INLINE
void wr_state_delete(WrState *aState)
WR_DESTRUCTOR_SAFE_FUNC;

WR_INLINE
WrState *wr_state_new(WrPipelineId aPipelineId,
                      LayoutSize aContentSize)
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
bool wr_window_new(WrWindowId aWindowId,
                   uint32_t aWindowWidth,
                   uint32_t aWindowHeight,
                   void *aGlContext,
                   WrThreadPool *aThreadPool,
                   bool aEnableProfiler,
                   RenderApi **aOutApi,
                   Renderer **aOutRenderer)
WR_FUNC;

} // namespace wr
} // namespace mozilla

} // extern "C"

/* DO NOT MODIFY THIS MANUALLY! This file was generated using cbindgen.
 * To generate this file:
 *   1. Get the latest cbindgen using `cargo install --force cbindgen`
 *      a. Alternatively, you can clone `https://github.com/rlhunt/cbindgen` and use a tagged release
 *   2. Run `cbindgen toolkit/library/rust/ --crate webrender_bindings -o gfx/webrender_bindings/webrender_ffi_generated.h`
 */
