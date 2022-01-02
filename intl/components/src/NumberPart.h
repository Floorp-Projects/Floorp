/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef intl_components_NumberPart_h_
#define intl_components_NumberPart_h_

#include <cstddef>
#include <cstdint>

#include "mozilla/Vector.h"

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

}  // namespace mozilla::intl
#endif
