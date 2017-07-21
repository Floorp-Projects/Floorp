/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define PANGO_ENABLE_BACKEND
#define PANGO_ENABLE_ENGINE

#include "gfxPlatformGtk.h"
#include "prenv.h"

#include "nsUnicharUtils.h"
#include "nsUnicodeProperties.h"
#include "gfx2DGlue.h"
#include "gfxFcPlatformFontList.h"
#include "gfxFontconfigUtils.h"
#include "gfxFontconfigFonts.h"
#include "gfxConfig.h"
#include "gfxContext.h"
#include "gfxUserFontSet.h"
#include "gfxUtils.h"
#include "gfxFT2FontBase.h"
#include "gfxPrefs.h"
#include "VsyncSource.h"
#include "mozilla/Atomics.h"
#include "mozilla/Monitor.h"
#include "base/task.h"
#include "base/thread.h"
#include "base/message_loop.h"
#include "mozilla/gfx/Logging.h"

#include "mozilla/gfx/2D.h"

#include "cairo.h"
#include <gtk/gtk.h>

#include "gfxImageSurface.h"
#ifdef MOZ_X11
#include <gdk/gdkx.h>
#include "gfxXlibSurface.h"
#include "cairo-xlib.h"
#include "mozilla/Preferences.h"
#include "mozilla/X11Util.h"

#ifdef GL_PROVIDER_GLX
#include "GLContextProvider.h"
#include "GLContextGLX.h"
#include "GLXLibrary.h"
#endif

/* Undefine the Status from Xlib since it will conflict with system headers on OSX */
#if defined(__APPLE__) && defined(Status)
#undef Status
#endif

#endif /* MOZ_X11 */

#include <fontconfig/fontconfig.h>

#include "nsMathUtils.h"

#define GDK_PIXMAP_SIZE_MAX 32767

#define GFX_PREF_MAX_GENERIC_SUBSTITUTIONS "gfx.font_rendering.fontconfig.max_generic_substitutions"

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::unicode;

#if (MOZ_WIDGET_GTK == 2)
static cairo_user_data_key_t cairo_gdk_drawable_key;
#endif

gfxPlatformGtk::gfxPlatformGtk()
{
    if (!gfxPlatform::IsHeadless()) {
        gtk_init(nullptr, nullptr);
    }

    mMaxGenericSubstitutions = UNINITIALIZED_VALUE;

#ifdef MOZ_X11
    if (!gfxPlatform::IsHeadless() && XRE_IsParentProcess()) {
      if (GDK_IS_X11_DISPLAY(gdk_display_get_default()) &&
          mozilla::Preferences::GetBool("gfx.xrender.enabled"))
      {
          gfxVars::SetUseXRender(true);
      }
    }
#endif

    uint32_t canvasMask = BackendTypeBit(BackendType::CAIRO);
    uint32_t contentMask = BackendTypeBit(BackendType::CAIRO);
#ifdef USE_SKIA
    canvasMask |= BackendTypeBit(BackendType::SKIA);
    contentMask |= BackendTypeBit(BackendType::SKIA);
#endif
    InitBackendPrefs(canvasMask, BackendType::CAIRO,
                     contentMask, BackendType::CAIRO);

#ifdef MOZ_X11
    if (gfxPlatform::IsHeadless() && GDK_IS_X11_DISPLAY(gdk_display_get_default())) {
      mCompositorDisplay = XOpenDisplay(nullptr);
      MOZ_ASSERT(mCompositorDisplay, "Failed to create compositor display!");
    } else {
      mCompositorDisplay = nullptr;
    }
#endif // MOZ_X11
}

gfxPlatformGtk::~gfxPlatformGtk()
{
#ifdef MOZ_X11
    if (mCompositorDisplay) {
      XCloseDisplay(mCompositorDisplay);
    }
#endif // MOZ_X11
}

void
gfxPlatformGtk::FlushContentDrawing()
{
    if (gfxVars::UseXRender()) {
        XFlush(DefaultXDisplay());
    }
}

already_AddRefed<gfxASurface>
gfxPlatformGtk::CreateOffscreenSurface(const IntSize& aSize,
                                       gfxImageFormat aFormat)
{
    if (!Factory::AllowedSurfaceSize(aSize)) {
        return nullptr;
    }

    RefPtr<gfxASurface> newSurface;
    bool needsClear = true;
#ifdef MOZ_X11
    // XXX we really need a different interface here, something that passes
    // in more context, including the display and/or target surface type that
    // we should try to match
    GdkScreen *gdkScreen = gdk_screen_get_default();
    if (gdkScreen) {
        // When forcing PaintedLayers to use image surfaces for content,
        // force creation of gfxImageSurface surfaces.
        if (gfxVars::UseXRender() && !UseImageOffscreenSurfaces()) {
            Screen *screen = gdk_x11_screen_get_xscreen(gdkScreen);
            XRenderPictFormat* xrenderFormat =
                gfxXlibSurface::FindRenderFormat(DisplayOfScreen(screen),
                                                 aFormat);

            if (xrenderFormat) {
                newSurface = gfxXlibSurface::Create(screen, xrenderFormat,
                                                    aSize);
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
        newSurface = nullptr; // surface isn't valid for some reason
    }

    if (newSurface && needsClear) {
        gfxUtils::ClearThebesSurface(newSurface);
    }

    return newSurface.forget();
}

nsresult
gfxPlatformGtk::GetFontList(nsIAtom *aLangGroup,
                            const nsACString& aGenericFamily,
                            nsTArray<nsString>& aListOfFonts)
{
    gfxPlatformFontList::PlatformFontList()->GetFontList(aLangGroup,
                                                         aGenericFamily,
                                                         aListOfFonts);
    return NS_OK;
}

nsresult
gfxPlatformGtk::UpdateFontList()
{
    gfxPlatformFontList::PlatformFontList()->UpdateFontList();
    return NS_OK;
}

// xxx - this is ubuntu centric, need to go through other distros and flesh
// out a more general list
static const char kFontDejaVuSans[] = "DejaVu Sans";
static const char kFontDejaVuSerif[] = "DejaVu Serif";
static const char kFontEmojiOneMozilla[] = "EmojiOne Mozilla";
static const char kFontFreeSans[] = "FreeSans";
static const char kFontFreeSerif[] = "FreeSerif";
static const char kFontTakaoPGothic[] = "TakaoPGothic";
static const char kFontDroidSansFallback[] = "Droid Sans Fallback";
static const char kFontWenQuanYiMicroHei[] = "WenQuanYi Micro Hei";
static const char kFontNanumGothic[] = "NanumGothic";

void
gfxPlatformGtk::GetCommonFallbackFonts(uint32_t aCh, uint32_t aNextCh,
                                       Script aRunScript,
                                       nsTArray<const char*>& aFontList)
{
    if (aNextCh == 0xfe0fu) {
      // if char is followed by VS16, try for a color emoji glyph
      aFontList.AppendElement(kFontEmojiOneMozilla);
    }

    aFontList.AppendElement(kFontDejaVuSerif);
    aFontList.AppendElement(kFontFreeSerif);
    aFontList.AppendElement(kFontDejaVuSans);
    aFontList.AppendElement(kFontFreeSans);

    if (!IS_IN_BMP(aCh)) {
        uint32_t p = aCh >> 16;
        if (p == 1) { // try color emoji font, unless VS15 (text style) present
            if (aNextCh != 0xfe0fu && aNextCh != 0xfe0eu) {
                aFontList.AppendElement(kFontEmojiOneMozilla);
            }
        }
    }

    // add fonts for CJK ranges
    // xxx - this isn't really correct, should use the same CJK font ordering
    // as the pref font code
    if (aCh >= 0x3000 &&
        ((aCh < 0xe000) ||
         (aCh >= 0xf900 && aCh < 0xfff0) ||
         ((aCh >> 16) == 2))) {
        aFontList.AppendElement(kFontTakaoPGothic);
        aFontList.AppendElement(kFontDroidSansFallback);
        aFontList.AppendElement(kFontWenQuanYiMicroHei);
        aFontList.AppendElement(kFontNanumGothic);
    }
}

gfxPlatformFontList*
gfxPlatformGtk::CreatePlatformFontList()
{
    gfxPlatformFontList* list = new gfxFcPlatformFontList();
    if (NS_SUCCEEDED(list->InitFontList())) {
        return list;
    }
    gfxPlatformFontList::Shutdown();
    return nullptr;
}

nsresult
gfxPlatformGtk::GetStandardFamilyName(const nsAString& aFontName, nsAString& aFamilyName)
{
    gfxPlatformFontList::PlatformFontList()->
        GetStandardFamilyName(aFontName, aFamilyName);
    return NS_OK;
}

gfxFontGroup *
gfxPlatformGtk::CreateFontGroup(const FontFamilyList& aFontFamilyList,
                                const gfxFontStyle* aStyle,
                                gfxTextPerfMetrics* aTextPerf,
                                gfxUserFontSet* aUserFontSet,
                                gfxFloat aDevToCssSize)
{
    return new gfxFontGroup(aFontFamilyList, aStyle, aTextPerf,
                            aUserFontSet, aDevToCssSize);
}

gfxFontEntry*
gfxPlatformGtk::LookupLocalFont(const nsAString& aFontName,
                                uint16_t aWeight,
                                int16_t aStretch,
                                uint8_t aStyle)
{
    gfxPlatformFontList* pfl = gfxPlatformFontList::PlatformFontList();
    return pfl->LookupLocalFont(aFontName, aWeight, aStretch,
                                aStyle);
}

gfxFontEntry*
gfxPlatformGtk::MakePlatformFont(const nsAString& aFontName,
                                 uint16_t aWeight,
                                 int16_t aStretch,
                                 uint8_t aStyle,
                                 const uint8_t* aFontData,
                                 uint32_t aLength)
{
    gfxPlatformFontList* pfl = gfxPlatformFontList::PlatformFontList();
    return pfl->MakePlatformFont(aFontName, aWeight, aStretch,
                                 aStyle, aFontData, aLength);
}

FT_Library
gfxPlatformGtk::GetFTLibrary()
{
    return gfxFcPlatformFontList::GetFTLibrary();
}

static int32_t sDPI = 0;

int32_t
gfxPlatformGtk::GetFontScaleDPI()
{
    if (!sDPI) {
        // Make sure init is run so we have a resolution
        GdkScreen *screen = gdk_screen_get_default();
        gtk_settings_get_for_screen(screen);
        sDPI = int32_t(round(gdk_screen_get_resolution(screen)));
        if (sDPI <= 0) {
            // Fall back to something sane
            sDPI = 96;
        }
    }
    return sDPI;
}

double
gfxPlatformGtk::GetFontScaleFactor()
{
    // Integer scale factors work well with GTK window scaling, image scaling,
    // and pixel alignment, but there is a range where 1 is too small and 2 is
    // too big.  An additional step of 1.5 is added because this is common
    // scale on WINNT and at this ratio the advantages of larger rendering
    // outweigh the disadvantages from scaling and pixel mis-alignment.
    int32_t dpi = GetFontScaleDPI();
    if (dpi < 132) {
        return 1.0;
    }
    if (dpi < 168) {
        return 1.5;
    }
    return round(dpi/96.0);

}

bool
gfxPlatformGtk::UseImageOffscreenSurfaces()
{
    return GetDefaultContentBackend() != mozilla::gfx::BackendType::CAIRO ||
           gfxPrefs::UseImageOffscreenSurfaces();
}

gfxImageFormat
gfxPlatformGtk::GetOffscreenFormat()
{
    // Make sure there is a screen
    GdkScreen *screen = gdk_screen_get_default();
    if (screen && gdk_visual_get_depth(gdk_visual_get_system()) == 16) {
        return SurfaceFormat::R5G6B5_UINT16;
    }

    return SurfaceFormat::X8R8G8B8_UINT32;
}

void gfxPlatformGtk::FontsPrefsChanged(const char *aPref)
{
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

uint32_t gfxPlatformGtk::MaxGenericSubstitions()
{
    if (mMaxGenericSubstitutions == UNINITIALIZED_VALUE) {
        mMaxGenericSubstitutions =
            Preferences::GetInt(GFX_PREF_MAX_GENERIC_SUBSTITUTIONS, 3);
        if (mMaxGenericSubstitutions < 0) {
            mMaxGenericSubstitutions = 3;
        }
    }

    return uint32_t(mMaxGenericSubstitutions);
}

void
gfxPlatformGtk::GetPlatformCMSOutputProfile(void *&mem, size_t &size)
{
    mem = nullptr;
    size = 0;

#ifdef MOZ_X11
    GdkDisplay *display = gdk_display_get_default();
    if (!GDK_IS_X11_DISPLAY(display))
        return;

    const char EDID1_ATOM_NAME[] = "XFree86_DDC_EDID1_RAWDATA";
    const char ICC_PROFILE_ATOM_NAME[] = "_ICC_PROFILE";

    Atom edidAtom, iccAtom;
    Display *dpy = GDK_DISPLAY_XDISPLAY(display);
    // In xpcshell tests, we never initialize X and hence don't have a Display.
    // In this case, there's no output colour management to be done, so we just
    // return with nullptr.
    if (!dpy)
        return;

    Window root = gdk_x11_get_default_root_xwindow();

    Atom retAtom;
    int retFormat;
    unsigned long retLength, retAfter;
    unsigned char *retProperty ;

    iccAtom = XInternAtom(dpy, ICC_PROFILE_ATOM_NAME, TRUE);
    if (iccAtom) {
        // read once to get size, once for the data
        if (Success == XGetWindowProperty(dpy, root, iccAtom,
                                          0, INT_MAX /* length */,
                                          False, AnyPropertyType,
                                          &retAtom, &retFormat, &retLength,
                                          &retAfter, &retProperty)) {

            if (retLength > 0) {
                void *buffer = malloc(retLength);
                if (buffer) {
                    memcpy(buffer, retProperty, retLength);
                    mem = buffer;
                    size = retLength;
                }
            }

            XFree(retProperty);
            if (size > 0) {
#ifdef DEBUG_tor
                fprintf(stderr,
                        "ICM profile read from %s successfully\n",
                        ICC_PROFILE_ATOM_NAME);
#endif
                return;
            }
        }
    }

    edidAtom = XInternAtom(dpy, EDID1_ATOM_NAME, TRUE);
    if (edidAtom) {
        if (Success == XGetWindowProperty(dpy, root, edidAtom, 0, 32,
                                          False, AnyPropertyType,
                                          &retAtom, &retFormat, &retLength,
                                          &retAfter, &retProperty)) {
            double gamma;
            qcms_CIE_xyY whitePoint;
            qcms_CIE_xyYTRIPLE primaries;

            if (retLength != 128) {
#ifdef DEBUG_tor
                fprintf(stderr, "Short EDID data\n");
#endif
                return;
            }

            // Format documented in "VESA E-EDID Implementation Guide"

            gamma = (100 + retProperty[0x17]) / 100.0;
            whitePoint.x = ((retProperty[0x21] << 2) |
                            (retProperty[0x1a] >> 2 & 3)) / 1024.0;
            whitePoint.y = ((retProperty[0x22] << 2) |
                            (retProperty[0x1a] >> 0 & 3)) / 1024.0;
            whitePoint.Y = 1.0;

            primaries.red.x = ((retProperty[0x1b] << 2) |
                               (retProperty[0x19] >> 6 & 3)) / 1024.0;
            primaries.red.y = ((retProperty[0x1c] << 2) |
                               (retProperty[0x19] >> 4 & 3)) / 1024.0;
            primaries.red.Y = 1.0;

            primaries.green.x = ((retProperty[0x1d] << 2) |
                                 (retProperty[0x19] >> 2 & 3)) / 1024.0;
            primaries.green.y = ((retProperty[0x1e] << 2) |
                                 (retProperty[0x19] >> 0 & 3)) / 1024.0;
            primaries.green.Y = 1.0;

            primaries.blue.x = ((retProperty[0x1f] << 2) |
                               (retProperty[0x1a] >> 6 & 3)) / 1024.0;
            primaries.blue.y = ((retProperty[0x20] << 2) |
                               (retProperty[0x1a] >> 4 & 3)) / 1024.0;
            primaries.blue.Y = 1.0;

            XFree(retProperty);

#ifdef DEBUG_tor
            fprintf(stderr, "EDID gamma: %f\n", gamma);
            fprintf(stderr, "EDID whitepoint: %f %f %f\n",
                    whitePoint.x, whitePoint.y, whitePoint.Y);
            fprintf(stderr, "EDID primaries: [%f %f %f] [%f %f %f] [%f %f %f]\n",
                    primaries.Red.x, primaries.Red.y, primaries.Red.Y,
                    primaries.Green.x, primaries.Green.y, primaries.Green.Y,
                    primaries.Blue.x, primaries.Blue.y, primaries.Blue.Y);
#endif

            qcms_data_create_rgb_with_gamma(whitePoint, primaries, gamma, &mem, &size);

#ifdef DEBUG_tor
            if (size > 0) {
                fprintf(stderr,
                        "ICM profile read from %s successfully\n",
                        EDID1_ATOM_NAME);
            }
#endif
        }
    }
#endif
}


#if (MOZ_WIDGET_GTK == 2)
void
gfxPlatformGtk::SetGdkDrawable(cairo_surface_t *target,
                               GdkDrawable *drawable)
{
    if (cairo_surface_status(target))
        return;

    g_object_ref(drawable);

    cairo_surface_set_user_data (target,
                                 &cairo_gdk_drawable_key,
                                 drawable,
                                 g_object_unref);
}

GdkDrawable *
gfxPlatformGtk::GetGdkDrawable(cairo_surface_t *target)
{
    if (cairo_surface_status(target))
        return nullptr;

    GdkDrawable *result;

    result = (GdkDrawable*) cairo_surface_get_user_data (target,
                                                         &cairo_gdk_drawable_key);
    if (result)
        return result;

#ifdef MOZ_X11
    if (cairo_surface_get_type(target) != CAIRO_SURFACE_TYPE_XLIB)
        return nullptr;

    // try looking it up in gdk's table
    result = (GdkDrawable*) gdk_xid_table_lookup(cairo_xlib_surface_get_drawable(target));
    if (result) {
        SetGdkDrawable(target, result);
        return result;
    }
#endif

    return nullptr;
}
#endif

already_AddRefed<ScaledFont>
gfxPlatformGtk::GetScaledFontForFont(DrawTarget* aTarget, gfxFont *aFont)
{
  if (aFont->GetType() == gfxFont::FONT_TYPE_FONTCONFIG) {
      gfxFontconfigFontBase* fcFont = static_cast<gfxFontconfigFontBase*>(aFont);
      return Factory::CreateScaledFontForFontconfigFont(
              fcFont->GetCairoScaledFont(),
              fcFont->GetPattern(),
              fcFont->GetUnscaledFont(),
              fcFont->GetAdjustedSize());
  }
  return GetScaledFontForFontWithCairoSkia(aTarget, aFont);
}

#ifdef GL_PROVIDER_GLX

class GLXVsyncSource final : public VsyncSource
{
public:
  GLXVsyncSource()
  {
    MOZ_ASSERT(NS_IsMainThread());
    mGlobalDisplay = new GLXDisplay();
  }

  virtual ~GLXVsyncSource()
  {
    MOZ_ASSERT(NS_IsMainThread());
  }

  virtual Display& GetGlobalDisplay() override
  {
    return *mGlobalDisplay;
  }

  class GLXDisplay final : public VsyncSource::Display
  {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(GLXDisplay)

  public:
    GLXDisplay() : mGLContext(nullptr)
                 , mXDisplay(nullptr)
                 , mSetupLock("GLXVsyncSetupLock")
                 , mVsyncThread("GLXVsyncThread")
                 , mVsyncTask(nullptr)
                 , mVsyncEnabledLock("GLXVsyncEnabledLock")
                 , mVsyncEnabled(false)
    {
    }

    // Sets up the display's GL context on a worker thread.
    // Required as GLContexts may only be used by the creating thread.
    // Returns true if setup was a success.
    bool Setup()
    {
      MonitorAutoLock lock(mSetupLock);
      MOZ_ASSERT(NS_IsMainThread());
      if (!mVsyncThread.Start())
        return false;

      RefPtr<Runnable> vsyncSetup =
        NewRunnableMethod("GLXVsyncSource::GLXDisplay::SetupGLContext",
                          this,
                          &GLXDisplay::SetupGLContext);
      mVsyncThread.message_loop()->PostTask(vsyncSetup.forget());
      // Wait until the setup has completed.
      lock.Wait();
      return mGLContext != nullptr;
    }

    // Called on the Vsync thread to setup the GL context.
    void SetupGLContext()
    {
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
        if (!gl::GLContextGLX::FindFBConfigForWindow(mXDisplay, screen, root,
                                                     &cfgs, &config, &visid,
                                                     forWebRender)) {
          lock.NotifyAll();
          return;
        }

        mGLContext = gl::GLContextGLX::CreateGLContext(gl::CreateContextFlags::NONE,
                                                       gl::SurfaceCaps::Any(), false,
                                                       mXDisplay, root, config, false,
                                                       nullptr);

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

    virtual void EnableVsync() override
    {
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
        mVsyncTask = NewRunnableMethod(
          "GLXVsyncSource::GLXDisplay::RunVsync", this, &GLXDisplay::RunVsync);
        RefPtr<Runnable> addrefedTask = mVsyncTask;
        mVsyncThread.message_loop()->PostTask(addrefedTask.forget());
      }
    }

    virtual void DisableVsync() override
    {
      MonitorAutoLock lock(mVsyncEnabledLock);
      mVsyncEnabled = false;
    }

    virtual bool IsVsyncEnabled() override
    {
      MonitorAutoLock lock(mVsyncEnabledLock);
      return mVsyncEnabled;
    }

    virtual void Shutdown() override
    {
      MOZ_ASSERT(NS_IsMainThread());
      DisableVsync();

      // Cleanup thread-specific resources before shutting down.
      RefPtr<Runnable> shutdownTask = NewRunnableMethod(
        "GLXVsyncSource::GLXDisplay::Cleanup", this, &GLXDisplay::Cleanup);
      mVsyncThread.message_loop()->PostTask(shutdownTask.forget());

      // Stop, waiting for the cleanup task to finish execution.
      mVsyncThread.Stop();
    }

  private:
    virtual ~GLXDisplay()
    {
    }

    void RunVsync()
    {
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
        if ((status = gl::sGLXLibrary.fWaitVideoSync(2, nextSync % 2, &syncCounter)) != 0) {
          gfxWarningOnce() << "glXWaitVideoSync returned " << status;
          useSoftware = true;
        }

        if (syncCounter == (nextSync - 1)) {
          gfxWarningOnce() << "glXWaitVideoSync failed to increment the sync counter.";
          useSoftware = true;
        }

        if (useSoftware) {
          double remaining = (1000.f / 60.f) -
            (TimeStamp::Now() - lastVsync).ToMilliseconds();
          if (remaining > 0) {
            PlatformThread::Sleep(remaining);
          }
        }

        lastVsync = TimeStamp::Now();
        NotifyVsync(lastVsync);
      }
    }

    void Cleanup() {
      MOZ_ASSERT(!NS_IsMainThread());

      mGLContext = nullptr;
      XCloseDisplay(mXDisplay);
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

already_AddRefed<gfx::VsyncSource>
gfxPlatformGtk::CreateHardwareVsyncSource()
{
  // Only use GLX vsync when the OpenGL compositor is being used.
  // The extra cost of initializing a GLX context while blocking the main
  // thread is not worth it when using basic composition.
  if (gfxConfig::IsEnabled(Feature::HW_COMPOSITING)) {
    if (gl::sGLXLibrary.SupportsVideoSync()) {
      RefPtr<VsyncSource> vsyncSource = new GLXVsyncSource();
      VsyncSource::Display& display = vsyncSource->GetGlobalDisplay();
      if (!static_cast<GLXVsyncSource::GLXDisplay&>(display).Setup()) {
        NS_WARNING("Failed to setup GLContext, falling back to software vsync.");
        return gfxPlatform::CreateHardwareVsyncSource();
      }
      return vsyncSource.forget();
    }
    NS_WARNING("SGI_video_sync unsupported. Falling back to software vsync.");
  }
  return gfxPlatform::CreateHardwareVsyncSource();
}

#endif
