/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ScrollStyles.h"
#include "mozilla/WritingModes.h"
#include "nsStyleStruct.h"  // for nsStyleDisplay & nsStyleBackground::Position

namespace mozilla {

ScrollStyles::ScrollStyles(WritingMode aWritingMode, StyleOverflow aH,
                           StyleOverflow aV, const nsStyleDisplay* aDisplay)
    : mHorizontal(aH),
      mVertical(aV),
      mOverscrollBehaviorX(aDisplay->mOverscrollBehaviorX),
      mOverscrollBehaviorY(aDisplay->mOverscrollBehaviorY) {}

ScrollStyles::ScrollStyles(WritingMode aWritingMode,
                           const nsStyleDisplay* aDisplay)
    : mHorizontal(aDisplay->mOverflowX),
      mVertical(aDisplay->mOverflowY),
      mOverscrollBehaviorX(aDisplay->mOverscrollBehaviorX),
      mOverscrollBehaviorY(aDisplay->mOverscrollBehaviorY) {}

}  // namespace mozilla
