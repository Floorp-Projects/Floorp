/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/FontPropertyTypes.h"
#include "mozilla/RDDProcessManager.h"
#include "mozilla/image/ImageMemoryReporter.h"
#include "mozilla/layers/CompositorManagerChild.h"
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/layers/ImageBridgeChild.h"
#include "mozilla/layers/ISurfaceAllocator.h"  // for GfxMemoryImageReporter
#include "mozilla/layers/CompositorBridgeChild.h"
#include "mozilla/layers/TiledContentClient.h"
#include "mozilla/webrender/RenderThread.h"
#include "mozilla/webrender/WebRenderAPI.h"
#include "mozilla/webrender/webrender_ffi.h"
#include "mozilla/layers/PaintThread.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/gfx/GPUProcessManager.h"
#include "mozilla/gfx/GraphicsMessages.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Telemetry.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/Unused.h"
#include "mozilla/IntegerPrintfMacros.h"

#include "mozilla/Logging.h"
#include "mozilla/Services.h"
#include "nsAppDirectoryServiceDefs.h"

#include "gfxCrashReporterUtils.h"
#include "gfxPlatform.h"
#include "gfxPrefs.h"
#include "gfxEnv.h"
#include "gfxTextRun.h"
#include "gfxUserFontSet.h"
#include "gfxConfig.h"
#include "VRProcessManager.h"
#include "VRThread.h"

#ifdef XP_WIN
#  include <process.h>
#  define getpid _getpid
#else
#  include <unistd.h>
#endif

#include "nsXULAppAPI.h"
#include "nsDirectoryServiceUtils.h"
#include "nsDirectoryServiceDefs.h"

#if defined(XP_WIN)
#  include "gfxWindowsPlatform.h"
#elif defined(XP_MACOSX)
#  include "gfxPlatformMac.h"
#  include "gfxQuartzSurface.h"
#  include "nsCocoaFeatures.h"
#elif defined(MOZ_WIDGET_GTK)
#  include "gfxPlatformGtk.h"
#elif defined(ANDROID)
#  include "gfxAndroidPlatform.h"
#endif
#if defined(MOZ_WIDGET_ANDROID)
#  include "mozilla/jni/Utils.h"  // for IsFennec
#endif

#ifdef XP_WIN
#  include "mozilla/WindowsVersion.h"
#  include "mozilla/gfx/DeviceManagerDx.h"
#endif

#ifdef MOZ_WAYLAND
#  include "mozilla/widget/nsWaylandDisplayShutdown.h"
#endif

#include "nsGkAtoms.h"
#include "gfxPlatformFontList.h"
#include "gfxContext.h"
#include "gfxImageSurface.h"
#include "nsUnicodeProperties.h"
#include "harfbuzz/hb.h"
#include "gfxGraphiteShaper.h"
#include "gfx2DGlue.h"
#include "gfxGradientCache.h"
#include "gfxUtils.h"  // for NextPowerOfTwo
#include "gfxFontMissingGlyphs.h"

#include "nsExceptionHandler.h"
#include "nsServiceManagerUtils.h"
#include "nsTArray.h"
#include "nsIObserverService.h"
#include "nsIScreenManager.h"
#include "MainThreadUtils.h"

#include "nsWeakReference.h"

#include "cairo.h"
#include "qcms.h"

#include "imgITools.h"

#include "plstr.h"
#include "nsCRT.h"
#include "GLContext.h"
#include "GLContextProvider.h"
#include "mozilla/gfx/Logging.h"

#ifdef USE_SKIA
#  ifdef __GNUC__
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wshadow"
#  endif
#  include "skia/include/core/SkGraphics.h"
#  ifdef MOZ_ENABLE_FREETYPE
#    include "skia/include/ports/SkTypeface_cairo.h"
#  endif
#  include "mozilla/gfx/SkMemoryReporter.h"
#  ifdef __GNUC__
#    pragma GCC diagnostic pop  // -Wshadow
#  endif
static const uint32_t kDefaultGlyphCacheSize = -1;

#endif

#include "mozilla/Preferences.h"
#include "mozilla/Assertions.h"
#include "mozilla/Atomics.h"
#include "mozilla/Attributes.h"
#include "mozilla/Mutex.h"

#include "nsAlgorithm.h"
#include "nsIGfxInfo.h"
#include "nsIXULRuntime.h"
#include "VsyncSource.h"
#include "SoftwareVsyncSource.h"
#include "nscore.h"  // for NS_FREE_PERMANENT_DATA
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/TouchEvent.h"
#include "gfxVR.h"
#include "VRManagerChild.h"
#include "mozilla/gfx/GPUParent.h"
#include "mozilla/layers/MemoryReportingMLGPU.h"
#include "prsystem.h"

using namespace mozilla;
using namespace mozilla::layers;
using namespace mozilla::gl;
using namespace mozilla::gfx;

gfxPlatform* gPlatform = nullptr;
static bool gEverInitialized = false;

static int32_t gLastUsedFrameRate = -1;

const ContentDeviceData* gContentDeviceInitData = nullptr;

static Mutex* gGfxPlatformPrefsLock = nullptr;

// These two may point to the same profile
static qcms_profile* gCMSOutputProfile = nullptr;
static qcms_profile* gCMSsRGBProfile = nullptr;

static bool gCMSRGBTransformFailed = false;
static qcms_transform* gCMSRGBTransform = nullptr;
static qcms_transform* gCMSInverseRGBTransform = nullptr;
static qcms_transform* gCMSRGBATransform = nullptr;

static bool gCMSInitialized = false;
static eCMSMode gCMSMode = eCMSMode_Off;

static void ShutdownCMS();

#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/SourceSurfaceCairo.h"
using namespace mozilla::gfx;

/* Class to listen for pref changes so that chrome code can dynamically
   force sRGB as an output profile. See Bug #452125. */
class SRGBOverrideObserver final : public nsIObserver,
                                   public nsSupportsWeakReference {
  ~SRGBOverrideObserver() = default;

 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
};

/// This override of the LogForwarder, initially used for the critical graphics
/// errors, is sending the log to the crash annotations as well, but only
/// if the capacity set with the method below is >= 2.  We always retain the
/// very first critical message, and the latest capacity-1 messages are
/// rotated through. Note that we don't expect the total number of times
/// this gets called to be large - it is meant for critical errors only.

class CrashStatsLogForwarder : public mozilla::gfx::LogForwarder {
 public:
  explicit CrashStatsLogForwarder(CrashReporter::Annotation aKey);
  void Log(const std::string& aString) override;
  void CrashAction(LogReason aReason) override;
  bool UpdateStringsVector(const std::string& aString) override;

  LoggingRecord LoggingRecordCopy() override;

  void SetCircularBufferSize(uint32_t aCapacity);

 private:
  // Helper for the Log()
  void UpdateCrashReport();

 private:
  LoggingRecord mBuffer;
  CrashReporter::Annotation mCrashCriticalKey;
  uint32_t mMaxCapacity;
  int32_t mIndex;
  Mutex mMutex;
};

CrashStatsLogForwarder::CrashStatsLogForwarder(CrashReporter::Annotation aKey)
    : mBuffer(),
      mCrashCriticalKey(aKey),
      mMaxCapacity(0),
      mIndex(-1),
      mMutex("CrashStatsLogForwarder") {}

void CrashStatsLogForwarder::SetCircularBufferSize(uint32_t aCapacity) {
  MutexAutoLock lock(mMutex);

  mMaxCapacity = aCapacity;
  mBuffer.reserve(static_cast<size_t>(aCapacity));
}

LoggingRecord CrashStatsLogForwarder::LoggingRecordCopy() {
  MutexAutoLock lock(mMutex);
  return mBuffer;
}

bool CrashStatsLogForwarder::UpdateStringsVector(const std::string& aString) {
  // We want at least the first one and the last one.  Otherwise, no point.
  if (mMaxCapacity < 2) {
    return false;
  }

  mIndex += 1;
  MOZ_ASSERT(mIndex >= 0);

  // index will count 0, 1, 2, ..., max-1, 1, 2, ..., max-1, 1, 2, ...
  int32_t index = mIndex ? (mIndex - 1) % (mMaxCapacity - 1) + 1 : 0;
  MOZ_ASSERT(index >= 0 && index < (int32_t)mMaxCapacity);
  MOZ_ASSERT(index <= mIndex && index <= (int32_t)mBuffer.size());

  double tStamp = (TimeStamp::NowLoRes() - TimeStamp::ProcessCreation())
                      .ToSecondsSigDigits();

  // Checking for index >= mBuffer.size(), rather than index == mBuffer.size()
  // just out of paranoia, but we know index <= mBuffer.size().
  LoggingRecordEntry newEntry(mIndex, aString, tStamp);
  if (index >= static_cast<int32_t>(mBuffer.size())) {
    mBuffer.push_back(newEntry);
  } else {
    mBuffer[index] = newEntry;
  }
  return true;
}

void CrashStatsLogForwarder::UpdateCrashReport() {
  std::stringstream message;
  std::string logAnnotation;

  switch (XRE_GetProcessType()) {
    case GeckoProcessType_Default:
      logAnnotation = "|[";
      break;
    case GeckoProcessType_Content:
      logAnnotation = "|[C";
      break;
    case GeckoProcessType_GPU:
      logAnnotation = "|[G";
      break;
    default:
      logAnnotation = "|[X";
      break;
  }

  for (auto& it : mBuffer) {
    message << logAnnotation << Get<0>(it) << "]" << Get<1>(it)
            << " (t=" << Get<2>(it) << ") ";
  }

  nsCString reportString(message.str().c_str());
  nsresult annotated =
      CrashReporter::AnnotateCrashReport(mCrashCriticalKey, reportString);

  if (annotated != NS_OK) {
    printf("Crash Annotation %s: %s",
           CrashReporter::AnnotationToString(mCrashCriticalKey),
           message.str().c_str());
  }
}

class LogForwarderEvent : public Runnable {
  virtual ~LogForwarderEvent() = default;

 public:
  NS_INLINE_DECL_REFCOUNTING_INHERITED(LogForwarderEvent, Runnable)

  explicit LogForwarderEvent(const nsCString& aMessage)
      : mozilla::Runnable("LogForwarderEvent"), mMessage(aMessage) {}

  NS_IMETHOD Run() override {
    MOZ_ASSERT(NS_IsMainThread() &&
               (XRE_IsContentProcess() || XRE_IsGPUProcess()));

    if (XRE_IsContentProcess()) {
      dom::ContentChild* cc = dom::ContentChild::GetSingleton();
      Unused << cc->SendGraphicsError(mMessage);
    } else if (XRE_IsGPUProcess()) {
      GPUParent* gp = GPUParent::GetSingleton();
      Unused << gp->SendGraphicsError(mMessage);
    }

    return NS_OK;
  }

 protected:
  nsCString mMessage;
};

void CrashStatsLogForwarder::Log(const std::string& aString) {
  MutexAutoLock lock(mMutex);

  if (UpdateStringsVector(aString)) {
    UpdateCrashReport();
  }

  // Add it to the parent strings
  if (!XRE_IsParentProcess()) {
    nsCString stringToSend(aString.c_str());
    if (NS_IsMainThread()) {
      if (XRE_IsContentProcess()) {
        dom::ContentChild* cc = dom::ContentChild::GetSingleton();
        Unused << cc->SendGraphicsError(stringToSend);
      } else if (XRE_IsGPUProcess()) {
        GPUParent* gp = GPUParent::GetSingleton();
        Unused << gp->SendGraphicsError(stringToSend);
      }
    } else {
      nsCOMPtr<nsIRunnable> r1 = new LogForwarderEvent(stringToSend);
      NS_DispatchToMainThread(r1);
    }
  }
}

class CrashTelemetryEvent : public Runnable {
  virtual ~CrashTelemetryEvent() = default;

 public:
  NS_INLINE_DECL_REFCOUNTING_INHERITED(CrashTelemetryEvent, Runnable)

  explicit CrashTelemetryEvent(uint32_t aReason)
      : mozilla::Runnable("CrashTelemetryEvent"), mReason(aReason) {}

  NS_IMETHOD Run() override {
    MOZ_ASSERT(NS_IsMainThread());
    Telemetry::Accumulate(Telemetry::GFX_CRASH, mReason);
    return NS_OK;
  }

 protected:
  uint32_t mReason;
};

void CrashStatsLogForwarder::CrashAction(LogReason aReason) {
#ifndef RELEASE_OR_BETA
  // Non-release builds crash by default, but will use telemetry
  // if this environment variable is present.
  static bool useTelemetry = gfxEnv::GfxDevCrashTelemetry();
#else
  // Release builds use telemetry by default, but will crash instead
  // if this environment variable is present.
  static bool useTelemetry = !gfxEnv::GfxDevCrashMozCrash();
#endif

  if (useTelemetry) {
    // The callers need to assure that aReason is in the range
    // that the telemetry call below supports.
    if (NS_IsMainThread()) {
      Telemetry::Accumulate(Telemetry::GFX_CRASH, (uint32_t)aReason);
    } else {
      nsCOMPtr<nsIRunnable> r1 = new CrashTelemetryEvent((uint32_t)aReason);
      NS_DispatchToMainThread(r1);
    }
  } else {
    // ignoring aReason, we can get the information we need from the stack
    MOZ_CRASH("GFX_CRASH");
  }
}

NS_IMPL_ISUPPORTS(SRGBOverrideObserver, nsIObserver, nsISupportsWeakReference)

#define GFX_DOWNLOADABLE_FONTS_ENABLED "gfx.downloadable_fonts.enabled"

#define GFX_PREF_FALLBACK_USE_CMAPS \
  "gfx.font_rendering.fallback.always_use_cmaps"

#define GFX_PREF_OPENTYPE_SVG "gfx.font_rendering.opentype_svg.enabled"

#define GFX_PREF_WORD_CACHE_CHARLIMIT "gfx.font_rendering.wordcache.charlimit"
#define GFX_PREF_WORD_CACHE_MAXENTRIES "gfx.font_rendering.wordcache.maxentries"

#define GFX_PREF_GRAPHITE_SHAPING "gfx.font_rendering.graphite.enabled"

#define BIDI_NUMERAL_PREF "bidi.numeral"

#define GFX_PREF_CMS_FORCE_SRGB "gfx.color_management.force_srgb"

#define FONT_VARIATIONS_PREF "layout.css.font-variations.enabled"

NS_IMETHODIMP
SRGBOverrideObserver::Observe(nsISupports* aSubject, const char* aTopic,
                              const char16_t* someData) {
  NS_ASSERTION(NS_strcmp(someData, (u"" GFX_PREF_CMS_FORCE_SRGB)) == 0,
               "Restarting CMS on wrong pref!");
  ShutdownCMS();
  // Update current cms profile.
  gfxPlatform::CreateCMSOutputProfile();
  return NS_OK;
}

static const char* kObservedPrefs[] = {"gfx.downloadable_fonts.",
                                       "gfx.font_rendering.", BIDI_NUMERAL_PREF,
                                       nullptr};

static void FontPrefChanged(const char* aPref, void* aData) {
  MOZ_ASSERT(aPref);
  NS_ASSERTION(gfxPlatform::GetPlatform(), "the singleton instance has gone");
  gfxPlatform::GetPlatform()->FontsPrefsChanged(aPref);
}

void gfxPlatform::OnMemoryPressure(layers::MemoryPressureReason aWhy) {
  Factory::PurgeAllCaches();
  gfxGradientCache::PurgeAllCaches();
  gfxFontMissingGlyphs::Purge();
  PurgeSkiaFontCache();
  if (XRE_IsParentProcess()) {
    layers::CompositorManagerChild* manager =
        CompositorManagerChild::GetInstance();
    if (manager) {
      manager->SendNotifyMemoryPressure();
    }
  }
}

gfxPlatform::gfxPlatform()
    : mHasVariationFontSupport(false),
      mAzureCanvasBackendCollector(this, &gfxPlatform::GetAzureBackendInfo),
      mApzSupportCollector(this, &gfxPlatform::GetApzSupportInfo),
      mTilesInfoCollector(this, &gfxPlatform::GetTilesSupportInfo),
      mFrameStatsCollector(this, &gfxPlatform::GetFrameStats),
      mCompositorBackend(layers::LayersBackend::LAYERS_NONE),
      mScreenDepth(0) {
  mAllowDownloadableFonts = UNINITIALIZED_VALUE;
  mFallbackUsesCmaps = UNINITIALIZED_VALUE;

  mWordCacheCharLimit = UNINITIALIZED_VALUE;
  mWordCacheMaxEntries = UNINITIALIZED_VALUE;
  mGraphiteShapingEnabled = UNINITIALIZED_VALUE;
  mOpenTypeSVGEnabled = UNINITIALIZED_VALUE;
  mBidiNumeralOption = UNINITIALIZED_VALUE;

  InitBackendPrefs(GetBackendPrefs());

  mTotalSystemMemory = PR_GetPhysicalMemorySize();

  VRManager::ManagerInit();
}

gfxPlatform* gfxPlatform::GetPlatform() {
  if (!gPlatform) {
    MOZ_RELEASE_ASSERT(!XRE_IsContentProcess(),
                       "Content Process should have called InitChild() before "
                       "first GetPlatform()");
    Init();
  }
  return gPlatform;
}

bool gfxPlatform::Initialized() { return !!gPlatform; }

/* static */
void gfxPlatform::InitChild(const ContentDeviceData& aData) {
  MOZ_ASSERT(XRE_IsContentProcess());
  MOZ_RELEASE_ASSERT(!gPlatform,
                     "InitChild() should be called before first GetPlatform()");
  // Make the provided initial ContentDeviceData available to the init
  // routines, so they don't have to do a sync request from the parent.
  gContentDeviceInitData = &aData;
  Init();
  gContentDeviceInitData = nullptr;
}

void RecordingPrefChanged(const char* aPrefName, void* aClosure) {
  if (Preferences::GetBool("gfx.2d.recording", false)) {
    nsAutoCString fileName;
    nsAutoString prefFileName;
    nsresult rv = Preferences::GetString("gfx.2d.recordingfile", prefFileName);
    if (NS_SUCCEEDED(rv)) {
      CopyUTF16toUTF8(prefFileName, fileName);
    } else {
      nsCOMPtr<nsIFile> tmpFile;
      if (NS_FAILED(NS_GetSpecialDirectory(NS_OS_TEMP_DIR,
                                           getter_AddRefs(tmpFile)))) {
        return;
      }
      fileName.AppendPrintf("moz2drec_%i_%i.aer", XRE_GetProcessType(),
                            getpid());

      nsresult rv = tmpFile->AppendNative(fileName);
      if (NS_FAILED(rv)) return;

#ifdef XP_WIN
      rv = tmpFile->GetPath(prefFileName);
      CopyUTF16toUTF8(prefFileName, fileName);
#else
      rv = tmpFile->GetNativePath(fileName);
#endif
      if (NS_FAILED(rv)) return;
    }

#ifdef XP_WIN
    gPlatform->mRecorder =
        Factory::CreateEventRecorderForFile(prefFileName.BeginReading());
#else
    gPlatform->mRecorder =
        Factory::CreateEventRecorderForFile(fileName.BeginReading());
#endif
    printf_stderr("Recording to %s\n", fileName.get());
    Factory::SetGlobalEventRecorder(gPlatform->mRecorder);
  } else {
    Factory::SetGlobalEventRecorder(nullptr);
  }
}

#define WR_DEBUG_PREF "gfx.webrender.debug"

static void WebRenderDebugPrefChangeCallback(const char* aPrefName, void*) {
  wr::DebugFlags flags{0};
#define GFX_WEBRENDER_DEBUG(suffix, bit)                   \
  if (Preferences::GetBool(WR_DEBUG_PREF suffix, false)) { \
    flags |= (bit);                                        \
  }

  GFX_WEBRENDER_DEBUG(".profiler", wr::DebugFlags_PROFILER_DBG)
  GFX_WEBRENDER_DEBUG(".render-targets", wr::DebugFlags_RENDER_TARGET_DBG)
  GFX_WEBRENDER_DEBUG(".texture-cache", wr::DebugFlags_TEXTURE_CACHE_DBG)
  GFX_WEBRENDER_DEBUG(".gpu-time-queries", wr::DebugFlags_GPU_TIME_QUERIES)
  GFX_WEBRENDER_DEBUG(".gpu-sample-queries", wr::DebugFlags_GPU_SAMPLE_QUERIES)
  GFX_WEBRENDER_DEBUG(".disable-batching", wr::DebugFlags_DISABLE_BATCHING)
  GFX_WEBRENDER_DEBUG(".epochs", wr::DebugFlags_EPOCHS)
  GFX_WEBRENDER_DEBUG(".compact-profiler", wr::DebugFlags_COMPACT_PROFILER)
  GFX_WEBRENDER_DEBUG(".echo-driver-messages",
                      wr::DebugFlags_ECHO_DRIVER_MESSAGES)
  GFX_WEBRENDER_DEBUG(".new-frame-indicator",
                      wr::DebugFlags_NEW_FRAME_INDICATOR)
  GFX_WEBRENDER_DEBUG(".new-scene-indicator",
                      wr::DebugFlags_NEW_SCENE_INDICATOR)
  GFX_WEBRENDER_DEBUG(".show-overdraw", wr::DebugFlags_SHOW_OVERDRAW)
  GFX_WEBRENDER_DEBUG(".gpu-cache", wr::DebugFlags_GPU_CACHE_DBG)
  GFX_WEBRENDER_DEBUG(".slow-frame-indicator",
                      wr::DebugFlags_SLOW_FRAME_INDICATOR)
  GFX_WEBRENDER_DEBUG(".texture-cache.clear-evicted",
                      wr::DebugFlags_TEXTURE_CACHE_DBG_CLEAR_EVICTED)
  GFX_WEBRENDER_DEBUG(".picture-caching", wr::DebugFlags_PICTURE_CACHING_DBG)
  GFX_WEBRENDER_DEBUG(".primitives", wr::DebugFlags_PRIMITIVE_DBG)
  // Bit 18 is for the zoom display, which requires the mouse position and thus
  // currently only works in wrench.
  GFX_WEBRENDER_DEBUG(".small-screen", wr::DebugFlags_SMALL_SCREEN)
  GFX_WEBRENDER_DEBUG(".disable-opaque-pass",
                      wr::DebugFlags_DISABLE_OPAQUE_PASS)
  GFX_WEBRENDER_DEBUG(".disable-alpha-pass", wr::DebugFlags_DISABLE_ALPHA_PASS)
  GFX_WEBRENDER_DEBUG(".disable-clip-masks", wr::DebugFlags_DISABLE_CLIP_MASKS)
  GFX_WEBRENDER_DEBUG(".disable-text-prims", wr::DebugFlags_DISABLE_TEXT_PRIMS)
  GFX_WEBRENDER_DEBUG(".disable-gradient-prims",
                      wr::DebugFlags_DISABLE_GRADIENT_PRIMS)
#undef GFX_WEBRENDER_DEBUG

  gfx::gfxVars::SetWebRenderDebugFlags(flags.bits);
}

#if defined(USE_SKIA)
static uint32_t GetSkiaGlyphCacheSize() {
  // Only increase font cache size on non-android to save memory.
#  if !defined(MOZ_WIDGET_ANDROID)
  // 10mb as the default pref cache size on desktop due to talos perf tweaking.
  // Chromium uses 20mb and skia default uses 2mb.
  // We don't need to change the font cache count since we usually
  // cache thrash due to asian character sets in talos.
  // Only increase memory on the content process
  uint32_t cacheSize = gfxPrefs::SkiaContentFontCacheSize() * 1024 * 1024;
  if (mozilla::BrowserTabsRemoteAutostart()) {
    return XRE_IsContentProcess() ? cacheSize : kDefaultGlyphCacheSize;
  }

  return cacheSize;
#  else
  return kDefaultGlyphCacheSize;
#  endif  // MOZ_WIDGET_ANDROID
}
#endif

class WebRenderMemoryReporter final : public nsIMemoryReporter {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIMEMORYREPORTER

 private:
  ~WebRenderMemoryReporter() = default;
};

// Memory reporter for WebRender.
//
// The reporting within WebRender is manual and incomplete. We could do a much
// more thorough job by depending on the malloc_size_of crate, but integrating
// that into WebRender is tricky [1].
//
// So the idea is to start with manual reporting for the large allocations
// detected by DMD, and see how much that can cover in practice (which may
// require a few rounds of iteration). If that approach turns out to be
// fundamentally insufficient, we can either duplicate more of the
// malloc_size_of functionality in WebRender, or deal with the complexity of a
// gecko-only crate dependency.
//
// [1] See https://bugzilla.mozilla.org/show_bug.cgi?id=1480293#c1
struct WebRenderMemoryReporterHelper {
  WebRenderMemoryReporterHelper(nsIHandleReportCallback* aCallback,
                                nsISupports* aData)
      : mCallback(aCallback), mData(aData) {}
  nsCOMPtr<nsIHandleReportCallback> mCallback;
  nsCOMPtr<nsISupports> mData;

  void Report(size_t aBytes, const char* aName) const {
    nsPrintfCString path("explicit/gfx/webrender/%s", aName);
    nsCString desc(NS_LITERAL_CSTRING("CPU heap memory used by WebRender"));
    ReportInternal(aBytes, path, desc, nsIMemoryReporter::KIND_HEAP);
  }

  void ReportTexture(size_t aBytes, const char* aName) const {
    nsPrintfCString path("gfx/webrender/textures/%s", aName);
    nsCString desc(NS_LITERAL_CSTRING("GPU texture memory used by WebRender"));
    ReportInternal(aBytes, path, desc, nsIMemoryReporter::KIND_OTHER);
  }

  void ReportTotalGPUBytes(size_t aBytes) const {
    nsCString path(NS_LITERAL_CSTRING("gfx/webrender/total-gpu-bytes"));
    nsCString desc(NS_LITERAL_CSTRING(
        "Total GPU bytes used by WebRender (should match textures/ sum)"));
    ReportInternal(aBytes, path, desc, nsIMemoryReporter::KIND_OTHER);
  }

  void ReportInternal(size_t aBytes, nsACString& aPath, nsACString& aDesc,
                      int32_t aKind) const {
    // Generally, memory reporters pass the empty string as the process name to
    // indicate "current process". However, if we're using a GPU process, the
    // measurements will actually take place in that process, and it's easier to
    // just note that here rather than trying to invoke the memory reporter in
    // the GPU process.
    nsAutoCString processName;
    if (gfxConfig::IsEnabled(Feature::GPU_PROCESS)) {
      GPUParent::GetGPUProcessName(processName);
    }

    mCallback->Callback(processName, aPath, aKind,
                        nsIMemoryReporter::UNITS_BYTES, aBytes, aDesc, mData);
  }
};

static void FinishAsyncMemoryReport() {
  nsCOMPtr<nsIMemoryReporterManager> imgr =
      do_GetService("@mozilla.org/memory-reporter-manager;1");
  if (imgr) {
    imgr->EndReport();
  }
}

// clang-format off
// (For some reason, clang-format gets the second macro right, but totally mangles the first).
#define REPORT_INTERNER(id)                      \
  helper.Report(aReport.interning.interners.id, \
                "interning/" #id "/interners");
// clang-format on

#define REPORT_DATA_STORE(id)                     \
  helper.Report(aReport.interning.data_stores.id, \
                "interning/" #id "/data-stores");

NS_IMPL_ISUPPORTS(WebRenderMemoryReporter, nsIMemoryReporter)

NS_IMETHODIMP
WebRenderMemoryReporter::CollectReports(nsIHandleReportCallback* aHandleReport,
                                        nsISupports* aData, bool aAnonymize) {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());
  layers::CompositorManagerChild* manager =
      CompositorManagerChild::GetInstance();
  if (!manager) {
    FinishAsyncMemoryReport();
    return NS_OK;
  }

  WebRenderMemoryReporterHelper helper(aHandleReport, aData);
  manager->SendReportMemory(
      [=](wr::MemoryReport aReport) {
        // CPU Memory.
        helper.Report(aReport.clip_stores, "clip-stores");
        helper.Report(aReport.gpu_cache_metadata, "gpu-cache/metadata");
        helper.Report(aReport.gpu_cache_cpu_mirror, "gpu-cache/cpu-mirror");
        helper.Report(aReport.render_tasks, "render-tasks");
        helper.Report(aReport.hit_testers, "hit-testers");
        helper.Report(aReport.fonts, "resource-cache/fonts");
        helper.Report(aReport.images, "resource-cache/images");
        helper.Report(aReport.rasterized_blobs,
                      "resource-cache/rasterized-blobs");
        helper.Report(aReport.shader_cache, "shader-cache");

        WEBRENDER_FOR_EACH_INTERNER(REPORT_INTERNER);
        WEBRENDER_FOR_EACH_INTERNER(REPORT_DATA_STORE);

        // GPU Memory.
        helper.ReportTexture(aReport.gpu_cache_textures, "gpu-cache");
        helper.ReportTexture(aReport.vertex_data_textures, "vertex-data");
        helper.ReportTexture(aReport.render_target_textures, "render-targets");
        helper.ReportTexture(aReport.texture_cache_textures, "texture-cache");
        helper.ReportTexture(aReport.depth_target_textures, "depth-targets");
        helper.ReportTexture(aReport.swap_chain, "swap-chains");

        FinishAsyncMemoryReport();
      },
      [](mozilla::ipc::ResponseRejectReason&& aReason) {
        FinishAsyncMemoryReport();
      });

  return NS_OK;
}

#undef REPORT_INTERNER
#undef REPORT_DATA_STORE

static const char* const WR_ROLLOUT_PREF = "gfx.webrender.all.qualified";
static const char* const WR_ROLLOUT_PREF_DEFAULT =
    "gfx.webrender.all.qualified.default";
static const char* const WR_ROLLOUT_PREF_OVERRIDE =
    "gfx.webrender.all.qualified.gfxPref-default-override";
static const char* const WR_ROLLOUT_HW_QUALIFIED_OVERRIDE =
    "gfx.webrender.all.qualified.hardware-override";
static const char* const PROFILE_BEFORE_CHANGE_TOPIC = "profile-before-change";

// If the "gfx.webrender.all.qualified" pref is true we want to enable
// WebRender for qualified hardware. This pref may be set by the Normandy
// Preference Rollout feature. The Normandy pref rollout code sets default
// values on rolled out prefs on every startup. Default pref values are not
// persisted; they only exist in memory for that session. Gfx starts up
// before Normandy does. So it's too early to observe the WR qualified pref
// changed by Normandy rollout on gfx startup. So we add a shutdown observer to
// save the default value on shutdown, and read the saved value on startup
// instead.
class WrRolloutPrefShutdownSaver : public nsIObserver {
 public:
  NS_DECL_ISUPPORTS

  NS_IMETHOD Observe(nsISupports*, const char* aTopic,
                     const char16_t*) override {
    if (strcmp(PROFILE_BEFORE_CHANGE_TOPIC, aTopic) != 0) {
      // Not the observer we're looking for, move along.
      return NS_OK;
    }

    SaveRolloutPref();

    // Shouldn't receive another notification, remove the observer.
    RefPtr<WrRolloutPrefShutdownSaver> kungFuDeathGrip(this);
    nsCOMPtr<nsIObserverService> observerService =
        mozilla::services::GetObserverService();
    if (NS_WARN_IF(!observerService)) {
      return NS_ERROR_FAILURE;
    }
    observerService->RemoveObserver(this, PROFILE_BEFORE_CHANGE_TOPIC);
    return NS_OK;
  }

  static void AddShutdownObserver() {
    MOZ_ASSERT(XRE_IsParentProcess());
    nsCOMPtr<nsIObserverService> observerService =
        mozilla::services::GetObserverService();
    if (NS_WARN_IF(!observerService)) {
      return;
    }
    RefPtr<WrRolloutPrefShutdownSaver> wrRolloutSaver =
        new WrRolloutPrefShutdownSaver();
    observerService->AddObserver(wrRolloutSaver, PROFILE_BEFORE_CHANGE_TOPIC,
                                 false);
  }

 private:
  virtual ~WrRolloutPrefShutdownSaver() = default;

  void SaveRolloutPref() {
    if (Preferences::HasUserValue(WR_ROLLOUT_PREF) ||
        Preferences::GetType(WR_ROLLOUT_PREF) == nsIPrefBranch::PREF_INVALID) {
      // Don't need to create a backup of default value, because either:
      // 1. the user or the WR SHIELD study has set a user pref value, or
      // 2. we've not had a default pref set by Normandy that needs to be saved
      //    for reading before Normandy has started up.
      return;
    }

    bool defaultValue =
        Preferences::GetBool(WR_ROLLOUT_PREF, false, PrefValueKind::Default);
    Preferences::SetBool(WR_ROLLOUT_PREF_DEFAULT, defaultValue);
  }
};

NS_IMPL_ISUPPORTS(WrRolloutPrefShutdownSaver, nsIObserver)

void gfxPlatform::Init() {
  MOZ_RELEASE_ASSERT(!XRE_IsGPUProcess(), "GFX: Not allowed in GPU process.");
  MOZ_RELEASE_ASSERT(!XRE_IsRDDProcess(), "GFX: Not allowed in RDD process.");
  MOZ_RELEASE_ASSERT(NS_IsMainThread(), "GFX: Not in main thread.");

  if (gEverInitialized) {
    MOZ_CRASH("Already started???");
  }
  gEverInitialized = true;

  // Initialize the preferences by creating the singleton.
  gfxPrefs::GetSingleton();
  gfxVars::Initialize();

  gfxConfig::Init();

  if (XRE_IsParentProcess() || recordreplay::IsRecordingOrReplaying()) {
    GPUProcessManager::Initialize();
    RDDProcessManager::Initialize();

    if (Preferences::GetBool("media.wmf.skip-blacklist")) {
      gfxVars::SetPDMWMFDisableD3D11Dlls(nsCString());
      gfxVars::SetPDMWMFDisableD3D9Dlls(nsCString());
    } else {
      nsAutoCString d3d11;
      Preferences::GetCString("media.wmf.disable-d3d11-for-dlls", d3d11);
      gfxVars::SetPDMWMFDisableD3D11Dlls(d3d11);
      nsAutoCString d3d9;
      Preferences::GetCString("media.wmf.disable-d3d9-for-dlls", d3d9);
      gfxVars::SetPDMWMFDisableD3D9Dlls(d3d9);
    }

    nsCOMPtr<nsIFile> file;
    nsresult rv = NS_GetSpecialDirectory(NS_GRE_DIR, getter_AddRefs(file));
    if (NS_FAILED(rv)) {
      gfxVars::SetGREDirectory(nsString());
    } else {
      nsAutoString path;
      file->GetPath(path);
      gfxVars::SetGREDirectory(nsString(path));
    }
  }

  if (XRE_IsParentProcess()) {
    WrRolloutPrefShutdownSaver::AddShutdownObserver();

    nsCOMPtr<nsIFile> profDir;
    nsresult rv = NS_GetSpecialDirectory(NS_APP_PROFILE_DIR_STARTUP,
                                         getter_AddRefs(profDir));
    if (NS_FAILED(rv)) {
      gfxVars::SetProfDirectory(nsString());
    } else {
      nsAutoString path;
      profDir->GetPath(path);
      gfxVars::SetProfDirectory(nsString(path));
    }

    gfxUtils::RemoveShaderCacheFromDiskIfNecessary();
  }

  // Drop a note in the crash report if we end up forcing an option that could
  // destabilize things.  New items should be appended at the end (of an
  // existing or in a new section), so that we don't have to know the version to
  // interpret these cryptic strings.
  {
    nsAutoCString forcedPrefs;
    // D2D prefs
    forcedPrefs.AppendPrintf("FP(D%d%d", gfxPrefs::Direct2DDisabled(),
                             gfxPrefs::Direct2DForceEnabled());
    // Layers prefs
    forcedPrefs.AppendPrintf(
        "-L%d%d%d%d", gfxPrefs::LayersAMDSwitchableGfxEnabled(),
        gfxPrefs::LayersAccelerationDisabledDoNotUseDirectly(),
        gfxPrefs::LayersAccelerationForceEnabledDoNotUseDirectly(),
        gfxPrefs::LayersD3D11ForceWARP());
    // WebGL prefs
    forcedPrefs.AppendPrintf(
        "-W%d%d%d%d%d%d%d%d", gfxPrefs::WebGLANGLEForceD3D11(),
        gfxPrefs::WebGLANGLEForceWARP(), gfxPrefs::WebGLDisabled(),
        gfxPrefs::WebGLDisableANGLE(), gfxPrefs::WebGLDXGLEnabled(),
        gfxPrefs::WebGLForceEnabled(), gfxPrefs::WebGLForceLayersReadback(),
        gfxPrefs::WebGLForceMSAA());
    // Prefs that don't fit into any of the other sections
    forcedPrefs.AppendPrintf("-T%d%d%d) ", gfxPrefs::AndroidRGB16Force(),
                             0,  // SkiaGL canvas no longer supported
                             gfxPrefs::ForceShmemTiles());
    ScopedGfxFeatureReporter::AppNote(forcedPrefs);
  }

  InitMoz2DLogging();

  gGfxPlatformPrefsLock = new Mutex("gfxPlatform::gGfxPlatformPrefsLock");

  /* Initialize the GfxInfo service.
   * Note: we can't call functions on GfxInfo that depend
   * on gPlatform until after it has been initialized
   * below. GfxInfo initialization annotates our
   * crash reports so we want to do it before
   * we try to load any drivers and do device detection
   * incase that code crashes. See bug #591561. */
  nsCOMPtr<nsIGfxInfo> gfxInfo;
  /* this currently will only succeed on Windows */
  gfxInfo = services::GetGfxInfo();

  if (XRE_IsParentProcess()) {
    // Some gfxVars must be initialized prior gPlatform for coherent results.
    gfxVars::SetDXInterop2Blocked(IsDXInterop2Blocked());
    gfxVars::SetDXNV12Blocked(IsDXNV12Blocked());
    gfxVars::SetDXP010Blocked(IsDXP010Blocked());
    gfxVars::SetDXP016Blocked(IsDXP016Blocked());
  }

#if defined(XP_WIN)
  gPlatform = new gfxWindowsPlatform;
#elif defined(XP_MACOSX)
  gPlatform = new gfxPlatformMac;
#elif defined(MOZ_WIDGET_GTK)
  gPlatform = new gfxPlatformGtk;
#elif defined(ANDROID)
  gPlatform = new gfxAndroidPlatform;
#else
#  error "No gfxPlatform implementation available"
#endif
  gPlatform->PopulateScreenInfo();
  gPlatform->InitAcceleration();
  gPlatform->InitWebRenderConfig();
  // When using WebRender, we defer initialization of the D3D11 devices until
  // the (rare) cases where they're used. Note that the GPU process where
  // WebRender runs doesn't initialize gfxPlatform and performs explicit
  // initialization of the bits it needs.
  if (!gfxVars::UseWebRender()) {
    gPlatform->EnsureDevicesInitialized();
  }
  gPlatform->InitOMTPConfig();

  if (gfxConfig::IsEnabled(Feature::GPU_PROCESS)) {
    GPUProcessManager* gpu = GPUProcessManager::Get();
    gpu->LaunchGPUProcess();
  }

  gLastUsedFrameRate = ForceSoftwareVsync() ? GetSoftwareVsyncRate() : -1;
  auto updateFrameRateCallback = [](const GfxPrefValue& aValue) -> void {
    int32_t newRate = ForceSoftwareVsync() ? GetSoftwareVsyncRate() : -1;
    if (newRate != gLastUsedFrameRate) {
      gLastUsedFrameRate = newRate;
      ReInitFrameRate();
    }
  };
  gfxPrefs::SetLayoutFrameRateChangeCallback(updateFrameRateCallback);
  // Set up the vsync source for the parent process.
  ReInitFrameRate();

#ifdef USE_SKIA
  SkGraphics::Init();
#  ifdef MOZ_ENABLE_FREETYPE
  SkInitCairoFT(gPlatform->FontHintingEnabled());
#  endif
#endif

  InitLayersIPC();

  gPlatform->ComputeTileSize();

#ifdef MOZ_ENABLE_FREETYPE
  Factory::SetFTLibrary(gPlatform->GetFTLibrary());
#endif

  gPlatform->mHasVariationFontSupport = gPlatform->CheckVariationFontSupport();

  nsresult rv;
  rv = gfxPlatformFontList::Init();
  if (NS_FAILED(rv)) {
    MOZ_CRASH("Could not initialize gfxPlatformFontList");
  }

  gPlatform->mScreenReferenceSurface = gPlatform->CreateOffscreenSurface(
      IntSize(1, 1), SurfaceFormat::A8R8G8B8_UINT32);
  if (!gPlatform->mScreenReferenceSurface) {
    MOZ_CRASH("Could not initialize mScreenReferenceSurface");
  }

  gPlatform->mScreenReferenceDrawTarget =
      gPlatform->CreateOffscreenContentDrawTarget(IntSize(1, 1),
                                                  SurfaceFormat::B8G8R8A8);
  if (!gPlatform->mScreenReferenceDrawTarget ||
      !gPlatform->mScreenReferenceDrawTarget->IsValid()) {
    // If TDR is detected, create a draw target with software backend
    // and it should be replaced later when the process gets the device
    // reset notification.
    if (!gPlatform->DidRenderingDeviceReset()) {
      gfxCriticalError() << "Could not initialize mScreenReferenceDrawTarget";
    }
  }

  rv = gfxFontCache::Init();
  if (NS_FAILED(rv)) {
    MOZ_CRASH("Could not initialize gfxFontCache");
  }

  /* Create and register our CMS Override observer. */
  gPlatform->mSRGBOverrideObserver = new SRGBOverrideObserver();
  Preferences::AddWeakObserver(gPlatform->mSRGBOverrideObserver,
                               GFX_PREF_CMS_FORCE_SRGB);

  Preferences::RegisterPrefixCallbacks(FontPrefChanged, kObservedPrefs);

  GLContext::PlatformStartup();

  Preferences::RegisterCallbackAndCall(RecordingPrefChanged,
                                       "gfx.2d.recording");

  CreateCMSOutputProfile();

  // Listen to memory pressure event so we can purge DrawTarget caches
  gPlatform->mMemoryPressureObserver =
      layers::MemoryPressureObserver::Create(gPlatform);

  // Request the imgITools service, implicitly initializing ImageLib.
  nsCOMPtr<imgITools> imgTools = do_GetService("@mozilla.org/image/tools;1");
  if (!imgTools) {
    MOZ_CRASH("Could not initialize ImageLib");
  }

  RegisterStrongMemoryReporter(new GfxMemoryImageReporter());
  if (XRE_IsParentProcess() && gfxVars::UseWebRender()) {
    RegisterStrongAsyncMemoryReporter(new WebRenderMemoryReporter());
  }

#ifdef USE_SKIA
  RegisterStrongMemoryReporter(new SkMemoryReporter());
#endif
  mlg::InitializeMemoryReporters();

#ifdef USE_SKIA
  uint32_t skiaCacheSize = GetSkiaGlyphCacheSize();
  if (skiaCacheSize != kDefaultGlyphCacheSize) {
    SkGraphics::SetFontCacheLimit(skiaCacheSize);
  }
#endif

  InitNullMetadata();
  InitOpenGLConfig();

  if (XRE_IsParentProcess()) {
    Preferences::Unlock(FONT_VARIATIONS_PREF);
    if (!gPlatform->HasVariationFontSupport()) {
      // Ensure variation fonts are disabled and the pref is locked.
      Preferences::SetBool(FONT_VARIATIONS_PREF, false, PrefValueKind::Default);
      Preferences::SetBool(FONT_VARIATIONS_PREF, false);
      Preferences::Lock(FONT_VARIATIONS_PREF);
    }
  }

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (obs) {
    obs->NotifyObservers(nullptr, "gfx-features-ready", nullptr);
  }
}

static bool IsFeatureSupported(long aFeature) {
  nsCOMPtr<nsIGfxInfo> gfxInfo = services::GetGfxInfo();
  nsCString blockId;
  int32_t status;
  if (!NS_SUCCEEDED(gfxInfo->GetFeatureStatus(aFeature, blockId, &status))) {
    return true;
  }
  return status != nsIGfxInfo::FEATURE_STATUS_OK;
}
/* static*/
bool gfxPlatform::IsDXInterop2Blocked() {
  return IsFeatureSupported(nsIGfxInfo::FEATURE_DX_INTEROP2);
}

/* static*/
bool gfxPlatform::IsDXNV12Blocked() {
  return IsFeatureSupported(nsIGfxInfo::FEATURE_DX_NV12);
}

/* static*/
bool gfxPlatform::IsDXP010Blocked() {
  return IsFeatureSupported(nsIGfxInfo::FEATURE_DX_P010);
}

/* static*/
bool gfxPlatform::IsDXP016Blocked() {
  return IsFeatureSupported(nsIGfxInfo::FEATURE_DX_P016);
}

/* static */
int32_t gfxPlatform::MaxTextureSize() {
  // Make sure we don't completely break rendering because of a typo in the
  // pref or whatnot.
  const int32_t kMinSizePref = 2048;
  return std::max(kMinSizePref, gfxPrefs::MaxTextureSizeDoNotUseDirectly());
}

/* static */
int32_t gfxPlatform::MaxAllocSize() {
  // Make sure we don't completely break rendering because of a typo in the
  // pref or whatnot.
  const int32_t kMinAllocPref = 10000000;
  return std::max(kMinAllocPref, gfxPrefs::MaxAllocSizeDoNotUseDirectly());
}

/* static */
void gfxPlatform::InitMoz2DLogging() {
  auto fwd = new CrashStatsLogForwarder(
      CrashReporter::Annotation::GraphicsCriticalError);
  fwd->SetCircularBufferSize(gfxPrefs::GfxLoggingCrashLength());

  mozilla::gfx::Config cfg;
  cfg.mLogForwarder = fwd;
  cfg.mMaxTextureSize = gfxPlatform::MaxTextureSize();
  cfg.mMaxAllocSize = gfxPlatform::MaxAllocSize();

  gfx::Factory::Init(cfg);
}

/* static */
bool gfxPlatform::IsHeadless() {
  static bool initialized = false;
  static bool headless = false;
  if (!initialized) {
    initialized = true;
    headless = PR_GetEnv("MOZ_HEADLESS");
  }
  return headless;
}

static bool sLayersIPCIsUp = false;

/* static */
void gfxPlatform::InitNullMetadata() {
  ScrollMetadata::sNullMetadata = new ScrollMetadata();
  ClearOnShutdown(&ScrollMetadata::sNullMetadata);
}

void gfxPlatform::Shutdown() {
  // In some cases, gPlatform may not be created but Shutdown() called,
  // e.g., during xpcshell tests.
  if (!gPlatform) {
    return;
  }

  MOZ_ASSERT(!sLayersIPCIsUp);

  // These may be called before the corresponding subsystems have actually
  // started up. That's OK, they can handle it.
  gfxFontCache::Shutdown();
  gfxGradientCache::Shutdown();
  gfxAlphaBoxBlur::ShutdownBlurCache();
  gfxGraphiteShaper::Shutdown();
  gfxPlatformFontList::Shutdown();
  gfxFontMissingGlyphs::Shutdown();
  ShutdownTileCache();

  // Free the various non-null transforms and loaded profiles
  ShutdownCMS();

  /* Unregister our CMS Override callback. */
  NS_ASSERTION(gPlatform->mSRGBOverrideObserver,
               "mSRGBOverrideObserver has alreay gone");
  Preferences::RemoveObserver(gPlatform->mSRGBOverrideObserver,
                              GFX_PREF_CMS_FORCE_SRGB);
  gPlatform->mSRGBOverrideObserver = nullptr;

  Preferences::UnregisterPrefixCallbacks(FontPrefChanged, kObservedPrefs);

  NS_ASSERTION(gPlatform->mMemoryPressureObserver,
               "mMemoryPressureObserver has already gone");
  if (gPlatform->mMemoryPressureObserver) {
    gPlatform->mMemoryPressureObserver->Unregister();
    gPlatform->mMemoryPressureObserver = nullptr;
  }

  if (XRE_IsParentProcess()) {
    gPlatform->mVsyncSource->Shutdown();
  }

  gPlatform->mVsyncSource = nullptr;

  // Shut down the default GL context provider.
  GLContextProvider::Shutdown();

#if defined(XP_WIN)
  // The above shutdown calls operate on the available context providers on
  // most platforms.  Windows is a "special snowflake", though, and has three
  // context providers available, so we have to shut all of them down.
  // We should only support the default GL provider on Windows; then, this
  // could go away. Unfortunately, we currently support WGL (the default) for
  // WebGL on Optimus.
  GLContextProviderEGL::Shutdown();
#endif

  if (XRE_IsParentProcess()) {
    GPUProcessManager::Shutdown();
    VRProcessManager::Shutdown();
    RDDProcessManager::Shutdown();
  }

  gfx::Factory::ShutDown();

  delete gGfxPlatformPrefsLock;

  gfxVars::Shutdown();
  gfxPrefs::DestroySingleton();
  gfxFont::DestroySingletons();

  gfxConfig::Shutdown();

  gPlatform->WillShutdown();

  delete gPlatform;
  gPlatform = nullptr;
}

/* static */
void gfxPlatform::InitLayersIPC() {
  if (sLayersIPCIsUp) {
    return;
  }
  sLayersIPCIsUp = true;

  if (XRE_IsContentProcess()) {
    if (gfxVars::UseOMTP() && !recordreplay::IsRecordingOrReplaying()) {
      layers::PaintThread::Start();
    }
  }

  if (XRE_IsParentProcess() || recordreplay::IsRecordingOrReplaying()) {
    if (!gfxConfig::IsEnabled(Feature::GPU_PROCESS) &&
        gfxVars::UseWebRender()) {
      wr::RenderThread::Start();
      image::ImageMemoryReporter::InitForWebRender();
    }

    layers::CompositorThreadHolder::Start();
  }
}

/* static */
void gfxPlatform::ShutdownLayersIPC() {
  if (!sLayersIPCIsUp) {
    return;
  }
  sLayersIPCIsUp = false;

  if (XRE_IsContentProcess()) {
    gfx::VRManagerChild::ShutDown();
    // cf bug 1215265.
    if (gfxPrefs::ChildProcessShutdown()) {
      layers::CompositorManagerChild::Shutdown();
      layers::ImageBridgeChild::ShutDown();
    }

    if (gfxVars::UseOMTP() && !recordreplay::IsRecordingOrReplaying()) {
      layers::PaintThread::Shutdown();
    }
  } else if (XRE_IsParentProcess()) {
#ifdef MOZ_WAYLAND
    widget::WaylandDisplayShutdown();
#endif
    gfx::VRManagerChild::ShutDown();
    layers::CompositorManagerChild::Shutdown();
    layers::ImageBridgeChild::ShutDown();
    // This has to happen after shutting down the child protocols.
    layers::CompositorThreadHolder::Shutdown();
    image::ImageMemoryReporter::ShutdownForWebRender();
    // There is a case that RenderThread exists when gfxVars::UseWebRender() is
    // false. This could happen when WebRender was fallbacked to compositor.
    if (wr::RenderThread::Get()) {
      wr::RenderThread::ShutDown();

      Preferences::UnregisterCallback(WebRenderDebugPrefChangeCallback,
                                      WR_DEBUG_PREF);
    }

  } else {
    // TODO: There are other kind of processes and we should make sure gfx
    // stuff is either not created there or shut down properly.
  }
}

void gfxPlatform::WillShutdown() {
  // Destoy these first in case they depend on backend-specific resources.
  // Otherwise, the backend's destructor would be called before the
  // base gfxPlatform destructor.
  mScreenReferenceSurface = nullptr;
  mScreenReferenceDrawTarget = nullptr;
}

gfxPlatform::~gfxPlatform() {
  // The cairo folks think we should only clean up in debug builds,
  // but we're generally in the habit of trying to shut down as
  // cleanly as possible even in production code, so call this
  // cairo_debug_* function unconditionally.
  //
  // because cairo can assert and thus crash on shutdown, don't do this in
  // release builds
#ifdef NS_FREE_PERMANENT_DATA
#  ifdef USE_SKIA
  // must do Skia cleanup before Cairo cleanup, because Skia may be referencing
  // Cairo objects e.g. through SkCairoFTTypeface
  SkGraphics::PurgeFontCache();
#  endif

#  if MOZ_TREE_CAIRO
  cairo_debug_reset_static_data();
#  endif
#endif
}

/* static */
already_AddRefed<DrawTarget> gfxPlatform::CreateDrawTargetForSurface(
    gfxASurface* aSurface, const IntSize& aSize) {
  SurfaceFormat format = aSurface->GetSurfaceFormat();
  RefPtr<DrawTarget> drawTarget = Factory::CreateDrawTargetForCairoSurface(
      aSurface->CairoSurface(), aSize, &format);
  if (!drawTarget) {
    gfxWarning() << "gfxPlatform::CreateDrawTargetForSurface failed in "
                    "CreateDrawTargetForCairoSurface";
    return nullptr;
  }
  return drawTarget.forget();
}

cairo_user_data_key_t kSourceSurface;

/**
 * Record the backend that was used to construct the SourceSurface.
 * When getting the cached SourceSurface for a gfxASurface/DrawTarget pair,
 * we check to make sure the DrawTarget's backend matches the backend
 * for the cached SourceSurface, and only use it if they match. This
 * can avoid expensive and unnecessary readbacks.
 */
struct SourceSurfaceUserData {
  RefPtr<SourceSurface> mSrcSurface;
  BackendType mBackendType;
};

static void SourceBufferDestroy(void* srcSurfUD) {
  delete static_cast<SourceSurfaceUserData*>(srcSurfUD);
}

UserDataKey kThebesSurface;

struct DependentSourceSurfaceUserData {
  RefPtr<gfxASurface> mSurface;
};

static void SourceSurfaceDestroyed(void* aData) {
  delete static_cast<DependentSourceSurfaceUserData*>(aData);
}

void gfxPlatform::ClearSourceSurfaceForSurface(gfxASurface* aSurface) {
  aSurface->SetData(&kSourceSurface, nullptr, nullptr);
}

/* static */
already_AddRefed<SourceSurface> gfxPlatform::GetSourceSurfaceForSurface(
    RefPtr<DrawTarget> aTarget, gfxASurface* aSurface, bool aIsPlugin) {
  if (!aSurface->CairoSurface() || aSurface->CairoStatus()) {
    return nullptr;
  }

  if (!aTarget) {
    aTarget = gfxPlatform::GetPlatform()->ScreenReferenceDrawTarget();
  }

  void* userData = aSurface->GetData(&kSourceSurface);

  if (userData) {
    SourceSurfaceUserData* surf = static_cast<SourceSurfaceUserData*>(userData);

    if (surf->mSrcSurface->IsValid() &&
        surf->mBackendType == aTarget->GetBackendType()) {
      RefPtr<SourceSurface> srcSurface(surf->mSrcSurface);
      return srcSurface.forget();
    }
    // We can just continue here as when setting new user data the destroy
    // function will be called for the old user data.
  }

  SurfaceFormat format = aSurface->GetSurfaceFormat();

  if (aTarget->GetBackendType() == BackendType::CAIRO) {
    // If we're going to be used with a CAIRO DrawTarget, then just create a
    // SourceSurfaceCairo since we don't know the underlying type of the CAIRO
    // DrawTarget and can't pick a better surface type. Doing this also avoids
    // readback of aSurface's surface into memory if, for example, aSurface
    // wraps an xlib cairo surface (which can be important to avoid a major
    // slowdown).
    //
    // We return here regardless of whether CreateSourceSurfaceFromNativeSurface
    // succeeds or not since we don't expect to be able to do any better below
    // if it fails.
    //
    // Note that the returned SourceSurfaceCairo holds a strong reference to
    // the cairo_surface_t* that it wraps, which essencially means it holds a
    // strong reference to aSurface since aSurface shares its
    // cairo_surface_t*'s reference count variable. As a result we can't cache
    // srcBuffer on aSurface (see below) since aSurface would then hold a
    // strong reference back to srcBuffer, creating a reference loop and a
    // memory leak. Not caching is fine since wrapping is cheap enough (no
    // copying) so we can just wrap again next time we're called.
    return Factory::CreateSourceSurfaceForCairoSurface(
        aSurface->CairoSurface(), aSurface->GetSize(), format);
  }

  RefPtr<SourceSurface> srcBuffer;

  // Currently no other DrawTarget types implement
  // CreateSourceSurfaceFromNativeSurface

  if (!srcBuffer) {
    // If aSurface wraps data, we can create a SourceSurfaceRawData that wraps
    // the same data, then optimize it for aTarget:
    RefPtr<DataSourceSurface> surf = GetWrappedDataSourceSurface(aSurface);
    if (surf) {
      srcBuffer = aIsPlugin
                      ? aTarget->OptimizeSourceSurfaceForUnknownAlpha(surf)
                      : aTarget->OptimizeSourceSurface(surf);

      if (srcBuffer == surf) {
        // GetWrappedDataSourceSurface returns a SourceSurface that holds a
        // strong reference to aSurface since it wraps aSurface's data and
        // needs it to stay alive. As a result we can't cache srcBuffer on
        // aSurface (below) since aSurface would then hold a strong reference
        // back to srcBuffer, creating a reference loop and a memory leak. Not
        // caching is fine since wrapping is cheap enough (no copying) so we
        // can just wrap again next time we're called.
        //
        // Note that the check below doesn't catch this since srcBuffer will be
        // a SourceSurfaceRawData object (even if aSurface is not a
        // gfxImageSurface object), which is why we need this separate check.
        return srcBuffer.forget();
      }
    }
  }

  if (!srcBuffer) {
    MOZ_ASSERT(aTarget->GetBackendType() != BackendType::CAIRO,
               "We already tried CreateSourceSurfaceFromNativeSurface with a "
               "DrawTargetCairo above");
    // We've run out of performant options. We now try creating a SourceSurface
    // using a temporary DrawTargetCairo and then optimizing it to aTarget's
    // actual type. The CreateSourceSurfaceFromNativeSurface() call will
    // likely create a DataSourceSurface (possibly involving copying and/or
    // readback), and the OptimizeSourceSurface may well copy again and upload
    // to the GPU. So, while this code path is rarely hit, hitting it may be
    // very slow.
    srcBuffer = Factory::CreateSourceSurfaceForCairoSurface(
        aSurface->CairoSurface(), aSurface->GetSize(), format);
    if (srcBuffer) {
      srcBuffer = aTarget->OptimizeSourceSurface(srcBuffer);
    }
  }

  if (!srcBuffer) {
    return nullptr;
  }

  if ((srcBuffer->GetType() == SurfaceType::CAIRO &&
       static_cast<SourceSurfaceCairo*>(srcBuffer.get())->GetSurface() ==
           aSurface->CairoSurface()) ||
      (srcBuffer->GetType() == SurfaceType::CAIRO_IMAGE &&
       static_cast<DataSourceSurfaceCairo*>(srcBuffer.get())->GetSurface() ==
           aSurface->CairoSurface())) {
    // See the "Note that the returned SourceSurfaceCairo..." comment above.
    return srcBuffer.forget();
  }

  // Add user data to aSurface so we can cache lookups in the future.
  auto* srcSurfUD = new SourceSurfaceUserData;
  srcSurfUD->mBackendType = aTarget->GetBackendType();
  srcSurfUD->mSrcSurface = srcBuffer;
  aSurface->SetData(&kSourceSurface, srcSurfUD, SourceBufferDestroy);

  return srcBuffer.forget();
}

already_AddRefed<DataSourceSurface> gfxPlatform::GetWrappedDataSourceSurface(
    gfxASurface* aSurface) {
  RefPtr<gfxImageSurface> image = aSurface->GetAsImageSurface();
  if (!image) {
    return nullptr;
  }
  RefPtr<DataSourceSurface> result = Factory::CreateWrappingDataSourceSurface(
      image->Data(), image->Stride(), image->GetSize(),
      ImageFormatToSurfaceFormat(image->Format()));

  if (!result) {
    return nullptr;
  }

  // If we wrapped the underlying data of aSurface, then we need to add user
  // data to make sure aSurface stays alive until we are done with the data.
  auto* srcSurfUD = new DependentSourceSurfaceUserData;
  srcSurfUD->mSurface = aSurface;
  result->AddUserData(&kThebesSurface, srcSurfUD, SourceSurfaceDestroyed);

  return result.forget();
}

void gfxPlatform::ComputeTileSize() {
  // The tile size should be picked in the parent processes
  // and sent to the child processes over IPDL GetTileSize.
  if (!XRE_IsParentProcess()) {
    return;
  }

  int32_t w = gfxPrefs::LayersTileWidth();
  int32_t h = gfxPrefs::LayersTileHeight();

  if (gfxPrefs::LayersTilesAdjust()) {
    gfx::IntSize screenSize = GetScreenSize();
    if (screenSize.width > 0) {
      // Choose a size so that there are between 2 and 4 tiles per screen width.
      // FIXME: we should probably make sure this is within the max texture
      // size, but I think everything should at least support 1024
      w = h = clamped(int32_t(RoundUpPow2(screenSize.width)) / 4, 256, 1024);
    }
  }

  // Don't allow changing the tile size after we've set it.
  // Right now the code assumes that the tile size doesn't change.
  MOZ_ASSERT(gfxVars::TileSize().width == -1 &&
             gfxVars::TileSize().height == -1);

  gfxVars::SetTileSize(IntSize(w, h));
}

void gfxPlatform::PopulateScreenInfo() {
  nsCOMPtr<nsIScreenManager> manager =
      do_GetService("@mozilla.org/gfx/screenmanager;1");
  MOZ_ASSERT(manager, "failed to get nsIScreenManager");

  nsCOMPtr<nsIScreen> screen;
  manager->GetPrimaryScreen(getter_AddRefs(screen));
  if (!screen) {
    // This can happen in xpcshell, for instance
    return;
  }

  screen->GetColorDepth(&mScreenDepth);
  if (XRE_IsParentProcess()) {
    gfxVars::SetScreenDepth(mScreenDepth);
  }

  int left, top;
  screen->GetRect(&left, &top, &mScreenSize.width, &mScreenSize.height);
}

bool gfxPlatform::SupportsAzureContentForDrawTarget(DrawTarget* aTarget) {
  if (!aTarget || !aTarget->IsValid()) {
    return false;
  }

  return SupportsAzureContentForType(aTarget->GetBackendType());
}

void gfxPlatform::PurgeSkiaFontCache() {
#ifdef USE_SKIA
  if (gfxPlatform::GetPlatform()->GetDefaultContentBackend() ==
      BackendType::SKIA) {
    SkGraphics::PurgeFontCache();
  }
#endif
}

already_AddRefed<DrawTarget> gfxPlatform::CreateDrawTargetForBackend(
    BackendType aBackend, const IntSize& aSize, SurfaceFormat aFormat) {
  // There is a bunch of knowledge in the gfxPlatform heirarchy about how to
  // create the best offscreen surface for the current system and situation. We
  // can easily take advantage of this for the Cairo backend, so that's what we
  // do.
  // mozilla::gfx::Factory can get away without having all this knowledge for
  // now, but this might need to change in the future (using
  // CreateOffscreenSurface() and CreateDrawTargetForSurface() for all
  // backends).
  if (aBackend == BackendType::CAIRO) {
    RefPtr<gfxASurface> surf =
        CreateOffscreenSurface(aSize, SurfaceFormatToImageFormat(aFormat));
    if (!surf || surf->CairoStatus()) {
      return nullptr;
    }
    return CreateDrawTargetForSurface(surf, aSize);
  }
  return Factory::CreateDrawTarget(aBackend, aSize, aFormat);
}

already_AddRefed<DrawTarget> gfxPlatform::CreateOffscreenCanvasDrawTarget(
    const IntSize& aSize, SurfaceFormat aFormat) {
  NS_ASSERTION(mPreferredCanvasBackend != BackendType::NONE, "No backend.");
  RefPtr<DrawTarget> target =
      CreateDrawTargetForBackend(mPreferredCanvasBackend, aSize, aFormat);
  if (target || mFallbackCanvasBackend == BackendType::NONE) {
    return target.forget();
  }

#ifdef XP_WIN
  // On Windows, the fallback backend (Cairo) should use its image backend.
  return Factory::CreateDrawTarget(mFallbackCanvasBackend, aSize, aFormat);
#else
  return CreateDrawTargetForBackend(mFallbackCanvasBackend, aSize, aFormat);
#endif
}

already_AddRefed<DrawTarget> gfxPlatform::CreateOffscreenContentDrawTarget(
    const IntSize& aSize, SurfaceFormat aFormat, bool aFallback) {
  BackendType backend = (aFallback) ? mSoftwareBackend : mContentBackend;
  NS_ASSERTION(backend != BackendType::NONE, "No backend.");
  RefPtr<DrawTarget> dt = CreateDrawTargetForBackend(backend, aSize, aFormat);

  if (!dt) {
    return nullptr;
  }

  // We'd prefer this to take proper care and return a CaptureDT, but for the
  // moment since we can't and this means we're going to be drawing on the main
  // thread force it's initialization. See bug 1526045 and bug 1521368.
  dt->ClearRect(gfx::Rect());
  if (!dt->IsValid()) {
    return nullptr;
  }
  return dt.forget();
}

already_AddRefed<DrawTarget> gfxPlatform::CreateSimilarSoftwareDrawTarget(
    DrawTarget* aDT, const IntSize& aSize, SurfaceFormat aFormat) {
  RefPtr<DrawTarget> dt;

  if (Factory::DoesBackendSupportDataDrawtarget(aDT->GetBackendType())) {
    dt = aDT->CreateSimilarDrawTarget(aSize, aFormat);
  } else {
#ifdef USE_SKIA
    BackendType backendType = BackendType::SKIA;
#else
    BackendType backendType = BackendType::CAIRO;
#endif
    dt = Factory::CreateDrawTarget(backendType, aSize, aFormat);
  }

  return dt.forget();
}

/* static */
already_AddRefed<DrawTarget> gfxPlatform::CreateDrawTargetForData(
    unsigned char* aData, const IntSize& aSize, int32_t aStride,
    SurfaceFormat aFormat, bool aUninitialized) {
  BackendType backendType = gfxVars::ContentBackend();
  NS_ASSERTION(backendType != BackendType::NONE, "No backend.");

  if (!Factory::DoesBackendSupportDataDrawtarget(backendType)) {
#ifdef USE_SKIA
    backendType = BackendType::SKIA;
#else
    backendType = BackendType::CAIRO;
#endif
  }

  RefPtr<DrawTarget> dt = Factory::CreateDrawTargetForData(
      backendType, aData, aSize, aStride, aFormat, aUninitialized);

  return dt.forget();
}

/* static */
BackendType gfxPlatform::BackendTypeForName(const nsCString& aName) {
  if (aName.EqualsLiteral("cairo")) return BackendType::CAIRO;
  if (aName.EqualsLiteral("skia")) return BackendType::SKIA;
  if (aName.EqualsLiteral("direct2d")) return BackendType::DIRECT2D;
  if (aName.EqualsLiteral("direct2d1.1")) return BackendType::DIRECT2D1_1;
  return BackendType::NONE;
}

nsresult gfxPlatform::GetFontList(nsAtom* aLangGroup,
                                  const nsACString& aGenericFamily,
                                  nsTArray<nsString>& aListOfFonts) {
  gfxPlatformFontList::PlatformFontList()->GetFontList(
      aLangGroup, aGenericFamily, aListOfFonts);
  return NS_OK;
}

nsresult gfxPlatform::UpdateFontList() {
  gfxPlatformFontList::PlatformFontList()->UpdateFontList();
  return NS_OK;
}

void gfxPlatform::GetStandardFamilyName(const nsCString& aFontName,
                                        nsACString& aFamilyName) {
  gfxPlatformFontList::PlatformFontList()->GetStandardFamilyName(aFontName,
                                                                 aFamilyName);
}

nsAutoCString gfxPlatform::GetDefaultFontName(
    const nsACString& aLangGroup, const nsACString& aGenericFamily) {
  // To benefit from Return Value Optimization, all paths here must return
  // this one variable:
  nsAutoCString result;

  FamilyAndGeneric fam =
      gfxPlatformFontList::PlatformFontList()->GetDefaultFontFamily(
          aLangGroup, aGenericFamily);
  if (fam.mFamily.mIsShared) {
    if (fam.mFamily.mShared) {
      fontlist::FontList* fontList =
          gfxPlatformFontList::PlatformFontList()->SharedFontList();
      result = fam.mFamily.mShared->DisplayName().AsString(fontList);
    }
  } else if (fam.mFamily.mUnshared) {
    fam.mFamily.mUnshared->LocalizedName(result);
  }  // (else, leave 'result' empty)

  return result;
}

bool gfxPlatform::DownloadableFontsEnabled() {
  if (mAllowDownloadableFonts == UNINITIALIZED_VALUE) {
    mAllowDownloadableFonts =
        Preferences::GetBool(GFX_DOWNLOADABLE_FONTS_ENABLED, false);
  }

  return mAllowDownloadableFonts;
}

bool gfxPlatform::UseCmapsDuringSystemFallback() {
  if (mFallbackUsesCmaps == UNINITIALIZED_VALUE) {
    mFallbackUsesCmaps =
        Preferences::GetBool(GFX_PREF_FALLBACK_USE_CMAPS, false);
  }

  return mFallbackUsesCmaps;
}

bool gfxPlatform::OpenTypeSVGEnabled() {
  if (mOpenTypeSVGEnabled == UNINITIALIZED_VALUE) {
    mOpenTypeSVGEnabled = Preferences::GetBool(GFX_PREF_OPENTYPE_SVG, false);
  }

  return mOpenTypeSVGEnabled > 0;
}

uint32_t gfxPlatform::WordCacheCharLimit() {
  if (mWordCacheCharLimit == UNINITIALIZED_VALUE) {
    mWordCacheCharLimit =
        Preferences::GetInt(GFX_PREF_WORD_CACHE_CHARLIMIT, 32);
    if (mWordCacheCharLimit < 0) {
      mWordCacheCharLimit = 32;
    }
  }

  return uint32_t(mWordCacheCharLimit);
}

uint32_t gfxPlatform::WordCacheMaxEntries() {
  if (mWordCacheMaxEntries == UNINITIALIZED_VALUE) {
    mWordCacheMaxEntries =
        Preferences::GetInt(GFX_PREF_WORD_CACHE_MAXENTRIES, 10000);
    if (mWordCacheMaxEntries < 0) {
      mWordCacheMaxEntries = 10000;
    }
  }

  return uint32_t(mWordCacheMaxEntries);
}

bool gfxPlatform::UseGraphiteShaping() {
  if (mGraphiteShapingEnabled == UNINITIALIZED_VALUE) {
    mGraphiteShapingEnabled =
        Preferences::GetBool(GFX_PREF_GRAPHITE_SHAPING, false);
  }

  return mGraphiteShapingEnabled;
}

bool gfxPlatform::IsFontFormatSupported(uint32_t aFormatFlags) {
  // check for strange format flags
  MOZ_ASSERT(!(aFormatFlags & gfxUserFontSet::FLAG_FORMAT_NOT_USED),
             "strange font format hint set");

  // accept "common" formats that we support on all platforms
  if (aFormatFlags & gfxUserFontSet::FLAG_FORMATS_COMMON) {
    return true;
  }

  // reject all other formats, known and unknown
  if (aFormatFlags != 0) {
    return false;
  }

  // no format hint set, need to look at data
  return true;
}

gfxFontEntry* gfxPlatform::LookupLocalFont(const nsACString& aFontName,
                                           WeightRange aWeightForEntry,
                                           StretchRange aStretchForEntry,
                                           SlantStyleRange aStyleForEntry) {
  return gfxPlatformFontList::PlatformFontList()->LookupLocalFont(
      aFontName, aWeightForEntry, aStretchForEntry, aStyleForEntry);
}

gfxFontEntry* gfxPlatform::MakePlatformFont(const nsACString& aFontName,
                                            WeightRange aWeightForEntry,
                                            StretchRange aStretchForEntry,
                                            SlantStyleRange aStyleForEntry,
                                            const uint8_t* aFontData,
                                            uint32_t aLength) {
  return gfxPlatformFontList::PlatformFontList()->MakePlatformFont(
      aFontName, aWeightForEntry, aStretchForEntry, aStyleForEntry, aFontData,
      aLength);
}

mozilla::layers::DiagnosticTypes gfxPlatform::GetLayerDiagnosticTypes() {
  mozilla::layers::DiagnosticTypes type = DiagnosticTypes::NO_DIAGNOSTIC;
  if (gfxPrefs::DrawLayerBorders()) {
    type |= mozilla::layers::DiagnosticTypes::LAYER_BORDERS;
  }
  if (gfxPrefs::DrawTileBorders()) {
    type |= mozilla::layers::DiagnosticTypes::TILE_BORDERS;
  }
  if (gfxPrefs::DrawBigImageBorders()) {
    type |= mozilla::layers::DiagnosticTypes::BIGIMAGE_BORDERS;
  }
  if (gfxPrefs::FlashLayerBorders()) {
    type |= mozilla::layers::DiagnosticTypes::FLASH_BORDERS;
  }
  return type;
}

BackendPrefsData gfxPlatform::GetBackendPrefs() const {
  BackendPrefsData data;

  data.mCanvasBitmask = BackendTypeBit(BackendType::CAIRO);
  data.mContentBitmask = BackendTypeBit(BackendType::CAIRO);
#ifdef USE_SKIA
  data.mCanvasBitmask |= BackendTypeBit(BackendType::SKIA);
  data.mContentBitmask |= BackendTypeBit(BackendType::SKIA);
#endif
  data.mCanvasDefault = BackendType::CAIRO;
  data.mContentDefault = BackendType::CAIRO;

  return data;
}

void gfxPlatform::InitBackendPrefs(BackendPrefsData&& aPrefsData) {
  mPreferredCanvasBackend = GetCanvasBackendPref(aPrefsData.mCanvasBitmask);
  if (mPreferredCanvasBackend == BackendType::NONE) {
    mPreferredCanvasBackend = aPrefsData.mCanvasDefault;
  }

  if (mPreferredCanvasBackend == BackendType::DIRECT2D1_1) {
    // Falling back to D2D 1.0 won't help us here. When D2D 1.1 DT creation
    // fails it means the surface was too big or there's something wrong with
    // the device. D2D 1.0 will encounter a similar situation.
    mFallbackCanvasBackend = GetCanvasBackendPref(
        aPrefsData.mCanvasBitmask & ~(BackendTypeBit(mPreferredCanvasBackend) |
                                      BackendTypeBit(BackendType::DIRECT2D)));
  } else {
    mFallbackCanvasBackend = GetCanvasBackendPref(
        aPrefsData.mCanvasBitmask & ~BackendTypeBit(mPreferredCanvasBackend));
  }

  mContentBackendBitmask = aPrefsData.mContentBitmask;
  mContentBackend = GetContentBackendPref(mContentBackendBitmask);
  if (mContentBackend == BackendType::NONE) {
    mContentBackend = aPrefsData.mContentDefault;
    // mContentBackendBitmask is our canonical reference for supported
    // backends so we need to add the default if we are using it and
    // overriding the prefs.
    mContentBackendBitmask |= BackendTypeBit(aPrefsData.mContentDefault);
  }

  uint32_t swBackendBits =
      BackendTypeBit(BackendType::SKIA) | BackendTypeBit(BackendType::CAIRO);
  mSoftwareBackend = GetContentBackendPref(swBackendBits);

  if (XRE_IsParentProcess()) {
    gfxVars::SetContentBackend(mContentBackend);
    gfxVars::SetSoftwareBackend(mSoftwareBackend);
  }
}

/* static */
BackendType gfxPlatform::GetCanvasBackendPref(uint32_t aBackendBitmask) {
  return GetBackendPref("gfx.canvas.azure.backends", aBackendBitmask);
}

/* static */
BackendType gfxPlatform::GetContentBackendPref(uint32_t& aBackendBitmask) {
  return GetBackendPref("gfx.content.azure.backends", aBackendBitmask);
}

/* static */
BackendType gfxPlatform::GetBackendPref(const char* aBackendPrefName,
                                        uint32_t& aBackendBitmask) {
  nsTArray<nsCString> backendList;
  nsAutoCString prefString;
  if (NS_SUCCEEDED(Preferences::GetCString(aBackendPrefName, prefString))) {
    ParseString(prefString, ',', backendList);
  }

  uint32_t allowedBackends = 0;
  BackendType result = BackendType::NONE;
  for (uint32_t i = 0; i < backendList.Length(); ++i) {
    BackendType type = BackendTypeForName(backendList[i]);
    if (BackendTypeBit(type) & aBackendBitmask) {
      allowedBackends |= BackendTypeBit(type);
      if (result == BackendType::NONE) {
        result = type;
      }
    }
  }

  aBackendBitmask = allowedBackends;
  return result;
}

bool gfxPlatform::InSafeMode() {
  static bool sSafeModeInitialized = false;
  static bool sInSafeMode = false;

  if (!sSafeModeInitialized) {
    sSafeModeInitialized = true;
    nsCOMPtr<nsIXULRuntime> xr = do_GetService("@mozilla.org/xre/runtime;1");
    if (xr) {
      xr->GetInSafeMode(&sInSafeMode);
    }
  }
  return sInSafeMode;
}

bool gfxPlatform::OffMainThreadCompositingEnabled() {
  return UsesOffMainThreadCompositing();
}

eCMSMode gfxPlatform::GetCMSMode() {
  if (!gCMSInitialized) {
    int32_t mode = gfxPrefs::CMSMode();
    if (mode >= 0 && mode < eCMSMode_AllCount) {
      gCMSMode = static_cast<eCMSMode>(mode);
    }

    bool enableV4 = gfxPrefs::CMSEnableV4();
    if (enableV4) {
      qcms_enable_iccv4();
    }
    gCMSInitialized = true;
  }
  return gCMSMode;
}

int gfxPlatform::GetRenderingIntent() {
  // gfxPrefs.h is using 0 as the default for the rendering
  // intent preference, based on that being the value for
  // QCMS_INTENT_DEFAULT.  Assert here to catch if that ever
  // changes and we can then figure out what to do about it.
  MOZ_ASSERT(QCMS_INTENT_DEFAULT == 0);

  /* Try to query the pref system for a rendering intent. */
  int32_t pIntent = gfxPrefs::CMSRenderingIntent();
  if ((pIntent < QCMS_INTENT_MIN) || (pIntent > QCMS_INTENT_MAX)) {
    /* If the pref is out of range, use embedded profile. */
    pIntent = -1;
  }
  return pIntent;
}

void gfxPlatform::TransformPixel(const Color& in, Color& out,
                                 qcms_transform* transform) {
  if (transform) {
    /* we want the bytes in RGB order */
#ifdef IS_LITTLE_ENDIAN
    /* ABGR puts the bytes in |RGBA| order on little endian */
    uint32_t packed = in.ToABGR();
    qcms_transform_data(transform, (uint8_t*)&packed, (uint8_t*)&packed, 1);
    out = Color::FromABGR(packed);
#else
    /* ARGB puts the bytes in |ARGB| order on big endian */
    uint32_t packed = in.UnusualToARGB();
    /* add one to move past the alpha byte */
    qcms_transform_data(transform, (uint8_t*)&packed + 1, (uint8_t*)&packed + 1,
                        1);
    out = Color::UnusualFromARGB(packed);
#endif
  }

  else if (&out != &in)
    out = in;
}

void gfxPlatform::GetPlatformCMSOutputProfile(void*& mem, size_t& size) {
  mem = nullptr;
  size = 0;
}

void gfxPlatform::GetCMSOutputProfileData(void*& mem, size_t& size) {
  nsAutoCString fname;
  Preferences::GetCString("gfx.color_management.display_profile", fname);
  mem = nullptr;
  if (!fname.IsEmpty()) {
    qcms_data_from_path(fname.get(), &mem, &size);
  }
  if (mem == nullptr) {
    gfxPlatform::GetPlatform()->GetPlatformCMSOutputProfile(mem, size);
  }
}

void gfxPlatform::CreateCMSOutputProfile() {
  if (!gCMSOutputProfile) {
    /* Determine if we're using the internal override to force sRGB as
       an output profile for reftests. See Bug 452125.

       Note that we don't normally (outside of tests) set a default value
       of this preference, which means nsIPrefBranch::GetBoolPref will
       typically throw (and leave its out-param untouched).
     */
    if (Preferences::GetBool(GFX_PREF_CMS_FORCE_SRGB, false)) {
      gCMSOutputProfile = GetCMSsRGBProfile();
    }

    if (!gCMSOutputProfile) {
      void* mem = nullptr;
      size_t size = 0;

      GetCMSOutputProfileData(mem, size);
      if ((mem != nullptr) && (size > 0)) {
        gCMSOutputProfile = qcms_profile_from_memory(mem, size);
        free(mem);
      }
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

qcms_profile* gfxPlatform::GetCMSOutputProfile() { return gCMSOutputProfile; }

qcms_profile* gfxPlatform::GetCMSsRGBProfile() {
  if (!gCMSsRGBProfile) {
    /* Create the profile using qcms. */
    gCMSsRGBProfile = qcms_profile_sRGB();
  }
  return gCMSsRGBProfile;
}

qcms_transform* gfxPlatform::GetCMSRGBTransform() {
  if (!gCMSRGBTransform && !gCMSRGBTransformFailed) {
    qcms_profile *inProfile, *outProfile;
    outProfile = GetCMSOutputProfile();
    inProfile = GetCMSsRGBProfile();

    if (!inProfile || !outProfile) return nullptr;

    gCMSRGBTransform =
        qcms_transform_create(inProfile, QCMS_DATA_RGB_8, outProfile,
                              QCMS_DATA_RGB_8, QCMS_INTENT_PERCEPTUAL);
    if (!gCMSRGBTransform) {
      gCMSRGBTransformFailed = true;
    }
  }

  return gCMSRGBTransform;
}

qcms_transform* gfxPlatform::GetCMSInverseRGBTransform() {
  if (!gCMSInverseRGBTransform) {
    qcms_profile *inProfile, *outProfile;
    inProfile = GetCMSOutputProfile();
    outProfile = GetCMSsRGBProfile();

    if (!inProfile || !outProfile) return nullptr;

    gCMSInverseRGBTransform =
        qcms_transform_create(inProfile, QCMS_DATA_RGB_8, outProfile,
                              QCMS_DATA_RGB_8, QCMS_INTENT_PERCEPTUAL);
  }

  return gCMSInverseRGBTransform;
}

qcms_transform* gfxPlatform::GetCMSRGBATransform() {
  if (!gCMSRGBATransform) {
    qcms_profile *inProfile, *outProfile;
    outProfile = GetCMSOutputProfile();
    inProfile = GetCMSsRGBProfile();

    if (!inProfile || !outProfile) return nullptr;

    gCMSRGBATransform =
        qcms_transform_create(inProfile, QCMS_DATA_RGBA_8, outProfile,
                              QCMS_DATA_RGBA_8, QCMS_INTENT_PERCEPTUAL);
  }

  return gCMSRGBATransform;
}

/* Shuts down various transforms and profiles for CMS. */
static void ShutdownCMS() {
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
    if (gCMSsRGBProfile == gCMSOutputProfile) gCMSsRGBProfile = nullptr;
    gCMSOutputProfile = nullptr;
  }
  if (gCMSsRGBProfile) {
    qcms_profile_release(gCMSsRGBProfile);
    gCMSsRGBProfile = nullptr;
  }

  // Reset the state variables
  gCMSMode = eCMSMode_Off;
  gCMSInitialized = false;
}

int32_t gfxPlatform::GetBidiNumeralOption() {
  if (mBidiNumeralOption == UNINITIALIZED_VALUE) {
    mBidiNumeralOption = Preferences::GetInt(BIDI_NUMERAL_PREF, 0);
  }
  return mBidiNumeralOption;
}

/* static */
void gfxPlatform::FlushFontAndWordCaches() {
  gfxFontCache* fontCache = gfxFontCache::GetCache();
  if (fontCache) {
    fontCache->AgeAllGenerations();
    fontCache->FlushShapedWordCaches();
  }

  gfxPlatform::PurgeSkiaFontCache();
}

/* static */
void gfxPlatform::ForceGlobalReflow() {
  MOZ_ASSERT(NS_IsMainThread());
  if (XRE_IsParentProcess()) {
    // Modify a preference that will trigger reflow everywhere (in all
    // content processes, as well as the parent).
    static const char kPrefName[] = "font.internaluseonly.changed";
    bool fontInternalChange = Preferences::GetBool(kPrefName, false);
    Preferences::SetBool(kPrefName, !fontInternalChange);
  } else {
    // Send a notification that will be observed by PresShells in this
    // process only.
    nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
    if (obs) {
      obs->NotifyObservers(nullptr, "font-info-updated", nullptr);
    }
  }
}

void gfxPlatform::FontsPrefsChanged(const char* aPref) {
  NS_ASSERTION(aPref != nullptr, "null preference");
  if (!strcmp(GFX_DOWNLOADABLE_FONTS_ENABLED, aPref)) {
    mAllowDownloadableFonts = UNINITIALIZED_VALUE;
  } else if (!strcmp(GFX_PREF_FALLBACK_USE_CMAPS, aPref)) {
    mFallbackUsesCmaps = UNINITIALIZED_VALUE;
  } else if (!strcmp(GFX_PREF_WORD_CACHE_CHARLIMIT, aPref)) {
    mWordCacheCharLimit = UNINITIALIZED_VALUE;
    FlushFontAndWordCaches();
  } else if (!strcmp(GFX_PREF_WORD_CACHE_MAXENTRIES, aPref)) {
    mWordCacheMaxEntries = UNINITIALIZED_VALUE;
    FlushFontAndWordCaches();
  } else if (!strcmp(GFX_PREF_GRAPHITE_SHAPING, aPref)) {
    mGraphiteShapingEnabled = UNINITIALIZED_VALUE;
    FlushFontAndWordCaches();
  } else if (!strcmp(BIDI_NUMERAL_PREF, aPref)) {
    mBidiNumeralOption = UNINITIALIZED_VALUE;
  } else if (!strcmp(GFX_PREF_OPENTYPE_SVG, aPref)) {
    mOpenTypeSVGEnabled = UNINITIALIZED_VALUE;
    gfxFontCache::GetCache()->AgeAllGenerations();
    gfxFontCache::GetCache()->NotifyGlyphsChanged();
  }
}

mozilla::LogModule* gfxPlatform::GetLog(eGfxLog aWhichLog) {
  // logs shared across gfx
  static LazyLogModule sFontlistLog("fontlist");
  static LazyLogModule sFontInitLog("fontinit");
  static LazyLogModule sTextrunLog("textrun");
  static LazyLogModule sTextrunuiLog("textrunui");
  static LazyLogModule sCmapDataLog("cmapdata");
  static LazyLogModule sTextPerfLog("textperf");

  switch (aWhichLog) {
    case eGfxLog_fontlist:
      return sFontlistLog;
    case eGfxLog_fontinit:
      return sFontInitLog;
    case eGfxLog_textrun:
      return sTextrunLog;
    case eGfxLog_textrunui:
      return sTextrunuiLog;
    case eGfxLog_cmapdata:
      return sCmapDataLog;
    case eGfxLog_textperf:
      return sTextPerfLog;
  }

  MOZ_ASSERT_UNREACHABLE("Unexpected log type");
  return nullptr;
}

RefPtr<mozilla::gfx::DrawTarget> gfxPlatform::ScreenReferenceDrawTarget() {
  return (mScreenReferenceDrawTarget)
             ? mScreenReferenceDrawTarget
             : gPlatform->CreateOffscreenContentDrawTarget(
                   IntSize(1, 1), SurfaceFormat::B8G8R8A8, true);
}

mozilla::gfx::SurfaceFormat gfxPlatform::Optimal2DFormatForContent(
    gfxContentType aContent) {
  switch (aContent) {
    case gfxContentType::COLOR:
      switch (GetOffscreenFormat()) {
        case SurfaceFormat::A8R8G8B8_UINT32:
          return mozilla::gfx::SurfaceFormat::B8G8R8A8;
        case SurfaceFormat::X8R8G8B8_UINT32:
          return mozilla::gfx::SurfaceFormat::B8G8R8X8;
        case SurfaceFormat::R5G6B5_UINT16:
          return mozilla::gfx::SurfaceFormat::R5G6B5_UINT16;
        default:
          MOZ_ASSERT_UNREACHABLE(
              "unknown gfxImageFormat for "
              "gfxContentType::COLOR");
          return mozilla::gfx::SurfaceFormat::B8G8R8A8;
      }
    case gfxContentType::ALPHA:
      return mozilla::gfx::SurfaceFormat::A8;
    case gfxContentType::COLOR_ALPHA:
      return mozilla::gfx::SurfaceFormat::B8G8R8A8;
    default:
      MOZ_ASSERT_UNREACHABLE("unknown gfxContentType");
      return mozilla::gfx::SurfaceFormat::B8G8R8A8;
  }
}

gfxImageFormat gfxPlatform::OptimalFormatForContent(gfxContentType aContent) {
  switch (aContent) {
    case gfxContentType::COLOR:
      return GetOffscreenFormat();
    case gfxContentType::ALPHA:
      return SurfaceFormat::A8;
    case gfxContentType::COLOR_ALPHA:
      return SurfaceFormat::A8R8G8B8_UINT32;
    default:
      MOZ_ASSERT_UNREACHABLE("unknown gfxContentType");
      return SurfaceFormat::A8R8G8B8_UINT32;
  }
}

/**
 * There are a number of layers acceleration (or layers in general) preferences
 * that should be consistent for the lifetime of the application (bug 840967).
 * As such, we will evaluate them all as soon as one of them is evaluated
 * and remember the values.  Changing these preferences during the run will
 * not have any effect until we restart.
 */
static mozilla::Atomic<bool> sLayersSupportsHardwareVideoDecoding(false);
static bool sLayersHardwareVideoDecodingFailed = false;
static bool sBufferRotationCheckPref = true;

static mozilla::Atomic<bool> sLayersAccelerationPrefsInitialized(false);

static void VideoDecodingFailedChangedCallback(const char* aPref, void*) {
  sLayersHardwareVideoDecodingFailed = Preferences::GetBool(aPref, false);
  gfxPlatform::GetPlatform()->UpdateCanUseHardwareVideoDecoding();
}

void gfxPlatform::UpdateCanUseHardwareVideoDecoding() {
  if (XRE_IsParentProcess()) {
    gfxVars::SetCanUseHardwareVideoDecoding(CanUseHardwareVideoDecoding());
  }
}

void gfxPlatform::InitAcceleration() {
  if (sLayersAccelerationPrefsInitialized) {
    return;
  }

  InitCompositorAccelerationPrefs();

  // If this is called for the first time on a non-main thread, we're screwed.
  // At the moment there's no explicit guarantee that the main thread calls
  // this before the compositor thread, but let's at least make the assumption
  // explicit.
  MOZ_ASSERT(NS_IsMainThread(), "can only initialize prefs on the main thread");

  gfxPrefs::GetSingleton();

  nsCOMPtr<nsIGfxInfo> gfxInfo = services::GetGfxInfo();
  nsCString discardFailureId;
  int32_t status;

  if (XRE_IsParentProcess()) {
    gfxVars::SetBrowserTabsRemoteAutostart(BrowserTabsRemoteAutostart());
    gfxVars::SetOffscreenFormat(GetOffscreenFormat());
    gfxVars::SetRequiresAcceleratedGLContextForCompositorOGL(
        RequiresAcceleratedGLContextForCompositorOGL());
#ifdef XP_WIN
    if (NS_SUCCEEDED(
            gfxInfo->GetFeatureStatus(nsIGfxInfo::FEATURE_D3D11_KEYED_MUTEX,
                                      discardFailureId, &status))) {
      gfxVars::SetAllowD3D11KeyedMutex(status == nsIGfxInfo::FEATURE_STATUS_OK);
    } else {
      // If we couldn't properly evaluate the status, err on the side
      // of caution and give this functionality to the user.
      gfxCriticalNote << "Cannot evaluate keyed mutex feature status";
      gfxVars::SetAllowD3D11KeyedMutex(true);
    }
#endif
  }

  if (Preferences::GetBool("media.hardware-video-decoding.enabled", false) &&
#ifdef XP_WIN
      Preferences::GetBool("media.wmf.dxva.enabled", true) &&
#endif
      NS_SUCCEEDED(
          gfxInfo->GetFeatureStatus(nsIGfxInfo::FEATURE_HARDWARE_VIDEO_DECODING,
                                    discardFailureId, &status))) {
    if (status == nsIGfxInfo::FEATURE_STATUS_OK ||
        gfxPrefs::HardwareVideoDecodingForceEnabled()) {
      sLayersSupportsHardwareVideoDecoding = true;
    }
  }

  sLayersAccelerationPrefsInitialized = true;

  if (XRE_IsParentProcess()) {
    Preferences::RegisterCallbackAndCall(
        VideoDecodingFailedChangedCallback,
        "media.hardware-video-decoding.failed");
    InitGPUProcessPrefs();
  }
}

void gfxPlatform::InitGPUProcessPrefs() {
  // We want to hide this from about:support, so only set a default if the
  // pref is known to be true.
  if (!gfxPrefs::GPUProcessEnabled() && !gfxPrefs::GPUProcessForceEnabled()) {
    return;
  }

  FeatureState& gpuProc = gfxConfig::GetFeature(Feature::GPU_PROCESS);

  // We require E10S - otherwise, there is very little benefit to the GPU
  // process, since the UI process must still use acceleration for
  // performance.
  if (!BrowserTabsRemoteAutostart()) {
    gpuProc.DisableByDefault(FeatureStatus::Unavailable,
                             "Multi-process mode is not enabled",
                             NS_LITERAL_CSTRING("FEATURE_FAILURE_NO_E10S"));
  } else {
    gpuProc.SetDefaultFromPref(gfxPrefs::GetGPUProcessEnabledPrefName(), true,
                               gfxPrefs::GetGPUProcessEnabledPrefDefault());
  }

  if (gfxPrefs::GPUProcessForceEnabled()) {
    gpuProc.UserForceEnable("User force-enabled via pref");
  }

  if (IsHeadless()) {
    gpuProc.ForceDisable(FeatureStatus::Blocked, "Headless mode is enabled",
                         NS_LITERAL_CSTRING("FEATURE_FAILURE_HEADLESS_MODE"));
    return;
  }
  if (InSafeMode()) {
    gpuProc.ForceDisable(FeatureStatus::Blocked, "Safe-mode is enabled",
                         NS_LITERAL_CSTRING("FEATURE_FAILURE_SAFE_MODE"));
    return;
  }
  if (gfxPrefs::LayerScopeEnabled()) {
    gpuProc.ForceDisable(FeatureStatus::Blocked,
                         "LayerScope does not work in the GPU process",
                         NS_LITERAL_CSTRING("FEATURE_FAILURE_LAYERSCOPE"));
    return;
  }
}

void gfxPlatform::InitCompositorAccelerationPrefs() {
  const char* acceleratedEnv = PR_GetEnv("MOZ_ACCELERATED");

  FeatureState& feature = gfxConfig::GetFeature(Feature::HW_COMPOSITING);

  // Base value - does the platform allow acceleration?
  if (feature.SetDefault(AccelerateLayersByDefault(), FeatureStatus::Blocked,
                         "Acceleration blocked by platform")) {
    if (gfxPrefs::LayersAccelerationDisabledDoNotUseDirectly()) {
      feature.UserDisable("Disabled by pref",
                          NS_LITERAL_CSTRING("FEATURE_FAILURE_COMP_PREF"));
    } else if (acceleratedEnv && *acceleratedEnv == '0') {
      feature.UserDisable("Disabled by envvar",
                          NS_LITERAL_CSTRING("FEATURE_FAILURE_COMP_ENV"));
    }
  } else {
    if (acceleratedEnv && *acceleratedEnv == '1') {
      feature.UserEnable("Enabled by envvar");
    }
  }

  // This has specific meaning elsewhere, so we always record it.
  if (gfxPrefs::LayersAccelerationForceEnabledDoNotUseDirectly()) {
    feature.UserForceEnable("Force-enabled by pref");
  }

  // Safe, headless, and record/replay modes override everything.
  if (InSafeMode()) {
    feature.ForceDisable(FeatureStatus::Blocked,
                         "Acceleration blocked by safe-mode",
                         NS_LITERAL_CSTRING("FEATURE_FAILURE_COMP_SAFEMODE"));
  }
  if (IsHeadless()) {
    feature.ForceDisable(
        FeatureStatus::Blocked, "Acceleration blocked by headless mode",
        NS_LITERAL_CSTRING("FEATURE_FAILURE_COMP_HEADLESSMODE"));
  }
  if (recordreplay::IsRecordingOrReplaying()) {
    feature.ForceDisable(
        FeatureStatus::Blocked, "Acceleration blocked by recording/replaying",
        NS_LITERAL_CSTRING("FEATURE_FAILURE_COMP_RECORDREPLAY"));
  }
}

/*static*/
bool gfxPlatform::WebRenderPrefEnabled() {
  return gfxPrefs::WebRenderAll() ||
         gfxPrefs::WebRenderEnabledDoNotUseDirectly();
}

/*static*/
bool gfxPlatform::WebRenderEnvvarEnabled() {
  const char* env = PR_GetEnv("MOZ_WEBRENDER");
  return (env && *env == '1');
}

static bool WebRenderEnvvarDisabled() {
  const char* env = PR_GetEnv("MOZ_WEBRENDER");
  return (env && *env == '0');
}

// If the "gfx.webrender.all.qualified" pref is true we want to enable
// WebRender for qualifying hardware. The Normandy pref rollout code sets
// default values on rolled out prefs on every startup, but Gfx starts up
// before Normandy does. So it's too early to observe the WR qualified pref
// default value changed by Normandy rollout here yet. So we have a shutdown
// observer to save the default value on shutdown, and read the saved default
// value here instead, and emulate the behavior of the pref system, with
// respect to default/user values of the rollout pref.
static bool CalculateWrQualifiedPrefValue() {
  auto clearPrefOnExit = MakeScopeExit([]() {
    // Clear the mirror of the default value of the rollout pref on scope exit,
    // if we have one. This ensures the user doesn't mess with the pref.
    // If we need it again, we'll re-create it on shutdown.
    Preferences::ClearUser(WR_ROLLOUT_PREF_DEFAULT);
  });

  if (!Preferences::HasUserValue(WR_ROLLOUT_PREF) &&
      Preferences::HasUserValue(WR_ROLLOUT_PREF_DEFAULT)) {
    // The user has not set a user pref, and we have a default value set by the
    // shutdown observer. We should use that instead of the gfxPref's default,
    // as if Normandy had a chance to set it before startup, that is the value
    // gfxPrefs would return, rather than the default set by DECL_GFX_PREF.
    return gfxPrefs::WebRenderAllQualifiedDefault();
  }

  // We don't have a user value for the rollout pref, and we don't have the
  // value of the rollout pref at last shutdown stored. So we should fallback
  // to using whatever default is stored in the gfxPref. *But* if we're running
  // under the Marionette pref rollout work-around test, we may want to override
  // the default value expressed here, so we can test the "default disabled;
  // rollout pref enabled" case.
  if (Preferences::HasUserValue(WR_ROLLOUT_PREF_OVERRIDE)) {
    return Preferences::GetBool(WR_ROLLOUT_PREF_OVERRIDE);
  }
  return gfxPrefs::WebRenderAllQualified();
}

static FeatureState& WebRenderHardwareQualificationStatus(
    const IntSize& aScreenSize, bool aHasBattery, nsCString& aOutFailureId) {
  FeatureState& featureWebRenderQualified =
      gfxConfig::GetFeature(Feature::WEBRENDER_QUALIFIED);
  featureWebRenderQualified.EnableByDefault();

  if (Preferences::HasUserValue(WR_ROLLOUT_HW_QUALIFIED_OVERRIDE)) {
    if (!Preferences::GetBool(WR_ROLLOUT_HW_QUALIFIED_OVERRIDE)) {
      featureWebRenderQualified.Disable(
          FeatureStatus::BlockedOverride, "HW qualification pref override",
          NS_LITERAL_CSTRING("FEATURE_FAILURE_WR_QUALIFICATION_OVERRIDE"));
    }
    return featureWebRenderQualified;
  }

  nsCOMPtr<nsIGfxInfo> gfxInfo = services::GetGfxInfo();
  int32_t status;
  if (NS_SUCCEEDED(gfxInfo->GetFeatureStatus(nsIGfxInfo::FEATURE_WEBRENDER,
                                             aOutFailureId, &status))) {
    if (status != nsIGfxInfo::FEATURE_STATUS_OK) {
      featureWebRenderQualified.Disable(FeatureStatus::Blacklisted,
                                        "No qualified hardware", aOutFailureId);
    } else {
      nsAutoString adapterVendorID;
      gfxInfo->GetAdapterVendorID(adapterVendorID);

      nsAutoString adapterDeviceID;
      gfxInfo->GetAdapterDeviceID(adapterDeviceID);
      nsresult valid;
      int32_t deviceID = adapterDeviceID.ToInteger(&valid, 16);
      if (valid != NS_OK) {
        featureWebRenderQualified.Disable(
            FeatureStatus::BlockedDeviceUnknown, "Bad device id",
            NS_LITERAL_CSTRING("FEATURE_FAILURE_BAD_DEVICE_ID"));
      } else {
#ifdef NIGHTLY_BUILD
        // For Intel devices, if we have a battery, ignore it if the screen is
        // small enough. Note that we always check for a battery with NVIDIA
        // because we do not have a limited/curated set of devices to support
        // WebRender on.
        const int32_t kMaxPixelsBattery = 1920 * 1200;  // WUXGA
        const int32_t screenPixels = aScreenSize.width * aScreenSize.height;
        bool disableForBattery = aHasBattery;
        if (adapterVendorID == u"0x8086" && screenPixels > 0 &&
            screenPixels <= kMaxPixelsBattery) {
          disableForBattery = false;
        }
#else
        bool disableForBattery = aHasBattery;
#endif

        if (disableForBattery) {
          featureWebRenderQualified.Disable(
              FeatureStatus::BlockedHasBattery, "Has battery",
              NS_LITERAL_CSTRING("FEATURE_FAILURE_WR_HAS_BATTERY"));
        } else if (adapterVendorID == u"0x10de") {
          if (deviceID < 0x6c0) {
            // 0x6c0 is the lowest Fermi device id. Unfortunately some Tesla
            // devices that don't support D3D 10.1 have higher deviceIDs. They
            // will be included, but blocked by ANGLE.
            featureWebRenderQualified.Disable(
                FeatureStatus::BlockedDeviceTooOld, "Device too old",
                NS_LITERAL_CSTRING("FEATURE_FAILURE_DEVICE_TOO_OLD"));
          }
        } else if (adapterVendorID == u"0x1002") {  // AMD
          // AMD deviceIDs are not very well ordered. This
          // condition is based off the information in gpu-db
          if ((deviceID >= 0x6600 && deviceID < 0x66b0) ||
              (deviceID >= 0x6700 && deviceID < 0x6720) ||
              (deviceID >= 0x6780 && deviceID < 0x6840) ||
              (deviceID >= 0x6860 && deviceID < 0x6880) ||
              (deviceID >= 0x6900 && deviceID < 0x6a00) ||
              (deviceID == 0x7300) ||
              (deviceID >= 0x9830 && deviceID < 0x9870) ||
              (deviceID >= 0x9900 && deviceID < 0x9a00)) {
            // we have a desktop CAYMAN, SI, CIK, VI, or GFX9 device
            // so treat the device as qualified.
          } else {
            featureWebRenderQualified.Disable(
                FeatureStatus::BlockedDeviceTooOld, "Device too old",
                NS_LITERAL_CSTRING("FEATURE_FAILURE_DEVICE_TOO_OLD"));
          }
#ifdef NIGHTLY_BUILD
        } else if (adapterVendorID == u"0x8086") {  // Intel
          const uint16_t supportedDevices[] = {
              // skylake gt2+
              0x1912,
              0x1913,
              0x1915,
              0x1916,
              0x1917,
              0x191a,
              0x191b,
              0x191d,
              0x191e,
              0x1921,
              0x1923,
              0x1926,
              0x1927,
              0x192b,
              0x1932,
              0x193b,
              0x193d,

              // kabylake gt2+
              0x5912,
              0x5916,
              0x5917,
              0x591a,
              0x591b,
              0x591c,
              0x591d,
              0x591e,
              0x5921,
              0x5926,
              0x5923,
              0x5927,
              0x593b,

              // coffeelake gt2+
              0x3e91,
              0x3e92,
              0x3e96,
              0x3e98,
              0x3e9a,
              0x3e9b,
              0x3e94,
              0x3ea0,
              0x3ea9,
              0x3ea2,
              0x3ea6,
              0x3ea7,
              0x3ea8,
              0x3ea5,

              // broadwell gt2+
              0x1612,
              0x1616,
              0x161a,
              0x161b,
              0x161d,
              0x161e,
              0x1622,
              0x1626,
              0x162a,
              0x162b,
              0x162d,
              0x162e,
              0x1632,
              0x1636,
              0x163a,
              0x163b,
              0x163d,
              0x163e,

              // HD Graphics 4600
              0x0412,
              0x0416,
              0x041a,
              0x041b,
              0x041e,
              0x0a12,
              0x0a16,
              0x0a1a,
              0x0a1b,
              0x0a1e,
          };
          bool supported = false;
          for (uint16_t id : supportedDevices) {
            if (deviceID == id) {
              supported = true;
              break;
            }
          }
          if (!supported) {
            featureWebRenderQualified.Disable(
                FeatureStatus::BlockedDeviceTooOld, "Device too old",
                NS_LITERAL_CSTRING("FEATURE_FAILURE_DEVICE_TOO_OLD"));
          }
#  ifdef MOZ_WIDGET_GTK
          else {
            // Performance is not great on 4k screens with WebRender + Linux.
            // Disable it for now if it is too large.
            const int32_t kMaxPixelsLinux = 3440 * 1440;  // UWQHD
            if (screenPixels > kMaxPixelsLinux) {
              featureWebRenderQualified.Disable(
                  FeatureStatus::BlockedScreenTooLarge, "Screen size too large",
                  NS_LITERAL_CSTRING("FEATURE_FAILURE_SCREEN_SIZE_TOO_LARGE"));
            } else if (screenPixels <= 0) {
              featureWebRenderQualified.Disable(
                  FeatureStatus::BlockedScreenUnknown, "Screen size unknown",
                  NS_LITERAL_CSTRING("FEATURE_FAILURE_SCREEN_SIZE_UNKNOWN"));
            }
          }
#  endif  // MOZ_WIDGET_GTK
#endif    // NIGHTLY_BUILD
        } else {
          featureWebRenderQualified.Disable(
              FeatureStatus::BlockedVendorUnsupported, "Unsupported vendor",
              NS_LITERAL_CSTRING("FEATURE_FAILURE_UNSUPPORTED_VENDOR"));
        }
      }
    }
  } else {
    featureWebRenderQualified.Disable(
        FeatureStatus::BlockedNoGfxInfo, "gfxInfo is broken",
        NS_LITERAL_CSTRING("FEATURE_FAILURE_WR_NO_GFX_INFO"));
  }
  return featureWebRenderQualified;
}

void gfxPlatform::InitWebRenderConfig() {
  bool prefEnabled = WebRenderPrefEnabled();
  bool envvarEnabled = WebRenderEnvvarEnabled();

  // WR? WR+   => means WR was enabled via gfx.webrender.all.qualified on
  //              qualified hardware
  // WR! WR+   => means WR was enabled via gfx.webrender.{all,enabled} or
  //              envvar, possibly on unqualified hardware
  // In all cases WR- means WR was not enabled, for one of many possible
  // reasons. Prior to bug 1523788 landing the gfx.webrender.{all,enabled}
  // prefs only worked on Nightly so keep that in mind when looking at older
  // crash reports.
  ScopedGfxFeatureReporter reporter("WR", prefEnabled || envvarEnabled);
  if (!XRE_IsParentProcess()) {
    // Force-disable WebRender in recording/replaying child processes, which
    // have their own compositor.
    if (recordreplay::IsRecordingOrReplaying()) {
      gfxVars::SetUseWebRender(false);
    }

    // The parent process runs through all the real decision-making code
    // later in this function. For other processes we still want to report
    // the state of the feature for crash reports.
    if (gfxVars::UseWebRender()) {
      reporter.SetSuccessful();
    }
    return;
  }

  nsCString failureId;
  FeatureState& featureWebRenderQualified =
      WebRenderHardwareQualificationStatus(GetScreenSize(), HasBattery(),
                                           failureId);
  FeatureState& featureWebRender = gfxConfig::GetFeature(Feature::WEBRENDER);

  featureWebRender.DisableByDefault(
      FeatureStatus::OptIn, "WebRender is an opt-in feature",
      NS_LITERAL_CSTRING("FEATURE_FAILURE_DEFAULT_OFF"));

  const bool wrQualifiedAll = CalculateWrQualifiedPrefValue();

  // envvar works everywhere; note that we need this for testing in CI.
  // Prior to bug 1523788, the `prefEnabled` check was only done on Nightly,
  // so as to prevent random users from easily enabling WebRender on
  // unqualified hardware in beta/release.
  if (envvarEnabled) {
    featureWebRender.UserEnable("Force enabled by envvar");
  } else if (prefEnabled) {
    featureWebRender.UserEnable("Force enabled by pref");
  } else if (wrQualifiedAll && featureWebRenderQualified.IsEnabled()) {
    featureWebRender.UserEnable("Qualified enabled by pref ");
  }

  // If the user set the pref to force-disable, let's do that. This will
  // override all the other enabling prefs (gfx.webrender.enabled,
  // gfx.webrender.all, and gfx.webrender.all.qualified).
  if (gfxPrefs::WebRenderForceDisabled() || WebRenderEnvvarDisabled()) {
    featureWebRender.UserDisable(
        "User force-disabled WR",
        NS_LITERAL_CSTRING("FEATURE_FAILURE_USER_FORCE_DISABLED"));
  }

  // HW_COMPOSITING being disabled implies interfacing with the GPU might break
  if (!gfxConfig::IsEnabled(Feature::HW_COMPOSITING)) {
    featureWebRender.ForceDisable(
        FeatureStatus::UnavailableNoHwCompositing,
        "Hardware compositing is disabled",
        NS_LITERAL_CSTRING("FEATURE_FAILURE_WEBRENDER_NEED_HWCOMP"));
  }

  // WebRender relies on the GPU process when on Windows
#ifdef XP_WIN
  if (!gfxConfig::IsEnabled(Feature::GPU_PROCESS)) {
    featureWebRender.ForceDisable(
        FeatureStatus::UnavailableNoGpuProcess, "GPU Process is disabled",
        NS_LITERAL_CSTRING("FEATURE_FAILURE_GPU_PROCESS_DISABLED"));
  }
#endif

  if (InSafeMode()) {
    featureWebRender.ForceDisable(
        FeatureStatus::UnavailableInSafeMode, "Safe-mode is enabled",
        NS_LITERAL_CSTRING("FEATURE_FAILURE_SAFE_MODE"));
  }

#ifndef MOZ_BUILD_WEBRENDER
  featureWebRender.ForceDisable(
      FeatureStatus::UnavailableNotBuilt, "Build doesn't include WebRender",
      NS_LITERAL_CSTRING("FEATURE_FAILURE_NO_WEBRENDER"));
#endif

#ifdef XP_WIN
  if (Preferences::GetBool("gfx.webrender.force-angle", false)) {
    if (!gfxConfig::IsEnabled(Feature::D3D11_HW_ANGLE)) {
      featureWebRender.ForceDisable(
          FeatureStatus::UnavailableNoAngle, "ANGLE is disabled",
          NS_LITERAL_CSTRING("FEATURE_FAILURE_ANGLE_DISABLED"));
    } else {
      gfxVars::SetUseWebRenderANGLE(gfxConfig::IsEnabled(Feature::WEBRENDER));
    }
  }
#endif

  if (Preferences::GetBool("gfx.webrender.program-binary-disk", false)) {
    gfxVars::SetUseWebRenderProgramBinaryDisk(
        gfxConfig::IsEnabled(Feature::WEBRENDER));
  }

#ifdef MOZ_WIDGET_ANDROID
  if (jni::IsFennec()) {
    featureWebRender.ForceDisable(
        FeatureStatus::Unavailable,
        "WebRender not ready for use on non-e10s Android",
        NS_LITERAL_CSTRING("FEATURE_FAILURE_ANDROID"));
  }
#endif

  // gfxFeature is not usable in the GPU process, so we use gfxVars to transmit
  // this feature
  if (gfxConfig::IsEnabled(Feature::WEBRENDER)) {
    gfxVars::SetUseWebRender(true);
    reporter.SetSuccessful();

    if (XRE_IsParentProcess()) {
      Preferences::RegisterPrefixCallbackAndCall(
          WebRenderDebugPrefChangeCallback, WR_DEBUG_PREF);
    }
  }
#if defined(MOZ_WIDGET_GTK)
  else if (gfxConfig::IsEnabled(Feature::HW_COMPOSITING)) {
    // Hardware compositing should be disabled by default if we aren't using
    // WebRender. We had to check if it is enabled at all, because it may
    // already have been forced disabled (e.g. safe mode, headless). It may
    // still be forced on by the user, and if so, this should have no effect.
    gfxConfig::Disable(Feature::HW_COMPOSITING, FeatureStatus::Blocked,
                       "Acceleration blocked by platform");
  }
#endif

#ifdef XP_WIN
  if (Preferences::GetBool("gfx.webrender.dcomp-win.enabled", false)) {
    // XXX relax win version to windows 8.
    if (IsWin10OrLater() && gfxVars::UseWebRender() &&
        gfxVars::UseWebRenderANGLE()) {
      gfxVars::SetUseWebRenderDCompWin(true);
    }
  }
  if (Preferences::GetBool("gfx.webrender.dcomp-win-triple-buffering.enabled",
                           false)) {
    if (gfxVars::UseWebRenderDCompWin()) {
      gfxVars::SetUseWebRenderDCompWinTripleBuffering(true);
    }
  }
#endif
}

void gfxPlatform::InitOMTPConfig() {
  ScopedGfxFeatureReporter reporter("OMTP");

  FeatureState& omtp = gfxConfig::GetFeature(Feature::OMTP);
  int32_t paintWorkerCount = PaintThread::CalculatePaintWorkerCount();

  if (!XRE_IsParentProcess()) {
    // The parent process runs through all the real decision-making code
    // later in this function. For other processes we still want to report
    // the state of the feature for crash reports.
    if (gfxVars::UseOMTP()) {
      reporter.SetSuccessful(paintWorkerCount);
    }
    return;
  }

  omtp.SetDefaultFromPref("layers.omtp.enabled", true,
                          Preferences::GetBool("layers.omtp.enabled", false,
                                               PrefValueKind::Default));

  if (mContentBackend == BackendType::CAIRO) {
    omtp.ForceDisable(FeatureStatus::Broken,
                      "OMTP is not supported when using cairo",
                      NS_LITERAL_CSTRING("FEATURE_FAILURE_COMP_PREF"));
  }

#ifdef XP_MACOSX
  if (!nsCocoaFeatures::OnYosemiteOrLater()) {
    omtp.ForceDisable(FeatureStatus::Blocked, "OMTP blocked before OSX 10.10",
                      NS_LITERAL_CSTRING("FEATURE_FAILURE_OMTP_OSX_MAVERICKS"));
  }
#endif

  if (InSafeMode()) {
    omtp.ForceDisable(FeatureStatus::Blocked, "OMTP blocked by safe-mode",
                      NS_LITERAL_CSTRING("FEATURE_FAILURE_COMP_SAFEMODE"));
  }

  if (omtp.IsEnabled()) {
    gfxVars::SetUseOMTP(true);
    reporter.SetSuccessful(paintWorkerCount);
  }
}

bool gfxPlatform::CanUseHardwareVideoDecoding() {
  // this function is called from the compositor thread, so it is not
  // safe to init the prefs etc. from here.
  MOZ_ASSERT(sLayersAccelerationPrefsInitialized);
  return sLayersSupportsHardwareVideoDecoding &&
         !sLayersHardwareVideoDecodingFailed;
}

bool gfxPlatform::AccelerateLayersByDefault() {
#if defined(MOZ_GL_PROVIDER) || defined(MOZ_WIDGET_UIKIT)
  return true;
#else
  return false;
#endif
}

bool gfxPlatform::BufferRotationEnabled() {
  MutexAutoLock autoLock(*gGfxPlatformPrefsLock);

  return sBufferRotationCheckPref && gfxPrefs::BufferRotationEnabled();
}

void gfxPlatform::DisableBufferRotation() {
  MutexAutoLock autoLock(*gGfxPlatformPrefsLock);

  sBufferRotationCheckPref = false;
}

/* static */
bool gfxPlatform::UsesOffMainThreadCompositing() {
  if (XRE_GetProcessType() == GeckoProcessType_GPU) {
    return true;
  }

  static bool firstTime = true;
  static bool result = false;

  if (firstTime) {
    MOZ_ASSERT(sLayersAccelerationPrefsInitialized);
    result = gfxVars::BrowserTabsRemoteAutostart() ||
             !gfxPrefs::LayersOffMainThreadCompositionForceDisabled();
#if defined(MOZ_WIDGET_GTK)
    // Linux users who chose OpenGL are being grandfathered in to OMTC
    result |= gfxPrefs::LayersAccelerationForceEnabledDoNotUseDirectly();

#endif
    firstTime = false;
  }

  return result;
}

bool gfxPlatform::UsesTiling() const {
  bool usesSkia = GetDefaultContentBackend() == BackendType::SKIA;

  // We can't just test whether the PaintThread is initialized here because
  // this function is used when initializing the PaintThread. So instead we
  // check the conditions that enable OMTP with parallel painting.
  bool usesPOMTP = XRE_IsContentProcess() && gfxVars::UseOMTP() &&
                   (gfxPrefs::LayersOMTPPaintWorkers() == -1 ||
                    gfxPrefs::LayersOMTPPaintWorkers() > 1);

  return gfxPrefs::LayersTilesEnabled() ||
         (gfxPrefs::LayersTilesEnabledIfSkiaPOMTP() && usesSkia && usesPOMTP);
}

bool gfxPlatform::ContentUsesTiling() const {
  BackendPrefsData data = GetBackendPrefs();
  BackendType contentBackend = GetContentBackendPref(data.mContentBitmask);
  if (contentBackend == BackendType::NONE) {
    contentBackend = data.mContentDefault;
  }

  bool contentUsesSkia = contentBackend == BackendType::SKIA;
  bool contentUsesPOMTP =
      gfxVars::UseOMTP() && (gfxPrefs::LayersOMTPPaintWorkers() == -1 ||
                             gfxPrefs::LayersOMTPPaintWorkers() > 1);

  return gfxPrefs::LayersTilesEnabled() ||
         (gfxPrefs::LayersTilesEnabledIfSkiaPOMTP() && contentUsesSkia &&
          contentUsesPOMTP);
}

/***
 * The preference "layout.frame_rate" has 3 meanings depending on the value:
 *
 * -1 = Auto (default), use hardware vsync or software vsync @ 60 hz if hw
 *      vsync fails.
 *  0 = ASAP mode - used during talos testing.
 *  X = Software vsync at a rate of X times per second.
 */
already_AddRefed<mozilla::gfx::VsyncSource>
gfxPlatform::CreateHardwareVsyncSource() {
  RefPtr<mozilla::gfx::VsyncSource> softwareVsync = new SoftwareVsyncSource();
  return softwareVsync.forget();
}

/* static */
bool gfxPlatform::IsInLayoutAsapMode() {
  // There are 2 modes of ASAP mode.
  // 1 is that the refresh driver and compositor are in lock step
  // the second is that the compositor goes ASAP and the refresh driver
  // goes at whatever the configurated rate is. This only checks the version
  // talos uses, which is the refresh driver and compositor are in lockstep.
  return gfxPrefs::LayoutFrameRate() == 0;
}

/* static */
bool gfxPlatform::ForceSoftwareVsync() {
  return gfxPrefs::LayoutFrameRate() > 0 ||
         recordreplay::IsRecordingOrReplaying();
}

/* static */
int gfxPlatform::GetSoftwareVsyncRate() {
  int preferenceRate = gfxPrefs::LayoutFrameRate();
  if (preferenceRate <= 0) {
    return gfxPlatform::GetDefaultFrameRate();
  }
  return preferenceRate;
}

/* static */
int gfxPlatform::GetDefaultFrameRate() { return 60; }

/* static */
void gfxPlatform::ReInitFrameRate() {
  if (XRE_IsParentProcess() || recordreplay::IsRecordingOrReplaying()) {
    RefPtr<VsyncSource> oldSource = gPlatform->mVsyncSource;

    // Start a new one:
    if (gfxPlatform::ForceSoftwareVsync()) {
      gPlatform->mVsyncSource =
          (gPlatform)->gfxPlatform::CreateHardwareVsyncSource();
    } else {
      gPlatform->mVsyncSource = gPlatform->CreateHardwareVsyncSource();
    }
    // Tidy up old vsync source.
    if (oldSource) {
      oldSource->MoveListenersToNewSource(gPlatform->mVsyncSource);
      oldSource->Shutdown();
    }
  }
}

void gfxPlatform::GetAzureBackendInfo(mozilla::widget::InfoObject& aObj) {
  if (gfxConfig::IsEnabled(Feature::GPU_PROCESS)) {
    aObj.DefineProperty("AzureCanvasBackend (UI Process)",
                        GetBackendName(mPreferredCanvasBackend));
    aObj.DefineProperty("AzureFallbackCanvasBackend (UI Process)",
                        GetBackendName(mFallbackCanvasBackend));
    aObj.DefineProperty("AzureContentBackend (UI Process)",
                        GetBackendName(mContentBackend));

    // Assume content process' backend prefs.
    BackendPrefsData data = GetBackendPrefs();
    BackendType canvasBackend = GetCanvasBackendPref(data.mCanvasBitmask);
    if (canvasBackend == BackendType::NONE) {
      canvasBackend = data.mCanvasDefault;
    }
    BackendType contentBackend = GetContentBackendPref(data.mContentBitmask);
    if (contentBackend == BackendType::NONE) {
      contentBackend = data.mContentDefault;
    }
    aObj.DefineProperty("AzureCanvasBackend", GetBackendName(canvasBackend));
    aObj.DefineProperty("AzureContentBackend", GetBackendName(contentBackend));
  } else {
    aObj.DefineProperty("AzureCanvasBackend",
                        GetBackendName(mPreferredCanvasBackend));
    aObj.DefineProperty("AzureFallbackCanvasBackend",
                        GetBackendName(mFallbackCanvasBackend));
    aObj.DefineProperty("AzureContentBackend", GetBackendName(mContentBackend));
  }
}

void gfxPlatform::GetApzSupportInfo(mozilla::widget::InfoObject& aObj) {
  if (!gfxPlatform::AsyncPanZoomEnabled()) {
    return;
  }

  if (SupportsApzWheelInput()) {
    aObj.DefineProperty("ApzWheelInput", 1);
  }

  if (SupportsApzTouchInput()) {
    aObj.DefineProperty("ApzTouchInput", 1);
  }

  if (SupportsApzDragInput()) {
    aObj.DefineProperty("ApzDragInput", 1);
  }

  if (SupportsApzKeyboardInput() && !gfxPrefs::AccessibilityBrowseWithCaret()) {
    aObj.DefineProperty("ApzKeyboardInput", 1);
  }

  if (SupportsApzAutoscrolling()) {
    aObj.DefineProperty("ApzAutoscrollInput", 1);
  }
}

void gfxPlatform::GetTilesSupportInfo(mozilla::widget::InfoObject& aObj) {
  if (!gfxPrefs::LayersTilesEnabled()) {
    return;
  }

  IntSize tileSize = gfxVars::TileSize();
  aObj.DefineProperty("TileHeight", tileSize.height);
  aObj.DefineProperty("TileWidth", tileSize.width);
}

void gfxPlatform::GetFrameStats(mozilla::widget::InfoObject& aObj) {
  uint32_t i = 0;
  for (FrameStats& f : mFrameStats) {
    nsPrintfCString name("Slow Frame #%02u", ++i);

    nsPrintfCString value(
        "Frame %" PRIu64
        "(%s) CONTENT_FRAME_TIME %d - Transaction start %f, main-thread time "
        "%f, full paint time %f, Skipped composites %u, Composite start %f, "
        "Resource upload time %f, GPU cache upload time %f, Render time %f, "
        "Composite time %f",
        f.id().mId, f.url().get(), f.contentFrameTime(),
        (f.transactionStart() - f.refreshStart()).ToMilliseconds(),
        (f.fwdTime() - f.transactionStart()).ToMilliseconds(),
        f.sceneBuiltTime()
            ? (f.sceneBuiltTime() - f.transactionStart()).ToMilliseconds()
            : 0.0,
        f.skippedComposites(),
        (f.compositeStart() - f.refreshStart()).ToMilliseconds(),
        f.resourceUploadTime(), f.gpuCacheUploadTime(),
        (f.compositeEnd() - f.renderStart()).ToMilliseconds(),
        (f.compositeEnd() - f.compositeStart()).ToMilliseconds());
    aObj.DefineProperty(name.get(), value.get());
  }
}

class FrameStatsComparator {
 public:
  bool Equals(const FrameStats& aA, const FrameStats& aB) const {
    return aA.contentFrameTime() == aB.contentFrameTime();
  }
  // Reverse the condition here since we want the array sorted largest to
  // smallest.
  bool LessThan(const FrameStats& aA, const FrameStats& aB) const {
    return aA.contentFrameTime() > aB.contentFrameTime();
  }
};

void gfxPlatform::NotifyFrameStats(nsTArray<FrameStats>&& aFrameStats) {
  if (!gfxPrefs::LoggingSlowFramesEnabled()) {
    return;
  }

  FrameStatsComparator comp;
  for (FrameStats& f : aFrameStats) {
    mFrameStats.InsertElementSorted(f, comp);
  }
  if (mFrameStats.Length() > 10) {
    mFrameStats.SetLength(10);
  }
}

/*static*/
uint32_t gfxPlatform::TargetFrameRate() {
  if (gPlatform && gPlatform->mVsyncSource) {
    VsyncSource::Display& display = gPlatform->mVsyncSource->GetGlobalDisplay();
    return round(1000.0 / display.GetVsyncRate().ToMilliseconds());
  }
  return 0;
}

/*static*/
bool gfxPlatform::AsyncPanZoomEnabled() {
#if !defined(MOZ_WIDGET_ANDROID) && !defined(MOZ_WIDGET_UIKIT)
  // For XUL applications (everything but Firefox on Android)
  // we only want to use APZ when E10S is enabled. If
  // we ever get input events off the main thread we can consider relaxing
  // this requirement.
  if (!BrowserTabsRemoteAutostart()) {
    return false;
  }
#endif
#ifdef MOZ_WIDGET_ANDROID
  return true;
#else
  if (!gfxPrefs::SingletonExists()) {
    // Make sure the gfxPrefs has been initialized before reading from it.
    MOZ_ASSERT(NS_IsMainThread());
    gfxPrefs::GetSingleton();
  }
  return gfxPrefs::AsyncPanZoomEnabledDoNotUseDirectly();
#endif
}

/*static*/
bool gfxPlatform::PerfWarnings() { return gfxPrefs::PerfWarnings(); }

void gfxPlatform::GetAcceleratedCompositorBackends(
    nsTArray<LayersBackend>& aBackends) {
  if (gfxConfig::IsEnabled(Feature::OPENGL_COMPOSITING)) {
    aBackends.AppendElement(LayersBackend::LAYERS_OPENGL);
  } else {
    static int tell_me_once = 0;
    if (!tell_me_once) {
      NS_WARNING("OpenGL-accelerated layers are not supported on this system");
      tell_me_once = 1;
    }
#ifdef MOZ_WIDGET_ANDROID
    MOZ_CRASH(
        "OpenGL-accelerated layers are a hard requirement on this platform. "
        "Cannot continue without support for them");
#endif
  }
}

void gfxPlatform::GetCompositorBackends(
    bool useAcceleration, nsTArray<mozilla::layers::LayersBackend>& aBackends) {
  if (useAcceleration) {
    GetAcceleratedCompositorBackends(aBackends);
  }
  aBackends.AppendElement(LayersBackend::LAYERS_BASIC);
}

void gfxPlatform::NotifyCompositorCreated(LayersBackend aBackend) {
  if (mCompositorBackend == aBackend) {
    return;
  }

  if (mCompositorBackend != LayersBackend::LAYERS_NONE) {
    gfxCriticalNote << "Compositors might be mixed (" << int(mCompositorBackend)
                    << "," << int(aBackend) << ")";
  }

  // Set the backend before we notify so it's available immediately.
  mCompositorBackend = aBackend;

  // Notify that we created a compositor, so telemetry can update.
  NS_DispatchToMainThread(
      NS_NewRunnableFunction("gfxPlatform::NotifyCompositorCreated", [] {
        if (nsCOMPtr<nsIObserverService> obsvc =
                services::GetObserverService()) {
          obsvc->NotifyObservers(nullptr, "compositor:created", nullptr);
        }
      }));
}

/* static */
void gfxPlatform::NotifyGPUProcessDisabled() {
  if (gfxConfig::IsEnabled(Feature::WEBRENDER)) {
    gfxConfig::GetFeature(Feature::WEBRENDER)
        .ForceDisable(
            FeatureStatus::Unavailable, "GPU Process is disabled",
            NS_LITERAL_CSTRING("FEATURE_FAILURE_GPU_PROCESS_DISABLED"));
    gfxVars::SetUseWebRender(false);
  }
}

void gfxPlatform::FetchAndImportContentDeviceData() {
  MOZ_ASSERT(XRE_IsContentProcess());

  if (gContentDeviceInitData) {
    ImportContentDeviceData(*gContentDeviceInitData);
    return;
  }

  mozilla::dom::ContentChild* cc = mozilla::dom::ContentChild::GetSingleton();

  mozilla::gfx::ContentDeviceData data;
  cc->SendGetGraphicsDeviceInitData(&data);

  ImportContentDeviceData(data);
}

void gfxPlatform::ImportContentDeviceData(
    const mozilla::gfx::ContentDeviceData& aData) {
  MOZ_ASSERT(XRE_IsContentProcess());

  const DevicePrefs& prefs = aData.prefs();
  gfxConfig::Inherit(Feature::HW_COMPOSITING, prefs.hwCompositing());
  gfxConfig::Inherit(Feature::OPENGL_COMPOSITING, prefs.oglCompositing());
}

void gfxPlatform::BuildContentDeviceData(
    mozilla::gfx::ContentDeviceData* aOut) {
  MOZ_ASSERT(XRE_IsParentProcess());

  // Make sure our settings are synchronized from the GPU process.
  GPUProcessManager::Get()->EnsureGPUReady();

  aOut->prefs().hwCompositing() = gfxConfig::GetValue(Feature::HW_COMPOSITING);
  aOut->prefs().oglCompositing() =
      gfxConfig::GetValue(Feature::OPENGL_COMPOSITING);
}

void gfxPlatform::ImportGPUDeviceData(
    const mozilla::gfx::GPUDeviceData& aData) {
  MOZ_ASSERT(XRE_IsParentProcess());

  gfxConfig::ImportChange(Feature::OPENGL_COMPOSITING, aData.oglCompositing());
  gfxConfig::ImportChange(Feature::ADVANCED_LAYERS, aData.advancedLayers());
}

bool gfxPlatform::SupportsApzTouchInput() const {
  return dom::TouchEvent::PrefEnabled(nullptr);
}

bool gfxPlatform::SupportsApzDragInput() const {
  return gfxPrefs::APZDragEnabled();
}

bool gfxPlatform::SupportsApzKeyboardInput() const {
  return gfxPrefs::APZKeyboardEnabled();
}

bool gfxPlatform::SupportsApzAutoscrolling() const {
  return gfxPrefs::APZAutoscrollEnabled();
}

void gfxPlatform::InitOpenGLConfig() {
#ifdef XP_WIN
  // Don't enable by default on Windows, since it could show up in about:support
  // even though it'll never get used. Only attempt if user enables the pref
  if (!Preferences::GetBool("layers.prefer-opengl")) {
    return;
  }
#endif

  FeatureState& openGLFeature =
      gfxConfig::GetFeature(Feature::OPENGL_COMPOSITING);

  // Check to see hw comp supported
  if (!gfxConfig::IsEnabled(Feature::HW_COMPOSITING)) {
    openGLFeature.DisableByDefault(
        FeatureStatus::Unavailable, "Hardware compositing is disabled",
        NS_LITERAL_CSTRING("FEATURE_FAILURE_OPENGL_NEED_HWCOMP"));
    return;
  }

#ifdef XP_WIN
  openGLFeature.SetDefaultFromPref(
      gfxPrefs::GetLayersPreferOpenGLPrefName(), true,
      gfxPrefs::GetLayersPreferOpenGLPrefDefault());
#else
  openGLFeature.EnableByDefault();
#endif

  // When layers acceleration is force-enabled, enable it even for blacklisted
  // devices.
  if (gfxPrefs::LayersAccelerationForceEnabledDoNotUseDirectly()) {
    openGLFeature.UserForceEnable("Force-enabled by pref");
    return;
  }

  nsCString message;
  nsCString failureId;
  if (!IsGfxInfoStatusOkay(nsIGfxInfo::FEATURE_OPENGL_LAYERS, &message,
                           failureId)) {
    openGLFeature.Disable(FeatureStatus::Blacklisted, message.get(), failureId);
  }
}

bool gfxPlatform::IsGfxInfoStatusOkay(int32_t aFeature, nsCString* aOutMessage,
                                      nsCString& aFailureId) {
  nsCOMPtr<nsIGfxInfo> gfxInfo = services::GetGfxInfo();
  if (!gfxInfo) {
    return true;
  }

  int32_t status;
  if (NS_SUCCEEDED(gfxInfo->GetFeatureStatus(aFeature, aFailureId, &status)) &&
      status != nsIGfxInfo::FEATURE_STATUS_OK) {
    aOutMessage->AssignLiteral("#BLOCKLIST_");
    aOutMessage->AppendASCII(aFailureId.get());
    return false;
  }

  return true;
}
