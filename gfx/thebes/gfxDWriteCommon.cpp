/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxDWriteCommon.h"

#include <unordered_map>

#include "mozilla/Atomics.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/gfx/Logging.h"

class gfxDWriteFontFileStream;

static mozilla::StaticMutex sFontFileStreamsMutex;
static uint64_t sNextFontFileKey = 0;
static std::unordered_map<uint64_t, gfxDWriteFontFileStream*> sFontFileStreams;

IDWriteFontFileLoader* gfxDWriteFontFileLoader::mInstance = nullptr;

class gfxDWriteFontFileStream final : public IDWriteFontFileStream {
 public:
  /**
   * Used by the FontFileLoader to create a new font stream,
   * this font stream is created from data in memory. The memory
   * passed may be released after object creation, it will be
   * copied internally.
   *
   * @param aData Font data
   */
  gfxDWriteFontFileStream(const uint8_t* aData, uint32_t aLength,
                          uint64_t aFontFileKey);
  ~gfxDWriteFontFileStream();

  // IUnknown interface
  IFACEMETHOD(QueryInterface)(IID const& iid, OUT void** ppObject) {
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

  IFACEMETHOD_(ULONG, AddRef)() {
    MOZ_ASSERT(int32_t(mRefCnt) >= 0, "illegal refcnt");
    return ++mRefCnt;
  }

  IFACEMETHOD_(ULONG, Release)() {
    MOZ_ASSERT(0 != mRefCnt, "dup release");
    uint32_t count = --mRefCnt;
    if (count == 0) {
      // Avoid locking unless necessary. Verify the refcount hasn't changed
      // while locked. Delete within the scope of the lock when zero.
      mozilla::StaticMutexAutoLock lock(sFontFileStreamsMutex);
      if (0 != mRefCnt) {
        return mRefCnt;
      }
      delete this;
    }
    return count;
  }

  // IDWriteFontFileStream methods
  virtual HRESULT STDMETHODCALLTYPE
  ReadFileFragment(void const** fragmentStart, UINT64 fileOffset,
                   UINT64 fragmentSize, OUT void** fragmentContext);

  virtual void STDMETHODCALLTYPE ReleaseFileFragment(void* fragmentContext);

  virtual HRESULT STDMETHODCALLTYPE GetFileSize(OUT UINT64* fileSize);

  virtual HRESULT STDMETHODCALLTYPE GetLastWriteTime(OUT UINT64* lastWriteTime);

  size_t SizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const {
    return mData.ShallowSizeOfExcludingThis(mallocSizeOf);
  }

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const {
    return mallocSizeOf(this) + SizeOfExcludingThis(mallocSizeOf);
  }

 private:
  FallibleTArray<uint8_t> mData;
  mozilla::Atomic<uint32_t> mRefCnt;
  uint64_t mFontFileKey;
};

gfxDWriteFontFileStream::gfxDWriteFontFileStream(const uint8_t* aData,
                                                 uint32_t aLength,
                                                 uint64_t aFontFileKey)
    : mFontFileKey(aFontFileKey) {
  // If this fails, mData will remain empty. That's OK: GetFileSize()
  // will then return 0, etc., and the font just won't load.
  if (!mData.AppendElements(aData, aLength, mozilla::fallible_t())) {
    NS_WARNING("Failed to store data in gfxDWriteFontFileStream");
  }
}

gfxDWriteFontFileStream::~gfxDWriteFontFileStream() {
  sFontFileStreams.erase(mFontFileKey);
}

HRESULT STDMETHODCALLTYPE
gfxDWriteFontFileStream::GetFileSize(UINT64* fileSize) {
  *fileSize = mData.Length();
  return S_OK;
}

HRESULT STDMETHODCALLTYPE
gfxDWriteFontFileStream::GetLastWriteTime(UINT64* lastWriteTime) {
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE gfxDWriteFontFileStream::ReadFileFragment(
    const void** fragmentStart, UINT64 fileOffset, UINT64 fragmentSize,
    void** fragmentContext) {
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
gfxDWriteFontFileStream::ReleaseFileFragment(void* fragmentContext) {}

HRESULT STDMETHODCALLTYPE gfxDWriteFontFileLoader::CreateStreamFromKey(
    const void* fontFileReferenceKey, UINT32 fontFileReferenceKeySize,
    IDWriteFontFileStream** fontFileStream) {
  if (!fontFileReferenceKey || !fontFileStream) {
    return E_POINTER;
  }

  mozilla::StaticMutexAutoLock lock(sFontFileStreamsMutex);
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
gfxDWriteFontFileLoader::CreateCustomFontFile(
    const uint8_t* aFontData, uint32_t aLength, IDWriteFontFile** aFontFile,
    IDWriteFontFileStream** aFontFileStream) {
  MOZ_ASSERT(aFontFile);
  MOZ_ASSERT(aFontFileStream);

  RefPtr<IDWriteFactory> factory = mozilla::gfx::Factory::GetDWriteFactory();
  if (!factory) {
    gfxCriticalError()
        << "Failed to get DWrite Factory in CreateCustomFontFile.";
    return E_FAIL;
  }

  sFontFileStreamsMutex.Lock();
  uint64_t fontFileKey = sNextFontFileKey++;
  RefPtr<gfxDWriteFontFileStream> ffsRef =
      new gfxDWriteFontFileStream(aFontData, aLength, fontFileKey);
  sFontFileStreams[fontFileKey] = ffsRef;
  sFontFileStreamsMutex.Unlock();

  RefPtr<IDWriteFontFile> fontFile;
  HRESULT hr = factory->CreateCustomFontFileReference(
      &fontFileKey, sizeof(fontFileKey), Instance(), getter_AddRefs(fontFile));
  if (FAILED(hr)) {
    NS_WARNING("Failed to load font file from data!");
    return hr;
  }

  fontFile.forget(aFontFile);
  ffsRef.forget(aFontFileStream);

  return S_OK;
}

size_t gfxDWriteFontFileLoader::SizeOfIncludingThis(
    mozilla::MallocSizeOf mallocSizeOf) const {
  size_t sizes = mallocSizeOf(this);

  // We are a singleton type that is effective owner of sFontFileStreams.
  MOZ_ASSERT(this == mInstance);
  for (const auto& entry : sFontFileStreams) {
    gfxDWriteFontFileStream* fileStream = entry.second;
    sizes += fileStream->SizeOfIncludingThis(mallocSizeOf);
  }

  return sizes;
}
