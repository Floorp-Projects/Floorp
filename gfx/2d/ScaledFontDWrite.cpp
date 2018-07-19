/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScaledFontDWrite.h"
#include "UnscaledFontDWrite.h"
#include "PathD2D.h"
#include "gfxFont.h"
#include "Logging.h"
#include "mozilla/FontPropertyTypes.h"
#include "mozilla/webrender/WebRenderTypes.h"

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
#include "dw-extra.h"
#endif

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
DWriteFontStretchFromStretch(FontStretch aStretch)
{
    if (aStretch == FontStretch::UltraCondensed()) {
        return DWRITE_FONT_STRETCH_ULTRA_CONDENSED;
    }
    if (aStretch == FontStretch::ExtraCondensed()) {
        return DWRITE_FONT_STRETCH_EXTRA_CONDENSED;
    }
    if (aStretch == FontStretch::Condensed()) {
        return DWRITE_FONT_STRETCH_CONDENSED;
    }
    if (aStretch == FontStretch::SemiCondensed()) {
        return DWRITE_FONT_STRETCH_SEMI_CONDENSED;
    }
    if (aStretch == FontStretch::Normal()) {
        return DWRITE_FONT_STRETCH_NORMAL;
    }
    if (aStretch == FontStretch::SemiExpanded()) {
        return DWRITE_FONT_STRETCH_SEMI_EXPANDED;
    }
    if (aStretch == FontStretch::Expanded()) {
        return DWRITE_FONT_STRETCH_EXPANDED;
    }
    if (aStretch == FontStretch::ExtraExpanded()) {
        return DWRITE_FONT_STRETCH_EXTRA_EXPANDED;
    }
    if (aStretch == FontStretch::UltraExpanded()) {
        return DWRITE_FONT_STRETCH_ULTRA_EXPANDED;
    }
    return DWRITE_FONT_STRETCH_UNDEFINED;
 }

ScaledFontDWrite::ScaledFontDWrite(IDWriteFontFace *aFontFace,
                                   const RefPtr<UnscaledFont>& aUnscaledFont,
                                   Float aSize,
                                   bool aUseEmbeddedBitmap,
                                   bool aForceGDIMode,
                                   IDWriteRenderingParams* aParams,
                                   Float aGamma,
                                   Float aContrast,
                                   const gfxFontStyle* aStyle)
    : ScaledFontBase(aUnscaledFont, aSize)
    , mFontFace(aFontFace)
    , mUseEmbeddedBitmap(aUseEmbeddedBitmap)
    , mForceGDIMode(aForceGDIMode)
    , mParams(aParams)
    , mGamma(aGamma)
    , mContrast(aContrast)
{
  if (aStyle) {
    mStyle = SkFontStyle(aStyle->weight.ToIntRounded(),
                         DWriteFontStretchFromStretch(aStyle->stretch),
                         // FIXME(jwatt): also use kOblique_Slant
                         aStyle->style ==  FontSlantStyle::Normal()?
                         SkFontStyle::kUpright_Slant : SkFontStyle::kItalic_Slant);
  }
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
    RefPtr<IDWriteFactory> factory = Factory::GetDWriteFactory();
    if (!factory) {
      return nullptr;
    }

    Float gamma = mGamma;
    // Skia doesn't support a gamma value outside of 0-4, so default to 2.2
    if (gamma < 0.0f || gamma > 4.0f) {
      gamma = 2.2f;
    }

    Float contrast = mContrast;
    // Skia doesn't support a contrast value outside of 0-1, so default to 1.0
    if (contrast < 0.0f || contrast > 1.0f) {
      contrast = 1.0f;
    }

    mTypeface = SkCreateTypefaceFromDWriteFont(factory, mFontFace, mStyle, mForceGDIMode, gamma, contrast);
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
ScaledFontDWrite::GetGlyphDesignMetrics(const uint16_t* aGlyphs,
                                        uint32_t aNumGlyphs,
                                        GlyphMetrics* aGlyphMetrics)
{
  DWRITE_FONT_METRICS fontMetrics;
  mFontFace->GetMetrics(&fontMetrics);

  vector<DWRITE_GLYPH_METRICS> metrics(aNumGlyphs);
  mFontFace->GetDesignGlyphMetrics(aGlyphs, aNumGlyphs, &metrics.front());

  Float scaleFactor = mSize / fontMetrics.designUnitsPerEm;

  for (uint32_t i = 0; i < aNumGlyphs; i++) {
    aGlyphMetrics[i].mXBearing = metrics[i].leftSideBearing * scaleFactor;
    aGlyphMetrics[i].mXAdvance = metrics[i].advanceWidth * scaleFactor;
    aGlyphMetrics[i].mYBearing = (metrics[i].topSideBearing -
                                  metrics[i].verticalOriginY) * scaleFactor;
    aGlyphMetrics[i].mYAdvance = metrics[i].advanceHeight * scaleFactor;
    aGlyphMetrics[i].mWidth = (metrics[i].advanceWidth -
                               metrics[i].leftSideBearing -
                               metrics[i].rightSideBearing) * scaleFactor;
    aGlyphMetrics[i].mHeight = (metrics[i].advanceHeight -
                                metrics[i].topSideBearing -
                                metrics[i].bottomSideBearing) * scaleFactor;
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
UnscaledFontDWrite::GetFontFileData(FontFileDataOutput aDataCallback, void *aBaton)
{
  UINT32 fileCount = 0;
  mFontFace->GetFiles(&fileCount, nullptr);

  if (fileCount > 1) {
    MOZ_ASSERT(false);
    return false;
  }

  if (!aDataCallback) {
    return true;
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

  aDataCallback((uint8_t*)fragmentStart, fileSize, mFontFace->GetIndex(), aBaton);

  stream->ReleaseFileFragment(context);

  return true;
}

static bool
GetDWriteName(RefPtr<IDWriteLocalizedStrings> aNames, std::vector<WCHAR>& aOutName)
{
  BOOL exists = false;
  UINT32 index = 0;
  HRESULT hr = aNames->FindLocaleName(L"en-us", &index, &exists);
  if (FAILED(hr)) {
    return false;
  }
  if (!exists) {
    // No english found, use whatever is first in the list.
    index = 0;
  }

  UINT32 length;
  hr = aNames->GetStringLength(index, &length);
  if (FAILED(hr)) {
    return false;
  }
  aOutName.resize(length + 1);
  hr = aNames->GetString(index, aOutName.data(), length + 1);
  return SUCCEEDED(hr);
}

static bool
GetDWriteFamilyName(const RefPtr<IDWriteFontFamily>& aFamily, std::vector<WCHAR>& aOutName)
{
  RefPtr<IDWriteLocalizedStrings> names;
  HRESULT hr = aFamily->GetFamilyNames(getter_AddRefs(names));
  if (FAILED(hr)) {
    return false;
  }
  return GetDWriteName(names, aOutName);
}

bool
UnscaledFontDWrite::GetWRFontDescriptor(WRFontDescriptorOutput aCb, void* aBaton)
{
  if (!mFont) {
    return false;
  }

  RefPtr<IDWriteFontFamily> family;
  HRESULT hr = mFont->GetFontFamily(getter_AddRefs(family));
  if (FAILED(hr)) {
    return false;
  }

  DWRITE_FONT_WEIGHT weight = mFont->GetWeight();
  DWRITE_FONT_STRETCH stretch = mFont->GetStretch();
  DWRITE_FONT_STYLE style = mFont->GetStyle();

  RefPtr<IDWriteFont> match;
  hr = family->GetFirstMatchingFont(weight, stretch, style, getter_AddRefs(match));
  if (FAILED(hr) ||
      match->GetWeight() != weight ||
      match->GetStretch() != stretch ||
      match->GetStyle() != style) {
    return false;
  }

  std::vector<WCHAR> familyName;
  if (!GetDWriteFamilyName(family, familyName)) {
    return false;
  }

  // The style information that identifies the font can be encoded easily in
  // less than 32 bits. Since the index is needed for font descriptors, only
  // the family name and style information, pass along the style in the index
  // data to avoid requiring a more complicated structure packing for it in
  // the data payload.
  uint32_t index = weight | (stretch << 16) | (style << 24);
  aCb(reinterpret_cast<const uint8_t*>(familyName.data()),
      (familyName.size() - 1) * sizeof(WCHAR),
      index, aBaton);
  return true;
}

// Helper for ScaledFontDWrite::GetFontInstanceData: if the font has variation
// axes, get their current values into the aOutput vector.
static void
GetVariationsFromFontFace(IDWriteFontFace* aFace,
                          std::vector<FontVariation>* aOutput)
{
  RefPtr<IDWriteFontFace5> ff5;
  aFace->QueryInterface(__uuidof(IDWriteFontFace5), (void**)getter_AddRefs(ff5));
  if (!ff5 || !ff5->HasVariations()) {
    return;
  }

  uint32_t count = ff5->GetFontAxisValueCount();
  if (!count) {
    return;
  }

  RefPtr<IDWriteFontResource> res;
  if (FAILED(ff5->GetFontResource(getter_AddRefs(res)))) {
    return;
  }

  std::vector<DWRITE_FONT_AXIS_VALUE> values(count);
  if (FAILED(ff5->GetFontAxisValues(values.data(), count))) {
    return;
  }

  aOutput->reserve(count);
  for (uint32_t i = 0; i < count; i++) {
    DWRITE_FONT_AXIS_ATTRIBUTES attr = res->GetFontAxisAttributes(i);
    if (attr & DWRITE_FONT_AXIS_ATTRIBUTES_VARIABLE) {
      float v = values[i].value;
      uint32_t t = TRUETYPE_TAG(uint8_t(values[i].axisTag),
                                uint8_t(values[i].axisTag >> 8),
                                uint8_t(values[i].axisTag >> 16),
                                uint8_t(values[i].axisTag >> 24));
      aOutput->push_back(FontVariation{uint32_t(t), float(v)});
    }
  }
}

bool
ScaledFontDWrite::GetFontInstanceData(FontInstanceDataOutput aCb, void* aBaton)
{
  InstanceData instance(this);

  // If the font has variations, get the list of axis values.
  std::vector<FontVariation> variations;
  GetVariationsFromFontFace(mFontFace, &variations);

  aCb(reinterpret_cast<uint8_t*>(&instance), sizeof(instance),
      variations.data(), variations.size(), aBaton);

  return true;
}

bool
ScaledFontDWrite::GetWRFontInstanceOptions(Maybe<wr::FontInstanceOptions>* aOutOptions,
                                           Maybe<wr::FontInstancePlatformOptions>* aOutPlatformOptions,
                                           std::vector<FontVariation>* aOutVariations)
{
  wr::FontInstanceOptions options;
  options.render_mode = wr::ToFontRenderMode(GetDefaultAAMode());
  options.flags = wr::FontInstanceFlags::SUBPIXEL_POSITION;
  if (mFontFace->GetSimulations() & DWRITE_FONT_SIMULATIONS_BOLD) {
    options.flags |= wr::FontInstanceFlags::SYNTHETIC_BOLD;
  }
  if (UseEmbeddedBitmaps()) {
    options.flags |= wr::FontInstanceFlags::EMBEDDED_BITMAPS;
  }
  if (ForceGDIMode()) {
    options.flags |= wr::FontInstanceFlags::FORCE_GDI;
  }
  options.bg_color = wr::ToColorU(Color());
  options.synthetic_italics = wr::DegreesToSyntheticItalics(GetSyntheticObliqueAngle());

  wr::FontInstancePlatformOptions platformOptions;
  platformOptions.gamma = uint16_t(std::round(mGamma * 100.0f));
  platformOptions.contrast = uint16_t(std::round(std::min(mContrast, 1.0f) * 100.0f));

  *aOutOptions = Some(options);
  *aOutPlatformOptions = Some(platformOptions);
  return true;
}

// Helper for UnscaledFontDWrite::CreateScaledFont: create a clone of the
// given IDWriteFontFace, with specified variation-axis values applied.
// Returns nullptr in case of failure.
static already_AddRefed<IDWriteFontFace5>
CreateFaceWithVariations(IDWriteFontFace* aFace,
                         const FontVariation* aVariations,
                         uint32_t aNumVariations)
{
  auto makeDWriteAxisTag = [](uint32_t aTag) {
    return DWRITE_MAKE_FONT_AXIS_TAG((aTag >> 24) & 0xff,
                                     (aTag >> 16) & 0xff,
                                     (aTag >> 8) & 0xff,
                                     aTag & 0xff);
  };

  RefPtr<IDWriteFontFace5> ff5;
  aFace->QueryInterface(__uuidof(IDWriteFontFace5), (void**)getter_AddRefs(ff5));
  if (!ff5) {
    return nullptr;
  }

  DWRITE_FONT_SIMULATIONS sims = aFace->GetSimulations();
  RefPtr<IDWriteFontResource> res;
  if (FAILED(ff5->GetFontResource(getter_AddRefs(res)))) {
    return nullptr;
  }

  std::vector<DWRITE_FONT_AXIS_VALUE> fontAxisValues;
  fontAxisValues.reserve(aNumVariations);
  for (uint32_t i = 0; i < aNumVariations; i++) {
    DWRITE_FONT_AXIS_VALUE axisValue = {
      makeDWriteAxisTag(aVariations[i].mTag),
      aVariations[i].mValue
    };
    fontAxisValues.push_back(axisValue);
  }

  RefPtr<IDWriteFontFace5> newFace;
  if (FAILED(res->CreateFontFace(sims,
                                 fontAxisValues.data(),
                                 fontAxisValues.size(),
                                 getter_AddRefs(newFace)))) {
    return nullptr;
  }

  return newFace.forget();
}

already_AddRefed<ScaledFont>
UnscaledFontDWrite::CreateScaledFont(Float aGlyphSize,
                                     const uint8_t* aInstanceData,
                                     uint32_t aInstanceDataLength,
                                     const FontVariation* aVariations,
                                     uint32_t aNumVariations)
{
  if (aInstanceDataLength < sizeof(ScaledFontDWrite::InstanceData)) {
    gfxWarning() << "DWrite scaled font instance data is truncated.";
    return nullptr;
  }

  IDWriteFontFace* face = mFontFace;

  // If variations are required, we create a separate IDWriteFontFace5 with
  // the requested settings applied.
  RefPtr<IDWriteFontFace5> ff5;
  if (aNumVariations) {
    ff5 = CreateFaceWithVariations(mFontFace, aVariations, aNumVariations);
    if (ff5) {
      face = ff5;
    } else {
      gfxWarning() << "Failed to create IDWriteFontFace5 with variations.";
    }
  }

  const ScaledFontDWrite::InstanceData *instanceData =
    reinterpret_cast<const ScaledFontDWrite::InstanceData*>(aInstanceData);
  RefPtr<ScaledFontBase> scaledFont =
    new ScaledFontDWrite(face, this, aGlyphSize,
                         instanceData->mUseEmbeddedBitmap,
                         instanceData->mForceGDIMode,
                         nullptr,
                         instanceData->mGamma,
                         instanceData->mContrast);

  if (mNeedsCairo && !scaledFont->PopulateCairoScaledFont()) {
    gfxWarning() << "Unable to create cairo scaled font DWrite font.";
    return nullptr;
  }

  return scaledFont.forget();
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
