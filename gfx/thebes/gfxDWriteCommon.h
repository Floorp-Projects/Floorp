/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_DWRITECOMMON_H
#define GFX_DWRITECOMMON_H

// Mozilla includes
#include "mozilla/MemoryReporting.h"
#include "mozilla/FontPropertyTypes.h"
#include "nscore.h"
#include "nsIServiceManager.h"
#include "nsCOMPtr.h"
#include "cairo-features.h"
#include "gfxFontConstants.h"
#include "nsTArray.h"
#include "gfxWindowsPlatform.h"
#include "nsIUUIDGenerator.h"

#include <windows.h>
#include <dwrite.h>

static inline DWRITE_FONT_STRETCH
DWriteFontStretchFromStretch(mozilla::FontStretch aStretch)
{
    if (aStretch == mozilla::FontStretch::UltraCondensed()) {
        return DWRITE_FONT_STRETCH_ULTRA_CONDENSED;
    }
    if (aStretch == mozilla::FontStretch::ExtraCondensed()) {
        return DWRITE_FONT_STRETCH_EXTRA_CONDENSED;
    }
    if (aStretch == mozilla::FontStretch::Condensed()) {
        return DWRITE_FONT_STRETCH_CONDENSED;
    }
    if (aStretch == mozilla::FontStretch::SemiCondensed()) {
        return DWRITE_FONT_STRETCH_SEMI_CONDENSED;
    }
    if (aStretch == mozilla::FontStretch::Normal()) {
        return DWRITE_FONT_STRETCH_NORMAL;
    }
    if (aStretch == mozilla::FontStretch::SemiExpanded()) {
        return DWRITE_FONT_STRETCH_SEMI_EXPANDED;
    }
    if (aStretch == mozilla::FontStretch::Expanded()) {
        return DWRITE_FONT_STRETCH_EXPANDED;
    }
    if (aStretch == mozilla::FontStretch::ExtraExpanded()) {
        return DWRITE_FONT_STRETCH_EXTRA_EXPANDED;
    }
    if (aStretch == mozilla::FontStretch::UltraExpanded()) {
        return DWRITE_FONT_STRETCH_ULTRA_EXPANDED;
    }
    return DWRITE_FONT_STRETCH_UNDEFINED;
}

static inline mozilla::FontStretch
FontStretchFromDWriteStretch(DWRITE_FONT_STRETCH aStretch)
{
    switch (aStretch) {
        case DWRITE_FONT_STRETCH_ULTRA_CONDENSED:
            return mozilla::FontStretch::UltraCondensed();
        case DWRITE_FONT_STRETCH_EXTRA_CONDENSED:
            return mozilla::FontStretch::ExtraCondensed();
        case DWRITE_FONT_STRETCH_CONDENSED:
            return mozilla::FontStretch::Condensed();
        case DWRITE_FONT_STRETCH_SEMI_CONDENSED:
            return mozilla::FontStretch::SemiCondensed();
        case DWRITE_FONT_STRETCH_NORMAL:
            return mozilla::FontStretch::Normal();
        case DWRITE_FONT_STRETCH_SEMI_EXPANDED:
            return mozilla::FontStretch::SemiExpanded();
        case DWRITE_FONT_STRETCH_EXPANDED:
            return mozilla::FontStretch::Expanded();
        case DWRITE_FONT_STRETCH_EXTRA_EXPANDED:
            return mozilla::FontStretch::ExtraExpanded();
        case DWRITE_FONT_STRETCH_ULTRA_EXPANDED:
            return mozilla::FontStretch::UltraExpanded();
        default:
            return mozilla::FontStretch::Normal();
    }
}

class gfxDWriteFontFileLoader : public IDWriteFontFileLoader
{
public:
    gfxDWriteFontFileLoader()
    {
    }

    // IUnknown interface
    IFACEMETHOD(QueryInterface)(IID const& iid, OUT void** ppObject)
    {
        if (iid == __uuidof(IDWriteFontFileLoader)) {
            *ppObject = static_cast<IDWriteFontFileLoader*>(this);
            return S_OK;
        } else if (iid == __uuidof(IUnknown)) {
            *ppObject = static_cast<IUnknown*>(this);
            return S_OK;
        } else {
            return E_NOINTERFACE;
        }
    }

    IFACEMETHOD_(ULONG, AddRef)()
    {
        return 1;
    }

    IFACEMETHOD_(ULONG, Release)()
    {
        return 1;
    }

    // IDWriteFontFileLoader methods
    /**
     * Important! Note the key here -has- to be a pointer to a uint64_t.
     */
    virtual HRESULT STDMETHODCALLTYPE 
        CreateStreamFromKey(void const* fontFileReferenceKey,
                            UINT32 fontFileReferenceKeySize,
                            OUT IDWriteFontFileStream** fontFileStream);

    /**
     * Gets the singleton loader instance. Note that when using this font
     * loader, the key must be a pointer to a unint64_t.
     */
    static IDWriteFontFileLoader* Instance()
    {
        if (!mInstance) {
            mInstance = new gfxDWriteFontFileLoader();
            mozilla::gfx::Factory::GetDWriteFactory()->
                RegisterFontFileLoader(mInstance);
        }
        return mInstance;
    }

    /**
     * Creates a IDWriteFontFile and IDWriteFontFileStream from aFontData.
     * The data from aFontData will be copied internally, so the caller
     * is free to dispose of it once this method returns.
     *
     * @param aFontData the font data for the custom font file
     * @param aLength length of the font data
     * @param aFontFile out param for the created font file
     * @param aFontFileStream out param for the corresponding stream
     * @return HRESULT of internal calls
     */
    static HRESULT CreateCustomFontFile(const uint8_t* aFontData,
                                        uint32_t aLength,
                                        IDWriteFontFile** aFontFile,
                                        IDWriteFontFileStream** aFontFileStream);

    size_t SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const;

private:
    static IDWriteFontFileLoader* mInstance;
}; 

#endif /* GFX_DWRITECOMMON_H */
