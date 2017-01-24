/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServoAnimationRule.h"

namespace mozilla {

void
ServoAnimationRule::AddValue(nsCSSPropertyID aProperty,
                             RawServoAnimationValue* aValue)
{
  MOZ_ASSERT(aProperty != eCSSProperty_UNKNOWN,
             "Unexpected css property");
  mAnimationValues.Put(aProperty, aValue);
}

RawServoDeclarationBlockStrong
ServoAnimationRule::GetValues() const
{
  // FIXME: Pass the hash table into the FFI directly.
  nsTArray<const RawServoAnimationValue*> values(mAnimationValues.Count());
  auto iter = mAnimationValues.ConstIter();
  for (; !iter.Done(); iter.Next()) {
    values.AppendElement(iter.Data());
  }
  return Servo_AnimationValues_Uncompute(&values);
}

} // namespace mozilla
