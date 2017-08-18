// Copyright (c) 2009-2017 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OTS_SILL_H_
#define OTS_SILL_H_

#include "ots.h"
#include "graphite.h"

#include <vector>

namespace ots {

class OpenTypeSILL : public Table {
 public:
  explicit OpenTypeSILL(Font* font, uint32_t tag)
      : Table(font, tag, tag) { }

  bool Parse(const uint8_t* data, size_t length);
  bool Serialize(OTSStream* out);

 private:
  struct LanguageEntry : public TablePart<OpenTypeSILL> {
    explicit LanguageEntry(OpenTypeSILL* parent)
        : TablePart<OpenTypeSILL>(parent) { }
    bool ParsePart(Buffer &table);
    bool SerializePart(OTSStream* out) const;
    uint8_t langcode[4];
    uint16_t numSettings;
    uint16_t offset;
  };
  struct LangFeatureSetting : public TablePart<OpenTypeSILL> {
    explicit LangFeatureSetting(OpenTypeSILL* parent)
        : TablePart<OpenTypeSILL>(parent) { }
    bool ParsePart(Buffer &table);
    bool SerializePart(OTSStream* out) const;
    uint32_t featureId;
    int16_t value;
    uint16_t reserved;
  };
  uint32_t version;
  uint16_t numLangs;
  uint16_t searchRange;
  uint16_t entrySelector;
  uint16_t rangeShift;
  std::vector<LanguageEntry> entries;
  std::vector<LangFeatureSetting> settings;
};

}  // namespace ots

#endif  // OTS_SILL_H_
