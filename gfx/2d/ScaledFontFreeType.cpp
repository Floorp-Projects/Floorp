/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScaledFontFreeType.h"
#include "UnscaledFontFreeType.h"
#include "NativeFontResourceFreeType.h"
#include "Logging.h"
#include "StackArray.h"
#include "mozilla/webrender/WebRenderTypes.h"

#ifdef USE_SKIA
#include "skia/include/ports/SkTypeface_cairo.h"
#endif

#include FT_MULTIPLE_MASTERS_H

namespace mozilla {
namespace gfx {

// On Linux and Android our "platform" font is a cairo_scaled_font_t and we use
// an SkFontHost implementation that allows Skia to render using this.
// This is mainly because FT_Face is not good for sharing between libraries, which
// is a requirement when we consider runtime switchable backends and so on
ScaledFontFreeType::ScaledFontFreeType(cairo_scaled_font_t* aScaledFont,
                                       FT_Face aFace,
                                       const RefPtr<UnscaledFont>& aUnscaledFont,
                                       Float aSize)
  : ScaledFontBase(aUnscaledFont, aSize)
  , mFace(aFace)
{
  SetCairoScaledFont(aScaledFont);
}

#ifdef USE_SKIA
SkTypeface* ScaledFontFreeType::GetSkTypeface()
{
  if (!mTypeface) {
    mTypeface = SkCreateTypefaceFromCairoFTFont(mScaledFont);
  }

  return mTypeface;
}
#endif

bool
ScaledFontFreeType::GetFontInstanceData(FontInstanceDataOutput aCb, void* aBaton)
{
  std::vector<FontVariation> variations;
  if (HasVariationSettings()) {
    UnscaledFontFreeType::GetVariationSettingsFromFace(&variations, mFace);
  }

  aCb(nullptr, 0, variations.data(), variations.size(), aBaton);
  return true;
}

bool
ScaledFontFreeType::GetWRFontInstanceOptions(Maybe<wr::FontInstanceOptions>* aOutOptions,
                                             Maybe<wr::FontInstancePlatformOptions>* aOutPlatformOptions,
                                             std::vector<FontVariation>* aOutVariations)
{
  wr::FontInstanceOptions options;
  options.render_mode = wr::FontRenderMode::Alpha;
  // FIXME: Cairo-FT metrics are not compatible with subpixel positioning.
  // options.flags = wr::FontInstanceFlags::SUBPIXEL_POSITION;
  options.flags = 0;
  options.bg_color = wr::ToColorU(Color());

  wr::FontInstancePlatformOptions platformOptions;
  platformOptions.lcd_filter = wr::FontLCDFilter::None;
  platformOptions.hinting = wr::FontHinting::None;

  *aOutOptions = Some(options);
  *aOutPlatformOptions = Some(platformOptions);

  if (HasVariationSettings()) {
    UnscaledFontFreeType::GetVariationSettingsFromFace(aOutVariations, mFace);
  }

  return true;
}

static cairo_user_data_key_t sNativeFontResourceKey;

static void
ReleaseNativeFontResource(void* aData)
{
  static_cast<NativeFontResource*>(aData)->Release();
}

static cairo_user_data_key_t sFaceKey;

static void
ReleaseFace(void* aData)
{
  Factory::ReleaseFTFace(static_cast<FT_Face>(aData));
}

already_AddRefed<ScaledFont>
UnscaledFontFreeType::CreateScaledFont(Float aGlyphSize,
                                       const uint8_t* aInstanceData,
                                       uint32_t aInstanceDataLength,
                                       const FontVariation* aVariations,
                                       uint32_t aNumVariations)
{
  FT_Face face = GetFace();
  if (!face) {
    gfxWarning() << "Attempted to deserialize FreeType scaled font without FreeType face";
    return nullptr;
  }

  NativeFontResourceFreeType* nfr = static_cast<NativeFontResourceFreeType*>(mNativeFontResource.get());
  FT_Face varFace = nullptr;
  if (nfr && aNumVariations > 0) {
    varFace = nfr->CloneFace();
    if (varFace) {
      face = varFace;
    } else {
      gfxWarning() << "Failed cloning face for variations";
    }
  }

  StackArray<FT_Fixed, 32> coords(aNumVariations);
  for (uint32_t i = 0; i < aNumVariations; i++) {
    coords[i] = std::round(aVariations[i].mValue * 65536.0);
  }

  int flags = FT_LOAD_NO_AUTOHINT | FT_LOAD_NO_HINTING;
  cairo_font_face_t* font =
    cairo_ft_font_face_create_for_ft_face(face, flags, coords.data(), aNumVariations);
  if (cairo_font_face_status(font) != CAIRO_STATUS_SUCCESS) {
    gfxWarning() << "Failed creating Cairo font face for FreeType face";
    if (varFace) {
      Factory::ReleaseFTFace(varFace);
    }
    return nullptr;
  }


  if (nfr) {
    // Bug 1362117 - Cairo may keep the font face alive after the owning NativeFontResource
    // was freed. To prevent this, we must bind the NativeFontResource to the font face so that
    // it stays alive at least as long as the font face.
    nfr->AddRef();
    // Bug 1412545 - Setting Cairo font user data is not thread-safe. If FT_Faces match,
    // cairo_ft_font_face_create_for_ft_face may share Cairo faces. We need to lock setting user data
    // to prevent races if multiple threads are thus sharing the same Cairo face.
    FT_Library library = face->glyph->library;
    Factory::LockFTLibrary(library);
    cairo_status_t err = CAIRO_STATUS_SUCCESS;
    bool cleanupFace = false;
    if (varFace) {
      err = cairo_font_face_set_user_data(font,
                                          &sFaceKey,
                                          varFace,
                                          ReleaseFace);
    }

    if (err != CAIRO_STATUS_SUCCESS) {
      cleanupFace = true;
    } else {
      err = cairo_font_face_set_user_data(font,
                                          &sNativeFontResourceKey,
                                          nfr,
                                          ReleaseNativeFontResource);
    }
    Factory::UnlockFTLibrary(library);
    if (err != CAIRO_STATUS_SUCCESS) {
      gfxWarning() << "Failed binding NativeFontResource to Cairo font face";
      if (varFace && cleanupFace) {
        Factory::ReleaseFTFace(varFace);
      }
      nfr->Release();
      cairo_font_face_destroy(font);
      return nullptr;
    }
  }

  cairo_matrix_t sizeMatrix;
  cairo_matrix_init(&sizeMatrix, aGlyphSize, 0, 0, aGlyphSize, 0, 0);

  cairo_matrix_t identityMatrix;
  cairo_matrix_init_identity(&identityMatrix);

  cairo_font_options_t *fontOptions = cairo_font_options_create();
  cairo_font_options_set_hint_metrics(fontOptions, CAIRO_HINT_METRICS_OFF);

  cairo_scaled_font_t* cairoScaledFont =
    cairo_scaled_font_create(font, &sizeMatrix, &identityMatrix, fontOptions);

  cairo_font_options_destroy(fontOptions);

  cairo_font_face_destroy(font);

  if (cairo_scaled_font_status(cairoScaledFont) != CAIRO_STATUS_SUCCESS) {
    gfxWarning() << "Failed creating Cairo scaled font for font face";
    return nullptr;
  }

  RefPtr<ScaledFontFreeType> scaledFont =
    new ScaledFontFreeType(cairoScaledFont, face, this, aGlyphSize);

  cairo_scaled_font_destroy(cairoScaledFont);

  // Only apply variations if we have an explicitly cloned face. Otherwise,
  // if the pattern holds the pathname, Cairo will handle setting of variations.
  if (varFace) {
    ApplyVariationsToFace(aVariations, aNumVariations, varFace);
  }

  return scaledFont.forget();
}

bool
ScaledFontFreeType::HasVariationSettings()
{
  // Check if the FT face has been cloned.
  return mFace && mFace->face_flags & FT_FACE_FLAG_MULTIPLE_MASTERS &&
         mFace != static_cast<UnscaledFontFreeType*>(mUnscaledFont.get())->GetFace();
}

} // namespace gfx
} // namespace mozilla
