/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFXFCPLATFORMFONTLIST_H_
#define GFXFCPLATFORMFONTLIST_H_

#include "gfxFont.h"
#include "gfxFontEntry.h"
#include "gfxFT2FontBase.h"
#include "gfxPlatformFontList.h"
#include "mozilla/mozalloc.h"

#include <fontconfig/fontconfig.h>
#include "ft2build.h"
#include FT_FREETYPE_H
#include FT_TRUETYPE_TABLES_H
#include <cairo.h>
#include <cairo-ft.h>

#include "gfxFontconfigUtils.h" // xxx - only for nsAutoRefTraits<FcPattern>, etc.

template <>
class nsAutoRefTraits<FcObjectSet> : public nsPointerRefTraits<FcObjectSet>
{
public:
    static void Release(FcObjectSet *ptr) { FcObjectSetDestroy(ptr); }
};

template <>
class nsAutoRefTraits<FcConfig> : public nsPointerRefTraits<FcConfig>
{
public:
    static void Release(FcConfig *ptr) { FcConfigDestroy(ptr); }
    static void AddRef(FcConfig *ptr) { FcConfigReference(ptr); }
};

// Helper classes used for clearning out user font data when cairo font
// face is destroyed. Since multiple faces may use the same data, be
// careful to assure that the data is only cleared out when all uses
// expire. The font entry object contains a refptr to FTUserFontData and
// each cairo font created from that font entry contains a
// FTUserFontDataRef with a refptr to that same FTUserFontData object.

class FTUserFontData {
public:
    NS_INLINE_DECL_REFCOUNTING(FTUserFontData)

    explicit FTUserFontData(FT_Face aFace, const uint8_t* aData)
        : mFace(aFace), mFontData(aData)
    {
    }

    const uint8_t *FontData() const { return mFontData; }

private:
    ~FTUserFontData()
    {
        FT_Done_Face(mFace);
        if (mFontData) {
            NS_Free((void*)mFontData);
        }
    }

    FT_Face        mFace;
    const uint8_t *mFontData;
};

class FTUserFontDataRef {
public:
    explicit FTUserFontDataRef(FTUserFontData *aUserFontData)
        : mUserFontData(aUserFontData)
    {
    }

    static void Destroy(void* aData) {
        FTUserFontDataRef* aUserFontDataRef =
            static_cast<FTUserFontDataRef*>(aData);
        delete aUserFontDataRef;
    }

private:
    nsRefPtr<FTUserFontData> mUserFontData;
};

// The names for the font entry and font classes should really
// the common 'Fc' abbreviation but the gfxPangoFontGroup code already
// defines versions of these, so use the verbose name for now.

class gfxFontconfigFontEntry : public gfxFontEntry {
public:
    // used for system fonts with explicit patterns
    explicit gfxFontconfigFontEntry(const nsAString& aFaceName,
                                    FcPattern* aFontPattern);

    // used for data fonts where the fontentry takes ownership
    // of the font data and the FT_Face
    explicit gfxFontconfigFontEntry(const nsAString& aFaceName,
                                    uint16_t aWeight,
                                    int16_t aStretch,
                                    bool aItalic,
                                    const uint8_t *aData,
                                    FT_Face aFace);

    // used for @font-face local system fonts with explicit patterns
    explicit gfxFontconfigFontEntry(const nsAString& aFaceName,
                                    FcPattern* aFontPattern,
                                    uint16_t aWeight,
                                    int16_t aStretch,
                                    bool aItalic);

    FcPattern* GetPattern() { return mFontPattern; }

    bool SupportsLangGroup(nsIAtom *aLangGroup) const override;

    nsresult ReadCMAP(FontInfoData *aFontInfoData = nullptr) override;
    bool TestCharacterMap(uint32_t aCh) override;

    hb_blob_t* GetFontTable(uint32_t aTableTag) override;

    void ForgetHBFace() override;
    void ReleaseGrFace(gr_face* aFace) override;

protected:
    virtual ~gfxFontconfigFontEntry();

    gfxFont *CreateFontInstance(const gfxFontStyle *aFontStyle,
                                bool aNeedsBold) override;

    // helper method for creating cairo font from pattern
    cairo_scaled_font_t*
    CreateScaledFont(FcPattern* aRenderPattern,
                     const gfxFontStyle *aStyle,
                     bool aNeedsBold);

    // override to pull data from FTFace
    virtual nsresult
    CopyFontTable(uint32_t aTableTag,
                  FallibleTArray<uint8_t>& aBuffer) override;

    // if HB or GR faces are gone, close down the FT_Face
    void MaybeReleaseFTFace();

    double GetAspect();

    // pattern for a single face of a family
    nsCountedRef<FcPattern> mFontPattern;

    // user font data, when needed
    nsRefPtr<FTUserFontData> mUserFontData;

    // FTFace - initialized when needed
    FT_Face   mFTFace;
    bool      mFTFaceInitialized;
    double    mAspect;

    // data font
    const uint8_t* mFontData;
};

class gfxFontconfigFontFamily : public gfxFontFamily {
public:
    explicit gfxFontconfigFontFamily(const nsAString& aName) :
        gfxFontFamily(aName) { }

    void FindStyleVariations(FontInfoData *aFontInfoData = nullptr) override;

    // Families are constructed initially with just references to patterns.
    // When necessary, these are enumerated within FindStyleVariations.
    void AddFontPattern(FcPattern* aFontPattern);

protected:
    virtual ~gfxFontconfigFontFamily() { }

    nsTArray<nsCountedRef<FcPattern> > mFontPatterns;
};

class gfxFontconfigFont : public gfxFT2FontBase {
public:
    gfxFontconfigFont(cairo_scaled_font_t *aScaledFont,
                      gfxFontEntry *aFontEntry,
                      const gfxFontStyle *aFontStyle,
                      bool aNeedsBold);

#ifdef USE_SKIA
    virtual already_AddRefed<mozilla::gfx::GlyphRenderingOptions>
    GetGlyphRenderingOptions(const TextRunDrawParams* aRunParams = nullptr) override;
#endif

protected:
    virtual ~gfxFontconfigFont();
};

class gfxFcPlatformFontList : public gfxPlatformFontList {
public:
    gfxFcPlatformFontList();

    // initialize font lists
    nsresult InitFontList() override;

    void GetFontList(nsIAtom *aLangGroup,
                     const nsACString& aGenericFamily,
                     nsTArray<nsString>& aListOfFonts) override;


    gfxFontFamily*
    GetDefaultFont(const gfxFontStyle* aStyle) override;

    gfxFontEntry*
    LookupLocalFont(const nsAString& aFontName, uint16_t aWeight,
                    int16_t aStretch, bool aItalic) override;

    gfxFontEntry*
    MakePlatformFont(const nsAString& aFontName, uint16_t aWeight,
                     int16_t aStretch, bool aItalic,
                     const uint8_t* aFontData,
                     uint32_t aLength) override;

    gfxFontFamily* FindFamily(const nsAString& aFamily,
                              gfxFontStyle* aStyle = nullptr) override;

    bool GetStandardFamilyName(const nsAString& aFontName,
                               nsAString& aFamilyName) override;

    FcConfig* GetLastConfig() const { return mLastConfig; }

    static FT_Library GetFTLibrary();

protected:
    virtual ~gfxFcPlatformFontList();

    // add all the font families found in a font set
    void AddFontSetFamilies(FcFontSet* aFontSet);

    // figure out which family fontconfig maps a generic to
    // (aGeneric assumed already lowercase)
    gfxFontFamily* FindGenericFamily(const nsAString& aGeneric,
                                     nsIAtom* aLanguage);

    static void CheckFontUpdates(nsITimer *aTimer, void *aThis);

#ifdef MOZ_BUNDLED_FONTS
    void ActivateBundledFonts();
    nsCString mBundledFontsPath;
    bool mBundledFontsInitialized;
#endif

    // to avoid enumerating all fonts, maintain a mapping of local font
    // names to family
    nsBaseHashtable<nsStringHashKey,
                    nsCountedRef<FcPattern>,
                    FcPattern*> mLocalNames;

    // caching generic/lang ==> font family
    nsRefPtrHashtable<nsCStringHashKey, gfxFontFamily> mGenericMappings;

    // caching family lookups as found by FindFamily after resolving substitutions
    nsRefPtrHashtable<nsCStringHashKey, gfxFontFamily> mFcSubstituteCache;

    nsCOMPtr<nsITimer> mCheckFontUpdatesTimer;
    nsCountedRef<FcConfig> mLastConfig;

    static FT_Library sCairoFTLibrary;
};

#endif /* GFXPLATFORMFONTLIST_H_ */
