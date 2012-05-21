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
#include "nsAutoPtr.h"
#include "cairo-features.h"
#include "gfxFontConstants.h"
#include "nsTArray.h"
#include "gfxWindowsPlatform.h"
#include "nsIUUIDGenerator.h"

#include <windows.h>
#include <dwrite.h>

static DWRITE_FONT_STRETCH
DWriteFontStretchFromStretch(PRInt16 aStretch) 
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

static PRInt16
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
    FallibleTArray<PRUint8> *mArray;
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
     * Important! Note the key here -has- to be a pointer to an
     * FallibleTArray<PRUint8>.
     */
    virtual HRESULT STDMETHODCALLTYPE 
        CreateStreamFromKey(void const* fontFileReferenceKey,
                            UINT32 fontFileReferenceKeySize,
                            OUT IDWriteFontFileStream** fontFileStream);

    /**
     * Gets the singleton loader instance. Note that when using this font
     * loader, the key must be a pointer to an FallibleTArray<PRUint8>. This
     * array will be empty when the function returns.
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

private:
    static IDWriteFontFileLoader* mInstance;
}; 

class gfxDWriteFontFileStream : public IDWriteFontFileStream
{
public:
    /**
     * Used by the FontFileLoader to create a new font stream,
     * this font stream is created from data in memory. The memory
     * passed may be released after object creation, it will be
     * copied internally.
     *
     * @param aData Font data
     */
    gfxDWriteFontFileStream(FallibleTArray<PRUint8> *aData);
    ~gfxDWriteFontFileStream();

    // IUnknown interface
    IFACEMETHOD(QueryInterface)(IID const& iid, OUT void** ppObject)
    {
        if (iid == __uuidof(IDWriteFontFileStream)) {
            *ppObject = static_cast<IDWriteFontFileStream*>(this);
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
        NS_PRECONDITION(PRInt32(mRefCnt) >= 0, "illegal refcnt");
        ++mRefCnt;
        return mRefCnt;
    }

    IFACEMETHOD_(ULONG, Release)()
    {
        NS_PRECONDITION(0 != mRefCnt, "dup release");
        --mRefCnt;
        if (mRefCnt == 0) {
            delete this;
            return 0;
        }
        return mRefCnt;
    }

    // IDWriteFontFileStream methods
    virtual HRESULT STDMETHODCALLTYPE ReadFileFragment(void const** fragmentStart,
                                                       UINT64 fileOffset,
                                                       UINT64 fragmentSize,
                                                       OUT void** fragmentContext);

    virtual void STDMETHODCALLTYPE ReleaseFileFragment(void* fragmentContext);

    virtual HRESULT STDMETHODCALLTYPE GetFileSize(OUT UINT64* fileSize);

    virtual HRESULT STDMETHODCALLTYPE GetLastWriteTime(OUT UINT64* lastWriteTime);

private:
    FallibleTArray<PRUint8> mData;
    nsAutoRefCnt mRefCnt;
}; 

#endif /* GFX_DWRITECOMMON_H */
