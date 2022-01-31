/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScrollGeneration.h"

#include <ostream>

namespace mozilla {

uint64_t ScrollGeneration::sCounter = 0;

ScrollGeneration ScrollGeneration::New() {
  uint64_t value = ++sCounter;
  return ScrollGeneration(value);
}

ScrollGeneration::ScrollGeneration() : mValue(0) {}

ScrollGeneration::ScrollGeneration(uint64_t aValue) : mValue(aValue) {}

bool ScrollGeneration::operator<(const ScrollGeneration& aOther) const {
  return mValue < aOther.mValue;
}

bool ScrollGeneration::operator==(const ScrollGeneration& aOther) const {
  return mValue == aOther.mValue;
}

bool ScrollGeneration::operator!=(const ScrollGeneration& aOther) const {
  return !(*this == aOther);
}

std::ostream& operator<<(std::ostream& aStream, const ScrollGeneration& aGen) {
  return aStream << aGen.mValue;
}

}  // namespace mozilla
