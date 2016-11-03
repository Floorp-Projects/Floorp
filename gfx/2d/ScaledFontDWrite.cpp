/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DrawTargetD2D1.h"
#include "ScaledFontDWrite.h"
#include "PathD2D.h"
#include "gfxFont.h"

using namespace std;

#ifdef USE_SKIA
#include "PathSkia.h"
#include "skia/include/core/SkPaint.h"
#include "skia/include/core/SkPath.h"
#include "skia/include/ports/SkTypeface_win.h"
#endif

#include <vector>

#ifdef USE_CAIRO_SCALED_FONT
#include "cairo-win32.h"
#endif

#include "HelpersWinFonts.h"

namespace mozilla {
namespace gfx {

#define GASP_TAG 0x70736167
#define GASP_DOGRAY 0x2

static inline unsigned short
readShort(const char *aBuf)
{
  return (*aBuf << 8) | *(aBuf + 1);
}

static bool
DoGrayscale(IDWriteFontFace *aDWFace, Float ppem)
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

ScaledFontDWrite::ScaledFontDWrite(IDWriteFontFace *aFontFace, Float aSize,
                                   bool aUseEmbeddedBitmap, bool aForceGDIMode,
                                   const gfxFontStyle* aStyle)
    : ScaledFontBase(aSize)
    , mFontFace(aFontFace)
    , mUseEmbeddedBitmap(aUseEmbeddedBitmap)
    , mForceGDIMode(aForceGDIMode)
{
  mStyle = SkFontStyle(aStyle->weight,
                       DWriteFontStretchFromStretch(aStyle->stretch),
                       aStyle->style == NS_FONT_STYLE_NORMAL ?
                       SkFontStyle::kUpright_Slant : SkFontStyle::kItalic_Slant);
}

already_AddRefed<Path>
ScaledFontDWrite::GetPathForGlyphs(const GlyphBuffer &aBuffer, const DrawTarget *aTarget)
{
  if (aTarget->GetBackendType() != BackendType::DIRECT2D && aTarget->GetBackendType() != BackendType::DIRECT2D1_1) {
    return ScaledFontBase::GetPathForGlyphs(aBuffer, aTarget);
  }

  RefPtr<PathBuilder> pathBuilder = aTarget->CreatePathBuilder();

  PathBuilderD2D *pathBuilderD2D =
    static_cast<PathBuilderD2D*>(pathBuilder.get());

  CopyGlyphsToSink(aBuffer, pathBuilderD2D->GetSink());

  return pathBuilder->Finish();
}


#ifdef USE_SKIA
SkTypeface*
ScaledFontDWrite::GetSkTypeface()
{
  if (!mTypeface) {
    IDWriteFactory *factory = DrawTargetD2D1::GetDWriteFactory();
    if (!factory) {
      return nullptr;
    }

    mTypeface = SkCreateTypefaceFromDWriteFont(factory, mFontFace, mStyle);
  }
  return mTypeface;
}
#endif

void
ScaledFontDWrite::CopyGlyphsToBuilder(const GlyphBuffer &aBuffer, PathBuilder *aBuilder, const Matrix *aTransformHint)
{
  BackendType backendType = aBuilder->GetBackendType();
  if (backendType != BackendType::DIRECT2D && backendType != BackendType::DIRECT2D1_1) {
    ScaledFontBase::CopyGlyphsToBuilder(aBuffer, aBuilder, aTransformHint);
    return;
  }

  PathBuilderD2D *pathBuilderD2D =
    static_cast<PathBuilderD2D*>(aBuilder);

  if (pathBuilderD2D->IsFigureActive()) {
    gfxCriticalNote << "Attempting to copy glyphs to PathBuilderD2D with active figure.";
  }

  CopyGlyphsToSink(aBuffer, pathBuilderD2D->GetSink());
}

void
ScaledFontDWrite::GetGlyphDesignMetrics(const uint16_t* aGlyphs, uint32_t aNumGlyphs, GlyphMetrics* aGlyphMetrics)
{
  DWRITE_FONT_METRICS fontMetrics;
  mFontFace->GetMetrics(&fontMetrics);

  vector<DWRITE_GLYPH_METRICS> metrics(aNumGlyphs);
  mFontFace->GetDesignGlyphMetrics(aGlyphs, aNumGlyphs, &metrics.front());

  Float designUnitCorrection = 1.f / fontMetrics.designUnitsPerEm;

  for (uint32_t i = 0; i < aNumGlyphs; i++) {
    aGlyphMetrics[i].mXBearing = metrics[i].leftSideBearing * designUnitCorrection * mSize;
    aGlyphMetrics[i].mXAdvance = metrics[i].advanceWidth * designUnitCorrection * mSize;
    aGlyphMetrics[i].mYBearing = metrics[i].topSideBearing * designUnitCorrection * mSize;
    aGlyphMetrics[i].mYAdvance = metrics[i].advanceHeight * designUnitCorrection * mSize;
    aGlyphMetrics[i].mWidth = (metrics[i].advanceHeight - metrics[i].topSideBearing - metrics[i].bottomSideBearing) *
                              designUnitCorrection * mSize;
    aGlyphMetrics[i].mHeight = (metrics[i].topSideBearing - metrics[i].verticalOriginY) * designUnitCorrection * mSize;
  }
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

  HRESULT hr =
    mFontFace->GetGlyphRunOutline(mSize, &indices.front(), &advances.front(),
                                  &offsets.front(), aBuffer.mNumGlyphs,
                                  FALSE, FALSE, aSink);
  if (FAILED(hr)) {
    gfxCriticalNote << "Failed to copy glyphs to geometry sink. Code: " << hexa(hr);
  }
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
  mFontFace->GetFiles(&fileCount, getter_AddRefs(file));

  const void *referenceKey;
  UINT32 refKeySize;
  // XXX - This can currently crash for webfonts, as when we get the reference
  // key out of the file, that can be an invalid reference key for the loader
  // we use it with. The fix to this is not obvious but it will probably 
  // have to happen inside thebes.
  file->GetReferenceKey(&referenceKey, &refKeySize);

  RefPtr<IDWriteFontFileLoader> loader;
  file->GetLoader(getter_AddRefs(loader));
  
  RefPtr<IDWriteFontFileStream> stream;
  loader->CreateStreamFromKey(referenceKey, refKeySize, getter_AddRefs(stream));

  UINT64 fileSize64;
  stream->GetFileSize(&fileSize64);
  if (fileSize64 > UINT32_MAX) {
    MOZ_ASSERT(false);
    return false;
  }
  
  uint32_t fileSize = static_cast<uint32_t>(fileSize64);
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
  AntialiasMode defaultMode = GetSystemDefaultAAMode();

  if (defaultMode == AntialiasMode::GRAY) {
    if (!DoGrayscale(mFontFace, mSize)) {
      defaultMode = AntialiasMode::NONE;
    }
  }
  return defaultMode;
}

#ifdef USE_CAIRO_SCALED_FONT
cairo_font_face_t*
ScaledFontDWrite::GetCairoFontFace()
{
  if (!mFontFace) {
    return nullptr;
  }

  return cairo_dwrite_font_face_create_for_dwrite_fontface(nullptr, mFontFace);
}
#endif

}
}
