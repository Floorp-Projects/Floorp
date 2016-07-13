/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/AnimationEffectTimingReadOnly.h"

#include "mozilla/AnimationUtils.h"
#include "mozilla/dom/AnimatableBinding.h"
#include "mozilla/dom/AnimationEffectTimingReadOnlyBinding.h"
#include "mozilla/dom/CSSPseudoElement.h"
#include "mozilla/dom/KeyframeEffectBinding.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(AnimationEffectTimingReadOnly, mDocument)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(AnimationEffectTimingReadOnly, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(AnimationEffectTimingReadOnly, Release)

JSObject*
AnimationEffectTimingReadOnly::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return AnimationEffectTimingReadOnlyBinding::Wrap(aCx, this, aGivenProto);
}

void
AnimationEffectTimingReadOnly::GetDuration(
    OwningUnrestrictedDoubleOrString& aRetVal) const
{
  if (mTiming.mDuration) {
    aRetVal.SetAsUnrestrictedDouble() = mTiming.mDuration->ToMilliseconds();
  } else {
    aRetVal.SetAsString().AssignLiteral("auto");
  }
}

void
AnimationEffectTimingReadOnly::GetEasing(nsString& aRetVal) const
{
  if (mTiming.mFunction) {
    mTiming.mFunction->AppendToString(aRetVal);
  } else {
    aRetVal.AssignLiteral("linear");
  }
}

} // namespace dom
} // namespace mozilla
