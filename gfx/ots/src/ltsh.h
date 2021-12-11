// Copyright (c) 2009-2017 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OTS_LTSH_H_
#define OTS_LTSH_H_

#include <vector>

#include "ots.h"

namespace ots {

class OpenTypeLTSH : public Table {
 public:
  explicit OpenTypeLTSH(Font *font, uint32_t tag)
      : Table(font, tag, tag) { }

  bool Parse(const uint8_t *data, size_t length);
  bool Serialize(OTSStream *out);
  bool ShouldSerialize();

 private:
  uint16_t version;
  std::vector<uint8_t> ypels;
};

}  // namespace ots

#endif  // OTS_LTSH_H_
