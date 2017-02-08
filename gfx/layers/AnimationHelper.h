/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_AnimationHelper_h
#define mozilla_layers_AnimationHelper_h

#include "mozilla/ComputedTimingFunction.h" // for ComputedTimingFunction
#include "mozilla/TimeStamp.h"          // for TimeStamp


namespace mozilla {
  class StyleAnimationValue;
namespace layers {
class Animation;

typedef InfallibleTArray<layers::Animation> AnimationArray;

struct AnimData {
  InfallibleTArray<mozilla::StyleAnimationValue> mStartValues;
  InfallibleTArray<mozilla::StyleAnimationValue> mEndValues;
  InfallibleTArray<Maybe<mozilla::ComputedTimingFunction>> mFunctions;
};

class AnimationHelper
{
public:

  static bool
  SampleAnimationForEachNode(TimeStamp aPoint,
                             AnimationArray& aAnimations,
                             InfallibleTArray<AnimData>& aAnimationData,
                             StyleAnimationValue& aAnimationValue,
                             bool& aHasInEffectAnimations);

  static void
  SetAnimations(AnimationArray& aAnimations,
                InfallibleTArray<AnimData>& aAnimData,
                StyleAnimationValue& aBaseAnimationStyle);
  static uint64_t GetNextCompositorAnimationsId();
};

} // namespace layers
} // namespace mozilla

#endif // mozilla_layers_AnimationHelper_h
