/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Generated with cbindgen:0.1.5 */

/* DO NOT MODIFY THIS MANUALLY! This file was generated using cbindgen.
 * To generate this file, clone `https://github.com/rlhunt/cbindgen` or run `cargo install cbindgen`,
 * then run `cbindgen -c wr gfx/webrender_bindings/ gfx/webrender_bindings/webrender_ffi_generated.h` */

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

enum class WrExternalImageType : uint32_t {
  NativeTexture = 0,
  RawData = 1,

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
  RGBA8 = 3,
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

struct WrAPI;

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
  const uint8_t* buffer;
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

struct WrState;

struct WrBuiltDisplayListDescriptor {
  size_t display_list_items_size;
  uint64_t serialization_start_time;
  uint64_t serialization_end_time;

  bool operator==(const WrBuiltDisplayListDescriptor& aOther) const {
    return display_list_items_size == aOther.display_list_items_size &&
      serialization_start_time == aOther.serialization_start_time &&
      serialization_end_time == aOther.serialization_end_time;
  }
};

struct WrVecU8 {
  uint8_t* data;
  size_t length;
  size_t capacity;

  bool operator==(const WrVecU8& aOther) const {
    return data == aOther.data &&
      length == aOther.length &&
      capacity == aOther.capacity;
  }
};

struct WrAuxiliaryListsDescriptor {
  size_t gradient_stops_size;
  size_t complex_clip_regions_size;
  size_t filters_size;
  size_t glyph_instances_size;

  bool operator==(const WrAuxiliaryListsDescriptor& aOther) const {
    return gradient_stops_size == aOther.gradient_stops_size &&
      complex_clip_regions_size == aOther.complex_clip_regions_size &&
      filters_size == aOther.filters_size &&
      glyph_instances_size == aOther.glyph_instances_size;
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

  bool operator==(const WrMatrix& aOther) const {
    return values == aOther.values;
  }
};

struct WrTransformProperty {
  uint64_t id;
  WrMatrix transform;

  bool operator==(const WrTransformProperty& aOther) const {
    return id == aOther.id &&
      transform == aOther.transform;
  }
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

struct WrItemRange {
  size_t start;
  size_t length;

  bool operator==(const WrItemRange& aOther) const {
    return start == aOther.start &&
      length == aOther.length;
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

struct WrClipRegion {
  WrRect main;
  WrItemRange complex;
  WrImageMask image_mask;
  bool has_image_mask;

  bool operator==(const WrClipRegion& aOther) const {
    return main == aOther.main &&
      complex == aOther.complex &&
      image_mask == aOther.image_mask &&
      has_image_mask == aOther.has_image_mask;
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

struct WrComplexClipRegion {
  WrRect rect;
  WrBorderRadius radii;

  bool operator==(const WrComplexClipRegion& aOther) const {
    return rect == aOther.rect &&
      radii == aOther.radii;
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

struct WrBorderSide {
  WrColor color;
  WrBorderStyle style;

  bool operator==(const WrBorderSide& aOther) const {
    return color == aOther.color &&
      style == aOther.style;
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

struct WrGlyphInstance {
  uint32_t index;
  WrPoint point;

  bool operator==(const WrGlyphInstance& aOther) const {
    return index == aOther.index &&
      point == aOther.point;
  }
};

struct WrRenderedEpochs;

struct WrRenderer;

struct WrExternalImage {
  WrExternalImageType image_type;
  uint32_t handle;
  float u0;
  float v0;
  float u1;
  float v1;
  const uint8_t* buff;
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

typedef WrExternalImage (*LockExternalImageCallback)(void*, WrExternalImageId);

typedef void (*UnlockExternalImageCallback)(void*, WrExternalImageId);

struct WrExternalImageHandler {
  void* external_image_obj;
  LockExternalImageCallback lock_func;
  UnlockExternalImageCallback unlock_func;

  bool operator==(const WrExternalImageHandler& aOther) const {
    return external_image_obj == aOther.external_image_obj &&
      lock_func == aOther.lock_func &&
      unlock_func == aOther.unlock_func;
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

/* DO NOT MODIFY THIS MANUALLY! This file was generated using cbindgen.
 * To generate this file, clone `https://github.com/rlhunt/cbindgen` or run `cargo install cbindgen`,
 * then run `cbindgen -c wr gfx/webrender_bindings/ gfx/webrender_bindings/webrender_ffi_generated.h` */

WR_INLINE void
wr_api_add_blob_image(WrAPI* api,
    WrImageKey image_key,
    const WrImageDescriptor* descriptor,
    WrByteSlice bytes)
WR_FUNC;

WR_INLINE void
wr_api_add_external_image_buffer(WrAPI* api,
    WrImageKey image_key,
    const WrImageDescriptor* descriptor,
    WrExternalImageId external_image_id)
WR_FUNC;

WR_INLINE void
wr_api_add_external_image_handle(WrAPI* api,
    WrImageKey image_key,
    const WrImageDescriptor* descriptor,
    WrExternalImageId external_image_id)
WR_FUNC;

WR_INLINE void
wr_api_add_image(WrAPI* api,
    WrImageKey image_key,
    const WrImageDescriptor* descriptor,
    WrByteSlice bytes)
WR_FUNC;

WR_INLINE void
wr_api_add_raw_font(WrAPI* api,
    WrFontKey key,
    uint8_t* font_buffer,
    size_t buffer_size)
WR_FUNC;

WR_INLINE void
wr_api_clear_root_display_list(WrAPI* api,
    WrEpoch epoch,
    WrPipelineId pipeline_id)
WR_FUNC;

WR_INLINE void
wr_api_delete(WrAPI* api)
WR_DESTRUCTOR_SAFE_FUNC;

WR_INLINE void
wr_api_delete_font(WrAPI* api,
    WrFontKey key)
WR_FUNC;

WR_INLINE void
wr_api_delete_image(WrAPI* api,
    WrImageKey key)
WR_FUNC;

WR_INLINE void
wr_api_finalize_builder(WrState* state,
    WrBuiltDisplayListDescriptor* dl_descriptor,
    WrVecU8* dl_data,
    WrAuxiliaryListsDescriptor* aux_descriptor,
    WrVecU8* aux_data)
WR_FUNC;

WR_INLINE void
wr_api_generate_frame(WrAPI* api)
WR_FUNC;

WR_INLINE void
wr_api_generate_frame_with_properties(WrAPI* api,
    const WrOpacityProperty* opacity_array,
    size_t opacity_count,
    const WrTransformProperty* transform_array,
    size_t transform_count)
WR_FUNC;

WR_INLINE WrIdNamespace
wr_api_get_namespace(WrAPI* api)
WR_FUNC;

WR_INLINE void
wr_api_send_external_event(WrAPI* api,
    size_t evt)
WR_DESTRUCTOR_SAFE_FUNC;

WR_INLINE void
wr_api_set_root_display_list(WrAPI* api,
    WrEpoch epoch,
    float viewport_width,
    float viewport_height,
    WrPipelineId pipeline_id,
    WrBuiltDisplayListDescriptor dl_descriptor,
    uint8_t* dl_data,
    size_t dl_size,
    WrAuxiliaryListsDescriptor aux_descriptor,
    uint8_t* aux_data,
    size_t aux_size)
WR_FUNC;

WR_INLINE void
wr_api_set_root_pipeline(WrAPI* api,
    WrPipelineId pipeline_id)
WR_FUNC;

WR_INLINE void
wr_api_set_window_parameters(WrAPI* api,
    int32_t width,
    int32_t height)
WR_FUNC;

WR_INLINE void
wr_api_update_image(WrAPI* api,
    WrImageKey key,
    const WrImageDescriptor* descriptor,
    WrByteSlice bytes)
WR_FUNC;

WR_INLINE void
wr_dp_begin(WrState* state,
    uint32_t width,
    uint32_t height)
WR_FUNC;

WR_INLINE void
wr_dp_end(WrState* state)
WR_FUNC;

WR_INLINE WrClipRegion
wr_dp_new_clip_region(WrState* state,
    WrRect main,
    const WrComplexClipRegion* complex,
    size_t complex_count,
    const WrImageMask* image_mask)
WR_FUNC;

WR_INLINE void
wr_dp_pop_scroll_layer(WrState* state)
WR_FUNC;

WR_INLINE void
wr_dp_pop_stacking_context(WrState* state)
WR_FUNC;

WR_INLINE void
wr_dp_push_border(WrState* state,
    WrRect rect,
    WrClipRegion clip,
    WrBorderWidths widths,
    WrBorderSide top,
    WrBorderSide right,
    WrBorderSide bottom,
    WrBorderSide left,
    WrBorderRadius radius)
WR_FUNC;

WR_INLINE void
wr_dp_push_border_gradient(WrState* state,
    WrRect rect,
    WrClipRegion clip,
    WrBorderWidths widths,
    WrPoint start_point,
    WrPoint end_point,
    const WrGradientStop* stops,
    size_t stops_count,
    WrGradientExtendMode extend_mode,
    WrSideOffsets2Df32 outset)
WR_FUNC;

WR_INLINE void
wr_dp_push_border_image(WrState* state,
    WrRect rect,
    WrClipRegion clip,
    WrBorderWidths widths,
    WrImageKey image,
    WrNinePatchDescriptor patch,
    WrSideOffsets2Df32 outset,
    WrRepeatMode repeat_horizontal,
    WrRepeatMode repeat_vertical)
WR_FUNC;

WR_INLINE void
wr_dp_push_border_radial_gradient(WrState* state,
    WrRect rect,
    WrClipRegion clip,
    WrBorderWidths widths,
    WrPoint center,
    WrSize radius,
    const WrGradientStop* stops,
    size_t stops_count,
    WrGradientExtendMode extend_mode,
    WrSideOffsets2Df32 outset)
WR_FUNC;

WR_INLINE void
wr_dp_push_box_shadow(WrState* state,
    WrRect rect,
    WrClipRegion clip,
    WrRect box_bounds,
    WrPoint offset,
    WrColor color,
    float blur_radius,
    float spread_radius,
    float border_radius,
    WrBoxShadowClipMode clip_mode)
WR_FUNC;

WR_INLINE void
wr_dp_push_built_display_list(WrState* state,
    WrBuiltDisplayListDescriptor dl_descriptor,
    WrVecU8 dl_data,
    WrAuxiliaryListsDescriptor aux_descriptor,
    WrVecU8 aux_data)
WR_FUNC;

WR_INLINE void
wr_dp_push_iframe(WrState* state,
    WrRect rect,
    WrClipRegion clip,
    WrPipelineId pipeline_id)
WR_FUNC;

WR_INLINE void
wr_dp_push_image(WrState* state,
    WrRect bounds,
    WrClipRegion clip,
    WrSize stretch_size,
    WrSize tile_spacing,
    WrImageRendering image_rendering,
    WrImageKey key)
WR_FUNC;

WR_INLINE void
wr_dp_push_linear_gradient(WrState* state,
    WrRect rect,
    WrClipRegion clip,
    WrPoint start_point,
    WrPoint end_point,
    const WrGradientStop* stops,
    size_t stops_count,
    WrGradientExtendMode extend_mode,
    WrSize tile_size,
    WrSize tile_spacing)
WR_FUNC;

WR_INLINE void
wr_dp_push_radial_gradient(WrState* state,
    WrRect rect,
    WrClipRegion clip,
    WrPoint center,
    WrSize radius,
    const WrGradientStop* stops,
    size_t stops_count,
    WrGradientExtendMode extend_mode,
    WrSize tile_size,
    WrSize tile_spacing)
WR_FUNC;

WR_INLINE void
wr_dp_push_rect(WrState* state,
    WrRect rect,
    WrClipRegion clip,
    WrColor color)
WR_FUNC;

WR_INLINE void
wr_dp_push_scroll_layer(WrState* state,
    WrRect content_rect,
    WrRect clip_rect,
    const WrImageMask* mask)
WR_FUNC;

WR_INLINE void
wr_dp_push_stacking_context(WrState* state,
    WrRect bounds,
    uint64_t animation_id,
    const float* opacity,
    const WrMatrix* transform,
    WrMixBlendMode mix_blend_mode)
WR_FUNC;

WR_INLINE void
wr_dp_push_text(WrState* state,
    WrRect bounds,
    WrClipRegion clip,
    WrColor color,
    WrFontKey font_key,
    const WrGlyphInstance* glyphs,
    uint32_t glyph_count,
    float glyph_size)
WR_FUNC;

WR_INLINE void
wr_rendered_epochs_delete(WrRenderedEpochs* pipeline_epochs)
WR_DESTRUCTOR_SAFE_FUNC;

WR_INLINE bool
wr_rendered_epochs_next(WrRenderedEpochs* pipeline_epochs,
    WrPipelineId* out_pipeline,
    WrEpoch* out_epoch)
WR_FUNC;

WR_INLINE bool
wr_renderer_current_epoch(WrRenderer* renderer,
    WrPipelineId pipeline_id,
    WrEpoch* out_epoch)
WR_FUNC;

WR_INLINE void
wr_renderer_delete(WrRenderer* renderer)
WR_DESTRUCTOR_SAFE_FUNC;

WR_INLINE WrRenderedEpochs*
wr_renderer_flush_rendered_epochs(WrRenderer* renderer)
WR_FUNC;

WR_INLINE void
wr_renderer_readback(WrRenderer* renderer,
    uint32_t width,
    uint32_t height,
    uint8_t* dst_buffer,
    size_t buffer_size)
WR_FUNC;

WR_INLINE void
wr_renderer_render(WrRenderer* renderer,
    uint32_t width,
    uint32_t height)
WR_FUNC;

WR_INLINE void
wr_renderer_set_external_image_handler(WrRenderer* renderer,
    WrExternalImageHandler* external_image_handler)
WR_FUNC;

WR_INLINE void
wr_renderer_set_profiler_enabled(WrRenderer* renderer,
    bool enabled)
WR_FUNC;

WR_INLINE void
wr_renderer_update(WrRenderer* renderer)
WR_FUNC;

WR_INLINE void
wr_state_delete(WrState* state)
WR_DESTRUCTOR_SAFE_FUNC;

WR_INLINE WrState*
wr_state_new(WrPipelineId pipeline_id)
WR_FUNC;

WR_INLINE void
wr_vec_u8_free(WrVecU8 v)
WR_FUNC;

WR_INLINE bool
wr_window_new(WrWindowId window_id,
    uint32_t window_width,
    uint32_t window_height,
    void* gl_context,
    bool enable_profiler,
    WrAPI** out_api,
    WrRenderer** out_renderer)
WR_FUNC;

/* DO NOT MODIFY THIS MANUALLY! This file was generated using cbindgen.
 * To generate this file, clone `https://github.com/rlhunt/cbindgen` or run `cargo install cbindgen`,
 * then run `cbindgen -c wr gfx/webrender_bindings/ gfx/webrender_bindings/webrender_ffi_generated.h` */
