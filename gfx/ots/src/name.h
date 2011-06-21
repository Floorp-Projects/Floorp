// Copyright (c) 2011 Mozilla Foundation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OTS_NAME_H_
#define OTS_NAME_H_

#include <new>
#include <utility>
#include <vector>
#include <string>

#include "ots.h"

namespace ots {

struct NameRecord {
  NameRecord()
  { }

  NameRecord(uint16_t p, uint16_t e, uint16_t l, uint16_t n)
    : platformID(p), encodingID(e), languageID(l), nameID(n)
  { }

  uint16_t platformID;
  uint16_t encodingID;
  uint16_t languageID;
  uint16_t nameID;
  std::string text;

  bool operator<(const NameRecord& rhs) const {
    if (platformID < rhs.platformID) return true;
    if (platformID > rhs.platformID) return false;
    if (encodingID < rhs.encodingID) return true;
    if (encodingID > rhs.encodingID) return false;
    if (languageID < rhs.languageID) return true;
    if (languageID > rhs.languageID) return false;
    if (nameID < rhs.nameID) return true;
    return false;
  }
};

struct OpenTypeNAME {
  std::vector<NameRecord>  names;
  std::vector<std::string> langTags;
};

}  // namespace ots

#endif  // OTS_NAME_H_
