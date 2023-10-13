/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Logging.h"

#include <algorithm>

#import <AppKit/AppKit.h>

#include "gfxFontConstants.h"
#include "gfxPlatformMac.h"
#include "gfxMacPlatformFontList.h"
#include "gfxMacFont.h"
#include "gfxUserFontSet.h"
#include "SharedFontList-impl.h"

#include "harfbuzz/hb.h"

#include "AppleUtils.h"
#include "MainThreadUtils.h"
#include "nsDirectoryServiceUtils.h"
#include "nsDirectoryServiceDefs.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsIDirectoryEnumerator.h"
#include "nsCharTraits.h"
#include "nsCocoaUtils.h"
#include "nsComponentManagerUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsTArray.h"

#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/FontPropertyTypes.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/Preferences.h"
#include "mozilla/ProfilerLabels.h"
#include "mozilla/Sprintf.h"
#include "mozilla/StaticPrefs_gfx.h"
#include "mozilla/Telemetry.h"
#include "mozilla/gfx/2D.h"

#include <unistd.h>
#include <time.h>
#include <dlfcn.h>

#include "StandardFonts-macos.inc"

using namespace mozilla;
using namespace mozilla::gfx;

static const char kSystemFont_system[] = "-apple-system";

// cache Cocoa's "shared font manager" for performance
static NSFontManager* sFontManager;

static void GetStringForNSString(const NSString* aSrc, nsAString& aDest) {
  aDest.SetLength([aSrc length]);
  [aSrc getCharacters:reinterpret_cast<unichar*>(aDest.BeginWriting())
                range:NSMakeRange(0, [aSrc length])];
}

static NSString* GetNSStringForString(const nsAString& aSrc) {
  return [NSString
      stringWithCharacters:reinterpret_cast<const unichar*>(aSrc.BeginReading())
                    length:aSrc.Length()];
}

#define LOG_FONTLIST(args) \
  MOZ_LOG(gfxPlatform::GetLog(eGfxLog_fontlist), mozilla::LogLevel::Debug, args)
#define LOG_FONTLIST_ENABLED() \
  MOZ_LOG_TEST(gfxPlatform::GetLog(eGfxLog_fontlist), mozilla::LogLevel::Debug)
#define LOG_CMAPDATA_ENABLED() \
  MOZ_LOG_TEST(gfxPlatform::GetLog(eGfxLog_cmapdata), mozilla::LogLevel::Debug)

class gfxMacFontFamily final : public CTFontFamily {
 public:
  gfxMacFontFamily(const nsACString& aName, NSFont* aSystemFont)
      : CTFontFamily(aName, FontVisibility::Unknown),
        mForSystemFont(aSystemFont) {
    // I don't think the system font instance is at much risk of being deleted,
    // but to be on the safe side let's retain a reference until we're finished
    // using it for lazy initialization.
    [mForSystemFont retain];
  }

  void FindStyleVariationsLocked(FontInfoData* aFontInfoData = nullptr)
      MOZ_REQUIRES(mLock) override;

 protected:
  // If non-null, this is a family representing the system UI font, and should
  // use the given NSFont as the basis for initialization as the normal
  // font-manager APIs based on family name won't handle it.
  NSFont* mForSystemFont = nullptr;
};

void gfxMacFontFamily::FindStyleVariationsLocked(FontInfoData* aFontInfoData) {
  if (mHasStyles) {
    return;
  }

  AUTO_PROFILER_LABEL_DYNAMIC_NSCSTRING("gfxMacFontFamily::FindStyleVariations",
                                        LAYOUT, mName);

  nsAutoreleasePool localPool;

  if (mForSystemFont) {
    MOZ_ASSERT(gfxPlatform::HasVariationFontSupport());

    auto addToFamily = [&](NSFont* aNSFont) MOZ_REQUIRES(mLock) {
      NSString* psNameNS = [[aNSFont fontDescriptor] postscriptName];
      nsAutoString nameUTF16;
      nsAutoCString psName;
      nsCocoaUtils::GetStringForNSString(psNameNS, nameUTF16);
      CopyUTF16toUTF8(nameUTF16, psName);

      auto* fe =
          new CTFontEntry(psName, WeightRange(FontWeight::NORMAL), true, 0.0);

      // Set the appropriate style, assuming it may not have a variation range.
      fe->mStyleRange = SlantStyleRange(
          ([[aNSFont fontDescriptor] symbolicTraits] & NSFontItalicTrait)
              ? FontSlantStyle::ITALIC
              : FontSlantStyle::NORMAL);

      // Set up weight (and width, if present) ranges.
      fe->SetupVariationRanges();
      AddFontEntryLocked(fe);
    };

    addToFamily(mForSystemFont);

    // See if there is a corresponding italic face, and add it to the family.
    NSFont* italicFont = [sFontManager convertFont:mForSystemFont
                                       toHaveTrait:NSItalicFontMask];
    if (italicFont != mForSystemFont) {
      addToFamily(italicFont);
    }

    [mForSystemFont release];
    mForSystemFont = nullptr;
    SetHasStyles(true);

    return;
  }

  CTFontFamily::FindStyleVariationsLocked(aFontInfoData);
}

/* gfxSingleFaceMacFontFamily */

class gfxSingleFaceMacFontFamily final : public gfxFontFamily {
 public:
  gfxSingleFaceMacFontFamily(const nsACString& aName,
                             FontVisibility aVisibility)
      : gfxFontFamily(aName, aVisibility) {
    mFaceNamesInitialized = true;  // omit from face name lists
  }

  virtual ~gfxSingleFaceMacFontFamily() = default;

  void FindStyleVariationsLocked(FontInfoData* aFontInfoData = nullptr)
      MOZ_REQUIRES(mLock) override{};

  void LocalizedName(nsACString& aLocalizedName) override;

  void ReadOtherFamilyNames(gfxPlatformFontList* aPlatformFontList) override;

  bool IsSingleFaceFamily() const override { return true; }
};

void gfxSingleFaceMacFontFamily::LocalizedName(nsACString& aLocalizedName) {
  nsAutoreleasePool localPool;

  AutoReadLock lock(mLock);

  if (!HasOtherFamilyNames()) {
    aLocalizedName = mName;
    return;
  }

  gfxFontEntry* fe = mAvailableFonts[0];
  NSFont* font = [NSFont
      fontWithName:GetNSStringForString(NS_ConvertUTF8toUTF16(fe->Name()))
              size:0.0];
  if (font) {
    NSString* localized = [font displayName];
    if (localized) {
      nsAutoString locName;
      GetStringForNSString(localized, locName);
      CopyUTF16toUTF8(locName, aLocalizedName);
      return;
    }
  }

  // failed to get localized name, just use the canonical one
  aLocalizedName = mName;
}

void gfxSingleFaceMacFontFamily::ReadOtherFamilyNames(
    gfxPlatformFontList* aPlatformFontList) {
  AutoWriteLock lock(mLock);
  if (mOtherFamilyNamesInitialized) {
    return;
  }

  gfxFontEntry* fe = mAvailableFonts[0];
  if (!fe) {
    return;
  }

  const uint32_t kNAME = TRUETYPE_TAG('n', 'a', 'm', 'e');

  gfxFontEntry::AutoTable nameTable(fe, kNAME);
  if (!nameTable) {
    return;
  }

  mHasOtherFamilyNames =
      ReadOtherFamilyNamesForFace(aPlatformFontList, nameTable, true);

  mOtherFamilyNamesInitialized = true;
}

/* gfxMacPlatformFontList */
#pragma mark -

gfxMacPlatformFontList::gfxMacPlatformFontList() : CoreTextFontList() {
  CheckFamilyList(kBaseFonts);

  // cache this in a static variable so that gfxMacFontFamily objects
  // don't have to repeatedly look it up
  sFontManager = [NSFontManager sharedFontManager];

  // Load the font-list preferences now, so that we don't have to do it from
  // Init[Shared]FontListForPlatform, which may be called off-main-thread.
  gfxFontUtils::GetPrefsFontList("font.single-face-list", mSingleFaceFonts);
}

FontVisibility gfxMacPlatformFontList::GetVisibilityForFamily(
    const nsACString& aName) const {
  if (aName[0] == '.' || aName.LowerCaseEqualsLiteral("lastresort")) {
    return FontVisibility::Hidden;
  }
  if (FamilyInList(aName, kBaseFonts)) {
    return FontVisibility::Base;
  }
#ifdef MOZ_BUNDLED_FONTS
  if (mBundledFamilies.Contains(aName)) {
    return FontVisibility::Base;
  }
#endif
  return FontVisibility::User;
}

bool gfxMacPlatformFontList::DeprecatedFamilyIsAvailable(
    const nsACString& aName) {
  NSString* family = GetNSStringForString(NS_ConvertUTF8toUTF16(aName));
  return [[sFontManager availableMembersOfFontFamily:family] count] > 0;
}

void gfxMacPlatformFontList::InitAliasesForSingleFaceList() {
  for (const auto& familyName : mSingleFaceFonts) {
    LOG_FONTLIST(("(fontlist-singleface) face name: %s\n", familyName.get()));
    // Each entry in the "single face families" list is expected to be a
    // colon-separated pair of FaceName:Family,
    // where FaceName is the individual face name (psname) of a font
    // that should be exposed as a separate family name,
    // and Family is the standard family to which that face belongs.
    // The only such face listed by default is
    //    Osaka-Mono:Osaka
    auto colon = familyName.FindChar(':');
    if (colon == kNotFound) {
      continue;
    }

    // Look for the parent family in the main font family list,
    // and ensure we have loaded its list of available faces.
    nsAutoCString key;
    GenerateFontListKey(Substring(familyName, colon + 1), key);
    fontlist::Family* family = SharedFontList()->FindFamily(key);
    if (!family) {
      // The parent family is not present, so just ignore this entry.
      continue;
    }
    if (!family->IsInitialized()) {
      if (!gfxPlatformFontList::InitializeFamily(family)) {
        // This shouldn't ever fail, but if it does, we can safely ignore it.
        MOZ_ASSERT(false, "failed to initialize font family");
        continue;
      }
    }

    // Truncate the entry from prefs at the colon, so now it is just the
    // desired single-face-family name.
    nsAutoCString aliasName(Substring(familyName, 0, colon));

    // Look through the family's faces to see if this one is present.
    fontlist::FontList* list = SharedFontList();
    const fontlist::Pointer* facePtrs = family->Faces(list);
    for (size_t i = 0; i < family->NumFaces(); i++) {
      if (facePtrs[i].IsNull()) {
        continue;
      }
      auto* face = facePtrs[i].ToPtr<const fontlist::Face>(list);
      if (face->mDescriptor.AsString(list).Equals(aliasName)) {
        // Found it! Create an entry in the Alias table.
        GenerateFontListKey(aliasName, key);
        if (SharedFontList()->FindFamily(key) || mAliasTable.Get(key)) {
          // If the family name is already known, something's misconfigured;
          // just ignore it.
          MOZ_ASSERT(false, "single-face family already known");
          break;
        }
        auto aliasData = mAliasTable.GetOrInsertNew(key);
        // The "alias" here isn't based on an existing family, so we don't call
        // aliasData->InitFromFamily(); the various flags are left as defaults.
        aliasData->mFaces.AppendElement(facePtrs[i]);
        aliasData->mBaseFamily = aliasName;
        aliasData->mVisibility = family->Visibility();
        break;
      }
    }
  }
  if (!mAliasTable.IsEmpty()) {
    // This will be updated when the font loader completes, but we require
    // at least the Osaka-Mono alias to be available immediately.
    SharedFontList()->SetAliases(mAliasTable);
  }
}

void gfxMacPlatformFontList::InitSingleFaceList() {
  for (const auto& familyName : mSingleFaceFonts) {
    LOG_FONTLIST(("(fontlist-singleface) face name: %s\n", familyName.get()));
    // Each entry in the "single face families" list is expected to be a
    // colon-separated pair of FaceName:Family,
    // where FaceName is the individual face name (psname) of a font
    // that should be exposed as a separate family name,
    // and Family is the standard family to which that face belongs.
    // The only such face listed by default is
    //    Osaka-Mono:Osaka
    auto colon = familyName.FindChar(':');
    if (colon == kNotFound) {
      continue;
    }

    // Look for the parent family in the main font family list,
    // and ensure we have loaded its list of available faces.
    nsAutoCString key(Substring(familyName, colon + 1));
    ToLowerCase(key);
    gfxFontFamily* family = mFontFamilies.GetWeak(key);
    if (!family || family->IsHidden()) {
      continue;
    }
    family->FindStyleVariations();

    // Truncate the entry from prefs at the colon, so now it is just the
    // desired single-face-family name.
    nsAutoCString aliasName(Substring(familyName, 0, colon));

    // Look through the family's faces to see if this one is present.
    const gfxFontEntry* fe = nullptr;
    family->ReadLock();
    for (const auto& face : family->GetFontList()) {
      if (face->Name().Equals(aliasName)) {
        fe = face;
        break;
      }
    }
    family->ReadUnlock();
    if (!fe) {
      continue;
    }

    // We found the correct face, so create the single-face family record.
    GenerateFontListKey(aliasName, key);
    LOG_FONTLIST(("(fontlist-singleface) family name: %s, key: %s\n",
                  aliasName.get(), key.get()));

    // add only if doesn't exist already
    if (!mFontFamilies.GetWeak(key)) {
      RefPtr<gfxFontFamily> familyEntry =
          new gfxSingleFaceMacFontFamily(aliasName, family->Visibility());
      // We need a separate font entry, because its family name will
      // differ from the one we found in the main list.
      CTFontEntry* fontEntry =
          new CTFontEntry(fe->Name(), fe->Weight(), true,
                          static_cast<const CTFontEntry*>(fe)->mSizeHint);
      familyEntry->AddFontEntry(fontEntry);
      familyEntry->SetHasStyles(true);
      mFontFamilies.InsertOrUpdate(key, std::move(familyEntry));
      LOG_FONTLIST(("(fontlist-singleface) added new family: %s, key: %s\n",
                    aliasName.get(), key.get()));
    }
  }
}

// System fonts under OSX may contain weird "meta" names but if we create
// a new font using just the Postscript name, the NSFont api returns an object
// with the actual real family name. For example, under OSX 10.11:
//
// [[NSFont menuFontOfSize:8.0] familyName] ==> .AppleSystemUIFont
// [[NSFont fontWithName:[[[NSFont menuFontOfSize:8.0] fontDescriptor]
// postscriptName]
//          size:8.0] familyName] ==> .SF NS Text

static NSString* GetRealFamilyName(NSFont* aFont) {
  NSString* psName = [[aFont fontDescriptor] postscriptName];
  // With newer macOS versions and SDKs (e.g. when compiled against SDK 10.15),
  // [NSFont fontWithName:] fails for hidden system fonts, because the
  // underlying Core Text functions it uses reject such names and tell us to use
  // the special CTFontCreateUIFontForLanguage API instead. To work around this,
  // as we don't yet work directly with the CTFontUIFontType identifiers, we
  // create a Core Graphics font (as it doesn't reject system font names), and
  // use this to create a Core Text font that we can query for the family name.
  // Eventually we should move to using CTFontUIFontType constants to identify
  // system fonts, and eliminate the need to instantiate them (indirectly) from
  // their postscript names.
  AutoCFRelease<CGFontRef> cgFont =
      CGFontCreateWithFontName(CFStringRef(psName));
  if (!cgFont) {
    return [aFont familyName];
  }

  AutoCFRelease<CTFontRef> ctFont =
      CTFontCreateWithGraphicsFont(cgFont, 0.0, nullptr, nullptr);
  if (!ctFont) {
    return [aFont familyName];
  }
  NSString* familyName = (NSString*)CTFontCopyFamilyName(ctFont);

  return [familyName autorelease];
}

void gfxMacPlatformFontList::InitSystemFontNames() {
  // text font family
  NSFont* sys = [NSFont systemFontOfSize:0.0];
  NSString* textFamilyName = GetRealFamilyName(sys);
  nsAutoString familyName;
  nsCocoaUtils::GetStringForNSString(textFamilyName, familyName);
  CopyUTF16toUTF8(familyName, mSystemFontFamilyName);

  // We store an in-process gfxFontFamily for the system font even if using the
  // shared fontlist to manage "normal" fonts, because the hidden system fonts
  // may be excluded from the font list altogether. This family will be
  // populated based on the given NSFont.
  RefPtr<gfxFontFamily> fam = new gfxMacFontFamily(mSystemFontFamilyName, sys);
  if (fam) {
    nsAutoCString key;
    GenerateFontListKey(mSystemFontFamilyName, key);
    mFontFamilies.InsertOrUpdate(key, std::move(fam));
  }

#ifdef DEBUG
  // different system font API's always map to the same family under OSX, so
  // just assume that and emit a warning if that ever changes
  NSString* sysFamily = GetRealFamilyName([NSFont systemFontOfSize:0.0]);
  if ([sysFamily compare:GetRealFamilyName([NSFont
                             boldSystemFontOfSize:0.0])] != NSOrderedSame ||
      [sysFamily compare:GetRealFamilyName([NSFont
                             controlContentFontOfSize:0.0])] != NSOrderedSame ||
      [sysFamily compare:GetRealFamilyName([NSFont menuBarFontOfSize:0.0])] !=
          NSOrderedSame ||
      [sysFamily compare:GetRealFamilyName([NSFont toolTipsFontOfSize:0.0])] !=
          NSOrderedSame) {
    NS_WARNING("system font types map to different font families"
               " -- please log a bug!!");
  }
#endif
}

FontFamily gfxMacPlatformFontList::GetDefaultFontForPlatform(
    nsPresContext* aPresContext, const gfxFontStyle* aStyle,
    nsAtom* aLanguage) {
  nsAutoreleasePool localPool;

  NSString* defaultFamily = [[NSFont userFontOfSize:aStyle->size] familyName];
  nsAutoString familyName;

  GetStringForNSString(defaultFamily, familyName);
  return FindFamily(aPresContext, NS_ConvertUTF16toUTF8(familyName));
}

void gfxMacPlatformFontList::LookupSystemFont(LookAndFeel::FontID aSystemFontID,
                                              nsACString& aSystemFontName,
                                              gfxFontStyle& aFontStyle) {
  // Provide a local pool because we may be called from stylo threads.
  nsAutoreleasePool localPool;

  // code moved here from widget/cocoa/nsLookAndFeel.mm
  NSFont* font = nullptr;
  char* systemFontName = nullptr;
  switch (aSystemFontID) {
    case LookAndFeel::FontID::MessageBox:
    case LookAndFeel::FontID::StatusBar:
    case LookAndFeel::FontID::MozList:
    case LookAndFeel::FontID::MozField:
    case LookAndFeel::FontID::MozButton:
      font = [NSFont systemFontOfSize:[NSFont smallSystemFontSize]];
      systemFontName = (char*)kSystemFont_system;
      break;

    case LookAndFeel::FontID::SmallCaption:
      font = [NSFont boldSystemFontOfSize:[NSFont smallSystemFontSize]];
      systemFontName = (char*)kSystemFont_system;
      break;

    case LookAndFeel::FontID::Icon:  // used in urlbar; tried labelFont, but too
                                     // small
      font = [NSFont controlContentFontOfSize:0.0];
      systemFontName = (char*)kSystemFont_system;
      break;

    case LookAndFeel::FontID::MozPullDownMenu:
      font = [NSFont menuBarFontOfSize:0.0];
      systemFontName = (char*)kSystemFont_system;
      break;

    case LookAndFeel::FontID::Caption:
    case LookAndFeel::FontID::Menu:
    default:
      font = [NSFont systemFontOfSize:0.0];
      systemFontName = (char*)kSystemFont_system;
      break;
  }
  NS_ASSERTION(font, "system font not set");
  NS_ASSERTION(systemFontName, "system font name not set");

  if (systemFontName) {
    aSystemFontName.AssignASCII(systemFontName);
  }

  NSFontSymbolicTraits traits = [[font fontDescriptor] symbolicTraits];
  aFontStyle.style = (traits & NSFontItalicTrait) ? FontSlantStyle::ITALIC
                                                  : FontSlantStyle::NORMAL;
  aFontStyle.weight =
      (traits & NSFontBoldTrait) ? FontWeight::BOLD : FontWeight::NORMAL;
  aFontStyle.stretch = (traits & NSFontExpandedTrait) ? FontStretch::EXPANDED
                       : (traits & NSFontCondensedTrait)
                           ? FontStretch::CONDENSED
                           : FontStretch::NORMAL;
  aFontStyle.size = [font pointSize];
  aFontStyle.systemFont = true;
}
