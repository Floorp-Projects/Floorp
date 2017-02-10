/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WR_h
#define WR_h

#include "mozilla/layers/LayersMessages.h"
#include "mozilla/gfx/Types.h"

extern "C" {

// ----
// Functions invoked from Rust code
// ----

bool is_in_compositor_thread();
bool is_in_render_thread();
void* get_proc_address_from_glcontext(void* glcontext_ptr, const char* procname);

// -----
// Enums used in C++ code with corresponding enums in Rust code
// -----

enum class WrImageFormat
{
  Invalid,
  A8,
  RGB8,
  RGBA8,
  RGBAF32,

  Sentinel /* this must be last, for IPC serialization purposes */
};

enum class WrBorderStyle
{
  None,
  Solid,
  Double,
  Dotted,
  Dashed,
  Hidden,
  Groove,
  Ridge,
  Inset,
  Outset,

  Sentinel /* this must be last, for IPC serialization purposes */
};

enum class WrTextureFilter
{
  Linear,
  Point,

  Sentinel /* this must be last, for IPC serialization purposes */
};

enum class WrExternalImageIdType
{
  TEXTURE_HANDLE, // Currently, we only support gl texture handle.
  // TODO(Jerry): handle shmem or cpu raw buffers.
  //// MEM_OR_SHMEM,
};

enum class WrMixBlendMode
{
  Normal,
  Multiply,
  Screen,
  Overlay,
  Darken,
  Lighten,
  ColorDodge,
  ColorBurn,
  HardLight,
  SoftLight,
  Difference,
  Exclusion,
  Hue,
  Saturation,
  Color,
  Luminosity,

  Sentinel /* this must be last, for IPC serialization purposes */
};

#ifdef DEBUG
// This ensures that the size of |enum class| and |enum| are the same, because
// we use |enum class| in this file (to avoid polluting the global namespace)
// but Rust assumes |enum|. If there is a size mismatch that could lead to
// problems with values being corrupted across the language boundary.
class DebugEnumSizeChecker
{ // scope the enum to the class
  enum DummyWrImageFormatEnum
  {
    Invalid,
    A8,
    RGB8,
    RGBA8,
    RGBAF32,
    Sentinel
  };

  static_assert(sizeof(WrImageFormat) == sizeof(DummyWrImageFormatEnum),
    "Size of enum doesn't match size of enum class!");
};
#endif

// -----
// Typedefs for struct fields and function signatures below.
// -----

typedef uint64_t WrWindowId;
typedef uint64_t WrImageKey;
typedef uint64_t WrFontKey;
typedef uint64_t WrPipelineId;
typedef uint32_t WrEpoch;
typedef uint64_t WrImageIdType;

// -----
// Structs used in C++ code with corresponding types in Rust code
// -----

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

struct WrBorderSide {
  float width;
  WrColor color;
  WrBorderStyle style;

  bool operator==(const WrBorderSide& aRhs) const
  {
    return width == aRhs.width && color == aRhs.color &&
           style == aRhs.style;
  }
};

struct WrLayoutSize
{
  float width;
  float height;

  bool operator==(const WrLayoutSize& aRhs) const
  {
    return width == aRhs.width && height == aRhs.height;
  }
};

struct WrBorderRadius {
  WrLayoutSize top_left;
  WrLayoutSize top_right;
  WrLayoutSize bottom_left;
  WrLayoutSize bottom_right;

  bool operator==(const WrBorderRadius& aRhs) const
  {
    return top_left == aRhs.top_left && top_right == aRhs.top_right &&
           bottom_left == aRhs.bottom_left && bottom_right == aRhs.bottom_right;
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

struct WrExternalImageIdId
{
  WrImageIdType id;
};

struct WrExternalImageId
{
  WrExternalImageIdType type;

  // Texture coordinate
  float u0, v0;
  float u1, v1;

  // external buffer handle
  uint32_t handle;

  // TODO(Jerry): handle shmem or cpu raw buffers.
  //// shmem or memory buffer
  //// uint8_t* buff;
  //// size_t size;
};

typedef WrExternalImageId (*LockExternalImageCallback)(void*, WrExternalImageIdId);
typedef void (*UnlockExternalImageCallback)(void*, WrExternalImageIdId);
typedef void (*ReleaseExternalImageCallback)(void*, WrExternalImageIdId);

struct WrExternalImageIdHandler
{
  void* ExternalImageObj;
  LockExternalImageCallback lock_func;
  UnlockExternalImageCallback unlock_func;
  ReleaseExternalImageCallback release_func;
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
#ifdef MOZ_ENABLE_WEBRENDER
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
wr_renderer_update(WrRenderer* renderer)
WR_FUNC;

WR_INLINE void
wr_renderer_render(WrRenderer* renderer, uint32_t width, uint32_t height)
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

WR_INLINE void
wr_gl_init(void* aGLContext)
WR_FUNC;

WR_INLINE void
wr_window_new(WrWindowId window_id, bool enable_profiler, WrAPI** out_api,
              WrRenderer** out_renderer)
WR_FUNC;

WR_INLINE void
wr_api_delete(WrAPI* api)
WR_DESTRUCTOR_SAFE_FUNC;

WR_INLINE WrImageKey
wr_api_add_image(WrAPI* api, uint32_t width, uint32_t height,
                 uint32_t stride, WrImageFormat format, uint8_t *bytes,
                 size_t size)
WR_FUNC;

WR_INLINE WrImageKey
wr_api_add_external_image_texture(WrAPI* api, uint32_t width, uint32_t height,
                                  WrImageFormat format, uint64_t external_image_id)
WR_FUNC;

//TODO(Jerry): handle shmem in WR
//// WR_INLINE WrImageKey
//// wr_api_add_external_image_buffer(WrAPI* api, uint32_t width, uint32_t height, uint32_t stride,
////                                  WrImageFormat format, uint8_t *bytes, size_t size)
//// WR_FUNC;

WR_INLINE void
wr_api_update_image(WrAPI* api, WrImageKey key, uint32_t width, uint32_t height,
                    WrImageFormat format, uint8_t *bytes, size_t size)
WR_FUNC;

WR_INLINE void
wr_api_delete_image(WrAPI* api, WrImageKey key)
WR_FUNC;

WR_INLINE void
wr_api_set_root_pipeline(WrAPI* api, WrPipelineId pipeline_id)
WR_FUNC;

WR_INLINE void
wr_api_set_root_display_list(WrAPI* api, WrState* state, uint32_t epoch, float w, float h)
WR_FUNC;

WR_INLINE void
wr_api_send_external_event(WrAPI* api, uintptr_t evt)
WR_DESTRUCTOR_SAFE_FUNC;

WR_INLINE WrFontKey
wr_api_add_raw_font(WrAPI* api, uint8_t* font_buffer, size_t buffer_size)
WR_FUNC;

WR_INLINE WrState*
wr_state_new(uint32_t width, uint32_t height, WrPipelineId pipeline_id)
WR_FUNC;

WR_INLINE void
wr_state_delete(WrState* state)
WR_DESTRUCTOR_SAFE_FUNC;

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
wr_dp_begin(WrState* wrState, uint32_t width, uint32_t height)
WR_FUNC;

WR_INLINE void
wr_dp_end(WrState* builder, WrAPI* api, uint32_t epoch)
WR_FUNC;

WR_INLINE void
wr_dp_push_rect(WrState* wrState, WrRect bounds, WrRect clip,
                WrColor color)
WR_FUNC;

WR_INLINE void
wr_dp_push_text(WrState* wrState, WrRect bounds, WrRect clip, WrColor color,
                WrFontKey font_Key, const WrGlyphInstance* glyphs,
                uint32_t glyph_count, float glyph_size)
WR_FUNC;

WR_INLINE void
wr_dp_push_border(WrState* wrState, WrRect bounds, WrRect clip,
                  WrBorderSide top, WrBorderSide right, WrBorderSide bottom, WrBorderSide left,
                  WrBorderRadius radius)
WR_FUNC;

WR_INLINE void
wr_dp_push_image(WrState* wrState, WrRect bounds, WrRect clip,
                 const WrImageMask* mask, WrTextureFilter filter, WrImageKey key)
WR_FUNC;

WR_INLINE void
wr_dp_push_iframe(WrState* wrState, WrRect bounds, WrRect clip, WrPipelineId layers_id)
WR_FUNC;

// It is the responsibility of the caller to manage the dst_buffer memory
// and also free it at the proper time.
WR_INLINE const uint8_t*
wr_renderer_readback(uint32_t width, uint32_t height,
                     uint8_t* dst_buffer, size_t buffer_length)
WR_FUNC;

#undef WR_FUNC
#undef WR_DESTRUCTOR_SAFE_FUNC
} // extern "C"

#endif // WR_h
