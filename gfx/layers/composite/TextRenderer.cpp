/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TextRenderer.h"
#include "FontData.h"
#include "ConsolasFontData.h"
#include "png.h"
#include "mozilla/Base64.h"
#include "mozilla/layers/Compositor.h"
#include "mozilla/layers/TextureHost.h"
#include "mozilla/layers/Effects.h"

namespace mozilla {
namespace layers {

using namespace gfx;

const Float sBackgroundOpacity = 0.8f;
const SurfaceFormat sTextureFormat = SurfaceFormat::B8G8R8A8;

static void PNGAPI info_callback(png_structp png_ptr, png_infop info_ptr) {
  png_read_update_info(png_ptr, info_ptr);
}

static void PNGAPI row_callback(png_structp png_ptr, png_bytep new_row,
                                png_uint_32 row_num, int pass) {
  MOZ_ASSERT(sTextureFormat == SurfaceFormat::B8G8R8A8);

  TextRenderer::FontCache* cache =
      static_cast<TextRenderer::FontCache*>(png_get_progressive_ptr(png_ptr));

  uint32_t* dst =
      (uint32_t*)(cache->mMap.mData + cache->mMap.mStride * row_num);

  for (uint32_t x = 0; x < cache->mInfo->mTextureWidth; x++) {
    // We blend to a transparent white background, this will make text readable
    // even if it's on a dark background. Without hurting our ability to
    // interact with the content behind the text.
    Float alphaValue = Float(0xFF - new_row[x]) / 255.0f;
    Float baseValue = sBackgroundOpacity * (1.0f - alphaValue);
    Color pixelColor(baseValue, baseValue, baseValue, baseValue + alphaValue);
    dst[x] = pixelColor.ToABGR();
  }
}

TextRenderer::~TextRenderer() {}

TextRenderer::FontCache::~FontCache() { mGlyphBitmaps->Unmap(); }

void TextRenderer::RenderText(Compositor* aCompositor, const std::string& aText,
                              const IntPoint& aOrigin,
                              const Matrix4x4& aTransform, uint32_t aTextSize,
                              uint32_t aTargetPixelWidth, FontType aFontType) {
  const FontBitmapInfo* info = GetFontInfo(aFontType);

  // For now we only have a bitmap font with a 24px cell size, so we just
  // scale it up if the user wants larger text.
  Float scaleFactor = Float(aTextSize) / Float(info->mCellHeight);
  aTargetPixelWidth /= scaleFactor;

  RefPtr<TextureSource> src =
      RenderText(aCompositor, aText, aTargetPixelWidth, aFontType);
  if (!src) {
    return;
  }

  RefPtr<EffectRGB> effect = new EffectRGB(src, true, SamplingFilter::LINEAR);
  EffectChain chain;
  chain.mPrimaryEffect = effect;

  Matrix4x4 transform = aTransform;
  transform.PreScale(scaleFactor, scaleFactor, 1.0f);

  IntRect drawRect(aOrigin, src->GetSize());
  IntRect clip(-10000, -10000, 20000, 20000);
  aCompositor->DrawQuad(Rect(drawRect), clip, chain, 1.0f, transform);
}

IntSize TextRenderer::ComputeSurfaceSize(const std::string& aText,
                                         uint32_t aTargetPixelWidth,
                                         FontType aFontType) {
  if (!EnsureInitialized(aFontType)) {
    return IntSize();
  }

  FontCache* cache = mFonts[aFontType].get();
  const FontBitmapInfo* info = cache->mInfo;

  uint32_t numLines = 1;
  uint32_t maxWidth = 0;
  uint32_t lineWidth = 0;
  // Calculate the size of the surface needed to draw all the glyphs.
  for (uint32_t i = 0; i < aText.length(); i++) {
    // Insert a line break if we go past the TargetPixelWidth.
    // XXX - this has the downside of overrunning the intended width, causing
    // things at the edge of a window to be cut off.
    if (aText[i] == '\n' ||
        (aText[i] == ' ' && lineWidth > aTargetPixelWidth)) {
      numLines++;
      lineWidth = 0;
      continue;
    }

    lineWidth += info->GetGlyphWidth(aText[i]);
    maxWidth = std::max(lineWidth, maxWidth);
  }

  return IntSize(maxWidth, numLines * info->mCellHeight);
}

RefPtr<TextureSource> TextRenderer::RenderText(TextureSourceProvider* aProvider,
                                               const std::string& aText,
                                               uint32_t aTargetPixelWidth,
                                               FontType aFontType) {
  if (!EnsureInitialized(aFontType)) {
    return nullptr;
  }

  IntSize size = ComputeSurfaceSize(aText, aTargetPixelWidth, aFontType);

  // Create a DrawTarget to draw our glyphs to.
  RefPtr<DrawTarget> dt =
      Factory::CreateDrawTarget(BackendType::SKIA, size, sTextureFormat);

  RenderTextToDrawTarget(dt, aText, aTargetPixelWidth, aFontType);
  RefPtr<SourceSurface> surf = dt->Snapshot();
  RefPtr<DataSourceSurface> dataSurf = surf->GetDataSurface();
  RefPtr<DataTextureSource> src = aProvider->CreateDataTextureSource();

  if (!src->Update(dataSurf)) {
    // Upload failed.
    return nullptr;
  }

  return src;
}

void TextRenderer::RenderTextToDrawTarget(DrawTarget* aDrawTarget,
                                          const std::string& aText,
                                          uint32_t aTargetPixelWidth,
                                          FontType aFontType) {
  if (!EnsureInitialized(aFontType)) {
    return;
  }

  // Initialize the DrawTarget to transparent white.
  IntSize size = aDrawTarget->GetSize();
  aDrawTarget->FillRect(Rect(0, 0, size.width, size.height),
                        ColorPattern(Color(1.0, 1.0, 1.0, sBackgroundOpacity)),
                        DrawOptions(1.0, CompositionOp::OP_SOURCE));

  IntPoint currentPos;

  FontCache* cache = mFonts[aFontType].get();
  const FontBitmapInfo* info = cache->mInfo;

  const unsigned int kGlyphsPerLine = info->mTextureWidth / info->mCellWidth;

  // Copy our glyphs onto the DrawTarget.
  for (uint32_t i = 0; i < aText.length(); i++) {
    if (aText[i] == '\n' ||
        (aText[i] == ' ' && currentPos.x > int32_t(aTargetPixelWidth))) {
      currentPos.y += info->mCellHeight;
      currentPos.x = 0;
      continue;
    }

    uint32_t index = aText[i] - info->mFirstChar;
    uint32_t cellIndexY = index / kGlyphsPerLine;
    uint32_t cellIndexX = index - (cellIndexY * kGlyphsPerLine);
    uint32_t glyphWidth = info->GetGlyphWidth(aText[i]);
    IntRect srcRect(cellIndexX * info->mCellWidth,
                    cellIndexY * info->mCellHeight, glyphWidth,
                    info->mCellHeight);

    aDrawTarget->CopySurface(cache->mGlyphBitmaps, srcRect, currentPos);

    currentPos.x += glyphWidth;
  }
}

/* static */ const FontBitmapInfo* TextRenderer::GetFontInfo(FontType aType) {
  switch (aType) {
    case FontType::Default:
      return &sDefaultCompositorFont;
    case FontType::FixedWidth:
      return &sFixedWidthCompositorFont;
    default:
      MOZ_ASSERT_UNREACHABLE("unknown font type");
      return nullptr;
  }
}

bool TextRenderer::EnsureInitialized(FontType aType) {
  if (mFonts[aType]) {
    return true;
  }

  const FontBitmapInfo* info = GetFontInfo(aType);

  IntSize size(info->mTextureWidth, info->mTextureHeight);
  RefPtr<DataSourceSurface> surface =
      Factory::CreateDataSourceSurface(size, sTextureFormat);
  if (NS_WARN_IF(!surface)) {
    return false;
  }

  DataSourceSurface::MappedSurface map;
  if (NS_WARN_IF(!surface->Map(DataSourceSurface::MapType::READ_WRITE, &map))) {
    return false;
  }

  UniquePtr<FontCache> cache = MakeUnique<FontCache>();
  cache->mGlyphBitmaps = surface;
  cache->mMap = map;
  cache->mInfo = info;

  png_structp png_ptr = NULL;
  png_ptr =
      png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);

  png_set_progressive_read_fn(png_ptr, cache.get(), info_callback, row_callback,
                              nullptr);
  png_infop info_ptr = NULL;
  info_ptr = png_create_info_struct(png_ptr);

  png_process_data(png_ptr, info_ptr, (uint8_t*)info->mPNG, info->mPNGLength);

  png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);

  mFonts[aType] = std::move(cache);
  return true;
}

}  // namespace layers
}  // namespace mozilla
