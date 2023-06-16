// Copyright (c) 2011-2017 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OTS_GPOS_H_
#define OTS_GPOS_H_

#include "ots.h"
#include "layout.h"

namespace ots {

class OpenTypeGPOS : public OpenTypeLayoutTable {
 public:
  explicit OpenTypeGPOS(Font *font, uint32_t tag)
      : OpenTypeLayoutTable(font, tag, tag) { }

 private:
  bool ValidLookupSubtableType(const uint16_t lookup_type,
                               bool extension = false) const;
  bool ParseLookupSubtable(const uint8_t *data, const size_t length,
                           const uint16_t lookup_type);

  bool ParseSingleAdjustment(const uint8_t *data, const size_t length);
  bool ParsePairAdjustment(const uint8_t *data, const size_t length);
  bool ParseCursiveAttachment(const uint8_t *data, const size_t length);
  bool ParseMarkToBaseAttachment(const uint8_t *data, const size_t length);
  bool ParseMarkToLigatureAttachment(const uint8_t *data, const size_t length);
  bool ParseMarkToMarkAttachment(const uint8_t *data, const size_t length);
};

}  // namespace ots

#endif

