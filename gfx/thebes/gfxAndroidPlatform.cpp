/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"

#include "gfxAndroidPlatform.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/CountingAllocatorBase.h"
#include "mozilla/intl/LocaleService.h"
#include "mozilla/intl/OSPreferences.h"
#include "mozilla/jni/Utils.h"
#include "mozilla/layers/AndroidHardwareBuffer.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPrefs_gfx.h"
#include "mozilla/StaticPrefs_webgl.h"
#include "mozilla/widget/AndroidVsync.h"

#include "gfx2DGlue.h"
#include "gfxFT2FontList.h"
#include "gfxImageSurface.h"
#include "gfxTextRun.h"
#include "nsXULAppAPI.h"
#include "nsIScreen.h"
#include "nsServiceManagerUtils.h"
#include "nsUnicodeProperties.h"
#include "cairo.h"
#include "VsyncSource.h"

#include "ft2build.h"
#include FT_FREETYPE_H
#include FT_MODULE_H

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::gfx;
using namespace mozilla::unicode;
using mozilla::intl::LocaleService;
using mozilla::intl::OSPreferences;

static FT_Library gPlatformFTLibrary = nullptr;

class FreetypeReporter final : public nsIMemoryReporter,
                               public CountingAllocatorBase<FreetypeReporter> {
 private:
  ~FreetypeReporter() {}

 public:
  NS_DECL_ISUPPORTS

  static void* Malloc(FT_Memory, long size) { return CountingMalloc(size); }

  static void Free(FT_Memory, void* p) { return CountingFree(p); }

  static void* Realloc(FT_Memory, long cur_size, long new_size, void* p) {
    return CountingRealloc(p, new_size);
  }

  NS_IMETHOD CollectReports(nsIHandleReportCallback* aHandleReport,
                            nsISupports* aData, bool aAnonymize) override {
    MOZ_COLLECT_REPORT("explicit/freetype", KIND_HEAP, UNITS_BYTES,
                       MemoryAllocated(), "Memory used by Freetype.");

    return NS_OK;
  }
};

NS_IMPL_ISUPPORTS(FreetypeReporter, nsIMemoryReporter)

template <>
CountingAllocatorBase<FreetypeReporter>::AmountType
    CountingAllocatorBase<FreetypeReporter>::sAmount(0);

static FT_MemoryRec_ sFreetypeMemoryRecord;

gfxAndroidPlatform::gfxAndroidPlatform() {
  // A custom allocator.  It counts allocations, enabling memory reporting.
  sFreetypeMemoryRecord.user = nullptr;
  sFreetypeMemoryRecord.alloc = FreetypeReporter::Malloc;
  sFreetypeMemoryRecord.free = FreetypeReporter::Free;
  sFreetypeMemoryRecord.realloc = FreetypeReporter::Realloc;

  // These two calls are equivalent to FT_Init_FreeType(), but allow us to
  // provide a custom memory allocator.
  FT_New_Library(&sFreetypeMemoryRecord, &gPlatformFTLibrary);
  FT_Add_Default_Modules(gPlatformFTLibrary);

  Factory::SetFTLibrary(gPlatformFTLibrary);

  RegisterStrongMemoryReporter(new FreetypeReporter());

  mOffscreenFormat = GetScreenDepth() == 16 ? SurfaceFormat::R5G6B5_UINT16
                                            : SurfaceFormat::X8R8G8B8_UINT32;

  if (StaticPrefs::gfx_android_rgb16_force_AtStartup()) {
    mOffscreenFormat = SurfaceFormat::R5G6B5_UINT16;
  }
}

gfxAndroidPlatform::~gfxAndroidPlatform() {
  FT_Done_Library(gPlatformFTLibrary);
  gPlatformFTLibrary = nullptr;
  layers::AndroidHardwareBufferManager::Shutdown();
  layers::AndroidHardwareBufferApi::Shutdown();
}

void gfxAndroidPlatform::InitAcceleration() {
  gfxPlatform::InitAcceleration();
  if (XRE_IsParentProcess() && jni::GetAPIVersion() >= 26) {
    if (StaticPrefs::gfx_use_ahardwarebuffer_content_AtStartup()) {
      gfxVars::SetUseAHardwareBufferContent(true);
    }
    if (StaticPrefs::webgl_enable_ahardwarebuffer()) {
      gfxVars::SetUseAHardwareBufferSharedSurface(true);
    }
  }
  if (gfx::gfxVars::UseAHardwareBufferContent() ||
      gfxVars::UseAHardwareBufferSharedSurface()) {
    layers::AndroidHardwareBufferApi::Init();
    layers::AndroidHardwareBufferManager::Init();
  }
}

already_AddRefed<gfxASurface> gfxAndroidPlatform::CreateOffscreenSurface(
    const IntSize& aSize, gfxImageFormat aFormat) {
  if (!Factory::AllowedSurfaceSize(aSize)) {
    return nullptr;
  }

  RefPtr<gfxASurface> newSurface;
  newSurface = new gfxImageSurface(aSize, aFormat);

  return newSurface.forget();
}

static bool IsJapaneseLocale() {
  static bool sInitialized = false;
  static bool sIsJapanese = false;

  if (!sInitialized) {
    sInitialized = true;

    nsAutoCString appLocale;
    LocaleService::GetInstance()->GetAppLocaleAsBCP47(appLocale);

    const nsDependentCSubstring lang(appLocale, 0, 2);
    if (lang.EqualsLiteral("ja")) {
      sIsJapanese = true;
    } else {
      OSPreferences::GetInstance()->GetSystemLocale(appLocale);

      const nsDependentCSubstring lang(appLocale, 0, 2);
      if (lang.EqualsLiteral("ja")) {
        sIsJapanese = true;
      }
    }
  }

  return sIsJapanese;
}

void gfxAndroidPlatform::GetCommonFallbackFonts(
    uint32_t aCh, Script aRunScript, eFontPresentation aPresentation,
    nsTArray<const char*>& aFontList) {
  static const char kDroidSansJapanese[] = "Droid Sans Japanese";
  static const char kMotoyaLMaru[] = "MotoyaLMaru";
  static const char kNotoSansCJKJP[] = "Noto Sans CJK JP";
  static const char kNotoColorEmoji[] = "Noto Color Emoji";

  if (PrefersColor(aPresentation)) {
    aFontList.AppendElement(kNotoColorEmoji);
  }

  if (IS_IN_BMP(aCh)) {
    // try language-specific "Droid Sans *" and "Noto Sans *" fonts for
    // certain blocks, as most devices probably have these
    uint8_t block = (aCh >> 8) & 0xff;
    switch (block) {
      case 0x05:
        aFontList.AppendElement("Noto Sans Hebrew");
        aFontList.AppendElement("Droid Sans Hebrew");
        aFontList.AppendElement("Noto Sans Armenian");
        aFontList.AppendElement("Droid Sans Armenian");
        break;
      case 0x06:
        aFontList.AppendElement("Noto Sans Arabic");
        aFontList.AppendElement("Droid Sans Arabic");
        break;
      case 0x09:
        aFontList.AppendElement("Noto Sans Devanagari");
        aFontList.AppendElement("Noto Sans Bengali");
        aFontList.AppendElement("Droid Sans Devanagari");
        break;
      case 0x0a:
        aFontList.AppendElement("Noto Sans Gurmukhi");
        aFontList.AppendElement("Noto Sans Gujarati");
        break;
      case 0x0b:
        aFontList.AppendElement("Noto Sans Tamil");
        aFontList.AppendElement("Noto Sans Oriya");
        aFontList.AppendElement("Droid Sans Tamil");
        break;
      case 0x0c:
        aFontList.AppendElement("Noto Sans Telugu");
        aFontList.AppendElement("Noto Sans Kannada");
        break;
      case 0x0d:
        aFontList.AppendElement("Noto Sans Malayalam");
        aFontList.AppendElement("Noto Sans Sinhala");
        break;
      case 0x0e:
        aFontList.AppendElement("Noto Sans Thai");
        aFontList.AppendElement("Noto Sans Lao");
        aFontList.AppendElement("Droid Sans Thai");
        break;
      case 0x0f:
        aFontList.AppendElement("Noto Sans Tibetan");
        break;
      case 0x10:
      case 0x2d:
        aFontList.AppendElement("Noto Sans Georgian");
        aFontList.AppendElement("Droid Sans Georgian");
        break;
      case 0x12:
      case 0x13:
        aFontList.AppendElement("Noto Sans Ethiopic");
        aFontList.AppendElement("Droid Sans Ethiopic");
        break;
      case 0x21:
      case 0x23:
      case 0x24:
      case 0x26:
      case 0x27:
      case 0x29:
        aFontList.AppendElement("Noto Sans Symbols");
        break;
      case 0xf9:
      case 0xfa:
        if (IsJapaneseLocale()) {
          aFontList.AppendElement(kMotoyaLMaru);
          aFontList.AppendElement(kNotoSansCJKJP);
          aFontList.AppendElement(kDroidSansJapanese);
        }
        break;
      default:
        if (block >= 0x2e && block <= 0x9f && IsJapaneseLocale()) {
          aFontList.AppendElement(kMotoyaLMaru);
          aFontList.AppendElement(kNotoSansCJKJP);
          aFontList.AppendElement(kDroidSansJapanese);
        }
        break;
    }
  }
  // and try Droid Sans Fallback as a last resort
  aFontList.AppendElement("Droid Sans Fallback");
}

bool gfxAndroidPlatform::CreatePlatformFontList() {
  return gfxPlatformFontList::Initialize(new gfxFT2FontList);
}

void gfxAndroidPlatform::ReadSystemFontList(
    mozilla::dom::SystemFontList* aFontList) {
  gfxFT2FontList::PlatformFontList()->ReadSystemFontList(aFontList);
}

bool gfxAndroidPlatform::FontHintingEnabled() {
  // In "mobile" builds, we sometimes use non-reflow-zoom, so we
  // might not want hinting.  Let's see.

#ifdef MOZ_WIDGET_ANDROID
  // On Android, we currently only use gecko to render web
  // content that can always be be non-reflow-zoomed.  So turn off
  // hinting.
  //
  // XXX when gecko-android-java is used as an "app runtime", we may
  // want to re-enable hinting for non-browser processes there.
  //
  // If this value is changed, we must ensure that the argument passed
  // to SkInitCairoFT() in GPUParent::RecvInit() is also updated.
  return false;
#endif  //  MOZ_WIDGET_ANDROID

  // Currently, we don't have any other targets, but if/when we do,
  // decide how to handle them here.

  MOZ_ASSERT_UNREACHABLE("oops, what platform is this?");
  return gfxPlatform::FontHintingEnabled();
}

bool gfxAndroidPlatform::RequiresLinearZoom() {
#ifdef MOZ_WIDGET_ANDROID
  // On Android, we currently only use gecko to render web
  // content that can always be be non-reflow-zoomed.
  //
  // XXX when gecko-android-java is used as an "app runtime", we may
  // want to use linear zoom only for the web browser process, not other apps.
  return true;
#endif

  MOZ_ASSERT_UNREACHABLE("oops, what platform is this?");
  return gfxPlatform::RequiresLinearZoom();
}

class AndroidVsyncSource final : public VsyncSource {
 public:
  class Display final : public VsyncSource::Display,
                        public widget::AndroidVsync::Observer {
   public:
    Display() : mAndroidVsync(widget::AndroidVsync::GetInstance()) {}
    ~Display() { DisableVsync(); }

    bool IsVsyncEnabled() override {
      MOZ_ASSERT(NS_IsMainThread());
      return mObservingVsync;
    }

    void EnableVsync() override {
      MOZ_ASSERT(NS_IsMainThread());

      if (mObservingVsync) {
        return;
      }
      mAndroidVsync->RegisterObserver(this, widget::AndroidVsync::RENDER);
      mObservingVsync = true;
    }

    void DisableVsync() override {
      MOZ_ASSERT(NS_IsMainThread());

      if (!mObservingVsync) {
        return;
      }
      mAndroidVsync->UnregisterObserver(this, widget::AndroidVsync::RENDER);
      mObservingVsync = false;
    }

    TimeDuration GetVsyncRate() override {
      return mAndroidVsync->GetVsyncRate();
    }

    void Shutdown() override { DisableVsync(); }

    // Override for the widget::AndroidVsync::Observer method
    void OnVsync(const TimeStamp& aTimeStamp) override {
      // Use the timebase from the frame callback as the vsync time, unless it
      // is in the future.
      TimeStamp now = TimeStamp::Now();
      TimeStamp vsyncTime = aTimeStamp < now ? aTimeStamp : now;
      TimeStamp outputTime = vsyncTime + GetVsyncRate();

      NotifyVsync(vsyncTime, outputTime);
    }

   private:
    RefPtr<widget::AndroidVsync> mAndroidVsync;
    bool mObservingVsync = false;
    TimeDuration mVsyncDuration;
  };

  Display& GetGlobalDisplay() final { return GetDisplayInstance(); }

 private:
  virtual ~AndroidVsyncSource() = default;

  static Display& GetDisplayInstance() {
    static RefPtr<Display> globalDisplay = new Display();
    return *globalDisplay;
  }
};

already_AddRefed<mozilla::gfx::VsyncSource>
gfxAndroidPlatform::CreateHardwareVsyncSource() {
  // Vsync was introduced since JB (API 16~18) but inaccurate. Enable only for
  // KK (API 19) and later.
  if (jni::GetAPIVersion() >= 19) {
    RefPtr<AndroidVsyncSource> vsyncSource = new AndroidVsyncSource();
    return vsyncSource.forget();
  }

  NS_WARNING("Vsync not supported. Falling back to software vsync");
  return gfxPlatform::CreateHardwareVsyncSource();
}
