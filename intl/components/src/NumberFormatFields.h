/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef intl_components_NumberFormatFields_h_
#define intl_components_NumberFormatFields_h_
#include "mozilla/intl/ICUError.h"
#include "mozilla/Maybe.h"
#include "mozilla/Result.h"
#include "mozilla/Vector.h"

struct UFormattedNumber;
struct UFormattedValue;

namespace mozilla::intl {

enum class NumberPartType : int16_t {
  ApproximatelySign,
  Compact,
  Currency,
  Decimal,
  ExponentInteger,
  ExponentMinusSign,
  ExponentSeparator,
  Fraction,
  Group,
  Infinity,
  Integer,
  Literal,
  MinusSign,
  Nan,
  Percent,
  PlusSign,
  Unit,
};

enum class NumberPartSource : int16_t { Shared, Start, End };

// Because parts fully partition the formatted string, we only track the
// index of the end of each part -- the beginning is implicitly the last
// part's end.
struct NumberPart {
  NumberPartType type;
  NumberPartSource source;
  size_t endIndex;

  bool operator==(const NumberPart& rhs) const {
    return type == rhs.type && source == rhs.source && endIndex == rhs.endIndex;
  }
  bool operator!=(const NumberPart& rhs) const { return !(*this == rhs); }
};

using NumberPartVector = mozilla::Vector<NumberPart, 8>;

struct NumberFormatField {
  uint32_t begin;
  uint32_t end;
  NumberPartType type;

  // Needed for vector-resizing scratch space.
  NumberFormatField() = default;

  NumberFormatField(uint32_t begin, uint32_t end, NumberPartType type)
      : begin(begin), end(end), type(type) {}
};

struct NumberPartSourceMap {
  struct Range {
    uint32_t begin = 0;
    uint32_t end = 0;
  };

  // Begin and end position of the start range.
  Range start;

  // Begin and end position of the end range.
  Range end;

  NumberPartSource source(uint32_t endIndex) {
    if (start.begin < endIndex && endIndex <= start.end) {
      return NumberPartSource::Start;
    }
    if (end.begin < endIndex && endIndex <= end.end) {
      return NumberPartSource::End;
    }
    return NumberPartSource::Shared;
  }

  NumberPartSource source(const NumberFormatField& field) {
    return source(field.end);
  }
};

class NumberFormatFields {
  using FieldsVector = Vector<NumberFormatField, 16>;

  FieldsVector fields_;

 public:
  [[nodiscard]] bool append(NumberPartType type, int32_t begin, int32_t end);

  [[nodiscard]] bool toPartsVector(size_t overallLength,
                                   NumberPartVector& parts) {
    return toPartsVector(overallLength, {}, parts);
  }

  [[nodiscard]] bool toPartsVector(size_t overallLength,
                                   const NumberPartSourceMap& sourceMap,
                                   NumberPartVector& parts);
};

Result<std::u16string_view, ICUError> FormatResultToParts(
    const UFormattedNumber* value, Maybe<double> number, bool isNegative,
    bool formatForUnit, NumberPartVector& parts);

Result<std::u16string_view, ICUError> FormatResultToParts(
    const UFormattedValue* value, Maybe<double> number, bool isNegative,
    bool formatForUnit, NumberPartVector& parts);

}  // namespace mozilla::intl

#endif
