/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_StyleAnimationValueInlines_h_
#define mozilla_StyleAnimationValueInlines_h_

#include "mozilla/StyleAnimationValue.h"
#include "mozilla/ServoBindings.h"

namespace mozilla {

bool
AnimationValue::operator==(const AnimationValue& aOther) const
{
    // mGecko and mServo are mutual exclusive, one of them must be null
    if (mServo) {
      return Servo_AnimationValue_DeepEqual(mServo, aOther.mServo);
    }
    return mGecko == aOther.mGecko;
}

float
AnimationValue::GetOpacity() const
{
  return mServo ? Servo_AnimationValue_GetOpacity(mServo)
                : mGecko.GetFloatValue();
}

gfxSize
AnimationValue::GetScaleValue(const nsIFrame* aFrame) const
{
  if (mServo) {
    RefPtr<nsCSSValueSharedList> list;
    Servo_AnimationValue_GetTransform(mServo, &list);
    return nsStyleTransformMatrix::GetScaleValue(list, aFrame);
  }
  return mGecko.GetScaleValue(aFrame);
}

void
AnimationValue::SerializeSpecifiedValue(nsCSSPropertyID aProperty,
                                        nsAString& aString) const
{
  if (mServo) {
    Servo_AnimationValue_Serialize(mServo, aProperty, &aString);
    return;
  }

  DebugOnly<bool> uncomputeResult =
    StyleAnimationValue::UncomputeValue(aProperty, mGecko, aString);
  MOZ_ASSERT(uncomputeResult, "failed to uncompute StyleAnimationValue");
}

} // namespace mozilla

#endif // mozilla_StyleAnimationValueInlines_h_
