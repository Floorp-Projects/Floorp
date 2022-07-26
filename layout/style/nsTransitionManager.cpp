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
#include "mozilla/EventDispatcher.h"
#include "mozilla/ServoBindings.h"
#include "mozilla/StyleAnimationValue.h"
#include "mozilla/dom/DocumentTimeline.h"
#include "mozilla/dom/Element.h"
#include "nsIFrame.h"
#include "Layers.h"
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

static inline bool ExtractNonDiscreteComputedValue(
    nsCSSPropertyID aProperty, const ComputedStyle& aComputedStyle,
    AnimationValue& aAnimationValue) {
  if (Servo_Property_IsDiscreteAnimatable(aProperty) &&
      aProperty != eCSSProperty_visibility) {
    return false;
  }

  aAnimationValue.mServo =
      Servo_ComputedValues_ExtractAnimationValue(&aComputedStyle, aProperty)
          .Consume();
  return !!aAnimationValue.mServo;
}

bool nsTransitionManager::UpdateTransitions(dom::Element* aElement,
                                            PseudoStyleType aPseudoType,
                                            const ComputedStyle& aOldStyle,
                                            const ComputedStyle& aNewStyle) {
  if (!mPresContext->IsDynamic()) {
    // For print or print preview, ignore transitions.
    return false;
  }

  if (aNewStyle.StyleDisplay()->mDisplay == StyleDisplay::None) {
    StopAnimationsForElement(aElement, aPseudoType);
    return false;
  }

  CSSTransitionCollection* collection =
      CSSTransitionCollection::GetAnimationCollection(aElement, aPseudoType);
  return DoUpdateTransitions(*aNewStyle.StyleUIReset(), aElement, aPseudoType,
                             collection, aOldStyle, aNewStyle);
}

bool nsTransitionManager::DoUpdateTransitions(
    const nsStyleUIReset& aStyle, dom::Element* aElement,
    PseudoStyleType aPseudoType, CSSTransitionCollection*& aElementTransitions,
    const ComputedStyle& aOldStyle, const ComputedStyle& aNewStyle) {
  MOZ_ASSERT(!aElementTransitions || aElementTransitions->mElement == aElement,
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
    if (i == 0 && aStyle.GetTransitionCombinedDuration(i) <= 0.0f) {
      continue;
    }

    nsCSSPropertyID property = aStyle.GetTransitionProperty(i);
    if (property == eCSSPropertyExtra_no_properties ||
        property == eCSSPropertyExtra_variable ||
        property == eCSSProperty_UNKNOWN) {
      // Nothing to do.
      continue;
    }
    // We might have something to transition.  See if any of the
    // properties in question changed and are animatable.
    // FIXME: Would be good to find a way to share code between this
    // interpretation of transition-property and the one below.
    // FIXME(emilio): This should probably just use the "all" shorthand id, and
    // we should probably remove eCSSPropertyExtra_all_properties.
    if (property == eCSSPropertyExtra_all_properties) {
      for (nsCSSPropertyID p = nsCSSPropertyID(0);
           p < eCSSProperty_COUNT_no_shorthands; p = nsCSSPropertyID(p + 1)) {
        if (!nsCSSProps::IsEnabled(p, CSSEnabledState::ForAllContent)) {
          continue;
        }
        startedAny |= ConsiderInitiatingTransition(
            p, aStyle, i, aElement, aPseudoType, aElementTransitions, aOldStyle,
            aNewStyle, propertiesChecked);
      }
    } else if (nsCSSProps::IsShorthand(property)) {
      CSSPROPS_FOR_SHORTHAND_SUBPROPERTIES(subprop, property,
                                           CSSEnabledState::ForAllContent) {
        startedAny |= ConsiderInitiatingTransition(
            *subprop, aStyle, i, aElement, aPseudoType, aElementTransitions,
            aOldStyle, aNewStyle, propertiesChecked);
      }
    } else {
      startedAny |= ConsiderInitiatingTransition(
          property, aStyle, i, aElement, aPseudoType, aElementTransitions,
          aOldStyle, aNewStyle, propertiesChecked);
    }
  }

  // Stop any transitions for properties that are no longer in
  // 'transition-property', including finished transitions.
  // Also stop any transitions (and remove any finished transitions)
  // for properties that just changed (and are still in the set of
  // properties to transition), but for which we didn't just start the
  // transition.  This can happen delay and duration are both zero, or
  // because the new value is not interpolable.
  // Note that we also do the latter set of work in
  // nsTransitionManager::PruneCompletedTransitions.
  if (aElementTransitions) {
    bool checkProperties =
        aStyle.GetTransitionProperty(0) != eCSSPropertyExtra_all_properties;
    nsCSSPropertyIDSet allTransitionProperties;
    if (checkProperties) {
      for (uint32_t i = aStyle.mTransitionPropertyCount; i-- != 0;) {
        // FIXME: Would be good to find a way to share code between this
        // interpretation of transition-property and the one above.
        nsCSSPropertyID property = aStyle.GetTransitionProperty(i);
        if (property == eCSSPropertyExtra_no_properties ||
            property == eCSSPropertyExtra_variable ||
            property == eCSSProperty_UNKNOWN) {
          // Nothing to do, but need to exclude this from cases below.
        } else if (property == eCSSPropertyExtra_all_properties) {
          for (nsCSSPropertyID p = nsCSSPropertyID(0);
               p < eCSSProperty_COUNT_no_shorthands;
               p = nsCSSPropertyID(p + 1)) {
            allTransitionProperties.AddProperty(
                nsCSSProps::Physicalize(p, aNewStyle));
          }
        } else if (nsCSSProps::IsShorthand(property)) {
          CSSPROPS_FOR_SHORTHAND_SUBPROPERTIES(subprop, property,
                                               CSSEnabledState::ForAllContent) {
            auto p = nsCSSProps::Physicalize(*subprop, aNewStyle);
            allTransitionProperties.AddProperty(p);
          }
        } else {
          allTransitionProperties.AddProperty(
              nsCSSProps::Physicalize(property, aNewStyle));
        }
      }
    }

    OwningCSSTransitionPtrArray& animations = aElementTransitions->mAnimations;
    size_t i = animations.Length();
    MOZ_ASSERT(i != 0, "empty transitions list?");
    AnimationValue currentValue;
    do {
      --i;
      CSSTransition* anim = animations[i];
      // properties no longer in 'transition-property'
      if ((checkProperties &&
           !allTransitionProperties.HasProperty(anim->TransitionProperty())) ||
          // properties whose computed values changed but for which we
          // did not start a new transition (because delay and
          // duration are both zero, or because the new value is not
          // interpolable); a new transition would have anim->ToValue()
          // matching currentValue
          !ExtractNonDiscreteComputedValue(anim->TransitionProperty(),
                                           aNewStyle, currentValue) ||
          currentValue != anim->ToValue()) {
        // stop the transition
        if (anim->HasCurrentEffect()) {
          EffectSet* effectSet = EffectSet::GetEffectSet(aElement, aPseudoType);
          if (effectSet) {
            effectSet->UpdateAnimationGeneration(mPresContext);
          }
        }
        anim->CancelFromStyle(PostRestyleMode::IfNeeded);
        animations.RemoveElementAt(i);
      }
    } while (i != 0);

    if (animations.IsEmpty()) {
      aElementTransitions->Destroy();
      aElementTransitions = nullptr;
    }
  }

  return startedAny;
}

static Keyframe& AppendKeyframe(double aOffset, nsCSSPropertyID aProperty,
                                AnimationValue&& aValue,
                                nsTArray<Keyframe>& aKeyframes) {
  Keyframe& frame = *aKeyframes.AppendElement();
  frame.mOffset.emplace(aOffset);
  MOZ_ASSERT(aValue.mServo);
  RefPtr<RawServoDeclarationBlock> decl =
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
      !aElementTransitions || aElementTransitions->mElement == aElement,
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

  float delay = aStyle.GetTransitionDelay(transitionIdx);

  // The spec says a negative duration is treated as zero.
  float duration = std::max(aStyle.GetTransitionDuration(transitionIdx), 0.0f);

  // If the combined duration of this transition is 0 or less don't start a
  // transition.
  if (delay + duration <= 0.0f) {
    return false;
  }

  dom::DocumentTimeline* timeline = aElement->OwnerDoc()->Timeline();

  AnimationValue startValue, endValue;
  bool haveValues =
      ExtractNonDiscreteComputedValue(aProperty, aOldStyle, startValue) &&
      ExtractNonDiscreteComputedValue(aProperty, aNewStyle, endValue);

  bool haveChange = startValue != endValue;

  bool shouldAnimate = haveValues && haveChange &&
                       startValue.IsInterpolableWith(aProperty, endValue);

  bool haveCurrentTransition = false;
  size_t currentIndex = nsTArray<KeyframeEffect>::NoIndex;
  const CSSTransition* oldTransition = nullptr;
  if (aElementTransitions) {
    OwningCSSTransitionPtrArray& animations = aElementTransitions->mAnimations;
    for (size_t i = 0, i_end = animations.Length(); i < i_end; ++i) {
      if (animations[i]->TransitionProperty() == aProperty) {
        haveCurrentTransition = true;
        currentIndex = i;
        oldTransition = animations[i];
        break;
      }
    }
  }

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
  if (haveCurrentTransition && haveValues &&
      aElementTransitions->mAnimations[currentIndex]->ToValue() == endValue) {
    // GetAnimationRule already called RestyleForAnimation.
    return false;
  }

  if (!shouldAnimate) {
    if (haveCurrentTransition) {
      // We're in the middle of a transition, and just got a non-transition
      // style change to something that we can't animate.  This might happen
      // because we got a non-transition style change changing to the current
      // in-progress value (which is particularly easy to cause when we're
      // currently in the 'transition-delay').  It also might happen because we
      // just got a style change to a value that can't be interpolated.
      OwningCSSTransitionPtrArray& animations =
          aElementTransitions->mAnimations;
      animations[currentIndex]->CancelFromStyle(PostRestyleMode::IfNeeded);
      oldTransition = nullptr;  // Clear pointer so it doesn't dangle
      animations.RemoveElementAt(currentIndex);
      EffectSet* effectSet = EffectSet::GetEffectSet(aElement, aPseudoType);
      if (effectSet) {
        effectSet->UpdateAnimationGeneration(mPresContext);
      }

      if (animations.IsEmpty()) {
        aElementTransitions->Destroy();
        // |aElementTransitions| is now a dangling pointer!
        aElementTransitions = nullptr;
      }
      // GetAnimationRule already called RestyleForAnimation.
    }
    return false;
  }

  AnimationValue startForReversingTest = startValue;
  double reversePortion = 1.0;

  // If the new transition reverses an existing one, we'll need to
  // handle the timing differently.
  if (haveCurrentTransition &&
      aElementTransitions->mAnimations[currentIndex]->HasCurrentEffect() &&
      oldTransition && oldTransition->StartForReversingTest() == endValue) {
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
    if (delay < 0.0f) {
      delay *= valuePortion;
    }

    duration *= valuePortion;

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

  KeyframeEffectParams effectOptions;
  RefPtr<KeyframeEffect> keyframeEffect = new KeyframeEffect(
      aElement->OwnerDoc(), OwningAnimationTarget(aElement, aPseudoType),
      std::move(timing), effectOptions);

  keyframeEffect->SetKeyframes(
      GetTransitionKeyframes(aProperty, std::move(startValue),
                             std::move(endValue)),
      &aNewStyle);

  if (NS_WARN_IF(MOZ_UNLIKELY(!keyframeEffect->IsValidTransition()))) {
    return false;
  }

  RefPtr<CSSTransition> animation =
      new CSSTransition(mPresContext->Document()->GetScopeObject());
  animation->SetOwningElement(OwningElementRef(*aElement, aPseudoType));
  animation->SetTimelineNoUpdate(timeline);
  animation->SetCreationSequence(
      mPresContext->RestyleManager()->GetAnimationGeneration());
  animation->SetEffectFromStyle(keyframeEffect);
  animation->SetReverseParameters(std::move(startForReversingTest),
                                  reversePortion);
  animation->PlayFromStyle();

  if (!aElementTransitions) {
    bool createdCollection = false;
    aElementTransitions =
        CSSTransitionCollection::GetOrCreateAnimationCollection(
            aElement, aPseudoType, &createdCollection);
    if (!aElementTransitions) {
      MOZ_ASSERT(!createdCollection, "outparam should agree with return value");
      NS_WARNING("allocating collection failed");
      return false;
    }

    if (createdCollection) {
      AddElementCollection(aElementTransitions);
    }
  }

  OwningCSSTransitionPtrArray& animations = aElementTransitions->mAnimations;
#ifdef DEBUG
  for (size_t i = 0, i_end = animations.Length(); i < i_end; ++i) {
    MOZ_ASSERT(
        i == currentIndex || animations[i]->TransitionProperty() != aProperty,
        "duplicate transitions for property");
  }
#endif
  if (haveCurrentTransition) {
    // If this new transition is replacing an existing transition that is
    // running on the compositor, we store select parameters from the replaced
    // transition so that later, once all scripts have run, we can update the
    // start value of the transition using TimeStamp::Now(). This allows us to
    // avoid a large jump when starting a new transition when the main thread
    // lags behind the compositor.
    auto replacedTransitionProperties =
        GetReplacedTransitionProperties(oldTransition, timeline);
    if (replacedTransitionProperties) {
      animation->SetReplacedTransition(
          std::move(replacedTransitionProperties.ref()));
    }

    animations[currentIndex]->CancelFromStyle(PostRestyleMode::IfNeeded);
    oldTransition = nullptr;  // Clear pointer so it doesn't dangle
    animations[currentIndex] = animation;
  } else {
    // XXX(Bug 1631371) Check if this should use a fallible operation as it
    // pretended earlier.
    animations.AppendElement(animation);
  }

  EffectSet* effectSet = EffectSet::GetEffectSet(aElement, aPseudoType);
  if (effectSet) {
    effectSet->UpdateAnimationGeneration(mPresContext);
  }

  return true;
}
