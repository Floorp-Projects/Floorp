/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef MOZ_LOGGING
#define FORCE_PR_LOG /* Allow logging in the release build */
#endif

#include "mozilla/layers/CompositorChild.h"
#include "mozilla/layers/CompositorParent.h"
#include "mozilla/layers/ImageBridgeChild.h"

#include "prlog.h"
#include "prenv.h"

#include "gfxPlatform.h"

#include "nsXULAppAPI.h"

#if defined(XP_WIN)
#include "gfxWindowsPlatform.h"
#include "gfxD2DSurface.h"
#elif defined(XP_MACOSX)
#include "gfxPlatformMac.h"
#elif defined(MOZ_WIDGET_GTK)
#include "gfxPlatformGtk.h"
#elif defined(MOZ_WIDGET_QT)
#include "gfxQtPlatform.h"
#elif defined(XP_OS2)
#include "gfxOS2Platform.h"
#elif defined(ANDROID)
#include "gfxAndroidPlatform.h"
#endif

#include "nsGkAtoms.h"
#include "gfxPlatformFontList.h"
#include "gfxContext.h"
#include "gfxImageSurface.h"
#include "gfxUserFontSet.h"
#include "nsUnicodeProperties.h"
#include "harfbuzz/hb.h"
#include "gfxGraphiteShaper.h"

#include "nsUnicodeRange.h"
#include "nsServiceManagerUtils.h"
#include "nsTArray.h"
#include "nsUnicharUtilCIID.h"
#include "nsILocaleService.h"
#include "nsReadableUtils.h"

#include "nsWeakReference.h"

#include "cairo.h"
#include "qcms.h"

#include "plstr.h"
#include "nsCRT.h"
#include "GLContext.h"
#include "GLContextProvider.h"

#ifdef MOZ_WIDGET_ANDROID
#include "TexturePoolOGL.h"
#endif

#ifdef USE_SKIA_GPU
#include "skia/GrContext.h"
#include "skia/GrGLInterface.h"
#include "GLContextSkia.h"
#endif

#include "mozilla/Preferences.h"
#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"

#include "nsIGfxInfo.h"

using namespace mozilla;
using namespace mozilla::layers;

gfxPlatform *gPlatform = nullptr;
static bool gEverInitialized = false;

// These two may point to the same profile
static qcms_profile *gCMSOutputProfile = nullptr;
static qcms_profile *gCMSsRGBProfile = nullptr;

static qcms_transform *gCMSRGBTransform = nullptr;
static qcms_transform *gCMSInverseRGBTransform = nullptr;
static qcms_transform *gCMSRGBATransform = nullptr;

static bool gCMSInitialized = false;
static eCMSMode gCMSMode = eCMSMode_Off;
static int gCMSIntent = -2;

static void ShutdownCMS();
static void MigratePrefs();

static bool sDrawLayerBorders = false;

#include "mozilla/gfx/2D.h"
using namespace mozilla::gfx;

// logs shared across gfx
#ifdef PR_LOGGING
static PRLogModuleInfo *sFontlistLog = nullptr;
static PRLogModuleInfo *sFontInitLog = nullptr;
static PRLogModuleInfo *sTextrunLog = nullptr;
static PRLogModuleInfo *sTextrunuiLog = nullptr;
static PRLogModuleInfo *sCmapDataLog = nullptr;
#endif

/* Class to listen for pref changes so that chrome code can dynamically
   force sRGB as an output profile. See Bug #452125. */
class SRGBOverrideObserver MOZ_FINAL : public nsIObserver,
                                       public nsSupportsWeakReference
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIOBSERVER
};

NS_IMPL_ISUPPORTS2(SRGBOverrideObserver, nsIObserver, nsISupportsWeakReference)

NS_IMETHODIMP
SRGBOverrideObserver::Observe(nsISupports *aSubject,
                              const char *aTopic,
                              const PRUnichar *someData)
{
    NS_ASSERTION(NS_strcmp(someData,
                   NS_LITERAL_STRING("gfx.color_mangement.force_srgb").get()),
                 "Restarting CMS on wrong pref!");
    ShutdownCMS();
    return NS_OK;
}

#define GFX_DOWNLOADABLE_FONTS_ENABLED "gfx.downloadable_fonts.enabled"

#define GFX_PREF_HARFBUZZ_SCRIPTS "gfx.font_rendering.harfbuzz.scripts"
#define HARFBUZZ_SCRIPTS_DEFAULT  mozilla::unicode::SHAPING_DEFAULT
#define GFX_PREF_FALLBACK_USE_CMAPS  "gfx.font_rendering.fallback.always_use_cmaps"

#define GFX_PREF_OPENTYPE_SVG "gfx.font_rendering.opentype_svg.enabled"

#define GFX_PREF_GRAPHITE_SHAPING "gfx.font_rendering.graphite.enabled"

#define BIDI_NUMERAL_PREF "bidi.numeral"

static const char* kObservedPrefs[] = {
    "gfx.downloadable_fonts.",
    "gfx.font_rendering.",
    "bidi.numeral",
    nullptr
};

class FontPrefsObserver MOZ_FINAL : public nsIObserver
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIOBSERVER
};

NS_IMPL_ISUPPORTS1(FontPrefsObserver, nsIObserver)

NS_IMETHODIMP
FontPrefsObserver::Observe(nsISupports *aSubject,
                           const char *aTopic,
                           const PRUnichar *someData)
{
    if (!someData) {
        NS_ERROR("font pref observer code broken");
        return NS_ERROR_UNEXPECTED;
    }
    NS_ASSERTION(gfxPlatform::GetPlatform(), "the singleton instance has gone");
    gfxPlatform::GetPlatform()->FontsPrefsChanged(NS_ConvertUTF16toUTF8(someData).get());

    return NS_OK;
}

class OrientationSyncPrefsObserver MOZ_FINAL : public nsIObserver
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIOBSERVER
};

NS_IMPL_ISUPPORTS1(OrientationSyncPrefsObserver, nsIObserver)

NS_IMETHODIMP
OrientationSyncPrefsObserver::Observe(nsISupports *aSubject,
                                      const char *aTopic,
                                      const PRUnichar *someData)
{
    if (!someData) {
        NS_ERROR("orientation sync pref observer broken");
        return NS_ERROR_UNEXPECTED;
    }
    NS_ASSERTION(gfxPlatform::GetPlatform(), "the singleton instance has gone");
    gfxPlatform::GetPlatform()->OrientationSyncPrefsObserverChanged();

    return NS_OK;
}

// this needs to match the list of pref font.default.xx entries listed in all.js!
// the order *must* match the order in eFontPrefLang
static const char *gPrefLangNames[] = {
    "x-western",
    "x-central-euro",
    "ja",
    "zh-TW",
    "zh-CN",
    "zh-HK",
    "ko",
    "x-cyrillic",
    "x-baltic",
    "el",
    "tr",
    "th",
    "he",
    "ar",
    "x-devanagari",
    "x-tamil",
    "x-armn",
    "x-beng",
    "x-cans",
    "x-ethi",
    "x-geor",
    "x-gujr",
    "x-guru",
    "x-khmr",
    "x-mlym",
    "x-orya",
    "x-telu",
    "x-knda",
    "x-sinh",
    "x-tibt",
    "x-unicode",
    "x-user-def"
};

gfxPlatform::gfxPlatform()
  : mAzureCanvasBackendCollector(this, &gfxPlatform::GetAzureBackendInfo)
{
    mUseHarfBuzzScripts = UNINITIALIZED_VALUE;
    mAllowDownloadableFonts = UNINITIALIZED_VALUE;
    mFallbackUsesCmaps = UNINITIALIZED_VALUE;

    mGraphiteShapingEnabled = UNINITIALIZED_VALUE;
    mBidiNumeralOption = UNINITIALIZED_VALUE;

    uint32_t canvasMask = (1 << BACKEND_CAIRO) | (1 << BACKEND_SKIA);
    uint32_t contentMask = 0;
    InitBackendPrefs(canvasMask, contentMask);
}

gfxPlatform*
gfxPlatform::GetPlatform()
{
    if (!gPlatform) {
        Init();
    }
    return gPlatform;
}

int RecordingPrefChanged(const char *aPrefName, void *aClosure)
{
  if (Preferences::GetBool("gfx.2d.recording", false)) {
    nsAutoCString fileName;
    nsAdoptingString prefFileName = Preferences::GetString("gfx.2d.recordingfile");

    if (prefFileName) {
      fileName.Append(NS_ConvertUTF16toUTF8(prefFileName));
    } else {
      fileName.AssignLiteral("browserrecording.aer");
    }

    gPlatform->mRecorder = Factory::CreateEventRecorderForFile(fileName.BeginReading());
    Factory::SetGlobalEventRecorder(gPlatform->mRecorder);
  } else {
    Factory::SetGlobalEventRecorder(nullptr);
  }

  return 0;
}

void
gfxPlatform::Init()
{
    if (gEverInitialized) {
        NS_RUNTIMEABORT("Already started???");
    }
    gEverInitialized = true;

#ifdef PR_LOGGING
    sFontlistLog = PR_NewLogModule("fontlist");;
    sFontInitLog = PR_NewLogModule("fontinit");;
    sTextrunLog = PR_NewLogModule("textrun");;
    sTextrunuiLog = PR_NewLogModule("textrunui");;
    sCmapDataLog = PR_NewLogModule("cmapdata");;
#endif

    bool useOffMainThreadCompositing = false;
    useOffMainThreadCompositing = GetPrefLayersOffMainThreadCompositionEnabled() ||
        Preferences::GetBool("browser.tabs.remote", false);
#ifdef MOZ_X11
    useOffMainThreadCompositing &= (PR_GetEnv("MOZ_USE_OMTC") != NULL) ||
                                   (PR_GetEnv("MOZ_OMTC_ENABLED") != NULL);
#endif

    if (useOffMainThreadCompositing && (XRE_GetProcessType() ==
                                        GeckoProcessType_Default)) {
        CompositorParent::StartUp();
        if (Preferences::GetBool("layers.async-video.enabled",false)) {
            ImageBridgeChild::StartUp();
        }

    }

    /* Initialize the GfxInfo service.
     * Note: we can't call functions on GfxInfo that depend
     * on gPlatform until after it has been initialized
     * below. GfxInfo initialization annotates our
     * crash reports so we want to do it before
     * we try to load any drivers and do device detection
     * incase that code crashes. See bug #591561. */
    nsCOMPtr<nsIGfxInfo> gfxInfo;
    /* this currently will only succeed on Windows */
    gfxInfo = do_GetService("@mozilla.org/gfx/info;1");

#if defined(XP_WIN)
    gPlatform = new gfxWindowsPlatform;
#elif defined(XP_MACOSX)
    gPlatform = new gfxPlatformMac;
#elif defined(MOZ_WIDGET_GTK)
    gPlatform = new gfxPlatformGtk;
#elif defined(MOZ_WIDGET_QT)
    gPlatform = new gfxQtPlatform;
#elif defined(XP_OS2)
    gPlatform = new gfxOS2Platform;
#elif defined(ANDROID)
    gPlatform = new gfxAndroidPlatform;
#else
    #error "No gfxPlatform implementation available"
#endif

#ifdef DEBUG
    mozilla::gl::GLContext::StaticInit();
#endif

    nsresult rv;

#if defined(XP_MACOSX) || defined(XP_WIN) || defined(ANDROID) // temporary, until this is implemented on others
    rv = gfxPlatformFontList::Init();
    if (NS_FAILED(rv)) {
        NS_RUNTIMEABORT("Could not initialize gfxPlatformFontList");
    }
#endif

    gPlatform->mScreenReferenceSurface =
        gPlatform->CreateOffscreenSurface(gfxIntSize(1,1),
                                          gfxASurface::CONTENT_COLOR_ALPHA);
    if (!gPlatform->mScreenReferenceSurface) {
        NS_RUNTIMEABORT("Could not initialize mScreenReferenceSurface");
    }

    rv = gfxFontCache::Init();
    if (NS_FAILED(rv)) {
        NS_RUNTIMEABORT("Could not initialize gfxFontCache");
    }

    /* Pref migration hook. */
    MigratePrefs();

    /* Create and register our CMS Override observer. */
    gPlatform->mSRGBOverrideObserver = new SRGBOverrideObserver();
    Preferences::AddWeakObserver(gPlatform->mSRGBOverrideObserver, "gfx.color_management.force_srgb");

    gPlatform->mFontPrefsObserver = new FontPrefsObserver();
    Preferences::AddStrongObservers(gPlatform->mFontPrefsObserver, kObservedPrefs);

    gPlatform->mOrientationSyncPrefsObserver = new OrientationSyncPrefsObserver();
    Preferences::AddStrongObserver(gPlatform->mOrientationSyncPrefsObserver, "layers.orientation.sync.timeout");

    gPlatform->mWorkAroundDriverBugs = Preferences::GetBool("gfx.work-around-driver-bugs", true);

    mozilla::Preferences::AddBoolVarCache(&gPlatform->mWidgetUpdateFlashing,
                                          "nglayout.debug.widget_update_flashing");

    mozilla::gl::GLContext::PlatformStartup();

#ifdef MOZ_WIDGET_ANDROID
    // Texture pool init
    mozilla::gl::TexturePoolOGL::Init();
#endif

    // Force registration of the gfx component, thus arranging for
    // ::Shutdown to be called.
    nsCOMPtr<nsISupports> forceReg
        = do_CreateInstance("@mozilla.org/gfx/init;1");

    Preferences::RegisterCallbackAndCall(RecordingPrefChanged, "gfx.2d.recording", nullptr);

    gPlatform->mOrientationSyncMillis = Preferences::GetUint("layers.orientation.sync.timeout", (uint32_t)0);

    mozilla::Preferences::AddBoolVarCache(&sDrawLayerBorders,
                                          "layers.draw-borders",
                                          false);

    CreateCMSOutputProfile();
}

void
gfxPlatform::Shutdown()
{
    // These may be called before the corresponding subsystems have actually
    // started up. That's OK, they can handle it.
    gfxFontCache::Shutdown();
    gfxFontGroup::Shutdown();
    gfxGraphiteShaper::Shutdown();
#if defined(XP_MACOSX) || defined(XP_WIN) // temporary, until this is implemented on others
    gfxPlatformFontList::Shutdown();
#endif

    // Free the various non-null transforms and loaded profiles
    ShutdownCMS();

    // In some cases, gPlatform may not be created but Shutdown() called,
    // e.g., during xpcshell tests.
    if (gPlatform) {
        /* Unregister our CMS Override callback. */
        NS_ASSERTION(gPlatform->mSRGBOverrideObserver, "mSRGBOverrideObserver has alreay gone");
        Preferences::RemoveObserver(gPlatform->mSRGBOverrideObserver, "gfx.color_management.force_srgb");
        gPlatform->mSRGBOverrideObserver = nullptr;

        NS_ASSERTION(gPlatform->mFontPrefsObserver, "mFontPrefsObserver has alreay gone");
        Preferences::RemoveObservers(gPlatform->mFontPrefsObserver, kObservedPrefs);
        gPlatform->mFontPrefsObserver = nullptr;
    }

#ifdef MOZ_WIDGET_ANDROID
    // Shut down the texture pool
    mozilla::gl::TexturePoolOGL::Shutdown();
#endif

    // Shut down the default GL context provider.
    mozilla::gl::GLContextProvider::Shutdown();

#if defined(XP_WIN)
    // The above shutdown calls operate on the available context providers on
    // most platforms.  Windows is a "special snowflake", though, and has three
    // context providers available, so we have to shut all of them down.
    // We should only support the default GL provider on Windows; then, this
    // could go away. Unfortunately, we currently support WGL (the default) for
    // WebGL on Optimus.
    mozilla::gl::GLContextProviderEGL::Shutdown();
#endif

    // This will block this thread untill the ImageBridge protocol is completely
    // deleted.
    ImageBridgeChild::ShutDown();

    CompositorParent::ShutDown();

    delete gPlatform;
    gPlatform = nullptr;
}

gfxPlatform::~gfxPlatform()
{
    mScreenReferenceSurface = nullptr;

    // The cairo folks think we should only clean up in debug builds,
    // but we're generally in the habit of trying to shut down as
    // cleanly as possible even in production code, so call this
    // cairo_debug_* function unconditionally.
    //
    // because cairo can assert and thus crash on shutdown, don't do this in release builds
#if MOZ_TREE_CAIRO && (defined(DEBUG) || defined(NS_BUILD_REFCNT_LOGGING) || defined(NS_TRACE_MALLOC))
    cairo_debug_reset_static_data();
#endif

#if 0
    // It would be nice to do this (although it might need to be after
    // the cairo shutdown that happens in ~gfxPlatform).  It even looks
    // idempotent.  But it has fatal assertions that fire if stuff is
    // leaked, and we hit them.
    FcFini();
#endif
}

already_AddRefed<gfxASurface>
gfxPlatform::CreateOffscreenImageSurface(const gfxIntSize& aSize,
                                         gfxASurface::gfxContentType aContentType)
{
  nsRefPtr<gfxASurface> newSurface;
  newSurface = new gfxImageSurface(aSize, OptimalFormatForContent(aContentType));

  return newSurface.forget();
}

already_AddRefed<gfxASurface>
gfxPlatform::OptimizeImage(gfxImageSurface *aSurface,
                           gfxASurface::gfxImageFormat format)
{
    const gfxIntSize& surfaceSize = aSurface->GetSize();

#ifdef XP_WIN
    if (gfxWindowsPlatform::GetPlatform()->GetRenderMode() ==
        gfxWindowsPlatform::RENDER_DIRECT2D) {
        return nullptr;
    }
#endif
    nsRefPtr<gfxASurface> optSurface = CreateOffscreenSurface(surfaceSize, gfxASurface::ContentFromFormat(format));
    if (!optSurface || optSurface->CairoStatus() != 0)
        return nullptr;

    gfxContext tmpCtx(optSurface);
    tmpCtx.SetOperator(gfxContext::OPERATOR_SOURCE);
    tmpCtx.SetSource(aSurface);
    tmpCtx.Paint();

    return optSurface.forget();
}

cairo_user_data_key_t kDrawTarget;

RefPtr<DrawTarget>
gfxPlatform::CreateDrawTargetForSurface(gfxASurface *aSurface, const IntSize& aSize)
{
  RefPtr<DrawTarget> drawTarget = Factory::CreateDrawTargetForCairoSurface(aSurface->CairoSurface(), aSize);
  aSurface->SetData(&kDrawTarget, drawTarget, NULL);
  return drawTarget;
}

cairo_user_data_key_t kSourceSurface;

/**
 * Record the backend that was used to construct the SourceSurface.
 * When getting the cached SourceSurface for a gfxASurface/DrawTarget pair,
 * we check to make sure the DrawTarget's backend matches the backend
 * for the cached SourceSurface, and only use it if they match. This
 * can avoid expensive and unnecessary readbacks.
 */
struct SourceSurfaceUserData
{
  RefPtr<SourceSurface> mSrcSurface;
  BackendType mBackendType;
};

void SourceBufferDestroy(void *srcSurfUD)
{
  delete static_cast<SourceSurfaceUserData*>(srcSurfUD);
}

void SourceSnapshotDetached(cairo_surface_t *nullSurf)
{
  gfxImageSurface* origSurf =
    static_cast<gfxImageSurface*>(cairo_surface_get_user_data(nullSurf, &kSourceSurface));

  origSurf->SetData(&kSourceSurface, NULL, NULL);
}

RefPtr<SourceSurface>
gfxPlatform::GetSourceSurfaceForSurface(DrawTarget *aTarget, gfxASurface *aSurface)
{
  void *userData = aSurface->GetData(&kSourceSurface);

  if (userData) {
    SourceSurfaceUserData *surf = static_cast<SourceSurfaceUserData*>(userData);

    if (surf->mSrcSurface->IsValid() && surf->mBackendType == aTarget->GetType()) {
      return surf->mSrcSurface;
    }
    // We can just continue here as when setting new user data the destroy
    // function will be called for the old user data.
  }

  SurfaceFormat format;
  if (aSurface->GetContentType() == gfxASurface::CONTENT_ALPHA) {
    format = FORMAT_A8;
  } else if (aSurface->GetContentType() == gfxASurface::CONTENT_COLOR) {
    format = FORMAT_B8G8R8X8;
  } else {
    format = FORMAT_B8G8R8A8;
  }

  RefPtr<SourceSurface> srcBuffer;

#ifdef XP_WIN
  if (aSurface->GetType() == gfxASurface::SurfaceTypeD2D) {
    NativeSurface surf;
    surf.mFormat = format;
    surf.mType = NATIVE_SURFACE_D3D10_TEXTURE;
    surf.mSurface = static_cast<gfxD2DSurface*>(aSurface)->GetTexture();
    mozilla::gfx::DrawTarget *dt = static_cast<mozilla::gfx::DrawTarget*>(aSurface->GetData(&kDrawTarget));
    if (dt) {
      dt->Flush();
    }
    srcBuffer = aTarget->CreateSourceSurfaceFromNativeSurface(surf);
  } else
#endif
  if (aSurface->CairoSurface() && aTarget->GetType() == BACKEND_CAIRO) {
    // If this is an xlib cairo surface we don't want to fetch it into memory
    // because this is a major slow down.
    NativeSurface surf;
    surf.mFormat = format;
    surf.mType = NATIVE_SURFACE_CAIRO_SURFACE;
    surf.mSurface = aSurface->CairoSurface();
    srcBuffer = aTarget->CreateSourceSurfaceFromNativeSurface(surf);

    if (srcBuffer) {
      // It's cheap enough to make a new one so we won't keep it around and
      // keeping it creates a cycle.
      return srcBuffer;
    }
  }

  if (!srcBuffer) {
    nsRefPtr<gfxImageSurface> imgSurface = aSurface->GetAsImageSurface();

    bool isWin32ImageSurf = imgSurface &&
                            aSurface->GetType() == gfxASurface::SurfaceTypeWin32;

    if (!imgSurface) {
      imgSurface = new gfxImageSurface(aSurface->GetSize(), OptimalFormatForContent(aSurface->GetContentType()));
      nsRefPtr<gfxContext> ctx = new gfxContext(imgSurface);
      ctx->SetSource(aSurface);
      ctx->SetOperator(gfxContext::OPERATOR_SOURCE);
      ctx->Paint();
    }

    gfxImageFormat cairoFormat = imgSurface->Format();
    switch(cairoFormat) {
      case gfxASurface::ImageFormatARGB32:
        format = FORMAT_B8G8R8A8;
        break;
      case gfxASurface::ImageFormatRGB24:
        format = FORMAT_B8G8R8X8;
        break;
      case gfxASurface::ImageFormatA8:
        format = FORMAT_A8;
        break;
      case gfxASurface::ImageFormatRGB16_565:
        format = FORMAT_R5G6B5;
        break;
      default:
        NS_RUNTIMEABORT("Invalid surface format!");
    }

    IntSize size = IntSize(imgSurface->GetSize().width, imgSurface->GetSize().height);
    srcBuffer = aTarget->CreateSourceSurfaceFromData(imgSurface->Data(),
                                                     size,
                                                     imgSurface->Stride(),
                                                     format);

    if (!srcBuffer) {
      // We need to check if our gfxASurface will keep the underlying data
      // alive. This is true if gfxASurface actually -is- an ImageSurface or
      // if it is a gfxWindowsSurface which supports GetAsImageSurface.
      if (imgSurface != aSurface && !isWin32ImageSurf) {
        // This shouldn't happen for now, it can be easily supported by making
        // a copy. For now let's just abort.
        NS_RUNTIMEABORT("Attempt to create unsupported SourceSurface from"
            "non-image surface.");
        return nullptr;
      }

      srcBuffer = Factory::CreateWrappingDataSourceSurface(imgSurface->Data(),
                                                           imgSurface->Stride(),
                                                           size, format);

    }

    cairo_surface_t *nullSurf =
	cairo_null_surface_create(CAIRO_CONTENT_COLOR_ALPHA);
    cairo_surface_set_user_data(nullSurf,
                                &kSourceSurface,
                                imgSurface,
                                NULL);
    cairo_surface_attach_snapshot(imgSurface->CairoSurface(), nullSurf, SourceSnapshotDetached);
    cairo_surface_destroy(nullSurf);
  }

  SourceSurfaceUserData *srcSurfUD = new SourceSurfaceUserData;
  srcSurfUD->mBackendType = aTarget->GetType();
  srcSurfUD->mSrcSurface = srcBuffer;
  aSurface->SetData(&kSourceSurface, srcSurfUD, SourceBufferDestroy);

  return srcBuffer;
}

TemporaryRef<ScaledFont>
gfxPlatform::GetScaledFontForFont(DrawTarget* aTarget, gfxFont *aFont)
{
  NativeFont nativeFont;
  nativeFont.mType = NATIVE_FONT_CAIRO_FONT_FACE;
  nativeFont.mFont = aFont->GetCairoScaledFont();
  RefPtr<ScaledFont> scaledFont =
    Factory::CreateScaledFontForNativeFont(nativeFont,
                                           aFont->GetAdjustedSize());
  return scaledFont;
}

cairo_user_data_key_t kDrawSourceSurface;
static void
DataSourceSurfaceDestroy(void *dataSourceSurface)
{
  static_cast<DataSourceSurface*>(dataSourceSurface)->Release();
}

cairo_user_data_key_t kDrawTargetForSurface;
static void
DataDrawTargetDestroy(void *aTarget)
{
  static_cast<DrawTarget*>(aTarget)->Release();
}

bool
gfxPlatform::UseAcceleratedSkiaCanvas()
{
  return Preferences::GetBool("gfx.canvas.azure.accelerated", false) &&
         mPreferredCanvasBackend == BACKEND_SKIA;
}

already_AddRefed<gfxASurface>
gfxPlatform::GetThebesSurfaceForDrawTarget(DrawTarget *aTarget)
{
  if (aTarget->GetType() == BACKEND_CAIRO) {
    cairo_surface_t* csurf =
      static_cast<cairo_surface_t*>(aTarget->GetNativeSurface(NATIVE_SURFACE_CAIRO_SURFACE));
    return gfxASurface::Wrap(csurf);
  }

  // The semantics of this part of the function are sort of weird. If we
  // don't have direct support for the backend, we snapshot the first time
  // and then return the snapshotted surface for the lifetime of the draw
  // target. Sometimes it seems like this works out, but it seems like it
  // might result in no updates ever.
  RefPtr<SourceSurface> source = aTarget->Snapshot();
  RefPtr<DataSourceSurface> data = source->GetDataSurface();

  if (!data) {
    return NULL;
  }

  IntSize size = data->GetSize();
  gfxASurface::gfxImageFormat format = OptimalFormatForContent(ContentForFormat(data->GetFormat()));


  nsRefPtr<gfxASurface> surf =
    new gfxImageSurface(data->GetData(), gfxIntSize(size.width, size.height),
                        data->Stride(), format);

  surf->SetData(&kDrawSourceSurface, data.forget().drop(), DataSourceSurfaceDestroy);
  // keep the draw target alive as long as we need its data
  aTarget->AddRef();
  surf->SetData(&kDrawTargetForSurface, aTarget, DataDrawTargetDestroy);

  return surf.forget();
}

RefPtr<DrawTarget>
gfxPlatform::CreateDrawTargetForBackend(BackendType aBackend, const IntSize& aSize, SurfaceFormat aFormat)
{
  // There is a bunch of knowledge in the gfxPlatform heirarchy about how to
  // create the best offscreen surface for the current system and situation. We
  // can easily take advantage of this for the Cairo backend, so that's what we
  // do.
  // mozilla::gfx::Factory can get away without having all this knowledge for
  // now, but this might need to change in the future (using
  // CreateOffscreenSurface() and CreateDrawTargetForSurface() for all
  // backends).
  if (aBackend == BACKEND_CAIRO) {
    nsRefPtr<gfxASurface> surf = CreateOffscreenSurface(ThebesIntSize(aSize),
                                                        ContentForFormat(aFormat));
    if (!surf || surf->CairoStatus()) {
      return NULL;
    }

    return CreateDrawTargetForSurface(surf, aSize);
  } else {
    return Factory::CreateDrawTarget(aBackend, aSize, aFormat);
  }
}

RefPtr<DrawTarget>
gfxPlatform::CreateOffscreenDrawTarget(const IntSize& aSize, SurfaceFormat aFormat)
{
  NS_ASSERTION(mPreferredCanvasBackend, "No backend.");
  RefPtr<DrawTarget> target = CreateDrawTargetForBackend(mPreferredCanvasBackend, aSize, aFormat);
  if (target ||
      mFallbackCanvasBackend == BACKEND_NONE) {
    return target;
  }

  return CreateDrawTargetForBackend(mFallbackCanvasBackend, aSize, aFormat);
}


RefPtr<DrawTarget>
gfxPlatform::CreateDrawTargetForData(unsigned char* aData, const IntSize& aSize, int32_t aStride, SurfaceFormat aFormat)
{
  NS_ASSERTION(mPreferredCanvasBackend, "No backend.");
  return Factory::CreateDrawTargetForData(mPreferredCanvasBackend, aData, aSize, aStride, aFormat);
}

RefPtr<DrawTarget>
gfxPlatform::CreateDrawTargetForFBO(unsigned int aFBOID, mozilla::gl::GLContext* aGLContext, const IntSize& aSize, SurfaceFormat aFormat)
{
  NS_ASSERTION(mPreferredCanvasBackend, "No backend.");
#ifdef USE_SKIA_GPU
  if (mPreferredCanvasBackend == BACKEND_SKIA) {
    static uint8_t sGrContextKey;
    GrContext* ctx = reinterpret_cast<GrContext*>(aGLContext->GetUserData(&sGrContextKey));
    if (!ctx) {
      GrGLInterface* grInterface = CreateGrInterfaceFromGLContext(aGLContext);
      ctx = GrContext::Create(kOpenGL_Shaders_GrEngine, (GrPlatform3DContext)grInterface);
      aGLContext->SetUserData(&sGrContextKey, ctx);
    }

    // Unfortunately Factory can't depend on GLContext, so it needs to be passed a GrContext instead
    return Factory::CreateSkiaDrawTargetForFBO(aFBOID, ctx, aSize, aFormat);
  }
#endif
  return nullptr;
}

/* static */ BackendType
gfxPlatform::BackendTypeForName(const nsCString& aName)
{
  if (aName.EqualsLiteral("cairo"))
    return BACKEND_CAIRO;
  if (aName.EqualsLiteral("skia"))
    return BACKEND_SKIA;
  if (aName.EqualsLiteral("direct2d"))
    return BACKEND_DIRECT2D;
  if (aName.EqualsLiteral("cg"))
    return BACKEND_COREGRAPHICS;
  return BACKEND_NONE;
}

nsresult
gfxPlatform::GetFontList(nsIAtom *aLangGroup,
                         const nsACString& aGenericFamily,
                         nsTArray<nsString>& aListOfFonts)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
gfxPlatform::UpdateFontList()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

bool
gfxPlatform::DownloadableFontsEnabled()
{
    if (mAllowDownloadableFonts == UNINITIALIZED_VALUE) {
        mAllowDownloadableFonts =
            Preferences::GetBool(GFX_DOWNLOADABLE_FONTS_ENABLED, false);
    }

    return mAllowDownloadableFonts;
}

bool
gfxPlatform::UseCmapsDuringSystemFallback()
{
    if (mFallbackUsesCmaps == UNINITIALIZED_VALUE) {
        mFallbackUsesCmaps =
            Preferences::GetBool(GFX_PREF_FALLBACK_USE_CMAPS, false);
    }

    return mFallbackUsesCmaps;
}

bool
gfxPlatform::UseGraphiteShaping()
{
    if (mGraphiteShapingEnabled == UNINITIALIZED_VALUE) {
        mGraphiteShapingEnabled =
            Preferences::GetBool(GFX_PREF_GRAPHITE_SHAPING, false);
    }

    return mGraphiteShapingEnabled;
}

bool
gfxPlatform::UseHarfBuzzForScript(int32_t aScriptCode)
{
    if (mUseHarfBuzzScripts == UNINITIALIZED_VALUE) {
        mUseHarfBuzzScripts = Preferences::GetInt(GFX_PREF_HARFBUZZ_SCRIPTS, HARFBUZZ_SCRIPTS_DEFAULT);
    }

    int32_t shapingType = mozilla::unicode::ScriptShapingType(aScriptCode);

    return (mUseHarfBuzzScripts & shapingType) != 0;
}

gfxFontEntry*
gfxPlatform::MakePlatformFont(const gfxProxyFontEntry *aProxyEntry,
                              const uint8_t *aFontData,
                              uint32_t aLength)
{
    // Default implementation does not handle activating downloaded fonts;
    // just free the data and return.
    // Platforms that support @font-face must override this,
    // using the data to instantiate the font, and taking responsibility
    // for freeing it when no longer required.
    if (aFontData) {
        NS_Free((void*)aFontData);
    }
    return nullptr;
}

static void
AppendGenericFontFromPref(nsString& aFonts, nsIAtom *aLangGroup, const char *aGenericName)
{
    NS_ENSURE_TRUE_VOID(Preferences::GetRootBranch());

    nsAutoCString prefName, langGroupString;

    aLangGroup->ToUTF8String(langGroupString);

    nsAutoCString genericDotLang;
    if (aGenericName) {
        genericDotLang.Assign(aGenericName);
    } else {
        prefName.AssignLiteral("font.default.");
        prefName.Append(langGroupString);
        genericDotLang = Preferences::GetCString(prefName.get());
    }

    genericDotLang.AppendLiteral(".");
    genericDotLang.Append(langGroupString);

    // fetch font.name.xxx value
    prefName.AssignLiteral("font.name.");
    prefName.Append(genericDotLang);
    nsAdoptingString nameValue = Preferences::GetString(prefName.get());
    if (nameValue) {
        if (!aFonts.IsEmpty())
            aFonts.AppendLiteral(", ");
        aFonts += nameValue;
    }

    // fetch font.name-list.xxx value
    prefName.AssignLiteral("font.name-list.");
    prefName.Append(genericDotLang);
    nsAdoptingString nameListValue = Preferences::GetString(prefName.get());
    if (nameListValue && !nameListValue.Equals(nameValue)) {
        if (!aFonts.IsEmpty())
            aFonts.AppendLiteral(", ");
        aFonts += nameListValue;
    }
}

void
gfxPlatform::GetPrefFonts(nsIAtom *aLanguage, nsString& aFonts, bool aAppendUnicode)
{
    aFonts.Truncate();

    AppendGenericFontFromPref(aFonts, aLanguage, nullptr);
    if (aAppendUnicode)
        AppendGenericFontFromPref(aFonts, nsGkAtoms::Unicode, nullptr);
}

bool gfxPlatform::ForEachPrefFont(eFontPrefLang aLangArray[], uint32_t aLangArrayLen, PrefFontCallback aCallback,
                                    void *aClosure)
{
    NS_ENSURE_TRUE(Preferences::GetRootBranch(), false);

    uint32_t    i;
    for (i = 0; i < aLangArrayLen; i++) {
        eFontPrefLang prefLang = aLangArray[i];
        const char *langGroup = GetPrefLangName(prefLang);

        nsAutoCString prefName;

        prefName.AssignLiteral("font.default.");
        prefName.Append(langGroup);
        nsAdoptingCString genericDotLang = Preferences::GetCString(prefName.get());

        genericDotLang.AppendLiteral(".");
        genericDotLang.Append(langGroup);

        // fetch font.name.xxx value
        prefName.AssignLiteral("font.name.");
        prefName.Append(genericDotLang);
        nsAdoptingCString nameValue = Preferences::GetCString(prefName.get());
        if (nameValue) {
            if (!aCallback(prefLang, NS_ConvertUTF8toUTF16(nameValue), aClosure))
                return false;
        }

        // fetch font.name-list.xxx value
        prefName.AssignLiteral("font.name-list.");
        prefName.Append(genericDotLang);
        nsAdoptingCString nameListValue = Preferences::GetCString(prefName.get());
        if (nameListValue && !nameListValue.Equals(nameValue)) {
            const char kComma = ',';
            const char *p, *p_end;
            nsAutoCString list(nameListValue);
            list.BeginReading(p);
            list.EndReading(p_end);
            while (p < p_end) {
                while (nsCRT::IsAsciiSpace(*p)) {
                    if (++p == p_end)
                        break;
                }
                if (p == p_end)
                    break;
                const char *start = p;
                while (++p != p_end && *p != kComma)
                    /* nothing */ ;
                nsAutoCString fontName(Substring(start, p));
                fontName.CompressWhitespace(false, true);
                if (!aCallback(prefLang, NS_ConvertUTF8toUTF16(fontName), aClosure))
                    return false;
                p++;
            }
        }
    }

    return true;
}

eFontPrefLang
gfxPlatform::GetFontPrefLangFor(const char* aLang)
{
    if (!aLang || !aLang[0])
        return eFontPrefLang_Others;
    for (uint32_t i = 0; i < uint32_t(eFontPrefLang_LangCount); ++i) {
        if (!PL_strcasecmp(gPrefLangNames[i], aLang))
            return eFontPrefLang(i);
    }
    return eFontPrefLang_Others;
}

eFontPrefLang
gfxPlatform::GetFontPrefLangFor(nsIAtom *aLang)
{
    if (!aLang)
        return eFontPrefLang_Others;
    nsAutoCString lang;
    aLang->ToUTF8String(lang);
    return GetFontPrefLangFor(lang.get());
}

const char*
gfxPlatform::GetPrefLangName(eFontPrefLang aLang)
{
    if (uint32_t(aLang) < uint32_t(eFontPrefLang_AllCount))
        return gPrefLangNames[uint32_t(aLang)];
    return nullptr;
}

eFontPrefLang
gfxPlatform::GetFontPrefLangFor(uint8_t aUnicodeRange)
{
    switch (aUnicodeRange) {
        case kRangeSetLatin:   return eFontPrefLang_Western;
        case kRangeCyrillic:   return eFontPrefLang_Cyrillic;
        case kRangeGreek:      return eFontPrefLang_Greek;
        case kRangeTurkish:    return eFontPrefLang_Turkish;
        case kRangeHebrew:     return eFontPrefLang_Hebrew;
        case kRangeArabic:     return eFontPrefLang_Arabic;
        case kRangeBaltic:     return eFontPrefLang_Baltic;
        case kRangeThai:       return eFontPrefLang_Thai;
        case kRangeKorean:     return eFontPrefLang_Korean;
        case kRangeJapanese:   return eFontPrefLang_Japanese;
        case kRangeSChinese:   return eFontPrefLang_ChineseCN;
        case kRangeTChinese:   return eFontPrefLang_ChineseTW;
        case kRangeDevanagari: return eFontPrefLang_Devanagari;
        case kRangeTamil:      return eFontPrefLang_Tamil;
        case kRangeArmenian:   return eFontPrefLang_Armenian;
        case kRangeBengali:    return eFontPrefLang_Bengali;
        case kRangeCanadian:   return eFontPrefLang_Canadian;
        case kRangeEthiopic:   return eFontPrefLang_Ethiopic;
        case kRangeGeorgian:   return eFontPrefLang_Georgian;
        case kRangeGujarati:   return eFontPrefLang_Gujarati;
        case kRangeGurmukhi:   return eFontPrefLang_Gurmukhi;
        case kRangeKhmer:      return eFontPrefLang_Khmer;
        case kRangeMalayalam:  return eFontPrefLang_Malayalam;
        case kRangeOriya:      return eFontPrefLang_Oriya;
        case kRangeTelugu:     return eFontPrefLang_Telugu;
        case kRangeKannada:    return eFontPrefLang_Kannada;
        case kRangeSinhala:    return eFontPrefLang_Sinhala;
        case kRangeTibetan:    return eFontPrefLang_Tibetan;
        case kRangeSetCJK:     return eFontPrefLang_CJKSet;
        default:               return eFontPrefLang_Others;
    }
}

bool
gfxPlatform::IsLangCJK(eFontPrefLang aLang)
{
    switch (aLang) {
        case eFontPrefLang_Japanese:
        case eFontPrefLang_ChineseTW:
        case eFontPrefLang_ChineseCN:
        case eFontPrefLang_ChineseHK:
        case eFontPrefLang_Korean:
        case eFontPrefLang_CJKSet:
            return true;
        default:
            return false;
    }
}

bool
gfxPlatform::DrawLayerBorders()
{
    return sDrawLayerBorders;
}


void
gfxPlatform::GetLangPrefs(eFontPrefLang aPrefLangs[], uint32_t &aLen, eFontPrefLang aCharLang, eFontPrefLang aPageLang)
{
    if (IsLangCJK(aCharLang)) {
        AppendCJKPrefLangs(aPrefLangs, aLen, aCharLang, aPageLang);
    } else {
        AppendPrefLang(aPrefLangs, aLen, aCharLang);
    }

    AppendPrefLang(aPrefLangs, aLen, eFontPrefLang_Others);
}

void
gfxPlatform::AppendCJKPrefLangs(eFontPrefLang aPrefLangs[], uint32_t &aLen, eFontPrefLang aCharLang, eFontPrefLang aPageLang)
{
    // prefer the lang specified by the page *if* CJK
    if (IsLangCJK(aPageLang)) {
        AppendPrefLang(aPrefLangs, aLen, aPageLang);
    }

    // if not set up, set up the default CJK order, based on accept lang settings and locale
    if (mCJKPrefLangs.Length() == 0) {

        // temp array
        eFontPrefLang tempPrefLangs[kMaxLenPrefLangList];
        uint32_t tempLen = 0;

        // Add the CJK pref fonts from accept languages, the order should be same order
        nsAdoptingCString list = Preferences::GetLocalizedCString("intl.accept_languages");
        if (!list.IsEmpty()) {
            const char kComma = ',';
            const char *p, *p_end;
            list.BeginReading(p);
            list.EndReading(p_end);
            while (p < p_end) {
                while (nsCRT::IsAsciiSpace(*p)) {
                    if (++p == p_end)
                        break;
                }
                if (p == p_end)
                    break;
                const char *start = p;
                while (++p != p_end && *p != kComma)
                    /* nothing */ ;
                nsAutoCString lang(Substring(start, p));
                lang.CompressWhitespace(false, true);
                eFontPrefLang fpl = gfxPlatform::GetFontPrefLangFor(lang.get());
                switch (fpl) {
                    case eFontPrefLang_Japanese:
                    case eFontPrefLang_Korean:
                    case eFontPrefLang_ChineseCN:
                    case eFontPrefLang_ChineseHK:
                    case eFontPrefLang_ChineseTW:
                        AppendPrefLang(tempPrefLangs, tempLen, fpl);
                        break;
                    default:
                        break;
                }
                p++;
            }
        }

        do { // to allow 'break' to abort this block if a call fails
            nsresult rv;
            nsCOMPtr<nsILocaleService> ls =
                do_GetService(NS_LOCALESERVICE_CONTRACTID, &rv);
            if (NS_FAILED(rv))
                break;

            nsCOMPtr<nsILocale> appLocale;
            rv = ls->GetApplicationLocale(getter_AddRefs(appLocale));
            if (NS_FAILED(rv))
                break;

            nsString localeStr;
            rv = appLocale->
                GetCategory(NS_LITERAL_STRING(NSILOCALE_MESSAGE), localeStr);
            if (NS_FAILED(rv))
                break;

            const nsAString& lang = Substring(localeStr, 0, 2);
            if (lang.EqualsLiteral("ja")) {
                AppendPrefLang(tempPrefLangs, tempLen, eFontPrefLang_Japanese);
            } else if (lang.EqualsLiteral("zh")) {
                const nsAString& region = Substring(localeStr, 3, 2);
                if (region.EqualsLiteral("CN")) {
                    AppendPrefLang(tempPrefLangs, tempLen, eFontPrefLang_ChineseCN);
                } else if (region.EqualsLiteral("TW")) {
                    AppendPrefLang(tempPrefLangs, tempLen, eFontPrefLang_ChineseTW);
                } else if (region.EqualsLiteral("HK")) {
                    AppendPrefLang(tempPrefLangs, tempLen, eFontPrefLang_ChineseHK);
                }
            } else if (lang.EqualsLiteral("ko")) {
                AppendPrefLang(tempPrefLangs, tempLen, eFontPrefLang_Korean);
            }
        } while (0);

        // last resort... (the order is same as old gfx.)
        AppendPrefLang(tempPrefLangs, tempLen, eFontPrefLang_Japanese);
        AppendPrefLang(tempPrefLangs, tempLen, eFontPrefLang_Korean);
        AppendPrefLang(tempPrefLangs, tempLen, eFontPrefLang_ChineseCN);
        AppendPrefLang(tempPrefLangs, tempLen, eFontPrefLang_ChineseHK);
        AppendPrefLang(tempPrefLangs, tempLen, eFontPrefLang_ChineseTW);

        // copy into the cached array
        uint32_t j;
        for (j = 0; j < tempLen; j++) {
            mCJKPrefLangs.AppendElement(tempPrefLangs[j]);
        }
    }

    // append in cached CJK langs
    uint32_t  i, numCJKlangs = mCJKPrefLangs.Length();

    for (i = 0; i < numCJKlangs; i++) {
        AppendPrefLang(aPrefLangs, aLen, (eFontPrefLang) (mCJKPrefLangs[i]));
    }

}

void
gfxPlatform::AppendPrefLang(eFontPrefLang aPrefLangs[], uint32_t& aLen, eFontPrefLang aAddLang)
{
    if (aLen >= kMaxLenPrefLangList) return;

    // make sure
    uint32_t  i = 0;
    while (i < aLen && aPrefLangs[i] != aAddLang) {
        i++;
    }

    if (i == aLen) {
        aPrefLangs[aLen] = aAddLang;
        aLen++;
    }
}

void
gfxPlatform::InitBackendPrefs(uint32_t aCanvasBitmask, uint32_t aContentBitmask)
{
    mPreferredCanvasBackend = GetCanvasBackendPref(aCanvasBitmask);
    if (!mPreferredCanvasBackend) {
      mPreferredCanvasBackend = BACKEND_CAIRO;
    }
    mFallbackCanvasBackend = GetCanvasBackendPref(aCanvasBitmask & ~(1 << mPreferredCanvasBackend));
    mContentBackend = GetContentBackendPref(aContentBitmask);
}

/* static */ BackendType
gfxPlatform::GetCanvasBackendPref(uint32_t aBackendBitmask)
{
    return GetBackendPref(nullptr, "gfx.canvas.azure.backends", aBackendBitmask);
}

/* static */ BackendType
gfxPlatform::GetContentBackendPref(uint32_t aBackendBitmask)
{
    return GetBackendPref("gfx.content.azure.enabled", "gfx.content.azure.backends", aBackendBitmask);
}

/* static */ BackendType
gfxPlatform::GetBackendPref(const char* aEnabledPrefName, const char* aBackendPrefName, uint32_t aBackendBitmask)
{
    if (aEnabledPrefName &&
        !Preferences::GetBool(aEnabledPrefName, false)) {
        return BACKEND_NONE;
    }

    nsTArray<nsCString> backendList;
    nsCString prefString;
    if (NS_SUCCEEDED(Preferences::GetCString(aBackendPrefName, &prefString))) {
        ParseString(prefString, ',', backendList);
    }

    for (uint32_t i = 0; i < backendList.Length(); ++i) {
        BackendType result = BackendTypeForName(backendList[i]);
        if ((1 << result) & aBackendBitmask) {
            return result;
        }
    }
    return BACKEND_NONE;
}

bool
gfxPlatform::UseProgressiveTilePainting()
{
    static bool sUseProgressiveTilePainting;
    static bool sUseProgressiveTilePaintingPrefCached = false;

    if (!sUseProgressiveTilePaintingPrefCached) {
        sUseProgressiveTilePaintingPrefCached = true;
        mozilla::Preferences::AddBoolVarCache(&sUseProgressiveTilePainting,
                                              "layers.progressive-paint",
                                              false);
    }

    return sUseProgressiveTilePainting;
}

bool
gfxPlatform::UseLowPrecisionBuffer()
{
    static bool sUseLowPrecisionBuffer;
    static bool sUseLowPrecisionBufferPrefCached = false;

    if (!sUseLowPrecisionBufferPrefCached) {
        sUseLowPrecisionBufferPrefCached = true;
        mozilla::Preferences::AddBoolVarCache(&sUseLowPrecisionBuffer,
                                              "layers.low-precision-buffer",
                                              false);
    }

    return sUseLowPrecisionBuffer;
}

float
gfxPlatform::GetLowPrecisionResolution()
{
    static float sLowPrecisionResolution;
    static bool sLowPrecisionResolutionPrefCached = false;

    if (!sLowPrecisionResolutionPrefCached) {
        int32_t lowPrecisionResolution = 250;
        sLowPrecisionResolutionPrefCached = true;
        mozilla::Preferences::AddIntVarCache(&lowPrecisionResolution,
                                             "layers.low-precision-resolution",
                                             250);
        sLowPrecisionResolution = lowPrecisionResolution / 1000.f;
    }

    return sLowPrecisionResolution;
}

bool
gfxPlatform::UseReusableTileStore()
{
    static bool sUseReusableTileStore;
    static bool sUseReusableTileStorePrefCached = false;

    if (!sUseReusableTileStorePrefCached) {
        sUseReusableTileStorePrefCached = true;
        mozilla::Preferences::AddBoolVarCache(&sUseReusableTileStore,
                                              "layers.reuse-invalid-tiles",
                                              false);
    }

    return sUseReusableTileStore;
}

bool
gfxPlatform::OffMainThreadCompositingEnabled()
{
  return XRE_GetProcessType() == GeckoProcessType_Default ?
    CompositorParent::CompositorLoop() != nullptr :
    CompositorChild::ChildProcessHasCompositor();
}

eCMSMode
gfxPlatform::GetCMSMode()
{
    if (gCMSInitialized == false) {
        gCMSInitialized = true;
        nsresult rv;

        int32_t mode;
        rv = Preferences::GetInt("gfx.color_management.mode", &mode);
        if (NS_SUCCEEDED(rv) && (mode >= 0) && (mode < eCMSMode_AllCount)) {
            gCMSMode = static_cast<eCMSMode>(mode);
        }

        bool enableV4;
        rv = Preferences::GetBool("gfx.color_management.enablev4", &enableV4);
        if (NS_SUCCEEDED(rv) && enableV4) {
            qcms_enable_iccv4();
        }
    }
    return gCMSMode;
}

int
gfxPlatform::GetRenderingIntent()
{
    if (gCMSIntent == -2) {

        /* Try to query the pref system for a rendering intent. */
        int32_t pIntent;
        if (NS_SUCCEEDED(Preferences::GetInt("gfx.color_management.rendering_intent", &pIntent))) {
            /* If the pref is within range, use it as an override. */
            if ((pIntent >= QCMS_INTENT_MIN) && (pIntent <= QCMS_INTENT_MAX)) {
                gCMSIntent = pIntent;
            }
            /* If the pref is out of range, use embedded profile. */
            else {
                gCMSIntent = -1;
            }
        }
        /* If we didn't get a valid intent from prefs, use the default. */
        else {
            gCMSIntent = QCMS_INTENT_DEFAULT;
        }
    }
    return gCMSIntent;
}

void
gfxPlatform::TransformPixel(const gfxRGBA& in, gfxRGBA& out, qcms_transform *transform)
{

    if (transform) {
        /* we want the bytes in RGB order */
#ifdef IS_LITTLE_ENDIAN
        /* ABGR puts the bytes in |RGBA| order on little endian */
        uint32_t packed = in.Packed(gfxRGBA::PACKED_ABGR);
        qcms_transform_data(transform,
                       (uint8_t *)&packed, (uint8_t *)&packed,
                       1);
        out.~gfxRGBA();
        new (&out) gfxRGBA(packed, gfxRGBA::PACKED_ABGR);
#else
        /* ARGB puts the bytes in |ARGB| order on big endian */
        uint32_t packed = in.Packed(gfxRGBA::PACKED_ARGB);
        /* add one to move past the alpha byte */
        qcms_transform_data(transform,
                       (uint8_t *)&packed + 1, (uint8_t *)&packed + 1,
                       1);
        out.~gfxRGBA();
        new (&out) gfxRGBA(packed, gfxRGBA::PACKED_ARGB);
#endif
    }

    else if (&out != &in)
        out = in;
}

qcms_profile *
gfxPlatform::GetPlatformCMSOutputProfile()
{
    return nullptr;
}

void
gfxPlatform::CreateCMSOutputProfile()
{
    if (!gCMSOutputProfile) {
        /* Determine if we're using the internal override to force sRGB as
           an output profile for reftests. See Bug 452125.

           Note that we don't normally (outside of tests) set a
           default value of this preference, which means nsIPrefBranch::GetBoolPref
           will typically throw (and leave its out-param untouched).
         */
        if (Preferences::GetBool("gfx.color_management.force_srgb", false)) {
            gCMSOutputProfile = GetCMSsRGBProfile();
        }

        if (!gCMSOutputProfile) {
            nsAdoptingCString fname = Preferences::GetCString("gfx.color_management.display_profile");
            if (!fname.IsEmpty()) {
                gCMSOutputProfile = qcms_profile_from_path(fname);
            }
        }

        if (!gCMSOutputProfile) {
            gCMSOutputProfile =
                gfxPlatform::GetPlatform()->GetPlatformCMSOutputProfile();
        }

        /* Determine if the profile looks bogus. If so, close the profile
         * and use sRGB instead. See bug 460629, */
        if (gCMSOutputProfile && qcms_profile_is_bogus(gCMSOutputProfile)) {
            NS_ASSERTION(gCMSOutputProfile != GetCMSsRGBProfile(),
                         "Builtin sRGB profile tagged as bogus!!!");
            qcms_profile_release(gCMSOutputProfile);
            gCMSOutputProfile = nullptr;
        }

        if (!gCMSOutputProfile) {
            gCMSOutputProfile = GetCMSsRGBProfile();
        }
        /* Precache the LUT16 Interpolations for the output profile. See
           bug 444661 for details. */
        qcms_profile_precache_output_transform(gCMSOutputProfile);
    }
}

qcms_profile *
gfxPlatform::GetCMSOutputProfile()
{
    return gCMSOutputProfile;
}

qcms_profile *
gfxPlatform::GetCMSsRGBProfile()
{
    if (!gCMSsRGBProfile) {

        /* Create the profile using qcms. */
        gCMSsRGBProfile = qcms_profile_sRGB();
    }
    return gCMSsRGBProfile;
}

qcms_transform *
gfxPlatform::GetCMSRGBTransform()
{
    if (!gCMSRGBTransform) {
        qcms_profile *inProfile, *outProfile;
        outProfile = GetCMSOutputProfile();
        inProfile = GetCMSsRGBProfile();

        if (!inProfile || !outProfile)
            return nullptr;

        gCMSRGBTransform = qcms_transform_create(inProfile, QCMS_DATA_RGB_8,
                                              outProfile, QCMS_DATA_RGB_8,
                                             QCMS_INTENT_PERCEPTUAL);
    }

    return gCMSRGBTransform;
}

qcms_transform *
gfxPlatform::GetCMSInverseRGBTransform()
{
    if (!gCMSInverseRGBTransform) {
        qcms_profile *inProfile, *outProfile;
        inProfile = GetCMSOutputProfile();
        outProfile = GetCMSsRGBProfile();

        if (!inProfile || !outProfile)
            return nullptr;

        gCMSInverseRGBTransform = qcms_transform_create(inProfile, QCMS_DATA_RGB_8,
                                                     outProfile, QCMS_DATA_RGB_8,
                                                     QCMS_INTENT_PERCEPTUAL);
    }

    return gCMSInverseRGBTransform;
}

qcms_transform *
gfxPlatform::GetCMSRGBATransform()
{
    if (!gCMSRGBATransform) {
        qcms_profile *inProfile, *outProfile;
        outProfile = GetCMSOutputProfile();
        inProfile = GetCMSsRGBProfile();

        if (!inProfile || !outProfile)
            return nullptr;

        gCMSRGBATransform = qcms_transform_create(inProfile, QCMS_DATA_RGBA_8,
                                               outProfile, QCMS_DATA_RGBA_8,
                                               QCMS_INTENT_PERCEPTUAL);
    }

    return gCMSRGBATransform;
}

/* Shuts down various transforms and profiles for CMS. */
static void ShutdownCMS()
{

    if (gCMSRGBTransform) {
        qcms_transform_release(gCMSRGBTransform);
        gCMSRGBTransform = nullptr;
    }
    if (gCMSInverseRGBTransform) {
        qcms_transform_release(gCMSInverseRGBTransform);
        gCMSInverseRGBTransform = nullptr;
    }
    if (gCMSRGBATransform) {
        qcms_transform_release(gCMSRGBATransform);
        gCMSRGBATransform = nullptr;
    }
    if (gCMSOutputProfile) {
        qcms_profile_release(gCMSOutputProfile);

        // handle the aliased case
        if (gCMSsRGBProfile == gCMSOutputProfile)
            gCMSsRGBProfile = nullptr;
        gCMSOutputProfile = nullptr;
    }
    if (gCMSsRGBProfile) {
        qcms_profile_release(gCMSsRGBProfile);
        gCMSsRGBProfile = nullptr;
    }

    // Reset the state variables
    gCMSIntent = -2;
    gCMSMode = eCMSMode_Off;
    gCMSInitialized = false;
}

static void MigratePrefs()
{
    /* Migrate from the boolean color_management.enabled pref - we now use
       color_management.mode. */
    if (Preferences::HasUserValue("gfx.color_management.enabled")) {
        if (Preferences::GetBool("gfx.color_management.enabled", false)) {
            Preferences::SetInt("gfx.color_management.mode", static_cast<int32_t>(eCMSMode_All));
        }
        Preferences::ClearUser("gfx.color_management.enabled");
    }
}

// default SetupClusterBoundaries, based on Unicode properties;
// platform subclasses may override if they wish
void
gfxPlatform::SetupClusterBoundaries(gfxTextRun *aTextRun, const PRUnichar *aString)
{
    if (aTextRun->GetFlags() & gfxTextRunFactory::TEXT_IS_8BIT) {
        // 8-bit text doesn't have clusters.
        // XXX is this true in all languages???
        // behdad: don't think so.  Czech for example IIRC has a
        // 'ch' grapheme.
        // jfkthame: but that's not expected to behave as a grapheme cluster
        // for selection/editing/etc.
        return;
    }

    aTextRun->SetupClusterBoundaries(0, aString, aTextRun->GetLength());
}

int32_t
gfxPlatform::GetBidiNumeralOption()
{
    if (mBidiNumeralOption == UNINITIALIZED_VALUE) {
        mBidiNumeralOption = Preferences::GetInt(BIDI_NUMERAL_PREF, 0);
    }
    return mBidiNumeralOption;
}

void
gfxPlatform::FontsPrefsChanged(const char *aPref)
{
    NS_ASSERTION(aPref != nullptr, "null preference");
    if (!strcmp(GFX_DOWNLOADABLE_FONTS_ENABLED, aPref)) {
        mAllowDownloadableFonts = UNINITIALIZED_VALUE;
    } else if (!strcmp(GFX_PREF_FALLBACK_USE_CMAPS, aPref)) {
        mFallbackUsesCmaps = UNINITIALIZED_VALUE;
    } else if (!strcmp(GFX_PREF_GRAPHITE_SHAPING, aPref)) {
        mGraphiteShapingEnabled = UNINITIALIZED_VALUE;
        gfxFontCache *fontCache = gfxFontCache::GetCache();
        if (fontCache) {
            fontCache->AgeAllGenerations();
            fontCache->FlushShapedWordCaches();
        }
    } else if (!strcmp(GFX_PREF_HARFBUZZ_SCRIPTS, aPref)) {
        mUseHarfBuzzScripts = UNINITIALIZED_VALUE;
        gfxFontCache *fontCache = gfxFontCache::GetCache();
        if (fontCache) {
            fontCache->AgeAllGenerations();
            fontCache->FlushShapedWordCaches();
        }
    } else if (!strcmp(BIDI_NUMERAL_PREF, aPref)) {
        mBidiNumeralOption = UNINITIALIZED_VALUE;
    } else if (!strcmp(GFX_PREF_OPENTYPE_SVG, aPref)) {
        gfxFontCache::GetCache()->AgeAllGenerations();
    }
}


PRLogModuleInfo*
gfxPlatform::GetLog(eGfxLog aWhichLog)
{
#ifdef PR_LOGGING
    switch (aWhichLog) {
    case eGfxLog_fontlist:
        return sFontlistLog;
        break;
    case eGfxLog_fontinit:
        return sFontInitLog;
        break;
    case eGfxLog_textrun:
        return sTextrunLog;
        break;
    case eGfxLog_textrunui:
        return sTextrunuiLog;
        break;
    case eGfxLog_cmapdata:
        return sCmapDataLog;
        break;
    default:
        break;
    }

    return nullptr;
#else
    return nullptr;
#endif
}

int
gfxPlatform::GetScreenDepth() const
{
    NS_WARNING("GetScreenDepth not implemented on this platform -- returning 0!");
    return 0;
}

mozilla::gfx::SurfaceFormat
gfxPlatform::Optimal2DFormatForContent(gfxASurface::gfxContentType aContent)
{
  switch (aContent) {
  case gfxASurface::CONTENT_COLOR:
    switch (GetOffscreenFormat()) {
    case gfxASurface::ImageFormatARGB32:
      return mozilla::gfx::FORMAT_B8G8R8A8;
    case gfxASurface::ImageFormatRGB24:
      return mozilla::gfx::FORMAT_B8G8R8X8;
    case gfxASurface::ImageFormatRGB16_565:
      return mozilla::gfx::FORMAT_R5G6B5;
    default:
      NS_NOTREACHED("unknown gfxImageFormat for CONTENT_COLOR");
      return mozilla::gfx::FORMAT_B8G8R8A8;
    }
  case gfxASurface::CONTENT_ALPHA:
    return mozilla::gfx::FORMAT_A8;
  case gfxASurface::CONTENT_COLOR_ALPHA:
    return mozilla::gfx::FORMAT_B8G8R8A8;
  default:
    NS_NOTREACHED("unknown gfxContentType");
    return mozilla::gfx::FORMAT_B8G8R8A8;
  }
}

gfxImageFormat
gfxPlatform::OptimalFormatForContent(gfxASurface::gfxContentType aContent)
{
  switch (aContent) {
  case gfxASurface::CONTENT_COLOR:
    return GetOffscreenFormat();
  case gfxASurface::CONTENT_ALPHA:
    return gfxASurface::ImageFormatA8;
  case gfxASurface::CONTENT_COLOR_ALPHA:
    return gfxASurface::ImageFormatARGB32;
  default:
    NS_NOTREACHED("unknown gfxContentType");
    return gfxASurface::ImageFormatARGB32;
  }
}

void
gfxPlatform::OrientationSyncPrefsObserverChanged()
{
  mOrientationSyncMillis = Preferences::GetUint("layers.orientation.sync.timeout", (uint32_t)0);
}

uint32_t
gfxPlatform::GetOrientationSyncMillis() const
{
  return mOrientationSyncMillis;
}

/**
 * There are a number of layers acceleration (or layers in general) preferences
 * that should be consistent for the lifetime of the application (bug 840967).
 * As such, we will evaluate them all as soon as one of them is evaluated
 * and remember the values.  Changing these preferences during the run will
 * not have any effect until we restart.
 */
static bool sPrefLayersOffMainThreadCompositionEnabled = false;
static bool sPrefLayersOffMainThreadCompositionTestingEnabled = false;
static bool sPrefLayersAccelerationForceEnabled = false;
static bool sPrefLayersAccelerationDisabled = false;
static bool sPrefLayersPreferOpenGL = false;
static bool sPrefLayersPreferD3D9 = false;

void InitLayersAccelerationPrefs()
{
  static bool sLayersAccelerationPrefsInitialized = false;
  if (!sLayersAccelerationPrefsInitialized)
  {
    sPrefLayersOffMainThreadCompositionEnabled = Preferences::GetBool("layers.offmainthreadcomposition.enabled", false);
    sPrefLayersOffMainThreadCompositionTestingEnabled = Preferences::GetBool("layers.offmainthreadcomposition.testing.enabled", false);
    sPrefLayersAccelerationForceEnabled = Preferences::GetBool("layers.acceleration.force-enabled", false) ||
                                          Preferences::GetBool("browser.tabs.remote", false);
    sPrefLayersAccelerationDisabled = Preferences::GetBool("layers.acceleration.disabled", false);
    sPrefLayersPreferOpenGL = Preferences::GetBool("layers.prefer-opengl", false);
    sPrefLayersPreferD3D9 = Preferences::GetBool("layers.prefer-d3d9", false);

    sLayersAccelerationPrefsInitialized = true;
  }
}

bool gfxPlatform::GetPrefLayersOffMainThreadCompositionEnabled()
{
  InitLayersAccelerationPrefs();
  return sPrefLayersOffMainThreadCompositionEnabled ||
         sPrefLayersOffMainThreadCompositionTestingEnabled;
}

bool gfxPlatform::GetPrefLayersAccelerationForceEnabled()
{
  InitLayersAccelerationPrefs();
  return sPrefLayersAccelerationForceEnabled;
}

bool
gfxPlatform::GetPrefLayersAccelerationDisabled()
{
  InitLayersAccelerationPrefs();
  return sPrefLayersAccelerationDisabled;
}

bool gfxPlatform::GetPrefLayersPreferOpenGL()
{
  InitLayersAccelerationPrefs();
  return sPrefLayersPreferOpenGL;
}

bool gfxPlatform::GetPrefLayersPreferD3D9()
{
  InitLayersAccelerationPrefs();
  return sPrefLayersPreferD3D9;
}
