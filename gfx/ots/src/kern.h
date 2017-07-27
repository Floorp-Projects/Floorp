// Copyright (c) 2009-2017 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OTS_KERN_H_
#define OTS_KERN_H_

#include <vector>

#include "ots.h"

namespace ots {

struct OpenTypeKERNFormat0Pair {
  uint16_t left;
  uint16_t right;
  int16_t value;
};

struct OpenTypeKERNFormat0 {
  uint16_t version;
  uint16_t coverage;
  uint16_t search_range;
  uint16_t entry_selector;
  uint16_t range_shift;
  std::vector<OpenTypeKERNFormat0Pair> pairs;
};

// Format 2 is not supported. Since the format is not supported by Windows,
// WebFonts unlikely use it. I've checked thousands of proprietary fonts and
// free fonts, and found no font uses the format.

class OpenTypeKERN : public Table {
 public:
  explicit OpenTypeKERN(Font *font, uint32_t tag)
      : Table(font, tag, tag) { }

  bool Parse(const uint8_t *data, size_t length);
  bool Serialize(OTSStream *out);
  bool ShouldSerialize();

 private:
  uint16_t version;
  std::vector<OpenTypeKERNFormat0> subtables;
};

}  // namespace ots

#endif  // OTS_KERN_H_
