// Copyright (c) 2009-2017 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OTS_FPGM_H_
#define OTS_FPGM_H_

#include "ots.h"

namespace ots {

class OpenTypeFPGM : public Table {
 public:
  explicit OpenTypeFPGM(Font *font, uint32_t tag)
      : Table(font, tag, tag) { }

  bool Parse(const uint8_t *data, size_t length);
  bool Serialize(OTSStream *out);
  bool ShouldSerialize();

 private:
  const uint8_t *data;
  uint32_t length;
};

}  // namespace ots

#endif  // OTS_FPGM_H_
