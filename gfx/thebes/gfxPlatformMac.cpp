/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxPlatformMac.h"

#include "gfxQuartzSurface.h"
#include "gfxQuartzImageSurface.h"
#include "mozilla/gfx/2D.h"

#include "gfxMacPlatformFontList.h"
#include "gfxMacFont.h"
#include "gfxCoreTextShaper.h"
#include "gfxTextRun.h"
#include "gfxUserFontSet.h"

#include "nsTArray.h"
#include "mozilla/Preferences.h"
#include "mozilla/VsyncDispatcher.h"
#include "qcms.h"
#include "gfx2DGlue.h"
#include "gfxPrefs.h"

#include <dlfcn.h>
#include <CoreVideo/CoreVideo.h>

#include "nsCocoaFeatures.h"

using namespace mozilla;
using namespace mozilla::gfx;

// cribbed from CTFontManager.h
enum {
   kAutoActivationDisabled = 1
};
typedef uint32_t AutoActivationSetting;

// bug 567552 - disable auto-activation of fonts

static void 
DisableFontActivation()
{
    // get the main bundle identifier
    CFBundleRef mainBundle = ::CFBundleGetMainBundle();
    CFStringRef mainBundleID = nullptr;

    if (mainBundle) {
        mainBundleID = ::CFBundleGetIdentifier(mainBundle);
    }

    // bug 969388 and bug 922590 - mainBundlID as null is sometimes problematic
    if (!mainBundleID) {
        NS_WARNING("missing bundle ID, packaging set up incorrectly");
        return;
    }

    // if possible, fetch CTFontManagerSetAutoActivationSetting
    void (*CTFontManagerSetAutoActivationSettingPtr)
            (CFStringRef, AutoActivationSetting);
    CTFontManagerSetAutoActivationSettingPtr =
        (void (*)(CFStringRef, AutoActivationSetting))
        dlsym(RTLD_DEFAULT, "CTFontManagerSetAutoActivationSetting");

    // bug 567552 - disable auto-activation of fonts
    if (CTFontManagerSetAutoActivationSettingPtr) {
        CTFontManagerSetAutoActivationSettingPtr(mainBundleID,
                                                 kAutoActivationDisabled);
    }
}

gfxPlatformMac::gfxPlatformMac()
{
    DisableFontActivation();
    mFontAntiAliasingThreshold = ReadAntiAliasingThreshold();

    uint32_t canvasMask = BackendTypeBit(BackendType::CAIRO) |
                          BackendTypeBit(BackendType::SKIA) |
                          BackendTypeBit(BackendType::COREGRAPHICS);
    uint32_t contentMask = BackendTypeBit(BackendType::COREGRAPHICS);
    InitBackendPrefs(canvasMask, BackendType::COREGRAPHICS,
                     contentMask, BackendType::COREGRAPHICS);
}

gfxPlatformMac::~gfxPlatformMac()
{
    gfxCoreTextShaper::Shutdown();
}

gfxPlatformFontList*
gfxPlatformMac::CreatePlatformFontList()
{
    gfxPlatformFontList* list = new gfxMacPlatformFontList();
    if (NS_SUCCEEDED(list->InitFontList())) {
        return list;
    }
    gfxPlatformFontList::Shutdown();
    return nullptr;
}

already_AddRefed<gfxASurface>
gfxPlatformMac::CreateOffscreenSurface(const IntSize& size,
                                       gfxContentType contentType)
{
    nsRefPtr<gfxASurface> newSurface =
      new gfxQuartzSurface(ThebesIntSize(size),
                           OptimalFormatForContent(contentType));
    return newSurface.forget();
}

TemporaryRef<ScaledFont>
gfxPlatformMac::GetScaledFontForFont(DrawTarget* aTarget, gfxFont *aFont)
{
    gfxMacFont *font = static_cast<gfxMacFont*>(aFont);
    return font->GetScaledFont(aTarget);
}

nsresult
gfxPlatformMac::GetStandardFamilyName(const nsAString& aFontName, nsAString& aFamilyName)
{
    gfxPlatformFontList::PlatformFontList()->GetStandardFamilyName(aFontName, aFamilyName);
    return NS_OK;
}

gfxFontGroup *
gfxPlatformMac::CreateFontGroup(const FontFamilyList& aFontFamilyList,
                                const gfxFontStyle *aStyle,
                                gfxUserFontSet *aUserFontSet)
{
    return new gfxFontGroup(aFontFamilyList, aStyle, aUserFontSet);
}

// these will move to gfxPlatform once all platforms support the fontlist
gfxFontEntry* 
gfxPlatformMac::LookupLocalFont(const nsAString& aFontName,
                                uint16_t aWeight,
                                int16_t aStretch,
                                bool aItalic)
{
    return gfxPlatformFontList::PlatformFontList()->LookupLocalFont(aFontName,
                                                                    aWeight,
                                                                    aStretch,
                                                                    aItalic);
}

gfxFontEntry* 
gfxPlatformMac::MakePlatformFont(const nsAString& aFontName,
                                 uint16_t aWeight,
                                 int16_t aStretch,
                                 bool aItalic,
                                 const uint8_t* aFontData,
                                 uint32_t aLength)
{
    // Ownership of aFontData is received here, and passed on to
    // gfxPlatformFontList::MakePlatformFont(), which must ensure the data
    // is released with NS_Free when no longer needed
    return gfxPlatformFontList::PlatformFontList()->MakePlatformFont(aFontName,
                                                                     aWeight,
                                                                     aStretch,
                                                                     aItalic,
                                                                     aFontData,
                                                                     aLength);
}

bool
gfxPlatformMac::IsFontFormatSupported(nsIURI *aFontURI, uint32_t aFormatFlags)
{
    // check for strange format flags
    NS_ASSERTION(!(aFormatFlags & gfxUserFontSet::FLAG_FORMAT_NOT_USED),
                 "strange font format hint set");

    // accept supported formats
    if (aFormatFlags & (gfxUserFontSet::FLAG_FORMATS_COMMON |
                        gfxUserFontSet::FLAG_FORMAT_TRUETYPE_AAT)) {
        return true;
    }

    // reject all other formats, known and unknown
    if (aFormatFlags != 0) {
        return false;
    }

    // no format hint set, need to look at data
    return true;
}

// these will also move to gfxPlatform once all platforms support the fontlist
nsresult
gfxPlatformMac::GetFontList(nsIAtom *aLangGroup,
                            const nsACString& aGenericFamily,
                            nsTArray<nsString>& aListOfFonts)
{
    gfxPlatformFontList::PlatformFontList()->GetFontList(aLangGroup, aGenericFamily, aListOfFonts);
    return NS_OK;
}

nsresult
gfxPlatformMac::UpdateFontList()
{
    gfxPlatformFontList::PlatformFontList()->UpdateFontList();
    return NS_OK;
}

static const char kFontArialUnicodeMS[] = "Arial Unicode MS";
static const char kFontAppleBraille[] = "Apple Braille";
static const char kFontAppleColorEmoji[] = "Apple Color Emoji";
static const char kFontAppleSymbols[] = "Apple Symbols";
static const char kFontDevanagariSangamMN[] = "Devanagari Sangam MN";
static const char kFontEuphemiaUCAS[] = "Euphemia UCAS";
static const char kFontGeneva[] = "Geneva";
static const char kFontGeezaPro[] = "Geeza Pro";
static const char kFontGujaratiSangamMN[] = "Gujarati Sangam MN";
static const char kFontGurmukhiMN[] = "Gurmukhi MN";
static const char kFontHiraginoKakuGothic[] = "Hiragino Kaku Gothic ProN";
static const char kFontHiraginoSansGB[] = "Hiragino Sans GB";
static const char kFontKefa[] = "Kefa";
static const char kFontKhmerMN[] = "Khmer MN";
static const char kFontLaoMN[] = "Lao MN";
static const char kFontLucidaGrande[] = "Lucida Grande";
static const char kFontMenlo[] = "Menlo";
static const char kFontMicrosoftTaiLe[] = "Microsoft Tai Le";
static const char kFontMingLiUExtB[] = "MingLiU-ExtB";
static const char kFontMyanmarMN[] = "Myanmar MN";
static const char kFontPlantagenetCherokee[] = "Plantagenet Cherokee";
static const char kFontSimSunExtB[] = "SimSun-ExtB";
static const char kFontSongtiSC[] = "Songti SC";
static const char kFontSTHeiti[] = "STHeiti";
static const char kFontSTIXGeneral[] = "STIXGeneral";
static const char kFontTamilMN[] = "Tamil MN";

void
gfxPlatformMac::GetCommonFallbackFonts(uint32_t aCh, uint32_t aNextCh,
                                       int32_t aRunScript,
                                       nsTArray<const char*>& aFontList)
{
    if (aNextCh == 0xfe0f) {
        aFontList.AppendElement(kFontAppleColorEmoji);
    }

    aFontList.AppendElement(kFontLucidaGrande);

    if (!IS_IN_BMP(aCh)) {
        uint32_t p = aCh >> 16;
        uint32_t b = aCh >> 8;
        if (p == 1) {
            if (b >= 0x1f0 && b < 0x1f7) {
                aFontList.AppendElement(kFontAppleColorEmoji);
            } else {
                aFontList.AppendElement(kFontAppleSymbols);
                aFontList.AppendElement(kFontSTIXGeneral);
                aFontList.AppendElement(kFontGeneva);
            }
        } else if (p == 2) {
            // OSX installations with MS Office may have these fonts
            aFontList.AppendElement(kFontMingLiUExtB);
            aFontList.AppendElement(kFontSimSunExtB);
        }
    } else {
        uint32_t b = (aCh >> 8) & 0xff;

        switch (b) {
        case 0x03:
        case 0x05:
            aFontList.AppendElement(kFontGeneva);
            break;
        case 0x07:
            aFontList.AppendElement(kFontGeezaPro);
            break;
        case 0x09:
            aFontList.AppendElement(kFontDevanagariSangamMN);
            break;
        case 0x0a:
            aFontList.AppendElement(kFontGurmukhiMN);
            aFontList.AppendElement(kFontGujaratiSangamMN);
            break;
        case 0x0b:
            aFontList.AppendElement(kFontTamilMN);
            break;
        case 0x0e:
            aFontList.AppendElement(kFontLaoMN);
            break;
        case 0x0f:
            aFontList.AppendElement(kFontSongtiSC);
            break;
        case 0x10:
            aFontList.AppendElement(kFontMenlo);
            aFontList.AppendElement(kFontMyanmarMN);
            break;
        case 0x13:  // Cherokee
            aFontList.AppendElement(kFontPlantagenetCherokee);
            aFontList.AppendElement(kFontKefa);
            break;
        case 0x14:  // Unified Canadian Aboriginal Syllabics
        case 0x15:
        case 0x16:
            aFontList.AppendElement(kFontEuphemiaUCAS);
            aFontList.AppendElement(kFontGeneva);
            break;
        case 0x18:  // Mongolian, UCAS
            aFontList.AppendElement(kFontSTHeiti);
            aFontList.AppendElement(kFontEuphemiaUCAS);
            break;
        case 0x19:  // Khmer
            aFontList.AppendElement(kFontKhmerMN);
            aFontList.AppendElement(kFontMicrosoftTaiLe);
            break;
        case 0x1d:
        case 0x1e:
            aFontList.AppendElement(kFontGeneva);
            break;
        case 0x20:  // Symbol ranges
        case 0x21:
        case 0x22:
        case 0x23:
        case 0x24:
        case 0x25:
        case 0x26:
        case 0x27:
        case 0x29:
        case 0x2a:
        case 0x2b:
        case 0x2e:
            aFontList.AppendElement(kFontHiraginoKakuGothic);
            aFontList.AppendElement(kFontAppleSymbols);
            aFontList.AppendElement(kFontMenlo);
            aFontList.AppendElement(kFontSTIXGeneral);
            aFontList.AppendElement(kFontGeneva);
            aFontList.AppendElement(kFontAppleColorEmoji);
            break;
        case 0x2c:
            aFontList.AppendElement(kFontGeneva);
            break;
        case 0x2d:
            aFontList.AppendElement(kFontKefa);
            aFontList.AppendElement(kFontGeneva);
            break;
        case 0x28:  // Braille
            aFontList.AppendElement(kFontAppleBraille);
            break;
        case 0x31:
            aFontList.AppendElement(kFontHiraginoSansGB);
            break;
        case 0x4d:
            aFontList.AppendElement(kFontAppleSymbols);
            break;
        case 0xa0:  // Yi
        case 0xa1:
        case 0xa2:
        case 0xa3:
        case 0xa4:
            aFontList.AppendElement(kFontSTHeiti);
            break;
        case 0xa6:
        case 0xa7:
            aFontList.AppendElement(kFontGeneva);
            aFontList.AppendElement(kFontAppleSymbols);
            break;
        case 0xab:
            aFontList.AppendElement(kFontKefa);
            break;
        case 0xfc:
        case 0xff:
            aFontList.AppendElement(kFontAppleSymbols);
            break;
        default:
            break;
        }
    }

    // Arial Unicode MS has lots of glyphs for obscure, use it as a last resort
    aFontList.AppendElement(kFontArialUnicodeMS);
}

uint32_t
gfxPlatformMac::ReadAntiAliasingThreshold()
{
    uint32_t threshold = 0;  // default == no threshold

    // first read prefs flag to determine whether to use the setting or not
    bool useAntiAliasingThreshold = Preferences::GetBool("gfx.use_text_smoothing_setting", false);

    // if the pref setting is disabled, return 0 which effectively disables this feature
    if (!useAntiAliasingThreshold)
        return threshold;

    // value set via Appearance pref panel, "Turn off text smoothing for font sizes xxx and smaller"
    CFNumberRef prefValue = (CFNumberRef)CFPreferencesCopyAppValue(CFSTR("AppleAntiAliasingThreshold"), kCFPreferencesCurrentApplication);

    if (prefValue) {
        if (!CFNumberGetValue(prefValue, kCFNumberIntType, &threshold)) {
            threshold = 0;
        }
        CFRelease(prefValue);
    }

    return threshold;
}

bool
gfxPlatformMac::UseAcceleratedCanvas()
{
  // Lion or later is required
  return nsCocoaFeatures::OnLionOrLater() && Preferences::GetBool("gfx.canvas.azure.accelerated", false);
}

bool
gfxPlatformMac::UseTiling()
{
  if (gfxPrefs::LayersTilesForceEnabled()) {
    return true;
  }
  // Tiling seems to be slow on 10.6 so disable it until we figure it out
  return nsCocoaFeatures::OnLionOrLater() && gfxPlatform::UseTiling();
}

bool
gfxPlatformMac::UseProgressivePaint()
{
  // Progressive painting requires cross-process mutexes, which don't work so
  // well on OS X 10.6 so we disable there.
  return nsCocoaFeatures::OnLionOrLater() && gfxPlatform::UseProgressivePaint();
}

// This is the renderer output callback function, called on the vsync thread
static CVReturn VsyncCallback(CVDisplayLinkRef aDisplayLink,
                              const CVTimeStamp* aNow,
                              const CVTimeStamp* aOutputTime,
                              CVOptionFlags aFlagsIn,
                              CVOptionFlags* aFlagsOut,
                              void* aDisplayLinkContext)
{
  mozilla::VsyncSource* vsyncSource = (mozilla::VsyncSource*) aDisplayLinkContext;
  if (vsyncSource->IsVsyncEnabled()) {
    // Now refers to "Now" as in when this callback is called or when the current frame
    // is displayed. aOutputTime is when the next frame should be displayed.
    // Now is VERY VERY noisy, aOutputTime is in the future though.
    int64_t timestamp = aOutputTime->hostTime;
    mozilla::TimeStamp vsyncTime = mozilla::TimeStamp::FromSystemTime(timestamp);
    mozilla::VsyncDispatcher::GetInstance()->NotifyVsync(vsyncTime);
    return kCVReturnSuccess;
  } else {
    return kCVReturnDisplayLinkNotRunning;
  }
}

class OSXVsyncSource MOZ_FINAL : public mozilla::VsyncSource
{
public:
  OSXVsyncSource()
  {
    EnableVsync();
  }

  virtual void EnableVsync() MOZ_OVERRIDE
  {
    // Create a display link capable of being used with all active displays
    // TODO: See if we need to create an active DisplayLink for each monitor in multi-monitor
    // situations. According to the docs, it is compatible with all displays running on the computer
    // But if we have different monitors at different display rates, we may hit issues.
    if (CVDisplayLinkCreateWithActiveCGDisplays(&mDisplayLink) != kCVReturnSuccess) {
      NS_WARNING("Could not create a display link, returning");
      return;
    }

    // Set the renderer output callback function
    if (CVDisplayLinkSetOutputCallback(mDisplayLink, &VsyncCallback, this) != kCVReturnSuccess) {
      NS_WARNING("Could not set displaylink output callback");
      return;
    }

    // Activate the display link
    if (CVDisplayLinkStart(mDisplayLink) != kCVReturnSuccess) {
      NS_WARNING("Could not activate the display link");
      mDisplayLink = nullptr;
    }
  }

  virtual void DisableVsync() MOZ_OVERRIDE
  {
    // Release the display link
    if (mDisplayLink) {
      CVDisplayLinkRelease(mDisplayLink);
      mDisplayLink = nullptr;
    }
  }

  virtual bool IsVsyncEnabled() MOZ_OVERRIDE
  {
    return mDisplayLink != nullptr;
  }

private:
  virtual ~OSXVsyncSource()
  {
    DisableVsync();
  }

  // Manages the display link render thread
  CVDisplayLinkRef   mDisplayLink;
}; // OSXVsyncSource

void
gfxPlatformMac::InitHardwareVsync()
{
  nsRefPtr<VsyncSource> osxVsyncSource = new OSXVsyncSource();
  mozilla::VsyncDispatcher::GetInstance()->SetVsyncSource(osxVsyncSource);
}

void
gfxPlatformMac::GetPlatformCMSOutputProfile(void* &mem, size_t &size)
{
    mem = nullptr;
    size = 0;

    CGColorSpaceRef cspace = ::CGDisplayCopyColorSpace(::CGMainDisplayID());
    if (!cspace) {
        cspace = ::CGColorSpaceCreateDeviceRGB();
    }
    if (!cspace) {
        return;
    }

    CFDataRef iccp = ::CGColorSpaceCopyICCProfile(cspace);

    ::CFRelease(cspace);

    if (!iccp) {
        return;
    }

    // copy to external buffer
    size = static_cast<size_t>(::CFDataGetLength(iccp));
    if (size > 0) {
        void *data = malloc(size);
        if (data) {
            memcpy(data, ::CFDataGetBytePtr(iccp), size);
            mem = data;
        } else {
            size = 0;
        }
    }

    ::CFRelease(iccp);
}
