// Copyright (c) 2009-2017 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OTS_SILE_H_
#define OTS_SILE_H_

#include "ots.h"
#include "graphite.h"

#include <vector>

namespace ots {

class OpenTypeSILE : public Table {
 public:
  explicit OpenTypeSILE(Font* font, uint32_t tag)
      : Table(font, tag, tag) { }

  bool Parse(const uint8_t* data, size_t length);
  bool Serialize(OTSStream* out);

 private:
  uint32_t version;
  uint32_t checksum;
  uint32_t createTime[2];
  uint32_t modifyTime[2];
  uint16_t fontNameLength;
  std::vector<uint16_t> fontName;
  uint16_t fontFileLength;
  std::vector<uint16_t> baseFile;
};

}  // namespace ots

#endif  // OTS_SILE_H_
