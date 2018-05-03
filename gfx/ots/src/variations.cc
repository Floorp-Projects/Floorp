// Copyright (c) 2018 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "layout.h"

#include "fvar.h"

// OpenType Variations Common Table Formats

#define TABLE_NAME "Variations" // XXX: use individual table names

namespace {

bool ParseVariationRegionList(const ots::Font* font, const uint8_t* data, const size_t length,
                              uint16_t* regionCount) {
  ots::Buffer subtable(data, length);

  uint16_t axisCount;

  if (!subtable.ReadU16(&axisCount) ||
      !subtable.ReadU16(regionCount)) {
    return OTS_FAILURE_MSG("Failed to read variation region list header");
  }

  const ots::OpenTypeFVAR* fvar =
    static_cast<ots::OpenTypeFVAR*>(font->GetTypedTable(OTS_TAG_FVAR));
  if (!fvar) {
    return OTS_FAILURE_MSG("Required fvar table is missing");
  }
  if (axisCount != fvar->AxisCount()) {
    return OTS_FAILURE_MSG("Axis count mismatch");
  }

  for (unsigned i = 0; i < *regionCount; i++) {
    for (unsigned j = 0; j < axisCount; j++) {
      int16_t startCoord, peakCoord, endCoord;
      if (!subtable.ReadS16(&startCoord) ||
          !subtable.ReadS16(&peakCoord) ||
          !subtable.ReadS16(&endCoord)) {
        return OTS_FAILURE_MSG("Failed to read region axis coordinates");
      }
      if (startCoord > peakCoord || peakCoord > endCoord) {
        return OTS_FAILURE_MSG("Region axis coordinates out of order");
      }
      if (startCoord < -0x4000 || endCoord > 0x4000) {
        return OTS_FAILURE_MSG("Region axis coordinate out of range");
      }
      if ((peakCoord < 0 && endCoord > 0) ||
          (peakCoord > 0 && startCoord < 0)) {
        return OTS_FAILURE_MSG("Invalid region axis coordinates");
      }
    }
  }

  return true;
}

bool
ParseVariationDataSubtable(const ots::Font* font, const uint8_t* data, const size_t length,
                           const uint16_t regionCount) {
  ots::Buffer subtable(data, length);

  uint16_t itemCount;
  uint16_t shortDeltaCount;
  uint16_t regionIndexCount;

  if (!subtable.ReadU16(&itemCount) ||
      !subtable.ReadU16(&shortDeltaCount) ||
      !subtable.ReadU16(&regionIndexCount)) {
    return OTS_FAILURE_MSG("Failed to read variation data subtable header");
  }

  for (unsigned i = 0; i < regionIndexCount; i++) {
    uint16_t regionIndex;
    if (!subtable.ReadU16(&regionIndex) || regionIndex >= regionCount) {
      return OTS_FAILURE_MSG("Bad region index");
    }
  }

  if (!subtable.Skip(size_t(itemCount) * (size_t(shortDeltaCount) + size_t(regionIndexCount)))) {
    return OTS_FAILURE_MSG("Failed to read delta data");
  }

  return true;
}

} // namespace

namespace ots {

bool
ParseItemVariationStore(const Font* font, const uint8_t* data, const size_t length) {
  Buffer subtable(data, length);

  uint16_t format;
  uint32_t variationRegionListOffset;
  uint16_t itemVariationDataCount;

  if (!subtable.ReadU16(&format) ||
      !subtable.ReadU32(&variationRegionListOffset) ||
      !subtable.ReadU16(&itemVariationDataCount)) {
    return OTS_FAILURE_MSG("Failed to read item variation store header");
  }

  if (format != 1) {
    return OTS_FAILURE_MSG("Unknown item variation store format");
  }

  if (variationRegionListOffset < subtable.offset() + 4 * itemVariationDataCount ||
      variationRegionListOffset > length) {
    return OTS_FAILURE_MSG("Invalid variation region list offset");
  }

  uint16_t regionCount;
  if (!ParseVariationRegionList(font,
                                data + variationRegionListOffset,
                                length - variationRegionListOffset,
                                &regionCount)) {
    return OTS_FAILURE_MSG("Failed to parse variation region list");
  }

  for (unsigned i = 0; i < itemVariationDataCount; i++) {
    uint32_t offset;
    if (!subtable.ReadU32(&offset)) {
      return OTS_FAILURE_MSG("Failed to read variation data subtable offset");
    }
    if (offset >= length) {
      return OTS_FAILURE_MSG("Bad offset to variation data subtable");
    }
    if (!ParseVariationDataSubtable(font, data + offset, length - offset, regionCount)) {
      return OTS_FAILURE_MSG("Failed to parse variation data subtable");
    }
  }

  return true;
}

bool ParseDeltaSetIndexMap(const Font* font, const uint8_t* data, const size_t length) {
  Buffer subtable(data, length);

  uint16_t entryFormat;
  uint16_t mapCount;

  if (!subtable.ReadU16(&entryFormat) ||
      !subtable.ReadU16(&mapCount)) {
    return OTS_FAILURE_MSG("Failed to read delta set index map header");
  }

  const uint16_t INNER_INDEX_BIT_COUNT_MASK = 0x000F;
  const uint16_t MAP_ENTRY_SIZE_MASK = 0x0030;

  const uint16_t entrySize = (((entryFormat & MAP_ENTRY_SIZE_MASK) >> 4) + 1);
  if (!subtable.Skip(entrySize * mapCount)) {
    return OTS_FAILURE_MSG("Failed to read delta set index map data");
  }

  return true;
}

bool ParseVariationData(const Font* font, const uint8_t* data, size_t length,
                        size_t axisCount, size_t sharedTupleCount) {
  Buffer subtable(data, length);

  uint16_t tupleVariationCount;
  uint16_t dataOffset;
  if (!subtable.ReadU16(&tupleVariationCount) ||
      !subtable.ReadU16(&dataOffset)) {
    return OTS_FAILURE_MSG("Failed to read variation data header");
  }

  if (dataOffset > length) {
    return OTS_FAILURE_MSG("Invalid serialized data offset");
  }

  tupleVariationCount &= 0x0FFF; // mask off flags

  const uint16_t EMBEDDED_PEAK_TUPLE = 0x8000;
  const uint16_t INTERMEDIATE_REGION = 0x4000;
  const uint16_t TUPLE_INDEX_MASK    = 0x0FFF;

  for (unsigned i = 0; i < tupleVariationCount; i++) {
    uint16_t variationDataSize;
    uint16_t tupleIndex;

    if (!subtable.ReadU16(&variationDataSize) ||
        !subtable.ReadU16(&tupleIndex)) {
      return OTS_FAILURE_MSG("Failed to read tuple variation header");
    }

    if (tupleIndex & EMBEDDED_PEAK_TUPLE) {
      for (unsigned axis = 0; axis < axisCount; axis++) {
        int16_t coordinate;
        if (!subtable.ReadS16(&coordinate)) {
          return OTS_FAILURE_MSG("Failed to read tuple coordinate");
        }
        if (coordinate < -0x4000 || coordinate > 0x4000) {
          return OTS_FAILURE_MSG("Invalid tuple coordinate");
        }
      }
    }

    if (tupleIndex & INTERMEDIATE_REGION) {
      std::vector<int16_t> startTuple(axisCount);
      for (unsigned axis = 0; axis < axisCount; axis++) {
        int16_t coordinate;
        if (!subtable.ReadS16(&coordinate)) {
          return OTS_FAILURE_MSG("Failed to read tuple coordinate");
        }
        if (coordinate < -0x4000 || coordinate > 0x4000) {
          return OTS_FAILURE_MSG("Invalid tuple coordinate");
        }
        startTuple.push_back(coordinate);
      }

      std::vector<int16_t> endTuple(axisCount);
      for (unsigned axis = 0; axis < axisCount; axis++) {
        int16_t coordinate;
        if (!subtable.ReadS16(&coordinate)) {
          return OTS_FAILURE_MSG("Failed to read tuple coordinate");
        }
        if (coordinate < -0x4000 || coordinate > 0x4000) {
          return OTS_FAILURE_MSG("Invalid tuple coordinate");
        }
        endTuple.push_back(coordinate);
      }

      for (unsigned axis = 0; axis < axisCount; axis++) {
        if (startTuple[axis] > endTuple[axis]) {
          return OTS_FAILURE_MSG("Invalid intermediate range");
        }
      }
    }

    if (!(tupleIndex & EMBEDDED_PEAK_TUPLE)) {
      tupleIndex &= TUPLE_INDEX_MASK;
      if (tupleIndex >= sharedTupleCount) {
        return OTS_FAILURE_MSG("Tuple index out of range");
      }
    }
  }

  // TODO: we don't attempt to interpret the serialized data block

  return true;
}

} // namespace ots
