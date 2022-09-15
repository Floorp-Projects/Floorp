/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_ENV_H
#define GFX_ENV_H

#include "mozilla/Attributes.h"
#include "nsDebug.h"
#include "prenv.h"

#include <sstream>
#include <string_view>

// To register the check for an environment variable existence (and not empty),
// add a line in this file using the DECL_GFX_ENV macro.
//
// For example this line in the .h:
//   DECL_GFX_ENV(MOZ_GL_SPEW);

// means that you can call e.g.
//   if (gfxEnv::MOZ_GL_SPEW()) { ... }
//   if (gfxEnv::MOZ_GL_SPEW().as_str == "2") { ... }
// and that the value will be checked only once, first time we call it, then
// cached.

struct EnvVal {
  std::string_view as_str;

  static auto From(const char* const raw) {
    auto ret = EnvVal{};

    ret.as_str = std::string_view{};
    // Empty string counts as missing.
    if (raw) {
      ret.as_str = raw;
    }

    return ret;
  }

  MOZ_IMPLICIT operator bool() const {
    return !as_str.empty();  // Warning, this means ENV=0" -> true!
  }
};

class gfxEnv final {
 public:
  gfxEnv() = delete;

  static EnvVal Uncached(const char* name) {
    const auto raw = PR_GetEnv(name);
    const auto ret = EnvVal::From(raw);
    if (ret && ret.as_str == "0") {
      auto msg = std::stringstream{};
      msg << name << "=" << ret.as_str << " -> true!";
      NS_WARNING(msg.str().c_str());
    }
    return ret;
  }

#define DECL_GFX_ENV(Name)                      \
  static const EnvVal& Name() {                 \
    static const auto cached = Uncached(#Name); \
    return cached;                              \
  }

  // This is where DECL_GFX_ENV for each of the environment variables should go.
  // We will keep these in an alphabetical order by the environment variable,
  // to make it easier to see if a method accessing an entry already exists.
  // Just insert yours in the list.

  // OpenGL shader debugging in OGLShaderProgram, in DEBUG only
  DECL_GFX_ENV(MOZ_DEBUG_SHADERS)

  // Disabling the crash guard in DriverCrashGuard
  DECL_GFX_ENV(MOZ_DISABLE_CRASH_GUARD)
  DECL_GFX_ENV(MOZ_FORCE_CRASH_GUARD_NIGHTLY)

  // We force present to work around some Windows bugs - disable that if this
  // environment variable is set.
  DECL_GFX_ENV(MOZ_DISABLE_FORCE_PRESENT)

  // Together with paint dumping, only when MOZ_DUMP_PAINTING is defined.
  // Dumping compositor textures is broken pretty badly. For example,
  // on Linux it crashes TextureHost::GetAsSurface() returns null.
  // Expect to have to fix things like this if you turn it on.
  // Meanwhile, content-side texture dumping
  // (conditioned on DebugDumpPainting()) is a good replacement.
  DECL_GFX_ENV(MOZ_DUMP_COMPOSITOR_TEXTURES)

  // Dump GLBlitHelper shader source text.
  DECL_GFX_ENV(MOZ_DUMP_GLBLITHELPER)

  // Paint dumping, only when MOZ_DUMP_PAINTING is defined.
  DECL_GFX_ENV(MOZ_DUMP_PAINT)
  DECL_GFX_ENV(MOZ_DUMP_PAINT_ITEMS)
  DECL_GFX_ENV(MOZ_DUMP_PAINT_TO_FILE)

  // Force gfxDevCrash to use MOZ_CRASH in Beta and Release
  DECL_GFX_ENV(MOZ_GFX_CRASH_MOZ_CRASH)
  // Force gfxDevCrash to use telemetry in Nightly and Aurora
  DECL_GFX_ENV(MOZ_GFX_CRASH_TELEMETRY)

  // Debugging in GLContext
  DECL_GFX_ENV(MOZ_GL_DEBUG)
  DECL_GFX_ENV(MOZ_GL_DEBUG_VERBOSE)
  DECL_GFX_ENV(MOZ_GL_DEBUG_ABORT_ON_ERROR)
  DECL_GFX_ENV(MOZ_GL_RELEASE_ASSERT_CONTEXT_OWNERSHIP)
  DECL_GFX_ENV(MOZ_EGL_RELEASE_ASSERT_CONTEXT_OWNERSHIP)

  // Count GL extensions
  DECL_GFX_ENV(MOZ_GL_DUMP_EXTS)

  // Very noisy GLContext and GLContextProviderEGL
  DECL_GFX_ENV(MOZ_GL_SPEW)

  // Do extra work before and after each GLX call in GLContextProviderGLX
  DECL_GFX_ENV(MOZ_GLX_DEBUG)

  // GL compositing on Windows
  DECL_GFX_ENV(MOZ_LAYERS_PREFER_EGL)

  // Offscreen GL context for main layer manager
  DECL_GFX_ENV(MOZ_LAYERS_PREFER_OFFSCREEN)

  // WebGL workarounds
  DECL_GFX_ENV(MOZ_WEBGL_WORKAROUND_FIRST_AFFECTS_INSTANCE_ID)

  // WARNING:
  // For readability reasons, please make sure that you've added your new envvar
  // to the list above in alphabetical order.
  // Please do not just append it to the end of the list!

#undef DECL_GFX_ENV
};

#endif /* GFX_ENV_H */
