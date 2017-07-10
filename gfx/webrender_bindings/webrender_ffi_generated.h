/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Generated with cbindgen:0.1.13 */

/* DO NOT MODIFY THIS MANUALLY! This file was generated using cbindgen.
 * To generate this file:
 *   1. Get the latest cbindgen using `cargo install --force cbindgen`
 *      a. Alternatively, you can clone `https://github.com/rlhunt/cbindgen` and use a tagged release
 *   2. Run `cbindgen toolkit/library/rust/ --crate webrender_bindings -o gfx/webrender_bindings/webrender_ffi_generated.h`
 */

#include <cstdint>
#include <cstdlib>

extern "C" {

enum class WrBorderStyle : uint32_t {
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

enum class WrBoxShadowClipMode : uint32_t {
  None = 0,
  Outset = 1,
  Inset = 2,

  Sentinel /* this must be last for serialization purposes. */
};

enum class WrExternalImageBufferType : uint32_t {
  Texture2DHandle = 0,
  TextureRectHandle = 1,
  TextureExternalHandle = 2,
  ExternalBuffer = 3,

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

enum class WrGradientExtendMode : uint32_t {
  Clamp = 0,
  Repeat = 1,

  Sentinel /* this must be last for serialization purposes. */
};

enum class WrImageFormat : uint32_t {
  Invalid = 0,
  A8 = 1,
  RGB8 = 2,
  BGRA8 = 3,
  RGBAF32 = 4,
  RG8 = 5,

  Sentinel /* this must be last for serialization purposes. */
};

enum class WrImageRendering : uint32_t {
  Auto = 0,
  CrispEdges = 1,
  Pixelated = 2,

  Sentinel /* this must be last for serialization purposes. */
};

enum class WrMixBlendMode : uint32_t {
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

enum class WrRepeatMode : uint32_t {
  Stretch = 0,
  Repeat = 1,
  Round = 2,
  Space = 3,

  Sentinel /* this must be last for serialization purposes. */
};

enum class WrTransformStyle : uint32_t {
  Flat = 0,
  Preserve3D = 1,

  Sentinel /* this must be last for serialization purposes. */
};

enum class WrYuvColorSpace : uint32_t {
  Rec601 = 0,
  Rec709 = 1,

  Sentinel /* this must be last for serialization purposes. */
};

struct WrAPI;

struct WrRenderedEpochs;

struct WrRenderer;

struct WrState;

struct WrThreadPool;

struct WrImageKey {
  uint32_t mNamespace;
  uint32_t mHandle;

  bool operator==(const WrImageKey& aOther) const {
    return mNamespace == aOther.mNamespace &&
           mHandle == aOther.mHandle;
  }
};

struct WrImageDescriptor {
  WrImageFormat format;
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

struct WrByteSlice {
  const uint8_t *buffer;
  size_t len;

  bool operator==(const WrByteSlice& aOther) const {
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

struct WrFontKey {
  uint32_t mNamespace;
  uint32_t mHandle;

  bool operator==(const WrFontKey& aOther) const {
    return mNamespace == aOther.mNamespace &&
           mHandle == aOther.mHandle;
  }
};

struct WrEpoch {
  uint32_t mHandle;

  bool operator==(const WrEpoch& aOther) const {
    return mHandle == aOther.mHandle;
  }
  bool operator<(const WrEpoch& aOther) const {
    return mHandle < aOther.mHandle;
  }
  bool operator<=(const WrEpoch& aOther) const {
    return mHandle <= aOther.mHandle;
  }
};

struct WrPipelineId {
  uint32_t mNamespace;
  uint32_t mHandle;

  bool operator==(const WrPipelineId& aOther) const {
    return mNamespace == aOther.mNamespace &&
           mHandle == aOther.mHandle;
  }
};

struct WrSize {
  float width;
  float height;

  bool operator==(const WrSize& aOther) const {
    return width == aOther.width &&
           height == aOther.height;
  }
};

struct WrBuiltDisplayListDescriptor {
  uint64_t builder_start_time;
  uint64_t builder_finish_time;

  bool operator==(const WrBuiltDisplayListDescriptor& aOther) const {
    return builder_start_time == aOther.builder_start_time &&
           builder_finish_time == aOther.builder_finish_time;
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

struct WrMatrix {
  float values[16];
};

struct WrTransformProperty {
  uint64_t id;
  WrMatrix transform;
};

struct WrIdNamespace {
  uint32_t mHandle;

  bool operator==(const WrIdNamespace& aOther) const {
    return mHandle == aOther.mHandle;
  }
  bool operator<(const WrIdNamespace& aOther) const {
    return mHandle < aOther.mHandle;
  }
  bool operator<=(const WrIdNamespace& aOther) const {
    return mHandle <= aOther.mHandle;
  }
};

struct WrColor {
  float r;
  float g;
  float b;
  float a;

  bool operator==(const WrColor& aOther) const {
    return r == aOther.r &&
           g == aOther.g &&
           b == aOther.b &&
           a == aOther.a;
  }
};

struct WrRect {
  float x;
  float y;
  float width;
  float height;

  bool operator==(const WrRect& aOther) const {
    return x == aOther.x &&
           y == aOther.y &&
           width == aOther.width &&
           height == aOther.height;
  }
};

struct WrBorderWidths {
  float left;
  float top;
  float right;
  float bottom;

  bool operator==(const WrBorderWidths& aOther) const {
    return left == aOther.left &&
           top == aOther.top &&
           right == aOther.right &&
           bottom == aOther.bottom;
  }
};

struct WrBorderSide {
  WrColor color;
  WrBorderStyle style;

  bool operator==(const WrBorderSide& aOther) const {
    return color == aOther.color &&
           style == aOther.style;
  }
};

struct WrBorderRadius {
  WrSize top_left;
  WrSize top_right;
  WrSize bottom_left;
  WrSize bottom_right;

  bool operator==(const WrBorderRadius& aOther) const {
    return top_left == aOther.top_left &&
           top_right == aOther.top_right &&
           bottom_left == aOther.bottom_left &&
           bottom_right == aOther.bottom_right;
  }
};

struct WrPoint {
  float x;
  float y;

  bool operator==(const WrPoint& aOther) const {
    return x == aOther.x &&
           y == aOther.y;
  }
};

struct WrGradientStop {
  float offset;
  WrColor color;

  bool operator==(const WrGradientStop& aOther) const {
    return offset == aOther.offset &&
           color == aOther.color;
  }
};

struct WrSideOffsets2Df32 {
  float top;
  float right;
  float bottom;
  float left;

  bool operator==(const WrSideOffsets2Df32& aOther) const {
    return top == aOther.top &&
           right == aOther.right &&
           bottom == aOther.bottom &&
           left == aOther.left;
  }
};

struct WrSideOffsets2Du32 {
  uint32_t top;
  uint32_t right;
  uint32_t bottom;
  uint32_t left;

  bool operator==(const WrSideOffsets2Du32& aOther) const {
    return top == aOther.top &&
           right == aOther.right &&
           bottom == aOther.bottom &&
           left == aOther.left;
  }
};

struct WrNinePatchDescriptor {
  uint32_t width;
  uint32_t height;
  WrSideOffsets2Du32 slice;

  bool operator==(const WrNinePatchDescriptor& aOther) const {
    return width == aOther.width &&
           height == aOther.height &&
           slice == aOther.slice;
  }
};

struct WrComplexClipRegion {
  WrRect rect;
  WrBorderRadius radii;

  bool operator==(const WrComplexClipRegion& aOther) const {
    return rect == aOther.rect &&
           radii == aOther.radii;
  }
};

struct WrImageMask {
  WrImageKey image;
  WrRect rect;
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

struct WrGlyphInstance {
  uint32_t index;
  WrPoint point;

  bool operator==(const WrGlyphInstance& aOther) const {
    return index == aOther.index &&
           point == aOther.point;
  }
};

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
void wr_api_add_blob_image(WrAPI *aApi,
                           WrImageKey aImageKey,
                           const WrImageDescriptor *aDescriptor,
                           WrByteSlice aBytes)
WR_FUNC;

WR_INLINE
void wr_api_add_external_image(WrAPI *aApi,
                               WrImageKey aImageKey,
                               const WrImageDescriptor *aDescriptor,
                               WrExternalImageId aExternalImageId,
                               WrExternalImageBufferType aBufferType,
                               uint8_t aChannelIndex)
WR_FUNC;

WR_INLINE
void wr_api_add_external_image_buffer(WrAPI *aApi,
                                      WrImageKey aImageKey,
                                      const WrImageDescriptor *aDescriptor,
                                      WrExternalImageId aExternalImageId)
WR_FUNC;

WR_INLINE
void wr_api_add_image(WrAPI *aApi,
                      WrImageKey aImageKey,
                      const WrImageDescriptor *aDescriptor,
                      WrByteSlice aBytes)
WR_FUNC;

WR_INLINE
void wr_api_add_raw_font(WrAPI *aApi,
                         WrFontKey aKey,
                         uint8_t *aFontBuffer,
                         size_t aBufferSize,
                         uint32_t aIndex)
WR_FUNC;

WR_INLINE
void wr_api_clear_root_display_list(WrAPI *aApi,
                                    WrEpoch aEpoch,
                                    WrPipelineId aPipelineId)
WR_FUNC;

WR_INLINE
void wr_api_delete(WrAPI *aApi)
WR_DESTRUCTOR_SAFE_FUNC;

WR_INLINE
void wr_api_delete_font(WrAPI *aApi,
                        WrFontKey aKey)
WR_FUNC;

WR_INLINE
void wr_api_delete_image(WrAPI *aApi,
                         WrImageKey aKey)
WR_FUNC;

WR_INLINE
void wr_api_finalize_builder(WrState *aState,
                             WrSize *aContentSize,
                             WrBuiltDisplayListDescriptor *aDlDescriptor,
                             WrVecU8 *aDlData)
WR_FUNC;

WR_INLINE
void wr_api_generate_frame(WrAPI *aApi)
WR_FUNC;

WR_INLINE
void wr_api_generate_frame_with_properties(WrAPI *aApi,
                                           const WrOpacityProperty *aOpacityArray,
                                           size_t aOpacityCount,
                                           const WrTransformProperty *aTransformArray,
                                           size_t aTransformCount)
WR_FUNC;

WR_INLINE
WrIdNamespace wr_api_get_namespace(WrAPI *aApi)
WR_FUNC;

WR_INLINE
void wr_api_send_external_event(WrAPI *aApi,
                                size_t aEvt)
WR_DESTRUCTOR_SAFE_FUNC;

WR_INLINE
void wr_api_set_root_display_list(WrAPI *aApi,
                                  WrColor aColor,
                                  WrEpoch aEpoch,
                                  float aViewportWidth,
                                  float aViewportHeight,
                                  WrPipelineId aPipelineId,
                                  WrSize aContentSize,
                                  WrBuiltDisplayListDescriptor aDlDescriptor,
                                  uint8_t *aDlData,
                                  size_t aDlSize)
WR_FUNC;

WR_INLINE
void wr_api_set_root_pipeline(WrAPI *aApi,
                              WrPipelineId aPipelineId)
WR_FUNC;

WR_INLINE
void wr_api_set_window_parameters(WrAPI *aApi,
                                  int32_t aWidth,
                                  int32_t aHeight)
WR_FUNC;

WR_INLINE
void wr_api_update_image(WrAPI *aApi,
                         WrImageKey aKey,
                         const WrImageDescriptor *aDescriptor,
                         WrByteSlice aBytes)
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
                       WrRect aRect,
                       WrRect aClip,
                       WrBorderWidths aWidths,
                       WrBorderSide aTop,
                       WrBorderSide aRight,
                       WrBorderSide aBottom,
                       WrBorderSide aLeft,
                       WrBorderRadius aRadius)
WR_FUNC;

WR_INLINE
void wr_dp_push_border_gradient(WrState *aState,
                                WrRect aRect,
                                WrRect aClip,
                                WrBorderWidths aWidths,
                                WrPoint aStartPoint,
                                WrPoint aEndPoint,
                                const WrGradientStop *aStops,
                                size_t aStopsCount,
                                WrGradientExtendMode aExtendMode,
                                WrSideOffsets2Df32 aOutset)
WR_FUNC;

WR_INLINE
void wr_dp_push_border_image(WrState *aState,
                             WrRect aRect,
                             WrRect aClip,
                             WrBorderWidths aWidths,
                             WrImageKey aImage,
                             WrNinePatchDescriptor aPatch,
                             WrSideOffsets2Df32 aOutset,
                             WrRepeatMode aRepeatHorizontal,
                             WrRepeatMode aRepeatVertical)
WR_FUNC;

WR_INLINE
void wr_dp_push_border_radial_gradient(WrState *aState,
                                       WrRect aRect,
                                       WrRect aClip,
                                       WrBorderWidths aWidths,
                                       WrPoint aCenter,
                                       WrSize aRadius,
                                       const WrGradientStop *aStops,
                                       size_t aStopsCount,
                                       WrGradientExtendMode aExtendMode,
                                       WrSideOffsets2Df32 aOutset)
WR_FUNC;

WR_INLINE
void wr_dp_push_box_shadow(WrState *aState,
                           WrRect aRect,
                           WrRect aClip,
                           WrRect aBoxBounds,
                           WrPoint aOffset,
                           WrColor aColor,
                           float aBlurRadius,
                           float aSpreadRadius,
                           float aBorderRadius,
                           WrBoxShadowClipMode aClipMode)
WR_FUNC;

WR_INLINE
void wr_dp_push_built_display_list(WrState *aState,
                                   WrBuiltDisplayListDescriptor aDlDescriptor,
                                   WrVecU8 *aDlData)
WR_FUNC;

WR_INLINE
uint64_t wr_dp_push_clip(WrState *aState,
                         WrRect aRect,
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
                       WrRect aRect,
                       WrPipelineId aPipelineId)
WR_FUNC;

WR_INLINE
void wr_dp_push_image(WrState *aState,
                      WrRect aBounds,
                      WrRect aClip,
                      WrSize aStretchSize,
                      WrSize aTileSpacing,
                      WrImageRendering aImageRendering,
                      WrImageKey aKey)
WR_FUNC;

WR_INLINE
void wr_dp_push_linear_gradient(WrState *aState,
                                WrRect aRect,
                                WrRect aClip,
                                WrPoint aStartPoint,
                                WrPoint aEndPoint,
                                const WrGradientStop *aStops,
                                size_t aStopsCount,
                                WrGradientExtendMode aExtendMode,
                                WrSize aTileSize,
                                WrSize aTileSpacing)
WR_FUNC;

WR_INLINE
void wr_dp_push_radial_gradient(WrState *aState,
                                WrRect aRect,
                                WrRect aClip,
                                WrPoint aCenter,
                                WrSize aRadius,
                                const WrGradientStop *aStops,
                                size_t aStopsCount,
                                WrGradientExtendMode aExtendMode,
                                WrSize aTileSize,
                                WrSize aTileSpacing)
WR_FUNC;

WR_INLINE
void wr_dp_push_rect(WrState *aState,
                     WrRect aRect,
                     WrRect aClip,
                     WrColor aColor)
WR_FUNC;

WR_INLINE
void wr_dp_push_scroll_layer(WrState *aState,
                             uint64_t aScrollId,
                             WrRect aContentRect,
                             WrRect aClipRect)
WR_FUNC;

WR_INLINE
void wr_dp_push_stacking_context(WrState *aState,
                                 WrRect aBounds,
                                 uint64_t aAnimationId,
                                 const float *aOpacity,
                                 const WrMatrix *aTransform,
                                 WrTransformStyle aTransformStyle,
                                 WrMixBlendMode aMixBlendMode,
                                 const WrFilterOp *aFilters,
                                 size_t aFilterCount)
WR_FUNC;

WR_INLINE
void wr_dp_push_text(WrState *aState,
                     WrRect aBounds,
                     WrRect aClip,
                     WrColor aColor,
                     WrFontKey aFontKey,
                     const WrGlyphInstance *aGlyphs,
                     uint32_t aGlyphCount,
                     float aGlyphSize)
WR_FUNC;

WR_INLINE
void wr_dp_push_yuv_NV12_image(WrState *aState,
                               WrRect aBounds,
                               WrRect aClip,
                               WrImageKey aImageKey0,
                               WrImageKey aImageKey1,
                               WrYuvColorSpace aColorSpace,
                               WrImageRendering aImageRendering)
WR_FUNC;

WR_INLINE
void wr_dp_push_yuv_interleaved_image(WrState *aState,
                                      WrRect aBounds,
                                      WrRect aClip,
                                      WrImageKey aImageKey0,
                                      WrYuvColorSpace aColorSpace,
                                      WrImageRendering aImageRendering)
WR_FUNC;

WR_INLINE
void wr_dp_push_yuv_planar_image(WrState *aState,
                                 WrRect aBounds,
                                 WrRect aClip,
                                 WrImageKey aImageKey0,
                                 WrImageKey aImageKey1,
                                 WrImageKey aImageKey2,
                                 WrYuvColorSpace aColorSpace,
                                 WrImageRendering aImageRendering)
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
bool wr_renderer_current_epoch(WrRenderer *aRenderer,
                               WrPipelineId aPipelineId,
                               WrEpoch *aOutEpoch)
WR_FUNC;

WR_INLINE
void wr_renderer_delete(WrRenderer *aRenderer)
WR_DESTRUCTOR_SAFE_FUNC;

WR_INLINE
WrRenderedEpochs *wr_renderer_flush_rendered_epochs(WrRenderer *aRenderer)
WR_FUNC;

WR_INLINE
void wr_renderer_readback(WrRenderer *aRenderer,
                          uint32_t aWidth,
                          uint32_t aHeight,
                          uint8_t *aDstBuffer,
                          size_t aBufferSize)
WR_FUNC;

WR_INLINE
void wr_renderer_render(WrRenderer *aRenderer,
                        uint32_t aWidth,
                        uint32_t aHeight)
WR_FUNC;

WR_INLINE
void wr_renderer_set_external_image_handler(WrRenderer *aRenderer,
                                            WrExternalImageHandler *aExternalImageHandler)
WR_FUNC;

WR_INLINE
void wr_renderer_set_profiler_enabled(WrRenderer *aRenderer,
                                      bool aEnabled)
WR_FUNC;

WR_INLINE
void wr_renderer_update(WrRenderer *aRenderer)
WR_FUNC;

WR_INLINE
void wr_scroll_layer_with_id(WrAPI *aApi,
                             WrPipelineId aPipelineId,
                             uint64_t aScrollId,
                             WrPoint aNewScrollOrigin)
WR_FUNC;

WR_INLINE
void wr_state_delete(WrState *aState)
WR_DESTRUCTOR_SAFE_FUNC;

WR_INLINE
WrState *wr_state_new(WrPipelineId aPipelineId,
                      WrSize aContentSize)
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
                   WrAPI **aOutApi,
                   WrRenderer **aOutRenderer)
WR_FUNC;

} // extern "C"

/* DO NOT MODIFY THIS MANUALLY! This file was generated using cbindgen.
 * To generate this file:
 *   1. Get the latest cbindgen using `cargo install --force cbindgen`
 *      a. Alternatively, you can clone `https://github.com/rlhunt/cbindgen` and use a tagged release
 *   2. Run `cbindgen toolkit/library/rust/ --crate webrender_bindings -o gfx/webrender_bindings/webrender_ffi_generated.h`
 */
