/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "COLRFonts.h"
#include "gfxFontUtils.h"
#include "gfxUtils.h"
#include "harfbuzz/hb.h"

using namespace mozilla;
using namespace mozilla::gfx;

#pragma pack(1)

struct COLRHeader {
  AutoSwap_PRUint16 version;
  AutoSwap_PRUint16 numBaseGlyphRecord;
  AutoSwap_PRUint32 offsetBaseGlyphRecord;
  AutoSwap_PRUint32 offsetLayerRecord;
  AutoSwap_PRUint16 numLayerRecords;
};

struct COLRBaseGlyphRecord {
  AutoSwap_PRUint16 glyphId;
  AutoSwap_PRUint16 firstLayerIndex;
  AutoSwap_PRUint16 numLayers;
};

struct COLRLayerRecord {
  AutoSwap_PRUint16 glyphId;
  AutoSwap_PRUint16 paletteEntryIndex;
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

#pragma pack()

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

  if (sizeof(COLRLayerRecord) * numLayerRecords >
      colrLength - offsetLayerRecord) {
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

  const COLRLayerRecord* layer = reinterpret_cast<const COLRLayerRecord*>(
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

static int CompareBaseGlyph(const void* key, const void* data) {
  uint32_t glyphId = (uint32_t)(uintptr_t)key;
  const COLRBaseGlyphRecord* baseGlyph =
      reinterpret_cast<const COLRBaseGlyphRecord*>(data);
  uint32_t baseGlyphId = uint16_t(baseGlyph->glyphId);

  if (baseGlyphId == glyphId) {
    return 0;
  }

  return baseGlyphId > glyphId ? -1 : 1;
}

static COLRBaseGlyphRecord* LookForBaseGlyphRecord(const COLRHeader* aCOLR,
                                                   uint32_t aGlyphId) {
  const uint8_t* baseGlyphRecords = reinterpret_cast<const uint8_t*>(aCOLR) +
                                    uint32_t(aCOLR->offsetBaseGlyphRecord);
  // BaseGlyphRecord is sorted by glyphId
  return reinterpret_cast<COLRBaseGlyphRecord*>(
      bsearch((void*)(uintptr_t)aGlyphId, baseGlyphRecords,
              uint16_t(aCOLR->numBaseGlyphRecord), sizeof(COLRBaseGlyphRecord),
              CompareBaseGlyph));
}

bool COLRFonts::GetColorGlyphLayers(
    hb_blob_t* aCOLR, hb_blob_t* aCPAL, uint32_t aGlyphId,
    const mozilla::gfx::DeviceColor& aDefaultColor, nsTArray<uint16_t>& aGlyphs,
    nsTArray<mozilla::gfx::DeviceColor>& aColors) {
  unsigned int blobLength;
  const COLRHeader* colr =
      reinterpret_cast<const COLRHeader*>(hb_blob_get_data(aCOLR, &blobLength));
  MOZ_ASSERT(colr, "Cannot get COLR raw data");
  MOZ_ASSERT(blobLength, "Found COLR data, but length is 0");

  COLRBaseGlyphRecord* baseGlyph = LookForBaseGlyphRecord(colr, aGlyphId);
  if (!baseGlyph) {
    return false;
  }

  const CPALHeaderVersion0* cpal = reinterpret_cast<const CPALHeaderVersion0*>(
      hb_blob_get_data(aCPAL, &blobLength));
  MOZ_ASSERT(cpal, "Cannot get CPAL raw data");
  MOZ_ASSERT(blobLength, "Found CPAL data, but length is 0");

  const COLRLayerRecord* layer = reinterpret_cast<const COLRLayerRecord*>(
      reinterpret_cast<const uint8_t*>(colr) +
      uint32_t(colr->offsetLayerRecord) +
      sizeof(COLRLayerRecord) * uint16_t(baseGlyph->firstLayerIndex));
  const uint16_t numLayers = baseGlyph->numLayers;
  const uint32_t offsetFirstColorRecord = cpal->offsetFirstColorRecord;

  for (uint16_t layerIndex = 0; layerIndex < numLayers; layerIndex++) {
    aGlyphs.AppendElement(uint16_t(layer->glyphId));
    if (uint16_t(layer->paletteEntryIndex) == 0xFFFF) {
      aColors.AppendElement(aDefaultColor);
    } else {
      const CPALColorRecord* color = reinterpret_cast<const CPALColorRecord*>(
          reinterpret_cast<const uint8_t*>(cpal) + offsetFirstColorRecord +
          sizeof(CPALColorRecord) * uint16_t(layer->paletteEntryIndex));
      aColors.AppendElement(
          mozilla::gfx::ToDeviceColor(mozilla::gfx::sRGBColor::FromU8(
              color->red, color->green, color->blue, color->alpha)));
    }
    layer++;
  }
  return true;
}

bool COLRFonts::HasColorLayersForGlyph(hb_blob_t* aCOLR, uint32_t aGlyphId) {
  unsigned int blobLength;
  const COLRHeader* colr =
      reinterpret_cast<const COLRHeader*>(hb_blob_get_data(aCOLR, &blobLength));
  MOZ_ASSERT(colr, "Cannot get COLR raw data");
  MOZ_ASSERT(blobLength, "Found COLR data, but length is 0");

  return LookForBaseGlyphRecord(colr, aGlyphId);
}
