/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ScrollStyles.h"
#include "nsStyleStruct.h"  // for nsStyleDisplay

namespace mozilla {

ScrollStyles::ScrollStyles(const nsStyleDisplay& aDisplay)
    : mHorizontal(aDisplay.mOverflowX), mVertical(aDisplay.mOverflowY) {}

bool ScrollStyles::IsHiddenInBothDirections() const {
  return mHorizontal == StyleOverflow::Hidden &&
         mVertical == StyleOverflow::Hidden;
}

}  // namespace mozilla
