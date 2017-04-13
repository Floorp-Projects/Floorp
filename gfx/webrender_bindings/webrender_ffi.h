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
WR_DECL_FFI_1(WrExternalImageId, uint64_t)

WR_DECL_FFI_2(WrPipelineId, uint32_t, uint32_t)
WR_DECL_FFI_2(WrImageKey, uint32_t, uint32_t)
WR_DECL_FFI_2(WrFontKey, uint32_t, uint32_t)

#undef WR_DECL_FFI_1
#undef WR_DECL_FFI_2

// ----
// Functions invoked from Rust code
// ----

bool is_in_compositor_thread();
bool is_in_main_thread();
bool is_in_render_thread();
bool is_glcontext_egl(void* glcontext_ptr);
void gfx_critical_note(const char* msg);
void* get_proc_address_from_glcontext(void* glcontext_ptr, const char* procname);

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

#include "webrender_ffi_generated.h"

#undef WR_FUNC
#undef WR_DESTRUCTOR_SAFE_FUNC
} // extern "C"

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
