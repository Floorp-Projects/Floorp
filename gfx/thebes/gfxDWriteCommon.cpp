/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxDWriteCommon.h"

#include <unordered_map>

#include "mozilla/Atomics.h"
#include "mozilla/gfx/Logging.h"

static mozilla::Atomic<uint64_t> sNextFontFileKey;
static std::unordered_map<uint64_t, IDWriteFontFileStream*> sFontFileStreams;

IDWriteFontFileLoader* gfxDWriteFontFileLoader::mInstance = nullptr;

class gfxDWriteFontFileStream final : public IDWriteFontFileStream
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
  gfxDWriteFontFileStream(FallibleTArray<uint8_t> *aData,
                          uint64_t aFontFileKey);
  ~gfxDWriteFontFileStream();

  // IUnknown interface
  IFACEMETHOD(QueryInterface)(IID const& iid, OUT void** ppObject)
  {
    if (iid == __uuidof(IDWriteFontFileStream)) {
      *ppObject = static_cast<IDWriteFontFileStream*>(this);
      return S_OK;
    }
    else if (iid == __uuidof(IUnknown)) {
      *ppObject = static_cast<IUnknown*>(this);
      return S_OK;
    }
    else {
      return E_NOINTERFACE;
    }
  }

  IFACEMETHOD_(ULONG, AddRef)()
  {
    NS_PRECONDITION(int32_t(mRefCnt) >= 0, "illegal refcnt");
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
  FallibleTArray<uint8_t> mData;
  nsAutoRefCnt mRefCnt;
  uint64_t mFontFileKey;
};

gfxDWriteFontFileStream::gfxDWriteFontFileStream(FallibleTArray<uint8_t> *aData,
                                                 uint64_t aFontFileKey)
  : mFontFileKey(aFontFileKey)
{
  mData.SwapElements(*aData);
}

gfxDWriteFontFileStream::~gfxDWriteFontFileStream()
{
  sFontFileStreams.erase(mFontFileKey);
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
  *fragmentContext = nullptr;
  return S_OK;
}

void STDMETHODCALLTYPE
gfxDWriteFontFileStream::ReleaseFileFragment(void *fragmentContext)
{
}

HRESULT STDMETHODCALLTYPE
gfxDWriteFontFileLoader::CreateStreamFromKey(const void *fontFileReferenceKey, 
                                             UINT32 fontFileReferenceKeySize, 
                                             IDWriteFontFileStream **fontFileStream)
{
    if (!fontFileReferenceKey || !fontFileStream) {
        return E_POINTER;
    }

    uint64_t fontFileKey = *static_cast<const uint64_t*>(fontFileReferenceKey);
    auto found = sFontFileStreams.find(fontFileKey);
    if (found == sFontFileStreams.end()) {
      *fontFileStream = nullptr;
      return E_FAIL;
    }

    found->second->AddRef();
    *fontFileStream = found->second;
    return S_OK;
}

/* static */
HRESULT
gfxDWriteFontFileLoader::CreateCustomFontFile(FallibleTArray<uint8_t>& aFontData,
                                              IDWriteFontFile** aFontFile,
                                              IDWriteFontFileStream** aFontFileStream)
{
  MOZ_ASSERT(aFontFile);
  MOZ_ASSERT(aFontFileStream);

  IDWriteFactory *factory = mozilla::gfx::Factory::GetDWriteFactory();
  if (!factory) {
    gfxCriticalError() << "Failed to get DWrite Factory in CreateCustomFontFile.";
    return E_FAIL;
  }

  uint64_t fontFileKey = sNextFontFileKey++;
  RefPtr<IDWriteFontFileStream> ffsRef = new gfxDWriteFontFileStream(&aFontData, fontFileKey);
  sFontFileStreams[fontFileKey] = ffsRef;

  RefPtr<IDWriteFontFile> fontFile;
  HRESULT hr = factory->CreateCustomFontFileReference(&fontFileKey, sizeof(fontFileKey), Instance(), getter_AddRefs(fontFile));
  if (FAILED(hr)) {
    NS_WARNING("Failed to load font file from data!");
    return hr;
  }

  fontFile.forget(aFontFile);
  ffsRef.forget(aFontFileStream);

  return S_OK;
}
