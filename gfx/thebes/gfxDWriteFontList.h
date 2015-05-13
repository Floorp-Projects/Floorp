/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_DWRITEFONTLIST_H
#define GFX_DWRITEFONTLIST_H

#include "mozilla/MemoryReporting.h"
#include "gfxDWriteCommon.h"

#include "gfxFont.h"
#include "gfxUserFontSet.h"
#include "cairo-win32.h"

#include "gfxPlatformFontList.h"
#include "gfxPlatform.h"
#include <algorithm>


/**
 * gfxDWriteFontFamily is a class that describes one of the fonts on the
 * users system.  It holds each gfxDWriteFontEntry (maps more directly to
 * a font face) which holds font type, charset info and character map info.
 */
class gfxDWriteFontEntry;

/**
 * \brief Class representing directwrite font family.
 */
class gfxDWriteFontFamily : public gfxFontFamily
{
public:
    /**
     * Constructs a new DWriteFont Family.
     *
     * \param aName Name identifying the family
     * \param aFamily IDWriteFontFamily object representing the directwrite
     * family object.
     */
    gfxDWriteFontFamily(const nsAString& aName, 
                        IDWriteFontFamily *aFamily)
      : gfxFontFamily(aName), mDWFamily(aFamily), mForceGDIClassic(false) {}
    virtual ~gfxDWriteFontFamily();
    
    virtual void FindStyleVariations(FontInfoData *aFontInfoData = nullptr);

    virtual void LocalizedName(nsAString& aLocalizedName);

    virtual void ReadFaceNames(gfxPlatformFontList *aPlatformFontList,
                               bool aNeedFullnamePostscriptNames);

    void SetForceGDIClassic(bool aForce) { mForceGDIClassic = aForce; }

    virtual void AddSizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf,
                                        FontListSizes* aSizes) const;
    virtual void AddSizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf,
                                        FontListSizes* aSizes) const;

protected:
    /** This font family's directwrite fontfamily object */
    nsRefPtr<IDWriteFontFamily> mDWFamily;
    bool mForceGDIClassic;
};

/**
 * \brief Class representing DirectWrite FontEntry (a unique font style/family)
 */
class gfxDWriteFontEntry : public gfxFontEntry
{
public:
    /**
     * Constructs a font entry.
     *
     * \param aFaceName The name of the corresponding font face.
     * \param aFont DirectWrite font object
     */
    gfxDWriteFontEntry(const nsAString& aFaceName,
                              IDWriteFont *aFont) 
      : gfxFontEntry(aFaceName), mFont(aFont), mFontFile(nullptr),
        mForceGDIClassic(false)
    {
        mItalic = (aFont->GetStyle() == DWRITE_FONT_STYLE_ITALIC ||
                   aFont->GetStyle() == DWRITE_FONT_STYLE_OBLIQUE);
        mStretch = FontStretchFromDWriteStretch(aFont->GetStretch());
        uint16_t weight = NS_ROUNDUP(aFont->GetWeight() - 50, 100);

        weight = std::max<uint16_t>(100, weight);
        weight = std::min<uint16_t>(900, weight);
        mWeight = weight;

        mIsCJK = UNINITIALIZED_VALUE;
    }

    /**
     * Constructs a font entry using a font. But with custom font values.
     * This is used for creating correct font entries for @font-face with local
     * font source.
     *
     * \param aFaceName The name of the corresponding font face.
     * \param aFont DirectWrite font object
     * \param aWeight Weight of the font
     * \param aStretch Stretch of the font
     * \param aItalic True if italic
     */
    gfxDWriteFontEntry(const nsAString& aFaceName,
                              IDWriteFont *aFont,
                              uint16_t aWeight,
                              int16_t aStretch,
                              bool aItalic)
      : gfxFontEntry(aFaceName), mFont(aFont), mFontFile(nullptr),
        mForceGDIClassic(false)
    {
        mWeight = aWeight;
        mStretch = aStretch;
        mItalic = aItalic;
        mIsLocalUserFont = true;
        mIsCJK = UNINITIALIZED_VALUE;
    }

    /**
     * Constructs a font entry using a font file.
     *
     * \param aFaceName The name of the corresponding font face.
     * \param aFontFile DirectWrite fontfile object
     * \param aWeight Weight of the font
     * \param aStretch Stretch of the font
     * \param aItalic True if italic
     */
    gfxDWriteFontEntry(const nsAString& aFaceName,
                              IDWriteFontFile *aFontFile,
                              uint16_t aWeight,
                              int16_t aStretch,
                              bool aItalic)
      : gfxFontEntry(aFaceName), mFont(nullptr), mFontFile(aFontFile),
        mForceGDIClassic(false)
    {
        mWeight = aWeight;
        mStretch = aStretch;
        mItalic = aItalic;
        mIsDataUserFont = true;
        mIsCJK = UNINITIALIZED_VALUE;
    }

    virtual ~gfxDWriteFontEntry();

    virtual bool IsSymbolFont();

    virtual hb_blob_t* GetFontTable(uint32_t aTableTag) override;

    nsresult ReadCMAP(FontInfoData *aFontInfoData = nullptr);

    bool IsCJKFont();

    void SetForceGDIClassic(bool aForce) { mForceGDIClassic = aForce; }
    bool GetForceGDIClassic() { return mForceGDIClassic; }

    virtual void AddSizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf,
                                        FontListSizes* aSizes) const;
    virtual void AddSizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf,
                                        FontListSizes* aSizes) const;

protected:
    friend class gfxDWriteFont;
    friend class gfxDWriteFontList;

    virtual nsresult CopyFontTable(uint32_t aTableTag,
                                   FallibleTArray<uint8_t>& aBuffer) override;

    virtual gfxFont *CreateFontInstance(const gfxFontStyle *aFontStyle,
                                        bool aNeedsBold);
    
    nsresult CreateFontFace(
        IDWriteFontFace **aFontFace,
        DWRITE_FONT_SIMULATIONS aSimulations = DWRITE_FONT_SIMULATIONS_NONE);

    static bool InitLogFont(IDWriteFont *aFont, LOGFONTW *aLogFont);

    /**
     * A fontentry only needs to have either of these. If it has both only
     * the IDWriteFont will be used.
     */
    nsRefPtr<IDWriteFont> mFont;
    nsRefPtr<IDWriteFontFile> mFontFile;

    // font face corresponding to the mFont/mFontFile *without* any DWrite
    // style simulations applied
    nsRefPtr<IDWriteFontFace> mFontFace;

    DWRITE_FONT_FACE_TYPE mFaceType;

    int8_t mIsCJK;
    bool mForceGDIClassic;
};

// custom text renderer used to determine the fallback font for a given char
class FontFallbackRenderer final : public IDWriteTextRenderer
{
public:
    FontFallbackRenderer(IDWriteFactory *aFactory)
        : mRefCount(0)
    {
        HRESULT hr = S_OK;

        hr = aFactory->GetSystemFontCollection(getter_AddRefs(mSystemFonts));
        NS_ASSERTION(SUCCEEDED(hr), "GetSystemFontCollection failed!");
    }

    ~FontFallbackRenderer()
    {}

    // IDWriteTextRenderer methods
    IFACEMETHOD(DrawGlyphRun)(
        void* clientDrawingContext,
        FLOAT baselineOriginX,
        FLOAT baselineOriginY,
        DWRITE_MEASURING_MODE measuringMode,
        DWRITE_GLYPH_RUN const* glyphRun,
        DWRITE_GLYPH_RUN_DESCRIPTION const* glyphRunDescription,
        IUnknown* clientDrawingEffect
        );

    IFACEMETHOD(DrawUnderline)(
        void* clientDrawingContext,
        FLOAT baselineOriginX,
        FLOAT baselineOriginY,
        DWRITE_UNDERLINE const* underline,
        IUnknown* clientDrawingEffect
        )
    {
        return E_NOTIMPL;
    }


    IFACEMETHOD(DrawStrikethrough)(
        void* clientDrawingContext,
        FLOAT baselineOriginX,
        FLOAT baselineOriginY,
        DWRITE_STRIKETHROUGH const* strikethrough,
        IUnknown* clientDrawingEffect
        )
    {
        return E_NOTIMPL;
    }


    IFACEMETHOD(DrawInlineObject)(
        void* clientDrawingContext,
        FLOAT originX,
        FLOAT originY,
        IDWriteInlineObject* inlineObject,
        BOOL isSideways,
        BOOL isRightToLeft,
        IUnknown* clientDrawingEffect
        )
    {
        return E_NOTIMPL;
    }

    // IDWritePixelSnapping methods

    IFACEMETHOD(IsPixelSnappingDisabled)(
        void* clientDrawingContext,
        BOOL* isDisabled
        )
    {
        *isDisabled = FALSE;
        return S_OK;
    }

    IFACEMETHOD(GetCurrentTransform)(
        void* clientDrawingContext,
        DWRITE_MATRIX* transform
        )
    {
        const DWRITE_MATRIX ident = {1.0, 0.0, 0.0, 1.0, 0.0, 0.0};
        *transform = ident;
        return S_OK;
    }

    IFACEMETHOD(GetPixelsPerDip)(
        void* clientDrawingContext,
        FLOAT* pixelsPerDip
        )
    {
        *pixelsPerDip = 1.0f;
        return S_OK;
    }

    // IUnknown methods

    IFACEMETHOD_(unsigned long, AddRef) ()
    {
        return InterlockedIncrement(&mRefCount);
    }

    IFACEMETHOD_(unsigned long,  Release) ()
    {
        unsigned long newCount = InterlockedDecrement(&mRefCount);
        if (newCount == 0)
        {
            delete this;
            return 0;
        }

        return newCount;
    }

    IFACEMETHOD(QueryInterface) (IID const& riid, void** ppvObject)
    {
        if (__uuidof(IDWriteTextRenderer) == riid) {
            *ppvObject = this;
        } else if (__uuidof(IDWritePixelSnapping) == riid) {
            *ppvObject = this;
        } else if (__uuidof(IUnknown) == riid) {
            *ppvObject = this;
        } else {
            *ppvObject = nullptr;
            return E_FAIL;
        }

        this->AddRef();
        return S_OK;
    }

    const nsString& FallbackFamilyName() { return mFamilyName; }

protected:
    long mRefCount;
    nsRefPtr<IDWriteFontCollection> mSystemFonts;
    nsString mFamilyName;
};



class gfxDWriteFontList : public gfxPlatformFontList {
public:
    gfxDWriteFontList();

    static gfxDWriteFontList* PlatformFontList() {
        return static_cast<gfxDWriteFontList*>(sPlatformFontList);
    }

    // initialize font lists
    virtual nsresult InitFontList();

    virtual gfxFontFamily* GetDefaultFont(const gfxFontStyle* aStyle);

    virtual gfxFontEntry* LookupLocalFont(const nsAString& aFontName,
                                          uint16_t aWeight,
                                          int16_t aStretch,
                                          bool aItalic);

    virtual gfxFontEntry* MakePlatformFont(const nsAString& aFontName,
                                           uint16_t aWeight,
                                           int16_t aStretch,
                                           bool aItalic,
                                           const uint8_t* aFontData,
                                           uint32_t aLength);
    
    bool GetStandardFamilyName(const nsAString& aFontName,
                                 nsAString& aFamilyName);

    IDWriteGdiInterop *GetGDIInterop() { return mGDIInterop; }
    bool UseGDIFontTableAccess() { return mGDIFontTableAccess; }

    virtual gfxFontFamily* FindFamily(const nsAString& aFamily,
                                      nsIAtom* aLanguage = nullptr,
                                      bool aUseSystemFonts = false);

    virtual void GetFontFamilyList(nsTArray<nsRefPtr<gfxFontFamily> >& aFamilyArray);

    gfxFloat GetForceGDIClassicMaxFontSize() { return mForceGDIClassicMaxFontSize; }

    virtual void AddSizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf,
                                        FontListSizes* aSizes) const;
    virtual void AddSizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf,
                                        FontListSizes* aSizes) const;

private:
    friend class gfxDWriteFontFamily;

    nsresult GetFontSubstitutes();

    void GetDirectWriteSubstitutes();

    // search fonts system-wide for a given character, null otherwise
    virtual gfxFontEntry* GlobalFontFallback(const uint32_t aCh,
                                             int32_t aRunScript,
                                             const gfxFontStyle* aMatchStyle,
                                             uint32_t& aCmapCount,
                                             gfxFontFamily** aMatchedFamily);

    virtual bool UsesSystemFallback() { return true; }

    void GetFontsFromCollection(IDWriteFontCollection* aCollection);

#ifdef MOZ_BUNDLED_FONTS
    already_AddRefed<IDWriteFontCollection>
    CreateBundledFontsCollection(IDWriteFactory* aFactory);
#endif

    /**
     * Fonts listed in the registry as substitutes but for which no actual
     * font family is found.
     */
    nsTArray<nsString> mNonExistingFonts;

    typedef nsRefPtrHashtable<nsStringHashKey, gfxFontFamily> FontTable;

    /**
     * Table of font substitutes, we grab this from the registry to get
     * alternative font names.
     */
    FontTable mFontSubstitutes;

    bool mInitialized;
    virtual nsresult DelayedInitFontList();

    virtual already_AddRefed<FontInfoData> CreateFontInfoData();

    gfxFloat mForceGDIClassicMaxFontSize;

    // whether to use GDI font table access routines
    bool mGDIFontTableAccess;
    nsRefPtr<IDWriteGdiInterop> mGDIInterop;

    nsRefPtr<FontFallbackRenderer> mFallbackRenderer;
    nsRefPtr<IDWriteTextFormat>    mFallbackFormat;

    nsRefPtr<IDWriteFontCollection> mSystemFonts;
#ifdef MOZ_BUNDLED_FONTS
    nsRefPtr<IDWriteFontCollection> mBundledFonts;
#endif
};


#endif /* GFX_DWRITEFONTLIST_H */
