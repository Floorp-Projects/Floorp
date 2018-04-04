// Copyright (c) 2018 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OTS_CVAR_H_
#define OTS_CVAR_H_

#include "ots.h"

namespace ots {

// -----------------------------------------------------------------------------
// OpenTypeCVAR Interface
// -----------------------------------------------------------------------------

class OpenTypeCVAR : public Table {
 public:
  explicit OpenTypeCVAR(Font* font, uint32_t tag)
      : Table(font, tag, tag) { }

  bool Parse(const uint8_t* data, size_t length);
  bool Serialize(OTSStream* out);

 private:
  const uint8_t *m_data;
  size_t m_length;
};

}  // namespace ots

#endif  // OTS_CVAR_H_
