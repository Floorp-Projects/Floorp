// Copyright (c) 2009-2017 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OTS_FEAT_H_
#define OTS_FEAT_H_

#include <vector>
#include <unordered_set>

#include "ots.h"
#include "graphite.h"

namespace ots {

class OpenTypeFEAT : public Table {
 public:
  explicit OpenTypeFEAT(Font* font, uint32_t tag)
      : Table(font, tag, tag) { }

  bool Parse(const uint8_t* data, size_t length);
  bool Serialize(OTSStream* out);
  bool IsValidFeatureId(uint32_t id) const;

 private:
  struct FeatureDefn : public TablePart<OpenTypeFEAT> {
    explicit FeatureDefn(OpenTypeFEAT* parent)
        : TablePart<OpenTypeFEAT>(parent) { }
    bool ParsePart(Buffer& table);
    bool SerializePart(OTSStream* out) const;
    uint32_t id;
    uint16_t numSettings;
    uint16_t reserved;
    uint32_t offset;
    uint16_t flags;
    static const uint16_t HAS_DEFAULT_SETTING = 0x4000;
    static const uint16_t RESERVED = 0x3F00;
    static const uint16_t DEFAULT_SETTING = 0x00FF;
    uint16_t label;
  };
  struct FeatureSettingDefn : public TablePart<OpenTypeFEAT> {
    explicit FeatureSettingDefn(OpenTypeFEAT* parent)
        : TablePart<OpenTypeFEAT>(parent) { }
    bool ParsePart(Buffer& table) { return ParsePart(table, true); }
    bool ParsePart(Buffer& table, bool used);
    bool SerializePart(OTSStream* out) const;
    int16_t value;
    uint16_t label;
  };
  uint32_t version;
  uint16_t numFeat;
  uint16_t reserved;
  uint32_t reserved2;
  std::vector<FeatureDefn> features;
  std::vector<FeatureSettingDefn> featSettings;
  std::unordered_set<uint32_t> feature_ids;
};

}  // namespace ots

#endif  // OTS_FEAT_H_
