/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if defined(MOZ_WIDGET_GTK)
#  define GET_NATIVE_WINDOW_FROM_REAL_WIDGET(aWidget) \
    ((EGLNativeWindowType)aWidget->GetNativeData(NS_NATIVE_EGL_WINDOW))
#  define GET_NATIVE_WINDOW_FROM_COMPOSITOR_WIDGET(aWidget) \
    (aWidget->AsX11()->GetEGLNativeWindow())
#elif defined(MOZ_WIDGET_ANDROID)
#  define GET_NATIVE_WINDOW_FROM_REAL_WIDGET(aWidget) \
    ((EGLNativeWindowType)aWidget->GetNativeData(NS_JAVA_SURFACE))
#  define GET_NATIVE_WINDOW_FROM_COMPOSITOR_WIDGET(aWidget) \
    (aWidget->AsAndroid()->GetEGLNativeWindow())
#elif defined(XP_WIN)
#  define GET_NATIVE_WINDOW_FROM_REAL_WIDGET(aWidget) \
    ((EGLNativeWindowType)aWidget->GetNativeData(NS_NATIVE_WINDOW))
#  define GET_NATIVE_WINDOW_FROM_COMPOSITOR_WIDGET(aWidget) \
    ((EGLNativeWindowType)aWidget->AsWindows()->GetHwnd())
#else
#  define GET_NATIVE_WINDOW_FROM_REAL_WIDGET(aWidget) \
    ((EGLNativeWindowType)aWidget->GetNativeData(NS_NATIVE_WINDOW))
#  define GET_NATIVE_WINDOW_FROM_COMPOSITOR_WIDGET(aWidget)     \
    ((EGLNativeWindowType)aWidget->RealWidget()->GetNativeData( \
        NS_NATIVE_WINDOW))
#endif

#if defined(XP_UNIX)
#  ifdef MOZ_WIDGET_ANDROID
#    include <android/native_window.h>
#    include <android/native_window_jni.h>
#    include "mozilla/widget/AndroidCompositorWidget.h"
#  endif

#  define GLES2_LIB "libGLESv2.so"
#  define GLES2_LIB2 "libGLESv2.so.2"

#elif defined(XP_WIN)
#  include "mozilla/widget/WinCompositorWidget.h"
#  include "nsIFile.h"

#  define GLES2_LIB "libGLESv2.dll"

#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN 1
#  endif

#  include <windows.h>
#else
#  error "Platform not recognized"
#endif

#include "gfxASurface.h"
#include "gfxCrashReporterUtils.h"
#include "gfxFailure.h"
#include "gfxPlatform.h"
#include "gfxUtils.h"
#include "GLBlitHelper.h"
#include "GLContextEGL.h"
#include "GLContextProvider.h"
#include "GLLibraryEGL.h"
#include "GLLibraryLoader.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPrefs_gfx.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/gfx/BuildConstants.h"
#include "mozilla/gfx/Logging.h"
#include "mozilla/layers/CompositorOptions.h"
#include "mozilla/widget/CompositorWidget.h"
#include "nsDebug.h"
#include "nsIWidget.h"
#include "nsThreadUtils.h"
#include "ScopedGLHelpers.h"
#include "TextureImageEGL.h"

#if defined(MOZ_WIDGET_GTK)
#  include "mozilla/widget/GtkCompositorWidget.h"
#  if defined(MOZ_WAYLAND)
#    include <dlfcn.h>
#    include <gdk/gdkwayland.h>
#    include <wayland-egl.h>
#    define MOZ_GTK_WAYLAND 1
#  endif
#endif

inline bool IsWaylandDisplay() {
#ifdef MOZ_GTK_WAYLAND
  return gdk_display_get_default() &&
         GDK_IS_WAYLAND_DISPLAY(gdk_display_get_default());
#else
  return false;
#endif
}

inline bool IsX11Display() {
#ifdef MOZ_WIDGET_GTK
  return gdk_display_get_default() &&
         GDK_IS_X11_DISPLAY(gdk_display_get_default());
#else
  return false;
#endif
}

struct wl_egl_window;

using namespace mozilla::gfx;

namespace mozilla {
namespace gl {

using namespace mozilla::widget;

#if defined(MOZ_WAYLAND)
class WaylandGLSurface {
 public:
  WaylandGLSurface(struct wl_surface* aWaylandSurface,
                   struct wl_egl_window* aEGLWindow);
  ~WaylandGLSurface();

 private:
  struct wl_surface* mWaylandSurface;
  struct wl_egl_window* mEGLWindow;
};

static nsTHashMap<nsPtrHashKey<void>, WaylandGLSurface*> sWaylandGLSurface;

void DeleteWaylandGLSurface(EGLSurface surface) {
  // We're running on Wayland which means our EGLSurface may
  // have attached Wayland backend data which must be released.
  if (IsWaylandDisplay()) {
    auto entry = sWaylandGLSurface.Lookup(surface);
    if (entry) {
      delete entry.Data();
      entry.Remove();
    }
  }
}
#endif

static bool CreateConfigScreen(EglDisplay&, EGLConfig* const aConfig,
                               const bool aEnableDepthBuffer,
                               const bool aUseGles, int aVisual = 0);

// append three zeros at the end of attribs list to work around
// EGL implementation bugs that iterate until they find 0, instead of
// EGL_NONE. See bug 948406.
#define EGL_ATTRIBS_LIST_SAFE_TERMINATION_WORKING_AROUND_BUGS \
  LOCAL_EGL_NONE, 0, 0, 0

static EGLint kTerminationAttribs[] = {
    EGL_ATTRIBS_LIST_SAFE_TERMINATION_WORKING_AROUND_BUGS};

static int next_power_of_two(int v) {
  v--;
  v |= v >> 1;
  v |= v >> 2;
  v |= v >> 4;
  v |= v >> 8;
  v |= v >> 16;
  v++;

  return v;
}

static bool is_power_of_two(int v) {
  NS_ASSERTION(v >= 0, "bad value");

  if (v == 0) return true;

  return (v & (v - 1)) == 0;
}

static void DestroySurface(EglDisplay& egl, const EGLSurface oldSurface) {
  if (oldSurface != EGL_NO_SURFACE) {
    // TODO: This breaks TLS MakeCurrent caching.
    egl.fMakeCurrent(EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    egl.fDestroySurface(oldSurface);
#if defined(MOZ_WAYLAND)
    DeleteWaylandGLSurface(oldSurface);
#endif
  }
}

static EGLSurface CreateFallbackSurface(EglDisplay& egl,
                                        const EGLConfig& config) {
  if (egl.IsExtensionSupported(EGLExtension::KHR_surfaceless_context)) {
    // We don't need a PBuffer surface in this case
    return EGL_NO_SURFACE;
  }

  std::vector<EGLint> pbattrs;
  pbattrs.push_back(LOCAL_EGL_WIDTH);
  pbattrs.push_back(1);
  pbattrs.push_back(LOCAL_EGL_HEIGHT);
  pbattrs.push_back(1);

  for (const auto& cur : kTerminationAttribs) {
    pbattrs.push_back(cur);
  }

  EGLSurface surface = egl.fCreatePbufferSurface(config, pbattrs.data());
  if (!surface) {
    MOZ_CRASH("Failed to create fallback EGLSurface");
  }

  return surface;
}

static EGLSurface CreateSurfaceFromNativeWindow(
    EglDisplay& egl, const EGLNativeWindowType window, const EGLConfig config) {
  MOZ_ASSERT(window);
  EGLSurface newSurface = EGL_NO_SURFACE;

#ifdef MOZ_WIDGET_ANDROID
  JNIEnv* const env = jni::GetEnvForThread();
  ANativeWindow* const nativeWindow =
      ANativeWindow_fromSurface(env, reinterpret_cast<jobject>(window));
  if (!nativeWindow) {
    return EGL_NO_SURFACE;
  }
  const auto& display = egl.mLib->fGetDisplay(EGL_DEFAULT_DISPLAY);
  newSurface = egl.mLib->fCreateWindowSurface(display, config, nativeWindow, 0);
  ANativeWindow_release(nativeWindow);
#else
  newSurface = egl.fCreateWindowSurface(config, window, 0);
  if (!newSurface) {
    const auto err = egl.mLib->fGetError();
    gfxCriticalNote << "Failed to create EGLSurface!: " << gfx::hexa(err);
  }
#endif
  return newSurface;
}

/* GLContextEGLFactory class was added as a friend of GLContextEGL
 * so that it could access  GLContextEGL::CreateGLContext. This was
 * done so that a new function would not need to be added to the shared
 * GLContextProvider interface.
 */
class GLContextEGLFactory {
 public:
  static already_AddRefed<GLContext> Create(EGLNativeWindowType aWindow,
                                            bool aWebRender, int32_t aDepth);
  static already_AddRefed<GLContext> CreateImpl(EGLNativeWindowType aWindow,
                                                bool aWebRender, bool aUseGles,
                                                int32_t aDepth);

 private:
  GLContextEGLFactory() = default;
  ~GLContextEGLFactory() = default;
};

already_AddRefed<GLContext> GLContextEGLFactory::CreateImpl(
    EGLNativeWindowType aWindow, bool aWebRender, bool aUseGles,
    int32_t aDepth) {
  nsCString failureId;
  const auto lib = gl::DefaultEglLibrary(&failureId);
  if (!lib) {
    gfxCriticalNote << "Failed[3] to load EGL library: " << failureId.get();
    return nullptr;
  }
  const auto egl = lib->CreateDisplay(true, &failureId);
  if (!egl) {
    gfxCriticalNote << "Failed[3] to create EGL library  display: "
                    << failureId.get();
    return nullptr;
  }

  int visualID = 0;
  if (IsX11Display()) {
#ifdef MOZ_X11
    GdkDisplay* gdkDisplay = gdk_display_get_default();
    auto display = gdkDisplay ? GDK_DISPLAY_XDISPLAY(gdkDisplay) : nullptr;
    if (display) {
      XWindowAttributes windowAttrs;
      if (!XGetWindowAttributes(display, (Window)aWindow, &windowAttrs)) {
        NS_WARNING("[EGL] XGetWindowAttributes() failed");
        return nullptr;
      }
      visualID = XVisualIDFromVisual(windowAttrs.visual);
    }
#endif
  }

  bool doubleBuffered = true;

  EGLConfig config;
  if (aWebRender && egl->mLib->IsANGLE()) {
    // Force enable alpha channel to make sure ANGLE use correct framebuffer
    // formart
    const int bpp = 32;
    const bool withDepth = true;
    if (!CreateConfig(*egl, &config, bpp, withDepth, aUseGles)) {
      gfxCriticalNote << "Failed to create EGLConfig for WebRender ANGLE!";
      return nullptr;
    }
  } else {
    if (aDepth) {
      if (!CreateConfig(*egl, &config, aDepth, aWebRender, aUseGles,
                        visualID)) {
        gfxCriticalNote
            << "Failed to create EGLConfig for WebRender with depth!";
        return nullptr;
      }
    } else {
      if (!CreateConfigScreen(*egl, &config,
                              /* aEnableDepthBuffer */ aWebRender, aUseGles,
                              visualID)) {
        gfxCriticalNote << "Failed to create EGLConfig!";
        return nullptr;
      }
    }
  }

  EGLSurface surface = EGL_NO_SURFACE;
  if (aWindow) {
    surface = mozilla::gl::CreateSurfaceFromNativeWindow(*egl, aWindow, config);
    if (!surface) {
      return nullptr;
    }
  }

  CreateContextFlags flags = CreateContextFlags::NONE;
  if (aWebRender && StaticPrefs::gfx_webrender_prefer_robustness_AtStartup()) {
    flags |= CreateContextFlags::PREFER_ROBUSTNESS;
  }
  if (aWebRender && aUseGles) {
    flags |= CreateContextFlags::PREFER_ES3;
  }
  if (!aWebRender) {
    flags |= CreateContextFlags::REQUIRE_COMPAT_PROFILE;
  }

  const auto desc = GLContextDesc{{flags}, false};
  RefPtr<GLContextEGL> gl = GLContextEGL::CreateGLContext(
      egl, desc, config, surface, aUseGles, &failureId);
  if (!gl) {
    const auto err = egl->mLib->fGetError();
    gfxCriticalNote << "Failed to create EGLContext!: " << gfx::hexa(err);
    mozilla::gl::DestroySurface(*egl, surface);
    return nullptr;
  }

  gl->MakeCurrent();
  gl->SetIsDoubleBuffered(doubleBuffered);

  if (surface && IsWaylandDisplay()) {
    // Make eglSwapBuffers() non-blocking on wayland
    egl->fSwapInterval(0);
  }
  if (aWebRender && egl->mLib->IsANGLE()) {
    MOZ_ASSERT(doubleBuffered);
    egl->fSwapInterval(0);
  }
  return gl.forget();
}

already_AddRefed<GLContext> GLContextEGLFactory::Create(
    EGLNativeWindowType aWindow, bool aWebRender, int32_t aDepth) {
  RefPtr<GLContext> glContext;
#if !defined(MOZ_WIDGET_ANDROID)
  glContext = CreateImpl(aWindow, aWebRender, /* aUseGles */ false, aDepth);
#endif  // !defined(MOZ_WIDGET_ANDROID)

  if (!glContext) {
    glContext = CreateImpl(aWindow, aWebRender, /* aUseGles */ true, aDepth);
  }
  return glContext.forget();
}

/* static */
EGLSurface GLContextEGL::CreateEGLSurfaceForCompositorWidget(
    widget::CompositorWidget* aCompositorWidget, const EGLConfig aConfig) {
  nsCString discardFailureId;
  const auto egl = DefaultEglDisplay(&discardFailureId);
  if (!egl) {
    gfxCriticalNote << "Failed to load EGL library 6!";
    return EGL_NO_SURFACE;
  }

  MOZ_ASSERT(aCompositorWidget);
  EGLNativeWindowType window =
      GET_NATIVE_WINDOW_FROM_COMPOSITOR_WIDGET(aCompositorWidget);
  if (!window) {
    gfxCriticalNote << "window is null";
    return EGL_NO_SURFACE;
  }

  return mozilla::gl::CreateSurfaceFromNativeWindow(*egl, window, aConfig);
}

GLContextEGL::GLContextEGL(const std::shared_ptr<EglDisplay> egl,
                           const GLContextDesc& desc, EGLConfig config,
                           EGLSurface surface, EGLContext context)
    : GLContext(desc, nullptr, false),
      mEgl(egl),
      mConfig(config),
      mContext(context),
      mSurface(surface),
      mFallbackSurface(CreateFallbackSurface(*mEgl, mConfig)) {
#ifdef DEBUG
  printf_stderr("Initializing context %p surface %p on display %p\n", mContext,
                mSurface, mEgl->mDisplay);
#endif
}

void GLContextEGL::OnMarkDestroyed() {
  if (mSurfaceOverride != EGL_NO_SURFACE) {
    SetEGLSurfaceOverride(EGL_NO_SURFACE);
  }
}

GLContextEGL::~GLContextEGL() {
  MarkDestroyed();

  // Wrapped context should not destroy eglContext/Surface
  if (!mOwnsContext) {
    return;
  }

#ifdef DEBUG
  printf_stderr("Destroying context %p surface %p on display %p\n", mContext,
                mSurface, mEgl->mDisplay);
#endif

  mEgl->fDestroyContext(mContext);

  mozilla::gl::DestroySurface(*mEgl, mSurface);
  mozilla::gl::DestroySurface(*mEgl, mFallbackSurface);
}

bool GLContextEGL::Init() {
  if (!GLContext::Init()) return false;

  bool current = MakeCurrent();
  if (!current) {
    gfx::LogFailure("Couldn't get device attachments for device."_ns);
    return false;
  }

  mShareWithEGLImage =
      mEgl->HasKHRImageBase() &&
      mEgl->IsExtensionSupported(EGLExtension::KHR_gl_texture_2D_image) &&
      IsExtensionSupported(OES_EGL_image);

  return true;
}

bool GLContextEGL::BindTexImage() {
  if (!mSurface) return false;

  if (mBound && !ReleaseTexImage()) return false;

  EGLBoolean success =
      mEgl->fBindTexImage((EGLSurface)mSurface, LOCAL_EGL_BACK_BUFFER);
  if (success == LOCAL_EGL_FALSE) return false;

  mBound = true;
  return true;
}

bool GLContextEGL::ReleaseTexImage() {
  if (!mBound) return true;

  if (!mSurface) return false;

  EGLBoolean success;
  success = mEgl->fReleaseTexImage((EGLSurface)mSurface, LOCAL_EGL_BACK_BUFFER);
  if (success == LOCAL_EGL_FALSE) return false;

  mBound = false;
  return true;
}

void GLContextEGL::SetEGLSurfaceOverride(EGLSurface surf) {
  mSurfaceOverride = surf;
  DebugOnly<bool> ok = MakeCurrent(true);
  MOZ_ASSERT(ok);
}

bool GLContextEGL::MakeCurrentImpl() const {
  EGLSurface surface =
      (mSurfaceOverride != EGL_NO_SURFACE) ? mSurfaceOverride : mSurface;
  if (!surface) {
    surface = mFallbackSurface;
  }

  const bool succeeded = mEgl->fMakeCurrent(surface, surface, mContext);
  if (!succeeded) {
    const auto eglError = mEgl->mLib->fGetError();
    if (eglError == LOCAL_EGL_CONTEXT_LOST) {
      OnContextLostError();
    } else {
      NS_WARNING("Failed to make GL context current!");
#ifdef DEBUG
      printf_stderr("EGL Error: 0x%04x\n", eglError);
#endif
    }
  }

  return succeeded;
}

bool GLContextEGL::IsCurrentImpl() const {
  return mEgl->mLib->fGetCurrentContext() == mContext;
}

bool GLContextEGL::RenewSurface(CompositorWidget* aWidget) {
  if (!mOwnsContext) {
    return false;
  }
  // unconditionally release the surface and create a new one. Don't try to
  // optimize this away. If we get here, then by definition we know that we want
  // to get a new surface.
  ReleaseSurface();
  MOZ_ASSERT(aWidget);

  EGLNativeWindowType nativeWindow =
      GET_NATIVE_WINDOW_FROM_COMPOSITOR_WIDGET(aWidget);
  if (nativeWindow) {
    mSurface = mozilla::gl::CreateSurfaceFromNativeWindow(*mEgl, nativeWindow,
                                                          mConfig);
    if (!mSurface) {
      NS_WARNING("Failed to create EGLSurface from native window");
      return false;
    }
  }
  const bool ok = MakeCurrent(true);
  MOZ_ASSERT(ok);
  if (mSurface && IsWaylandDisplay()) {
    // Make eglSwapBuffers() non-blocking on wayland
    mEgl->fSwapInterval(0);
  }
  return ok;
}

void GLContextEGL::ReleaseSurface() {
  if (mOwnsContext) {
    mozilla::gl::DestroySurface(*mEgl, mSurface);
  }
  if (mSurface == mSurfaceOverride) {
    mSurfaceOverride = EGL_NO_SURFACE;
  }
  mSurface = EGL_NO_SURFACE;
}

Maybe<SymbolLoader> GLContextEGL::GetSymbolLoader() const {
  return mEgl->mLib->GetSymbolLoader();
}

bool GLContextEGL::SwapBuffers() {
  EGLSurface surface =
      mSurfaceOverride != EGL_NO_SURFACE ? mSurfaceOverride : mSurface;
  if (surface) {
    if ((mEgl->IsExtensionSupported(
             EGLExtension::EXT_swap_buffers_with_damage) ||
         mEgl->IsExtensionSupported(
             EGLExtension::KHR_swap_buffers_with_damage))) {
      std::vector<EGLint> rects;
      for (auto iter = mDamageRegion.RectIter(); !iter.Done(); iter.Next()) {
        const IntRect& r = iter.Get();
        rects.push_back(r.X());
        rects.push_back(r.Y());
        rects.push_back(r.Width());
        rects.push_back(r.Height());
      }
      mDamageRegion.SetEmpty();
      return mEgl->fSwapBuffersWithDamage(surface, rects.data(),
                                          rects.size() / 4);
    }
    return mEgl->fSwapBuffers(surface);
  } else {
    return false;
  }
}

void GLContextEGL::SetDamage(const nsIntRegion& aDamageRegion) {
  mDamageRegion = aDamageRegion;
}

void GLContextEGL::GetWSIInfo(nsCString* const out) const {
  out->AppendLiteral("EGL_VENDOR: ");
  out->Append(
      (const char*)mEgl->mLib->fQueryString(mEgl->mDisplay, LOCAL_EGL_VENDOR));

  out->AppendLiteral("\nEGL_VERSION: ");
  out->Append(
      (const char*)mEgl->mLib->fQueryString(mEgl->mDisplay, LOCAL_EGL_VERSION));

  out->AppendLiteral("\nEGL_EXTENSIONS: ");
  out->Append((const char*)mEgl->mLib->fQueryString(mEgl->mDisplay,
                                                    LOCAL_EGL_EXTENSIONS));

#ifndef ANDROID  // This query will crash some old android.
  out->AppendLiteral("\nEGL_EXTENSIONS(nullptr): ");
  out->Append(
      (const char*)mEgl->mLib->fQueryString(nullptr, LOCAL_EGL_EXTENSIONS));
#endif
}

// hold a reference to the given surface
// for the lifetime of this context.
void GLContextEGL::HoldSurface(gfxASurface* aSurf) { mThebesSurface = aSurf; }

bool GLContextEGL::HasExtBufferAge() const {
  return mEgl->IsExtensionSupported(EGLExtension::EXT_buffer_age);
}

bool GLContextEGL::HasKhrPartialUpdate() const {
  return mEgl->IsExtensionSupported(EGLExtension::KHR_partial_update);
}

GLint GLContextEGL::GetBufferAge() const {
  EGLSurface surface =
      mSurfaceOverride != EGL_NO_SURFACE ? mSurfaceOverride : mSurface;

  if (surface && (HasExtBufferAge() || HasKhrPartialUpdate())) {
    EGLint result;
    mEgl->fQuerySurface(surface, LOCAL_EGL_BUFFER_AGE_EXT, &result);
    return result;
  }

  return 0;
}

#define LOCAL_EGL_CONTEXT_PROVOKING_VERTEX_DONT_CARE_MOZ 0x6000

RefPtr<GLContextEGL> GLContextEGL::CreateGLContext(
    const std::shared_ptr<EglDisplay> egl, const GLContextDesc& desc,
    EGLConfig config, EGLSurface surface, const bool useGles,
    nsACString* const out_failureId) {
  const auto& flags = desc.flags;

  std::vector<EGLint> required_attribs;

  if (useGles) {
    // TODO: This fBindAPI could be more thread-safe
    if (egl->mLib->fBindAPI(LOCAL_EGL_OPENGL_ES_API) == LOCAL_EGL_FALSE) {
      *out_failureId = "FEATURE_FAILURE_EGL_ES"_ns;
      NS_WARNING("Failed to bind API to GLES!");
      return nullptr;
    }
    required_attribs.push_back(LOCAL_EGL_CONTEXT_MAJOR_VERSION);
    if (flags & CreateContextFlags::PREFER_ES3) {
      required_attribs.push_back(3);
    } else {
      required_attribs.push_back(2);
    }
  } else {
    if (egl->mLib->fBindAPI(LOCAL_EGL_OPENGL_API) == LOCAL_EGL_FALSE) {
      *out_failureId = "FEATURE_FAILURE_EGL"_ns;
      NS_WARNING("Failed to bind API to GL!");
      return nullptr;
    }
    if (flags & CreateContextFlags::REQUIRE_COMPAT_PROFILE) {
      required_attribs.push_back(LOCAL_EGL_CONTEXT_OPENGL_PROFILE_MASK);
      required_attribs.push_back(
          LOCAL_EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT);
      required_attribs.push_back(LOCAL_EGL_CONTEXT_MAJOR_VERSION);
      required_attribs.push_back(2);
    } else {
      // !REQUIRE_COMPAT_PROFILE means core profle.
      required_attribs.push_back(LOCAL_EGL_CONTEXT_OPENGL_PROFILE_MASK);
      required_attribs.push_back(LOCAL_EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT);
      required_attribs.push_back(LOCAL_EGL_CONTEXT_MAJOR_VERSION);
      required_attribs.push_back(3);
      required_attribs.push_back(LOCAL_EGL_CONTEXT_MINOR_VERSION);
      required_attribs.push_back(2);
    }
  }

  if ((flags & CreateContextFlags::PREFER_EXACT_VERSION) &&
      egl->mLib->IsANGLE()) {
    required_attribs.push_back(
        LOCAL_EGL_CONTEXT_OPENGL_BACKWARDS_COMPATIBLE_ANGLE);
    required_attribs.push_back(LOCAL_EGL_FALSE);
  }

  const auto debugFlags = GLContext::ChooseDebugFlags(flags);
  if (!debugFlags && flags & CreateContextFlags::NO_VALIDATION &&
      egl->IsExtensionSupported(EGLExtension::KHR_create_context_no_error)) {
    required_attribs.push_back(LOCAL_EGL_CONTEXT_OPENGL_NO_ERROR_KHR);
    required_attribs.push_back(LOCAL_EGL_TRUE);
  }

  if (flags & CreateContextFlags::PROVOKING_VERTEX_DONT_CARE &&
      egl->IsExtensionSupported(
          EGLExtension::MOZ_create_context_provoking_vertex_dont_care)) {
    required_attribs.push_back(
        LOCAL_EGL_CONTEXT_PROVOKING_VERTEX_DONT_CARE_MOZ);
    required_attribs.push_back(LOCAL_EGL_TRUE);
  }

  std::vector<EGLint> ext_robustness_attribs;
  std::vector<EGLint> ext_rbab_attribs;  // RBAB: Robust Buffer Access Behavior
  std::vector<EGLint> khr_robustness_attribs;
  std::vector<EGLint> khr_rbab_attribs;  // RBAB: Robust Buffer Access Behavior
  if (flags & CreateContextFlags::PREFER_ROBUSTNESS) {
    std::vector<EGLint> base_robustness_attribs = required_attribs;
    if (egl->IsExtensionSupported(
            EGLExtension::NV_robustness_video_memory_purge)) {
      base_robustness_attribs.push_back(
          LOCAL_EGL_GENERATE_RESET_ON_VIDEO_MEMORY_PURGE_NV);
      base_robustness_attribs.push_back(LOCAL_EGL_TRUE);
    }

    if (egl->IsExtensionSupported(
            EGLExtension::EXT_create_context_robustness)) {
      ext_robustness_attribs = base_robustness_attribs;
      ext_robustness_attribs.push_back(
          LOCAL_EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_EXT);
      ext_robustness_attribs.push_back(LOCAL_EGL_LOSE_CONTEXT_ON_RESET_EXT);

      if (gfxVars::AllowEglRbab()) {
        ext_rbab_attribs = ext_robustness_attribs;
        ext_rbab_attribs.push_back(LOCAL_EGL_CONTEXT_OPENGL_ROBUST_ACCESS_EXT);
        ext_rbab_attribs.push_back(LOCAL_EGL_TRUE);
      }
    }

    if (egl->IsExtensionSupported(EGLExtension::KHR_create_context)) {
      khr_robustness_attribs = base_robustness_attribs;
      khr_robustness_attribs.push_back(
          LOCAL_EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_KHR);
      khr_robustness_attribs.push_back(LOCAL_EGL_LOSE_CONTEXT_ON_RESET_KHR);

      khr_rbab_attribs = khr_robustness_attribs;
      khr_rbab_attribs.push_back(LOCAL_EGL_CONTEXT_FLAGS_KHR);
      khr_rbab_attribs.push_back(
          LOCAL_EGL_CONTEXT_OPENGL_ROBUST_ACCESS_BIT_KHR);
    }
  }

  const auto fnCreate = [&](const std::vector<EGLint>& attribs) {
    auto terminated_attribs = attribs;

    for (const auto& cur : kTerminationAttribs) {
      terminated_attribs.push_back(cur);
    }

    return egl->fCreateContext(config, EGL_NO_CONTEXT,
                               terminated_attribs.data());
  };

  EGLContext context;
  do {
    if (!khr_rbab_attribs.empty()) {
      context = fnCreate(khr_rbab_attribs);
      if (context) break;
      NS_WARNING("Failed to create EGLContext with khr_rbab_attribs");
    }

    if (!ext_rbab_attribs.empty()) {
      context = fnCreate(ext_rbab_attribs);
      if (context) break;
      NS_WARNING("Failed to create EGLContext with ext_rbab_attribs");
    }

    if (!khr_robustness_attribs.empty()) {
      context = fnCreate(khr_robustness_attribs);
      if (context) break;
      NS_WARNING("Failed to create EGLContext with khr_robustness_attribs");
    }

    if (!ext_robustness_attribs.empty()) {
      context = fnCreate(ext_robustness_attribs);
      if (context) break;
      NS_WARNING("Failed to create EGLContext with ext_robustness_attribs");
    }

    context = fnCreate(required_attribs);
    if (context) break;
    NS_WARNING("Failed to create EGLContext with required_attribs");

    *out_failureId = "FEATURE_FAILURE_EGL_CREATE"_ns;
    return nullptr;
  } while (false);
  MOZ_ASSERT(context);

  RefPtr<GLContextEGL> glContext =
      new GLContextEGL(egl, desc, config, surface, context);
  if (!glContext->Init()) {
    *out_failureId = "FEATURE_FAILURE_EGL_INIT"_ns;
    return nullptr;
  }

  if (GLContext::ShouldSpew()) {
    printf_stderr("new GLContextEGL %p on EGLDisplay %p\n", glContext.get(),
                  egl->mDisplay);
  }

  return glContext;
}

// static
EGLSurface GLContextEGL::CreatePBufferSurfaceTryingPowerOfTwo(
    EglDisplay& egl, EGLConfig config, EGLenum bindToTextureFormat,
    mozilla::gfx::IntSize& pbsize) {
  nsTArray<EGLint> pbattrs(16);
  EGLSurface surface = nullptr;

TRY_AGAIN_POWER_OF_TWO:
  pbattrs.Clear();
  pbattrs.AppendElement(LOCAL_EGL_WIDTH);
  pbattrs.AppendElement(pbsize.width);
  pbattrs.AppendElement(LOCAL_EGL_HEIGHT);
  pbattrs.AppendElement(pbsize.height);

  if (bindToTextureFormat != LOCAL_EGL_NONE) {
    pbattrs.AppendElement(LOCAL_EGL_TEXTURE_TARGET);
    pbattrs.AppendElement(LOCAL_EGL_TEXTURE_2D);

    pbattrs.AppendElement(LOCAL_EGL_TEXTURE_FORMAT);
    pbattrs.AppendElement(bindToTextureFormat);
  }

  for (const auto& cur : kTerminationAttribs) {
    pbattrs.AppendElement(cur);
  }

  surface = egl.fCreatePbufferSurface(config, &pbattrs[0]);
  if (!surface) {
    if (!is_power_of_two(pbsize.width) || !is_power_of_two(pbsize.height)) {
      if (!is_power_of_two(pbsize.width))
        pbsize.width = next_power_of_two(pbsize.width);
      if (!is_power_of_two(pbsize.height))
        pbsize.height = next_power_of_two(pbsize.height);

      NS_WARNING("Failed to create pbuffer, trying power of two dims");
      goto TRY_AGAIN_POWER_OF_TWO;
    }

    NS_WARNING("Failed to create pbuffer surface");
    return nullptr;
  }

  return surface;
}

#if defined(MOZ_WAYLAND)
WaylandGLSurface::WaylandGLSurface(struct wl_surface* aWaylandSurface,
                                   struct wl_egl_window* aEGLWindow)
    : mWaylandSurface(aWaylandSurface), mEGLWindow(aEGLWindow) {}

WaylandGLSurface::~WaylandGLSurface() {
  wl_egl_window_destroy(mEGLWindow);
  wl_surface_destroy(mWaylandSurface);
}
#endif

// static
EGLSurface GLContextEGL::CreateWaylandBufferSurface(
    EglDisplay& egl, EGLConfig config, mozilla::gfx::IntSize& pbsize) {
  wl_egl_window* eglwindow = nullptr;

#ifdef MOZ_GTK_WAYLAND
  struct wl_compositor* compositor =
      gdk_wayland_display_get_wl_compositor(gdk_display_get_default());
  struct wl_surface* wlsurface = wl_compositor_create_surface(compositor);
  eglwindow = wl_egl_window_create(wlsurface, pbsize.width, pbsize.height);
#endif
  if (!eglwindow) return nullptr;

  const auto surface = egl.fCreateWindowSurface(
      config, reinterpret_cast<EGLNativeWindowType>(eglwindow), 0);
  if (surface) {
#ifdef MOZ_GTK_WAYLAND
    MOZ_ASSERT(!sWaylandGLSurface.Contains(surface));
    sWaylandGLSurface.LookupOrInsert(
        surface, new WaylandGLSurface(wlsurface, eglwindow));
#endif
  }

  return surface;
}

static const EGLint kEGLConfigAttribsRGB16[] = {
    LOCAL_EGL_SURFACE_TYPE, LOCAL_EGL_WINDOW_BIT,
    LOCAL_EGL_RED_SIZE,     5,
    LOCAL_EGL_GREEN_SIZE,   6,
    LOCAL_EGL_BLUE_SIZE,    5,
    LOCAL_EGL_ALPHA_SIZE,   0};

static const EGLint kEGLConfigAttribsRGB24[] = {
    LOCAL_EGL_SURFACE_TYPE, LOCAL_EGL_WINDOW_BIT,
    LOCAL_EGL_RED_SIZE,     8,
    LOCAL_EGL_GREEN_SIZE,   8,
    LOCAL_EGL_BLUE_SIZE,    8,
    LOCAL_EGL_ALPHA_SIZE,   0};

static const EGLint kEGLConfigAttribsRGBA32[] = {
    LOCAL_EGL_SURFACE_TYPE, LOCAL_EGL_WINDOW_BIT,
    LOCAL_EGL_RED_SIZE,     8,
    LOCAL_EGL_GREEN_SIZE,   8,
    LOCAL_EGL_BLUE_SIZE,    8,
    LOCAL_EGL_ALPHA_SIZE,   8};

bool CreateConfig(EglDisplay& egl, EGLConfig* aConfig, int32_t depth,
                  bool aEnableDepthBuffer, bool aUseGles, int aVisual) {
  EGLConfig configs[64];
  std::vector<EGLint> attribs;
  EGLint ncfg = ArrayLength(configs);

  switch (depth) {
    case 16:
      for (const auto& cur : kEGLConfigAttribsRGB16) {
        attribs.push_back(cur);
      }
      break;
    case 24:
      for (const auto& cur : kEGLConfigAttribsRGB24) {
        attribs.push_back(cur);
      }
      break;
    case 32:
      for (const auto& cur : kEGLConfigAttribsRGBA32) {
        attribs.push_back(cur);
      }
      break;
    default:
      NS_ERROR("Unknown pixel depth");
      return false;
  }

  if (aUseGles) {
    attribs.push_back(LOCAL_EGL_RENDERABLE_TYPE);
    attribs.push_back(LOCAL_EGL_OPENGL_ES2_BIT);
  }
  for (const auto& cur : kTerminationAttribs) {
    attribs.push_back(cur);
  }

  if (!egl.fChooseConfig(attribs.data(), configs, ncfg, &ncfg) || ncfg < 1) {
    return false;
  }

  Maybe<EGLConfig> fallbackConfig;

  for (int j = 0; j < ncfg; ++j) {
    EGLConfig config = configs[j];
    EGLint r, g, b, a;
    if (egl.fGetConfigAttrib(config, LOCAL_EGL_RED_SIZE, &r) &&
        egl.fGetConfigAttrib(config, LOCAL_EGL_GREEN_SIZE, &g) &&
        egl.fGetConfigAttrib(config, LOCAL_EGL_BLUE_SIZE, &b) &&
        egl.fGetConfigAttrib(config, LOCAL_EGL_ALPHA_SIZE, &a) &&
        ((depth == 16 && r == 5 && g == 6 && b == 5) ||
         (depth == 24 && r == 8 && g == 8 && b == 8) ||
         (depth == 32 && r == 8 && g == 8 && b == 8 && a == 8))) {
      EGLint z;
      if (aEnableDepthBuffer) {
        if (!egl.fGetConfigAttrib(config, LOCAL_EGL_DEPTH_SIZE, &z) ||
            z != 24) {
          continue;
        }
      }
      if (kIsX11 && aVisual) {
        int vis;
        if (!egl.fGetConfigAttrib(config, LOCAL_EGL_NATIVE_VISUAL_ID, &vis) ||
            aVisual != vis) {
          if (!fallbackConfig) {
            fallbackConfig = Some(config);
          }
          continue;
        }
      }
      *aConfig = config;
      return true;
    }
  }

  // We don't have a frame buffer X11 visual which matches the EGL visual
  // from GLContextEGL::FindVisual(). Let's try to use the fallback one and hope
  // we're not on NVIDIA (Bug 1478454) as it causes X11 BadMatch error there.
  if (kIsX11 && fallbackConfig) {
    *aConfig = fallbackConfig.value();
    return true;
  }

  return false;
}

// Return true if a suitable EGLConfig was found and pass it out
// through aConfig.  Return false otherwise.
//
// NB: It's entirely legal for the returned EGLConfig to be valid yet
// have the value null.
// aVisual is used in Linux only.
static bool CreateConfigScreen(EglDisplay& egl, EGLConfig* const aConfig,
                               const bool aEnableDepthBuffer,
                               const bool aUseGles, int aVisual) {
  int32_t depth = gfxVars::ScreenDepth();
  if (CreateConfig(egl, aConfig, depth, aEnableDepthBuffer, aUseGles,
                   aVisual)) {
    return true;
  }
#ifdef MOZ_WIDGET_ANDROID
  // Bug 736005
  // Android doesn't always support 16 bit so also try 24 bit
  if (depth == 16) {
    return CreateConfig(egl, aConfig, 24, aEnableDepthBuffer, aUseGles);
  }
  // Bug 970096
  // Some devices that have 24 bit screens only support 16 bit OpenGL?
  if (depth == 24) {
    return CreateConfig(egl, aConfig, 16, aEnableDepthBuffer, aUseGles);
  }
#endif
  return false;
}

already_AddRefed<GLContext> GLContextProviderEGL::CreateForCompositorWidget(
    CompositorWidget* aCompositorWidget, bool aWebRender,
    bool /*aForceAccelerated*/) {
  EGLNativeWindowType window = nullptr;
  int32_t depth = 0;
  if (aCompositorWidget) {
    window = GET_NATIVE_WINDOW_FROM_COMPOSITOR_WIDGET(aCompositorWidget);
#if defined(MOZ_WIDGET_GTK)
    depth = aCompositorWidget->AsX11()->GetDepth();
#endif
  }
  return GLContextEGLFactory::Create(window, aWebRender, depth);
}

EGLSurface GLContextEGL::CreateCompatibleSurface(void* aWindow) const {
  MOZ_ASSERT(aWindow);
  if (mConfig == EGL_NO_CONFIG) {
    MOZ_CRASH("GFX: Failed with invalid EGLConfig 2!");
  }

  const auto fnCreate = [&](const bool useGles) -> EGLSurface {
    // NOTE: aWindow is an ANativeWindow
    auto config = mConfig;
    if (!config && !CreateConfigScreen(*mEgl, &config,
                                       /* aEnableDepthBuffer */ false,
                                       /* useGles */ useGles)) {
      return nullptr;
    }

    return mEgl->fCreateWindowSurface(
        config, reinterpret_cast<EGLNativeWindowType>(aWindow), 0);
  };

  auto surface = fnCreate(false);
  if (!surface) {
    surface = fnCreate(true);
  }
  if (!surface) {
    MOZ_CRASH("GFX: Failed to create EGLSurface 2!");
  }
  return surface;
}

static void FillContextAttribs(bool es3, bool useGles, nsTArray<EGLint>* out) {
  out->AppendElement(LOCAL_EGL_SURFACE_TYPE);
  if (IsWaylandDisplay()) {
    // Wayland on desktop does not support PBuffer or FBO.
    // We create a dummy wl_egl_window instead.
    out->AppendElement(LOCAL_EGL_WINDOW_BIT);
  } else {
    out->AppendElement(LOCAL_EGL_PBUFFER_BIT);
  }

  if (useGles) {
    out->AppendElement(LOCAL_EGL_RENDERABLE_TYPE);
    if (es3) {
      out->AppendElement(LOCAL_EGL_OPENGL_ES3_BIT_KHR);
    } else {
      out->AppendElement(LOCAL_EGL_OPENGL_ES2_BIT);
    }
  }

  out->AppendElement(LOCAL_EGL_RED_SIZE);
  out->AppendElement(8);

  out->AppendElement(LOCAL_EGL_GREEN_SIZE);
  out->AppendElement(8);

  out->AppendElement(LOCAL_EGL_BLUE_SIZE);
  out->AppendElement(8);

  out->AppendElement(LOCAL_EGL_ALPHA_SIZE);
  out->AppendElement(8);

  out->AppendElement(LOCAL_EGL_DEPTH_SIZE);
  out->AppendElement(0);

  out->AppendElement(LOCAL_EGL_STENCIL_SIZE);
  out->AppendElement(0);

  // EGL_ATTRIBS_LIST_SAFE_TERMINATION_WORKING_AROUND_BUGS
  out->AppendElement(LOCAL_EGL_NONE);
  out->AppendElement(0);

  out->AppendElement(0);
  out->AppendElement(0);
}

/*
/// Useful for debugging, but normally unused.
static GLint GetAttrib(GLLibraryEGL* egl, EGLConfig config, EGLint attrib) {
  EGLint bits = 0;
  egl->fGetConfigAttrib(config, attrib, &bits);
  MOZ_ASSERT(egl->fGetError() == LOCAL_EGL_SUCCESS);

  return bits;
}
*/

static EGLConfig ChooseConfig(EglDisplay& egl, const GLContextCreateDesc& desc,
                              const bool useGles) {
  nsTArray<EGLint> configAttribList;
  FillContextAttribs(bool(desc.flags & CreateContextFlags::PREFER_ES3), useGles,
                     &configAttribList);

  const EGLint* configAttribs = configAttribList.Elements();

  // The sorting dictated by the spec for eglChooseConfig reasonably assures
  // that a reasonable 'best' config is on top.
  const EGLint kMaxConfigs = 1;
  EGLConfig configs[kMaxConfigs];
  EGLint foundConfigs = 0;
  if (!egl.fChooseConfig(configAttribs, configs, kMaxConfigs, &foundConfigs) ||
      foundConfigs == 0) {
    return EGL_NO_CONFIG;
  }

  EGLConfig config = configs[0];
  return config;
}

#ifdef MOZ_X11
/* static */
bool GLContextEGL::FindVisual(bool aUseWebRender, bool useAlpha,
                              int* const out_visualId) {
  nsCString discardFailureId;
  const auto egl = DefaultEglDisplay(&discardFailureId);
  if (!egl) {
    gfxCriticalNote
        << "GLContextEGL::FindVisual(): Failed to load EGL library!";
    return false;
  }

  EGLConfig config;
  const int bpp = useAlpha ? 32 : 24;
  if (!CreateConfig(*egl, &config, bpp, aUseWebRender, /* aUseGles */ false)) {
    gfxCriticalNote
        << "GLContextEGL::FindVisual(): Failed to create EGLConfig!";
    return false;
  }
  if (egl->fGetConfigAttrib(config, LOCAL_EGL_NATIVE_VISUAL_ID, out_visualId)) {
    return true;
  }
  return false;
}
#endif

/*static*/
RefPtr<GLContextEGL> GLContextEGL::CreateEGLPBufferOffscreenContextImpl(
    const std::shared_ptr<EglDisplay> egl, const GLContextCreateDesc& desc,
    const mozilla::gfx::IntSize& size, const bool useGles,
    nsACString* const out_failureId) {
  const EGLConfig config = ChooseConfig(*egl, desc, useGles);
  if (config == EGL_NO_CONFIG) {
    *out_failureId = "FEATURE_FAILURE_EGL_NO_CONFIG"_ns;
    NS_WARNING("Failed to find a compatible config.");
    return nullptr;
  }

  if (GLContext::ShouldSpew()) {
    egl->DumpEGLConfig(config);
  }

  mozilla::gfx::IntSize pbSize(size);
  EGLSurface surface = nullptr;
  if (IsWaylandDisplay()) {
    surface = GLContextEGL::CreateWaylandBufferSurface(*egl, config, pbSize);
  } else {
    surface = GLContextEGL::CreatePBufferSurfaceTryingPowerOfTwo(
        *egl, config, LOCAL_EGL_NONE, pbSize);
  }
  if (!surface) {
    *out_failureId = "FEATURE_FAILURE_EGL_POT"_ns;
    NS_WARNING("Failed to create PBuffer for context!");
    return nullptr;
  }

  auto fullDesc = GLContextDesc{desc};
  fullDesc.isOffscreen = true;
  RefPtr<GLContextEGL> gl = GLContextEGL::CreateGLContext(
      egl, fullDesc, config, surface, useGles, out_failureId);
  if (!gl) {
    NS_WARNING("Failed to create GLContext from PBuffer");
    egl->fDestroySurface(surface);
#if defined(MOZ_WAYLAND)
    DeleteWaylandGLSurface(surface);
#endif
    return nullptr;
  }

  return gl;
}

/*static*/
RefPtr<GLContextEGL> GLContextEGL::CreateEGLPBufferOffscreenContext(
    const std::shared_ptr<EglDisplay> display, const GLContextCreateDesc& desc,
    const mozilla::gfx::IntSize& size, nsACString* const out_failureId) {
  RefPtr<GLContextEGL> gl = CreateEGLPBufferOffscreenContextImpl(
      display, desc, size, /* useGles */ false, out_failureId);
  if (!gl) {
    gl = CreateEGLPBufferOffscreenContextImpl(
        display, desc, size, /* useGles */ true, out_failureId);
  }
  return gl;
}

/*static*/
already_AddRefed<GLContext> GLContextProviderEGL::CreateHeadless(
    const GLContextCreateDesc& desc, nsACString* const out_failureId) {
  const auto display = DefaultEglDisplay(out_failureId);
  if (!display) {
    return nullptr;
  }
  mozilla::gfx::IntSize dummySize = mozilla::gfx::IntSize(16, 16);
  auto ret = GLContextEGL::CreateEGLPBufferOffscreenContext(
      display, desc, dummySize, out_failureId);
  return ret.forget();
}

// Don't want a global context on Android as 1) share groups across 2 threads
// fail on many Tegra drivers (bug 759225) and 2) some mobile devices have a
// very strict limit on global number of GL contexts (bug 754257) and 3) each
// EGL context eats 750k on B2G (bug 813783)
/*static*/
GLContext* GLContextProviderEGL::GetGlobalContext() { return nullptr; }

// -

static StaticMutex sMutex;
static StaticRefPtr<GLLibraryEGL> gDefaultEglLibrary;

RefPtr<GLLibraryEGL> DefaultEglLibrary(nsACString* const out_failureId) {
  StaticMutexAutoLock lock(sMutex);
  if (!gDefaultEglLibrary) {
    gDefaultEglLibrary = GLLibraryEGL::Create(out_failureId);
    if (!gDefaultEglLibrary) {
      NS_WARNING("GLLibraryEGL::Create failed");
    }
  }
  return gDefaultEglLibrary.get();
}

// -

/*static*/
void GLContextProviderEGL::Shutdown() {
  StaticMutexAutoLock lock(sMutex);
  if (!gDefaultEglLibrary) {
    return;
  }
  gDefaultEglLibrary = nullptr;
}

} /* namespace gl */
} /* namespace mozilla */

#undef EGL_ATTRIBS_LIST_SAFE_TERMINATION_WORKING_AROUND_BUGS
