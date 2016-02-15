/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_AnimationEffectTiming_h
#define mozilla_dom_AnimationEffectTiming_h

#include "mozilla/dom/AnimationEffectTimingReadOnly.h"

namespace mozilla {
namespace dom {

class AnimationEffectTiming : public AnimationEffectTimingReadOnly
{
public:
  explicit AnimationEffectTiming(const TimingParams& aTiming)
    : AnimationEffectTimingReadOnly(aTiming) { }

  JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_AnimationEffectTiming_h
