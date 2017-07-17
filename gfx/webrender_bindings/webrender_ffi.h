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
bool gfx_use_wrench();
void gfx_critical_note(const char* msg);
void* get_proc_address_from_glcontext(void* glcontext_ptr, const char* procname);

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

#endif // WR_h
