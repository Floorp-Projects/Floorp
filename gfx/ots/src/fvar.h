// Copyright (c) 2018 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OTS_FVAR_H_
#define OTS_FVAR_H_

#include <vector>

#include "ots.h"

namespace ots {

// -----------------------------------------------------------------------------
// OpenTypeFVAR Interface
// -----------------------------------------------------------------------------

class OpenTypeFVAR : public Table {
 public:
  explicit OpenTypeFVAR(Font* font, uint32_t tag)
      : Table(font, tag, tag) { }

  bool Parse(const uint8_t* data, size_t length);
  bool Serialize(OTSStream* out);

  uint16_t AxisCount() const { return axisCount; }

 private:
  uint16_t majorVersion;
  uint16_t minorVersion;
  uint16_t axesArrayOffset;
  uint16_t reserved;
  uint16_t axisCount;
  uint16_t axisSize;
  uint16_t instanceCount;
  uint16_t instanceSize;

  typedef int32_t Fixed; /* 16.16 fixed-point value */

  struct VariationAxisRecord {
    uint32_t axisTag;
    Fixed    minValue;
    Fixed    defaultValue;
    Fixed    maxValue;
    uint16_t flags;
    uint16_t axisNameID;
  };
  std::vector<VariationAxisRecord> axes;

  struct InstanceRecord {
    uint16_t subfamilyNameID;
    uint16_t flags;
    std::vector<Fixed> coordinates;
    uint16_t postScriptNameID; // optional
  };
  std::vector<InstanceRecord> instances;

  bool instancesHavePostScriptNameID;
};

}  // namespace ots

#endif  // OTS_FVAR_H_
