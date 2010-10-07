// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstring>

#include "cff.h"
#include "ots.h"

// name - Naming Table
// http://www.microsoft.com/opentype/otspec/name.htm

namespace ots {

bool ots_name_parse(OpenTypeFile *, const uint8_t *, size_t) {
  return true;
}

bool ots_name_should_serialise(OpenTypeFile *) {
  return true;
}

bool ots_name_serialise(OTSStream *out, OpenTypeFile *file) {
  // NAME is a required table, but we don't want anything to do with it. Thus,
  // we don't bother parsing it and we just serialise an empty name table.

  const char* kStrings[] = {
      "Derived font data",  // 0: copyright
      "OTS derived font",  // 1: the name the user sees
      "Unspecified",  // 2: face weight
      "UniqueID",  // 3: unique id
      "OTS derivied font",  // 4: human readable name
      "1.000",  // 5: version
      "False",  // 6: postscript name
      NULL,  // 7: trademark data
      "OTS",  // 8: foundary
      "OTS",  // 9: designer
  };
  static const size_t kStringsLen = sizeof(kStrings) / sizeof(kStrings[0]);

  // The spec says that "In CFF OpenType fonts, these two name strings, when
  // translated to ASCII, must also be identical to the font name as stored in
  // the CFF's Name INDEX." And actually, Mac OS X's font parser requires that.
  if (file->cff && !file->cff->name.empty()) {
    kStrings[6] = file->cff->name.c_str();
  }

  unsigned num_strings = 0;
  for (unsigned i = 0; i < kStringsLen; ++i) {
    if (kStrings[i]) num_strings++;
  }

  if (!out->WriteU16(0) ||  // version
      // Magic numbers:
      //   6:  This entry (U16 * 3 = 6 bytes)
      //   2:  Mac Roman & Windows Roman = 2 types
      //   12: Each string entry (U16 * 6 = 12 bytes)
      !out->WriteU16(num_strings * 2) ||  // count
      !out->WriteU16(6 + num_strings * 2 * 12)) {  // string data offset
    return OTS_FAILURE();
  }

  unsigned current_offset = 0;
  for (unsigned i = 0; i < kStringsLen; ++i) {
    if (!kStrings[i]) continue;

    // string length in UTF-8 (ASCII).
    size_t len = std::strlen(kStrings[i]);

    if (!out->WriteU16(1) ||  // Mac
        !out->WriteU16(0) ||  // Roman
        !out->WriteU16(0) ||  // English
        !out->WriteU16(i) ||
        !out->WriteU16(len) ||
        !out->WriteU16(current_offset)) {
      return OTS_FAILURE();
    }

    current_offset += len;
  }

  for (unsigned i = 0; i < kStringsLen; ++i) {
    if (!kStrings[i]) continue;

    // string length in UTF-16.
    size_t len = std::strlen(kStrings[i]) * 2;

    if (!out->WriteU16(3) ||  // Windows
        !out->WriteU16(1) ||  // Unicode BMP (UCS-2)
        !out->WriteU16(0x0409) ||  // US English
        !out->WriteU16(i) ||
        !out->WriteU16(len) ||
        !out->WriteU16(current_offset)) {
      return OTS_FAILURE();
    }

    current_offset += len;
  }

  // Write strings in Mac Roman compatible with ASCII.
  // Because all the entries are ASCII, we can just copy.
  for (unsigned i = 0; i < kStringsLen; ++i) {
    if (!kStrings[i]) continue;

    const size_t len = std::strlen(kStrings[i]);
    if (!out->Write(kStrings[i], len)) {
      return OTS_FAILURE();
    }
  }

  // Write strings in UCS-2. Because all the entries are ASCII,
  // we can just expand each byte to U16.
  for (unsigned i = 0; i < kStringsLen; ++i) {
    if (!kStrings[i]) continue;

    const size_t len = std::strlen(kStrings[i]);
    for (size_t j = 0; j < len; ++j) {
      uint16_t v = kStrings[i][j];
      if (!out->WriteU16(v)) {
        return OTS_FAILURE();
      }
    }
  }

  return true;
}

void ots_name_free(OpenTypeFile *) {
}

}  // namespace
