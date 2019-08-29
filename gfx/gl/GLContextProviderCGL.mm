/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GLContextProvider.h"
#include "GLContextCGL.h"
#include "nsDebug.h"
#include "nsIWidget.h"
#include <OpenGL/gl.h>
#include "gfxFailure.h"
#include "mozilla/StaticPrefs_gfx.h"
#include "mozilla/StaticPrefs_gl.h"
#include "mozilla/StaticPrefs_layout.h"
#include "prenv.h"
#include "GeckoProfiler.h"
#include "MozFramebuffer.h"
#include "mozilla/gfx/MacIOSurface.h"
#include "mozilla/layers/CompositorOptions.h"
#include "mozilla/widget/CompositorWidget.h"
#include "ScopedGLHelpers.h"

#include <OpenGL/OpenGL.h>

// When running inside a VM, creating an accelerated OpenGL context usually
// fails. Uncomment this line to emulate that behavior.
// #define EMULATE_VM

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

    const char* db = PR_GetEnv("MOZ_CGL_DB");
    if (db) {
      mUseDoubleBufferedWindows = *db != '0';
    }

    mInitialized = true;
    return true;
  }

  bool UseDoubleBufferedWindows() const {
    MOZ_ASSERT(mInitialized);
    return mUseDoubleBufferedWindows;
  }

  const auto& Library() const { return mOGLLibrary; }

 private:
  bool mInitialized = false;
  bool mUseDoubleBufferedWindows = true;
  PRLibrary* mOGLLibrary = nullptr;
};

CGLLibrary sCGLLibrary;

GLContextCGL::GLContextCGL(CreateContextFlags flags, const SurfaceCaps& caps,
                           NSOpenGLContext* context, bool isOffscreen)
    : GLContext(flags, caps, nullptr, isOffscreen), mContext(context) {}

GLContextCGL::~GLContextCGL() {
  MarkDestroyed();

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

void GLContextCGL::OnMarkDestroyed() {
  mRegisteredIOSurfaceFramebuffers.clear();

  mDefaultFramebuffer = 0;
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

GLenum GLContextCGL::GetPreferredARGB32Format() const { return LOCAL_GL_BGRA; }

bool GLContextCGL::IsDoubleBuffered() const { return sCGLLibrary.UseDoubleBufferedWindows(); }

bool GLContextCGL::SwapBuffers() {
  AUTO_PROFILER_LABEL("GLContextCGL::SwapBuffers", GRAPHICS);

  if (StaticPrefs::gfx_core_animation_enabled_AtStartup()) {
    // We do not have a framebuffer zero. Just do a flush.
    // Flushing is necessary if we want our IOSurfaces to have the correct
    // content once they're picked up by the WindowServer from our CALayers.
    fFlush();
  } else {
    [mContext flushBuffer];
  }

  return true;
}

void GLContextCGL::GetWSIInfo(nsCString* const out) const { out->AppendLiteral("CGL"); }

Maybe<SymbolLoader> GLContextCGL::GetSymbolLoader() const {
  const auto& lib = sCGLLibrary.Library();
  return Some(SymbolLoader(*lib));
}

GLuint GLContextCGL::GetDefaultFramebuffer() { return mDefaultFramebuffer; }

void GLContextCGL::UseRegisteredIOSurfaceForDefaultFramebuffer(IOSurfaceRef aSurface) {
  MakeCurrent();

  auto fb = mRegisteredIOSurfaceFramebuffers.find(aSurface);
  MOZ_RELEASE_ASSERT(fb != mRegisteredIOSurfaceFramebuffers.end(),
                     "IOSurface has not been registered with this GLContext");

  mDefaultFramebuffer = fb->second->mFB;
}

void GLContextCGL::RegisterIOSurface(IOSurfaceRef aSurface) {
  MOZ_RELEASE_ASSERT(
      mRegisteredIOSurfaceFramebuffers.find(aSurface) == mRegisteredIOSurfaceFramebuffers.end(),
      "double-registering IOSurface");

  uint32_t width = IOSurfaceGetWidth(aSurface);
  uint32_t height = IOSurfaceGetHeight(aSurface);

  MakeCurrent();
  GLuint tex = CreateTexture();

  {
    const ScopedBindTexture bindTex(this, tex, LOCAL_GL_TEXTURE_RECTANGLE_ARB);
    CGLTexImageIOSurface2D([mContext CGLContextObj], LOCAL_GL_TEXTURE_RECTANGLE_ARB, LOCAL_GL_RGBA,
                           width, height, LOCAL_GL_BGRA, LOCAL_GL_UNSIGNED_INT_8_8_8_8_REV,
                           aSurface, 0);
  }

  auto fb = MozFramebuffer::CreateWith(this, IntSize(width, height), 0, mCaps.depth,
                                       LOCAL_GL_TEXTURE_RECTANGLE_ARB, tex);
  mRegisteredIOSurfaceFramebuffers.insert({aSurface, std::move(fb)});
}

void GLContextCGL::UnregisterIOSurface(IOSurfaceRef aSurface) {
  size_t removeCount = mRegisteredIOSurfaceFramebuffers.erase(aSurface);
  MOZ_RELEASE_ASSERT(removeCount == 1, "Unregistering IOSurface that's not registered");
}

already_AddRefed<GLContext> GLContextProviderCGL::CreateWrappingExisting(void*, void*) {
  return nullptr;
}

static const NSOpenGLPixelFormatAttribute kAttribs_singleBuffered[] = {
    NSOpenGLPFAAllowOfflineRenderers, 0};

static const NSOpenGLPixelFormatAttribute kAttribs_singleBuffered_accel[] = {
    NSOpenGLPFAAccelerated, NSOpenGLPFAAllowOfflineRenderers, 0};

static const NSOpenGLPixelFormatAttribute kAttribs_doubleBuffered[] = {
    NSOpenGLPFAAllowOfflineRenderers, NSOpenGLPFADoubleBuffer, 0};

static const NSOpenGLPixelFormatAttribute kAttribs_doubleBuffered_accel[] = {
    NSOpenGLPFAAccelerated, NSOpenGLPFAAllowOfflineRenderers, NSOpenGLPFADoubleBuffer, 0};

static const NSOpenGLPixelFormatAttribute kAttribs_doubleBuffered_accel_webrender[] = {
    NSOpenGLPFAAccelerated,
    NSOpenGLPFAAllowOfflineRenderers,
    NSOpenGLPFADoubleBuffer,
    NSOpenGLPFAOpenGLProfile,
    NSOpenGLProfileVersion3_2Core,
    NSOpenGLPFADepthSize,
    24,
    0};

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

already_AddRefed<GLContext> GLContextProviderCGL::CreateForCompositorWidget(
    CompositorWidget* aCompositorWidget, bool aWebRender, bool aForceAccelerated) {
  if (!aCompositorWidget) {
    MOZ_ASSERT(false);
    return nullptr;
  }
  return CreateForWindow(aCompositorWidget->RealWidget(), aWebRender, aForceAccelerated);
}

already_AddRefed<GLContext> GLContextProviderCGL::CreateForWindow(nsIWidget* aWidget,
                                                                  bool aWebRender,
                                                                  bool aForceAccelerated) {
  if (!sCGLLibrary.EnsureInitialized()) {
    return nullptr;
  }

#ifdef EMULATE_VM
  if (aForceAccelerated) {
    return nullptr;
  }
#endif

  const NSOpenGLPixelFormatAttribute* attribs;
  SurfaceCaps caps = SurfaceCaps::ForRGBA();
  if (sCGLLibrary.UseDoubleBufferedWindows()) {
    if (aWebRender) {
      MOZ_RELEASE_ASSERT(aForceAccelerated,
                         "At the moment, aForceAccelerated is always true if aWebRender is true. "
                         "If this changes, please update the code here.");
      attribs = kAttribs_doubleBuffered_accel_webrender;
      caps.depth = true;
    } else {
      attribs = aForceAccelerated ? kAttribs_doubleBuffered_accel : kAttribs_doubleBuffered;
    }
  } else {
    attribs = aForceAccelerated ? kAttribs_singleBuffered_accel : kAttribs_singleBuffered;
  }
  NSOpenGLContext* context = CreateWithFormat(attribs);
  if (!context) {
    return nullptr;
  }

  GLint opaque = StaticPrefs::gfx_compositor_glcontext_opaque();
  [context setValues:&opaque forParameter:NSOpenGLCPSurfaceOpacity];

  RefPtr<GLContextCGL> glContext = new GLContextCGL(CreateContextFlags::NONE, caps, context, false);

  if (!glContext->Init()) {
    glContext = nullptr;
    [context release];
    return nullptr;
  }

  return glContext.forget();
}

static already_AddRefed<GLContextCGL> CreateOffscreenFBOContext(CreateContextFlags flags) {
  if (!sCGLLibrary.EnsureInitialized()) {
    return nullptr;
  }

  NSOpenGLContext* context = nullptr;

  std::vector<NSOpenGLPixelFormatAttribute> attribs;

  if (!StaticPrefs::gl_allow_high_power()) {
    flags &= ~CreateContextFlags::HIGH_POWER;
  }
  if (flags & CreateContextFlags::ALLOW_OFFLINE_RENDERER ||
      !(flags & CreateContextFlags::HIGH_POWER)) {
    // This is really poorly named on Apple's part, but "AllowOfflineRenderers" means
    // that we want to allow running on the iGPU instead of requiring the dGPU.
    attribs.push_back(NSOpenGLPFAAllowOfflineRenderers);
  }

  if (StaticPrefs::gl_require_hardware()) {
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

  RefPtr<GLContextCGL> glContext = new GLContextCGL(flags, SurfaceCaps::Any(), context, true);

  if (StaticPrefs::gl_multithreaded()) {
    CGLEnable(glContext->GetCGLContext(), kCGLCEMPEngine);
  }
  return glContext.forget();
}

already_AddRefed<GLContext> GLContextProviderCGL::CreateHeadless(CreateContextFlags flags,
                                                                 nsACString* const out_failureId) {
  RefPtr<GLContextCGL> gl;
  gl = CreateOffscreenFBOContext(flags);
  if (!gl) {
    *out_failureId = NS_LITERAL_CSTRING("FEATURE_FAILURE_CGL_FBO");
    return nullptr;
  }

  if (!gl->Init()) {
    *out_failureId = NS_LITERAL_CSTRING("FEATURE_FAILURE_CGL_INIT");
    NS_WARNING("Failed during Init.");
    return nullptr;
  }

  return gl.forget();
}

already_AddRefed<GLContext> GLContextProviderCGL::CreateOffscreen(const IntSize& size,
                                                                  const SurfaceCaps& minCaps,
                                                                  CreateContextFlags flags,
                                                                  nsACString* const out_failureId) {
  RefPtr<GLContext> gl = CreateHeadless(flags, out_failureId);
  if (!gl) {
    return nullptr;
  }

  if (!gl->InitOffscreen(size, minCaps)) {
    *out_failureId = NS_LITERAL_CSTRING("FEATURE_FAILURE_CGL_INIT");
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
    RefPtr<GLContext> temp = CreateHeadless(CreateContextFlags::NONE, &discardFailureId);
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
