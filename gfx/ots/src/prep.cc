// Copyright (c) 2009-2017 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "prep.h"

// prep - Control Value Program
// http://www.microsoft.com/typography/otspec/prep.htm

namespace ots {

bool OpenTypePREP::Parse(const uint8_t *data, size_t length) {
  Buffer table(data, length);

  if (length >= 128 * 1024u) {
    // almost all prep tables are less than 9k bytes.
    return Error("Table length %ld > 120K", length);
  }

  if (!table.Skip(length)) {
    return Error("Failed to read table of length %ld", length);
  }

  this->m_data = data;
  this->m_length = length;
  return true;
}

bool OpenTypePREP::Serialize(OTSStream *out) {
  if (!out->Write(this->m_data, this->m_length)) {
    return Error("Failed to write table length");
  }

  return true;
}

bool OpenTypePREP::ShouldSerialize() {
  return Table::ShouldSerialize() &&
         // this table is not for CFF fonts.
         GetFont()->GetTable(OTS_TAG_GLYF) != NULL;
}

}  // namespace ots
