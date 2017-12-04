/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"

#include "gfxAndroidPlatform.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/CountingAllocatorBase.h"
#include "mozilla/intl/LocaleService.h"
#include "mozilla/intl/OSPreferences.h"
#include "mozilla/Preferences.h"

#include "gfx2DGlue.h"
#include "gfxFT2FontList.h"
#include "gfxImageSurface.h"
#include "gfxTextRun.h"
#include "mozilla/dom/ContentChild.h"
#include "nsXULAppAPI.h"
#include "nsIScreen.h"
#include "nsIScreenManager.h"
#include "nsServiceManagerUtils.h"
#include "nsUnicodeProperties.h"
#include "gfxPrefs.h"
#include "cairo.h"
#include "VsyncSource.h"

#include "ft2build.h"
#include FT_FREETYPE_H
#include FT_MODULE_H

#include "GeneratedJNINatives.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::gfx;
using namespace mozilla::unicode;
using mozilla::intl::LocaleService;
using mozilla::intl::OSPreferences;

static FT_Library gPlatformFTLibrary = nullptr;

class FreetypeReporter final : public nsIMemoryReporter,
                               public CountingAllocatorBase<FreetypeReporter>
{
private:
    ~FreetypeReporter() {}

public:
    NS_DECL_ISUPPORTS

    static void* Malloc(FT_Memory, long size)
    {
        return CountingMalloc(size);
    }

    static void Free(FT_Memory, void* p)
    {
        return CountingFree(p);
    }

    static void*
    Realloc(FT_Memory, long cur_size, long new_size, void* p)
    {
        return CountingRealloc(p, new_size);
    }

    NS_IMETHOD CollectReports(nsIHandleReportCallback* aHandleReport,
                              nsISupports* aData, bool aAnonymize) override
    {
        MOZ_COLLECT_REPORT(
            "explicit/freetype", KIND_HEAP, UNITS_BYTES, MemoryAllocated(),
            "Memory used by Freetype.");

        return NS_OK;
    }
};

NS_IMPL_ISUPPORTS(FreetypeReporter, nsIMemoryReporter)

template<> Atomic<size_t> CountingAllocatorBase<FreetypeReporter>::sAmount(0);

static FT_MemoryRec_ sFreetypeMemoryRecord;

gfxAndroidPlatform::gfxAndroidPlatform()
{
    // A custom allocator.  It counts allocations, enabling memory reporting.
    sFreetypeMemoryRecord.user    = nullptr;
    sFreetypeMemoryRecord.alloc   = FreetypeReporter::Malloc;
    sFreetypeMemoryRecord.free    = FreetypeReporter::Free;
    sFreetypeMemoryRecord.realloc = FreetypeReporter::Realloc;

    // These two calls are equivalent to FT_Init_FreeType(), but allow us to
    // provide a custom memory allocator.
    FT_New_Library(&sFreetypeMemoryRecord, &gPlatformFTLibrary);
    FT_Add_Default_Modules(gPlatformFTLibrary);

    Factory::SetFTLibrary(gPlatformFTLibrary);

    RegisterStrongMemoryReporter(new FreetypeReporter());

    mOffscreenFormat = GetScreenDepth() == 16
                       ? SurfaceFormat::R5G6B5_UINT16
                       : SurfaceFormat::X8R8G8B8_UINT32;

    if (gfxPrefs::AndroidRGB16Force()) {
        mOffscreenFormat = SurfaceFormat::R5G6B5_UINT16;
    }
}

gfxAndroidPlatform::~gfxAndroidPlatform()
{
    FT_Done_Library(gPlatformFTLibrary);
    gPlatformFTLibrary = nullptr;
}

already_AddRefed<gfxASurface>
gfxAndroidPlatform::CreateOffscreenSurface(const IntSize& aSize,
                                           gfxImageFormat aFormat)
{
    if (!Factory::AllowedSurfaceSize(aSize)) {
        return nullptr;
    }

    RefPtr<gfxASurface> newSurface;
    newSurface = new gfxImageSurface(aSize, aFormat);

    return newSurface.forget();
}

static bool
IsJapaneseLocale()
{
    static bool sInitialized = false;
    static bool sIsJapanese = false;

    if (!sInitialized) {
        sInitialized = true;

        nsAutoCString appLocale;
        LocaleService::GetInstance()->GetAppLocaleAsLangTag(appLocale);

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

void
gfxAndroidPlatform::GetCommonFallbackFonts(uint32_t aCh, uint32_t aNextCh,
                                           Script aRunScript,
                                           nsTArray<const char*>& aFontList)
{
    static const char kDroidSansJapanese[] = "Droid Sans Japanese";
    static const char kMotoyaLMaru[] = "MotoyaLMaru";
    static const char kNotoSansCJKJP[] = "Noto Sans CJK JP";
    static const char kNotoColorEmoji[] = "Noto Color Emoji";

    EmojiPresentation emoji = GetEmojiPresentation(aCh);
    if (emoji != EmojiPresentation::TextOnly) {
        if (aNextCh == kVariationSelector16 ||
           (aNextCh != kVariationSelector15 &&
            emoji == EmojiPresentation::EmojiDefault)) {
            // if char is followed by VS16, try for a color emoji glyph
            aFontList.AppendElement(kNotoColorEmoji);
        }
    }

    if (IS_IN_BMP(aCh)) {
        // try language-specific "Droid Sans *" and "Noto Sans *" fonts for
        // certain blocks, as most devices probably have these
        uint8_t block = (aCh >> 8) & 0xff;
        switch (block) {
        case 0x05:
            aFontList.AppendElement("Droid Sans Hebrew");
            aFontList.AppendElement("Droid Sans Armenian");
            break;
        case 0x06:
            aFontList.AppendElement("Droid Sans Arabic");
            break;
        case 0x09:
            aFontList.AppendElement("Noto Sans Devanagari");
            aFontList.AppendElement("Droid Sans Devanagari");
            break;
        case 0x0b:
            aFontList.AppendElement("Noto Sans Tamil");
            aFontList.AppendElement("Droid Sans Tamil");
            break;
        case 0x0e:
            aFontList.AppendElement("Noto Sans Thai");
            aFontList.AppendElement("Droid Sans Thai");
            break;
        case 0x10: case 0x2d:
            aFontList.AppendElement("Droid Sans Georgian");
            break;
        case 0x12: case 0x13:
            aFontList.AppendElement("Droid Sans Ethiopic");
            break;
        case 0xf9: case 0xfa:
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

void
gfxAndroidPlatform::GetSystemFontList(InfallibleTArray<FontListEntry>* retValue)
{
    gfxFT2FontList::PlatformFontList()->GetSystemFontList(retValue);
}

gfxPlatformFontList*
gfxAndroidPlatform::CreatePlatformFontList()
{
    gfxPlatformFontList* list = new gfxFT2FontList();
    if (NS_SUCCEEDED(list->InitFontList())) {
        return list;
    }
    gfxPlatformFontList::Shutdown();
    return nullptr;
}

gfxFontGroup *
gfxAndroidPlatform::CreateFontGroup(const FontFamilyList& aFontFamilyList,
                                    const gfxFontStyle* aStyle,
                                    gfxTextPerfMetrics* aTextPerf,
                                    gfxUserFontSet* aUserFontSet,
                                    gfxFloat aDevToCssSize)
{
    return new gfxFontGroup(aFontFamilyList, aStyle, aTextPerf,
                            aUserFontSet, aDevToCssSize);
}

FT_Library
gfxAndroidPlatform::GetFTLibrary()
{
    return gPlatformFTLibrary;
}

bool
gfxAndroidPlatform::FontHintingEnabled()
{
    // In "mobile" builds, we sometimes use non-reflow-zoom, so we
    // might not want hinting.  Let's see.

#ifdef MOZ_WIDGET_ANDROID
    // On Android, we currently only use gecko to render web
    // content that can always be be non-reflow-zoomed.  So turn off
    // hinting.
    //
    // XXX when gecko-android-java is used as an "app runtime", we may
    // want to re-enable hinting for non-browser processes there.
    return false;
#endif //  MOZ_WIDGET_ANDROID

    // Currently, we don't have any other targets, but if/when we do,
    // decide how to handle them here.

    NS_NOTREACHED("oops, what platform is this?");
    return gfxPlatform::FontHintingEnabled();
}

bool
gfxAndroidPlatform::RequiresLinearZoom()
{
#ifdef MOZ_WIDGET_ANDROID
    // On Android, we currently only use gecko to render web
    // content that can always be be non-reflow-zoomed.
    //
    // XXX when gecko-android-java is used as an "app runtime", we may
    // want to use linear zoom only for the web browser process, not other apps.
    return true;
#endif

    NS_NOTREACHED("oops, what platform is this?");
    return gfxPlatform::RequiresLinearZoom();
}

class AndroidVsyncSource final : public VsyncSource {
public:
    class JavaVsyncSupport final : public java::VsyncSource::Natives<JavaVsyncSupport>
    {
    public:
        using Base = java::VsyncSource::Natives<JavaVsyncSupport>;
        using Base::DisposeNative;

        static void NotifyVsync() {
            GetDisplayInstance().NotifyVsync(TimeStamp::Now());
        }
    };

    class Display final : public VsyncSource::Display {
    public:
        Display()
            : mJavaVsync(java::VsyncSource::INSTANCE())
            , mObservingVsync(false)
        {
            JavaVsyncSupport::Init(); // To register native methods.
        }

        ~Display() { DisableVsync(); }

        bool IsVsyncEnabled() override
        {
            MOZ_ASSERT(NS_IsMainThread());
            MOZ_ASSERT(mJavaVsync);

            return mObservingVsync;
        }

        void EnableVsync() override
        {
            MOZ_ASSERT(NS_IsMainThread());
            MOZ_ASSERT(mJavaVsync);

            if (mObservingVsync) {
                return;
            }
            bool ok = mJavaVsync->ObserveVsync(true);
            if (ok && !mVsyncDuration) {
                float fps = mJavaVsync->GetRefreshRate();
                mVsyncDuration = TimeDuration::FromMilliseconds(1000.0 / fps);
            }
            mObservingVsync = ok;
            MOZ_ASSERT(mObservingVsync);
        }

        void DisableVsync() override
        {
            MOZ_ASSERT(NS_IsMainThread());
            MOZ_ASSERT(mJavaVsync);

            if (!mObservingVsync) {
              return;
            }
            mObservingVsync = mJavaVsync->ObserveVsync(false);
            MOZ_ASSERT(!mObservingVsync);
        }

        TimeDuration GetVsyncRate() override { return mVsyncDuration; }

        void Shutdown() override {
            DisableVsync();
            mJavaVsync = nullptr;
        }

    private:
        java::VsyncSource::GlobalRef mJavaVsync;
        bool mObservingVsync;
        TimeDuration mVsyncDuration;
    };

    Display& GetGlobalDisplay() final { return GetDisplayInstance(); }

private:
   virtual ~AndroidVsyncSource() {}

   static Display& GetDisplayInstance()
   {
       static Display globalDisplay;
       return globalDisplay;
   }
};

already_AddRefed<mozilla::gfx::VsyncSource>
gfxAndroidPlatform::CreateHardwareVsyncSource()
{
    // Vsync was introduced since JB (API 16~18) but inaccurate. Enable only for
    // KK (API 19) and later.
    if (AndroidBridge::Bridge() &&
            AndroidBridge::Bridge()->GetAPIVersion() >= 19) {
        RefPtr<AndroidVsyncSource> vsyncSource = new AndroidVsyncSource();
        return vsyncSource.forget();
    }

    NS_WARNING("Vsync not supported. Falling back to software vsync");
    return gfxPlatform::CreateHardwareVsyncSource();
}
