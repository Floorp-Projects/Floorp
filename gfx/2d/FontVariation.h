/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_FONTVARIATION_H_
#define MOZILLA_GFX_FONTVARIATION_H_

#include <stdint.h>
#include "mozilla/FloatingPoint.h"

namespace mozilla::gfx {

// An OpenType variation tag and value pair
struct FontVariation {
  uint32_t mTag;
  float mValue;

  bool operator==(const FontVariation& aOther) const {
    return mTag == aOther.mTag &&
           NumbersAreBitwiseIdentical(mValue, aOther.mValue);
  }
};

}  // namespace mozilla::gfx

#endif /* MOZILLA_GFX_FONTVARIATION_H_ */
