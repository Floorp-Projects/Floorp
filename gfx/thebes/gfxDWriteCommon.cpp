/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxDWriteCommon.h"

IDWriteFontFileLoader* gfxDWriteFontFileLoader::mInstance = NULL;

HRESULT STDMETHODCALLTYPE
gfxDWriteFontFileLoader::CreateStreamFromKey(const void *fontFileReferenceKey, 
                                             UINT32 fontFileReferenceKeySize, 
                                             IDWriteFontFileStream **fontFileStream)
{
    if (!fontFileReferenceKey || !fontFileStream) {
        return E_POINTER;
    }

    *fontFileStream = 
        new gfxDWriteFontFileStream(
        static_cast<const ffReferenceKey*>(fontFileReferenceKey)->mArray);

    if (!*fontFileStream) {
        return E_OUTOFMEMORY;
    }
    (*fontFileStream)->AddRef();
    return S_OK;
}

gfxDWriteFontFileStream::gfxDWriteFontFileStream(FallibleTArray<PRUint8> *aData)
{
    mData.SwapElements(*aData);
}

gfxDWriteFontFileStream::~gfxDWriteFontFileStream()
{
}

HRESULT STDMETHODCALLTYPE
gfxDWriteFontFileStream::GetFileSize(UINT64 *fileSize)
{
    *fileSize = mData.Length();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE
gfxDWriteFontFileStream::GetLastWriteTime(UINT64 *lastWriteTime)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE
gfxDWriteFontFileStream::ReadFileFragment(const void **fragmentStart,
                                          UINT64 fileOffset,
                                          UINT64 fragmentSize,
                                          void **fragmentContext)
{
    // We are required to do bounds checking.
    if (fileOffset + fragmentSize > (UINT64)mData.Length()) {
        return E_FAIL;
    }
    // We should be alive for the duration of this.
    *fragmentStart = &mData[fileOffset];
    *fragmentContext = NULL;
    return S_OK;
}

void STDMETHODCALLTYPE
gfxDWriteFontFileStream::ReleaseFileFragment(void *fragmentContext)
{
}
