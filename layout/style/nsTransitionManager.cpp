/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Code to start and animate CSS transitions. */

#include "nsTransitionManager.h"
#include "mozilla/dom/Document.h"
#include "nsAnimationManager.h"

#include "nsIContent.h"
#include "mozilla/ComputedStyle.h"
#include "mozilla/MemoryReporting.h"
#include "nsCSSPropertyIDSet.h"
#include "mozilla/EffectSet.h"
#include "mozilla/ElementAnimationData.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/ServoBindings.h"
#include "mozilla/StyleAnimationValue.h"
#include "mozilla/dom/DocumentTimeline.h"
#include "mozilla/dom/Element.h"
#include "nsIFrame.h"
#include "nsCSSProps.h"
#include "nsCSSPseudoElements.h"
#include "nsDisplayList.h"
#include "nsRFPService.h"
#include "nsStyleChangeList.h"
#include "mozilla/RestyleManager.h"

using mozilla::dom::CSSTransition;
using mozilla::dom::DocumentTimeline;
using mozilla::dom::KeyframeEffect;

using namespace mozilla;
using namespace mozilla::css;

bool nsTransitionManager::UpdateTransitions(dom::Element* aElement,
                                            PseudoStyleType aPseudoType,
                                            const ComputedStyle& aOldStyle,
                                            const ComputedStyle& aNewStyle) {
  if (mPresContext->Medium() == nsGkAtoms::print) {
    // For print or print preview, ignore transitions.
    return false;
  }

  MOZ_ASSERT(mPresContext->IsDynamic());
  if (aNewStyle.StyleDisplay()->mDisplay == StyleDisplay::None) {
    StopAnimationsForElement(aElement, aPseudoType);
    return false;
  }

  auto* collection = CSSTransitionCollection::Get(aElement, aPseudoType);
  return DoUpdateTransitions(*aNewStyle.StyleUIReset(), aElement, aPseudoType,
                             collection, aOldStyle, aNewStyle);
}

// This function expands the shorthands and "all" keyword specified in
// transition-property, and then execute |aHandler| on the expanded longhand.
// |aHandler| should be a lamda function which accepts nsCSSPropertyID.
template <typename T>
static void ExpandTransitionProperty(nsCSSPropertyID aProperty, T aHandler) {
  if (aProperty == eCSSPropertyExtra_no_properties ||
      aProperty == eCSSPropertyExtra_variable ||
      aProperty == eCSSProperty_UNKNOWN) {
    // Nothing to do.
    return;
  }

  // FIXME(emilio): This should probably just use the "all" shorthand id, and we
  // should probably remove eCSSPropertyExtra_all_properties.
  if (aProperty == eCSSPropertyExtra_all_properties) {
    for (nsCSSPropertyID p = nsCSSPropertyID(0);
         p < eCSSProperty_COUNT_no_shorthands; p = nsCSSPropertyID(p + 1)) {
      if (!nsCSSProps::IsEnabled(p, CSSEnabledState::ForAllContent)) {
        continue;
      }
      aHandler(p);
    }
  } else if (nsCSSProps::IsShorthand(aProperty)) {
    CSSPROPS_FOR_SHORTHAND_SUBPROPERTIES(subprop, aProperty,
                                         CSSEnabledState::ForAllContent) {
      aHandler(*subprop);
    }
  } else {
    aHandler(aProperty);
  }
}

bool nsTransitionManager::DoUpdateTransitions(
    const nsStyleUIReset& aStyle, dom::Element* aElement,
    PseudoStyleType aPseudoType, CSSTransitionCollection*& aElementTransitions,
    const ComputedStyle& aOldStyle, const ComputedStyle& aNewStyle) {
  MOZ_ASSERT(!aElementTransitions || &aElementTransitions->mElement == aElement,
             "Element mismatch");

  // Per http://lists.w3.org/Archives/Public/www-style/2009Aug/0109.html
  // I'll consider only the transitions from the number of items in
  // 'transition-property' on down, and later ones will override earlier
  // ones (tracked using |propertiesChecked|).
  bool startedAny = false;
  nsCSSPropertyIDSet propertiesChecked;
  for (uint32_t i = aStyle.mTransitionPropertyCount; i--;) {
    // We're not going to look at any further transitions, so we can just avoid
    // looking at this if we know it will not start any transitions.
    if (i == 0 && aStyle.GetTransitionCombinedDuration(i).seconds <= 0.0f) {
      continue;
    }

    ExpandTransitionProperty(
        aStyle.GetTransitionProperty(i), [&](nsCSSPropertyID aProperty) {
          // We might have something to transition.  See if any of the
          // properties in question changed and are animatable.
          startedAny |= ConsiderInitiatingTransition(
              aProperty, aStyle, i, aElement, aPseudoType, aElementTransitions,
              aOldStyle, aNewStyle, propertiesChecked);
        });
  }

  // Stop any transitions for properties that are no longer in
  // 'transition-property', including finished transitions.
  // Also stop any transitions (and remove any finished transitions)
  // for properties that just changed (and are still in the set of
  // properties to transition), but for which we didn't just start the
  // transition.  This can happen delay and duration are both zero, or
  // because the new value is not interpolable.
  if (aElementTransitions) {
    bool checkProperties =
        aStyle.GetTransitionProperty(0) != eCSSPropertyExtra_all_properties;
    nsCSSPropertyIDSet allTransitionProperties;
    if (checkProperties) {
      for (uint32_t i = aStyle.mTransitionPropertyCount; i-- != 0;) {
        ExpandTransitionProperty(
            aStyle.GetTransitionProperty(i), [&](nsCSSPropertyID aProperty) {
              allTransitionProperties.AddProperty(
                  nsCSSProps::Physicalize(aProperty, aNewStyle));
            });
      }
    }

    OwningCSSTransitionPtrArray& animations = aElementTransitions->mAnimations;
    size_t i = animations.Length();
    MOZ_ASSERT(i != 0, "empty transitions list?");
    AnimationValue currentValue;
    do {
      --i;
      CSSTransition* anim = animations[i];
      const nsCSSPropertyID property = anim->TransitionProperty();
      if (
          // Properties no longer in `transition-property`.
          (checkProperties && !allTransitionProperties.HasProperty(property)) ||
          // Properties whose computed values changed but for which we did not
          // start a new transition (because delay and duration are both zero,
          // or because the new value is not interpolable); a new transition
          // would have anim->ToValue() matching currentValue.
          !Servo_ComputedValues_TransitionValueMatches(
              &aNewStyle, property, anim->ToValue().mServo.get())) {
        // Stop the transition.
        DoCancelTransition(aElement, aPseudoType, aElementTransitions, i);
      }
    } while (i != 0);
  }

  return startedAny;
}

static Keyframe& AppendKeyframe(double aOffset, nsCSSPropertyID aProperty,
                                AnimationValue&& aValue,
                                nsTArray<Keyframe>& aKeyframes) {
  Keyframe& frame = *aKeyframes.AppendElement();
  frame.mOffset.emplace(aOffset);
  MOZ_ASSERT(aValue.mServo);
  RefPtr<StyleLockedDeclarationBlock> decl =
      Servo_AnimationValue_Uncompute(aValue.mServo).Consume();
  frame.mPropertyValues.AppendElement(
      PropertyValuePair(aProperty, std::move(decl)));
  return frame;
}

static nsTArray<Keyframe> GetTransitionKeyframes(nsCSSPropertyID aProperty,
                                                 AnimationValue&& aStartValue,
                                                 AnimationValue&& aEndValue) {
  nsTArray<Keyframe> keyframes(2);

  AppendKeyframe(0.0, aProperty, std::move(aStartValue), keyframes);
  AppendKeyframe(1.0, aProperty, std::move(aEndValue), keyframes);

  return keyframes;
}

static bool IsTransitionable(nsCSSPropertyID aProperty) {
  return Servo_Property_IsTransitionable(aProperty);
}

static Maybe<CSSTransition::ReplacedTransitionProperties>
GetReplacedTransitionProperties(const CSSTransition* aTransition,
                                const DocumentTimeline* aTimelineToMatch) {
  Maybe<CSSTransition::ReplacedTransitionProperties> result;

  // Transition needs to be currently running on the compositor to be
  // replaceable.
  if (!aTransition || !aTransition->HasCurrentEffect() ||
      !aTransition->IsRunningOnCompositor() ||
      aTransition->GetStartTime().IsNull()) {
    return result;
  }

  // Transition needs to be running on the same timeline.
  if (aTransition->GetTimeline() != aTimelineToMatch) {
    return result;
  }

  // The transition needs to have a keyframe effect.
  const KeyframeEffect* keyframeEffect =
      aTransition->GetEffect() ? aTransition->GetEffect()->AsKeyframeEffect()
                               : nullptr;
  if (!keyframeEffect) {
    return result;
  }

  // The keyframe effect needs to be a simple transition of the original
  // transition property (i.e. not replaced with something else).
  if (keyframeEffect->Properties().Length() != 1 ||
      keyframeEffect->Properties()[0].mSegments.Length() != 1 ||
      keyframeEffect->Properties()[0].mProperty !=
          aTransition->TransitionProperty()) {
    return result;
  }

  const AnimationPropertySegment& segment =
      keyframeEffect->Properties()[0].mSegments[0];

  result.emplace(CSSTransition::ReplacedTransitionProperties(
      {aTransition->GetStartTime().Value(), aTransition->PlaybackRate(),
       keyframeEffect->SpecifiedTiming(), segment.mTimingFunction,
       segment.mFromValue, segment.mToValue}));

  return result;
}

bool nsTransitionManager::ConsiderInitiatingTransition(
    nsCSSPropertyID aProperty, const nsStyleUIReset& aStyle,
    uint32_t transitionIdx, dom::Element* aElement, PseudoStyleType aPseudoType,
    CSSTransitionCollection*& aElementTransitions,
    const ComputedStyle& aOldStyle, const ComputedStyle& aNewStyle,
    nsCSSPropertyIDSet& aPropertiesChecked) {
  // IsShorthand itself will assert if aProperty is not a property.
  MOZ_ASSERT(!nsCSSProps::IsShorthand(aProperty), "property out of range");
  NS_ASSERTION(
      !aElementTransitions || &aElementTransitions->mElement == aElement,
      "Element mismatch");

  aProperty = nsCSSProps::Physicalize(aProperty, aNewStyle);

  // A later item in transition-property already specified a transition for
  // this property, so we ignore this one.
  //
  // See http://lists.w3.org/Archives/Public/www-style/2009Aug/0109.html .
  if (aPropertiesChecked.HasProperty(aProperty)) {
    return false;
  }

  aPropertiesChecked.AddProperty(aProperty);

  if (!IsTransitionable(aProperty)) {
    return false;
  }

  float delay = aStyle.GetTransitionDelay(transitionIdx).ToMilliseconds();

  // The spec says a negative duration is treated as zero.
  float duration = std::max(
      aStyle.GetTransitionDuration(transitionIdx).ToMilliseconds(), 0.0f);

  // If the combined duration of this transition is 0 or less don't start a
  // transition.
  if (delay + duration <= 0.0f) {
    return false;
  }

  size_t currentIndex = nsTArray<KeyframeEffect>::NoIndex;
  const auto* oldTransition = [&]() -> const CSSTransition* {
    if (!aElementTransitions) {
      return nullptr;
    }
    const OwningCSSTransitionPtrArray& animations =
        aElementTransitions->mAnimations;
    for (size_t i = 0, i_end = animations.Length(); i < i_end; ++i) {
      if (animations[i]->TransitionProperty() == aProperty) {
        currentIndex = i;
        return animations[i];
      }
    }
    return nullptr;
  }();

  AnimationValue startValue, endValue;
  const StyleShouldTransitionResult result =
      Servo_ComputedValues_ShouldTransition(
          &aOldStyle, &aNewStyle, aProperty,
          oldTransition ? oldTransition->ToValue().mServo.get() : nullptr,
          &startValue.mServo, &endValue.mServo);

  // If we got a style change that changed the value to the endpoint
  // of the currently running transition, we don't want to interrupt
  // its timing function.
  // This needs to be before the !shouldAnimate && haveCurrentTransition
  // case below because we might be close enough to the end of the
  // transition that the current value rounds to the final value.  In
  // this case, we'll end up with shouldAnimate as false (because
  // there's no value change), but we need to return early here rather
  // than cancel the running transition because shouldAnimate is false!
  //
  // Likewise, if we got a style change that changed the value to the
  // endpoint of our finished transition, we also don't want to start
  // a new transition for the reasons described in
  // https://lists.w3.org/Archives/Public/www-style/2015Jan/0444.html .
  if (result.old_transition_value_matches) {
    // GetAnimationRule already called RestyleForAnimation.
    return false;
  }

  if (!result.should_animate) {
    if (oldTransition) {
      // We're in the middle of a transition, and just got a non-transition
      // style change to something that we can't animate.  This might happen
      // because we got a non-transition style change changing to the current
      // in-progress value (which is particularly easy to cause when we're
      // currently in the 'transition-delay').  It also might happen because we
      // just got a style change to a value that can't be interpolated.
      DoCancelTransition(aElement, aPseudoType, aElementTransitions,
                         currentIndex);
    }
    return false;
  }

  AnimationValue startForReversingTest = startValue;
  double reversePortion = 1.0;

  // If the new transition reverses an existing one, we'll need to
  // handle the timing differently.
  if (oldTransition && oldTransition->HasCurrentEffect() &&
      oldTransition->StartForReversingTest() == endValue) {
    // Compute the appropriate negative transition-delay such that right
    // now we'd end up at the current position.
    double valuePortion =
        oldTransition->CurrentValuePortion() * oldTransition->ReversePortion() +
        (1.0 - oldTransition->ReversePortion());
    // A timing function with negative y1 (or y2!) might make
    // valuePortion negative.  In this case, we still want to apply our
    // reversing logic based on relative distances, not make duration
    // negative.
    if (valuePortion < 0.0) {
      valuePortion = -valuePortion;
    }
    // A timing function with y2 (or y1!) greater than one might
    // advance past its terminal value.  It's probably a good idea to
    // clamp valuePortion to be at most one to preserve the invariant
    // that a transition will complete within at most its specified
    // time.
    if (valuePortion > 1.0) {
      valuePortion = 1.0;
    }

    // Negative delays are essentially part of the transition
    // function, so reduce them along with the duration, but don't
    // reduce positive delays.
    if (delay < 0.0f && std::isfinite(delay)) {
      delay *= valuePortion;
    }

    if (std::isfinite(duration)) {
      duration *= valuePortion;
    }

    startForReversingTest = oldTransition->ToValue();
    reversePortion = valuePortion;
  }

  TimingParams timing = TimingParamsFromCSSParams(
      duration, delay, 1.0 /* iteration count */,
      dom::PlaybackDirection::Normal, dom::FillMode::Backwards);

  const StyleComputedTimingFunction& tf =
      aStyle.GetTransitionTimingFunction(transitionIdx);
  if (!tf.IsLinearKeyword()) {
    timing.SetTimingFunction(Some(tf));
  }

  RefPtr<CSSTransition> transition = DoCreateTransition(
      aProperty, aElement, aPseudoType, aNewStyle, aElementTransitions,
      std::move(timing), std::move(startValue), std::move(endValue),
      std::move(startForReversingTest), reversePortion);
  if (!transition) {
    return false;
  }

  OwningCSSTransitionPtrArray& transitions = aElementTransitions->mAnimations;
#ifdef DEBUG
  for (size_t i = 0, i_end = transitions.Length(); i < i_end; ++i) {
    MOZ_ASSERT(
        i == currentIndex || transitions[i]->TransitionProperty() != aProperty,
        "duplicate transitions for property");
  }
#endif
  if (oldTransition) {
    // If this new transition is replacing an existing transition that is
    // running on the compositor, we store select parameters from the replaced
    // transition so that later, once all scripts have run, we can update the
    // start value of the transition using TimeStamp::Now(). This allows us to
    // avoid a large jump when starting a new transition when the main thread
    // lags behind the compositor.
    const dom::DocumentTimeline* timeline = aElement->OwnerDoc()->Timeline();
    auto replacedTransitionProperties =
        GetReplacedTransitionProperties(oldTransition, timeline);
    if (replacedTransitionProperties) {
      transition->SetReplacedTransition(
          std::move(replacedTransitionProperties.ref()));
    }

    transitions[currentIndex]->CancelFromStyle(PostRestyleMode::IfNeeded);
    oldTransition = nullptr;  // Clear pointer so it doesn't dangle
    transitions[currentIndex] = transition;
  } else {
    // XXX(Bug 1631371) Check if this should use a fallible operation as it
    // pretended earlier.
    transitions.AppendElement(transition);
  }

  if (auto* effectSet = EffectSet::Get(aElement, aPseudoType)) {
    effectSet->UpdateAnimationGeneration(mPresContext);
  }

  return true;
}

already_AddRefed<CSSTransition> nsTransitionManager::DoCreateTransition(
    nsCSSPropertyID aProperty, dom::Element* aElement,
    PseudoStyleType aPseudoType, const mozilla::ComputedStyle& aNewStyle,
    CSSTransitionCollection*& aElementTransitions, TimingParams&& aTiming,
    AnimationValue&& aStartValue, AnimationValue&& aEndValue,
    AnimationValue&& aStartForReversingTest, double aReversePortion) {
  dom::DocumentTimeline* timeline = aElement->OwnerDoc()->Timeline();
  KeyframeEffectParams effectOptions;
  RefPtr<KeyframeEffect> keyframeEffect = new KeyframeEffect(
      aElement->OwnerDoc(), OwningAnimationTarget(aElement, aPseudoType),
      std::move(aTiming), effectOptions);

  keyframeEffect->SetKeyframes(
      GetTransitionKeyframes(aProperty, std::move(aStartValue),
                             std::move(aEndValue)),
      &aNewStyle, timeline);

  if (NS_WARN_IF(MOZ_UNLIKELY(!keyframeEffect->IsValidTransition()))) {
    return nullptr;
  }

  RefPtr<CSSTransition> animation =
      new CSSTransition(mPresContext->Document()->GetScopeObject());
  animation->SetOwningElement(OwningElementRef(*aElement, aPseudoType));
  animation->SetTimelineNoUpdate(timeline);
  animation->SetCreationSequence(
      mPresContext->RestyleManager()->GetAnimationGeneration());
  animation->SetEffectFromStyle(keyframeEffect);
  animation->SetReverseParameters(std::move(aStartForReversingTest),
                                  aReversePortion);
  animation->PlayFromStyle();

  if (!aElementTransitions) {
    aElementTransitions =
        &aElement->EnsureAnimationData().EnsureTransitionCollection(
            *aElement, aPseudoType);
    if (!aElementTransitions->isInList()) {
      AddElementCollection(aElementTransitions);
    }
  }
  return animation.forget();
}

void nsTransitionManager::DoCancelTransition(
    dom::Element* aElement, PseudoStyleType aPseudoType,
    CSSTransitionCollection*& aElementTransitions, size_t aIndex) {
  MOZ_ASSERT(aElementTransitions);
  OwningCSSTransitionPtrArray& transitions = aElementTransitions->mAnimations;
  CSSTransition* transition = transitions[aIndex];

  if (transition->HasCurrentEffect()) {
    if (auto* effectSet = EffectSet::Get(aElement, aPseudoType)) {
      effectSet->UpdateAnimationGeneration(mPresContext);
    }
  }
  transition->CancelFromStyle(PostRestyleMode::IfNeeded);
  transitions.RemoveElementAt(aIndex);

  if (transitions.IsEmpty()) {
    aElementTransitions->Destroy();
    // |aElementTransitions| is now a dangling pointer!
    aElementTransitions = nullptr;
  }
}
