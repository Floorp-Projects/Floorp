// Copyright (c) 2009-2017 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OTS_GLOC_H_
#define OTS_GLOC_H_

#include <vector>

#include "ots.h"
#include "graphite.h"

namespace ots {

class OpenTypeGLOC : public Table {
 public:
  explicit OpenTypeGLOC(Font* font, uint32_t tag)
      : Table(font, tag, tag) { }

  bool Parse(const uint8_t* data, size_t length);
  bool Serialize(OTSStream* out);
  const std::vector<uint32_t>& GetLocations();

 private:
  uint32_t version;
  uint16_t flags;
  static const uint16_t LONG_FORMAT = 0b1;
  static const uint16_t ATTRIB_IDS = 0b10;
  uint16_t numAttribs;
  std::vector<uint32_t> locations;
  std::vector<uint16_t> attribIds;
};

}  // namespace ots

#endif  // OTS_GLOC_H_
