/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_FONTVARIATION_H_
#define MOZILLA_GFX_FONTVARIATION_H_

namespace mozilla {
namespace gfx {

// An OpenType variation tag and value pair
struct FontVariation
{
  uint32_t mTag;
  float mValue;

  bool operator==(const FontVariation& aOther) const {
    return mTag == aOther.mTag && mValue == aOther.mValue;
  }
};

} // namespace gfx
} // namespace mozilla

#endif /* MOZILLA_GFX_FONTVARIATION_H_ */
