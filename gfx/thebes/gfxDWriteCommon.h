/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_DWRITECOMMON_H
#define GFX_DWRITECOMMON_H

// Mozilla includes
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
DWriteFontStretchFromStretch(int16_t aStretch) 
{
    switch (aStretch) {
        case NS_FONT_STRETCH_ULTRA_CONDENSED:
            return DWRITE_FONT_STRETCH_ULTRA_CONDENSED;
        case NS_FONT_STRETCH_EXTRA_CONDENSED:
            return DWRITE_FONT_STRETCH_EXTRA_CONDENSED;
        case NS_FONT_STRETCH_CONDENSED:
            return DWRITE_FONT_STRETCH_CONDENSED;
        case NS_FONT_STRETCH_SEMI_CONDENSED:
            return DWRITE_FONT_STRETCH_SEMI_CONDENSED;
        case NS_FONT_STRETCH_NORMAL:
            return DWRITE_FONT_STRETCH_NORMAL;
        case NS_FONT_STRETCH_SEMI_EXPANDED:
            return DWRITE_FONT_STRETCH_SEMI_EXPANDED;
        case NS_FONT_STRETCH_EXPANDED:
            return DWRITE_FONT_STRETCH_EXPANDED;
        case NS_FONT_STRETCH_EXTRA_EXPANDED:
            return DWRITE_FONT_STRETCH_EXTRA_EXPANDED;
        case NS_FONT_STRETCH_ULTRA_EXPANDED:
            return DWRITE_FONT_STRETCH_ULTRA_EXPANDED;
        default:
            return DWRITE_FONT_STRETCH_UNDEFINED;
    }
}

static inline int16_t
FontStretchFromDWriteStretch(DWRITE_FONT_STRETCH aStretch) 
{
    switch (aStretch) {
        case DWRITE_FONT_STRETCH_ULTRA_CONDENSED:
            return NS_FONT_STRETCH_ULTRA_CONDENSED;
        case DWRITE_FONT_STRETCH_EXTRA_CONDENSED:
            return NS_FONT_STRETCH_EXTRA_CONDENSED;
        case DWRITE_FONT_STRETCH_CONDENSED:
            return NS_FONT_STRETCH_CONDENSED;
        case DWRITE_FONT_STRETCH_SEMI_CONDENSED:
            return NS_FONT_STRETCH_SEMI_CONDENSED;
        case DWRITE_FONT_STRETCH_NORMAL:
            return NS_FONT_STRETCH_NORMAL;
        case DWRITE_FONT_STRETCH_SEMI_EXPANDED:
            return NS_FONT_STRETCH_SEMI_EXPANDED;
        case DWRITE_FONT_STRETCH_EXPANDED:
            return NS_FONT_STRETCH_EXPANDED;
        case DWRITE_FONT_STRETCH_EXTRA_EXPANDED:
            return NS_FONT_STRETCH_EXTRA_EXPANDED;
        case DWRITE_FONT_STRETCH_ULTRA_EXPANDED:
            return NS_FONT_STRETCH_ULTRA_EXPANDED;
        default:
            return NS_FONT_STRETCH_NORMAL;
    }
}

struct ffReferenceKey
{
    FallibleTArray<uint8_t> *mArray;
    nsID mGUID;
};

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
            gfxWindowsPlatform::GetPlatform()->GetDWriteFactory()->
                RegisterFontFileLoader(mInstance);
        }
        return mInstance;
    }

    /**
     * Creates a IDWriteFontFile and IDWriteFontFileStream from aFontData.
     * aFontData will be empty on return as it swaps out the data.
     *
     * @param aFontData the font data for the custom font file
     * @param aFontFile out param for the created font file
     * @param aFontFileStream out param for the corresponding stream
     * @return HRESULT of internal calls
     */
    static HRESULT CreateCustomFontFile(FallibleTArray<uint8_t>& aFontData,
                                        IDWriteFontFile** aFontFile,
                                        IDWriteFontFileStream** aFontFileStream);

private:
    static IDWriteFontFileLoader* mInstance;
}; 

#endif /* GFX_DWRITECOMMON_H */
