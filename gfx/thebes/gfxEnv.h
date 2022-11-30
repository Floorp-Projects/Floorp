/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_ENV_H
#define GFX_ENV_H

#include "prenv.h"

// To register the check for an environment variable existence (and not empty),
// add a line in this file using the DECL_GFX_ENV macro.
//
// For example this line in the .h:
//   DECL_GFX_ENV("MOZ_DISABLE_CONTEXT_SHARING_GLX",DisableContextSharingGLX);
// means that you can call
//   bool var = gfxEnv::DisableContextSharingGLX();
// and that the value will be checked only once, first time we call it, then
// cached.

#define DECL_GFX_ENV(Env, Name)        \
  static bool Name() {                 \
    static bool isSet = IsEnvSet(Env); \
    return isSet;                      \
  }

class gfxEnv final {
 public:
  // This is where DECL_GFX_ENV for each of the environment variables should go.
  // We will keep these in an alphabetical order by the environment variable,
  // to make it easier to see if a method accessing an entry already exists.
  // Just insert yours in the list.

  // Debugging inside of ContainerLayerComposite
  DECL_GFX_ENV("DUMP_DEBUG", DumpDebug);

  // OpenGL shader debugging in OGLShaderProgram, in DEBUG only
  DECL_GFX_ENV("MOZ_DEBUG_SHADERS", DebugShaders);

  // Disabling context sharing in GLContextProviderGLX
  DECL_GFX_ENV("MOZ_DISABLE_CONTEXT_SHARING_GLX", DisableContextSharingGlx);

  // Disabling the crash guard in DriverCrashGuard
  DECL_GFX_ENV("MOZ_DISABLE_CRASH_GUARD", DisableCrashGuard);
  DECL_GFX_ENV("MOZ_FORCE_CRASH_GUARD_NIGHTLY", ForceCrashGuardNightly);

  // We force present to work around some Windows bugs - disable that if this
  // environment variable is set.
  DECL_GFX_ENV("MOZ_DISABLE_FORCE_PRESENT", DisableForcePresent);

  // Together with paint dumping, only when MOZ_DUMP_PAINTING is defined.
  // Dumping compositor textures is broken pretty badly. For example,
  // on Linux it crashes TextureHost::GetAsSurface() returns null.
  // Expect to have to fix things like this if you turn it on.
  // Meanwhile, content-side texture dumping
  // (conditioned on DebugDumpPainting()) is a good replacement.
  DECL_GFX_ENV("MOZ_DUMP_COMPOSITOR_TEXTURES", DumpCompositorTextures);

  // Dumping the layer list in LayerSorter
  DECL_GFX_ENV("MOZ_DUMP_LAYER_SORT_LIST", DumpLayerSortList);

  // Paint dumping, only when MOZ_DUMP_PAINTING is defined.
  DECL_GFX_ENV("MOZ_DUMP_PAINT", DumpPaint);
  DECL_GFX_ENV("MOZ_DUMP_PAINT_INTERMEDIATE", DumpPaintIntermediate);
  DECL_GFX_ENV("MOZ_DUMP_PAINT_ITEMS", DumpPaintItems);
  DECL_GFX_ENV("MOZ_DUMP_PAINT_TO_FILE", DumpPaintToFile);

  // Force double buffering in ContentClient
  DECL_GFX_ENV("MOZ_FORCE_DOUBLE_BUFFERING", ForceDoubleBuffering);

  // Force gfxDevCrash to use MOZ_CRASH in Beta and Release
  DECL_GFX_ENV("MOZ_GFX_CRASH_MOZ_CRASH", GfxDevCrashMozCrash);
  // Force gfxDevCrash to use telemetry in Nightly and Aurora
  DECL_GFX_ENV("MOZ_GFX_CRASH_TELEMETRY", GfxDevCrashTelemetry);

  DECL_GFX_ENV("MOZ_GFX_VR_NO_DISTORTION", VRNoDistortion);

  // Debugging in GLContext
  DECL_GFX_ENV("MOZ_GL_DEBUG", GlDebug);
  DECL_GFX_ENV("MOZ_GL_DEBUG_VERBOSE", GlDebugVerbose);
  DECL_GFX_ENV("MOZ_GL_DEBUG_ABORT_ON_ERROR", GlDebugAbortOnError);

  // Count GL extensions
  DECL_GFX_ENV("MOZ_GL_DUMP_EXTS", GlDumpExtensions);

  // Very noisy GLContext and GLContextProviderEGL
  DECL_GFX_ENV("MOZ_GL_SPEW", GlSpew);

  //
  DECL_GFX_ENV("MOZ_GPU_SWITCHING_SPEW", GpuSwitchingSpew);

  // Do extra work before and after each GLX call in GLContextProviderGLX
  DECL_GFX_ENV("MOZ_GLX_DEBUG", GlxDebug);

  // Use X compositing
  DECL_GFX_ENV("MOZ_LAYERS_ENABLE_XLIB_SURFACES", LayersEnableXlibSurfaces);

  // GL compositing on Windows
  DECL_GFX_ENV("MOZ_LAYERS_PREFER_EGL", LayersPreferEGL);

  // Offscreen GL context for main layer manager
  DECL_GFX_ENV("MOZ_LAYERS_PREFER_OFFSCREEN", LayersPreferOffscreen);

  // Skip final window composition
  DECL_GFX_ENV("MOZ_SKIPCOMPOSITION", SkipComposition);

  // Skip rasterizing painted layer contents
  DECL_GFX_ENV("MOZ_SKIPRASTERIZATION", SkipRasterization);

  // Stop the VR rendering
  DECL_GFX_ENV("NO_VR_RENDERING", NoVRRendering);

  // WebGL workarounds
  DECL_GFX_ENV("MOZ_WEBGL_WORKAROUND_FIRST_AFFECTS_INSTANCE_ID", MOZ_WEBGL_WORKAROUND_FIRST_AFFECTS_INSTANCE_ID)

  // WARNING:
  // Please make sure that you've added your new envvar to the list above in
  // alphabetical order. Please do not just append it to the end of the list.

 private:
  // Helper function, can be re-used in the other macros
  static bool IsEnvSet(const char* aName) {
    const char* val = PR_GetEnv(aName);
    return (val != 0 && *val != '\0');
  }

  gfxEnv() = default;
  ~gfxEnv() = default;

  gfxEnv(const gfxEnv&) = delete;
  gfxEnv& operator=(const gfxEnv&) = delete;
};

#undef DECL_GFX_ENV

#endif /* GFX_ENV_H */
