/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef intl_components_NumberFormatFields_h_
#define intl_components_NumberFormatFields_h_
#include "mozilla/intl/NumberFormat.h"
#include "mozilla/Maybe.h"
#include "mozilla/Vector.h"

#include "unicode/unum.h"

namespace mozilla {
namespace intl {

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

Maybe<NumberPartType> GetPartTypeForNumberField(UNumberFormatFields fieldName,
                                                Maybe<double> number,
                                                bool isNegative,
                                                bool formatForUnit);

}  // namespace intl
}  // namespace mozilla

#endif
