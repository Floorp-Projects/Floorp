/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFXFCPLATFORMFONTLIST_H_
#define GFXFCPLATFORMFONTLIST_H_

#include "gfxFont.h"
#include "gfxFontEntry.h"
#include "gfxFT2FontBase.h"
#include "gfxPlatformFontList.h"
#include "mozilla/FontPropertyTypes.h"
#include "mozilla/mozalloc.h"
#include "nsAutoRef.h"
#include "nsClassHashtable.h"

#include <fontconfig/fontconfig.h>
#include "ft2build.h"
#include FT_FREETYPE_H
#include FT_TRUETYPE_TABLES_H
#include FT_MULTIPLE_MASTERS_H
#include <cairo.h>
#include <cairo-ft.h>

#if defined(MOZ_SANDBOX) && defined(XP_LINUX)
#  include "mozilla/SandboxBroker.h"
#endif

namespace mozilla {
namespace dom {
class SystemFontListEntry;
};
};  // namespace mozilla

template <>
class nsAutoRefTraits<FcPattern> : public nsPointerRefTraits<FcPattern> {
 public:
  static void Release(FcPattern* ptr) { FcPatternDestroy(ptr); }
  static void AddRef(FcPattern* ptr) { FcPatternReference(ptr); }
};

template <>
class nsAutoRefTraits<FcConfig> : public nsPointerRefTraits<FcConfig> {
 public:
  static void Release(FcConfig* ptr) { FcConfigDestroy(ptr); }
  static void AddRef(FcConfig* ptr) { FcConfigReference(ptr); }
};

// Helper classes used for clearning out user font data when cairo font
// face is destroyed. Since multiple faces may use the same data, be
// careful to assure that the data is only cleared out when all uses
// expire. The font entry object contains a refptr to FTUserFontData and
// each cairo font created from that font entry contains a
// FTUserFontDataRef with a refptr to that same FTUserFontData object.

class FTUserFontData final {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(FTUserFontData)

  explicit FTUserFontData(FT_Face aFace, const uint8_t* aData)
      : mFace(aFace), mFontData(aData) {}

  const uint8_t* FontData() const { return mFontData; }

 private:
  ~FTUserFontData() {
    mozilla::gfx::Factory::ReleaseFTFace(mFace);
    if (mFontData) {
      free((void*)mFontData);
    }
  }

  FT_Face mFace;
  const uint8_t* mFontData;
};

// The names for the font entry and font classes should really
// the common 'Fc' abbreviation but the gfxPangoFontGroup code already
// defines versions of these, so use the verbose name for now.

class gfxFontconfigFontEntry : public gfxFontEntry {
 public:
  // used for system fonts with explicit patterns
  explicit gfxFontconfigFontEntry(const nsACString& aFaceName,
                                  FcPattern* aFontPattern,
                                  bool aIgnoreFcCharmap);

  // used for data fonts where the fontentry takes ownership
  // of the font data and the FT_Face
  explicit gfxFontconfigFontEntry(const nsACString& aFaceName,
                                  WeightRange aWeight, StretchRange aStretch,
                                  SlantStyleRange aStyle, const uint8_t* aData,
                                  uint32_t aLength, FT_Face aFace);

  // used for @font-face local system fonts with explicit patterns
  explicit gfxFontconfigFontEntry(const nsACString& aFaceName,
                                  FcPattern* aFontPattern, WeightRange aWeight,
                                  StretchRange aStretch,
                                  SlantStyleRange aStyle);

  gfxFontEntry* Clone() const override;

  FcPattern* GetPattern() { return mFontPattern; }

  nsresult ReadCMAP(FontInfoData* aFontInfoData = nullptr) override;
  bool TestCharacterMap(uint32_t aCh) override;

  FT_Face GetFTFace();

  FT_MM_Var* GetMMVar() override;

  bool HasVariations() override;
  void GetVariationAxes(nsTArray<gfxFontVariationAxis>& aAxes) override;
  void GetVariationInstances(
      nsTArray<gfxFontVariationInstance>& aInstances) override;

  hb_blob_t* GetFontTable(uint32_t aTableTag) override;

  void ForgetHBFace() override;
  void ReleaseGrFace(gr_face* aFace) override;

  double GetAspect();

 protected:
  virtual ~gfxFontconfigFontEntry();

  gfxFont* CreateFontInstance(const gfxFontStyle* aFontStyle) override;

  // helper method for creating cairo font from pattern
  cairo_scaled_font_t* CreateScaledFont(FcPattern* aRenderPattern,
                                        gfxFloat aAdjustedSize,
                                        const gfxFontStyle* aStyle,
                                        FT_Face aFTFace);

  // override to pull data from FTFace
  virtual nsresult CopyFontTable(uint32_t aTableTag,
                                 nsTArray<uint8_t>& aBuffer) override;

  // if HB or GR faces are gone, close down the FT_Face
  void MaybeReleaseFTFace();

  // pattern for a single face of a family
  nsCountedRef<FcPattern> mFontPattern;

  // user font data, when needed
  RefPtr<FTUserFontData> mUserFontData;

  // FTFace - initialized when needed
  FT_Face mFTFace;
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

  // data font
  const uint8_t* mFontData;
  uint32_t mLength;

  class UnscaledFontCache {
   public:
    already_AddRefed<mozilla::gfx::UnscaledFontFontconfig> Lookup(
        const char* aFile, uint32_t aIndex);

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

class gfxFontconfigFontFamily : public gfxFontFamily {
 public:
  explicit gfxFontconfigFontFamily(const nsACString& aName)
      : gfxFontFamily(aName),
        mContainsAppFonts(false),
        mHasNonScalableFaces(false),
        mForceScalable(false) {}

  template <typename Func>
  void AddFacesToFontList(Func aAddPatternFunc);

  void FindStyleVariations(FontInfoData* aFontInfoData = nullptr) override;

  // Families are constructed initially with just references to patterns.
  // When necessary, these are enumerated within FindStyleVariations.
  void AddFontPattern(FcPattern* aFontPattern);

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

  nsTArray<nsCountedRef<FcPattern>> mFontPatterns;

  bool mContainsAppFonts;
  bool mHasNonScalableFaces;
  bool mForceScalable;
};

class gfxFontconfigFont : public gfxFT2FontBase {
 public:
  gfxFontconfigFont(
      const RefPtr<mozilla::gfx::UnscaledFontFontconfig>& aUnscaledFont,
      cairo_scaled_font_t* aScaledFont, FcPattern* aPattern,
      gfxFloat aAdjustedSize, gfxFontEntry* aFontEntry,
      const gfxFontStyle* aFontStyle);

  FontType GetType() const override { return FONT_TYPE_FONTCONFIG; }
  virtual FcPattern* GetPattern() const { return mPattern; }

  virtual already_AddRefed<mozilla::gfx::ScaledFont> GetScaledFont(
      DrawTarget* aTarget) override;

 private:
  virtual ~gfxFontconfigFont();

  nsCountedRef<FcPattern> mPattern;
};

class gfxFcPlatformFontList : public gfxPlatformFontList {
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

  void ReadSystemFontList(
      InfallibleTArray<mozilla::dom::SystemFontListEntry>* retValue);

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

  static FT_Library GetFTLibrary();

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

  FontFamily GetDefaultFontForPlatform(const gfxFontStyle* aStyle) override;

  gfxFontFamily* CreateFontFamily(const nsACString& aName) const override;

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
  nsBaseHashtable<nsCStringHashKey, nsCountedRef<FcPattern>, FcPattern*>
      mLocalNames;

  // caching generic/lang ==> font family list
  nsClassHashtable<nsCStringHashKey, PrefFontList> mGenericMappings;

  // Caching family lookups as found by FindAndAddFamilies after resolving
  // substitutions. The gfxFontFamily objects cached here are owned by the
  // gfxFcPlatformFontList via its mFamilies table; note that if the main
  // font list is rebuilt (e.g. due to a fontconfig configuration change),
  // these pointers will be invalidated. InitFontList() flushes the cache
  // in this case.
  nsDataHashtable<nsCStringHashKey, nsTArray<FamilyAndGeneric>>
      mFcSubstituteCache;

  nsCOMPtr<nsITimer> mCheckFontUpdatesTimer;
  nsCountedRef<FcConfig> mLastConfig;

  // By default, font prefs under Linux are set to simply lookup
  // via fontconfig the appropriate font for serif/sans-serif/monospace.
  // Rather than check each time a font pref is used, check them all at startup
  // and set a boolean to flag the case that non-default user font prefs exist
  // Note: langGroup == x-math is handled separately
  bool mAlwaysUseFontconfigGenerics;

  static FT_Library sCairoFTLibrary;
};

#endif /* GFXPLATFORMFONTLIST_H_ */
