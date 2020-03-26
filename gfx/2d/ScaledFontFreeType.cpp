/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScaledFontFreeType.h"
#include "UnscaledFontFreeType.h"
#include "NativeFontResourceFreeType.h"
#include "Logging.h"
#include "mozilla/StaticPrefs_gfx.h"
#include "mozilla/webrender/WebRenderTypes.h"

#ifdef USE_SKIA
#  include "skia/include/ports/SkTypeface_cairo.h"
#endif

#include FT_MULTIPLE_MASTERS_H

namespace mozilla {
namespace gfx {

ScaledFontFreeType::ScaledFontFreeType(
    RefPtr<SharedFTFace>&& aFace, const RefPtr<UnscaledFont>& aUnscaledFont,
    Float aSize, bool aApplySyntheticBold)
    : ScaledFontBase(aUnscaledFont, aSize),
      mFace(std::move(aFace)),
      mApplySyntheticBold(aApplySyntheticBold) {}

bool ScaledFontFreeType::UseSubpixelPosition() const {
  return !MOZ_UNLIKELY(
             StaticPrefs::
                 gfx_text_subpixel_position_force_disabled_AtStartup()) &&
         FT_IS_SCALABLE(mFace->GetFace());
}

#ifdef USE_SKIA
SkTypeface* ScaledFontFreeType::CreateSkTypeface() {
  return SkCreateTypefaceFromCairoFTFont(mFace->GetFace(), mFace.get());
}

void ScaledFontFreeType::SetupSkFontDrawOptions(SkFont& aFont) {
  aFont.setSubpixel(UseSubpixelPosition());

  if (mApplySyntheticBold) {
    aFont.setEmbolden(true);
  }

  aFont.setEmbeddedBitmaps(true);
}
#endif

#ifdef USE_CAIRO_SCALED_FONT
cairo_font_face_t* ScaledFontFreeType::CreateCairoFontFace(
    cairo_font_options_t* aFontOptions) {
  cairo_font_options_set_hint_metrics(aFontOptions, CAIRO_HINT_METRICS_OFF);

  int loadFlags = FT_LOAD_NO_AUTOHINT | FT_LOAD_NO_HINTING;
  if (mFace->GetFace()->face_flags & FT_FACE_FLAG_TRICKY) {
    loadFlags &= ~FT_LOAD_NO_AUTOHINT;
  }

  unsigned int synthFlags = 0;
  if (mApplySyntheticBold) {
    synthFlags |= CAIRO_FT_SYNTHESIZE_BOLD;
  }

  return cairo_ft_font_face_create_for_ft_face(mFace->GetFace(), loadFlags,
                                               synthFlags, mFace.get());
}
#endif

bool ScaledFontFreeType::GetFontInstanceData(FontInstanceDataOutput aCb,
                                             void* aBaton) {
  std::vector<FontVariation> variations;
  if (HasVariationSettings()) {
    UnscaledFontFreeType::GetVariationSettingsFromFace(&variations,
                                                       mFace->GetFace());
  }

  InstanceData instance(this);
  aCb(reinterpret_cast<uint8_t*>(&instance), sizeof(instance),
      variations.data(), variations.size(), aBaton);
  return true;
}

bool ScaledFontFreeType::GetWRFontInstanceOptions(
    Maybe<wr::FontInstanceOptions>* aOutOptions,
    Maybe<wr::FontInstancePlatformOptions>* aOutPlatformOptions,
    std::vector<FontVariation>* aOutVariations) {
  wr::FontInstanceOptions options;
  options.render_mode = wr::FontRenderMode::Alpha;
  options.flags = wr::FontInstanceFlags{0};
  if (UseSubpixelPosition()) {
    options.flags |= wr::FontInstanceFlags::SUBPIXEL_POSITION;
  }
  options.flags |= wr::FontInstanceFlags::EMBEDDED_BITMAPS;
  options.bg_color = wr::ToColorU(DeviceColor());
  options.synthetic_italics =
      wr::DegreesToSyntheticItalics(GetSyntheticObliqueAngle());

  if (mApplySyntheticBold) {
    options.flags |= wr::FontInstanceFlags::SYNTHETIC_BOLD;
  }

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

ScaledFontFreeType::InstanceData::InstanceData(
    const wr::FontInstanceOptions* aOptions,
    const wr::FontInstancePlatformOptions* aPlatformOptions)
    : mApplySyntheticBold(false) {
  if (aOptions) {
    if (aOptions->flags & wr::FontInstanceFlags::SYNTHETIC_BOLD) {
      mApplySyntheticBold = true;
    }
  }
}

already_AddRefed<ScaledFont> UnscaledFontFreeType::CreateScaledFont(
    Float aGlyphSize, const uint8_t* aInstanceData,
    uint32_t aInstanceDataLength, const FontVariation* aVariations,
    uint32_t aNumVariations) {
  if (aInstanceDataLength < sizeof(ScaledFontFreeType::InstanceData)) {
    gfxWarning() << "FreeType scaled font instance data is truncated.";
    return nullptr;
  }
  const ScaledFontFreeType::InstanceData& instanceData =
      *reinterpret_cast<const ScaledFontFreeType::InstanceData*>(aInstanceData);

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

  // Only apply variations if we have an explicitly cloned face.
  if (aNumVariations > 0 && face != GetFace()) {
    ApplyVariationsToFace(aVariations, aNumVariations, face->GetFace());
  }

  RefPtr<ScaledFontFreeType> scaledFont = new ScaledFontFreeType(
      std::move(face), this, aGlyphSize, instanceData.mApplySyntheticBold);

  return scaledFont.forget();
}

already_AddRefed<ScaledFont> UnscaledFontFreeType::CreateScaledFontFromWRFont(
    Float aGlyphSize, const wr::FontInstanceOptions* aOptions,
    const wr::FontInstancePlatformOptions* aPlatformOptions,
    const FontVariation* aVariations, uint32_t aNumVariations) {
  ScaledFontFreeType::InstanceData instanceData(aOptions, aPlatformOptions);
  return CreateScaledFont(aGlyphSize, reinterpret_cast<uint8_t*>(&instanceData),
                          sizeof(instanceData), aVariations, aNumVariations);
}

bool ScaledFontFreeType::HasVariationSettings() {
  // Check if the FT face has been cloned.
  return mFace &&
         mFace->GetFace()->face_flags & FT_FACE_FLAG_MULTIPLE_MASTERS &&
         mFace !=
             static_cast<UnscaledFontFreeType*>(mUnscaledFont.get())->GetFace();
}

already_AddRefed<UnscaledFont> UnscaledFontFreeType::CreateFromFontDescriptor(
    const uint8_t* aData, uint32_t aDataLength, uint32_t aIndex) {
  if (aDataLength == 0) {
    gfxWarning() << "FreeType font descriptor is truncated.";
    return nullptr;
  }
  const char* path = reinterpret_cast<const char*>(aData);
  RefPtr<UnscaledFont> unscaledFont =
      new UnscaledFontFreeType(std::string(path, aDataLength), aIndex);
  return unscaledFont.forget();
}

}  // namespace gfx
}  // namespace mozilla
