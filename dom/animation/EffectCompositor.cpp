/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EffectCompositor.h"

#include "mozilla/dom/Animation.h"
#include "mozilla/dom/KeyframeEffect.h"
#include "mozilla/AnimationUtils.h"
#include "mozilla/EffectSet.h"
#include "nsLayoutUtils.h"

using mozilla::dom::Animation;
using mozilla::dom::KeyframeEffectReadOnly;

namespace mozilla {

/* static */ nsTArray<RefPtr<dom::Animation>>
EffectCompositor::GetAnimationsForCompositor(const nsIFrame* aFrame,
                                             nsCSSProperty aProperty)
{
  nsTArray<RefPtr<dom::Animation>> result;

  if (!nsLayoutUtils::AreAsyncAnimationsEnabled()) {
    if (nsLayoutUtils::IsAnimationLoggingEnabled()) {
      nsCString message;
      message.AppendLiteral("Performance warning: Async animations are "
                            "disabled");
      AnimationUtils::LogAsyncAnimationFailure(message);
    }
    return result;
  }

  if (aFrame->RefusedAsyncAnimation()) {
    return result;
  }

  EffectSet* effects = EffectSet::GetEffectSet(aFrame);
  if (!effects) {
    return result;
  }

  for (KeyframeEffectReadOnly* effect : *effects) {
    MOZ_ASSERT(effect && effect->GetAnimation());
    Animation* animation = effect->GetAnimation();

    if (!animation->IsPlaying()) {
      continue;
    }

    if (effect->ShouldBlockCompositorAnimations(aFrame)) {
      result.Clear();
      return result;
    }

    if (!effect->HasAnimationOfProperty(aProperty)) {
      continue;
    }

    result.AppendElement(animation);
  }

  return result;
}

} // namespace mozilla
