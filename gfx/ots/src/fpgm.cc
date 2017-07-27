// Copyright (c) 2009-2017 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fpgm.h"

// fpgm - Font Program
// http://www.microsoft.com/typography/otspec/fpgm.htm

namespace ots {

bool OpenTypeFPGM::Parse(const uint8_t *data, size_t length) {
  Buffer table(data, length);

  if (length >= 128 * 1024u) {
    return Error("length (%ld) > 120", length);  // almost all fpgm tables are less than 5k bytes.
  }

  if (!table.Skip(length)) {
    return Error("Bad table length");
  }

  this->data = data;
  this->length = length;
  return true;
}

bool OpenTypeFPGM::Serialize(OTSStream *out) {
  if (!out->Write(this->data, this->length)) {
    return Error("Failed to write fpgm table");
  }

  return true;
}

bool OpenTypeFPGM::ShouldSerialize() {
  return Table::ShouldSerialize() &&
         // this table is not for CFF fonts.
         GetFont()->GetTable(OTS_TAG_GLYF) != NULL;
}

}  // namespace ots
