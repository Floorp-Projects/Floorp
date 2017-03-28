/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WR_h
#define WR_h

#include "mozilla/gfx/Types.h"
#include "nsTArray.h"
#include "mozilla/gfx/Point.h"
// ---
#define WR_DECL_FFI_1(WrType, t1)                 \
struct WrType {                                   \
  t1 mHandle;                                     \
  bool operator==(const WrType& rhs) const {      \
    return mHandle == rhs.mHandle;                \
  }                                               \
  bool operator!=(const WrType& rhs) const {      \
    return mHandle != rhs.mHandle;                \
  }                                               \
  bool operator<(const WrType& rhs) const {       \
    return mHandle < rhs.mHandle;                 \
  }                                               \
  bool operator<=(const WrType& rhs) const {      \
    return mHandle <= rhs.mHandle;                \
  }                                               \
};                                                \
// ---

// ---
#define WR_DECL_FFI_2(WrType, t1, t2)             \
struct WrType {                                   \
  t1 mNamespace;                                  \
  t2 mHandle;                                     \
  bool operator==(const WrType& rhs) const {      \
    return mNamespace == rhs.mNamespace           \
        && mHandle == rhs.mHandle;                \
  }                                               \
  bool operator!=(const WrType& rhs) const {      \
    return mNamespace != rhs.mNamespace           \
        || mHandle != rhs.mHandle;                \
  }                                               \
};                                                \
// ---


extern "C" {

// If you modify any of the declarations below, make sure to update the
// serialization code in WebRenderMessageUtils.h and the rust bindings.

WR_DECL_FFI_1(WrEpoch, uint32_t)
WR_DECL_FFI_1(WrIdNamespace, uint32_t)
WR_DECL_FFI_1(WrWindowId, uint64_t)

WR_DECL_FFI_2(WrPipelineId, uint32_t, uint32_t)
WR_DECL_FFI_2(WrImageKey, uint32_t, uint32_t)
WR_DECL_FFI_2(WrFontKey, uint32_t, uint32_t)

#undef WR_DECL_FFI_1
#undef WR_DECL_FFI_2

// FFI-safe slice of bytes. Use this accross the FFI boundary to pass a temporary
// view of a buffer of bytes.
// The canonical gecko equivalent is mozilla::Range<uint8_t>.
struct WrByteSlice {
  uint8_t* mBuffer;
  size_t mLength;
};

// ----
// Functions invoked from Rust code
// ----

bool is_in_compositor_thread();
bool is_in_main_thread();
bool is_in_render_thread();
bool is_glcontext_egl(void* glcontext_ptr);
void gfx_critical_note(const char* msg);
void* get_proc_address_from_glcontext(void* glcontext_ptr, const char* procname);

// -----
// Enums used in C++ code with corresponding enums in Rust code
// -----
enum class WrBoxShadowClipMode: uint32_t {
  None,
  Outset,
  Inset,

  Sentinel /* this must be last, for IPC serialization purposes */
};

enum class WrImageFormat: uint32_t
{
  Invalid = 0,
  A8      = 1,
  RGB8    = 2,
  RGBA8   = 3,
  RGBAF32 = 4,

  Sentinel /* this must be last, for IPC serialization purposes */
};

enum class WrBorderStyle: uint32_t
{
  None    = 0,
  Solid   = 1,
  Double  = 2,
  Dotted  = 3,
  Dashed  = 4,
  Hidden  = 5,
  Groove  = 6,
  Ridge   = 7,
  Inset   = 8,
  Outset  = 9,

  Sentinel /* this must be last, for IPC serialization purposes */
};

enum class WrImageRendering: uint32_t
{
  Auto        = 0,
  CrispEdges  = 1,
  Pixelated   = 2,

  Sentinel /* this must be last, for IPC serialization purposes */
};

enum class WrExternalImageIdType: uint32_t
{
  NativeTexture, // Currently, we only support gl texture handle.
  RawData,

  Sentinel /* this must be last, for IPC serialization purposes */
};

enum class WrMixBlendMode: uint32_t
{
  Normal      = 0,
  Multiply    = 1,
  Screen      = 2,
  Overlay     = 3,
  Darken      = 4,
  Lighten     = 5,
  ColorDodge  = 6,
  ColorBurn   = 7,
  HardLight   = 8,
  SoftLight   = 9,
  Difference  = 10,
  Exclusion   = 11,
  Hue         = 12,
  Saturation  = 13,
  Color       = 14,
  Luminosity  = 15,

  Sentinel /* this must be last, for IPC serialization purposes */
};

enum class WrGradientExtendMode : uint32_t
{
  Clamp      = 0,
  Repeat     = 1,

  Sentinel /* this must be last, for IPC serialization purposes */
};

enum class WrRepeatMode : uint32_t
{
  Stretch      = 0,
  Repeat       = 1,
  Round        = 2,
  Space        = 3,

  Sentinel /* this must be last, for IPC serialization purposes */
};

// -----
// Typedefs for struct fields and function signatures below.
// -----

typedef uint64_t WrImageIdType;

// -----
// Structs used in C++ code with corresponding types in Rust code
// -----

struct WrItemRange
{
  size_t start;
  size_t length;
};

struct WrBuiltDisplayListDescriptor {
  size_t display_list_items_size;
};

struct WrAuxiliaryListsDescriptor {
  size_t gradient_stops_size;
  size_t complex_clip_regions_size;
  size_t filters_size;
  size_t glyph_instances_size;
};

struct WrPoint
{
  float x;
  float y;

  bool operator==(const WrPoint& aRhs) const {
    return x == aRhs.x && y == aRhs.y;
  }

  operator mozilla::gfx::Point() const { return mozilla::gfx::Point(x, y); }
};

struct WrSize
{
  float width;
  float height;

  bool operator==(const WrSize& aRhs) const
  {
    return width == aRhs.width && height == aRhs.height;
  }
};

struct WrRect
{
  float x;
  float y;
  float width;
  float height;

  bool operator==(const WrRect& aRhs) const
  {
    return x == aRhs.x && y == aRhs.y &&
           width == aRhs.width && height == aRhs.height;
  }
};

struct WrColor
{
  float r;
  float g;
  float b;
  float a;

  bool operator==(const WrColor& aRhs) const
  {
    return r == aRhs.r && g == aRhs.g &&
           b == aRhs.b && a == aRhs.a;
  }
};

struct WrGlyphInstance
{
  uint32_t index;
  float x;
  float y;

  bool operator==(const WrGlyphInstance& other) const
  {
    return index == other.index &&
           x == other.x &&
           y == other.y;
  }
};

// Note that the type is slightly different than
// the definition in bindings.rs. WrGlyphInstance
// versus GlyphInstance, but their layout is the same.
// So we're really overlapping the types for the same memory.
struct WrGlyphArray
{
  mozilla::gfx::Color color;
  nsTArray<WrGlyphInstance> glyphs;

  bool operator==(const WrGlyphArray& other) const
  {
    if (!(color == other.color) ||
       (glyphs.Length() != other.glyphs.Length())) {
      return false;
    }

    for (size_t i = 0; i < glyphs.Length(); i++) {
      if (!(glyphs[i] == other.glyphs[i])) {
        return false;
      }
    }

    return true;
  }
};

struct WrGradientStop {
  float offset;
  WrColor color;

  bool operator==(const WrGradientStop& aRhs) const
  {
    return offset == aRhs.offset && color == aRhs.color;
  }
};

struct WrBorderSide {
  WrColor color;
  WrBorderStyle style;

  bool operator==(const WrBorderSide& aRhs) const
  {
    return color == aRhs.color && style == aRhs.style;
  }
};

struct WrBorderRadius {
  WrSize top_left;
  WrSize top_right;
  WrSize bottom_left;
  WrSize bottom_right;

  bool operator==(const WrBorderRadius& aRhs) const
  {
    return top_left == aRhs.top_left && top_right == aRhs.top_right &&
           bottom_left == aRhs.bottom_left && bottom_right == aRhs.bottom_right;
  }
};

struct WrBorderWidths {
  float left;
  float top;
  float right;
  float bottom;

  bool operator==(const WrBorderWidths& aRhs) const
  {
    return left == aRhs.left && top == aRhs.top &&
           right == aRhs.right && bottom == aRhs.bottom;
  }
};

struct WrSideOffsets2Du32 {
  uint32_t top;
  uint32_t right;
  uint32_t bottom;
  uint32_t left;

  bool operator==(const WrSideOffsets2Du32& aRhs) const
  {
    return top == aRhs.top && right == aRhs.right &&
           bottom == aRhs.bottom && left == aRhs.left;
  }
};

struct WrSideOffsets2Df32 {
  float top;
  float right;
  float bottom;
  float left;

  bool operator==(const WrSideOffsets2Df32& aRhs) const
  {
    return top == aRhs.top && right == aRhs.right &&
           bottom == aRhs.bottom && left == aRhs.left;
  }
};

struct WrNinePatchDescriptor {
  uint32_t width;
  uint32_t height;
  WrSideOffsets2Du32 slice;

  bool operator==(const WrNinePatchDescriptor& aRhs) const
  {
    return width == aRhs.width && height == aRhs.height &&
           slice == aRhs.slice;
  }
};

struct WrImageMask
{
  WrImageKey image;
  WrRect rect;
  bool repeat;

  bool operator==(const WrImageMask& aRhs) const
  {
    return image == aRhs.image && rect == aRhs.rect && repeat == aRhs.repeat;
  }
};

struct WrComplexClipRegion
{
  WrRect rect;
  WrBorderRadius radii;
};

struct WrClipRegion
{
  WrRect main;
  WrItemRange complex;
  WrImageMask image_mask;
  bool has_image_mask;
};

struct WrExternalImageId
{
  WrImageIdType id;
};

struct WrExternalImage
{
  WrExternalImageIdType type;

  // Texture coordinate
  float u0, v0;
  float u1, v1;

  // external buffer handle
  uint32_t handle;

  // handle RawData.
  uint8_t* buff;
  size_t size;
};

typedef WrExternalImage (*LockExternalImageCallback)(void*, WrExternalImageId);
typedef void (*UnlockExternalImageCallback)(void*, WrExternalImageId);
typedef void (*ReleaseExternalImageCallback)(void*, WrExternalImageId);

struct WrExternalImageHandler
{
  void* renderer_obj;
  LockExternalImageCallback lock_func;
  UnlockExternalImageCallback unlock_func;
  ReleaseExternalImageCallback release_func;
};

struct WrImageDescriptor {
    WrImageFormat format;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    bool is_opaque;
};

struct WrVecU8 {
    uint8_t *data;
    size_t length;
    size_t capacity;
};

// -----
// Functions exposed by the webrender API
// -----

// Some useful defines to stub out webrender binding functions for when we
// build gecko without webrender. We try to tell the compiler these functions
// are unreachable in that case, but VC++ emits a warning if it finds any
// unreachable functions invoked from destructors. That warning gets turned into
// an error and causes the build to fail. So for wr_* functions called by
// destructors in C++ classes, use WR_DESTRUCTOR_SAFE_FUNC instead, which omits
// the unreachable annotation.
#ifdef MOZ_BUILD_WEBRENDER
#  define WR_INLINE
#  define WR_FUNC
#  define WR_DESTRUCTOR_SAFE_FUNC
#else
#  define WR_INLINE inline
#  define WR_FUNC { MOZ_MAKE_COMPILER_ASSUME_IS_UNREACHABLE("WebRender disabled"); }
#  define WR_DESTRUCTOR_SAFE_FUNC {}
#endif

// Structs defined in Rust, but opaque to C++ code.
struct WrRenderedEpochs;
struct WrRenderer;
struct WrState;
struct WrAPI;

WR_INLINE void
wr_renderer_set_external_image_handler(WrRenderer* renderer,
                                       WrExternalImageHandler* handler)
WR_FUNC;

WR_INLINE void
wr_renderer_update(WrRenderer* renderer)
WR_FUNC;

WR_INLINE void
wr_renderer_render(WrRenderer* renderer, uint32_t width, uint32_t height)
WR_FUNC;

// It is the responsibility of the caller to manage the dst_buffer memory
// and also free it at the proper time.
WR_INLINE const uint8_t*
wr_renderer_readback(WrRenderer* renderer,
                     uint32_t width, uint32_t height,
                     uint8_t* dst_buffer, size_t buffer_length)
WR_FUNC;

WR_INLINE void
wr_renderer_set_profiler_enabled(WrRenderer* renderer, bool enabled)
WR_FUNC;

WR_INLINE bool
wr_renderer_current_epoch(WrRenderer* renderer, WrPipelineId pipeline_id,
                          WrEpoch* out_epoch)
WR_FUNC;

WR_INLINE void
wr_renderer_delete(WrRenderer* renderer)
WR_DESTRUCTOR_SAFE_FUNC;

WR_INLINE WrRenderedEpochs*
wr_renderer_flush_rendered_epochs(WrRenderer* renderer) WR_FUNC;

WR_INLINE bool
wr_rendered_epochs_next(WrRenderedEpochs* pipeline_epochs,
                        WrPipelineId* out_pipeline,
                        WrEpoch* out_epoch) WR_FUNC;

WR_INLINE void
wr_rendered_epochs_delete(WrRenderedEpochs* pipeline_epochs) WR_DESTRUCTOR_SAFE_FUNC;

WR_INLINE bool
wr_window_new(WrWindowId window_id,
              uint32_t window_width,
              uint32_t window_height,
              void* aGLContext,
              bool enable_profiler,
              WrAPI** out_api,
              WrRenderer** out_renderer)
WR_FUNC;

WR_INLINE void
wr_api_delete(WrAPI* api)
WR_DESTRUCTOR_SAFE_FUNC;

WR_INLINE void
wr_api_add_image(WrAPI* api, WrImageKey key, const WrImageDescriptor* descriptor, const WrByteSlice aSlice)
WR_FUNC;

WR_INLINE void
wr_api_add_blob_image(WrAPI* api, WrImageKey key, const WrImageDescriptor* descriptor, const WrByteSlice aSlice)
WR_FUNC;

WR_INLINE void
wr_api_add_external_image_handle(WrAPI* api, WrImageKey key, uint32_t width, uint32_t height,
                                 WrImageFormat format, uint64_t external_image_id)
WR_FUNC;

WR_INLINE void
wr_api_add_external_image_buffer(WrAPI* api, WrImageKey key,
                                 const WrImageDescriptor* descriptor,
                                 uint64_t external_image_id)
WR_FUNC;

WR_INLINE void
wr_api_update_image(WrAPI* api, WrImageKey key,
                    const WrImageDescriptor* descriptor,
                    const WrByteSlice bytes)
WR_FUNC;

WR_INLINE void
wr_api_delete_image(WrAPI* api, WrImageKey key)
WR_FUNC;

WR_INLINE void
wr_api_set_root_pipeline(WrAPI* api, WrPipelineId pipeline_id)
WR_FUNC;

WR_INLINE void
wr_api_set_window_parameters(WrAPI* api, int width, int height)
WR_FUNC;

WR_INLINE void
wr_api_set_root_display_list(WrAPI* api, WrEpoch epoch, float w, float h,
                             WrPipelineId pipeline_id,
                             WrBuiltDisplayListDescriptor dl_descriptor,
                             uint8_t *dl_data,
                             size_t dl_size,
                             WrAuxiliaryListsDescriptor aux_descriptor,
                             uint8_t *aux_data,
                             size_t aux_size)
WR_FUNC;

WR_INLINE void
wr_api_clear_root_display_list(WrAPI* api, WrEpoch epoch, WrPipelineId pipeline_id)
WR_FUNC;

WR_INLINE void
wr_api_generate_frame(WrAPI* api)
WR_FUNC;

WR_INLINE void
wr_api_send_external_event(WrAPI* api, uintptr_t evt)
WR_DESTRUCTOR_SAFE_FUNC;

WR_INLINE void
wr_api_add_raw_font(WrAPI* api, WrFontKey key, uint8_t* font_buffer, size_t buffer_size)
WR_FUNC;

WR_INLINE WrIdNamespace
wr_api_get_namespace(WrAPI* api)
WR_FUNC;

WR_INLINE WrState*
wr_state_new(WrPipelineId pipeline_id)
WR_FUNC;

WR_INLINE void
wr_state_delete(WrState* state)
WR_DESTRUCTOR_SAFE_FUNC;

WR_INLINE void
wr_dp_begin(WrState* wrState, uint32_t width, uint32_t height)
WR_FUNC;

WR_INLINE void
wr_dp_end(WrState* wrState)
WR_FUNC;

WR_INLINE WrClipRegion
wr_dp_new_clip_region(WrState* wrState,
                      WrRect main,
                      const WrComplexClipRegion* complex, size_t complexCount,
                      const WrImageMask* image_mask)
WR_FUNC;

WR_INLINE void
wr_dp_push_stacking_context(WrState *wrState, WrRect bounds,
                            WrRect overflow, const WrImageMask *mask,
                            float opacity, const float* matrix,
                            WrMixBlendMode mixBlendMode)
WR_FUNC;

//XXX: matrix should use a proper type
WR_INLINE void
wr_dp_pop_stacking_context(WrState *wrState)
WR_FUNC;

WR_INLINE void
wr_dp_push_scroll_layer(WrState *wrState, WrRect bounds,
                        WrRect overflow, const WrImageMask *mask)
WR_FUNC;

WR_INLINE void
wr_dp_pop_scroll_layer(WrState *wrState)
WR_FUNC;

WR_INLINE void
wr_dp_push_iframe(WrState* wrState, WrRect bounds, WrClipRegion clip, WrPipelineId layers_id)
WR_FUNC;

WR_INLINE void
wr_dp_push_rect(WrState* wrState, WrRect bounds, WrClipRegion clip,
                WrColor color)
WR_FUNC;

WR_INLINE void
wr_dp_push_image(WrState* wrState, WrRect bounds, WrClipRegion clip,
                 WrImageRendering filter, WrImageKey key)
WR_FUNC;

WR_INLINE void
wr_dp_push_text(WrState* wrState, WrRect bounds, WrClipRegion clip, WrColor color,
                WrFontKey font_Key, const WrGlyphInstance* glyphs,
                uint32_t glyph_count, float glyph_size)
WR_FUNC;

WR_INLINE void
wr_dp_push_border(WrState* wrState, WrRect bounds, WrClipRegion clip,
                  WrBorderWidths widths,
                  WrBorderSide top, WrBorderSide right, WrBorderSide bottom, WrBorderSide left,
                  WrBorderRadius radius)
WR_FUNC;

WR_INLINE void
wr_dp_push_border_image(WrState* wrState, WrRect bounds, WrClipRegion clip,
                        WrBorderWidths widths,
                        WrImageKey image, WrNinePatchDescriptor patch, WrSideOffsets2Df32 outset,
                        WrRepeatMode repeat_horizontal,
                        WrRepeatMode repeat_vertical)
WR_FUNC;

WR_INLINE void
wr_dp_push_linear_gradient(WrState* wrState, WrRect bounds, WrClipRegion clip,
                           WrPoint startPoint, WrPoint endPoint,
                           const WrGradientStop* stops, size_t stopsCount,
                           WrGradientExtendMode extendMode)
WR_FUNC;

WR_INLINE void
wr_dp_push_radial_gradient(WrState* wrState, WrRect bounds, WrClipRegion clip,
                           WrPoint startCenter, WrPoint endCenter,
                           float startRadius, float endRadius,
                           const WrGradientStop* stops, size_t stopsCount,
                           WrGradientExtendMode extendMode)
WR_FUNC;

WR_INLINE void
wr_dp_push_box_shadow(WrState* wrState, WrRect rect, WrClipRegion clip,
                      WrRect box_bounds, WrPoint offset, WrColor color,
                      float blur_radius, float spread_radius, float border_radius,
                      WrBoxShadowClipMode clip_mode)
WR_FUNC;

WR_INLINE void
wr_api_finalize_builder(WrState* wrState,
                        WrBuiltDisplayListDescriptor& dl_descriptor,
                        WrVecU8& dl_data,
                        WrAuxiliaryListsDescriptor& aux_descriptor,
                        WrVecU8& aux_data)
WR_FUNC;

WR_INLINE void
wr_dp_push_built_display_list(WrState* wrState,
                              WrBuiltDisplayListDescriptor dl_descriptor,
                              WrVecU8 dl_data,
                              WrAuxiliaryListsDescriptor aux_descriptor,
                              WrVecU8 aux_data)
WR_FUNC;

WR_INLINE void
wr_vec_u8_free(WrVecU8 dl_data)
WR_FUNC;

#undef WR_FUNC
#undef WR_DESTRUCTOR_SAFE_FUNC
} // extern "C"

#endif // WR_h
