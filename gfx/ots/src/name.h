// Copyright (c) 2011-2017 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OTS_NAME_H_
#define OTS_NAME_H_

#include <new>
#include <string>
#include <utility>
#include <vector>
#include <unordered_set>

#include "ots.h"

namespace ots {

struct NameRecord {
  NameRecord() {
  }

  NameRecord(uint16_t platformID, uint16_t encodingID,
             uint16_t languageID, uint16_t nameID)
    : platform_id(platformID),
      encoding_id(encodingID),
      language_id(languageID),
      name_id(nameID) {
  }

  uint16_t platform_id;
  uint16_t encoding_id;
  uint16_t language_id;
  uint16_t name_id;
  std::string text;

  bool operator<(const NameRecord& rhs) const {
    if (platform_id < rhs.platform_id) return true;
    if (platform_id > rhs.platform_id) return false;
    if (encoding_id < rhs.encoding_id) return true;
    if (encoding_id > rhs.encoding_id) return false;
    if (language_id < rhs.language_id) return true;
    if (language_id > rhs.language_id) return false;
    return name_id < rhs.name_id;
  }
};

class OpenTypeNAME : public Table {
 public:
  explicit OpenTypeNAME(Font *font, uint32_t tag)
      : Table(font, tag, tag) { }

  bool Parse(const uint8_t *data, size_t length);
  bool Serialize(OTSStream *out);
  bool IsValidNameId(uint16_t nameID, bool addIfMissing = false);

 private:
  std::vector<NameRecord> names;
  std::vector<std::string> lang_tags;
  std::unordered_set<uint16_t> name_ids;
};

}  // namespace ots

#endif  // OTS_NAME_H_
