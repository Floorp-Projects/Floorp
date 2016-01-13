/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/AnimationEffectTimingReadOnly.h"
#include "mozilla/dom/AnimationEffectTimingReadOnlyBinding.h"

namespace mozilla {

bool
AnimationTiming::operator==(const AnimationTiming& aOther) const
{
  bool durationEqual;
  if (mDuration.IsUnrestrictedDouble()) {
    durationEqual = aOther.mDuration.IsUnrestrictedDouble() &&
                    (mDuration.GetAsUnrestrictedDouble() ==
                     aOther.mDuration.GetAsUnrestrictedDouble());
  } else if (mDuration.IsString()) {
    durationEqual = aOther.mDuration.IsString() &&
                    (mDuration.GetAsString() ==
                     aOther.mDuration.GetAsString());
  } else {
    // Check if both are uninitialized
    durationEqual = !aOther.mDuration.IsUnrestrictedDouble() &&
                    !aOther.mDuration.IsString();
  }
  return durationEqual &&
         mDelay == aOther.mDelay &&
         mIterations == aOther.mIterations &&
         mDirection == aOther.mDirection &&
         mFill == aOther.mFill;
}

namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(AnimationEffectTimingReadOnly, mParent)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(AnimationEffectTimingReadOnly, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(AnimationEffectTimingReadOnly, Release)

JSObject*
AnimationEffectTimingReadOnly::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return AnimationEffectTimingReadOnlyBinding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla
