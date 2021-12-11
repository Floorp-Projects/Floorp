/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxFT2FontBase.h"
#include "gfxFT2Utils.h"
#include "mozilla/Likely.h"

#ifdef HAVE_FONTCONFIG_FCFREETYPE_H
#  include <fontconfig/fcfreetype.h>
#endif

#include "ft2build.h"
#include FT_MULTIPLE_MASTERS_H

#include "prlink.h"

uint32_t gfxFT2LockedFace::GetGlyph(uint32_t aCharCode) {
  if (MOZ_UNLIKELY(!mFace)) return 0;

#ifdef HAVE_FONTCONFIG_FCFREETYPE_H
  // FcFreeTypeCharIndex will search starting from the most recently
  // selected charmap.  This can cause non-determistic behavior when more
  // than one charmap supports a character but with different glyphs, as
  // with older versions of MS Gothic, for example.  Always prefer a Unicode
  // charmap, if there is one; failing that, try MS_SYMBOL.
  // (FcFreeTypeCharIndex usually does the appropriate Unicode conversion,
  // but some fonts have non-Roman glyphs for FT_ENCODING_APPLE_ROMAN
  // characters.)
  if (!mFace->charmap || (mFace->charmap->encoding != FT_ENCODING_UNICODE &&
                          mFace->charmap->encoding != FT_ENCODING_MS_SYMBOL)) {
    if (FT_Err_Ok != FT_Select_Charmap(mFace, FT_ENCODING_UNICODE) &&
        FT_Err_Ok != FT_Select_Charmap(mFace, FT_ENCODING_MS_SYMBOL)) {
      NS_WARNING("failed to select Unicode or symbol charmap");
    }
  }

  return FcFreeTypeCharIndex(mFace, aCharCode);
#else
  return FT_Get_Char_Index(mFace, aCharCode);
#endif
}

typedef FT_UInt (*GetCharVariantFunction)(FT_Face face, FT_ULong charcode,
                                          FT_ULong variantSelector);

uint32_t gfxFT2LockedFace::GetUVSGlyph(uint32_t aCharCode,
                                       uint32_t aVariantSelector) {
  MOZ_ASSERT(aVariantSelector, "aVariantSelector should not be NULL");

  if (MOZ_UNLIKELY(!mFace)) return 0;

  // This function is available from FreeType 2.3.6 (June 2008).
  static CharVariantFunction sGetCharVariantPtr = FindCharVariantFunction();
  if (!sGetCharVariantPtr) return 0;

#ifdef HAVE_FONTCONFIG_FCFREETYPE_H
  // FcFreeTypeCharIndex may have changed the selected charmap.
  // FT_Face_GetCharVariantIndex needs a unicode charmap.
  if (!mFace->charmap || mFace->charmap->encoding != FT_ENCODING_UNICODE) {
    FT_Select_Charmap(mFace, FT_ENCODING_UNICODE);
  }
#endif

  return (*sGetCharVariantPtr)(mFace, aCharCode, aVariantSelector);
}

gfxFT2LockedFace::CharVariantFunction
gfxFT2LockedFace::FindCharVariantFunction() {
  // This function is available from FreeType 2.3.6 (June 2008).
  PRLibrary* lib = nullptr;
  CharVariantFunction function = reinterpret_cast<CharVariantFunction>(
      PR_FindFunctionSymbolAndLibrary("FT_Face_GetCharVariantIndex", &lib));
  if (!lib) {
    return nullptr;
  }

  FT_Int major;
  FT_Int minor;
  FT_Int patch;
  FT_Library_Version(mFace->glyph->library, &major, &minor, &patch);

  // Versions 2.4.0 to 2.4.3 crash if configured with
  // FT_CONFIG_OPTION_OLD_INTERNALS.  Presence of the symbol FT_Alloc
  // indicates FT_CONFIG_OPTION_OLD_INTERNALS.
  if (major == 2 && minor == 4 && patch < 4 &&
      PR_FindFunctionSymbol(lib, "FT_Alloc")) {
    function = nullptr;
  }

  // Decrement the reference count incremented in
  // PR_FindFunctionSymbolAndLibrary.
  PR_UnloadLibrary(lib);

  return function;
}

/*static*/
void gfxFT2Utils::GetVariationAxes(const FT_MM_Var* aMMVar,
                                   nsTArray<gfxFontVariationAxis>& aAxes) {
  MOZ_ASSERT(aAxes.IsEmpty());
  if (!aMMVar) {
    return;
  }
  aAxes.SetCapacity(aMMVar->num_axis);
  for (unsigned i = 0; i < aMMVar->num_axis; i++) {
    const auto& a = aMMVar->axis[i];
    gfxFontVariationAxis axis;
    axis.mMinValue = a.minimum / 65536.0;
    axis.mMaxValue = a.maximum / 65536.0;
    axis.mDefaultValue = a.def / 65536.0;
    axis.mTag = a.tag;
    axis.mName = a.name;
    aAxes.AppendElement(axis);
  }
}

/*static*/
void gfxFT2Utils::GetVariationInstances(
    gfxFontEntry* aFontEntry, const FT_MM_Var* aMMVar,
    nsTArray<gfxFontVariationInstance>& aInstances) {
  MOZ_ASSERT(aInstances.IsEmpty());
  if (!aMMVar) {
    return;
  }
  hb_blob_t* nameTable =
      aFontEntry->GetFontTable(TRUETYPE_TAG('n', 'a', 'm', 'e'));
  if (!nameTable) {
    return;
  }
  aInstances.SetCapacity(aMMVar->num_namedstyles);
  for (unsigned i = 0; i < aMMVar->num_namedstyles; i++) {
    const auto& ns = aMMVar->namedstyle[i];
    gfxFontVariationInstance inst;
    nsresult rv =
        gfxFontUtils::ReadCanonicalName(nameTable, ns.strid, inst.mName);
    if (NS_FAILED(rv)) {
      continue;
    }
    inst.mValues.SetCapacity(aMMVar->num_axis);
    for (unsigned j = 0; j < aMMVar->num_axis; j++) {
      gfxFontVariationValue value;
      value.mAxis = aMMVar->axis[j].tag;
      value.mValue = ns.coords[j] / 65536.0;
      inst.mValues.AppendElement(value);
    }
    aInstances.AppendElement(inst);
  }
  hb_blob_destroy(nameTable);
}
