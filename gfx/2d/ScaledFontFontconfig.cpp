/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScaledFontFontconfig.h"
#include "UnscaledFontFreeType.h"
#include "Logging.h"

#ifdef USE_SKIA
#include "skia/include/ports/SkTypeface_cairo.h"
#endif

#include <fontconfig/fcfreetype.h>

namespace mozilla {
namespace gfx {

// On Linux and Android our "platform" font is a cairo_scaled_font_t and we use
// an SkFontHost implementation that allows Skia to render using this.
// This is mainly because FT_Face is not good for sharing between libraries, which
// is a requirement when we consider runtime switchable backends and so on
ScaledFontFontconfig::ScaledFontFontconfig(cairo_scaled_font_t* aScaledFont,
                                           FcPattern* aPattern,
                                           const RefPtr<UnscaledFont>& aUnscaledFont,
                                           Float aSize)
  : ScaledFontBase(aUnscaledFont, aSize),
    mPattern(aPattern)
{
  SetCairoScaledFont(aScaledFont);
  FcPatternReference(aPattern);
}

ScaledFontFontconfig::~ScaledFontFontconfig()
{
  FcPatternDestroy(mPattern);
}

#ifdef USE_SKIA
SkTypeface* ScaledFontFontconfig::GetSkTypeface()
{
  if (!mTypeface) {
    mTypeface = SkCreateTypefaceFromCairoFTFontWithFontconfig(mScaledFont, mPattern);
  }

  return mTypeface;
}
#endif

ScaledFontFontconfig::InstanceData::InstanceData(cairo_scaled_font_t* aScaledFont, FcPattern* aPattern)
  : mFlags(0)
  , mHintStyle(FC_HINT_NONE)
  , mSubpixelOrder(FC_RGBA_UNKNOWN)
  , mLcdFilter(FC_LCD_LEGACY)
{
  // Record relevant Fontconfig properties into instance data.
  FcBool autohint;
  if (FcPatternGetBool(aPattern, FC_AUTOHINT, 0, &autohint) == FcResultMatch && autohint) {
    mFlags |= AUTOHINT;
  }
  FcBool bitmap;
  if (FcPatternGetBool(aPattern, FC_EMBEDDED_BITMAP, 0, &bitmap) == FcResultMatch && bitmap) {
    mFlags |= EMBEDDED_BITMAP;
  }
  FcBool embolden;
  if (FcPatternGetBool(aPattern, FC_EMBOLDEN, 0, &embolden) == FcResultMatch && embolden) {
    mFlags |= EMBOLDEN;
  }
  FcBool vertical;
  if (FcPatternGetBool(aPattern, FC_VERTICAL_LAYOUT, 0, &vertical) == FcResultMatch && vertical) {
    mFlags |= VERTICAL_LAYOUT;
  }

  FcBool antialias;
  if (FcPatternGetBool(aPattern, FC_ANTIALIAS, 0, &antialias) != FcResultMatch || antialias) {
    mFlags |= ANTIALIAS;

    // Only record subpixel order and lcd filtering if antialiasing is enabled.
    int rgba;
    if (FcPatternGetInteger(aPattern, FC_RGBA, 0, &rgba) == FcResultMatch) {
      mSubpixelOrder = rgba;
    }
    int filter;
    if (FcPatternGetInteger(aPattern, FC_LCD_FILTER, 0, &filter) == FcResultMatch) {
      mLcdFilter = filter;
    }
  }

  cairo_font_options_t* fontOptions = cairo_font_options_create();
  cairo_scaled_font_get_font_options(aScaledFont, fontOptions);
  // For printer fonts, Cairo hint metrics and hinting will be disabled.
  // For other fonts, allow hint metrics and hinting.
  if (cairo_font_options_get_hint_metrics(fontOptions) != CAIRO_HINT_METRICS_OFF) {
    mFlags |= HINT_METRICS;

    FcBool hinting;
    if (FcPatternGetBool(aPattern, FC_HINTING, 0, &hinting) != FcResultMatch || hinting) {
      int hintstyle;
      if (FcPatternGetInteger(aPattern, FC_HINT_STYLE, 0, &hintstyle) != FcResultMatch) {
        hintstyle = FC_HINT_FULL;
      }
      mHintStyle = hintstyle;
    }
  }
  cairo_font_options_destroy(fontOptions);

  // Some fonts supply an adjusted size or otherwise use the font matrix for italicization.
  // Record the scale and the skew to accomodate both of these cases.
  cairo_matrix_t fontMatrix;
  cairo_scaled_font_get_font_matrix(aScaledFont, &fontMatrix);
  mScale = Float(fontMatrix.xx);
  mSkew = Float(fontMatrix.xy);
}

void
ScaledFontFontconfig::InstanceData::SetupPattern(FcPattern* aPattern) const
{
  if (mFlags & AUTOHINT) {
    FcPatternAddBool(aPattern, FC_AUTOHINT, FcTrue);
  }
  if (mFlags & EMBEDDED_BITMAP) {
    FcPatternAddBool(aPattern, FC_EMBEDDED_BITMAP, FcTrue);
  }
  if (mFlags & EMBOLDEN) {
    FcPatternAddBool(aPattern, FC_EMBOLDEN, FcTrue);
  }
  if (mFlags & VERTICAL_LAYOUT) {
    FcPatternAddBool(aPattern, FC_VERTICAL_LAYOUT, FcTrue);
  }

  if (mFlags & ANTIALIAS) {
    FcPatternAddBool(aPattern, FC_ANTIALIAS, FcTrue);
    if (mSubpixelOrder != FC_RGBA_UNKNOWN) {
      FcPatternAddInteger(aPattern, FC_RGBA, mSubpixelOrder);
    }
    if (mLcdFilter != FC_LCD_LEGACY) {
      FcPatternAddInteger(aPattern, FC_LCD_FILTER, mLcdFilter);
    }
  } else {
    FcPatternAddBool(aPattern, FC_ANTIALIAS, FcFalse);
  }

  if (mHintStyle) {
    FcPatternAddBool(aPattern, FC_HINTING, FcTrue);
    FcPatternAddInteger(aPattern, FC_HINT_STYLE, mHintStyle);
  } else {
    FcPatternAddBool(aPattern, FC_HINTING, FcFalse);
  }
}

void
ScaledFontFontconfig::InstanceData::SetupFontOptions(cairo_font_options_t* aFontOptions) const
{
  // Try to build a sane initial set of Cairo font options based on the Fontconfig
  // pattern.
  if (mFlags & HINT_METRICS) {
    // For regular (non-printer) fonts, enable hint metrics as well as hinting
    // and (possibly subpixel) antialiasing.
    cairo_font_options_set_hint_metrics(aFontOptions, CAIRO_HINT_METRICS_ON);

    cairo_hint_style_t hinting;
    switch (mHintStyle) {
    case FC_HINT_NONE:
      hinting = CAIRO_HINT_STYLE_NONE;
      break;
    case FC_HINT_SLIGHT:
      hinting = CAIRO_HINT_STYLE_SLIGHT;
      break;
    case FC_HINT_MEDIUM:
    default:
      hinting = CAIRO_HINT_STYLE_MEDIUM;
      break;
    case FC_HINT_FULL:
      hinting = CAIRO_HINT_STYLE_FULL;
      break;
    }
    cairo_font_options_set_hint_style(aFontOptions, hinting);

    if (mFlags & ANTIALIAS) {
      cairo_subpixel_order_t subpixel = CAIRO_SUBPIXEL_ORDER_DEFAULT;
      switch (mSubpixelOrder) {
      case FC_RGBA_RGB:
        subpixel = CAIRO_SUBPIXEL_ORDER_RGB;
        break;
      case FC_RGBA_BGR:
        subpixel = CAIRO_SUBPIXEL_ORDER_BGR;
        break;
      case FC_RGBA_VRGB:
        subpixel = CAIRO_SUBPIXEL_ORDER_VRGB;
        break;
      case FC_RGBA_VBGR:
        subpixel = CAIRO_SUBPIXEL_ORDER_VBGR;
        break;
      default:
        break;
      }
      if (subpixel != CAIRO_SUBPIXEL_ORDER_DEFAULT) {
        cairo_font_options_set_antialias(aFontOptions, CAIRO_ANTIALIAS_SUBPIXEL);
        cairo_font_options_set_subpixel_order(aFontOptions, subpixel);
      } else {
        cairo_font_options_set_antialias(aFontOptions, CAIRO_ANTIALIAS_GRAY);
      }
    } else {
      cairo_font_options_set_antialias(aFontOptions, CAIRO_ANTIALIAS_NONE);
    }
  } else {
    // For printer fonts, disable hint metrics and hinting. Don't allow subpixel
    // antialiasing.
    cairo_font_options_set_hint_metrics(aFontOptions, CAIRO_HINT_METRICS_OFF);
    cairo_font_options_set_hint_style(aFontOptions, CAIRO_HINT_STYLE_NONE);
    cairo_font_options_set_antialias(aFontOptions,
      mFlags & ANTIALIAS ? CAIRO_ANTIALIAS_GRAY : CAIRO_ANTIALIAS_NONE);
  }
}

void
ScaledFontFontconfig::InstanceData::SetupFontMatrix(cairo_matrix_t* aFontMatrix) const
{
  // Build a font matrix that will reproduce a possibly adjusted size
  // and any italics/skew. This is just the concatenation of a simple
  // scale matrix with a matrix that skews on the X axis.
  cairo_matrix_init(aFontMatrix, mScale, 0, mSkew, mScale, 0, 0);
}

bool
ScaledFontFontconfig::GetFontInstanceData(FontInstanceDataOutput aCb, void* aBaton)
{
  InstanceData instance(GetCairoScaledFont(), mPattern);

  aCb(reinterpret_cast<uint8_t*>(&instance), sizeof(instance), aBaton);
  return true;
}

already_AddRefed<ScaledFont>
UnscaledFontFontconfig::CreateScaledFont(Float aGlyphSize,
                                         const uint8_t* aInstanceData,
                                         uint32_t aInstanceDataLength)
{
  if (aInstanceDataLength < sizeof(ScaledFontFontconfig::InstanceData)) {
    gfxWarning() << "Fontconfig scaled font instance data is truncated.";
    return nullptr;
  }
  const ScaledFontFontconfig::InstanceData *instanceData =
    reinterpret_cast<const ScaledFontFontconfig::InstanceData*>(aInstanceData);
  return ScaledFontFontconfig::CreateFromInstanceData(*instanceData, this, aGlyphSize,
                                                      mNativeFontResource.get());
}

static cairo_user_data_key_t sNativeFontResourceKey;

static void
ReleaseNativeFontResource(void* aData)
{
  static_cast<NativeFontResource*>(aData)->Release();
}

already_AddRefed<ScaledFont>
ScaledFontFontconfig::CreateFromInstanceData(const InstanceData& aInstanceData,
                                             UnscaledFontFontconfig* aUnscaledFont,
                                             Float aSize,
                                             NativeFontResource* aNativeFontResource)
{
  FcPattern* pattern = FcPatternCreate();
  if (!pattern) {
    gfxWarning() << "Failing initializing Fontconfig pattern for scaled font";
    return nullptr;
  }
  if (aUnscaledFont->GetFace()) {
    FcPatternAddFTFace(pattern, FC_FT_FACE, aUnscaledFont->GetFace());
  } else {
    FcPatternAddString(pattern, FC_FILE, reinterpret_cast<const FcChar8*>(aUnscaledFont->GetFile()));
    FcPatternAddInteger(pattern, FC_INDEX, aUnscaledFont->GetIndex());
  }
  FcPatternAddDouble(pattern, FC_PIXEL_SIZE, aSize);
  aInstanceData.SetupPattern(pattern);

  cairo_font_face_t* font = cairo_ft_font_face_create_for_pattern(pattern);
  if (cairo_font_face_status(font) != CAIRO_STATUS_SUCCESS) {
    gfxWarning() << "Failed creating Cairo font face for Fontconfig pattern";
    FcPatternDestroy(pattern);
    return nullptr;
  }

  if (aNativeFontResource) {
    // Bug 1362117 - Cairo may keep the font face alive after the owning NativeFontResource
    // was freed. To prevent this, we must bind the NativeFontResource to the font face so that
    // it stays alive at least as long as the font face.
    if (cairo_font_face_set_user_data(font,
                                      &sNativeFontResourceKey,
                                      aNativeFontResource,
                                      ReleaseNativeFontResource) != CAIRO_STATUS_SUCCESS) {
      gfxWarning() << "Failed binding NativeFontResource to Cairo font face";
      cairo_font_face_destroy(font);
      FcPatternDestroy(pattern);
      return nullptr;
    }
    aNativeFontResource->AddRef();
  }

  cairo_matrix_t sizeMatrix;
  aInstanceData.SetupFontMatrix(&sizeMatrix);

  cairo_matrix_t identityMatrix;
  cairo_matrix_init_identity(&identityMatrix);

  cairo_font_options_t *fontOptions = cairo_font_options_create();
  aInstanceData.SetupFontOptions(fontOptions);

  cairo_scaled_font_t* cairoScaledFont =
    cairo_scaled_font_create(font, &sizeMatrix, &identityMatrix, fontOptions);

  cairo_font_options_destroy(fontOptions);
  cairo_font_face_destroy(font);

  if (cairo_scaled_font_status(cairoScaledFont) != CAIRO_STATUS_SUCCESS) {
    gfxWarning() << "Failed creating Cairo scaled font for font face";
    FcPatternDestroy(pattern);
    return nullptr;
  }

  RefPtr<ScaledFontFontconfig> scaledFont =
    new ScaledFontFontconfig(cairoScaledFont, pattern, aUnscaledFont, aSize);

  cairo_scaled_font_destroy(cairoScaledFont);
  FcPatternDestroy(pattern);

  return scaledFont.forget();
}

already_AddRefed<UnscaledFont>
UnscaledFontFontconfig::CreateFromFontDescriptor(const uint8_t* aData, uint32_t aDataLength)
{
  if (aDataLength < sizeof(FontDescriptor)) {
    gfxWarning() << "Fontconfig font descriptor is truncated.";
    return nullptr;
  }
  const FontDescriptor* desc = reinterpret_cast<const FontDescriptor*>(aData);
  if (desc->mPathLength < 1 ||
      desc->mPathLength > aDataLength - sizeof(FontDescriptor)) {
    gfxWarning() << "Pathname in Fontconfig font descriptor has invalid size.";
    return nullptr;
  }
  const char* path = reinterpret_cast<const char*>(aData + sizeof(FontDescriptor));
  if (path[desc->mPathLength - 1] != '\0') {
    gfxWarning() << "Pathname in Fontconfig font descriptor is not terminated.";
    return nullptr;
  }

  RefPtr<UnscaledFont> unscaledFont = new UnscaledFontFontconfig(path, desc->mIndex);
  return unscaledFont.forget();
}

} // namespace gfx
} // namespace mozilla
