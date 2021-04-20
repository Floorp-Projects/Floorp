/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFXFCPLATFORMFONTLIST_H_
#define GFXFCPLATFORMFONTLIST_H_

#include "gfxFT2FontBase.h"
#include "gfxPlatformFontList.h"
#include "mozilla/FontPropertyTypes.h"
#include "mozilla/mozalloc.h"
#include "mozilla/RefPtr.h"
#include "mozilla/UniquePtr.h"
#include "nsClassHashtable.h"
#include "nsTHashMap.h"

#include <fontconfig/fontconfig.h>
#include "ft2build.h"
#include FT_FREETYPE_H
#include FT_TRUETYPE_TABLES_H
#include FT_MULTIPLE_MASTERS_H

#if defined(MOZ_SANDBOX) && defined(XP_LINUX)
#  include "mozilla/SandboxBroker.h"
#endif

namespace mozilla {
namespace dom {
class SystemFontListEntry;
};

template <>
class RefPtrTraits<FcPattern> {
 public:
  static void Release(FcPattern* ptr) { FcPatternDestroy(ptr); }
  static void AddRef(FcPattern* ptr) { FcPatternReference(ptr); }
};

template <>
class RefPtrTraits<FcConfig> {
 public:
  static void Release(FcConfig* ptr) { FcConfigDestroy(ptr); }
  static void AddRef(FcConfig* ptr) { FcConfigReference(ptr); }
};

template <>
class DefaultDelete<FcFontSet> {
 public:
  void operator()(FcFontSet* aPtr) { FcFontSetDestroy(aPtr); }
};

template <>
class DefaultDelete<FcObjectSet> {
 public:
  void operator()(FcObjectSet* aPtr) { FcObjectSetDestroy(aPtr); }
};

};  // namespace mozilla

// The names for the font entry and font classes should really
// the common 'Fc' abbreviation but the gfxPangoFontGroup code already
// defines versions of these, so use the verbose name for now.

class gfxFontconfigFontEntry final : public gfxFT2FontEntryBase {
  friend class gfxFcPlatformFontList;

 public:
  // used for system fonts with explicit patterns
  explicit gfxFontconfigFontEntry(const nsACString& aFaceName,
                                  FcPattern* aFontPattern,
                                  bool aIgnoreFcCharmap);

  // used for data fonts where the fontentry takes ownership
  // of the font data and the FT_Face
  explicit gfxFontconfigFontEntry(const nsACString& aFaceName,
                                  WeightRange aWeight, StretchRange aStretch,
                                  SlantStyleRange aStyle,
                                  RefPtr<mozilla::gfx::SharedFTFace>&& aFace);

  // used for @font-face local system fonts with explicit patterns
  explicit gfxFontconfigFontEntry(const nsACString& aFaceName,
                                  FcPattern* aFontPattern, WeightRange aWeight,
                                  StretchRange aStretch,
                                  SlantStyleRange aStyle);

  gfxFontEntry* Clone() const override;

  FcPattern* GetPattern() { return mFontPattern; }

  nsresult ReadCMAP(FontInfoData* aFontInfoData = nullptr) override;
  bool TestCharacterMap(uint32_t aCh) override;

  const RefPtr<mozilla::gfx::SharedFTFace>& GetFTFace();
  FTUserFontData* GetUserFontData();

  FT_MM_Var* GetMMVar() override;

  bool HasVariations() override;
  void GetVariationAxes(nsTArray<gfxFontVariationAxis>& aAxes) override;
  void GetVariationInstances(
      nsTArray<gfxFontVariationInstance>& aInstances) override;

  bool HasFontTable(uint32_t aTableTag) override;
  nsresult CopyFontTable(uint32_t aTableTag, nsTArray<uint8_t>&) override;
  hb_blob_t* GetFontTable(uint32_t aTableTag) override;

  double GetAspect();

 protected:
  virtual ~gfxFontconfigFontEntry();

  gfxFont* CreateFontInstance(const gfxFontStyle* aFontStyle) override;

  // pattern for a single face of a family
  RefPtr<FcPattern> mFontPattern;

  // FTFace - initialized when needed
  RefPtr<mozilla::gfx::SharedFTFace> mFTFace;
  bool mFTFaceInitialized;

  // Whether TestCharacterMap should check the actual cmap rather than asking
  // fontconfig about character coverage.
  // We do this for app-bundled (rather than system) fonts, as they may
  // include color glyphs that fontconfig would overlook, and for fonts
  // loaded via @font-face.
  bool mIgnoreFcCharmap;

  // Whether the face supports variations. For system-installed fonts, we
  // query fontconfig for this (so they will only work if fontconfig is
  // recent enough to include support); for downloaded user-fonts we query
  // the FreeType face.
  bool mHasVariations;
  bool mHasVariationsInitialized;

  double mAspect;

  class UnscaledFontCache {
   public:
    already_AddRefed<mozilla::gfx::UnscaledFontFontconfig> Lookup(
        const std::string& aFile, uint32_t aIndex);

    void Add(
        const RefPtr<mozilla::gfx::UnscaledFontFontconfig>& aUnscaledFont) {
      mUnscaledFonts[kNumEntries - 1] = aUnscaledFont;
      MoveToFront(kNumEntries - 1);
    }

   private:
    void MoveToFront(size_t aIndex);

    static const size_t kNumEntries = 3;
    mozilla::ThreadSafeWeakPtr<mozilla::gfx::UnscaledFontFontconfig>
        mUnscaledFonts[kNumEntries];
  };

  UnscaledFontCache mUnscaledFontCache;

  // Because of FreeType bug 52955, we keep the FT_MM_Var struct when it is
  // first loaded, rather than releasing it and re-fetching it as needed.
  FT_MM_Var* mMMVar = nullptr;
  bool mMMVarInitialized = false;
};

class gfxFontconfigFontFamily final : public gfxFontFamily {
 public:
  gfxFontconfigFontFamily(const nsACString& aName, FontVisibility aVisibility)
      : gfxFontFamily(aName, aVisibility),
        mContainsAppFonts(false),
        mHasNonScalableFaces(false),
        mForceScalable(false) {}

  template <typename Func>
  void AddFacesToFontList(Func aAddPatternFunc);

  void FindStyleVariations(FontInfoData* aFontInfoData = nullptr) override;

  // Families are constructed initially with just references to patterns.
  // When necessary, these are enumerated within FindStyleVariations.
  void AddFontPattern(FcPattern* aFontPattern, bool aSingleName);

  void SetFamilyContainsAppFonts(bool aContainsAppFonts) {
    mContainsAppFonts = aContainsAppFonts;
  }

  void FindAllFontsForStyle(const gfxFontStyle& aFontStyle,
                            nsTArray<gfxFontEntry*>& aFontEntryList,
                            bool aIgnoreSizeTolerance) override;

  bool FilterForFontList(nsAtom* aLangGroup,
                         const nsACString& aGeneric) const final {
    return SupportsLangGroup(aLangGroup);
  }

 protected:
  virtual ~gfxFontconfigFontFamily();

  // helper for FilterForFontList
  bool SupportsLangGroup(nsAtom* aLangGroup) const;

  nsTArray<RefPtr<FcPattern>> mFontPatterns;

  // Number of faces that have a single name. Faces that have multiple names are
  // sorted last.
  uint32_t mUniqueNameFaceCount = 0;
  bool mContainsAppFonts : 1;
  bool mHasNonScalableFaces : 1;
  bool mForceScalable : 1;
};

class gfxFontconfigFont final : public gfxFT2FontBase {
 public:
  gfxFontconfigFont(
      const RefPtr<mozilla::gfx::UnscaledFontFontconfig>& aUnscaledFont,
      RefPtr<mozilla::gfx::SharedFTFace>&& aFTFace, FcPattern* aPattern,
      gfxFloat aAdjustedSize, gfxFontEntry* aFontEntry,
      const gfxFontStyle* aFontStyle, int aLoadFlags, bool aEmbolden);

  FontType GetType() const override { return FONT_TYPE_FONTCONFIG; }
  FcPattern* GetPattern() const { return mPattern; }

  already_AddRefed<mozilla::gfx::ScaledFont> GetScaledFont(
      DrawTarget* aTarget) override;

  bool ShouldHintMetrics() const override;

 private:
  virtual ~gfxFontconfigFont();

  RefPtr<FcPattern> mPattern;
};

class gfxFcPlatformFontList final : public gfxPlatformFontList {
  using FontPatternListEntry = mozilla::dom::SystemFontListEntry;

 public:
  gfxFcPlatformFontList();

  static gfxFcPlatformFontList* PlatformFontList() {
    return static_cast<gfxFcPlatformFontList*>(sPlatformFontList);
  }

  // initialize font lists
  nsresult InitFontListForPlatform() override;
  void InitSharedFontListForPlatform() override;

  void GetFontList(nsAtom* aLangGroup, const nsACString& aGenericFamily,
                   nsTArray<nsString>& aListOfFonts) override;

  void ReadSystemFontList(nsTArray<FontPatternListEntry>* retValue);

  gfxFontEntry* CreateFontEntry(
      mozilla::fontlist::Face* aFace,
      const mozilla::fontlist::Family* aFamily) override;

  gfxFontEntry* LookupLocalFont(const nsACString& aFontName,
                                WeightRange aWeightForEntry,
                                StretchRange aStretchForEntry,
                                SlantStyleRange aStyleForEntry) override;

  gfxFontEntry* MakePlatformFont(const nsACString& aFontName,
                                 WeightRange aWeightForEntry,
                                 StretchRange aStretchForEntry,
                                 SlantStyleRange aStyleForEntry,
                                 const uint8_t* aFontData,
                                 uint32_t aLength) override;

  bool FindAndAddFamilies(mozilla::StyleGenericFontFamily aGeneric,
                          const nsACString& aFamily,
                          nsTArray<FamilyAndGeneric>* aOutput,
                          FindFamiliesFlags aFlags,
                          gfxFontStyle* aStyle = nullptr,
                          nsAtom* aLanguage = nullptr,
                          gfxFloat aDevToCssSize = 1.0) override;

  bool GetStandardFamilyName(const nsCString& aFontName,
                             nsACString& aFamilyName) override;

  FcConfig* GetLastConfig() const { return mLastConfig; }

  // override to use fontconfig lookup for generics
  void AddGenericFonts(mozilla::StyleGenericFontFamily, nsAtom* aLanguage,
                       nsTArray<FamilyAndGeneric>& aFamilyList) override;

  void ClearLangGroupPrefFonts() override;

  // clear out cached generic-lang ==> family-list mappings
  void ClearGenericMappings() { mGenericMappings.Clear(); }

  // map lang group ==> lang string
  // When aForFontEnumerationThread is true, this method will avoid using
  // LanguageService::LookupLanguage, because it is not safe for off-main-
  // thread use (except by stylo traversal, which does the necessary locking)
  void GetSampleLangForGroup(nsAtom* aLanguage, nsACString& aLangStr,
                             bool aForFontEnumerationThread = false);

 protected:
  virtual ~gfxFcPlatformFontList();

#if defined(MOZ_SANDBOX) && defined(XP_LINUX)
  typedef mozilla::SandboxBroker::Policy SandboxPolicy;
#else
  // Dummy type just so we can still have a SandboxPolicy* parameter.
  struct SandboxPolicy {};
#endif

  // Add all the font families found in a font set.
  // aAppFonts indicates whether this is the system or application fontset.
  void AddFontSetFamilies(FcFontSet* aFontSet, const SandboxPolicy* aPolicy,
                          bool aAppFonts);

  // Helper for above, to add a single font pattern.
  void AddPatternToFontList(FcPattern* aFont, FcChar8*& aLastFamilyName,
                            nsACString& aFamilyName,
                            RefPtr<gfxFontconfigFontFamily>& aFontFamily,
                            bool aAppFonts);

  // figure out which families fontconfig maps a generic to
  // (aGeneric assumed already lowercase)
  PrefFontList* FindGenericFamilies(const nsCString& aGeneric,
                                    nsAtom* aLanguage);

  // are all pref font settings set to use fontconfig generics?
  bool PrefFontListsUseOnlyGenerics();

  static void CheckFontUpdates(nsITimer* aTimer, void* aThis);

  FontFamily GetDefaultFontForPlatform(const gfxFontStyle* aStyle,
                                       nsAtom* aLanguage = nullptr) override;

  enum class DistroID : int8_t {
    Unknown = 0,
    Ubuntu = 1,
    Fedora = 2,
    // To be extended with any distros that ship a useful base set of fonts
    // that we want to explicitly support.
  };
  DistroID GetDistroID() const;  // -> DistroID::Unknown if we can't tell

  FontVisibility GetVisibilityForFamily(const nsACString& aName) const;

  gfxFontFamily* CreateFontFamily(const nsACString& aName,
                                  FontVisibility aVisibility) const override;

  // helper method for finding an appropriate lang string
  bool TryLangForGroup(const nsACString& aOSLang, nsAtom* aLangGroup,
                       nsACString& aLang, bool aForFontEnumerationThread);

#ifdef MOZ_BUNDLED_FONTS
  void ActivateBundledFonts();
  nsCString mBundledFontsPath;
  bool mBundledFontsInitialized;
#endif

  // to avoid enumerating all fonts, maintain a mapping of local font
  // names to family
  nsTHashMap<nsCString, RefPtr<FcPattern>> mLocalNames;

  // caching generic/lang ==> font family list
  nsClassHashtable<nsCStringHashKey, PrefFontList> mGenericMappings;

  // Caching family lookups as found by FindAndAddFamilies after resolving
  // substitutions. The gfxFontFamily objects cached here are owned by the
  // gfxFcPlatformFontList via its mFamilies table; note that if the main
  // font list is rebuilt (e.g. due to a fontconfig configuration change),
  // these pointers will be invalidated. InitFontList() flushes the cache
  // in this case.
  nsTHashMap<nsCStringHashKey, nsTArray<FamilyAndGeneric>> mFcSubstituteCache;

  nsCOMPtr<nsITimer> mCheckFontUpdatesTimer;
  RefPtr<FcConfig> mLastConfig;

  // By default, font prefs under Linux are set to simply lookup
  // via fontconfig the appropriate font for serif/sans-serif/monospace.
  // Rather than check each time a font pref is used, check them all at startup
  // and set a boolean to flag the case that non-default user font prefs exist
  // Note: langGroup == x-math is handled separately
  bool mAlwaysUseFontconfigGenerics;

  static FT_Library sFTLibrary;
};

#endif /* GFXPLATFORMFONTLIST_H_ */
