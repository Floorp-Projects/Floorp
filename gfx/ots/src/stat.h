// Copyright (c) 2018 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OTS_STAT_H_
#define OTS_STAT_H_

#include <vector>

#include "ots.h"

namespace ots {

// -----------------------------------------------------------------------------
// OpenTypeSTAT Interface
// -----------------------------------------------------------------------------

class OpenTypeSTAT : public Table {
 public:
  explicit OpenTypeSTAT(Font* font, uint32_t tag)
      : Table(font, tag, tag) { }

  bool Parse(const uint8_t* data, size_t length);
  bool Serialize(OTSStream* out);

 private:
  uint16_t majorVersion;
  uint16_t minorVersion;
  uint16_t designAxisSize;
  uint16_t designAxisCount;
  uint32_t designAxesOffset;
  uint16_t axisValueCount;
  uint32_t offsetToAxisValueOffsets;
  uint16_t elidedFallbackNameID;

  struct AxisRecord {
    uint32_t axisTag;
    uint16_t axisNameID;
    uint16_t axisOrdering;
  };
  std::vector<AxisRecord> designAxes;

  typedef int32_t Fixed; /* 16.16 fixed-point value */

  struct AxisValueFormat1 {
    uint16_t axisIndex;
    uint16_t flags;
    uint16_t valueNameID;
    Fixed    value;
    static size_t Length() {
      return 3 * sizeof(uint16_t) + sizeof(Fixed);
    }
  };

  struct AxisValueFormat2 {
    uint16_t axisIndex;
    uint16_t flags;
    uint16_t valueNameID;
    Fixed    nominalValue;
    Fixed    rangeMinValue;
    Fixed    rangeMaxValue;
    static size_t Length() {
      return 3 * sizeof(uint16_t) + 3 * sizeof(Fixed);
    }
  };

  struct AxisValueFormat3 {
    uint16_t axisIndex;
    uint16_t flags;
    uint16_t valueNameID;
    Fixed    value;
    Fixed    linkedValue;
    static size_t Length() {
      return 3 * sizeof(uint16_t) + 2 * sizeof(Fixed);
    }
  };

  struct AxisValueFormat4 {
    uint16_t axisCount;
    uint16_t flags;
    uint16_t valueNameID;
    struct AxisValue {
      uint16_t axisIndex;
      Fixed    value;
    };
    std::vector<AxisValue> axisValues;
    size_t Length() const {
      return 3 * sizeof(uint16_t) + axisValues.size() * (sizeof(uint16_t) + sizeof(Fixed));
    }
  };

  struct AxisValueRecord {
    uint16_t format;
    union {
      AxisValueFormat1 format1;
      AxisValueFormat2 format2;
      AxisValueFormat3 format3;
      AxisValueFormat4 format4;
    };
    explicit AxisValueRecord(uint16_t format_)
      : format(format_)
    {
      if (format == 4) {
        new (&this->format4) AxisValueFormat4();
      }
    }
    AxisValueRecord(const AxisValueRecord& other_)
      : format(other_.format)
    {
      switch (format) {
      case 1:
        format1 = other_.format1;
        break;
      case 2:
        format2 = other_.format2;
        break;
      case 3:
        format3 = other_.format3;
        break;
      case 4:
        new (&this->format4) AxisValueFormat4();
        format4 = other_.format4;
        break;
      }
    }
    ~AxisValueRecord() {
      if (format == 4) {
        this->format4.~AxisValueFormat4();
      }
    }
    uint32_t Length() const {
      switch (format) {
      case 1:
        return sizeof(uint16_t) + format1.Length();
      case 2:
        return sizeof(uint16_t) + format2.Length();
      case 3:
        return sizeof(uint16_t) + format3.Length();
      case 4:
        return sizeof(uint16_t) + format4.Length();
      default:
        // can't happen
        return 0;
      }
    }
  };

  std::vector<AxisValueRecord> axisValues;
};

}  // namespace ots

#endif  // OTS_STAT_H_
