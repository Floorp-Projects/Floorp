/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WR_h
#define WR_h

#include "mozilla/gfx/Types.h"
#include "nsTArray.h"

extern "C" {

// ----
// Functions invoked from Rust code
// ----

bool is_in_compositor_thread();
bool is_in_main_thread();
bool is_in_render_thread();
bool is_glcontext_egl(void* glcontext_ptr);
bool is_glcontext_angle(void* glcontext_ptr);
bool gfx_use_wrench();
const char* gfx_wr_resource_path_override();
void gfx_critical_note(const char* msg);
void gfx_critical_error(const char* msg);
void gecko_printf_stderr_output(const char* msg);
void* get_proc_address_from_glcontext(void* glcontext_ptr, const char* procname);
void gecko_profiler_register_thread(const char* threadname);
void gecko_profiler_unregister_thread();

// Prelude of types necessary before including webrender_ffi_generated.h
namespace mozilla {
namespace wr {

struct FontInstanceFlags {
  uint32_t bits;

  bool operator==(const FontInstanceFlags& aOther) const {
    return bits == aOther.bits;
  }

  FontInstanceFlags& operator=(uint32_t aBits) {
    bits = aBits;
    return *this;
  }

  FontInstanceFlags& operator|=(uint32_t aBits) {
    bits |= aBits;
    return *this;
  }

  FontInstanceFlags operator|(uint32_t aBits) {
    FontInstanceFlags flags = { bits | aBits };
    return flags;
  }

  FontInstanceFlags& operator&=(uint32_t aBits) {
    bits &= aBits;
    return *this;
  }

  FontInstanceFlags operator&(uint32_t aBits) {
    FontInstanceFlags flags = { bits & aBits };
    return flags;
  }

  enum : uint32_t {
    SYNTHETIC_BOLD    = 1 << 1,
    EMBEDDED_BITMAPS  = 1 << 2,
    SUBPIXEL_BGR      = 1 << 3,
    TRANSPOSE         = 1 << 4,
    FLIP_X            = 1 << 5,
    FLIP_Y            = 1 << 6,
    SUBPIXEL_POSITION = 1 << 7,

    FORCE_GDI         = 1 << 16,

    FONT_SMOOTHING    = 1 << 16,

    FORCE_AUTOHINT    = 1 << 16,
    NO_AUTOHINT       = 1 << 17,
    VERTICAL_LAYOUT   = 1 << 18,
    LCD_VERTICAL      = 1 << 19
  };
};

struct Transaction;
struct WrWindowId;
struct WrPipelineInfo;

} // namespace wr
} // namespace mozilla

void apz_register_updater(mozilla::wr::WrWindowId aWindowId);
void apz_pre_scene_swap(mozilla::wr::WrWindowId aWindowId);
void apz_post_scene_swap(mozilla::wr::WrWindowId aWindowId, mozilla::wr::WrPipelineInfo aInfo);
void apz_run_updater(mozilla::wr::WrWindowId aWindowId);
void apz_deregister_updater(mozilla::wr::WrWindowId aWindowId);

void apz_register_sampler(mozilla::wr::WrWindowId aWindowId);
void apz_sample_transforms(mozilla::wr::WrWindowId aWindowId, mozilla::wr::Transaction *aTransaction);
void apz_deregister_sampler(mozilla::wr::WrWindowId aWindowId);
} // extern "C"

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

#include "webrender_ffi_generated.h"

#undef WR_FUNC
#undef WR_DESTRUCTOR_SAFE_FUNC

#endif // WR_h
