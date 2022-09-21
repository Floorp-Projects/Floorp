// Copyright (c) 2022 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OTS_CPAL_H_
#define OTS_CPAL_H_

#include "ots.h"

#include <vector>

namespace ots {

class OpenTypeCPAL : public Table {
 public:
  explicit OpenTypeCPAL(Font *font, uint32_t tag)
      : Table(font, tag, tag) {
  }

  bool Parse(const uint8_t *data, size_t length);
  bool Serialize(OTSStream *out);

  // This is public so that COLR can access it.
  uint16_t num_palette_entries;

 private:
  uint16_t version;

  std::vector<uint16_t> colorRecordIndices;
  std::vector<uint32_t> colorRecords;

  // Arrays present only if version == 1.
  std::vector<uint32_t> paletteTypes;
  std::vector<uint16_t> paletteLabels;
  std::vector<uint16_t> paletteEntryLabels;
};

}  // namespace ots

#endif  // OTS_CPAL_H_
