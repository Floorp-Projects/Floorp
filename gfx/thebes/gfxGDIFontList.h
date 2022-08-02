/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_GDIFONTLIST_H
#define GFX_GDIFONTLIST_H

#include "mozilla/FontPropertyTypes.h"
#include "mozilla/MemoryReporting.h"
#include "gfxWindowsPlatform.h"
#include "gfxPlatformFontList.h"
#include "nsGkAtoms.h"
#include "mozilla/gfx/UnscaledFontGDI.h"

#include <windows.h>

class AutoDC  // get the global device context, and auto-release it on
              // destruction
{
 public:
  AutoDC() { mDC = ::GetDC(nullptr); }

  ~AutoDC() { ::ReleaseDC(nullptr, mDC); }

  HDC GetDC() { return mDC; }

 private:
  HDC mDC;
};

class AutoSelectFont  // select a font into the given DC, and auto-restore
{
 public:
  AutoSelectFont(HDC aDC, LOGFONTW* aLogFont) : mOwnsFont(false) {
    mFont = ::CreateFontIndirectW(aLogFont);
    if (mFont) {
      mOwnsFont = true;
      mDC = aDC;
      mOldFont = (HFONT)::SelectObject(aDC, mFont);
    } else {
      mOldFont = nullptr;
    }
  }

  AutoSelectFont(HDC aDC, HFONT aFont) : mOwnsFont(false) {
    mDC = aDC;
    mFont = aFont;
    mOldFont = (HFONT)::SelectObject(aDC, aFont);
  }

  ~AutoSelectFont() {
    if (mOldFont) {
      ::SelectObject(mDC, mOldFont);
      if (mOwnsFont) {
        ::DeleteObject(mFont);
      }
    }
  }

  bool IsValid() const { return mFont != nullptr; }

  HFONT GetFont() const { return mFont; }

 private:
  HDC mDC;
  HFONT mFont;
  HFONT mOldFont;
  bool mOwnsFont;
};

/**
 * List of different types of fonts we support on Windows.
 * These can generally be lumped in to 3 categories where we have to
 * do special things:  Really old fonts bitmap and vector fonts (device
 * and raster), Type 1 fonts, and TrueType/OpenType fonts.
 *
 * This list is sorted in order from least prefered to most prefered.
 * We prefer Type1 fonts over OpenType fonts to avoid falling back to
 * things like Arial (opentype) when you ask for Helvetica (type1)
 **/
enum gfxWindowsFontType {
  GFX_FONT_TYPE_UNKNOWN = 0,
  GFX_FONT_TYPE_DEVICE,
  GFX_FONT_TYPE_RASTER,
  GFX_FONT_TYPE_TRUETYPE,
  GFX_FONT_TYPE_PS_OPENTYPE,
  GFX_FONT_TYPE_TT_OPENTYPE,
  GFX_FONT_TYPE_TYPE1
};

// A single member of a font family (i.e. a single face, such as Times Italic)
// represented as a LOGFONT that will resolve to the correct face.
// This replaces FontEntry from gfxWindowsFonts.h/cpp.
class GDIFontEntry final : public gfxFontEntry {
 public:
  LPLOGFONTW GetLogFont() { return &mLogFont; }

  nsresult ReadCMAP(FontInfoData* aFontInfoData = nullptr) override;

  void FillLogFont(LOGFONTW* aLogFont, LONG aWeight, gfxFloat aSize);

  static gfxWindowsFontType DetermineFontType(const NEWTEXTMETRICW& metrics,
                                              DWORD fontType) {
    gfxWindowsFontType feType;
    if (metrics.ntmFlags & NTM_TYPE1)
      feType = GFX_FONT_TYPE_TYPE1;
    else if (metrics.ntmFlags & NTM_PS_OPENTYPE)
      feType = GFX_FONT_TYPE_PS_OPENTYPE;
    else if (metrics.ntmFlags & NTM_TT_OPENTYPE)
      feType = GFX_FONT_TYPE_TT_OPENTYPE;
    else if (fontType == TRUETYPE_FONTTYPE)
      feType = GFX_FONT_TYPE_TRUETYPE;
    else if (fontType == RASTER_FONTTYPE)
      feType = GFX_FONT_TYPE_RASTER;
    else if (fontType == DEVICE_FONTTYPE)
      feType = GFX_FONT_TYPE_DEVICE;
    else
      feType = GFX_FONT_TYPE_UNKNOWN;

    return feType;
  }

  bool IsType1() const { return (mFontType == GFX_FONT_TYPE_TYPE1); }

  bool IsTrueType() const {
    return (mFontType == GFX_FONT_TYPE_TRUETYPE ||
            mFontType == GFX_FONT_TYPE_PS_OPENTYPE ||
            mFontType == GFX_FONT_TYPE_TT_OPENTYPE);
  }

  bool SkipDuringSystemFallback() override {
    return !HasCmapTable();  // explicitly skip non-SFNT fonts
  }

  bool TestCharacterMap(uint32_t aCh) override;

  void AddSizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf,
                              FontListSizes* aSizes) const override;

  gfxFontEntry* Clone() const override;

  // GDI backend doesn't support font variations:
  bool HasVariations() override { return false; }
  void GetVariationAxes(nsTArray<gfxFontVariationAxis>&) override {}
  void GetVariationInstances(nsTArray<gfxFontVariationInstance>&) override {}

  // create a font entry for a font with a given name
  static GDIFontEntry* CreateFontEntry(const nsACString& aName,
                                       gfxWindowsFontType aFontType,
                                       SlantStyleRange aStyle,
                                       WeightRange aWeight,
                                       StretchRange aStretch,
                                       gfxUserFontData* aUserFontData);

  gfxWindowsFontType mFontType;
  bool mForceGDI;

 protected:
  friend class gfxGDIFont;

  GDIFontEntry(const nsACString& aFaceName, gfxWindowsFontType aFontType,
               SlantStyleRange aStyle, WeightRange aWeight,
               StretchRange aStretch, gfxUserFontData* aUserFontData);

  void InitLogFont(const nsACString& aName, gfxWindowsFontType aFontType);

  gfxFont* CreateFontInstance(const gfxFontStyle* aFontStyle) override;

  virtual nsresult CopyFontTable(uint32_t aTableTag,
                                 nsTArray<uint8_t>& aBuffer) override;

  already_AddRefed<mozilla::gfx::UnscaledFontGDI> LookupUnscaledFont(
      HFONT aFont);

  LOGFONTW mLogFont;

  mozilla::ThreadSafeWeakPtr<mozilla::gfx::UnscaledFontGDI> mUnscaledFont;
};

// a single font family, referencing one or more faces
class GDIFontFamily final : public gfxFontFamily {
 public:
  GDIFontFamily(const nsACString& aName, FontVisibility aVisibility)
      : gfxFontFamily(aName, aVisibility),
        mWindowsFamily(0),
        mWindowsPitch(0),
        mCharset() {}

  void FindStyleVariationsLocked(FontInfoData* aFontInfoData = nullptr)
      MOZ_REQUIRES(mLock) override;

  bool FilterForFontList(nsAtom* aLangGroup,
                         const nsACString& aGeneric) const final {
    return !IsSymbolFontFamily() && SupportsLangGroup(aLangGroup) &&
           MatchesGenericFamily(aGeneric);
  }

 protected:
  friend class gfxGDIFontList;

  // helpers for FilterForFontList
  bool IsSymbolFontFamily() const { return mCharset.test(SYMBOL_CHARSET); }

  bool MatchesGenericFamily(const nsACString& aGeneric) const {
    if (aGeneric.IsEmpty()) {
      return true;
    }

    // Japanese 'Mincho' fonts do not belong to FF_MODERN even if
    // they are fixed pitch because they have variable stroke width.
    if (mWindowsFamily == FF_ROMAN && mWindowsPitch & FIXED_PITCH) {
      return aGeneric.EqualsLiteral("monospace");
    }

    // Japanese 'Gothic' fonts do not belong to FF_SWISS even if
    // they are variable pitch because they have constant stroke width.
    if (mWindowsFamily == FF_MODERN && mWindowsPitch & VARIABLE_PITCH) {
      return aGeneric.EqualsLiteral("sans-serif");
    }

    // All other fonts will be grouped correctly using family...
    switch (mWindowsFamily) {
      case FF_DONTCARE:
        return false;
      case FF_ROMAN:
        return aGeneric.EqualsLiteral("serif");
      case FF_SWISS:
        return aGeneric.EqualsLiteral("sans-serif");
      case FF_MODERN:
        return aGeneric.EqualsLiteral("monospace");
      case FF_SCRIPT:
        return aGeneric.EqualsLiteral("cursive");
      case FF_DECORATIVE:
        return aGeneric.EqualsLiteral("fantasy");
    }

    return false;
  }

  bool SupportsLangGroup(nsAtom* aLangGroup) const {
    if (!aLangGroup || aLangGroup == nsGkAtoms::Unicode) {
      return true;
    }

    int16_t bit = -1;

    /* map our langgroup names in to Windows charset bits */
    if (aLangGroup == nsGkAtoms::x_western) {
      bit = ANSI_CHARSET;
    } else if (aLangGroup == nsGkAtoms::Japanese) {
      bit = SHIFTJIS_CHARSET;
    } else if (aLangGroup == nsGkAtoms::ko) {
      bit = HANGEUL_CHARSET;
    } else if (aLangGroup == nsGkAtoms::zh_cn) {
      bit = GB2312_CHARSET;
    } else if (aLangGroup == nsGkAtoms::zh_tw) {
      bit = CHINESEBIG5_CHARSET;
    } else if (aLangGroup == nsGkAtoms::el) {
      bit = GREEK_CHARSET;
    } else if (aLangGroup == nsGkAtoms::he) {
      bit = HEBREW_CHARSET;
    } else if (aLangGroup == nsGkAtoms::ar) {
      bit = ARABIC_CHARSET;
    } else if (aLangGroup == nsGkAtoms::x_cyrillic) {
      bit = RUSSIAN_CHARSET;
    } else if (aLangGroup == nsGkAtoms::th) {
      bit = THAI_CHARSET;
    }

    if (bit != -1) {
      return mCharset.test(bit);
    }

    return false;
  }

  uint8_t mWindowsFamily;
  uint8_t mWindowsPitch;

  gfxSparseBitSet mCharset;

 private:
  static int CALLBACK FamilyAddStylesProc(const ENUMLOGFONTEXW* lpelfe,
                                          const NEWTEXTMETRICEXW* nmetrics,
                                          DWORD fontType, LPARAM data);
};

class gfxGDIFontList final : public gfxPlatformFontList {
 public:
  static gfxGDIFontList* PlatformFontList() {
    return static_cast<gfxGDIFontList*>(
        gfxPlatformFontList::PlatformFontList());
  }

  virtual ~gfxGDIFontList() { AutoLock lock(mLock); }

  // initialize font lists
  nsresult InitFontListForPlatform() MOZ_REQUIRES(mLock) override;

  gfxFontFamily* CreateFontFamily(const nsACString& aName,
                                  FontVisibility aVisibility) const override;

  bool FindAndAddFamiliesLocked(
      nsPresContext* aPresContext, mozilla::StyleGenericFontFamily aGeneric,
      const nsACString& aFamily, nsTArray<FamilyAndGeneric>* aOutput,
      FindFamiliesFlags aFlags, gfxFontStyle* aStyle = nullptr,
      nsAtom* aLanguage = nullptr, gfxFloat aDevToCssSize = 1.0)
      MOZ_REQUIRES(mLock) override;

  gfxFontEntry* LookupLocalFont(nsPresContext* aPresContext,
                                const nsACString& aFontName,
                                WeightRange aWeightForEntry,
                                StretchRange aStretchForEntry,
                                SlantStyleRange aStyleForEntry) override;

  gfxFontEntry* MakePlatformFont(const nsACString& aFontName,
                                 WeightRange aWeightForEntry,
                                 StretchRange aStretchForEntry,
                                 SlantStyleRange aStyleForEntry,
                                 const uint8_t* aFontData,
                                 uint32_t aLength) override;

  void AddSizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf,
                              FontListSizes* aSizes) const override;
  void AddSizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf,
                              FontListSizes* aSizes) const override;

 protected:
  FontFamily GetDefaultFontForPlatform(nsPresContext* aPresContext,
                                       const gfxFontStyle* aStyle,
                                       nsAtom* aLanguage = nullptr)
      MOZ_REQUIRES(mLock) override;

 private:
  friend class gfxWindowsPlatform;

  gfxGDIFontList();

  nsresult GetFontSubstitutes() MOZ_REQUIRES(mLock);

  static int CALLBACK EnumFontFamExProc(ENUMLOGFONTEXW* lpelfe,
                                        NEWTEXTMETRICEXW* lpntme,
                                        DWORD fontType, LPARAM lParam);

  already_AddRefed<FontInfoData> CreateFontInfoData() override;

#ifdef MOZ_BUNDLED_FONTS
  void ActivateBundledFonts();
#endif

  FontFamilyTable mFontSubstitutes;
  nsTArray<nsCString> mNonExistingFonts;
};

#endif /* GFX_GDIFONTLIST_H */
