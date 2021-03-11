/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define PANGO_ENABLE_BACKEND
#define PANGO_ENABLE_ENGINE

#include "gfxPlatformGtk.h"

#include <gtk/gtk.h>
#include <fontconfig/fontconfig.h>

#include "base/task.h"
#include "base/thread.h"
#include "base/message_loop.h"
#include "cairo.h"
#include "gfx2DGlue.h"
#include "gfxFcPlatformFontList.h"
#include "gfxConfig.h"
#include "gfxContext.h"
#include "gfxImageSurface.h"
#include "gfxUserFontSet.h"
#include "gfxUtils.h"
#include "gfxFT2FontBase.h"
#include "gfxTextRun.h"
#include "GLContextProvider.h"
#include "mozilla/Atomics.h"
#include "mozilla/Components.h"
#include "mozilla/FontPropertyTypes.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Logging.h"
#include "mozilla/Monitor.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPrefs_gfx.h"
#include "mozilla/StaticPrefs_layers.h"
#include "nsAppRunner.h"
#include "nsIGfxInfo.h"
#include "nsMathUtils.h"
#include "nsUnicharUtils.h"
#include "nsUnicodeProperties.h"
#include "VsyncSource.h"

#ifdef MOZ_X11
#  include <gdk/gdkx.h>
#  include "cairo-xlib.h"
#  include "gfxXlibSurface.h"
#  include "GLContextGLX.h"
#  include "GLXLibrary.h"
#  include "mozilla/X11Util.h"

/* Undefine the Status from Xlib since it will conflict with system headers on
 * OSX */
#  if defined(__APPLE__) && defined(Status)
#    undef Status
#  endif
#endif /* MOZ_X11 */

#ifdef MOZ_WAYLAND
#  include <gdk/gdkwayland.h>
#  include "mozilla/widget/nsWaylandDisplay.h"
#  include "mozilla/widget/DMABufLibWrapper.h"
#  include "mozilla/StaticPrefs_media.h"
#endif

#define GDK_PIXMAP_SIZE_MAX 32767

#define GFX_PREF_MAX_GENERIC_SUBSTITUTIONS \
  "gfx.font_rendering.fontconfig.max_generic_substitutions"

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::unicode;
using namespace mozilla::widget;
using mozilla::dom::SystemFontListEntry;

static FT_Library gPlatformFTLibrary = nullptr;
static int32_t sDPI;

static void screen_resolution_changed(GdkScreen* aScreen, GParamSpec* aPspec,
                                      gpointer aClosure) {
  sDPI = 0;
}

#if defined(MOZ_X11)
// TODO(aosmond): The envvar is deprecated. We should remove it once EGL is the
// default in release.
static bool IsX11EGLEnvvarEnabled() {
  const char* eglPref = PR_GetEnv("MOZ_X11_EGL");
  return (eglPref && *eglPref);
}
#endif

gfxPlatformGtk::gfxPlatformGtk() {
  if (!gfxPlatform::IsHeadless()) {
    gtk_init(nullptr, nullptr);
  }

  mMaxGenericSubstitutions = UNINITIALIZED_VALUE;
  mIsX11Display = gfxPlatform::IsHeadless() ? false : GdkIsX11Display();
  if (XRE_IsParentProcess()) {
#ifdef MOZ_X11
    if (mIsX11Display && mozilla::Preferences::GetBool("gfx.xrender.enabled")) {
      gfxVars::SetUseXRender(true);
    }
#endif

    InitX11EGLConfig();

    if (IsWaylandDisplay() || gfxConfig::IsEnabled(Feature::X11_EGL)) {
      gfxVars::SetUseEGL(true);

      nsCOMPtr<nsIGfxInfo> gfxInfo = components::GfxInfo::Service();
      nsAutoCString drmRenderDevice;
      gfxInfo->GetDrmRenderDevice(drmRenderDevice);
      gfxVars::SetDrmRenderDevice(drmRenderDevice);
    }
  }

  InitBackendPrefs(GetBackendPrefs());

#ifdef MOZ_WAYLAND
  mUseWebGLDmabufBackend =
      gfxVars::UseEGL() && GetDMABufDevice()->IsDMABufWebGLEnabled();
#endif

  gPlatformFTLibrary = Factory::NewFTLibrary();
  MOZ_RELEASE_ASSERT(gPlatformFTLibrary);
  Factory::SetFTLibrary(gPlatformFTLibrary);

  GdkScreen* gdkScreen = gdk_screen_get_default();
  if (gdkScreen) {
    g_signal_connect(gdkScreen, "notify::resolution",
                     G_CALLBACK(screen_resolution_changed), nullptr);
  }
}

gfxPlatformGtk::~gfxPlatformGtk() {
  Factory::ReleaseFTLibrary(gPlatformFTLibrary);
  gPlatformFTLibrary = nullptr;
}

void gfxPlatformGtk::InitX11EGLConfig() {
  FeatureState& feature = gfxConfig::GetFeature(Feature::X11_EGL);
#ifdef MOZ_X11
  feature.EnableByDefault();

  if (StaticPrefs::gfx_x11_egl_force_enabled_AtStartup()) {
    feature.UserForceEnable("Force enabled by pref");
  } else if (IsX11EGLEnvvarEnabled()) {
    feature.UserForceEnable("Force enabled by envvar");
  } else if (StaticPrefs::gfx_x11_egl_force_disabled_AtStartup()) {
    feature.UserDisable("Force disabled by pref",
                        "FEATURE_FAILURE_USER_FORCE_DISABLED"_ns);
  }

  nsCString failureId;
  int32_t status;
  nsCOMPtr<nsIGfxInfo> gfxInfo = components::GfxInfo::Service();
  if (NS_FAILED(gfxInfo->GetFeatureStatus(nsIGfxInfo::FEATURE_X11_EGL,
                                          failureId, &status))) {
    feature.Disable(FeatureStatus::BlockedNoGfxInfo, "gfxInfo is broken",
                    "FEATURE_FAILURE_NO_GFX_INFO"_ns);
  } else if (status != nsIGfxInfo::FEATURE_STATUS_OK) {
    feature.Disable(FeatureStatus::Blocklisted, "Blocklisted by gfxInfo",
                    failureId);
  }

  nsAutoString testType;
  gfxInfo->GetTestType(testType);
  // We can only use X11/EGL if we actually found the EGL library and
  // successfully use it to determine system information in glxtest.
  if (testType != u"EGL") {
    feature.ForceDisable(FeatureStatus::Broken, "glxtest could not use EGL",
                         "FEATURE_FAILURE_GLXTEST_NO_EGL"_ns);
  }
#else
  feature.DisableByDefault(FeatureStatus::Unavailable, "X11 support missing",
                           "FEATURE_FAILURE_NO_X11"_ns);
#endif
}

void gfxPlatformGtk::FlushContentDrawing() {
  if (gfxVars::UseXRender()) {
    XFlush(DefaultXDisplay());
  }
}

void gfxPlatformGtk::InitPlatformGPUProcessPrefs() {
#ifdef MOZ_WAYLAND
  if (IsWaylandDisplay()) {
    FeatureState& gpuProc = gfxConfig::GetFeature(Feature::GPU_PROCESS);
    gpuProc.ForceDisable(FeatureStatus::Blocked,
                         "Wayland does not work in the GPU process",
                         "FEATURE_FAILURE_WAYLAND"_ns);
  }
#endif
}

already_AddRefed<gfxASurface> gfxPlatformGtk::CreateOffscreenSurface(
    const IntSize& aSize, gfxImageFormat aFormat) {
  if (!Factory::AllowedSurfaceSize(aSize)) {
    return nullptr;
  }

  RefPtr<gfxASurface> newSurface;
  bool needsClear = true;
#ifdef MOZ_X11
  // XXX we really need a different interface here, something that passes
  // in more context, including the display and/or target surface type that
  // we should try to match
  GdkScreen* gdkScreen = gdk_screen_get_default();
  if (gdkScreen) {
    // When forcing PaintedLayers to use image surfaces for content,
    // force creation of gfxImageSurface surfaces.
    if (gfxVars::UseXRender() && !UseImageOffscreenSurfaces()) {
      Screen* screen = gdk_x11_screen_get_xscreen(gdkScreen);
      XRenderPictFormat* xrenderFormat =
          gfxXlibSurface::FindRenderFormat(DisplayOfScreen(screen), aFormat);

      if (xrenderFormat) {
        newSurface = gfxXlibSurface::Create(screen, xrenderFormat, aSize);
      }
    } else {
      // We're not going to use XRender, so we don't need to
      // search for a render format
      newSurface = new gfxImageSurface(aSize, aFormat);
      // The gfxImageSurface ctor zeroes this for us, no need to
      // waste time clearing again
      needsClear = false;
    }
  }
#endif

  if (!newSurface) {
    // We couldn't create a native surface for whatever reason;
    // e.g., no display, no RENDER, bad size, etc.
    // Fall back to image surface for the data.
    newSurface = new gfxImageSurface(aSize, aFormat);
  }

  if (newSurface->CairoStatus()) {
    newSurface = nullptr;  // surface isn't valid for some reason
  }

  if (newSurface && needsClear) {
    gfxUtils::ClearThebesSurface(newSurface);
  }

  return newSurface.forget();
}

nsresult gfxPlatformGtk::GetFontList(nsAtom* aLangGroup,
                                     const nsACString& aGenericFamily,
                                     nsTArray<nsString>& aListOfFonts) {
  gfxPlatformFontList::PlatformFontList()->GetFontList(
      aLangGroup, aGenericFamily, aListOfFonts);
  return NS_OK;
}

// xxx - this is ubuntu centric, need to go through other distros and flesh
// out a more general list
static const char kFontDejaVuSans[] = "DejaVu Sans";
static const char kFontDejaVuSerif[] = "DejaVu Serif";
static const char kFontFreeSans[] = "FreeSans";
static const char kFontFreeSerif[] = "FreeSerif";
static const char kFontTakaoPGothic[] = "TakaoPGothic";
static const char kFontTwemojiMozilla[] = "Twemoji Mozilla";
static const char kFontDroidSansFallback[] = "Droid Sans Fallback";
static const char kFontWenQuanYiMicroHei[] = "WenQuanYi Micro Hei";
static const char kFontNanumGothic[] = "NanumGothic";
static const char kFontSymbola[] = "Symbola";

void gfxPlatformGtk::GetCommonFallbackFonts(uint32_t aCh, Script aRunScript,
                                            eFontPresentation aPresentation,
                                            nsTArray<const char*>& aFontList) {
  if (PrefersColor(aPresentation)) {
    aFontList.AppendElement(kFontTwemojiMozilla);
  }

  aFontList.AppendElement(kFontDejaVuSerif);
  aFontList.AppendElement(kFontFreeSerif);
  aFontList.AppendElement(kFontDejaVuSans);
  aFontList.AppendElement(kFontFreeSans);
  aFontList.AppendElement(kFontSymbola);

  // add fonts for CJK ranges
  // xxx - this isn't really correct, should use the same CJK font ordering
  // as the pref font code
  if (aCh >= 0x3000 && ((aCh < 0xe000) || (aCh >= 0xf900 && aCh < 0xfff0) ||
                        ((aCh >> 16) == 2))) {
    aFontList.AppendElement(kFontTakaoPGothic);
    aFontList.AppendElement(kFontDroidSansFallback);
    aFontList.AppendElement(kFontWenQuanYiMicroHei);
    aFontList.AppendElement(kFontNanumGothic);
  }
}

void gfxPlatformGtk::ReadSystemFontList(
    nsTArray<SystemFontListEntry>* retValue) {
  gfxFcPlatformFontList::PlatformFontList()->ReadSystemFontList(retValue);
}

gfxPlatformFontList* gfxPlatformGtk::CreatePlatformFontList() {
  gfxPlatformFontList* list = new gfxFcPlatformFontList();
  if (NS_SUCCEEDED(list->InitFontList())) {
    return list;
  }
  gfxPlatformFontList::Shutdown();
  return nullptr;
}

int32_t gfxPlatformGtk::GetFontScaleDPI() {
  if (MOZ_LIKELY(sDPI != 0)) {
    return sDPI;
  }
  GdkScreen* screen = gdk_screen_get_default();
  // Ensure settings in config files are processed.
  gtk_settings_get_for_screen(screen);
  int32_t dpi = int32_t(round(gdk_screen_get_resolution(screen)));
  if (dpi <= 0) {
    // Fall back to something sane
    dpi = 96;
  }
  sDPI = dpi;
  return dpi;
}

double gfxPlatformGtk::GetFontScaleFactor() {
  // Integer scale factors work well with GTK window scaling, image scaling, and
  // pixel alignment, but there is a range where 1 is too small and 2 is too
  // big.
  //
  // An additional step of 1.5 is added because this is common scale on WINNT
  // and at this ratio the advantages of larger rendering outweigh the
  // disadvantages from scaling and pixel mis-alignment.
  //
  // A similar step for 1.25 is added as well, because this is the scale that
  // "Large text" settings use in gnome, and it seems worth to allow, especially
  // on already-hidpi environments.
  int32_t dpi = GetFontScaleDPI();
  if (dpi < 120) {
    return 1.0;
  }
  if (dpi < 132) {
    return 1.25;
  }
  if (dpi < 168) {
    return 1.5;
  }
  return round(dpi / 96.0);
}

bool gfxPlatformGtk::UseImageOffscreenSurfaces() {
  return GetDefaultContentBackend() != mozilla::gfx::BackendType::CAIRO ||
         StaticPrefs::layers_use_image_offscreen_surfaces_AtStartup();
}

gfxImageFormat gfxPlatformGtk::GetOffscreenFormat() {
  // Make sure there is a screen
  GdkScreen* screen = gdk_screen_get_default();
  if (screen && gdk_visual_get_depth(gdk_visual_get_system()) == 16) {
    return SurfaceFormat::R5G6B5_UINT16;
  }

  return SurfaceFormat::X8R8G8B8_UINT32;
}

void gfxPlatformGtk::FontsPrefsChanged(const char* aPref) {
  // only checking for generic substitions, pass other changes up
  if (strcmp(GFX_PREF_MAX_GENERIC_SUBSTITUTIONS, aPref)) {
    gfxPlatform::FontsPrefsChanged(aPref);
    return;
  }

  mMaxGenericSubstitutions = UNINITIALIZED_VALUE;
  gfxFcPlatformFontList* pfl = gfxFcPlatformFontList::PlatformFontList();
  pfl->ClearGenericMappings();
  FlushFontAndWordCaches();
}

uint32_t gfxPlatformGtk::MaxGenericSubstitions() {
  if (mMaxGenericSubstitutions == UNINITIALIZED_VALUE) {
    mMaxGenericSubstitutions =
        Preferences::GetInt(GFX_PREF_MAX_GENERIC_SUBSTITUTIONS, 3);
    if (mMaxGenericSubstitutions < 0) {
      mMaxGenericSubstitutions = 3;
    }
  }

  return uint32_t(mMaxGenericSubstitutions);
}

bool gfxPlatformGtk::AccelerateLayersByDefault() { return true; }

#if defined(MOZ_X11)

static nsTArray<uint8_t> GetDisplayICCProfile(Display* dpy, Window& root) {
  const char kIccProfileAtomName[] = "_ICC_PROFILE";
  Atom iccAtom = XInternAtom(dpy, kIccProfileAtomName, TRUE);
  if (!iccAtom) {
    return nsTArray<uint8_t>();
  }

  Atom retAtom;
  int retFormat;
  unsigned long retLength, retAfter;
  unsigned char* retProperty;

  if (XGetWindowProperty(dpy, root, iccAtom, 0, INT_MAX /* length */, X11False,
                         AnyPropertyType, &retAtom, &retFormat, &retLength,
                         &retAfter, &retProperty) != Success) {
    return nsTArray<uint8_t>();
  }

  nsTArray<uint8_t> result;

  if (retLength > 0) {
    result.AppendElements(static_cast<uint8_t*>(retProperty), retLength);
  }

  XFree(retProperty);

  return result;
}

nsTArray<uint8_t> gfxPlatformGtk::GetPlatformCMSOutputProfileData() {
  nsTArray<uint8_t> prefProfileData = GetPrefCMSOutputProfileData();
  if (!prefProfileData.IsEmpty()) {
    return prefProfileData;
  }

  if (!mIsX11Display) {
    return nsTArray<uint8_t>();
  }

  GdkDisplay* display = gdk_display_get_default();
  Display* dpy = GDK_DISPLAY_XDISPLAY(display);
  // In xpcshell tests, we never initialize X and hence don't have a Display.
  // In this case, there's no output colour management to be done, so we just
  // return with nullptr.
  if (!dpy) {
    return nsTArray<uint8_t>();
  }

  Window root = gdk_x11_get_default_root_xwindow();

  // First try ICC Profile
  nsTArray<uint8_t> iccResult = GetDisplayICCProfile(dpy, root);
  if (!iccResult.IsEmpty()) {
    return iccResult;
  }

  // If ICC doesn't work, then try EDID
  const char kEdid1AtomName[] = "XFree86_DDC_EDID1_RAWDATA";
  Atom edidAtom = XInternAtom(dpy, kEdid1AtomName, TRUE);
  if (!edidAtom) {
    return nsTArray<uint8_t>();
  }

  Atom retAtom;
  int retFormat;
  unsigned long retLength, retAfter;
  unsigned char* retProperty;

  if (XGetWindowProperty(dpy, root, edidAtom, 0, 32, X11False, AnyPropertyType,
                         &retAtom, &retFormat, &retLength, &retAfter,
                         &retProperty) != Success) {
    return nsTArray<uint8_t>();
  }

  if (retLength != 128) {
    return nsTArray<uint8_t>();
  }

  // Format documented in "VESA E-EDID Implementation Guide"
  float gamma = (100 + retProperty[0x17]) / 100.0f;

  qcms_CIE_xyY whitePoint;
  whitePoint.x =
      ((retProperty[0x21] << 2) | (retProperty[0x1a] >> 2 & 3)) / 1024.0;
  whitePoint.y =
      ((retProperty[0x22] << 2) | (retProperty[0x1a] >> 0 & 3)) / 1024.0;
  whitePoint.Y = 1.0;

  qcms_CIE_xyYTRIPLE primaries;
  primaries.red.x =
      ((retProperty[0x1b] << 2) | (retProperty[0x19] >> 6 & 3)) / 1024.0;
  primaries.red.y =
      ((retProperty[0x1c] << 2) | (retProperty[0x19] >> 4 & 3)) / 1024.0;
  primaries.red.Y = 1.0;

  primaries.green.x =
      ((retProperty[0x1d] << 2) | (retProperty[0x19] >> 2 & 3)) / 1024.0;
  primaries.green.y =
      ((retProperty[0x1e] << 2) | (retProperty[0x19] >> 0 & 3)) / 1024.0;
  primaries.green.Y = 1.0;

  primaries.blue.x =
      ((retProperty[0x1f] << 2) | (retProperty[0x1a] >> 6 & 3)) / 1024.0;
  primaries.blue.y =
      ((retProperty[0x20] << 2) | (retProperty[0x1a] >> 4 & 3)) / 1024.0;
  primaries.blue.Y = 1.0;

  XFree(retProperty);

  void* mem = nullptr;
  size_t size = 0;
  qcms_data_create_rgb_with_gamma(whitePoint, primaries, gamma, &mem, &size);
  if (!mem) {
    return nsTArray<uint8_t>();
  }

  nsTArray<uint8_t> result;
  result.AppendElements(static_cast<uint8_t*>(mem), size);
  free(mem);

  return result;
}

#else  // defined(MOZ_X11)

nsTArray<uint8_t> gfxPlatformGtk::GetPlatformCMSOutputProfileData() {
  return nsTArray<uint8_t>();
}

#endif

bool gfxPlatformGtk::CheckVariationFontSupport() {
  // Although there was some variation/multiple-master support in FreeType
  // in older versions, it seems too incomplete/unstable for us to use
  // until at least 2.7.1.
  FT_Int major, minor, patch;
  FT_Library_Version(Factory::GetFTLibrary(), &major, &minor, &patch);
  return major * 1000000 + minor * 1000 + patch >= 2007001;
}

#ifdef MOZ_X11

class GtkVsyncSource final : public VsyncSource {
 public:
  GtkVsyncSource() {
    MOZ_ASSERT(NS_IsMainThread());
    mGlobalDisplay = new GLXDisplay();
  }

  virtual ~GtkVsyncSource() { MOZ_ASSERT(NS_IsMainThread()); }

  virtual Display& GetGlobalDisplay() override { return *mGlobalDisplay; }

  class GLXDisplay final : public VsyncSource::Display {
   public:
    GLXDisplay()
        : mGLContext(nullptr),
          mXDisplay(nullptr),
          mSetupLock("GLXVsyncSetupLock"),
          mVsyncThread("GLXVsyncThread"),
          mVsyncTask(nullptr),
          mVsyncEnabledLock("GLXVsyncEnabledLock"),
          mVsyncEnabled(false) {}

    // Sets up the display's GL context on a worker thread.
    // Required as GLContexts may only be used by the creating thread.
    // Returns true if setup was a success.
    bool Setup() {
      MonitorAutoLock lock(mSetupLock);
      MOZ_ASSERT(NS_IsMainThread());
      if (!mVsyncThread.Start()) return false;

      RefPtr<Runnable> vsyncSetup =
          NewRunnableMethod("GtkVsyncSource::GLXDisplay::SetupGLContext", this,
                            &GLXDisplay::SetupGLContext);
      mVsyncThread.message_loop()->PostTask(vsyncSetup.forget());
      // Wait until the setup has completed.
      lock.Wait();
      return mGLContext != nullptr;
    }

    // Called on the Vsync thread to setup the GL context.
    void SetupGLContext() {
      MonitorAutoLock lock(mSetupLock);
      MOZ_ASSERT(!NS_IsMainThread());
      MOZ_ASSERT(!mGLContext, "GLContext already setup!");

      // Create video sync timer on a separate Display to prevent locking the
      // main thread X display.
      mXDisplay = XOpenDisplay(nullptr);
      if (!mXDisplay) {
        lock.NotifyAll();
        return;
      }

      // Most compositors wait for vsync events on the root window.
      Window root = DefaultRootWindow(mXDisplay);
      int screen = DefaultScreen(mXDisplay);

      ScopedXFree<GLXFBConfig> cfgs;
      GLXFBConfig config;
      int visid;
      bool forWebRender = false;
      if (!gl::GLContextGLX::FindFBConfigForWindow(
              mXDisplay, screen, root, &cfgs, &config, &visid, forWebRender)) {
        lock.NotifyAll();
        return;
      }

      mGLContext = gl::GLContextGLX::CreateGLContext({}, mXDisplay, root,
                                                     config, false, nullptr);

      if (!mGLContext) {
        lock.NotifyAll();
        return;
      }

      mGLContext->MakeCurrent();

      // Test that SGI_video_sync lets us get the counter.
      unsigned int syncCounter = 0;
      if (gl::sGLXLibrary.fGetVideoSync(&syncCounter) != 0) {
        mGLContext = nullptr;
      }

      lock.NotifyAll();
    }

    virtual void EnableVsync() override {
      MOZ_ASSERT(NS_IsMainThread());
      MOZ_ASSERT(mGLContext, "GLContext not setup!");

      MonitorAutoLock lock(mVsyncEnabledLock);
      if (mVsyncEnabled) {
        return;
      }
      mVsyncEnabled = true;

      // If the task has not nulled itself out, it hasn't yet realized
      // that vsync was disabled earlier, so continue its execution.
      if (!mVsyncTask) {
        mVsyncTask = NewRunnableMethod("GtkVsyncSource::GLXDisplay::RunVsync",
                                       this, &GLXDisplay::RunVsync);
        RefPtr<Runnable> addrefedTask = mVsyncTask;
        mVsyncThread.message_loop()->PostTask(addrefedTask.forget());
      }
    }

    virtual void DisableVsync() override {
      MonitorAutoLock lock(mVsyncEnabledLock);
      mVsyncEnabled = false;
    }

    virtual bool IsVsyncEnabled() override {
      MonitorAutoLock lock(mVsyncEnabledLock);
      return mVsyncEnabled;
    }

    virtual void Shutdown() override {
      MOZ_ASSERT(NS_IsMainThread());
      DisableVsync();

      // Cleanup thread-specific resources before shutting down.
      RefPtr<Runnable> shutdownTask = NewRunnableMethod(
          "GtkVsyncSource::GLXDisplay::Cleanup", this, &GLXDisplay::Cleanup);
      mVsyncThread.message_loop()->PostTask(shutdownTask.forget());

      // Stop, waiting for the cleanup task to finish execution.
      mVsyncThread.Stop();
    }

   private:
    virtual ~GLXDisplay() = default;

    void RunVsync() {
      MOZ_ASSERT(!NS_IsMainThread());

      mGLContext->MakeCurrent();

      unsigned int syncCounter = 0;
      gl::sGLXLibrary.fGetVideoSync(&syncCounter);
      for (;;) {
        {
          MonitorAutoLock lock(mVsyncEnabledLock);
          if (!mVsyncEnabled) {
            mVsyncTask = nullptr;
            return;
          }
        }

        TimeStamp lastVsync = TimeStamp::Now();
        bool useSoftware = false;

        // Wait until the video sync counter reaches the next value by waiting
        // until the parity of the counter value changes.
        unsigned int nextSync = syncCounter + 1;
        int status;
        if ((status = gl::sGLXLibrary.fWaitVideoSync(2, nextSync % 2,
                                                     &syncCounter)) != 0) {
          gfxWarningOnce() << "glXWaitVideoSync returned " << status;
          useSoftware = true;
        }

        if (syncCounter == (nextSync - 1)) {
          gfxWarningOnce()
              << "glXWaitVideoSync failed to increment the sync counter.";
          useSoftware = true;
        }

        if (useSoftware) {
          double remaining =
              (1000.f / 60.f) - (TimeStamp::Now() - lastVsync).ToMilliseconds();
          if (remaining > 0) {
            PlatformThread::Sleep(remaining);
          }
        }

        lastVsync = TimeStamp::Now();
        TimeStamp outputTime = lastVsync + GetVsyncRate();
        NotifyVsync(lastVsync, outputTime);
      }
    }

    void Cleanup() {
      MOZ_ASSERT(!NS_IsMainThread());

      mGLContext = nullptr;
      if (mXDisplay) XCloseDisplay(mXDisplay);
    }

    // Owned by the vsync thread.
    RefPtr<gl::GLContextGLX> mGLContext;
    _XDisplay* mXDisplay;
    Monitor mSetupLock;
    base::Thread mVsyncThread;
    RefPtr<Runnable> mVsyncTask;
    Monitor mVsyncEnabledLock;
    bool mVsyncEnabled;
  };

 private:
  // We need a refcounted VsyncSource::Display to use chromium IPC runnables.
  RefPtr<GLXDisplay> mGlobalDisplay;
};

already_AddRefed<gfx::VsyncSource> gfxPlatformGtk::CreateHardwareVsyncSource() {
#  ifdef MOZ_WAYLAND
  if (IsWaylandDisplay()) {
    // For wayland, we simply return the standard software vsync for now.
    // This powers refresh drivers and the likes.
    return gfxPlatform::CreateHardwareVsyncSource();
  }
#  endif

  // Only use GLX vsync when the OpenGL compositor / WebRedner is being used.
  // The extra cost of initializing a GLX context while blocking the main
  // thread is not worth it when using basic composition.
  //
  // Don't call gl::sGLXLibrary.SupportsVideoSync() when EGL is used.
  // NVIDIA drivers refuse to use EGL GL context when GLX was initialized first
  // and fail silently.
  if (gfxConfig::IsEnabled(Feature::HW_COMPOSITING)) {
    bool useGlxVsync = false;

    nsCOMPtr<nsIGfxInfo> gfxInfo = components::GfxInfo::Service();
    nsString adapterDriverVendor;
    gfxInfo->GetAdapterDriverVendor(adapterDriverVendor);

    // Nvidia doesn't support GLX at the same time as EGL but Mesa does.
    if (!gfxVars::UseEGL() || (adapterDriverVendor.Find("mesa") != -1)) {
      useGlxVsync = gl::sGLXLibrary.SupportsVideoSync();
    }
    if (useGlxVsync) {
      RefPtr<VsyncSource> vsyncSource = new GtkVsyncSource();
      VsyncSource::Display& display = vsyncSource->GetGlobalDisplay();
      if (!static_cast<GtkVsyncSource::GLXDisplay&>(display).Setup()) {
        NS_WARNING(
            "Failed to setup GLContext, falling back to software vsync.");
        return gfxPlatform::CreateHardwareVsyncSource();
      }
      return vsyncSource.forget();
    }
    NS_WARNING("SGI_video_sync unsupported. Falling back to software vsync.");
  }
  return gfxPlatform::CreateHardwareVsyncSource();
}

#endif
