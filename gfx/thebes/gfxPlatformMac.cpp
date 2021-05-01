/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxPlatformMac.h"

#include "gfxQuartzSurface.h"
#include "mozilla/DataMutex.h"
#include "mozilla/gfx/2D.h"

#include "gfxMacPlatformFontList.h"
#include "gfxMacFont.h"
#include "gfxCoreTextShaper.h"
#include "gfxTextRun.h"
#include "gfxUserFontSet.h"
#include "gfxConfig.h"

#include "AppleUtils.h"
#include "nsTArray.h"
#include "mozilla/Preferences.h"
#include "mozilla/VsyncDispatcher.h"
#include "nsCocoaFeatures.h"
#include "nsComponentManagerUtils.h"
#include "nsIFile.h"
#include "nsUnicodeProperties.h"
#include "qcms.h"
#include "gfx2DGlue.h"
#include "GeckoProfiler.h"
#include "nsThreadUtils.h"

#include <dlfcn.h>
#include <CoreVideo/CoreVideo.h>

#include "mozilla/layers/CompositorBridgeParent.h"
#include "mozilla/layers/SurfacePool.h"
#include "VsyncSource.h"

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::unicode;

using mozilla::dom::SystemFontList;

// cribbed from CTFontManager.h
enum { kAutoActivationDisabled = 1 };
typedef uint32_t AutoActivationSetting;

// bug 567552 - disable auto-activation of fonts

static void DisableFontActivation() {
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
  void (*CTFontManagerSetAutoActivationSettingPtr)(CFStringRef,
                                                   AutoActivationSetting);
  CTFontManagerSetAutoActivationSettingPtr =
      (void (*)(CFStringRef, AutoActivationSetting))dlsym(
          RTLD_DEFAULT, "CTFontManagerSetAutoActivationSetting");

  // bug 567552 - disable auto-activation of fonts
  if (CTFontManagerSetAutoActivationSettingPtr) {
    CTFontManagerSetAutoActivationSettingPtr(mainBundleID,
                                             kAutoActivationDisabled);
  }
}

// Helpers for gfxPlatformMac::RegisterSupplementalFonts below.
static void ActivateFontsFromDir(const nsACString& aDir) {
  AutoCFRelease<CFURLRef> directory = CFURLCreateFromFileSystemRepresentation(
      kCFAllocatorDefault, (const UInt8*)nsPromiseFlatCString(aDir).get(),
      aDir.Length(), true);
  if (!directory) {
    return;
  }
  AutoCFRelease<CFURLEnumeratorRef> enumerator =
      CFURLEnumeratorCreateForDirectoryURL(kCFAllocatorDefault, directory,
                                           kCFURLEnumeratorDefaultBehavior,
                                           nullptr);
  if (!enumerator) {
    return;
  }
  AutoCFRelease<CFMutableArrayRef> urls =
      ::CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
  if (!urls) {
    return;
  }

  CFURLRef url;
  CFURLEnumeratorResult result;
  do {
    result = CFURLEnumeratorGetNextURL(enumerator, &url, nullptr);
    if (result == kCFURLEnumeratorSuccess) {
      CFArrayAppendValue(urls, url);
    }
  } while (result != kCFURLEnumeratorEnd);

  CTFontManagerRegisterFontsForURLs(urls, kCTFontManagerScopeProcess, nullptr);
}

#ifdef MOZ_BUNDLED_FONTS
static void ActivateBundledFonts() {
  nsCOMPtr<nsIFile> localDir;
  if (NS_FAILED(NS_GetSpecialDirectory(NS_GRE_DIR, getter_AddRefs(localDir)))) {
    return;
  }
  if (NS_FAILED(localDir->Append(u"fonts"_ns))) {
    return;
  }
  nsAutoCString path;
  if (NS_FAILED(localDir->GetNativePath(path))) {
    return;
  }
  ActivateFontsFromDir(path);
}
#endif

// A bunch of fonts for "additional language support" are shipped in a
// "Language Support" directory, and don't show up in the standard font
// list returned by CTFontManagerCopyAvailableFontFamilyNames unless
// we explicitly activate them.
//
// On macOS Big Sur, the various Noto fonts etc have moved to a new location
// under /System/Fonts. Whether they're exposed in the font list by default
// depends on the SDK used; when built with SDK 10.15, they're absent. So
// we explicitly activate them to be sure they'll be available.
#if __MAC_OS_X_VERSION_MAX_ALLOWED < 101500
static const nsLiteralCString kLangFontsDirs[] = {
    "/Library/Application Support/Apple/Fonts/Language Support"_ns};
#else
static const nsLiteralCString kLangFontsDirs[] = {
    "/Library/Application Support/Apple/Fonts/Language Support"_ns,
    "/System/Library/Fonts/Supplemental"_ns};
#endif

static void FontRegistrationCallback(void* aUnused) {
  AUTO_PROFILER_REGISTER_THREAD("RegisterFonts");
  PR_SetCurrentThreadName("RegisterFonts");

  for (const auto& dir : kLangFontsDirs) {
    ActivateFontsFromDir(dir);
  }
}

PRThread* gfxPlatformMac::sFontRegistrationThread = nullptr;

/* This is called from XPCOM_Init during startup (before gfxPlatform has been
   initialized), so that it can kick off the font activation on a secondary
   thread, and hope that it'll be finished by the time we're ready to build
   our font list. */
/* static */
void gfxPlatformMac::RegisterSupplementalFonts() {
  // On Catalina+, it appears to be sufficient to activate fonts in the parent
  // process; they are then also usable in child processes. But on pre-Catalina
  // systems we need to explicitly activate them in each child process (per bug
  // 1704273).
  if (XRE_IsParentProcess() || !nsCocoaFeatures::OnCatalinaOrLater()) {
    sFontRegistrationThread = PR_CreateThread(
        PR_USER_THREAD, FontRegistrationCallback, nullptr, PR_PRIORITY_NORMAL,
        PR_GLOBAL_THREAD, PR_JOINABLE_THREAD, 0);
  }
}

/* static */
void gfxPlatformMac::WaitForFontRegistration() {
  if (sFontRegistrationThread) {
    PR_JoinThread(sFontRegistrationThread);
    sFontRegistrationThread = nullptr;

#ifdef MOZ_BUNDLED_FONTS
    // This is not done by the font registration thread because it uses the
    // XPCOM directory service, which is not yet available at the time we start
    // the registration thread.
    //
    // We activate bundled fonts if the pref is > 0 (on) or < 0 (auto), only an
    // explicit value of 0 (off) will disable them.
    if (StaticPrefs::gfx_bundled_fonts_activate_AtStartup() != 0) {
      TimeStamp start = TimeStamp::Now();
      ActivateBundledFonts();
      TimeStamp end = TimeStamp::Now();
      Telemetry::Accumulate(Telemetry::FONTLIST_BUNDLEDFONTS_ACTIVATE,
                            (end - start).ToMilliseconds());
    }
#endif
  }
}

gfxPlatformMac::gfxPlatformMac() {
  DisableFontActivation();
  mFontAntiAliasingThreshold = ReadAntiAliasingThreshold();

  InitBackendPrefs(GetBackendPrefs());

  if (nsCocoaFeatures::OnHighSierraOrLater()) {
    mHasNativeColrFontSupport = true;
  }
}

gfxPlatformMac::~gfxPlatformMac() { gfxCoreTextShaper::Shutdown(); }

BackendPrefsData gfxPlatformMac::GetBackendPrefs() const {
  BackendPrefsData data;

  data.mCanvasBitmask = BackendTypeBit(BackendType::SKIA);
  data.mContentBitmask = BackendTypeBit(BackendType::SKIA);
  data.mCanvasDefault = BackendType::SKIA;
  data.mContentDefault = BackendType::SKIA;

  return data;
}

bool gfxPlatformMac::UsesTiling() const {
  // The non-tiling ContentClient requires CrossProcessSemaphore which
  // isn't implemented for OSX.
  return true;
}

bool gfxPlatformMac::ContentUsesTiling() const { return UsesTiling(); }

gfxPlatformFontList* gfxPlatformMac::CreatePlatformFontList() {
  gfxPlatformFontList* list = new gfxMacPlatformFontList();
  if (NS_SUCCEEDED(list->InitFontList())) {
    return list;
  }
  gfxPlatformFontList::Shutdown();
  return nullptr;
}

void gfxPlatformMac::ReadSystemFontList(SystemFontList* aFontList) {
  gfxMacPlatformFontList::PlatformFontList()->ReadSystemFontList(aFontList);
}

already_AddRefed<gfxASurface> gfxPlatformMac::CreateOffscreenSurface(
    const IntSize& aSize, gfxImageFormat aFormat) {
  if (!Factory::AllowedSurfaceSize(aSize)) {
    return nullptr;
  }

  RefPtr<gfxASurface> newSurface = new gfxQuartzSurface(aSize, aFormat);
  return newSurface.forget();
}

bool gfxPlatformMac::IsFontFormatSupported(uint32_t aFormatFlags) {
  if (gfxPlatform::IsFontFormatSupported(aFormatFlags)) {
    return true;
  }

  // If the generic method rejected the format hint, then check for any
  // platform-specific format we know about.
  if (aFormatFlags & gfxUserFontSet::FLAG_FORMAT_TRUETYPE_AAT) {
    return true;
  }

  return false;
}

void gfxPlatformMac::GetCommonFallbackFonts(uint32_t aCh, Script aRunScript,
                                            eFontPresentation aPresentation,
                                            nsTArray<const char*>& aFontList) {
  if (PrefersColor(aPresentation)) {
    aFontList.AppendElement("Apple Color Emoji");
  }

  switch (aRunScript) {
    case Script::INVALID:
    case Script::NUM_SCRIPT_CODES:
      // Ensure the switch covers all the Script enum values.
      MOZ_ASSERT_UNREACHABLE("bad script code");
      break;

    case Script::COMMON:
    case Script::INHERITED:
      // In most cases, COMMON and INHERITED characters will be merged into
      // their context, but if they occur without any specific script context
      // we'll just try common default fonts here.
    case Script::LATIN:
    case Script::CYRILLIC:
    case Script::GREEK:
      aFontList.AppendElement("Lucida Grande");
      break;

    case Script::MATHEMATICAL_NOTATION:
    case Script::SYMBOLS:
    case Script::SYMBOLS_EMOJI:
      // Not currently returned by script run resolution (but see below, after
      // the switch).
      break;

    // CJK-related script codes are a bit troublesome because of unification;
    // we'll probably just get HAN much of the time, so the choice of which
    // language font to try for fallback is rather arbitrary. Usually, though,
    // we hope that font prefs will have handled this earlier.
    case Script::BOPOMOFO:
    case Script::HAN_WITH_BOPOMOFO:
    case Script::SIMPLIFIED_HAN:
    case Script::HAN:
      aFontList.AppendElement("Songti SC");
      if (aCh > 0x10000) {
        // macOS installations with MS Office may have these -ExtB fonts
        aFontList.AppendElement("SimSun-ExtB");
      }
      break;

    // Currently, we don't resolve script runs to this value, but we may do so
    // in future if we get better at handling things like `lang=zh-Hant`, not
    // just resolving based on the Unicode text.
    case Script::TRADITIONAL_HAN:
      aFontList.AppendElement("Songti TC");
      if (aCh > 0x10000) {
        // macOS installations with MS Office may have these -ExtB fonts
        aFontList.AppendElement("MingLiU-ExtB");
      }
      break;

    case Script::HIRAGANA:
    case Script::KATAKANA:
    case Script::KATAKANA_OR_HIRAGANA:
    case Script::JAPANESE:
      aFontList.AppendElement("Hiragino Sans");
      aFontList.AppendElement("Hiragino Kaku Gothic ProN");
      break;

    case Script::JAMO:
    case Script::KOREAN:
    case Script::HANGUL:
      aFontList.AppendElement("Nanum Gothic");
      aFontList.AppendElement("Apple SD Gothic Neo");
      break;

    // For most other scripts, macOS comes with a default font we can use.
    case Script::ARABIC:
      aFontList.AppendElement("Geeza Pro");
      break;
    case Script::ARMENIAN:
      aFontList.AppendElement("Mshtakan");
      break;
    case Script::BENGALI:
      aFontList.AppendElement("Bangla Sangam MN");
      break;
    case Script::CHEROKEE:
      aFontList.AppendElement("Plantagenet Cherokee");
      break;
    case Script::COPTIC:
      aFontList.AppendElement("Noto Sans Coptic");
      break;
    case Script::DESERET:
      aFontList.AppendElement("Baskerville");
      break;
    case Script::DEVANAGARI:
      aFontList.AppendElement("Devanagari Sangam MN");
      break;
    case Script::ETHIOPIC:
      aFontList.AppendElement("Kefa");
      break;
    case Script::GEORGIAN:
      aFontList.AppendElement("Helvetica");
      break;
    case Script::GOTHIC:
      aFontList.AppendElement("Noto Sans Gothic");
      break;
    case Script::GUJARATI:
      aFontList.AppendElement("Gujarati Sangam MN");
      break;
    case Script::GURMUKHI:
      aFontList.AppendElement("Gurmukhi MN");
      break;
    case Script::HEBREW:
      aFontList.AppendElement("Lucida Grande");
      break;
    case Script::KANNADA:
      aFontList.AppendElement("Kannada MN");
      break;
    case Script::KHMER:
      aFontList.AppendElement("Khmer MN");
      break;
    case Script::LAO:
      aFontList.AppendElement("Lao MN");
      break;
    case Script::MALAYALAM:
      aFontList.AppendElement("Malayalam Sangam MN");
      break;
    case Script::MONGOLIAN:
      aFontList.AppendElement("Noto Sans Mongolian");
      break;
    case Script::MYANMAR:
      aFontList.AppendElement("Myanmar MN");
      break;
    case Script::OGHAM:
      aFontList.AppendElement("Noto Sans Ogham");
      break;
    case Script::OLD_ITALIC:
      aFontList.AppendElement("Noto Sans Old Italic");
      break;
    case Script::ORIYA:
      aFontList.AppendElement("Oriya Sangam MN");
      break;
    case Script::RUNIC:
      aFontList.AppendElement("Noto Sans Runic");
      break;
    case Script::SINHALA:
      aFontList.AppendElement("Sinhala Sangam MN");
      break;
    case Script::SYRIAC:
      aFontList.AppendElement("Noto Sans Syriac");
      break;
    case Script::TAMIL:
      aFontList.AppendElement("Tamil MN");
      break;
    case Script::TELUGU:
      aFontList.AppendElement("Telugu MN");
      break;
    case Script::THAANA:
      aFontList.AppendElement("Noto Sans Thaana");
      break;
    case Script::THAI:
      aFontList.AppendElement("Thonburi");
      break;
    case Script::TIBETAN:
      aFontList.AppendElement("Kailasa");
      break;
    case Script::CANADIAN_ABORIGINAL:
      aFontList.AppendElement("Euphemia UCAS");
      break;
    case Script::YI:
      aFontList.AppendElement("Noto Sans Yi");
      aFontList.AppendElement("STHeiti");
      break;
    case Script::TAGALOG:
      aFontList.AppendElement("Noto Sans Tagalog");
      break;
    case Script::HANUNOO:
      aFontList.AppendElement("Noto Sans Hanunoo");
      break;
    case Script::BUHID:
      aFontList.AppendElement("Noto Sans Buhid");
      break;
    case Script::TAGBANWA:
      aFontList.AppendElement("Noto Sans Tagbanwa");
      break;
    case Script::BRAILLE:
      aFontList.AppendElement("Apple Braille");
      break;
    case Script::CYPRIOT:
      aFontList.AppendElement("Noto Sans Cypriot");
      break;
    case Script::LIMBU:
      aFontList.AppendElement("Noto Sans Limbu");
      break;
    case Script::LINEAR_B:
      aFontList.AppendElement("Noto Sans Linear B");
      break;
    case Script::OSMANYA:
      aFontList.AppendElement("Noto Sans Osmanya");
      break;
    case Script::SHAVIAN:
      aFontList.AppendElement("Noto Sans Shavian");
      break;
    case Script::TAI_LE:
      aFontList.AppendElement("Noto Sans Tai Le");
      break;
    case Script::UGARITIC:
      aFontList.AppendElement("Noto Sans Ugaritic");
      break;
    case Script::BUGINESE:
      aFontList.AppendElement("Noto Sans Buginese");
      break;
    case Script::GLAGOLITIC:
      aFontList.AppendElement("Noto Sans Glagolitic");
      break;
    case Script::KHAROSHTHI:
      aFontList.AppendElement("Noto Sans Kharoshthi");
      break;
    case Script::SYLOTI_NAGRI:
      aFontList.AppendElement("Noto Sans Syloti Nagri");
      break;
    case Script::NEW_TAI_LUE:
      aFontList.AppendElement("Noto Sans New Tai Lue");
      break;
    case Script::TIFINAGH:
      aFontList.AppendElement("Noto Sans Tifinagh");
      break;
    case Script::OLD_PERSIAN:
      aFontList.AppendElement("Noto Sans Old Persian");
      break;
    case Script::BALINESE:
      aFontList.AppendElement("Noto Sans Balinese");
      break;
    case Script::BATAK:
      aFontList.AppendElement("Noto Sans Batak");
      break;
    case Script::BRAHMI:
      aFontList.AppendElement("Noto Sans Brahmi");
      break;
    case Script::CHAM:
      aFontList.AppendElement("Noto Sans Cham");
      break;
    case Script::EGYPTIAN_HIEROGLYPHS:
      aFontList.AppendElement("Noto Sans Egyptian Hieroglyphs");
      break;
    case Script::PAHAWH_HMONG:
      aFontList.AppendElement("Noto Sans Pahawh Hmong");
      break;
    case Script::OLD_HUNGARIAN:
      aFontList.AppendElement("Noto Sans Old Hungarian");
      break;
    case Script::JAVANESE:
      aFontList.AppendElement("Noto Sans Javanese");
      break;
    case Script::KAYAH_LI:
      aFontList.AppendElement("Noto Sans Kayah Li");
      break;
    case Script::LEPCHA:
      aFontList.AppendElement("Noto Sans Lepcha");
      break;
    case Script::LINEAR_A:
      aFontList.AppendElement("Noto Sans Linear A");
      break;
    case Script::MANDAIC:
      aFontList.AppendElement("Noto Sans Mandaic");
      break;
    case Script::NKO:
      aFontList.AppendElement("Noto Sans NKo");
      break;
    case Script::OLD_TURKIC:
      aFontList.AppendElement("Noto Sans Old Turkic");
      break;
    case Script::OLD_PERMIC:
      aFontList.AppendElement("Noto Sans Old Permic");
      break;
    case Script::PHAGS_PA:
      aFontList.AppendElement("Noto Sans PhagsPa");
      break;
    case Script::PHOENICIAN:
      aFontList.AppendElement("Noto Sans Phoenician");
      break;
    case Script::MIAO:
      aFontList.AppendElement("Noto Sans Miao");
      break;
    case Script::VAI:
      aFontList.AppendElement("Noto Sans Vai");
      break;
    case Script::CUNEIFORM:
      aFontList.AppendElement("Noto Sans Cuneiform");
      break;
    case Script::CARIAN:
      aFontList.AppendElement("Noto Sans Carian");
      break;
    case Script::TAI_THAM:
      aFontList.AppendElement("Noto Sans Tai Tham");
      break;
    case Script::LYCIAN:
      aFontList.AppendElement("Noto Sans Lycian");
      break;
    case Script::LYDIAN:
      aFontList.AppendElement("Noto Sans Lydian");
      break;
    case Script::OL_CHIKI:
      aFontList.AppendElement("Noto Sans Ol Chiki");
      break;
    case Script::REJANG:
      aFontList.AppendElement("Noto Sans Rejang");
      break;
    case Script::SAURASHTRA:
      aFontList.AppendElement("Noto Sans Saurashtra");
      break;
    case Script::SUNDANESE:
      aFontList.AppendElement("Noto Sans Sundanese");
      break;
    case Script::MEETEI_MAYEK:
      aFontList.AppendElement("Noto Sans Meetei Mayek");
      break;
    case Script::IMPERIAL_ARAMAIC:
      aFontList.AppendElement("Noto Sans Imperial Aramaic");
      break;
    case Script::AVESTAN:
      aFontList.AppendElement("Noto Sans Avestan");
      break;
    case Script::CHAKMA:
      aFontList.AppendElement("Noto Sans Chakma");
      break;
    case Script::KAITHI:
      aFontList.AppendElement("Noto Sans Kaithi");
      break;
    case Script::MANICHAEAN:
      aFontList.AppendElement("Noto Sans Manichaean");
      break;
    case Script::INSCRIPTIONAL_PAHLAVI:
      aFontList.AppendElement("Noto Sans Inscriptional Pahlavi");
      break;
    case Script::PSALTER_PAHLAVI:
      aFontList.AppendElement("Noto Sans Psalter Pahlavi");
      break;
    case Script::INSCRIPTIONAL_PARTHIAN:
      aFontList.AppendElement("Noto Sans Inscriptional Parthian");
      break;
    case Script::SAMARITAN:
      aFontList.AppendElement("Noto Sans Samaritan");
      break;
    case Script::TAI_VIET:
      aFontList.AppendElement("Noto Sans Tai Viet");
      break;
    case Script::BAMUM:
      aFontList.AppendElement("Noto Sans Bamum");
      break;
    case Script::LISU:
      aFontList.AppendElement("Noto Sans Lisu");
      break;
    case Script::OLD_SOUTH_ARABIAN:
      aFontList.AppendElement("Noto Sans Old South Arabian");
      break;
    case Script::BASSA_VAH:
      aFontList.AppendElement("Noto Sans Bassa Vah");
      break;
    case Script::DUPLOYAN:
      aFontList.AppendElement("Noto Sans Duployan");
      break;
    case Script::ELBASAN:
      aFontList.AppendElement("Noto Sans Elbasan");
      break;
    case Script::GRANTHA:
      aFontList.AppendElement("Noto Sans Grantha");
      break;
    case Script::MENDE_KIKAKUI:
      aFontList.AppendElement("Noto Sans Mende Kikakui");
      break;
    case Script::MEROITIC_CURSIVE:
    case Script::MEROITIC_HIEROGLYPHS:
      aFontList.AppendElement("Noto Sans Meroitic");
      break;
    case Script::OLD_NORTH_ARABIAN:
      aFontList.AppendElement("Noto Sans Old North Arabian");
      break;
    case Script::NABATAEAN:
      aFontList.AppendElement("Noto Sans Nabataean");
      break;
    case Script::PALMYRENE:
      aFontList.AppendElement("Noto Sans Palmyrene");
      break;
    case Script::KHUDAWADI:
      aFontList.AppendElement("Noto Sans Khudawadi");
      break;
    case Script::WARANG_CITI:
      aFontList.AppendElement("Noto Sans Warang Citi");
      break;
    case Script::MRO:
      aFontList.AppendElement("Noto Sans Mro");
      break;
    case Script::SHARADA:
      aFontList.AppendElement("Noto Sans Sharada");
      break;
    case Script::SORA_SOMPENG:
      aFontList.AppendElement("Noto Sans Sora Sompeng");
      break;
    case Script::TAKRI:
      aFontList.AppendElement("Noto Sans Takri");
      break;
    case Script::KHOJKI:
      aFontList.AppendElement("Noto Sans Khojki");
      break;
    case Script::TIRHUTA:
      aFontList.AppendElement("Noto Sans Tirhuta");
      break;
    case Script::CAUCASIAN_ALBANIAN:
      aFontList.AppendElement("Noto Sans Caucasian Albanian");
      break;
    case Script::MAHAJANI:
      aFontList.AppendElement("Noto Sans Mahajani");
      break;
    case Script::AHOM:
      aFontList.AppendElement("Noto Serif Ahom");
      break;
    case Script::HATRAN:
      aFontList.AppendElement("Noto Sans Hatran");
      break;
    case Script::MODI:
      aFontList.AppendElement("Noto Sans Modi");
      break;
    case Script::MULTANI:
      aFontList.AppendElement("Noto Sans Multani");
      break;
    case Script::PAU_CIN_HAU:
      aFontList.AppendElement("Noto Sans Pau Cin Hau");
      break;
    case Script::SIDDHAM:
      aFontList.AppendElement("Noto Sans Siddham");
      break;
    case Script::ADLAM:
      aFontList.AppendElement("Noto Sans Adlam");
      break;
    case Script::BHAIKSUKI:
      aFontList.AppendElement("Noto Sans Bhaiksuki");
      break;
    case Script::MARCHEN:
      aFontList.AppendElement("Noto Sans Marchen");
      break;
    case Script::NEWA:
      aFontList.AppendElement("Noto Sans Newa");
      break;
    case Script::OSAGE:
      aFontList.AppendElement("Noto Sans Osage");
      break;
    case Script::HANIFI_ROHINGYA:
      aFontList.AppendElement("Noto Sans Hanifi Rohingya");
      break;
    case Script::WANCHO:
      aFontList.AppendElement("Noto Sans Wancho");
      break;

    // Script codes for which no commonly-installed font is currently known.
    // Probably future macOS versions will add Noto fonts for many of these,
    // so we should watch for updates.
    case Script::OLD_CHURCH_SLAVONIC_CYRILLIC:
    case Script::DEMOTIC_EGYPTIAN:
    case Script::HIERATIC_EGYPTIAN:
    case Script::BLISSYMBOLS:
    case Script::CIRTH:
    case Script::KHUTSURI:
    case Script::HARAPPAN_INDUS:
    case Script::LATIN_FRAKTUR:
    case Script::LATIN_GAELIC:
    case Script::MAYAN_HIEROGLYPHS:
    case Script::RONGORONGO:
    case Script::SARATI:
    case Script::ESTRANGELO_SYRIAC:
    case Script::WESTERN_SYRIAC:
    case Script::EASTERN_SYRIAC:
    case Script::TENGWAR:
    case Script::VISIBLE_SPEECH:
    case Script::UNWRITTEN_LANGUAGES:
    case Script::UNKNOWN:
    case Script::SIGNWRITING:
    case Script::MOON:
    case Script::BOOK_PAHLAVI:
    case Script::NAKHI_GEBA:
    case Script::KPELLE:
    case Script::LOMA:
    case Script::AFAKA:
    case Script::JURCHEN:
    case Script::NUSHU:
    case Script::TANGUT:
    case Script::WOLEAI:
    case Script::ANATOLIAN_HIEROGLYPHS:
    case Script::MASARAM_GONDI:
    case Script::SOYOMBO:
    case Script::ZANABAZAR_SQUARE:
    case Script::DOGRA:
    case Script::GUNJALA_GONDI:
    case Script::MAKASAR:
    case Script::MEDEFAIDRIN:
    case Script::SOGDIAN:
    case Script::OLD_SOGDIAN:
    case Script::ELYMAIC:
    case Script::NYIAKENG_PUACHUE_HMONG:
    case Script::NANDINAGARI:
    case Script::CHORASMIAN:
    case Script::DIVES_AKURU:
    case Script::KHITAN_SMALL_SCRIPT:
    case Script::YEZIDI:
      break;
  }

  // Symbols/dingbats are generally Script=COMMON but may be resolved to any
  // surrounding script run. So we'll always append a couple of likely fonts
  // for such characters.
  const uint32_t b = aCh >> 8;
  if (aRunScript == Script::COMMON ||  // Stray COMMON chars not resolved
      (b >= 0x20 && b <= 0x2b) || b == 0x2e ||  // BMP symbols/punctuation/etc
      GetGenCategory(aCh) == nsUGenCategory::kSymbol ||
      GetGenCategory(aCh) == nsUGenCategory::kPunctuation) {
    if (b == 0x27) {
      aFontList.AppendElement("Zapf Dingbats");
    }
    aFontList.AppendElement("Geneva");
    aFontList.AppendElement("STIXGeneral");
    aFontList.AppendElement("Apple Symbols");
    // Japanese fonts also cover a lot of miscellaneous symbols
    aFontList.AppendElement("Hiragino Sans");
    aFontList.AppendElement("Hiragino Kaku Gothic ProN");
  }

  // Arial Unicode MS has lots of glyphs for obscure characters; try it as a
  // last resort.
  aFontList.AppendElement("Arial Unicode MS");
}

/*static*/
void gfxPlatformMac::LookupSystemFont(
    mozilla::LookAndFeel::FontID aSystemFontID, nsACString& aSystemFontName,
    gfxFontStyle& aFontStyle) {
  gfxMacPlatformFontList* pfl = gfxMacPlatformFontList::PlatformFontList();
  return pfl->LookupSystemFont(aSystemFontID, aSystemFontName, aFontStyle);
}

uint32_t gfxPlatformMac::ReadAntiAliasingThreshold() {
  uint32_t threshold = 0;  // default == no threshold

  // first read prefs flag to determine whether to use the setting or not
  bool useAntiAliasingThreshold =
      Preferences::GetBool("gfx.use_text_smoothing_setting", false);

  // if the pref setting is disabled, return 0 which effectively disables this
  // feature
  if (!useAntiAliasingThreshold) return threshold;

  // value set via Appearance pref panel, "Turn off text smoothing for font
  // sizes xxx and smaller"
  CFNumberRef prefValue = (CFNumberRef)CFPreferencesCopyAppValue(
      CFSTR("AppleAntiAliasingThreshold"), kCFPreferencesCurrentApplication);

  if (prefValue) {
    if (!CFNumberGetValue(prefValue, kCFNumberIntType, &threshold)) {
      threshold = 0;
    }
    CFRelease(prefValue);
  }

  return threshold;
}

bool gfxPlatformMac::AccelerateLayersByDefault() { return true; }

// This is the renderer output callback function, called on the vsync thread
static CVReturn VsyncCallback(CVDisplayLinkRef aDisplayLink,
                              const CVTimeStamp* aNow,
                              const CVTimeStamp* aOutputTime,
                              CVOptionFlags aFlagsIn, CVOptionFlags* aFlagsOut,
                              void* aDisplayLinkContext);

class OSXVsyncSource final : public VsyncSource {
 public:
  OSXVsyncSource() : mGlobalDisplay(new OSXDisplay()) {}

  Display& GetGlobalDisplay() override { return *mGlobalDisplay; }

  class OSXDisplay final : public VsyncSource::Display {
   public:
    OSXDisplay()
        : mDisplayLink(nullptr, "OSXVsyncSource::OSXDisplay::mDisplayLink") {
      MOZ_ASSERT(NS_IsMainThread());
      mTimer = NS_NewTimer();
      CGDisplayRegisterReconfigurationCallback(DisplayReconfigurationCallback,
                                               this);
    }

    virtual ~OSXDisplay() {
      MOZ_ASSERT(NS_IsMainThread());
      CGDisplayRemoveReconfigurationCallback(DisplayReconfigurationCallback,
                                             this);
    }

    static void RetryEnableVsync(nsITimer* aTimer, void* aOsxDisplay) {
      MOZ_ASSERT(NS_IsMainThread());
      OSXDisplay* osxDisplay = static_cast<OSXDisplay*>(aOsxDisplay);
      MOZ_ASSERT(osxDisplay);
      osxDisplay->EnableVsync();
    }

    void EnableVsync() override {
      MOZ_ASSERT(NS_IsMainThread());
      if (IsVsyncEnabled()) {
        return;
      }

      auto displayLink = mDisplayLink.Lock();

      // Create a display link capable of being used with all active displays
      // TODO: See if we need to create an active DisplayLink for each monitor
      // in multi-monitor situations. According to the docs, it is compatible
      // with all displays running on the computer But if we have different
      // monitors at different display rates, we may hit issues.
      CVReturn retval = CVDisplayLinkCreateWithActiveCGDisplays(&*displayLink);

      // Workaround for bug 1201401: CVDisplayLinkCreateWithCGDisplays()
      // (called by CVDisplayLinkCreateWithActiveCGDisplays()) sometimes
      // creates a CVDisplayLinkRef with an uninitialized (nulled) internal
      // pointer. If we continue to use this CVDisplayLinkRef, we will
      // eventually crash in CVCGDisplayLink::getDisplayTimes(), where the
      // internal pointer is dereferenced. Fortunately, when this happens
      // another internal variable is also left uninitialized (zeroed),
      // which is accessible via CVDisplayLinkGetCurrentCGDisplay(). In
      // normal conditions the current display is never zero.
      if ((retval == kCVReturnSuccess) &&
          (CVDisplayLinkGetCurrentCGDisplay(*displayLink) == 0)) {
        retval = kCVReturnInvalidDisplay;
      }

      if (retval != kCVReturnSuccess) {
        NS_WARNING(
            "Could not create a display link with all active displays. "
            "Retrying");
        CVDisplayLinkRelease(*displayLink);
        *displayLink = nullptr;

        // bug 1142708 - When coming back from sleep,
        // or when changing displays, active displays may not be ready yet,
        // even if listening for the kIOMessageSystemHasPoweredOn event
        // from OS X sleep notifications.
        // Active displays are those that are drawable.
        // bug 1144638 - When changing display configurations and getting
        // notifications from CGDisplayReconfigurationCallBack, the
        // callback gets called twice for each active display
        // so it's difficult to know when all displays are active.
        // Instead, try again soon. The delay is arbitrary. 100ms chosen
        // because on a late 2013 15" retina, it takes about that
        // long to come back up from sleep.
        uint32_t delay = 100;
        mTimer->InitWithNamedFuncCallback(RetryEnableVsync, this, delay,
                                          nsITimer::TYPE_ONE_SHOT,
                                          "RetryEnableVsync");
        return;
      }

      if (CVDisplayLinkSetOutputCallback(*displayLink, &VsyncCallback, this) !=
          kCVReturnSuccess) {
        NS_WARNING("Could not set displaylink output callback");
        CVDisplayLinkRelease(*displayLink);
        *displayLink = nullptr;
        return;
      }

      mPreviousTimestamp = TimeStamp::Now();
      if (CVDisplayLinkStart(*displayLink) != kCVReturnSuccess) {
        NS_WARNING("Could not activate the display link");
        CVDisplayLinkRelease(*displayLink);
        *displayLink = nullptr;
      }

      CVTime vsyncRate =
          CVDisplayLinkGetNominalOutputVideoRefreshPeriod(*displayLink);
      if (vsyncRate.flags & kCVTimeIsIndefinite) {
        NS_WARNING("Could not get vsync rate, setting to 60.");
        mVsyncRate = TimeDuration::FromMilliseconds(1000.0 / 60.0);
      } else {
        int64_t timeValue = vsyncRate.timeValue;
        int64_t timeScale = vsyncRate.timeScale;
        const int milliseconds = 1000;
        float rateInMs = ((double)timeValue / (double)timeScale) * milliseconds;
        mVsyncRate = TimeDuration::FromMilliseconds(rateInMs);
      }
    }

    void DisableVsync() override {
      MOZ_ASSERT(NS_IsMainThread());
      if (!IsVsyncEnabled()) {
        return;
      }

      // Release the display link
      auto displayLink = mDisplayLink.Lock();
      if (*displayLink) {
        CVDisplayLinkRelease(*displayLink);
        *displayLink = nullptr;
      }
    }

    bool IsVsyncEnabled() override {
      MOZ_ASSERT(NS_IsMainThread());
      auto displayLink = mDisplayLink.Lock();
      return *displayLink != nullptr;
    }

    TimeDuration GetVsyncRate() override { return mVsyncRate; }

    void Shutdown() override {
      MOZ_ASSERT(NS_IsMainThread());
      mTimer->Cancel();
      mTimer = nullptr;
      DisableVsync();
    }

    // The vsync timestamps given by the CVDisplayLinkCallback are
    // in the future for the NEXT frame. Large parts of Gecko, such
    // as animations assume a timestamp at either now or in the past.
    // Normalize the timestamps given to the VsyncDispatchers to the vsync
    // that just occured, not the vsync that is upcoming.
    TimeStamp mPreviousTimestamp;

   private:
    static void DisplayReconfigurationCallback(
        CGDirectDisplayID aDisplay, CGDisplayChangeSummaryFlags aFlags,
        void* aUserInfo) {
      static_cast<OSXDisplay*>(aUserInfo)->OnDisplayReconfiguration(aDisplay,
                                                                    aFlags);
    }

    void OnDisplayReconfiguration(CGDirectDisplayID aDisplay,
                                  CGDisplayChangeSummaryFlags aFlags) {
      // Display reconfiguration notifications are fired in two phases: Before
      // the reconfiguration and after the reconfiguration.
      // All displays are notified before (with a "BeginConfiguration" flag),
      // and the reconfigured displays are notified again after the
      // configuration.
      if (aFlags & kCGDisplayBeginConfigurationFlag) {
        // We're only interested in the "after" notification, for the display
        // link's current display.
        return;
      }

      if (!NS_IsMainThread()) {
        return;
      }

      bool didReconfigureCurrentDisplayLinkDisplay = false;
      {  // scope for lock
        auto displayLink = mDisplayLink.Lock();
        didReconfigureCurrentDisplayLinkDisplay =
            *displayLink &&
            CVDisplayLinkGetCurrentCGDisplay(*displayLink) == aDisplay;
      }

      if (didReconfigureCurrentDisplayLinkDisplay) {
        // The link's current display has been reconfigured.
        // Recreate the display link, because otherwise it may be stuck with a
        // "removed" display forever and never notify us again.
        DisableVsync();
        EnableVsync();
      }
    }

    // Accessed from main thread and from display reconfiguration callback
    // thread... which also happens to be the main thread.
    DataMutex<CVDisplayLinkRef> mDisplayLink;

    // Accessed only from the main thread.
    RefPtr<nsITimer> mTimer;
    TimeDuration mVsyncRate;
  };  // OSXDisplay

 private:
  virtual ~OSXVsyncSource() = default;

  RefPtr<OSXDisplay> mGlobalDisplay;
};  // OSXVsyncSource

static CVReturn VsyncCallback(CVDisplayLinkRef aDisplayLink,
                              const CVTimeStamp* aNow,
                              const CVTimeStamp* aOutputTime,
                              CVOptionFlags aFlagsIn, CVOptionFlags* aFlagsOut,
                              void* aDisplayLinkContext) {
  // Executed on OS X hardware vsync thread
  OSXVsyncSource::OSXDisplay* display =
      (OSXVsyncSource::OSXDisplay*)aDisplayLinkContext;

  mozilla::TimeStamp outputTime =
      mozilla::TimeStamp::FromSystemTime(aOutputTime->hostTime);
  mozilla::TimeStamp nextVsync = outputTime;
  mozilla::TimeStamp previousVsync = display->mPreviousTimestamp;
  mozilla::TimeStamp now = TimeStamp::Now();

  // Snow leopard sometimes sends vsync timestamps very far in the past.
  // Normalize the vsync timestamps to now.
  if (nextVsync <= previousVsync) {
    nextVsync = now;
    previousVsync = now;
  } else if (now < previousVsync) {
    // Bug 1158321 - The VsyncCallback can sometimes execute before the reported
    // vsync time. In those cases, normalize the timestamp to Now() as sending
    // timestamps in the future has undefined behavior. See the comment above
    // OSXDisplay::mPreviousTimestamp
    previousVsync = now;
  }

  display->mPreviousTimestamp = nextVsync;

  display->NotifyVsync(previousVsync, outputTime);
  return kCVReturnSuccess;
}

already_AddRefed<mozilla::gfx::VsyncSource>
gfxPlatformMac::CreateHardwareVsyncSource() {
  RefPtr<VsyncSource> osxVsyncSource = new OSXVsyncSource();
  VsyncSource::Display& primaryDisplay = osxVsyncSource->GetGlobalDisplay();
  primaryDisplay.EnableVsync();
  if (!primaryDisplay.IsVsyncEnabled()) {
    NS_WARNING(
        "OS X Vsync source not enabled. Falling back to software vsync.");
    return gfxPlatform::CreateHardwareVsyncSource();
  }

  primaryDisplay.DisableVsync();
  return osxVsyncSource.forget();
}

nsTArray<uint8_t> gfxPlatformMac::GetPlatformCMSOutputProfileData() {
  nsTArray<uint8_t> prefProfileData = GetPrefCMSOutputProfileData();
  if (!prefProfileData.IsEmpty()) {
    return prefProfileData;
  }

  CGColorSpaceRef cspace = ::CGDisplayCopyColorSpace(::CGMainDisplayID());
  if (!cspace) {
    cspace = ::CGColorSpaceCreateDeviceRGB();
  }
  if (!cspace) {
    return nsTArray<uint8_t>();
  }

  CFDataRef iccp = ::CGColorSpaceCopyICCProfile(cspace);

  ::CFRelease(cspace);

  if (!iccp) {
    return nsTArray<uint8_t>();
  }

  // copy to external buffer
  size_t size = static_cast<size_t>(::CFDataGetLength(iccp));

  nsTArray<uint8_t> result;

  if (size > 0) {
    result.AppendElements(::CFDataGetBytePtr(iccp), size);
  }

  ::CFRelease(iccp);

  return result;
}

bool gfxPlatformMac::CheckVariationFontSupport() {
  // We don't allow variation fonts to be enabled before 10.13,
  // as although the Core Text APIs existed, they are known to be
  // fairly buggy.
  // (Note that Safari also requires 10.13 for variation-font support.)
  return nsCocoaFeatures::OnHighSierraOrLater();
}

void gfxPlatformMac::InitPlatformGPUProcessPrefs() {
  FeatureState& gpuProc = gfxConfig::GetFeature(Feature::GPU_PROCESS);
  gpuProc.ForceDisable(FeatureStatus::Blocked,
                       "GPU process does not work on Mac",
                       "FEATURE_FAILURE_MAC_GPU_PROC"_ns);
}
