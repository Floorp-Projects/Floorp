/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WR_h
#define WR_h
extern "C" {
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
wr_init_window(uint64_t root_pipeline_id, const char* path_utf8)
WR_FUNC;

WR_INLINE wrstate*
wr_create(wrwindowstate* wrWindow, uint32_t width, uint32_t height, uint64_t layers_id)
WR_FUNC;

WR_INLINE void
wr_destroy(wrstate* wrstate)
WR_FUNC;

WR_INLINE WRImageKey
wr_add_image(wrwindowstate* wrWindow, uint32_t width, uint32_t height,
             uint32_t stride, WRImageFormat format, uint8_t *bytes, size_t size)
WR_FUNC;

WR_INLINE void
wr_update_image(wrwindowstate* wrWindow, WRImageKey key,
                uint32_t width, uint32_t height,
                WRImageFormat format, uint8_t *bytes, size_t size)
WR_FUNC;

WR_INLINE void
wr_delete_image(wrwindowstate* wrWindow, WRImageKey key)
WR_FUNC;

WR_INLINE void
wr_push_dl_builder(wrstate *wrState)
WR_FUNC;

//XXX: matrix should use a proper type
WR_INLINE void
wr_pop_dl_builder(wrwindowstate* wrWindow, wrstate *wrState, WRRect bounds,
                  WRRect overflow, const float* matrix, uint64_t scrollId)
WR_FUNC;

WR_INLINE void
wr_dp_begin(wrwindowstate* wrWindow, wrstate* wrState, uint32_t width, uint32_t height)
WR_FUNC;

WR_INLINE void
wr_dp_end(wrwindowstate* wrWindow, wrstate* wrState)
WR_FUNC;

WR_INLINE void
wr_composite(wrwindowstate* wrWindow)
WR_FUNC;

WR_INLINE void
wr_dp_push_rect(wrstate* wrState, WRRect bounds, WRRect clip,
                float r, float g, float b, float a)
WR_FUNC;

WR_INLINE void
wr_dp_push_image(wrstate* wrState, WRRect bounds, WRRect clip,
                 const WRImageMask* mask, WRImageKey key)
WR_FUNC;

WR_INLINE void
wr_dp_push_iframe(wrstate* wrState, WRRect bounds, WRRect clip,
                  uint64_t layers_id)
WR_FUNC;

// The pointer returned by wr_readback_buffer must be freed by rust, not C.
// After using the data, it is the responsibility of the caller to free the memory
// by giving the pointer, out_length, out_capacity to wr_free_buffer.
WR_INLINE const uint8_t*
wr_readback_buffer(uint32_t width, uint32_t height, uint32_t* out_length, uint32_t* out_capacity)
WR_FUNC;

WR_INLINE void
wr_free_buffer(const uint8_t* pointer, uint32_t length, uint32_t capacity)

WR_FUNC;

#undef WR_FUNC
}
#endif
