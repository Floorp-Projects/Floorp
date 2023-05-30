// Copyright (c) 2011-2017 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OTS_GSUB_H_
#define OTS_GSUB_H_

#include "ots.h"
#include "layout.h"

namespace ots {

class OpenTypeGSUB : public OpenTypeLayoutTable {
 public:
  explicit OpenTypeGSUB(Font *font, uint32_t tag)
      : OpenTypeLayoutTable(font, tag, tag) { }

 private:
  bool ValidLookupSubtableType(uint16_t lookup_type,
                               bool extension = false) const;
  bool ParseLookupSubtable(const uint8_t *data, const size_t length,
                           const uint16_t lookup_type);

  bool ParseSingleSubstitution(const uint8_t *data, const size_t length);
  bool ParseMutipleSubstitution(const uint8_t *data, const size_t length);
  bool ParseAlternateSubstitution(const uint8_t *data, const size_t length);
  bool ParseLigatureSubstitution(const uint8_t *data, const size_t length);
  bool ParseReverseChainingContextSingleSubstitution(const uint8_t *data, const size_t length);
};

}  // namespace ots

#endif  // OTS_GSUB_H_

