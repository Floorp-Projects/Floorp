/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/intl/BidiEmbeddingLevel.h"
#include "mozilla/Casting.h"
#include "mozilla/intl/ICU4CGlue.h"

namespace mozilla::intl {

bool BidiEmbeddingLevel::IsDefaultLTR() const { return *this == DefaultLTR(); };

bool BidiEmbeddingLevel::IsDefaultRTL() const { return *this == DefaultRTL(); };

bool BidiEmbeddingLevel::IsRTL() const {
  // If the least significant bit is 1, then the embedding level
  // is right-to-left.
  // If the least significant bit is 0, then the embedding level
  // is left-to-right.
  return (mValue & 0x1) == 1;
};

bool BidiEmbeddingLevel::IsLTR() const { return !IsRTL(); };

bool BidiEmbeddingLevel::IsSameDirection(BidiEmbeddingLevel aOther) const {
  return (((mValue ^ aOther) & 1) == 0);
}

BidiEmbeddingLevel BidiEmbeddingLevel::LTR() { return BidiEmbeddingLevel(0); };

BidiEmbeddingLevel BidiEmbeddingLevel::RTL() { return BidiEmbeddingLevel(1); };

BidiEmbeddingLevel BidiEmbeddingLevel::DefaultLTR() {
  return BidiEmbeddingLevel(kDefaultLTR);
};

BidiEmbeddingLevel BidiEmbeddingLevel::DefaultRTL() {
  return BidiEmbeddingLevel(kDefaultRTL);
};

BidiDirection BidiEmbeddingLevel::Direction() {
  return IsRTL() ? BidiDirection::RTL : BidiDirection::LTR;
};

uint8_t BidiEmbeddingLevel::Value() const { return mValue; }

}  // namespace mozilla::intl
