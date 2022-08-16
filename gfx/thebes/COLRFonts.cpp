/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "COLRFonts.h"
#include "gfxFontUtils.h"
#include "gfxUtils.h"
#include "harfbuzz/hb.h"
#include "mozilla/gfx/Helpers.h"
#include "TextDrawTarget.h"

using namespace mozilla;
using namespace mozilla::gfx;

namespace {  // anonymous namespace for implementation internals

#pragma pack(1)  // ensure no padding is added to the COLR structs

struct LayerRecord;

struct COLRHeader {
  AutoSwap_PRUint16 version;
  AutoSwap_PRUint16 numBaseGlyphRecord;
  AutoSwap_PRUint32 offsetBaseGlyphRecord;
  AutoSwap_PRUint32 offsetLayerRecord;
  AutoSwap_PRUint16 numLayerRecords;

  const COLRBaseGlyphRecord* GetBaseGlyphRecords() const {
    return reinterpret_cast<const COLRBaseGlyphRecord*>(
        reinterpret_cast<const char*>(this) + offsetBaseGlyphRecord);
  }

  const LayerRecord* GetLayerRecords() const {
    return reinterpret_cast<const LayerRecord*>(
        reinterpret_cast<const char*>(this) + offsetLayerRecord);
  }
};

struct CPALHeaderVersion0 {
  AutoSwap_PRUint16 version;
  AutoSwap_PRUint16 numPaletteEntries;
  AutoSwap_PRUint16 numPalettes;
  AutoSwap_PRUint16 numColorRecords;
  AutoSwap_PRUint32 offsetFirstColorRecord;
};

// sRGB color space
struct CPALColorRecord {
  uint8_t blue;
  uint8_t green;
  uint8_t red;
  uint8_t alpha;
};

struct PaintState {
  const COLRHeader* mHeader;
  const CPALColorRecord* mPalette;
  DrawTarget* mDrawTarget;
  ScaledFont* mScaledFont;
  DrawOptions mDrawOptions;
  sRGBColor mCurrentColor;
};

static const CPALColorRecord* GetPaletteByIndex(hb_blob_t* aCPAL,
                                                uint32_t aIndex) {
  const auto* cpal = reinterpret_cast<const CPALHeaderVersion0*>(
      hb_blob_get_data(aCPAL, nullptr));
  const uint32_t offset = cpal->offsetFirstColorRecord;
  return reinterpret_cast<const CPALColorRecord*>(
      reinterpret_cast<const uint8_t*>(cpal) + offset);
}

static DeviceColor DoGetColor(PaintState& aState, uint16_t aPaletteIndex,
                              float aAlpha) {
  sRGBColor color;
  if (aPaletteIndex == 0xFFFF) {
    color = aState.mCurrentColor;
  } else {
    const CPALColorRecord& c = aState.mPalette[uint16_t(aPaletteIndex)];
    color = sRGBColor(c.red / 255.0, c.green / 255.0, c.blue / 255.0,
                      c.alpha / 255.0 * aAlpha);
  }
  return ToDeviceColor(color);
}

struct LayerRecord {
  AutoSwap_PRUint16 glyphId;
  AutoSwap_PRUint16 paletteEntryIndex;

  bool Paint(PaintState& aState, float aAlpha, const Point& aPoint) const {
    Glyph glyph{uint16_t(glyphId), aPoint};
    // TODO validate glyph.mIndex is within range for the font
    GlyphBuffer buffer{&glyph, 1};
    aState.mDrawTarget->FillGlyphs(
        aState.mScaledFont, buffer,
        ColorPattern(DoGetColor(aState, paletteEntryIndex, aAlpha)),
        aState.mDrawOptions);
    return true;
  }
};

#pragma pack()

}  // end anonymous namespace

namespace mozilla::gfx {

struct COLRBaseGlyphRecord {
  AutoSwap_PRUint16 glyphId;
  AutoSwap_PRUint16 firstLayerIndex;
  AutoSwap_PRUint16 numLayers;

  bool Paint(PaintState& aState, float aAlpha, const Point& aPoint) const {
    uint32_t layerIndex = uint16_t(firstLayerIndex);
    uint32_t end = layerIndex + uint16_t(numLayers);
    if (end > uint16_t(aState.mHeader->numLayerRecords)) {
      MOZ_ASSERT_UNREACHABLE("bad COLRv0 table");
      return false;
    }
    const auto* layers = aState.mHeader->GetLayerRecords();
    while (layerIndex < end) {
      if (!layers[layerIndex].Paint(aState, aAlpha, aPoint)) {
        return false;
      }
      ++layerIndex;
    }
    return true;
  }
};

bool COLRFonts::ValidateColorGlyphs(hb_blob_t* aCOLR, hb_blob_t* aCPAL) {
  unsigned int colrLength;
  const COLRHeader* colr =
      reinterpret_cast<const COLRHeader*>(hb_blob_get_data(aCOLR, &colrLength));
  unsigned int cpalLength;
  const CPALHeaderVersion0* cpal = reinterpret_cast<const CPALHeaderVersion0*>(
      hb_blob_get_data(aCPAL, &cpalLength));

  if (!colr || !cpal || !colrLength || !cpalLength) {
    return false;
  }

  if (uint16_t(colr->version) != 0 || uint16_t(cpal->version) != 0) {
    // We only support version 0 headers.
    return false;
  }

  const uint32_t offsetBaseGlyphRecord = colr->offsetBaseGlyphRecord;
  const uint16_t numBaseGlyphRecord = colr->numBaseGlyphRecord;
  const uint32_t offsetLayerRecord = colr->offsetLayerRecord;
  const uint16_t numLayerRecords = colr->numLayerRecords;

  const uint32_t offsetFirstColorRecord = cpal->offsetFirstColorRecord;
  const uint16_t numColorRecords = cpal->numColorRecords;
  const uint32_t numPaletteEntries = cpal->numPaletteEntries;

  if (offsetBaseGlyphRecord >= colrLength) {
    return false;
  }

  if (offsetLayerRecord >= colrLength) {
    return false;
  }

  if (offsetFirstColorRecord >= cpalLength) {
    return false;
  }

  if (!numPaletteEntries) {
    return false;
  }

  if (sizeof(COLRBaseGlyphRecord) * numBaseGlyphRecord >
      colrLength - offsetBaseGlyphRecord) {
    // COLR base glyph record will be overflow
    return false;
  }

  if (sizeof(LayerRecord) * numLayerRecords > colrLength - offsetLayerRecord) {
    // COLR layer record will be overflow
    return false;
  }

  if (sizeof(CPALColorRecord) * numColorRecords >
      cpalLength - offsetFirstColorRecord) {
    // CPAL color record will be overflow
    return false;
  }

  if (numPaletteEntries * uint16_t(cpal->numPalettes) != numColorRecords) {
    // palette of CPAL color record will be overflow.
    return false;
  }

  uint16_t lastGlyphId = 0;
  const COLRBaseGlyphRecord* baseGlyph =
      reinterpret_cast<const COLRBaseGlyphRecord*>(
          reinterpret_cast<const uint8_t*>(colr) + offsetBaseGlyphRecord);

  for (uint16_t i = 0; i < numBaseGlyphRecord; i++, baseGlyph++) {
    const uint32_t firstLayerIndex = baseGlyph->firstLayerIndex;
    const uint16_t numLayers = baseGlyph->numLayers;
    const uint16_t glyphId = baseGlyph->glyphId;

    if (lastGlyphId && lastGlyphId >= glyphId) {
      // glyphId must be sorted
      return false;
    }
    lastGlyphId = glyphId;

    if (!numLayers) {
      // no layer
      return false;
    }
    if (firstLayerIndex + numLayers > numLayerRecords) {
      // layer length of target glyph is overflow
      return false;
    }
  }

  const LayerRecord* layer = reinterpret_cast<const LayerRecord*>(
      reinterpret_cast<const uint8_t*>(colr) + offsetLayerRecord);

  for (uint16_t i = 0; i < numLayerRecords; i++, layer++) {
    if (uint16_t(layer->paletteEntryIndex) >= numPaletteEntries &&
        uint16_t(layer->paletteEntryIndex) != 0xFFFF) {
      // CPAL palette entry record is overflow
      return false;
    }
  }

  return true;
}

const COLRBaseGlyphRecord* COLRFonts::GetGlyphLayers(hb_blob_t* aCOLR,
                                                     uint32_t aGlyphId) {
  unsigned int length;
  const COLRHeader* colr =
      reinterpret_cast<const COLRHeader*>(hb_blob_get_data(aCOLR, &length));
  // This should never be called unless we have checked that the COLR table is
  // structurally valid, so it will be safe to read the header fields.
  MOZ_RELEASE_ASSERT(colr && length >= sizeof(COLRHeader), "bad COLR table!");
  auto compareBaseGlyph = [](const void* key, const void* data) -> int {
    uint32_t glyphId = (uint32_t)(uintptr_t)key;
    const COLRBaseGlyphRecord* baseGlyph =
        reinterpret_cast<const COLRBaseGlyphRecord*>(data);
    uint32_t baseGlyphId = uint16_t(baseGlyph->glyphId);
    if (baseGlyphId == glyphId) {
      return 0;
    }
    return baseGlyphId > glyphId ? -1 : 1;
  };
  return reinterpret_cast<COLRBaseGlyphRecord*>(
      bsearch((void*)(uintptr_t)aGlyphId, colr->GetBaseGlyphRecords(),
              uint16_t(colr->numBaseGlyphRecord), sizeof(COLRBaseGlyphRecord),
              compareBaseGlyph));
}

bool COLRFonts::PaintGlyphLayers(
    hb_blob_t* aCOLR, hb_blob_t* aCPAL, const COLRBaseGlyphRecord* aLayers,
    DrawTarget* aDrawTarget, layout::TextDrawTarget* aTextDrawer,
    ScaledFont* aScaledFont, DrawOptions aDrawOptions,
    const sRGBColor& aCurrentColor, const Point& aPoint) {
  // Default to opaque rendering (non-webrender applies alpha with a layer)
  float alpha = 1.0;
  if (aTextDrawer) {
    // defaultColor is the one that comes from CSS, so it has transparency info.
    bool hasComplexTransparency =
        0.0 < aCurrentColor.a && aCurrentColor.a < 1.0;
    if (hasComplexTransparency && uint16_t(aLayers->numLayers) > 1) {
      // WebRender doesn't support drawing multi-layer transparent color-glyphs,
      // as it requires compositing all the layers before applying transparency.
      // (pretend to succeed, output doesn't matter, we will emit a blob)
      aTextDrawer->FoundUnsupportedFeature();
      return true;
    }

    // If we get here, then either alpha is 0 or 1, or there's only one layer
    // which shouldn't have composition issues. In all of these cases, applying
    // transparency directly to the glyph should work perfectly fine.
    //
    // Note that we must still emit completely transparent emoji, because they
    // might be wrapped in a shadow that uses the text run's glyphs.
    alpha = aCurrentColor.a;
  }
  auto* colr =
      reinterpret_cast<const COLRHeader*>(hb_blob_get_data(aCOLR, nullptr));
  auto* palette = GetPaletteByIndex(aCPAL, 0);  // TODO: font-palette support
  PaintState state{colr,        palette,      aDrawTarget,
                   aScaledFont, aDrawOptions, aCurrentColor};
  return aLayers->Paint(state, alpha, aPoint);
}

}  // end namespace mozilla::gfx
