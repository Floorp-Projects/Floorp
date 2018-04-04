// Copyright (c) 2018 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OTS_AVAR_H_
#define OTS_AVAR_H_

#include "ots.h"

#include <vector>

namespace ots {

// -----------------------------------------------------------------------------
// OpenTypeAVAR Interface
// -----------------------------------------------------------------------------

class OpenTypeAVAR : public Table {
 public:
  explicit OpenTypeAVAR(Font* font, uint32_t tag)
      : Table(font, tag, tag) { }

  bool Parse(const uint8_t* data, size_t length);
  bool Serialize(OTSStream* out);

 private:
  uint16_t majorVersion;
  uint16_t minorVersion;
  uint16_t reserved;
  uint16_t axisCount;

  struct AxisValueMap {
    int16_t fromCoordinate;
    int16_t toCoordinate;
  };

  std::vector<std::vector<AxisValueMap>> axisSegmentMaps;
};

}  // namespace ots

#endif  // OTS_AVAR_H_
