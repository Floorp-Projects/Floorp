// Copyright (c) 2012-2017 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cff.h"

#include <cstring>
#include <utility>
#include <vector>

#include "maxp.h"
#include "cff_charstring.h"
#include "variations.h"

// CFF - PostScript font program (Compact Font Format) table
// http://www.microsoft.com/typography/otspec/cff.htm
// http://www.microsoft.com/typography/otspec/cffspec.htm

#define TABLE_NAME "CFF"

namespace {

enum DICT_OPERAND_TYPE {
  DICT_OPERAND_INTEGER,
  DICT_OPERAND_REAL,
  DICT_OPERATOR,
};

enum DICT_DATA_TYPE {
  DICT_DATA_TOPLEVEL,
  DICT_DATA_FDARRAY,
  DICT_DATA_PRIVATE,
};

enum FONT_FORMAT {
  FORMAT_UNKNOWN,
  FORMAT_CID_KEYED,
  FORMAT_OTHER,  // Including synthetic fonts
};

// see Appendix. A
const size_t kNStdString = 390;

typedef std::pair<int32_t, DICT_OPERAND_TYPE> Operand;

bool ReadOffset(ots::Buffer &table, uint8_t off_size, uint32_t *offset) {
  if (off_size > 4) {
    return OTS_FAILURE();
  }

  uint32_t tmp32 = 0;
  for (unsigned i = 0; i < off_size; ++i) {
    uint8_t tmp8 = 0;
    if (!table.ReadU8(&tmp8)) {
      return OTS_FAILURE();
    }
    tmp32 <<= 8;
    tmp32 += tmp8;
  }
  *offset = tmp32;
  return true;
}

bool ParseIndex(ots::Buffer &table, ots::CFFIndex &index, bool cff2 = false) {
  index.off_size = 0;
  index.offsets.clear();

  if (cff2) {
    if (!table.ReadU32(&(index.count))) {
      return OTS_FAILURE();
    }
  } else {
    uint16_t count;
    if (!table.ReadU16(&count)) {
      return OTS_FAILURE();
    }
    index.count = count;
  }

  if (index.count == 0) {
    // An empty INDEX.
    index.offset_to_next = table.offset();
    return true;
  }

  if (!table.ReadU8(&(index.off_size))) {
    return OTS_FAILURE();
  }
  if (index.off_size < 1 || index.off_size > 4) {
    return OTS_FAILURE();
  }

  const size_t array_size = (index.count + 1) * index.off_size;
  // less than ((64k + 1) * 4), thus does not overflow.
  const size_t object_data_offset = table.offset() + array_size;
  // does not overflow too, since offset() <= 1GB.

  if (object_data_offset >= table.length()) {
    return OTS_FAILURE();
  }

  for (unsigned i = 0; i <= index.count; ++i) {  // '<=' is not a typo.
    uint32_t rel_offset = 0;
    if (!ReadOffset(table, index.off_size, &rel_offset)) {
      return OTS_FAILURE();
    }
    if (rel_offset < 1) {
      return OTS_FAILURE();
    }
    if (i == 0 && rel_offset != 1) {
      return OTS_FAILURE();
    }

    if (rel_offset > table.length()) {
      return OTS_FAILURE();
    }

    // does not underflow.
    if (object_data_offset > table.length() - (rel_offset - 1)) {
      return OTS_FAILURE();
    }

    index.offsets.push_back(
        object_data_offset + (rel_offset - 1));  // less than length(), 1GB.
  }

  for (unsigned i = 1; i < index.offsets.size(); ++i) {
    // We allow consecutive identical offsets here for zero-length strings.
    // See http://crbug.com/69341 for more details.
    if (index.offsets[i] < index.offsets[i - 1]) {
      return OTS_FAILURE();
    }
  }

  index.offset_to_next = index.offsets.back();
  return true;
}

bool ParseNameData(
    ots::Buffer *table, const ots::CFFIndex &index, std::string* out_name) {
  uint8_t name[256] = {0};

  const size_t length = index.offsets[1] - index.offsets[0];
  // font names should be no longer than 127 characters.
  if (length > 127) {
    return OTS_FAILURE();
  }

  table->set_offset(index.offsets[0]);
  if (!table->Read(name, length)) {
    return OTS_FAILURE();
  }

  for (size_t i = 0; i < length; ++i) {
    // setting the first byte to NUL is allowed.
    if (i == 0 && name[i] == 0) continue;
    // non-ASCII characters are not recommended (except the first character).
    if (name[i] < 33 || name[i] > 126) {
      return OTS_FAILURE();
    }
    // [, ], ... are not allowed.
    if (std::strchr("[](){}<>/% ", name[i])) {
      return OTS_FAILURE();
    }
  }

  *out_name = reinterpret_cast<char *>(name);
  return true;
}

bool CheckOffset(const Operand& operand, size_t table_length) {
  if (operand.second != DICT_OPERAND_INTEGER) {
    return OTS_FAILURE();
  }
  if (operand.first >= static_cast<int32_t>(table_length) || operand.first < 0) {
    return OTS_FAILURE();
  }
  return true;
}

bool CheckSid(const Operand& operand, size_t sid_max) {
  if (operand.second != DICT_OPERAND_INTEGER) {
    return OTS_FAILURE();
  }
  if (operand.first > static_cast<int32_t>(sid_max) || operand.first < 0) {
    return OTS_FAILURE();
  }
  return true;
}

bool ParseDictDataBcd(ots::Buffer &table, std::vector<Operand> &operands) {
  bool read_decimal_point = false;
  bool read_e = false;

  uint8_t nibble = 0;
  size_t count = 0;
  while (true) {
    if (!table.ReadU8(&nibble)) {
      return OTS_FAILURE();
    }
    if ((nibble & 0xf0) == 0xf0) {
      if ((nibble & 0xf) == 0xf) {
        // TODO(yusukes): would be better to store actual double value,
        // rather than the dummy integer.
        operands.push_back(std::make_pair(0, DICT_OPERAND_REAL));
        return true;
      }
      return OTS_FAILURE();
    }
    if ((nibble & 0x0f) == 0x0f) {
      operands.push_back(std::make_pair(0, DICT_OPERAND_REAL));
      return true;
    }

    // check number format
    uint8_t nibbles[2];
    nibbles[0] = (nibble & 0xf0) >> 8;
    nibbles[1] = (nibble & 0x0f);
    for (unsigned i = 0; i < 2; ++i) {
      if (nibbles[i] == 0xd) {  // reserved number
        return OTS_FAILURE();
      }
      if ((nibbles[i] == 0xe) &&  // minus
          ((count > 0) || (i > 0))) {
        return OTS_FAILURE();  // minus sign should be the first character.
      }
      if (nibbles[i] == 0xa) {  // decimal point
        if (!read_decimal_point) {
          read_decimal_point = true;
        } else {
          return OTS_FAILURE();  // two or more points.
        }
      }
      if ((nibbles[i] == 0xb) ||  // E+
          (nibbles[i] == 0xc)) {  // E-
        if (!read_e) {
          read_e = true;
        } else {
          return OTS_FAILURE();  // two or more E's.
        }
      }
    }
    ++count;
  }
}

bool ParseDictDataEscapedOperator(ots::Buffer &table,
                                  std::vector<Operand> &operands) {
  uint8_t op = 0;
  if (!table.ReadU8(&op)) {
    return OTS_FAILURE();
  }

  if ((op <= 14) ||
      (op >= 17 && op <= 23) ||
      (op >= 30 && op <= 38)) {
    operands.push_back(std::make_pair((12 << 8) + op, DICT_OPERATOR));
    return true;
  }

  // reserved area.
  return OTS_FAILURE();
}

bool ParseDictDataNumber(ots::Buffer &table, uint8_t b0,
                         std::vector<Operand> &operands) {
  uint8_t b1 = 0;
  uint8_t b2 = 0;
  uint8_t b3 = 0;
  uint8_t b4 = 0;

  switch (b0) {
    case 28:  // shortint
      if (!table.ReadU8(&b1) ||
          !table.ReadU8(&b2)) {
        return OTS_FAILURE();
      }
      //the two-byte value needs to be casted to int16_t in order to get the right sign
      operands.push_back(std::make_pair(
          static_cast<int16_t>((b1 << 8) + b2), DICT_OPERAND_INTEGER));
      return true;

    case 29:  // longint
      if (!table.ReadU8(&b1) ||
          !table.ReadU8(&b2) ||
          !table.ReadU8(&b3) ||
          !table.ReadU8(&b4)) {
        return OTS_FAILURE();
      }
      operands.push_back(std::make_pair(
          (b1 << 24) + (b2 << 16) + (b3 << 8) + b4,
          DICT_OPERAND_INTEGER));
      return true;

    case 30:  // binary coded decimal
      return ParseDictDataBcd(table, operands);

    default:
      break;
  }

  int32_t result;
  if (b0 >=32 && b0 <=246) {
    result = b0 - 139;
  } else if (b0 >=247 && b0 <= 250) {
    if (!table.ReadU8(&b1)) {
      return OTS_FAILURE();
    }
    result = (b0 - 247) * 256 + b1 + 108;
  } else if (b0 >= 251 && b0 <= 254) {
    if (!table.ReadU8(&b1)) {
      return OTS_FAILURE();
    }
    result = -(b0 - 251) * 256 + b1 - 108;
  } else {
    return OTS_FAILURE();
  }

  operands.push_back(std::make_pair(result, DICT_OPERAND_INTEGER));
  return true;
}

bool ParseDictDataReadNext(ots::Buffer &table,
                           std::vector<Operand> &operands) {
  uint8_t op = 0;
  if (!table.ReadU8(&op)) {
    return OTS_FAILURE();
  }
  if (op <= 24) {
    if (op == 12) {
      return ParseDictDataEscapedOperator(table, operands);
    }
    operands.push_back(std::make_pair(
        static_cast<int32_t>(op), DICT_OPERATOR));
    return true;
  } else if (op <= 27 || op == 31 || op == 255) {
    // reserved area.
    return OTS_FAILURE();
  }

  return ParseDictDataNumber(table, op, operands);
}

bool OperandsOverflow(std::vector<Operand>& operands, bool cff2) {
  // An operator may be preceded by up to a maximum of 48 operands in CFF1 and
  // 513 operands in CFF2.
  if ((cff2 && operands.size() > ots::kMaxCFF2ArgumentStack) ||
      (!cff2 && operands.size() > ots::kMaxCFF1ArgumentStack)) {
    return true;
  }
  return false;
}

bool ParseDictDataReadOperands(ots::Buffer& dict,
                               std::vector<Operand>& operands,
                               bool cff2) {
  if (!ParseDictDataReadNext(dict, operands)) {
    return OTS_FAILURE();
  }
  if (operands.empty()) {
    return OTS_FAILURE();
  }
  if (OperandsOverflow(operands, cff2)) {
    return OTS_FAILURE();
  }
  return true;
}

bool ValidCFF2DictOp(int32_t op, DICT_DATA_TYPE type) {
  if (type == DICT_DATA_TOPLEVEL) {
    switch (op) {
      case (12 << 8) + 7:  // FontMatrix
      case 17:              // CharStrings
      case (12 << 8) + 36: // FDArray
      case (12 << 8) + 37: // FDSelect
      case 24:              // vstore
        return true;
      default:
        return false;
    }
  } else if (type == DICT_DATA_FDARRAY) {
    if (op == 18) // Private DICT
      return true;
  } else if (type == DICT_DATA_PRIVATE) {
    switch (op) {
      case (12 << 8) + 14: // ForceBold
      case (12 << 8) + 19: // initialRandomSeed
      case 20:              // defaultWidthX
      case 21:              // nominalWidthX
        return false;
      default:
        return true;
    }
  }

  return false;
}

bool ParsePrivateDictData(
    ots::Buffer &table, size_t offset, size_t dict_length,
    DICT_DATA_TYPE type, ots::OpenTypeCFF *out_cff) {
  ots::Buffer dict(table.buffer() + offset, dict_length);
  std::vector<Operand> operands;
  bool cff2 = (out_cff->major == 2);
  bool blend_seen = false;
  int32_t vsindex = 0;

  // Since a Private DICT for FDArray might not have a Local Subr (e.g. Hiragino
  // Kaku Gothic Std W8), we create an empty Local Subr here to match the size
  // of FDArray the size of |local_subrs_per_font|.
  // For CFF2, |vsindex_per_font| gets a similar treatment.
  if (type == DICT_DATA_FDARRAY) {
    out_cff->local_subrs_per_font.push_back(new ots::CFFIndex);
    if (cff2) {
      out_cff->vsindex_per_font.push_back(vsindex);
    }
  }

  while (dict.offset() < dict.length()) {
    if (!ParseDictDataReadOperands(dict, operands, cff2)) {
      return OTS_FAILURE();
    }
    if (operands.back().second != DICT_OPERATOR) {
      continue;
    }

    // got operator
    const int32_t op = operands.back().first;
    operands.pop_back();

    if (cff2 && !ValidCFF2DictOp(op, DICT_DATA_PRIVATE)) {
      return OTS_FAILURE();
    }

    bool clear_operands = true;
    switch (op) {
      // hints
      case 6:  // BlueValues
      case 7:  // OtherBlues
      case 8:  // FamilyBlues
      case 9:  // FamilyOtherBlues
        if ((operands.size() % 2) != 0) {
          return OTS_FAILURE();
        }
        break;

      // array
      case (12 << 8) + 12:  // StemSnapH (delta)
      case (12 << 8) + 13:  // StemSnapV (delta)
        if (operands.empty()) {
          return OTS_FAILURE();
        }
        break;

      // number
      case 10:  // StdHW
      case 11:  // StdVW
      case 20:  // defaultWidthX
      case 21:  // nominalWidthX
      case (12 << 8) + 9:   // BlueScale
      case (12 << 8) + 10:  // BlueShift
      case (12 << 8) + 11:  // BlueFuzz
      case (12 << 8) + 17:  // LanguageGroup
      case (12 << 8) + 18:  // ExpansionFactor
      case (12 << 8) + 19:  // initialRandomSeed
        if (operands.size() != 1) {
          return OTS_FAILURE();
        }
        break;

      // Local Subrs INDEX, offset(self)
      case 19: {
        if (operands.size() != 1) {
          return OTS_FAILURE();
        }
        if (operands.back().second != DICT_OPERAND_INTEGER) {
          return OTS_FAILURE();
        }
        // In theory a negative operand could occur here, if the Local Subrs
        // were stored before the Private dict, but this does not seem to be
        // well supported by implementations, and mishandling of a negative
        // offset (e.g. by using unsigned offset arithmetic) might become a
        // vector for exploitation.  AFAIK no major font creation tool will
        // generate such an offset, so to be on the safe side, we don't allow
        // it here.
        if (operands.back().first >= 1024 * 1024 * 1024 || operands.back().first < 0) {
          return OTS_FAILURE();
        }
        if (operands.back().first + offset >= table.length()) {
          return OTS_FAILURE();
        }
        // parse "16. Local Subrs INDEX"
        table.set_offset(operands.back().first + offset);
        ots::CFFIndex *local_subrs_index = NULL;
        if (type == DICT_DATA_FDARRAY) {
          if (out_cff->local_subrs_per_font.empty()) {
            return OTS_FAILURE();  // not reached.
          }
          local_subrs_index = out_cff->local_subrs_per_font.back();
        } else { // type == DICT_DATA_TOPLEVEL
          if (out_cff->local_subrs) {
            return OTS_FAILURE();  // two or more local_subrs?
          }
          local_subrs_index = new ots::CFFIndex;
          out_cff->local_subrs = local_subrs_index;
        }
        if (!ParseIndex(table, *local_subrs_index, cff2)) {
          return OTS_FAILURE();
        }
        break;
      }

      // boolean
      case (12 << 8) + 14:  // ForceBold
        if (operands.size() != 1) {
          return OTS_FAILURE();
        }
        if (operands.back().second != DICT_OPERAND_INTEGER) {
          return OTS_FAILURE();
        }
        if (operands.back().first >= 2 || operands.back().first < 0) {
          return OTS_FAILURE();
        }
        break;

      case 22: { // vsindex
        if (!cff2) {
          return OTS_FAILURE();
        }
        if (operands.size() != 1) {
          return OTS_FAILURE();
        }
        if (operands.back().second != DICT_OPERAND_INTEGER) {
          return OTS_FAILURE();
        }
        if (blend_seen) {
          return OTS_FAILURE();
        }
        vsindex = operands.back().first;
        if (vsindex < 0 ||
            vsindex >= static_cast<int32_t>(out_cff->region_index_count.size())) {
          return OTS_FAILURE();
        }
        out_cff->vsindex_per_font.back() = vsindex;
        break;
      }

      case 23: { // blend
        if (!cff2) {
          return OTS_FAILURE();
        }
        if (operands.size() < 1) {
          return OTS_FAILURE();
        }
        if (vsindex >= static_cast<int32_t>(out_cff->region_index_count.size())) {
          return OTS_FAILURE();
        }
        uint16_t k = out_cff->region_index_count.at(vsindex);
          
        if (operands.back().first > static_cast<uint16_t>(0xffff) || operands.back().first < 0){
          return OTS_FAILURE();
        }
        uint16_t n = static_cast<uint16_t>(operands.back().first);
        if (operands.size() < n * (k + 1) + 1) {
          return OTS_FAILURE();
        }
        size_t operands_size = operands.size();
        // Keep the 1st n operands on the stack for the next operator to use
        // and pop the rest. There can be multiple consecutive blend operator,
        // so this makes sure the operands of all of them are kept on the
        // stack.
        while (operands.size() > operands_size - ((n * k) + 1))
          operands.pop_back();
        clear_operands = false;
        blend_seen = true;
        break;
      }

      default:
        return OTS_FAILURE();
    }
    if (clear_operands) {
      operands.clear();
    }
  }

  return true;
}

bool ParseVariationStore(ots::OpenTypeCFF& out_cff, ots::Buffer& table) {
  uint16_t length;

  if (!table.ReadU16(&length)) {
    return OTS_FAILURE();
  }

  // Empty VariationStore is allowed.
  if (!length) {
    return true;
  }

  if (length > table.remaining()) {
    return OTS_FAILURE();
  }

  if (!ParseItemVariationStore(out_cff.GetFont(),
                               table.buffer() + table.offset(), length,
                               &(out_cff.region_index_count))) {
    return OTS_FAILURE();
  }

  return true;
}

bool ParseDictData(ots::Buffer& table, ots::Buffer& dict,
                   uint16_t glyphs, size_t sid_max, DICT_DATA_TYPE type,
                   ots::OpenTypeCFF *out_cff);

bool ParseDictData(ots::Buffer& table, const ots::CFFIndex &index,
                   uint16_t glyphs, size_t sid_max, DICT_DATA_TYPE type,
                   ots::OpenTypeCFF *out_cff) {
  for (unsigned i = 1; i < index.offsets.size(); ++i) {
    size_t dict_length = index.offsets[i] - index.offsets[i - 1];
    ots::Buffer dict(table.buffer() + index.offsets[i - 1], dict_length);

    if (!ParseDictData(table, dict, glyphs, sid_max, type, out_cff)) {
      return OTS_FAILURE();
    }
  }
  return true;
}

bool ParseDictData(ots::Buffer& table, ots::Buffer& dict,
                   uint16_t glyphs, size_t sid_max, DICT_DATA_TYPE type,
                   ots::OpenTypeCFF *out_cff) {
  bool cff2 = (out_cff->major == 2);
  std::vector<Operand> operands;

  FONT_FORMAT font_format = FORMAT_UNKNOWN;
  bool have_ros = false;
  bool have_charstrings = false;
  bool have_vstore = false;
  size_t charset_offset = 0;
  bool have_private = false;

  if (cff2) {
    // Parse VariationStore first, since it might be referenced in other places
    // (e.g. FDArray) that might be parsed after it.
    size_t dict_offset = dict.offset();
    while (dict.offset() < dict.length()) {
      if (!ParseDictDataReadOperands(dict, operands, cff2)) {
        return OTS_FAILURE();
      }
      if (operands.back().second != DICT_OPERATOR) continue;

      // got operator
      const int32_t op = operands.back().first;
      operands.pop_back();

      if (op == 18 && type == DICT_DATA_FDARRAY) {
        have_private = true;
      }

      if (op == 24) {  // vstore
        if (type != DICT_DATA_TOPLEVEL) {
          return OTS_FAILURE();
        }
        if (operands.size() != 1) {
          return OTS_FAILURE();
        }
        if (!CheckOffset(operands.back(), table.length())) {
          return OTS_FAILURE();
        }
        // parse "VariationStore Data Contents"
        table.set_offset(operands.back().first);
        if (!ParseVariationStore(*out_cff, table)) {
          return OTS_FAILURE();
        }
        break;
      }
      operands.clear();
    }
    operands.clear();
    dict.set_offset(dict_offset);

    if (type == DICT_DATA_FDARRAY && !have_private) {
      return OTS_FAILURE();  // CFF2 FD must have PrivateDICT entry (even if 0, 0)
    }

  }

  while (dict.offset() < dict.length()) {
    if (!ParseDictDataReadOperands(dict, operands, cff2)) {
      return OTS_FAILURE();
    }
    if (operands.back().second != DICT_OPERATOR) continue;

    // got operator
    const int32_t op = operands.back().first;
    operands.pop_back();

    if (cff2 && !ValidCFF2DictOp(op, type)) {
      return OTS_FAILURE();
    }

    switch (op) {
      // SID
      case 0:   // version
      case 1:   // Notice
      case 2:   // Copyright
      case 3:   // FullName
      case 4:   // FamilyName
      case (12 << 8) + 0:   // Copyright
      case (12 << 8) + 21:  // PostScript
      case (12 << 8) + 22:  // BaseFontName
      case (12 << 8) + 38:  // FontName
        if (operands.size() != 1) {
          return OTS_FAILURE();
        }
        if (!CheckSid(operands.back(), sid_max)) {
          return OTS_FAILURE();
        }
        break;

      // array
      case 5:   // FontBBox
      case 14:  // XUID
      case (12 << 8) + 7:   // FontMatrix
      case (12 << 8) + 23:  // BaseFontBlend (delta)
        if (operands.empty()) {
          return OTS_FAILURE();
        }
        break;

      // number
      case 13:  // UniqueID
      case (12 << 8) + 2:   // ItalicAngle
      case (12 << 8) + 3:   // UnderlinePosition
      case (12 << 8) + 4:   // UnderlineThickness
      case (12 << 8) + 5:   // PaintType
      case (12 << 8) + 8:   // StrokeWidth
      case (12 << 8) + 20:  // SyntheticBase
        if (operands.size() != 1) {
          return OTS_FAILURE();
        }
        break;
      case (12 << 8) + 31:  // CIDFontVersion
      case (12 << 8) + 32:  // CIDFontRevision
      case (12 << 8) + 33:  // CIDFontType
      case (12 << 8) + 34:  // CIDCount
      case (12 << 8) + 35:  // UIDBase
        if (operands.size() != 1) {
          return OTS_FAILURE();
        }
        if (font_format != FORMAT_CID_KEYED) {
          return OTS_FAILURE();
        }
        break;
      case (12 << 8) + 6:   // CharstringType
        if (operands.size() != 1) {
          return OTS_FAILURE();
        }
        if(operands.back().second != DICT_OPERAND_INTEGER) {
          return OTS_FAILURE();
        }
        if (operands.back().first != 2) {
          // We only support the "Type 2 Charstring Format."
          // TODO(yusukes): Support Type 1 format? Is that still in use?
          return OTS_FAILURE();
        }
        break;

      // boolean
      case (12 << 8) + 1:   // isFixedPitch
        if (operands.size() != 1) {
          return OTS_FAILURE();
        }
        if (operands.back().second != DICT_OPERAND_INTEGER) {
          return OTS_FAILURE();
        }
        if (operands.back().first >= 2 || operands.back().first < 0) {
          return OTS_FAILURE();
        }
        break;

      // offset(0)
      case 15:  // charset
        if (operands.size() != 1) {
          return OTS_FAILURE();
        }
        if (operands.back().first <= 2 && operands.back().first >= 0) {
          // predefined charset, ISOAdobe, Expert or ExpertSubset, is used.
          break;
        }
        if (!CheckOffset(operands.back(), table.length())) {
          return OTS_FAILURE();
        }
        if (charset_offset) {
          return OTS_FAILURE();  // multiple charset tables?
        }
        charset_offset = operands.back().first;
        break;

      case 16: {  // Encoding
        if (operands.size() != 1) {
          return OTS_FAILURE();
        }
        if (operands.back().first <= 1 && operands.back().first >= 0) {
          break;  // predefined encoding, "Standard" or "Expert", is used.
        }
        if (!CheckOffset(operands.back(), table.length())) {
          return OTS_FAILURE();
        }

        table.set_offset(operands.back().first);
        uint8_t format = 0;
        if (!table.ReadU8(&format)) {
          return OTS_FAILURE();
        }
        if (format & 0x80) {
          // supplemental encoding is not supported at the moment.
          return OTS_FAILURE();
        }
        // TODO(yusukes): support & parse supplemental encoding tables.
        break;
      }

      case 17: {  // CharStrings
        if (type != DICT_DATA_TOPLEVEL) {
          return OTS_FAILURE();
        }
        if (operands.size() != 1) {
          return OTS_FAILURE();
        }
        if (!CheckOffset(operands.back(), table.length())) {
          return OTS_FAILURE();
        }
        // parse "14. CharStrings INDEX"
        table.set_offset(operands.back().first);
        ots::CFFIndex *charstring_index = out_cff->charstrings_index;
        if (!ParseIndex(table, *charstring_index, cff2)) {
          return OTS_FAILURE();
        }
        if (charstring_index->count < 2) {
          return OTS_FAILURE();
        }
        if (have_charstrings) {
          return OTS_FAILURE();  // multiple charstring tables?
        }
        have_charstrings = true;
        if (charstring_index->count != glyphs) {
          return OTS_FAILURE();  // CFF and maxp have different number of glyphs?
        }
        break;
      }

      case 24: {  // vstore
        if (!cff2) {
          return OTS_FAILURE();
        }
        if (have_vstore) {
          return OTS_FAILURE();  // multiple vstore tables?
        }
        have_vstore = true;
        // parsed above.
        break;
      }

      case (12 << 8) + 36: {  // FDArray
        if (type != DICT_DATA_TOPLEVEL) {
          return OTS_FAILURE();
        }
        if (operands.size() != 1) {
          return OTS_FAILURE();
        }
        if (!CheckOffset(operands.back(), table.length())) {
          return OTS_FAILURE();
        }

        // parse Font DICT INDEX.
        table.set_offset(operands.back().first);
        ots::CFFIndex sub_dict_index;
        if (!ParseIndex(table, sub_dict_index, cff2)) {
          return OTS_FAILURE();
        }
        if (!ParseDictData(table, sub_dict_index,
                           glyphs, sid_max, DICT_DATA_FDARRAY,
                           out_cff)) {
          return OTS_FAILURE();
        }
        if (out_cff->font_dict_length != 0) {
          return OTS_FAILURE();  // two or more FDArray found.
        }
        out_cff->font_dict_length = sub_dict_index.count;
        break;
      }

      case (12 << 8) + 37: {  // FDSelect
        if (type != DICT_DATA_TOPLEVEL) {
          return OTS_FAILURE();
        }
        if (operands.size() != 1) {
          return OTS_FAILURE();
        }
        if (!CheckOffset(operands.back(), table.length())) {
          return OTS_FAILURE();
        }

        // parse FDSelect data structure
        table.set_offset(operands.back().first);
        uint8_t format = 0;
        if (!table.ReadU8(&format)) {
          return OTS_FAILURE();
        }
        if (format == 0) {
          for (uint16_t j = 0; j < glyphs; ++j) {
            uint8_t fd_index = 0;
            if (!table.ReadU8(&fd_index)) {
              return OTS_FAILURE();
            }
            (out_cff->fd_select)[j] = fd_index;
          }
        } else if (format == 3) {
          uint16_t n_ranges = 0;
          if (!table.ReadU16(&n_ranges)) {
            return OTS_FAILURE();
          }
          if (n_ranges == 0) {
            return OTS_FAILURE();
          }

          uint16_t last_gid = 0;
          uint8_t fd_index = 0;
          for (unsigned j = 0; j < n_ranges; ++j) {
            uint16_t first = 0;  // GID
            if (!table.ReadU16(&first)) {
              return OTS_FAILURE();
            }

            // Sanity checks.
            if ((j == 0) && (first != 0)) {
              return OTS_FAILURE();
            }
            if ((j != 0) && (last_gid >= first)) {
              return OTS_FAILURE();  // not increasing order.
            }
            if (first >= glyphs) {
              return OTS_FAILURE();  // invalid gid.
            }

            // Copy the mapping to |out_cff->fd_select|.
            if (j != 0) {
              for (auto k = last_gid; k < first; ++k) {
                if (!out_cff->fd_select.insert(
                        std::make_pair(k, fd_index)).second) {
                  return OTS_FAILURE();
                }
              }
            }

            if (!table.ReadU8(&fd_index)) {
              return OTS_FAILURE();
            }
            last_gid = first;
          }
          uint16_t sentinel = 0;
          if (!table.ReadU16(&sentinel)) {
            return OTS_FAILURE();
          }
          if (last_gid >= sentinel) {
            return OTS_FAILURE();
          }
          if (sentinel > glyphs) {
            return OTS_FAILURE();  // invalid gid.
          }
          for (auto k = last_gid; k < sentinel; ++k) {
            if (!out_cff->fd_select.insert(
                    std::make_pair(k, fd_index)).second) {
              return OTS_FAILURE();
            }
          }
        } else if (cff2 && format == 4) {
          uint32_t n_ranges = 0;
          if (!table.ReadU32(&n_ranges)) {
            return OTS_FAILURE();
          }
          if (n_ranges == 0) {
            return OTS_FAILURE();
          }

          uint32_t last_gid = 0;
          uint16_t fd_index = 0;
          for (unsigned j = 0; j < n_ranges; ++j) {
            uint32_t first = 0;  // GID
            if (!table.ReadU32(&first)) {
              return OTS_FAILURE();
            }

            // Sanity checks.
            if ((j == 0) && (first != 0)) {
              return OTS_FAILURE();
            }
            if ((j != 0) && (last_gid >= first)) {
              return OTS_FAILURE();  // not increasing order.
            }
            if (first >= glyphs) {
              return OTS_FAILURE();  // invalid gid.
            }

            // Copy the mapping to |out_cff->fd_select|.
            if (j != 0) {
              for (auto k = last_gid; k < first; ++k) {
                if (!out_cff->fd_select.insert(
                        std::make_pair(k, fd_index)).second) {
                  return OTS_FAILURE();
                }
              }
            }

            if (!table.ReadU16(&fd_index)) {
              return OTS_FAILURE();
            }
            last_gid = first;
          }
          uint32_t sentinel = 0;
          if (!table.ReadU32(&sentinel)) {
            return OTS_FAILURE();
          }
          if (last_gid >= sentinel) {
            return OTS_FAILURE();
          }
          if (sentinel > glyphs) {
            return OTS_FAILURE();  // invalid gid.
          }
          for (auto k = last_gid; k < sentinel; ++k) {
            if (!out_cff->fd_select.insert(
                    std::make_pair(k, fd_index)).second) {
              return OTS_FAILURE();
            }
          }
        } else {
          // unknown format
          return OTS_FAILURE();
        }
        break;
      }

      // Private DICT (2 * number)
      case 18: {
        if (operands.size() != 2) {
          return OTS_FAILURE();
        }
        if (operands.back().second != DICT_OPERAND_INTEGER) {
          return OTS_FAILURE();
        }
        const int32_t private_offset = operands.back().first;
        operands.pop_back();
        if (operands.back().second != DICT_OPERAND_INTEGER) {
          return OTS_FAILURE();
        }
        const int32_t private_length = operands.back().first;
        if (private_offset > static_cast<int32_t>(table.length())) {
          return OTS_FAILURE();
        }
        if (private_length >= static_cast<int32_t>(table.length()) || private_length < 0) {
          return OTS_FAILURE();
        }
        if (private_length + private_offset > static_cast<int32_t>(table.length()) || private_length + private_offset < 0) {
          return OTS_FAILURE();
        }
        // parse "15. Private DICT data"
        if (!ParsePrivateDictData(table, private_offset, private_length,
                                  type, out_cff)) {
          return OTS_FAILURE();
        }
        break;
      }

      // ROS
      case (12 << 8) + 30:
        if (font_format != FORMAT_UNKNOWN) {
          return OTS_FAILURE();
        }
        font_format = FORMAT_CID_KEYED;
        if (operands.size() != 3) {
          return OTS_FAILURE();
        }
        // check SIDs
        operands.pop_back();  // ignore the first number.
        if (!CheckSid(operands.back(), sid_max)) {
          return OTS_FAILURE();
        }
        operands.pop_back();
        if (!CheckSid(operands.back(), sid_max)) {
          return OTS_FAILURE();
        }
        if (have_ros) {
          return OTS_FAILURE();  // multiple ROS tables?
        }
        have_ros = true;
        break;

      default:
        return OTS_FAILURE();
    }
    operands.clear();

    if (font_format == FORMAT_UNKNOWN) {
      font_format = FORMAT_OTHER;
    }
  }

  // parse "13. Charsets"
  if (charset_offset) {
    table.set_offset(charset_offset);
    uint8_t format = 0;
    if (!table.ReadU8(&format)) {
      return OTS_FAILURE();
    }
    switch (format) {
      case 0:
        for (uint16_t j = 1 /* .notdef is omitted */; j < glyphs; ++j) {
          uint16_t sid = 0;
          if (!table.ReadU16(&sid)) {
            return OTS_FAILURE();
          }
          if (!have_ros && (sid > sid_max)) {
            return OTS_FAILURE();
          }
          // TODO(yusukes): check CIDs when have_ros is true.
        }
        break;

      case 1:
      case 2: {
        uint32_t total = 1;  // .notdef is omitted.
        while (total < glyphs) {
          uint16_t sid = 0;
          if (!table.ReadU16(&sid)) {
            return OTS_FAILURE();
          }
          if (!have_ros && (sid > sid_max)) {
            return OTS_FAILURE();
          }
          // TODO(yusukes): check CIDs when have_ros is true.

          if (format == 1) {
            uint8_t left = 0;
            if (!table.ReadU8(&left)) {
              return OTS_FAILURE();
            }
            total += (left + 1);
          } else {
            uint16_t left = 0;
            if (!table.ReadU16(&left)) {
              return OTS_FAILURE();
            }
            total += (left + 1);
          }
        }
        break;
      }

      default:
        return OTS_FAILURE();
    }
  }
  return true;
}

}  // namespace

namespace ots {

bool OpenTypeCFF::ValidateFDSelect(uint16_t num_glyphs) {
  for (const auto& fd_select : this->fd_select) {
    if (fd_select.first >= num_glyphs) {
      return Error("Invalid glyph index in FDSelect: %d >= %d\n",
                   fd_select.first, num_glyphs);
    }
    if (fd_select.second >= this->font_dict_length) {
      return Error("Invalid FD index: %d >= %d\n",
                   fd_select.second, this->font_dict_length);
    }
  }
  return true;
}

bool OpenTypeCFF::Parse(const uint8_t *data, size_t length) {
  Buffer table(data, length);

  Font *font = GetFont();

  this->m_data = data;
  this->m_length = length;

  // parse "6. Header" in the Adobe Compact Font Format Specification
  uint8_t major = 0;
  uint8_t minor = 0;
  uint8_t hdr_size = 0;
  uint8_t off_size = 0;
  if (!table.ReadU8(&major) ||
      !table.ReadU8(&minor) ||
      !table.ReadU8(&hdr_size) ||
      !table.ReadU8(&off_size)) {
    return Error("Failed to read table header");
  }

  if (off_size < 1 || off_size > 4) {
    return Error("Bad offSize: %d", off_size);
  }

  if (major != 1 || minor != 0) {
    return Error("Unsupported table version: %d.%d", major, minor);
  }

  this->major = major;

  if (hdr_size != 4 || hdr_size >= length) {
    return Error("Bad hdrSize: %d", hdr_size);
  }

  // parse "7. Name INDEX"
  table.set_offset(hdr_size);
  CFFIndex name_index;
  if (!ParseIndex(table, name_index)) {
    return Error("Failed to parse Name INDEX");
  }
  if (name_index.count != 1 || name_index.offsets.size() != 2) {
    return Error("Name INDEX must contain only one entry, not %d",
                 name_index.count);
  }
  if (!ParseNameData(&table, name_index, &(this->name))) {
    return Error("Failed to parse Name INDEX data");
  }

  // parse "8. Top DICT INDEX"
  table.set_offset(name_index.offset_to_next);
  CFFIndex top_dict_index;
  if (!ParseIndex(table, top_dict_index)) {
    return Error("Failed to parse Top DICT INDEX");
  }
  if (top_dict_index.count != 1) {
    return Error("Top DICT INDEX must contain only one entry, not %d",
                 top_dict_index.count);
  }

  // parse "10. String INDEX"
  table.set_offset(top_dict_index.offset_to_next);
  CFFIndex string_index;
  if (!ParseIndex(table, string_index)) {
    return Error("Failed to parse String INDEX");
  }
  if (string_index.count >= 65000 - kNStdString) {
    return Error("Too many entries in String INDEX: %d", string_index.count);
  }

  OpenTypeMAXP *maxp = static_cast<OpenTypeMAXP*>(
    font->GetTypedTable(OTS_TAG_MAXP));
  if (!maxp) {
    return Error("Required maxp table missing");
  }
  const uint16_t num_glyphs = maxp->num_glyphs;
  const size_t sid_max = string_index.count + kNStdString;
  // string_index.count == 0 is allowed.

  // parse "9. Top DICT Data"
  this->charstrings_index = new ots::CFFIndex;
  if (!ParseDictData(table, top_dict_index,
                     num_glyphs, sid_max,
                     DICT_DATA_TOPLEVEL, this)) {
    return Error("Failed to parse Top DICT Data");
  }

  // parse "16. Global Subrs INDEX"
  table.set_offset(string_index.offset_to_next);
  CFFIndex global_subrs_index;
  if (!ParseIndex(table, global_subrs_index)) {
    return Error("Failed to parse Global Subrs INDEX");
  }

  // Check if all fd and glyph indices in FDSelect are valid.
  if (!ValidateFDSelect(num_glyphs)) {
    return Error("Failed to validate FDSelect");
  }

  // Check if all charstrings (font hinting code for each glyph) are valid.
  if (!ValidateCFFCharStrings(*this, global_subrs_index, &table)) {
    return Error("Failed validating CharStrings INDEX");
  }

  return true;
}

bool OpenTypeCFF::Serialize(OTSStream *out) {
  if (!out->Write(this->m_data, this->m_length)) {
    return Error("Failed to write table");
  }
  return true;
}

OpenTypeCFF::~OpenTypeCFF() {
  for (size_t i = 0; i < this->local_subrs_per_font.size(); ++i) {
    delete (this->local_subrs_per_font)[i];
  }
  delete this->charstrings_index;
  delete this->local_subrs;
}

bool OpenTypeCFF2::Parse(const uint8_t *data, size_t length) {
  Buffer table(data, length);

  Font *font = GetFont();

  this->m_data = data;
  this->m_length = length;

  // parse "6. Header"
  uint8_t major = 0;
  uint8_t minor = 0;
  uint8_t hdr_size = 0;
  uint16_t top_dict_size = 0;
  if (!table.ReadU8(&major) ||
      !table.ReadU8(&minor) ||
      !table.ReadU8(&hdr_size) ||
      !table.ReadU16(&top_dict_size)) {
    return Error("Failed to read table header");
  }

  if (major != 2 || minor != 0) {
    return Error("Unsupported table version: %d.%d", major, minor);
  }

  this->major = major;

  if (hdr_size >= length) {
    return Error("Bad hdrSize: %d", hdr_size);
  }

  if (top_dict_size == 0 || hdr_size + top_dict_size > length) {
    return Error("Bad topDictLength: %d", top_dict_size);
  }

  OpenTypeMAXP *maxp = static_cast<OpenTypeMAXP*>(
    font->GetTypedTable(OTS_TAG_MAXP));
  if (!maxp) {
    return Error("Required maxp table missing");
  }
  const uint16_t num_glyphs = maxp->num_glyphs;
  const size_t sid_max = kNStdString;

  // parse "7. Top DICT Data"
  ots::Buffer top_dict(data + hdr_size, top_dict_size);
  table.set_offset(hdr_size);
  this->charstrings_index = new ots::CFFIndex;
  if (!ParseDictData(table, top_dict,
                     num_glyphs, sid_max,
                     DICT_DATA_TOPLEVEL, this)) {
    return Error("Failed to parse Top DICT Data");
  }

  // parse "9. Global Subrs INDEX"
  table.set_offset(hdr_size + top_dict_size);
  CFFIndex global_subrs_index;
  if (!ParseIndex(table, global_subrs_index, true)) {
    return Error("Failed to parse Global Subrs INDEX");
  }

  // Check if all fd and glyph indices in FDSelect are valid.
  if (!ValidateFDSelect(num_glyphs)) {
    return Error("Failed to validate FDSelect");
  }

  // Check if all charstrings (font hinting code for each glyph) are valid.
  if (!ValidateCFFCharStrings(*this, global_subrs_index, &table)) {
    return Error("Failed validating CharStrings INDEX");
  }

  return true;
}

bool OpenTypeCFF2::Serialize(OTSStream *out) {
  if (!out->Write(this->m_data, this->m_length)) {
    return Error("Failed to write table");
  }
  return true;
}

}  // namespace ots

#undef TABLE_NAME
