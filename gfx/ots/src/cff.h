// Copyright (c) 2009-2017 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OTS_CFF_H_
#define OTS_CFF_H_

#include "ots.h"

#include <map>
#include <string>
#include <vector>

#undef major // glibc defines major!

namespace ots {

struct CFFIndex {
  CFFIndex()
      : count(0), off_size(0), offset_to_next(0) {}
  uint32_t count;
  uint8_t off_size;
  std::vector<uint32_t> offsets;
  uint32_t offset_to_next;
};

typedef std::map<uint32_t, uint16_t> CFFFDSelect;

class OpenTypeCFF : public Table {
 public:
  explicit OpenTypeCFF(Font *font, uint32_t tag)
      : Table(font, tag, tag),
        major(0),
        font_dict_length(0),
        charstrings_index(NULL),
        local_subrs(NULL),
        m_data(NULL),
        m_length(0) {
  }

  ~OpenTypeCFF();

  bool Parse(const uint8_t *data, size_t length);
  bool Serialize(OTSStream *out);

  // Major version number.
  uint8_t major;

  // Name INDEX. This name is used in name.cc as a postscript font name.
  std::string name;

  // The number of fonts the file has.
  size_t font_dict_length;
  // A map from glyph # to font #.
  CFFFDSelect fd_select;

  // A list of char strings.
  CFFIndex* charstrings_index;
  // A list of Local Subrs associated with FDArrays. Can be empty.
  std::vector<CFFIndex *> local_subrs_per_font;
  // A Local Subrs associated with Top DICT. Can be NULL.
  CFFIndex *local_subrs;

  // CFF2 VariationStore regionIndexCount.
  std::vector<uint16_t> region_index_count;

  // CFF2 vsindex: per FontDICT->PrivateDICT
  // default of 0 is stored for each font if not
  // explicitly set in Font's PrivateDICT.
  std::vector<int32_t> vsindex_per_font;

 protected:
  bool ValidateFDSelect(uint16_t num_glyphs);

 private:
  const uint8_t *m_data;
  size_t m_length;
};

class OpenTypeCFF2 : public OpenTypeCFF {
 public:
  explicit OpenTypeCFF2(Font *font, uint32_t tag)
      : OpenTypeCFF(font, tag),
        m_data(NULL),
        m_length(0) {
  }

  bool Parse(const uint8_t *data, size_t length);
  bool Serialize(OTSStream *out);

 private:
  const uint8_t *m_data;
  size_t m_length;
};

}  // namespace ots

#endif  // OTS_CFF_H_
