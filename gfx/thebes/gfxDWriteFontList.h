/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_DWRITEFONTLIST_H
#define GFX_DWRITEFONTLIST_H

#include "mozilla/FontPropertyTypes.h"
#include "mozilla/MemoryReporting.h"
#include "gfxDWriteCommon.h"
#include "dwrite_3.h"

 // Currently, we build with WINVER=0x601 (Win7), which means newer
 // declarations in dwrite_3.h will not be visible. Also, we don't
 // yet have the Fall Creators Update SDK available on build machines,
 // so even with updated WINVER, some of the interfaces we need would
 // not be present.
 // To work around this, until the build environment is updated,
 // we #include an extra header that contains copies of the relevant
 // classes/interfaces we need.
#if !defined(__MINGW32__) && WINVER < 0x0A00
#include "mozilla/gfx/dw-extra.h"
#endif

#include "gfxFont.h"
#include "gfxUserFontSet.h"
#include "cairo-win32.h"

#include "gfxPlatformFontList.h"
#include "gfxPlatform.h"
#include <algorithm>

#include "mozilla/gfx/UnscaledFontDWrite.h"


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
    typedef mozilla::FontWeight FontWeight;

    /**
     * Constructs a new DWriteFont Family.
     *
     * \param aName Name identifying the family
     * \param aFamily IDWriteFontFamily object representing the directwrite
     * family object.
     */
    gfxDWriteFontFamily(const nsAString& aName,
                        IDWriteFontFamily *aFamily,
                        bool aIsSystemFontFamily = false)
      : gfxFontFamily(aName), mDWFamily(aFamily),
        mIsSystemFontFamily(aIsSystemFontFamily), mForceGDIClassic(false) {}
    virtual ~gfxDWriteFontFamily();
    
    void FindStyleVariations(FontInfoData *aFontInfoData = nullptr) final;

    void LocalizedName(nsAString& aLocalizedName) final;

    void ReadFaceNames(gfxPlatformFontList *aPlatformFontList,
                       bool aNeedFullnamePostscriptNames,
                       FontInfoData *aFontInfoData = nullptr) final;

    void SetForceGDIClassic(bool aForce) { mForceGDIClassic = aForce; }

    void AddSizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf,
                                FontListSizes* aSizes) const final;
    void AddSizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf,
                                FontListSizes* aSizes) const final;

    bool FilterForFontList(nsAtom* aLangGroup,
                           const nsACString& aGeneric) const final {
        return !IsSymbolFontFamily();
    }

protected:
    // helper for FilterForFontList
    bool IsSymbolFontFamily() const;

    /** This font family's directwrite fontfamily object */
    RefPtr<IDWriteFontFamily> mDWFamily;
    bool mIsSystemFontFamily;
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
                       IDWriteFont *aFont,
                       bool aIsSystemFont = false)
      : gfxFontEntry(aFaceName), mFont(aFont), mFontFile(nullptr),
        mIsSystemFont(aIsSystemFont), mForceGDIClassic(false),
        mHasVariations(false), mHasVariationsInitialized(false)
    {
        DWRITE_FONT_STYLE dwriteStyle = aFont->GetStyle();
        mStyle = (dwriteStyle == DWRITE_FONT_STYLE_ITALIC ?
                  NS_FONT_STYLE_ITALIC :
                  (dwriteStyle == DWRITE_FONT_STYLE_OBLIQUE ?
                   NS_FONT_STYLE_OBLIQUE : NS_FONT_STYLE_NORMAL));
        mStretch = FontStretchFromDWriteStretch(aFont->GetStretch());
        uint16_t weight = NS_ROUNDUP(aFont->GetWeight() - 50, 100);

        weight = std::max<uint16_t>(100, weight);
        weight = std::min<uint16_t>(900, weight);
        mWeight = FontWeight(weight);

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
     * \param aStyle italic or oblique of font
     */
    gfxDWriteFontEntry(const nsAString& aFaceName,
                              IDWriteFont *aFont,
                              FontWeight aWeight,
                              int16_t aStretch,
                              uint8_t aStyle)
      : gfxFontEntry(aFaceName), mFont(aFont), mFontFile(nullptr),
        mIsSystemFont(false), mForceGDIClassic(false),
        mHasVariations(false), mHasVariationsInitialized(false)
    {
        mWeight = aWeight;
        mStretch = aStretch;
        mStyle = aStyle;
        mIsLocalUserFont = true;
        mIsCJK = UNINITIALIZED_VALUE;
    }

    /**
     * Constructs a font entry using a font file.
     *
     * \param aFaceName The name of the corresponding font face.
     * \param aFontFile DirectWrite fontfile object
     * \param aFontFileStream DirectWrite fontfile stream object
     * \param aWeight Weight of the font
     * \param aStretch Stretch of the font
     * \param aStyle italic or oblique of font
     */
    gfxDWriteFontEntry(const nsAString& aFaceName,
                              IDWriteFontFile *aFontFile,
                              IDWriteFontFileStream *aFontFileStream,
                              FontWeight aWeight,
                              int16_t aStretch,
                              uint8_t aStyle)
      : gfxFontEntry(aFaceName), mFont(nullptr),
        mFontFile(aFontFile), mFontFileStream(aFontFileStream),
        mIsSystemFont(false), mForceGDIClassic(false),
        mHasVariations(false), mHasVariationsInitialized(false)
    {
        mWeight = aWeight;
        mStretch = aStretch;
        mStyle = aStyle;
        mIsDataUserFont = true;
        mIsCJK = UNINITIALIZED_VALUE;
    }

    gfxFontEntry* Clone() const override;

    virtual ~gfxDWriteFontEntry();

    virtual hb_blob_t* GetFontTable(uint32_t aTableTag) override;

    nsresult ReadCMAP(FontInfoData *aFontInfoData = nullptr);

    bool IsCJKFont();

    bool HasVariations() override;
    void GetVariationAxes(nsTArray<gfxFontVariationAxis>& aAxes) override;
    void GetVariationInstances(nsTArray<gfxFontVariationInstance>& aInstances) override;

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
                                   nsTArray<uint8_t>& aBuffer) override;

    virtual gfxFont *CreateFontInstance(const gfxFontStyle *aFontStyle,
                                        bool aNeedsBold);
    
    nsresult CreateFontFace(
        IDWriteFontFace **aFontFace,
        const nsTArray<gfxFontVariation>* aVariations = nullptr,
        DWRITE_FONT_SIMULATIONS aSimulations = DWRITE_FONT_SIMULATIONS_NONE);

    static bool InitLogFont(IDWriteFont *aFont, LOGFONTW *aLogFont);

    /**
     * A fontentry only needs to have either of these. If it has both only
     * the IDWriteFont will be used.
     */
    RefPtr<IDWriteFont> mFont;
    RefPtr<IDWriteFontFile> mFontFile;

    // For custom fonts, we hold a reference to the IDWriteFontFileStream for
    // for the IDWriteFontFile, so that the data is available.
    RefPtr<IDWriteFontFileStream> mFontFileStream;

    // font face corresponding to the mFont/mFontFile *without* any DWrite
    // style simulations applied
    RefPtr<IDWriteFontFace> mFontFace;
    // Extended fontface interface if supported, else null
    RefPtr<IDWriteFontFace5> mFontFace5;

    DWRITE_FONT_FACE_TYPE mFaceType;

    int8_t mIsCJK;
    bool mIsSystemFont;
    bool mForceGDIClassic;
    bool mHasVariations;
    bool mHasVariationsInitialized;

    mozilla::ThreadSafeWeakPtr<mozilla::gfx::UnscaledFontDWrite> mUnscaledFont;
    mozilla::ThreadSafeWeakPtr<mozilla::gfx::UnscaledFontDWrite> mUnscaledFontBold;
};

// custom text renderer used to determine the fallback font for a given char
class DWriteFontFallbackRenderer final : public IDWriteTextRenderer
{
public:
    explicit DWriteFontFallbackRenderer(IDWriteFactory *aFactory)
        : mRefCount(0)
    {
        HRESULT hr = S_OK;

        hr = aFactory->GetSystemFontCollection(getter_AddRefs(mSystemFonts));
        NS_ASSERTION(SUCCEEDED(hr), "GetSystemFontCollection failed!");
    }

    ~DWriteFontFallbackRenderer()
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
    RefPtr<IDWriteFontCollection> mSystemFonts;
    nsString mFamilyName;
};



class gfxDWriteFontList : public gfxPlatformFontList {
public:
    gfxDWriteFontList();

    static gfxDWriteFontList* PlatformFontList() {
        return static_cast<gfxDWriteFontList*>(sPlatformFontList);
    }

    // initialize font lists
    virtual nsresult InitFontListForPlatform() override;

    gfxFontFamily* CreateFontFamily(const nsAString& aName) const override;

    virtual gfxFontEntry* LookupLocalFont(const nsAString& aFontName,
                                          FontWeight aWeight,
                                          int16_t aStretch,
                                          uint8_t aStyle);

    virtual gfxFontEntry* MakePlatformFont(const nsAString& aFontName,
                                           FontWeight aWeight,
                                           int16_t aStretch,
                                           uint8_t aStyle,
                                           const uint8_t* aFontData,
                                           uint32_t aLength);
    
    bool GetStandardFamilyName(const nsAString& aFontName,
                                 nsAString& aFamilyName);

    IDWriteGdiInterop *GetGDIInterop() { return mGDIInterop; }
    bool UseGDIFontTableAccess() { return mGDIFontTableAccess; }

    bool FindAndAddFamilies(const nsAString& aFamily,
                            nsTArray<gfxFontFamily*>* aOutput,
                            FindFamiliesFlags aFlags,
                            gfxFontStyle* aStyle = nullptr,
                            gfxFloat aDevToCssSize = 1.0) override;

    gfxFloat GetForceGDIClassicMaxFontSize() { return mForceGDIClassicMaxFontSize; }

    virtual void AddSizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf,
                                        FontListSizes* aSizes) const;
    virtual void AddSizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf,
                                        FontListSizes* aSizes) const;

protected:
    virtual gfxFontFamily*
    GetDefaultFontForPlatform(const gfxFontStyle* aStyle) override;

    // attempt to use platform-specific fallback for the given character,
    // return null if no usable result found
    gfxFontEntry*
    PlatformGlobalFontFallback(const uint32_t aCh,
                               Script aRunScript,
                               const gfxFontStyle* aMatchStyle,
                               gfxFontFamily** aMatchedFamily) override;

private:
    friend class gfxDWriteFontFamily;

    nsresult GetFontSubstitutes();

    void GetDirectWriteSubstitutes();

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

    /**
     * Table of font substitutes, we grab this from the registry to get
     * alternative font names.
     */
    FontFamilyTable mFontSubstitutes;

    virtual already_AddRefed<FontInfoData> CreateFontInfoData();

    gfxFloat mForceGDIClassicMaxFontSize;

    // whether to use GDI font table access routines
    bool mGDIFontTableAccess;
    RefPtr<IDWriteGdiInterop> mGDIInterop;

    RefPtr<DWriteFontFallbackRenderer> mFallbackRenderer;
    RefPtr<IDWriteTextFormat>    mFallbackFormat;

    RefPtr<IDWriteFontCollection> mSystemFonts;
#ifdef MOZ_BUNDLED_FONTS
    RefPtr<IDWriteFontCollection> mBundledFonts;
#endif
};


#endif /* GFX_DWRITEFONTLIST_H */
