/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScaledFontFreeType.h"
#include "UnscaledFontFreeType.h"
#include "NativeFontResourceFreeType.h"
#include "Logging.h"
#include "mozilla/webrender/WebRenderTypes.h"

#ifdef USE_SKIA
#  include "skia/include/ports/SkTypeface_cairo.h"
#endif

#include FT_MULTIPLE_MASTERS_H

namespace mozilla {
namespace gfx {

// On Linux and Android our "platform" font is a cairo_scaled_font_t and we use
// an SkFontHost implementation that allows Skia to render using this.
// This is mainly because FT_Face is not good for sharing between libraries,
// which is a requirement when we consider runtime switchable backends and so on
ScaledFontFreeType::ScaledFontFreeType(
    cairo_scaled_font_t* aScaledFont, RefPtr<SharedFTFace>&& aFace,
    const RefPtr<UnscaledFont>& aUnscaledFont, Float aSize)
    : ScaledFontBase(aUnscaledFont, aSize), mFace(std::move(aFace)) {
  SetCairoScaledFont(aScaledFont);
}

#ifdef USE_SKIA
SkTypeface* ScaledFontFreeType::CreateSkTypeface() {
  return SkCreateTypefaceFromCairoFTFont(mFace->GetFace(), mFace.get());
}

void ScaledFontFreeType::SetupSkFontDrawOptions(SkFont& aFont) {
  // SkFontHost_cairo does not support subpixel text positioning
  aFont.setSubpixel(false);

  aFont.setEmbeddedBitmaps(true);
}
#endif

bool ScaledFontFreeType::GetFontInstanceData(FontInstanceDataOutput aCb,
                                             void* aBaton) {
  std::vector<FontVariation> variations;
  if (HasVariationSettings()) {
    UnscaledFontFreeType::GetVariationSettingsFromFace(&variations,
                                                       mFace->GetFace());
  }

  aCb(nullptr, 0, variations.data(), variations.size(), aBaton);
  return true;
}

bool ScaledFontFreeType::GetWRFontInstanceOptions(
    Maybe<wr::FontInstanceOptions>* aOutOptions,
    Maybe<wr::FontInstancePlatformOptions>* aOutPlatformOptions,
    std::vector<FontVariation>* aOutVariations) {
  wr::FontInstanceOptions options;
  options.render_mode = wr::FontRenderMode::Alpha;
  // FIXME: Cairo-FT metrics are not compatible with subpixel positioning.
  // options.flags = wr::FontInstanceFlags_SUBPIXEL_POSITION;
  options.flags = wr::FontInstanceFlags{0};
  options.flags |= wr::FontInstanceFlags_EMBEDDED_BITMAPS;
  options.bg_color = wr::ToColorU(Color());
  options.synthetic_italics =
      wr::DegreesToSyntheticItalics(GetSyntheticObliqueAngle());

  wr::FontInstancePlatformOptions platformOptions;
  platformOptions.lcd_filter = wr::FontLCDFilter::None;
  platformOptions.hinting = wr::FontHinting::None;

  *aOutOptions = Some(options);
  *aOutPlatformOptions = Some(platformOptions);

  if (HasVariationSettings()) {
    UnscaledFontFreeType::GetVariationSettingsFromFace(aOutVariations,
                                                       mFace->GetFace());
  }

  return true;
}

already_AddRefed<ScaledFont> UnscaledFontFreeType::CreateScaledFont(
    Float aGlyphSize, const uint8_t* aInstanceData,
    uint32_t aInstanceDataLength, const FontVariation* aVariations,
    uint32_t aNumVariations) {
  RefPtr<SharedFTFace> face(InitFace());
  if (!face) {
    gfxWarning() << "Attempted to deserialize FreeType scaled font without "
                    "FreeType face";
    return nullptr;
  }

  if (aNumVariations > 0 && face->GetData()) {
    if (RefPtr<SharedFTFace> varFace = face->GetData()->CloneFace()) {
      face = varFace;
    }
  }

  int flags = FT_LOAD_NO_AUTOHINT | FT_LOAD_NO_HINTING;
  if (face->GetFace()->face_flags & FT_FACE_FLAG_TRICKY) {
    flags &= ~FT_LOAD_NO_AUTOHINT;
  }
  cairo_font_face_t* font = cairo_ft_font_face_create_for_ft_face(
      face->GetFace(), flags, 0, face.get());
  if (cairo_font_face_status(font) != CAIRO_STATUS_SUCCESS) {
    gfxWarning() << "Failed creating Cairo font face for FreeType face";
    return nullptr;
  }

  cairo_matrix_t sizeMatrix;
  cairo_matrix_init(&sizeMatrix, aGlyphSize, 0, 0, aGlyphSize, 0, 0);

  cairo_matrix_t identityMatrix;
  cairo_matrix_init_identity(&identityMatrix);

  cairo_font_options_t* fontOptions = cairo_font_options_create();
  cairo_font_options_set_hint_metrics(fontOptions, CAIRO_HINT_METRICS_OFF);

  cairo_scaled_font_t* cairoScaledFont =
      cairo_scaled_font_create(font, &sizeMatrix, &identityMatrix, fontOptions);

  cairo_font_options_destroy(fontOptions);

  cairo_font_face_destroy(font);

  if (cairo_scaled_font_status(cairoScaledFont) != CAIRO_STATUS_SUCCESS) {
    gfxWarning() << "Failed creating Cairo scaled font for font face";
    return nullptr;
  }

  // Only apply variations if we have an explicitly cloned face.
  if (aNumVariations > 0 && face != GetFace()) {
    ApplyVariationsToFace(aVariations, aNumVariations, face->GetFace());
  }

  RefPtr<ScaledFontFreeType> scaledFont = new ScaledFontFreeType(
      cairoScaledFont, std::move(face), this, aGlyphSize);

  cairo_scaled_font_destroy(cairoScaledFont);

  return scaledFont.forget();
}

bool ScaledFontFreeType::HasVariationSettings() {
  // Check if the FT face has been cloned.
  return mFace &&
         mFace->GetFace()->face_flags & FT_FACE_FLAG_MULTIPLE_MASTERS &&
         mFace !=
             static_cast<UnscaledFontFreeType*>(mUnscaledFont.get())->GetFace();
}

}  // namespace gfx
}  // namespace mozilla
