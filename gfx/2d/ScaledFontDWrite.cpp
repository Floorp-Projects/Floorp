/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScaledFontDWrite.h"
#include "PathD2D.h"
#include "DrawTargetD2D.h"
#include "Logging.h"

#include <vector>

namespace mozilla {
namespace gfx {

struct ffReferenceKey
{
    uint8_t *mData;
    uint32_t mSize;
};

class DWriteFontFileLoader : public IDWriteFontFileLoader
{
public:
  DWriteFontFileLoader()
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
    * ffReferenceKey object.
    */
  virtual HRESULT STDMETHODCALLTYPE 
    CreateStreamFromKey(void const* fontFileReferenceKey,
                        UINT32 fontFileReferenceKeySize,
                        OUT IDWriteFontFileStream** fontFileStream);

  /**
    * Gets the singleton loader instance. Note that when using this font
    * loader, the key must be a pointer to an FallibleTArray<uint8_t>. This
    * array will be empty when the function returns.
    */
  static IDWriteFontFileLoader* Instance()
  {
    if (!mInstance) {
      mInstance = new DWriteFontFileLoader();
      DrawTargetD2D::GetDWriteFactory()->
          RegisterFontFileLoader(mInstance);
    }
    return mInstance;
  }

private:
  static IDWriteFontFileLoader* mInstance;
}; 

class DWriteFontFileStream : public IDWriteFontFileStream
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
  DWriteFontFileStream(uint8_t *aData, uint32_t aSize);
  ~DWriteFontFileStream();

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
    ++mRefCnt;
    return mRefCnt;
  }

  IFACEMETHOD_(ULONG, Release)()
  {
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
  std::vector<uint8_t> mData;
  uint32_t mRefCnt;
};

static BYTE
GetSystemTextQuality()
{
  BOOL font_smoothing;
  UINT smoothing_type;

  if (!SystemParametersInfo(SPI_GETFONTSMOOTHING, 0, &font_smoothing, 0)) {
    return DEFAULT_QUALITY;
  }

  if (font_smoothing) {
      if (!SystemParametersInfo(SPI_GETFONTSMOOTHINGTYPE,
                                0, &smoothing_type, 0)) {
        return DEFAULT_QUALITY;
      }

      if (smoothing_type == FE_FONTSMOOTHINGCLEARTYPE) {
        return CLEARTYPE_QUALITY;
      }

      return ANTIALIASED_QUALITY;
  }

  return DEFAULT_QUALITY;
}

#define GASP_TAG 0x70736167
#define GASP_DOGRAY 0x2

static inline unsigned short
readShort(const char *aBuf)
{
  return (*aBuf << 8) | *(aBuf + 1);
}

static bool
DoGrayscale(IDWriteFontFace *aDWFace, unsigned int ppem)
{
  void *tableContext;
  char *tableData;
  UINT32 tableSize;
  BOOL exists;
  aDWFace->TryGetFontTable(GASP_TAG, (const void**)&tableData, &tableSize, &tableContext, &exists);

  if (exists) {
    if (tableSize < 4) {
      aDWFace->ReleaseFontTable(tableContext);
      return true;
    }
    struct gaspRange {
      unsigned short maxPPEM; // Stored big-endian
      unsigned short behavior; // Stored big-endian
    };
    unsigned short numRanges = readShort(tableData + 2);
    if (tableSize < (UINT)4 + numRanges * 4) {
      aDWFace->ReleaseFontTable(tableContext);
      return true;
    }
    gaspRange *ranges = (gaspRange *)(tableData + 4);
    for (int i = 0; i < numRanges; i++) {
      if (readShort((char*)&ranges[i].maxPPEM) > ppem) {
        if (!(readShort((char*)&ranges[i].behavior) & GASP_DOGRAY)) {
          aDWFace->ReleaseFontTable(tableContext);
          return false;
        }
        break;
      }
    }
    aDWFace->ReleaseFontTable(tableContext);
  }
  return true;
}

IDWriteFontFileLoader* DWriteFontFileLoader::mInstance = NULL;

HRESULT STDMETHODCALLTYPE
DWriteFontFileLoader::CreateStreamFromKey(const void *fontFileReferenceKey, 
                                          UINT32 fontFileReferenceKeySize, 
                                          IDWriteFontFileStream **fontFileStream)
{
  if (!fontFileReferenceKey || !fontFileStream) {
    return E_POINTER;
  }

  const ffReferenceKey *key = static_cast<const ffReferenceKey*>(fontFileReferenceKey);
  *fontFileStream = 
    new DWriteFontFileStream(key->mData, key->mSize);

  if (!*fontFileStream) {
    return E_OUTOFMEMORY;
  }
  (*fontFileStream)->AddRef();
  return S_OK;
}

DWriteFontFileStream::DWriteFontFileStream(uint8_t *aData, uint32_t aSize)
  : mRefCnt(0)
{
  mData.resize(aSize);
  memcpy(&mData.front(), aData, aSize);
}

DWriteFontFileStream::~DWriteFontFileStream()
{
}

HRESULT STDMETHODCALLTYPE
DWriteFontFileStream::GetFileSize(UINT64 *fileSize)
{
  *fileSize = mData.size();
  return S_OK;
}

HRESULT STDMETHODCALLTYPE
DWriteFontFileStream::GetLastWriteTime(UINT64 *lastWriteTime)
{
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE
DWriteFontFileStream::ReadFileFragment(const void **fragmentStart,
                                       UINT64 fileOffset,
                                       UINT64 fragmentSize,
                                       void **fragmentContext)
{
  // We are required to do bounds checking.
  if (fileOffset + fragmentSize > (UINT64)mData.size()) {
    return E_FAIL;
  }
  // We should be alive for the duration of this.
  *fragmentStart = &mData[fileOffset];
  *fragmentContext = NULL;
  return S_OK;
}

void STDMETHODCALLTYPE
DWriteFontFileStream::ReleaseFileFragment(void *fragmentContext)
{
}

ScaledFontDWrite::ScaledFontDWrite(uint8_t *aData, uint32_t aSize,
                                   uint32_t aIndex, Float aGlyphSize)
  : ScaledFontBase(aGlyphSize)
{
  IDWriteFactory *factory = DrawTargetD2D::GetDWriteFactory();

  ffReferenceKey key;
  key.mData = aData;
  key.mSize = aSize;

  RefPtr<IDWriteFontFile> fontFile;
  if (FAILED(factory->CreateCustomFontFileReference(&key, sizeof(ffReferenceKey), DWriteFontFileLoader::Instance(), byRef(fontFile)))) {
    gfxWarning() << "Failed to load font file from data!";
    return;
  }

  IDWriteFontFile *ff = fontFile;
  if (FAILED(factory->CreateFontFace(DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &ff, aIndex, DWRITE_FONT_SIMULATIONS_NONE, byRef(mFontFace)))) {
    gfxWarning() << "Failed to create font face from font file data!";
  }
}

TemporaryRef<Path>
ScaledFontDWrite::GetPathForGlyphs(const GlyphBuffer &aBuffer, const DrawTarget *aTarget)
{
  if (aTarget->GetType() != BACKEND_DIRECT2D) {
    return ScaledFontBase::GetPathForGlyphs(aBuffer, aTarget);
  }

  RefPtr<PathBuilder> pathBuilder = aTarget->CreatePathBuilder();

  PathBuilderD2D *pathBuilderD2D =
    static_cast<PathBuilderD2D*>(pathBuilder.get());

  CopyGlyphsToSink(aBuffer, pathBuilderD2D->GetSink());

  return pathBuilder->Finish();
}

void
ScaledFontDWrite::CopyGlyphsToBuilder(const GlyphBuffer &aBuffer, PathBuilder *aBuilder)
{
  // XXX - Check path builder type!
  PathBuilderD2D *pathBuilderD2D =
    static_cast<PathBuilderD2D*>(aBuilder);

  CopyGlyphsToSink(aBuffer, pathBuilderD2D->GetSink());
}

void
ScaledFontDWrite::CopyGlyphsToSink(const GlyphBuffer &aBuffer, ID2D1GeometrySink *aSink)
{
  std::vector<UINT16> indices;
  std::vector<FLOAT> advances;
  std::vector<DWRITE_GLYPH_OFFSET> offsets;
  indices.resize(aBuffer.mNumGlyphs);
  advances.resize(aBuffer.mNumGlyphs);
  offsets.resize(aBuffer.mNumGlyphs);

  memset(&advances.front(), 0, sizeof(FLOAT) * aBuffer.mNumGlyphs);
  for (unsigned int i = 0; i < aBuffer.mNumGlyphs; i++) {
    indices[i] = aBuffer.mGlyphs[i].mIndex;
    offsets[i].advanceOffset = aBuffer.mGlyphs[i].mPosition.x;
    offsets[i].ascenderOffset = -aBuffer.mGlyphs[i].mPosition.y;
  }

  mFontFace->GetGlyphRunOutline(mSize, &indices.front(), &advances.front(),
                                &offsets.front(), aBuffer.mNumGlyphs,
                                FALSE, FALSE, aSink);
}

bool
ScaledFontDWrite::GetFontFileData(FontFileDataOutput aDataCallback, void *aBaton)
{
  UINT32 fileCount = 0;
  mFontFace->GetFiles(&fileCount, nullptr);

  if (fileCount > 1) {
    MOZ_ASSERT(false);
    return false;
  }

  RefPtr<IDWriteFontFile> file;
  mFontFace->GetFiles(&fileCount, byRef(file));

  const void *referenceKey;
  UINT32 refKeySize;
  // XXX - This can currently crash for webfonts, as when we get the reference
  // key out of the file, that can be an invalid reference key for the loader
  // we use it with. The fix to this is not obvious but it will probably 
  // have to happen inside thebes.
  file->GetReferenceKey(&referenceKey, &refKeySize);

  RefPtr<IDWriteFontFileLoader> loader;
  file->GetLoader(byRef(loader));
  
  RefPtr<IDWriteFontFileStream> stream;
  loader->CreateStreamFromKey(referenceKey, refKeySize, byRef(stream));

  UINT64 fileSize;
  stream->GetFileSize(&fileSize);

  const void *fragmentStart;
  void *context;
  stream->ReadFileFragment(&fragmentStart, 0, fileSize, &context);

  aDataCallback((uint8_t*)fragmentStart, fileSize, mFontFace->GetIndex(), mSize, aBaton);

  stream->ReleaseFileFragment(context);

  return true;
}

AntialiasMode
ScaledFontDWrite::GetDefaultAAMode()
{
  AntialiasMode defaultMode = AA_SUBPIXEL;

  switch (GetSystemTextQuality()) {
  case CLEARTYPE_QUALITY:
    defaultMode = AA_SUBPIXEL;
    break;
  case ANTIALIASED_QUALITY:
    defaultMode = AA_GRAY;
    break;
  case DEFAULT_QUALITY:
    defaultMode = AA_NONE;
    break;
  }

  if (defaultMode == AA_GRAY) {
    if (!DoGrayscale(mFontFace, mSize)) {
      defaultMode = AA_NONE;
    }
  }
  return defaultMode;
}

}
}
