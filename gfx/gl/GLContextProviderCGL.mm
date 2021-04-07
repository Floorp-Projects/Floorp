/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GLContextProvider.h"
#include "GLContextCGL.h"
#include "GLLibraryLoader.h"
#include "nsDebug.h"
#include "nsIWidget.h"
#include <OpenGL/gl.h>
#include "gfxFailure.h"
#include "mozilla/IntegerRange.h"
#include "mozilla/StaticPrefs_gfx.h"
#include "mozilla/StaticPrefs_gl.h"
#include "mozilla/StaticPrefs_layout.h"
#include "prenv.h"
#include "prlink.h"
#include "mozilla/ProfilerLabels.h"
#include "MozFramebuffer.h"
#include "mozilla/layers/CompositorOptions.h"
#include "mozilla/widget/CompositorWidget.h"
#include "ScopedGLHelpers.h"

#include <OpenGL/OpenGL.h>

namespace mozilla {
namespace gl {

using namespace mozilla::gfx;
using namespace mozilla::widget;

class CGLLibrary {
 public:
  bool EnsureInitialized() {
    if (mInitialized) {
      return true;
    }
    if (!mOGLLibrary) {
      mOGLLibrary = PR_LoadLibrary("/System/Library/Frameworks/OpenGL.framework/OpenGL");
      if (!mOGLLibrary) {
        NS_WARNING("Couldn't load OpenGL Framework.");
        return false;
      }
    }

    mInitialized = true;
    return true;
  }

  const auto& Library() const { return mOGLLibrary; }

 private:
  bool mInitialized = false;
  PRLibrary* mOGLLibrary = nullptr;
};

CGLLibrary sCGLLibrary;

GLContextCGL::GLContextCGL(const GLContextDesc& desc, NSOpenGLContext* context)
    : GLContext(desc), mContext(context) {
  CGDisplayRegisterReconfigurationCallback(DisplayReconfigurationCallback, this);
}

GLContextCGL::~GLContextCGL() {
  MarkDestroyed();

  CGDisplayRemoveReconfigurationCallback(DisplayReconfigurationCallback, this);

  if (mContext) {
    if ([NSOpenGLContext currentContext] == mContext) {
      // Clear the current context before releasing. If we don't do
      // this, the next time we call [NSOpenGLContext currentContext],
      // "invalid context" will be printed to the console.
      [NSOpenGLContext clearCurrentContext];
    }
    [mContext release];
  }
}

CGLContextObj GLContextCGL::GetCGLContext() const {
  return static_cast<CGLContextObj>([mContext CGLContextObj]);
}

bool GLContextCGL::MakeCurrentImpl() const {
  if (mContext) {
    [mContext makeCurrentContext];
    MOZ_ASSERT(IsCurrentImpl());
    // Use non-blocking swap in "ASAP mode".
    // ASAP mode means that rendering is iterated as fast as possible.
    // ASAP mode is entered when layout.frame_rate=0 (requires restart).
    // If swapInt is 1, then glSwapBuffers will block and wait for a vblank signal.
    // When we're iterating as fast as possible, however, we want a non-blocking
    // glSwapBuffers, which will happen when swapInt==0.
    GLint swapInt = StaticPrefs::layout_frame_rate() == 0 ? 0 : 1;
    [mContext setValues:&swapInt forParameter:NSOpenGLCPSwapInterval];
  }
  return true;
}

bool GLContextCGL::IsCurrentImpl() const { return [NSOpenGLContext currentContext] == mContext; }

/* static */ void GLContextCGL::DisplayReconfigurationCallback(CGDirectDisplayID aDisplay,
                                                               CGDisplayChangeSummaryFlags aFlags,
                                                               void* aUserInfo) {
  if (aFlags & kCGDisplaySetModeFlag) {
    static_cast<GLContextCGL*>(aUserInfo)->mActiveGPUSwitchMayHaveOccurred = true;
  }
}

static NSOpenGLContext* CreateWithFormat(const NSOpenGLPixelFormatAttribute* attribs) {
  NSOpenGLPixelFormat* format = [[NSOpenGLPixelFormat alloc] initWithAttributes:attribs];
  if (!format) {
    NS_WARNING("Failed to create NSOpenGLPixelFormat.");
    return nullptr;
  }

  NSOpenGLContext* context = [[NSOpenGLContext alloc] initWithFormat:format shareContext:nullptr];

  [format release];

  return context;
}

// Get the "OpenGL display mask" for a fresh context. The return value of this
// function depends on the time at which this function is called.
// In practice, on a Macbook Pro with an integrated and a discrete GPU, this function returns the
// display mask for the GPU that currently drives the internal display.
//
// Quick reference of the concepts involved in the code below:
//   GPU switch: On Mac devices with an integrated and a discrete GPU, a GPU switch changes which
//     GPU drives the internal display. Both GPUs are still usable at all times. (When the
//     integrated GPU is driving the internal display, using the discrete GPU can incur a longer
//     warm-up cost.)
//   Virtual screen: A CGL concept. A "virtual screen" corresponds to a GL renderer. There's one
//     for the integrated GPU, one for each discrete GPU, and one for the Apple software renderer.
//     The list of virtual screens is per-NSOpenGLPixelFormat; it is filtered down to only the
//     renderers that support the requirements from the pixel format attributes. Indexes into this
//     list (such as currentVirtualScreen) cannot be used interchangably across different
//     NSOpenGLPixelFormat instances.
//   Display mask: A bitset per GL renderer. Different renderers have disjoint display masks. The
//     Apple software renderer has all bits zeroed. For each CGDirectDisplayID,
//     CGDisplayIDToOpenGLDisplayMask(displayID) returns a single bit in the display mask.
//   CGDirectDisplayID: An ID for each (physical screen, GPU which can drive this screen) pair. The
//     current CGDirectDisplayID for an NSScreen object can be obtained using [[[screen
//     deviceDescription] objectForKey:@"NSScreenNumber"] unsignedIntValue]; it changes depending on
//     which GPU is currently driving the screen.
static CGOpenGLDisplayMask GetFreshContextDisplayMask() {
  NSOpenGLPixelFormatAttribute attribs[] = {NSOpenGLPFAAllowOfflineRenderers, 0};
  NSOpenGLPixelFormat* pixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:attribs];
  MOZ_RELEASE_ASSERT(pixelFormat);
  NSOpenGLContext* context = [[NSOpenGLContext alloc] initWithFormat:pixelFormat
                                                        shareContext:nullptr];
  GLint displayMask = 0;
  [pixelFormat getValues:&displayMask
            forAttribute:NSOpenGLPFAScreenMask
        forVirtualScreen:[context currentVirtualScreen]];
  [pixelFormat release];
  [context release];
  return static_cast<CGOpenGLDisplayMask>(displayMask);
}

static bool IsSameGPU(CGOpenGLDisplayMask mask1, CGOpenGLDisplayMask mask2) {
  if ((mask1 & mask2) != 0) {
    return true;
  }
  // Both masks can be zero, when using the Apple software renderer.
  return !mask1 && !mask2;
}

void GLContextCGL::MigrateToActiveGPU() {
  if (!mActiveGPUSwitchMayHaveOccurred.compareExchange(true, false)) {
    return;
  }

  CGOpenGLDisplayMask newPreferredDisplayMask = GetFreshContextDisplayMask();
  NSOpenGLPixelFormat* pixelFormat = [mContext pixelFormat];
  GLint currentVirtualScreen = [mContext currentVirtualScreen];
  GLint currentDisplayMask = 0;
  [pixelFormat getValues:&currentDisplayMask
            forAttribute:NSOpenGLPFAScreenMask
        forVirtualScreen:currentVirtualScreen];
  if (IsSameGPU(currentDisplayMask, newPreferredDisplayMask)) {
    // No "virtual screen" change needed.
    return;
  }

  // Find the "virtual screen" with a display mask that matches newPreferredDisplayMask, if
  // available, and switch the context over to it.
  // This code was inspired by equivalent functionality in -[NSOpenGLContext update] which only
  // kicks in for contexts that present via a CAOpenGLLayer.
  for (const auto i : IntegerRange([pixelFormat numberOfVirtualScreens])) {
    GLint displayMask = 0;
    [pixelFormat getValues:&displayMask forAttribute:NSOpenGLPFAScreenMask forVirtualScreen:i];
    if (IsSameGPU(displayMask, newPreferredDisplayMask)) {
      CGLSetVirtualScreen([mContext CGLContextObj], i);
      return;
    }
  }
}

GLenum GLContextCGL::GetPreferredARGB32Format() const { return LOCAL_GL_BGRA; }

bool GLContextCGL::SwapBuffers() {
  AUTO_PROFILER_LABEL("GLContextCGL::SwapBuffers", GRAPHICS);

  // We do not have a default framebuffer. Just do a flush.
  // Flushing is necessary if we want our IOSurfaces to have the correct
  // content once they're picked up by the WindowServer from our CALayers.
  fFlush();

  return true;
}

void GLContextCGL::GetWSIInfo(nsCString* const out) const { out->AppendLiteral("CGL"); }

Maybe<SymbolLoader> GLContextCGL::GetSymbolLoader() const {
  const auto& lib = sCGLLibrary.Library();
  return Some(SymbolLoader(*lib));
}

already_AddRefed<GLContext> GLContextProviderCGL::CreateForCompositorWidget(
    CompositorWidget* aCompositorWidget, bool aHardwareWebRender, bool aForceAccelerated) {
  CreateContextFlags flags = CreateContextFlags::ALLOW_OFFLINE_RENDERER;
  if (aForceAccelerated) {
    flags |= CreateContextFlags::FORCE_ENABLE_HARDWARE;
  }
  if (!aHardwareWebRender) {
    flags |= CreateContextFlags::REQUIRE_COMPAT_PROFILE;
  }
  nsCString failureUnused;
  return CreateHeadless({flags}, &failureUnused);
}

static RefPtr<GLContextCGL> CreateOffscreenFBOContext(GLContextCreateDesc desc) {
  if (!sCGLLibrary.EnsureInitialized()) {
    return nullptr;
  }

  NSOpenGLContext* context = nullptr;

  std::vector<NSOpenGLPixelFormatAttribute> attribs;
  auto& flags = desc.flags;

  if (!StaticPrefs::gl_allow_high_power()) {
    flags &= ~CreateContextFlags::HIGH_POWER;
  }
  if (flags & CreateContextFlags::ALLOW_OFFLINE_RENDERER ||
      !(flags & CreateContextFlags::HIGH_POWER)) {
    // This is really poorly named on Apple's part, but "AllowOfflineRenderers" means
    // that we want to allow running on the iGPU instead of requiring the dGPU.
    attribs.push_back(NSOpenGLPFAAllowOfflineRenderers);
  }

  if (flags & CreateContextFlags::FORCE_ENABLE_HARDWARE) {
    attribs.push_back(NSOpenGLPFAAccelerated);
  }

  if (!(flags & CreateContextFlags::REQUIRE_COMPAT_PROFILE)) {
    auto coreAttribs = attribs;
    coreAttribs.push_back(NSOpenGLPFAOpenGLProfile);
    coreAttribs.push_back(NSOpenGLProfileVersion3_2Core);
    coreAttribs.push_back(0);
    context = CreateWithFormat(coreAttribs.data());
  }

  if (!context) {
    attribs.push_back(0);
    context = CreateWithFormat(attribs.data());
  }

  if (!context) {
    NS_WARNING("Failed to create NSOpenGLContext.");
    return nullptr;
  }

  RefPtr<GLContextCGL> glContext = new GLContextCGL({desc, true}, context);

  if (flags & CreateContextFlags::PREFER_MULTITHREADED) {
    CGLEnable(glContext->GetCGLContext(), kCGLCEMPEngine);
  }
  return glContext;
}

already_AddRefed<GLContext> GLContextProviderCGL::CreateHeadless(const GLContextCreateDesc& desc,
                                                                 nsACString* const out_failureId) {
  auto gl = CreateOffscreenFBOContext(desc);
  if (!gl) {
    *out_failureId = "FEATURE_FAILURE_CGL_FBO"_ns;
    return nullptr;
  }

  if (!gl->Init()) {
    *out_failureId = "FEATURE_FAILURE_CGL_INIT"_ns;
    NS_WARNING("Failed during Init.");
    return nullptr;
  }

  return gl.forget();
}

static RefPtr<GLContext> gGlobalContext;

GLContext* GLContextProviderCGL::GetGlobalContext() {
  static bool triedToCreateContext = false;
  if (!triedToCreateContext) {
    triedToCreateContext = true;

    MOZ_RELEASE_ASSERT(!gGlobalContext);
    nsCString discardFailureId;
    RefPtr<GLContext> temp = CreateHeadless({}, &discardFailureId);
    gGlobalContext = temp;

    if (!gGlobalContext) {
      NS_WARNING("Couldn't init gGlobalContext.");
    }
  }

  return gGlobalContext;
}

void GLContextProviderCGL::Shutdown() { gGlobalContext = nullptr; }

} /* namespace gl */
} /* namespace mozilla */
