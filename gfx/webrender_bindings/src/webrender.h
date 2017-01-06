/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WR_h
#define WR_h

#include "mozilla/layers/LayersMessages.h"

extern "C" {
bool is_in_compositor_thread();
void* get_proc_address_from_glcontext(void* glcontext_ptr, const char* procname);

enum WRImageFormat {
    Invalid,
    A8,
    RGB8,
    RGBA8,
    RGBAF32
};

struct WRImageKey {
  uint32_t a;
  uint32_t b;

  bool operator==(const WRImageKey& aRhs) const {
    return a == aRhs.a && b == aRhs.b;
  }
};

struct WRColor {
  float r;
  float g;
  float b;
  float a;

  bool operator==(const WRColor& aRhs) const {
    return r == aRhs.r && g == aRhs.g &&
           b == aRhs.b && a == aRhs.a;
  }
};

struct WRGlyphInstance {
  uint32_t index;
  float x;
  float y;

  bool operator==(const WRGlyphInstance& other) const {
    return index == other.index &&
           x == other.x &&
           y == other.y;
  }
};

// Note that the type is slightly different than
// the definition in bindings.rs. WRGlyphInstance
// versus GlyphInstance, but their layout is the same.
// So we're really overlapping the types for the same memory.
struct WRGlyphArray {
  WRColor color;
  nsTArray<WRGlyphInstance> glyphs;

  bool operator==(const WRGlyphArray& other) const {
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

enum WRBorderStyle {
  None,
  Solid,
  Double,
  Dotted,
  Dashed,
  Hidden,
  Groove,
  Ridge,
  Inset,
  Outset
};

struct WRBorderSide {
  float width;
  WRColor color;
  WRBorderStyle style;

  bool operator==(const WRBorderSide& aRhs) const {
    return width == aRhs.width && color == aRhs.color &&
           style == aRhs.style;
  }
};

struct WRLayoutSize {
  float width;
  float height;

  bool operator==(const WRLayoutSize& aRhs) const {
    return width == aRhs.width && height == aRhs.height;
  }
};

struct WRRect {
  float x;
  float y;
  float width;
  float height;

  bool operator==(const WRRect& aRhs) const {
    return x == aRhs.x && y == aRhs.y &&
           width == aRhs.width && height == aRhs.height;
  }
};

struct WRImageMask
{
    WRImageKey image;
    WRRect rect;
    bool repeat;

    bool operator==(const WRImageMask& aRhs) const {
      return image == aRhs.image && rect == aRhs.rect && repeat == aRhs.repeat;
    }
};

typedef uint64_t WRImageIdType;
struct WRExternalImageId {
  WRImageIdType id;
};

enum WRExternalImageType {
    TEXTURE_HANDLE, // Currently, we only support gl texture handle.
    // TODO(Jerry): handle shmem or cpu raw buffers.
    //// MEM_OR_SHMEM,
};

struct WRExternalImage {
  WRExternalImageType type;

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

typedef WRExternalImage (*GetExternalImageCallback)(void*, WRExternalImageId);
typedef void (*ReleaseExternalImageCallback)(void*, WRExternalImageId);

struct WRExternalImageHandler {
  void* ExternalImageObj;
  GetExternalImageCallback get_func;
  ReleaseExternalImageCallback release_func;
};

struct wrwindowstate;
struct wrstate;

#ifdef MOZ_ENABLE_WEBRENDER
#  define WR_INLINE
#  define WR_FUNC
#else
#  define WR_INLINE inline
#  define WR_FUNC { MOZ_MAKE_COMPILER_ASSUME_IS_UNREACHABLE("WebRender disabled"); }
#endif

WR_INLINE wrwindowstate*
wr_init_window(uint64_t root_pipeline_id,
               void* webrender_bridge_ptr,
               bool enable_profiler,
               WRExternalImageHandler* handler = nullptr)
WR_FUNC;

WR_INLINE wrstate*
wr_create(wrwindowstate* wrWindow, uint32_t width, uint32_t height, uint64_t layers_id)
WR_FUNC;

WR_INLINE void
wr_destroy(wrwindowstate* wrWindow, wrstate* wrstate)
WR_FUNC;

WR_INLINE WRImageKey
wr_add_image(wrwindowstate* wrWindow, uint32_t width, uint32_t height,
             uint32_t stride, WRImageFormat format, uint8_t *bytes, size_t size)
WR_FUNC;

WR_INLINE WRImageKey
wr_add_external_image_texture(wrwindowstate* wrWindow, uint32_t width, uint32_t height,
                              WRImageFormat format, uint64_t external_image_id)
WR_FUNC;

//TODO(Jerry): handle shmem in WR
//// WR_INLINE WRImageKey
//// wr_add_external_image_buffer(wrwindowstate* wrWindow, uint32_t width, uint32_t height,
////                              uint32_t stride, WRImageFormat format, uint8_t *bytes, size_t size)
//// WR_FUNC;

WR_INLINE void
wr_update_image(wrwindowstate* wrWindow, WRImageKey key,
                uint32_t width, uint32_t height,
                WRImageFormat format, uint8_t *bytes, size_t size)
WR_FUNC;

WR_INLINE void
wr_delete_image(wrwindowstate* wrWindow, WRImageKey key)
WR_FUNC;

WR_INLINE void
wr_dp_push_stacking_context(wrstate *wrState, WRRect bounds,
                         WRRect overflow, const float* matrix)
WR_FUNC;

//XXX: matrix should use a proper type
WR_INLINE void
wr_dp_pop_stacking_context(wrstate *wrState)
WR_FUNC;

WR_INLINE void
wr_dp_begin(wrwindowstate* wrWindow, wrstate* wrState, uint32_t width, uint32_t height)
WR_FUNC;

WR_INLINE void
wr_dp_end(wrwindowstate* wrWindow, wrstate* wrState)
WR_FUNC;

WR_INLINE void
wr_composite_window(wrwindowstate* wrWindow)
WR_FUNC;

WR_INLINE void
wr_dp_push_rect(wrstate* wrState, WRRect bounds, WRRect clip,
                float r, float g, float b, float a)
WR_FUNC;

WR_INLINE void
wr_dp_push_border(wrstate* wrState, WRRect bounds, WRRect clip,
                  WRBorderSide top, WRBorderSide right, WRBorderSide bottom, WRBorderSide left,
                  WRLayoutSize top_left_radius, WRLayoutSize top_right_radius,
                  WRLayoutSize bottom_left_radius, WRLayoutSize bottom_right_radius)
WR_FUNC;

WR_INLINE void
wr_dp_push_image(wrstate* wrState, WRRect bounds, WRRect clip,
                 const WRImageMask* mask, WRImageKey key)
WR_FUNC;

WR_INLINE void
wr_dp_push_iframe(wrwindowstate* wrWindow, wrstate* wrState, WRRect bounds, WRRect clip,
                  uint64_t layers_id)
WR_FUNC;

// It is the responsibility of the caller to manage the dst_buffer memory
// and also free it at the proper time.
WR_INLINE const uint8_t*
wr_readback_into_buffer(wrwindowstate* wrWindow, uint32_t width, uint32_t height,
                        uint8_t* dst_buffer, uint32_t buffer_length)
WR_FUNC;

WR_INLINE void
wr_profiler_set_enabled(wrwindowstate* wrWindow, bool enabled)
WR_FUNC;

WR_INLINE void
wr_dp_push_text(wrwindowstate* wrWindow, wrstate* wrState,
                WRRect bounds, WRRect clip,
                WRColor color, const WRGlyphInstance* glyphs,
                uint32_t glyph_count, float glyph_size,
                uint8_t* font_buffer, uint32_t buffer_size)
WR_FUNC;

#undef WR_FUNC
}
#endif
