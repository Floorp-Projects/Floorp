// Copyright (c) 2018 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OTS_GVAR_H_
#define OTS_GVAR_H_

#include "ots.h"

namespace ots {

// -----------------------------------------------------------------------------
// OpenTypeGVAR Interface
// -----------------------------------------------------------------------------

class OpenTypeGVAR : public Table {
 public:
  explicit OpenTypeGVAR(Font* font, uint32_t tag)
      : Table(font, tag, tag), m_ownsData(false) { }

  virtual ~OpenTypeGVAR() {
    if (m_ownsData) {
      delete[] m_data;
    }
  }

  bool Parse(const uint8_t* data, size_t length);
  bool Serialize(OTSStream* out);

#ifdef OTS_SYNTHESIZE_MISSING_GVAR
  bool InitEmpty();
#endif

 private:
  const uint8_t *m_data;
  size_t m_length;

  bool m_ownsData;
};

}  // namespace ots

#endif  // OTS_GVAR_H_
