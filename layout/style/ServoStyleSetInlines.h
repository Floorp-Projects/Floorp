/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ServoStyleSetInlines_h
#define mozilla_ServoStyleSetInlines_h

#include "mozilla/ServoStyleSet.h"
#include "mozilla/ServoBindings.h"

namespace mozilla {

nscoord ServoStyleSet::EvaluateSourceSizeList(
    const StyleSourceSizeList* aSourceSizeList) const {
  return Servo_SourceSizeList_Evaluate(mRawData.get(), aSourceSizeList);
}

already_AddRefed<ComputedStyle> ServoStyleSet::ResolveServoStyle(
    const dom::Element& aElement) {
  return Servo_ResolveStyle(&aElement).Consume();
}

}  // namespace mozilla

#endif  // mozilla_ServoStyleSetInlines_h
