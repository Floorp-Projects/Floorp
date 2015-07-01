/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAnimationManager.h"
#include "nsTransitionManager.h"
#include "mozilla/dom/CSSAnimationBinding.h"

#include "mozilla/EventDispatcher.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/StyleAnimationValue.h"
#include "mozilla/dom/KeyframeEffect.h"

#include "nsPresContext.h"
#include "nsStyleSet.h"
#include "nsStyleChangeList.h"
#include "nsCSSRules.h"
#include "RestyleManager.h"
#include "nsLayoutUtils.h"
#include "nsIFrame.h"
#include "nsIDocument.h"
#include "nsDOMMutationObserver.h"
#include <math.h>

using namespace mozilla;
using namespace mozilla::css;
using mozilla::dom::Animation;
using mozilla::dom::AnimationPlayState;
using mozilla::dom::KeyframeEffectReadOnly;
using mozilla::dom::CSSAnimation;

JSObject*
CSSAnimation::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return dom::CSSAnimationBinding::Wrap(aCx, this, aGivenProto);
}

mozilla::dom::Promise*
CSSAnimation::GetReady(ErrorResult& aRv)
{
  FlushStyle();
  return Animation::GetReady(aRv);
}

void
CSSAnimation::Play(ErrorResult &aRv, LimitBehavior aLimitBehavior)
{
  mPauseShouldStick = false;
  Animation::Play(aRv, aLimitBehavior);
}

void
CSSAnimation::Pause(ErrorResult& aRv)
{
  mPauseShouldStick = true;
  Animation::Pause(aRv);
}

AnimationPlayState
CSSAnimation::PlayStateFromJS() const
{
  // Flush style to ensure that any properties controlling animation state
  // (e.g. animation-play-state) are fully updated.
  FlushStyle();
  return Animation::PlayStateFromJS();
}

void
CSSAnimation::PlayFromJS(ErrorResult& aRv)
{
  // Note that flushing style below might trigger calls to
  // PlayFromStyle()/PauseFromStyle() on this object.
  FlushStyle();
  Animation::PlayFromJS(aRv);
}

void
CSSAnimation::PlayFromStyle()
{
  mIsStylePaused = false;
  if (!mPauseShouldStick) {
    ErrorResult rv;
    DoPlay(rv, Animation::LimitBehavior::Continue);
    // play() should not throw when LimitBehavior is Continue
    MOZ_ASSERT(!rv.Failed(), "Unexpected exception playing animation");
  }
}

void
CSSAnimation::PauseFromStyle()
{
  // Check if the pause state is being overridden
  if (mIsStylePaused) {
    return;
  }

  mIsStylePaused = true;
  ErrorResult rv;
  DoPause(rv);
  // pause() should only throw when *all* of the following conditions are true:
  // - we are in the idle state, and
  // - we have a negative playback rate, and
  // - we have an infinitely repeating animation
  // The first two conditions will never happen under regular style processing
  // but could happen if an author made modifications to the Animation object
  // and then updated animation-play-state. It's an unusual case and there's
  // no obvious way to pass on the exception information so we just silently
  // fail for now.
  if (rv.Failed()) {
    NS_WARNING("Unexpected exception pausing animation - silently failing");
  }
}

void
CSSAnimation::QueueEvents(EventArray& aEventsToDispatch)
{
  if (!mEffect) {
    return;
  }

  ComputedTiming computedTiming = mEffect->GetComputedTiming();

  if (computedTiming.mPhase == ComputedTiming::AnimationPhase_Null) {
    return; // do nothing
  }

  // Note that script can change the start time, so we have to handle moving
  // backwards through the animation as well as forwards. An 'animationstart'
  // is dispatched if we enter the active phase (regardless if that is from
  // before or after the animation's active phase). An 'animationend' is
  // dispatched if we leave the active phase (regardless if that is to before
  // or after the animation's active phase).

  bool wasActive = mPreviousPhaseOrIteration != PREVIOUS_PHASE_BEFORE &&
                   mPreviousPhaseOrIteration != PREVIOUS_PHASE_AFTER;
  bool isActive =
         computedTiming.mPhase == ComputedTiming::AnimationPhase_Active;
  bool isSameIteration =
         computedTiming.mCurrentIteration == mPreviousPhaseOrIteration;
  bool skippedActivePhase =
    (mPreviousPhaseOrIteration == PREVIOUS_PHASE_BEFORE &&
     computedTiming.mPhase == ComputedTiming::AnimationPhase_After) ||
    (mPreviousPhaseOrIteration == PREVIOUS_PHASE_AFTER &&
     computedTiming.mPhase == ComputedTiming::AnimationPhase_Before);

  MOZ_ASSERT(!skippedActivePhase || (!isActive && !wasActive),
             "skippedActivePhase only makes sense if we were & are inactive");

  if (computedTiming.mPhase == ComputedTiming::AnimationPhase_Before) {
    mPreviousPhaseOrIteration = PREVIOUS_PHASE_BEFORE;
  } else if (computedTiming.mPhase == ComputedTiming::AnimationPhase_Active) {
    mPreviousPhaseOrIteration = computedTiming.mCurrentIteration;
  } else if (computedTiming.mPhase == ComputedTiming::AnimationPhase_After) {
    mPreviousPhaseOrIteration = PREVIOUS_PHASE_AFTER;
  }

  dom::Element* target;
  nsCSSPseudoElements::Type targetPseudoType;
  mEffect->GetTarget(target, targetPseudoType);

  uint32_t message;

  if (!wasActive && isActive) {
    message = NS_ANIMATION_START;
  } else if (wasActive && !isActive) {
    message = NS_ANIMATION_END;
  } else if (wasActive && isActive && !isSameIteration) {
    message = NS_ANIMATION_ITERATION;
  } else if (skippedActivePhase) {
    // First notifying for start of 0th iteration by appending an
    // 'animationstart':
    StickyTimeDuration elapsedTime =
      std::min(StickyTimeDuration(mEffect->InitialAdvance()),
               computedTiming.mActiveDuration);
    AnimationEventInfo ei(target, mAnimationName, NS_ANIMATION_START,
                          elapsedTime,
                          PseudoTypeAsString(targetPseudoType));
    aEventsToDispatch.AppendElement(ei);
    // Then have the shared code below append an 'animationend':
    message = NS_ANIMATION_END;
  } else {
    return; // No events need to be sent
  }

  StickyTimeDuration elapsedTime;

  if (message == NS_ANIMATION_START ||
      message == NS_ANIMATION_ITERATION) {
    TimeDuration iterationStart = mEffect->Timing().mIterationDuration *
                                    computedTiming.mCurrentIteration;
    elapsedTime = StickyTimeDuration(std::max(iterationStart,
                                              mEffect->InitialAdvance()));
  } else {
    MOZ_ASSERT(message == NS_ANIMATION_END);
    elapsedTime = computedTiming.mActiveDuration;
  }

  AnimationEventInfo ei(target, mAnimationName, message, elapsedTime,
                        PseudoTypeAsString(targetPseudoType));
  aEventsToDispatch.AppendElement(ei);
}

CommonAnimationManager*
CSSAnimation::GetAnimationManager() const
{
  nsPresContext* context = GetPresContext();
  if (!context) {
    return nullptr;
  }

  return context->AnimationManager();
}

/* static */ nsString
CSSAnimation::PseudoTypeAsString(nsCSSPseudoElements::Type aPseudoType)
{
  switch (aPseudoType) {
    case nsCSSPseudoElements::ePseudo_before:
      return NS_LITERAL_STRING("::before");
    case nsCSSPseudoElements::ePseudo_after:
      return NS_LITERAL_STRING("::after");
    default:
      return EmptyString();
  }
}

void
nsAnimationManager::UpdateStyleAndEvents(AnimationCollection* aCollection,
                                         TimeStamp aRefreshTime,
                                         EnsureStyleRuleFlags aFlags)
{
  aCollection->EnsureStyleRuleFor(aRefreshTime, aFlags);
  QueueEvents(aCollection, mPendingEvents);
}

void
nsAnimationManager::QueueEvents(AnimationCollection* aCollection,
                                EventArray& aEventsToDispatch)
{
  for (size_t animIdx = aCollection->mAnimations.Length(); animIdx-- != 0; ) {
    CSSAnimation* anim = aCollection->mAnimations[animIdx]->AsCSSAnimation();
    MOZ_ASSERT(anim, "Expected a collection of CSS Animations");
    anim->QueueEvents(aEventsToDispatch);
  }
}

void
nsAnimationManager::MaybeUpdateCascadeResults(AnimationCollection* aCollection)
{
  for (size_t animIdx = aCollection->mAnimations.Length(); animIdx-- != 0; ) {
    CSSAnimation* anim = aCollection->mAnimations[animIdx]->AsCSSAnimation();
    if (anim->IsInEffect() != anim->mInEffectForCascadeResults) {
      // Update our own cascade results.
      mozilla::dom::Element* element = aCollection->GetElementToRestyle();
      bool updatedCascadeResults = false;
      if (element) {
        nsIFrame* frame = element->GetPrimaryFrame();
        if (frame) {
          UpdateCascadeResults(frame->StyleContext(), aCollection);
          updatedCascadeResults = true;
        }
      }

      if (!updatedCascadeResults) {
        // If we don't have a style context we can't do the work of updating
        // cascading results but we need to make sure to update
        // mInEffectForCascadeResults or else we'll keep running this
        // code every time (potentially leading to infinite recursion due
        // to the fact that this method both calls and is (indirectly) called
        // by nsTransitionManager).
        anim->mInEffectForCascadeResults = anim->IsInEffect();
      }

      // Notify the transition manager, whose results might depend on ours.
      mPresContext->TransitionManager()->
        UpdateCascadeResultsWithAnimations(aCollection);

      return;
    }
  }
}

/* virtual */ size_t
nsAnimationManager::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const
{
  return CommonAnimationManager::SizeOfExcludingThis(aMallocSizeOf);

  // Measurement of the following members may be added later if DMD finds it is
  // worthwhile:
  // - mPendingEvents
}

/* virtual */ size_t
nsAnimationManager::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
}

nsIStyleRule*
nsAnimationManager::CheckAnimationRule(nsStyleContext* aStyleContext,
                                       mozilla::dom::Element* aElement)
{
  if (!mPresContext->IsDynamic()) {
    // For print or print preview, ignore animations.
    return nullptr;
  }

  // Everything that causes our animation data to change triggers a
  // style change, which in turn triggers a non-animation restyle.
  // Likewise, when we initially construct frames, we're not in a
  // style change, but also not in an animation restyle.

  const nsStyleDisplay* disp = aStyleContext->StyleDisplay();
  AnimationCollection* collection =
    GetAnimations(aElement, aStyleContext->GetPseudoType(), false);
  if (!collection &&
      disp->mAnimationNameCount == 1 &&
      disp->mAnimations[0].GetName().IsEmpty()) {
    return nullptr;
  }

  nsAutoAnimationMutationBatch mb(aElement);

  // build the animations list
  dom::DocumentTimeline* timeline = aElement->OwnerDoc()->Timeline();
  AnimationPtrArray newAnimations;
  if (!aStyleContext->IsInDisplayNoneSubtree()) {
    BuildAnimations(aStyleContext, aElement, timeline, newAnimations);
  }

  if (newAnimations.IsEmpty()) {
    if (collection) {
      // There might be transitions that run now that animations don't
      // override them.
      mPresContext->TransitionManager()->
        UpdateCascadeResultsWithAnimationsToBeDestroyed(collection);

      collection->Destroy();
    }
    return nullptr;
  }

  if (collection) {
    collection->mStyleRule = nullptr;
    collection->mStyleRuleRefreshTime = TimeStamp();
    collection->UpdateAnimationGeneration(mPresContext);

    // Copy over the start times and (if still paused) pause starts
    // for each animation (matching on name only) that was also in the
    // old list of animations.
    // This means that we honor dynamic changes, which isn't what the
    // spec says to do, but WebKit seems to honor at least some of
    // them.  See
    // http://lists.w3.org/Archives/Public/www-style/2011Apr/0079.html
    // In order to honor what the spec said, we'd copy more data over
    // (or potentially optimize BuildAnimations to avoid rebuilding it
    // in the first place).
    if (!collection->mAnimations.IsEmpty()) {

      for (size_t newIdx = newAnimations.Length(); newIdx-- != 0;) {
        Animation* newAnim = newAnimations[newIdx];

        // Find the matching animation with this name in the old list
        // of animations.  We iterate through both lists in a backwards
        // direction which means that if there are more animations in
        // the new list of animations with a given name than in the old
        // list, it will be the animations towards the of the beginning of
        // the list that do not match and are treated as new animations.
        nsRefPtr<CSSAnimation> oldAnim;
        size_t oldIdx = collection->mAnimations.Length();
        while (oldIdx-- != 0) {
          CSSAnimation* a = collection->mAnimations[oldIdx]->AsCSSAnimation();
          MOZ_ASSERT(a, "All animations in the CSS Animation collection should"
                        " be CSSAnimation objects");
          if (a->AnimationName() ==
              newAnim->AsCSSAnimation()->AnimationName()) {
            oldAnim = a;
            break;
          }
        }
        if (!oldAnim) {
          continue;
        }

        bool animationChanged = false;

        // Update the old from the new so we can keep the original object
        // identity (and any expando properties attached to it).
        if (oldAnim->GetEffect() && newAnim->GetEffect()) {
          KeyframeEffectReadOnly* oldEffect = oldAnim->GetEffect();
          KeyframeEffectReadOnly* newEffect = newAnim->GetEffect();
          animationChanged =
            oldEffect->Timing() != newEffect->Timing() ||
            oldEffect->Properties() != newEffect->Properties();
          oldEffect->Timing() = newEffect->Timing();
          oldEffect->Properties() = newEffect->Properties();
        }

        // Reset compositor state so animation will be re-synchronized.
        oldAnim->ClearIsRunningOnCompositor();

        // Handle changes in play state. If the animation is idle, however,
        // changes to animation-play-state should *not* restart it.
        if (oldAnim->PlayState() != AnimationPlayState::Idle) {
          // CSSAnimation takes care of override behavior so that,
          // for example, if the author has called pause(), that will
          // override the animation-play-state.
          // (We should check newAnim->IsStylePaused() but that requires
          //  downcasting to CSSAnimation and we happen to know that
          //  newAnim will only ever be paused by calling PauseFromStyle
          //  making IsPausedOrPausing synonymous in this case.)
          if (!oldAnim->IsStylePaused() && newAnim->IsPausedOrPausing()) {
            oldAnim->PauseFromStyle();
            animationChanged = true;
          } else if (oldAnim->IsStylePaused() &&
                    !newAnim->IsPausedOrPausing()) {
            oldAnim->PlayFromStyle();
            animationChanged = true;
          }
        }

        if (animationChanged) {
          nsNodeUtils::AnimationChanged(oldAnim);
        }

        // Replace new animation with the (updated) old one and remove the
        // old one from the array so we don't try to match it any more.
        //
        // Although we're doing this while iterating this is safe because
        // we're not changing the length of newAnimations and we've finished
        // iterating over the list of old iterations.
        newAnim->CancelFromStyle();
        newAnim = nullptr;
        newAnimations.ReplaceElementAt(newIdx, oldAnim);
        collection->mAnimations.RemoveElementAt(oldIdx);

        // We've touched the old animation's timing properties, so this
        // could update the old animation's relevance.
        oldAnim->UpdateRelevance();
      }
    }
  } else {
    collection =
      GetAnimations(aElement, aStyleContext->GetPseudoType(), true);
  }
  collection->mAnimations.SwapElements(newAnimations);
  collection->mNeedsRefreshes = true;
  collection->Tick();

  // Cancel removed animations
  for (size_t newAnimIdx = newAnimations.Length(); newAnimIdx-- != 0; ) {
    newAnimations[newAnimIdx]->CancelFromStyle();
  }

  UpdateCascadeResults(aStyleContext, collection);

  TimeStamp refreshTime = mPresContext->RefreshDriver()->MostRecentRefresh();
  UpdateStyleAndEvents(collection, refreshTime,
                       EnsureStyleRule_IsNotThrottled);
  // We don't actually dispatch the mPendingEvents now.  We'll either
  // dispatch them the next time we get a refresh driver notification
  // or the next time somebody calls
  // nsPresShell::FlushPendingNotifications.
  if (!mPendingEvents.IsEmpty()) {
    mPresContext->Document()->SetNeedStyleFlush();
  }

  return GetAnimationRule(aElement, aStyleContext->GetPseudoType());
}

struct KeyframeData {
  float mKey;
  uint32_t mIndex; // store original order since sort algorithm is not stable
  nsCSSKeyframeRule *mRule;
};

struct KeyframeDataComparator {
  bool Equals(const KeyframeData& A, const KeyframeData& B) const {
    return A.mKey == B.mKey && A.mIndex == B.mIndex;
  }
  bool LessThan(const KeyframeData& A, const KeyframeData& B) const {
    return A.mKey < B.mKey || (A.mKey == B.mKey && A.mIndex < B.mIndex);
  }
};

class ResolvedStyleCache {
public:
  ResolvedStyleCache() : mCache() {}
  nsStyleContext* Get(nsPresContext *aPresContext,
                      nsStyleContext *aParentStyleContext,
                      nsCSSKeyframeRule *aKeyframe);

private:
  nsRefPtrHashtable<nsPtrHashKey<nsCSSKeyframeRule>, nsStyleContext> mCache;
};

nsStyleContext*
ResolvedStyleCache::Get(nsPresContext *aPresContext,
                        nsStyleContext *aParentStyleContext,
                        nsCSSKeyframeRule *aKeyframe)
{
  // FIXME (spec):  The css3-animations spec isn't very clear about how
  // properties are resolved when they have values that depend on other
  // properties (e.g., values in 'em').  I presume that they're resolved
  // relative to the other styles of the element.  The question is
  // whether they are resolved relative to other animations:  I assume
  // that they're not, since that would prevent us from caching a lot of
  // data that we'd really like to cache (in particular, the
  // StyleAnimationValue values in AnimationPropertySegment).
  nsStyleContext *result = mCache.GetWeak(aKeyframe);
  if (!result) {
    nsCOMArray<nsIStyleRule> rules;
    rules.AppendObject(aKeyframe);
    nsRefPtr<nsStyleContext> resultStrong = aPresContext->StyleSet()->
      ResolveStyleByAddingRules(aParentStyleContext, rules);
    mCache.Put(aKeyframe, resultStrong);
    result = resultStrong;
  }
  return result;
}

void
nsAnimationManager::BuildAnimations(nsStyleContext* aStyleContext,
                                    dom::Element* aTarget,
                                    dom::DocumentTimeline* aTimeline,
                                    AnimationPtrArray& aAnimations)
{
  MOZ_ASSERT(aAnimations.IsEmpty(), "expect empty array");

  ResolvedStyleCache resolvedStyles;

  const nsStyleDisplay *disp = aStyleContext->StyleDisplay();

  nsRefPtr<nsStyleContext> styleWithoutAnimation;

  for (size_t animIdx = 0, animEnd = disp->mAnimationNameCount;
       animIdx != animEnd; ++animIdx) {
    const StyleAnimation& src = disp->mAnimations[animIdx];

    // CSS Animations whose animation-name does not match a @keyframes rule do
    // not generate animation events. This includes when the animation-name is
    // "none" which is represented by an empty name in the StyleAnimation.
    // Since such animations neither affect style nor dispatch events, we do
    // not generate a corresponding Animation for them.
    nsCSSKeyframesRule* rule =
      src.GetName().IsEmpty()
      ? nullptr
      : mPresContext->StyleSet()->KeyframesRuleForName(src.GetName());
    if (!rule) {
      continue;
    }

    nsRefPtr<CSSAnimation> dest = new CSSAnimation(aTimeline, src.GetName());
    aAnimations.AppendElement(dest);

    AnimationTiming timing;
    timing.mIterationDuration =
      TimeDuration::FromMilliseconds(src.GetDuration());
    timing.mDelay = TimeDuration::FromMilliseconds(src.GetDelay());
    timing.mIterationCount = src.GetIterationCount();
    timing.mDirection = src.GetDirection();
    timing.mFillMode = src.GetFillMode();

    nsRefPtr<KeyframeEffectReadOnly> destEffect =
      new KeyframeEffectReadOnly(mPresContext->Document(), aTarget,
                                 aStyleContext->GetPseudoType(), timing);
    dest->SetEffect(destEffect);

    if (src.GetPlayState() == NS_STYLE_ANIMATION_PLAY_STATE_PAUSED) {
      dest->PauseFromStyle();
    } else {
      dest->PlayFromStyle();
    }

    // While current drafts of css3-animations say that later keyframes
    // with the same key entirely replace earlier ones (no cascading),
    // this is a bad idea and contradictory to the rest of CSS.  So
    // we're going to keep all the keyframes for each key and then do
    // the replacement on a per-property basis rather than a per-rule
    // basis, just like everything else in CSS.

    AutoInfallibleTArray<KeyframeData, 16> sortedKeyframes;

    for (uint32_t ruleIdx = 0, ruleEnd = rule->StyleRuleCount();
         ruleIdx != ruleEnd; ++ruleIdx) {
      css::Rule* cssRule = rule->GetStyleRuleAt(ruleIdx);
      MOZ_ASSERT(cssRule, "must have rule");
      MOZ_ASSERT(cssRule->GetType() == css::Rule::KEYFRAME_RULE,
                 "must be keyframe rule");
      nsCSSKeyframeRule *kfRule = static_cast<nsCSSKeyframeRule*>(cssRule);

      const nsTArray<float> &keys = kfRule->GetKeys();
      for (uint32_t keyIdx = 0, keyEnd = keys.Length();
           keyIdx != keyEnd; ++keyIdx) {
        float key = keys[keyIdx];
        // FIXME (spec):  The spec doesn't say what to do with
        // out-of-range keyframes.  We'll ignore them.
        if (0.0f <= key && key <= 1.0f) {
          KeyframeData *data = sortedKeyframes.AppendElement();
          data->mKey = key;
          data->mIndex = ruleIdx;
          data->mRule = kfRule;
        }
      }
    }

    sortedKeyframes.Sort(KeyframeDataComparator());

    if (sortedKeyframes.Length() == 0) {
      // no segments
      continue;
    }

    // Record the properties that are present in any keyframe rules we
    // are using.
    nsCSSPropertySet properties;

    for (uint32_t kfIdx = 0, kfEnd = sortedKeyframes.Length();
         kfIdx != kfEnd; ++kfIdx) {
      css::Declaration *decl = sortedKeyframes[kfIdx].mRule->Declaration();
      for (uint32_t propIdx = 0, propEnd = decl->Count();
           propIdx != propEnd; ++propIdx) {
        nsCSSProperty prop = decl->GetPropertyAt(propIdx);
        if (prop != eCSSPropertyExtra_variable) {
          // CSS Variables are not animatable
          properties.AddProperty(prop);
        }
      }
    }

    for (nsCSSProperty prop = nsCSSProperty(0);
         prop < eCSSProperty_COUNT_no_shorthands;
         prop = nsCSSProperty(prop + 1)) {
      if (!properties.HasProperty(prop) ||
          nsCSSProps::kAnimTypeTable[prop] == eStyleAnimType_None) {
        continue;
      }

      // Build a list of the keyframes to use for this property.  This
      // means we need every keyframe with the property in it, except
      // for those keyframes where a later keyframe with the *same key*
      // also has the property.
      AutoInfallibleTArray<uint32_t, 16> keyframesWithProperty;
      float lastKey = 100.0f; // an invalid key
      for (uint32_t kfIdx = 0, kfEnd = sortedKeyframes.Length();
           kfIdx != kfEnd; ++kfIdx) {
        KeyframeData &kf = sortedKeyframes[kfIdx];
        if (!kf.mRule->Declaration()->HasProperty(prop)) {
          continue;
        }
        if (kf.mKey == lastKey) {
          // Replace previous occurrence of same key.
          keyframesWithProperty[keyframesWithProperty.Length() - 1] = kfIdx;
        } else {
          keyframesWithProperty.AppendElement(kfIdx);
        }
        lastKey = kf.mKey;
      }

      AnimationProperty &propData = *destEffect->Properties().AppendElement();
      propData.mProperty = prop;
      propData.mWinsInCascade = true;

      KeyframeData *fromKeyframe = nullptr;
      nsRefPtr<nsStyleContext> fromContext;
      bool interpolated = true;
      for (uint32_t wpIdx = 0, wpEnd = keyframesWithProperty.Length();
           wpIdx != wpEnd; ++wpIdx) {
        uint32_t kfIdx = keyframesWithProperty[wpIdx];
        KeyframeData &toKeyframe = sortedKeyframes[kfIdx];

        nsRefPtr<nsStyleContext> toContext =
          resolvedStyles.Get(mPresContext, aStyleContext, toKeyframe.mRule);

        if (fromKeyframe) {
          interpolated = interpolated &&
            BuildSegment(propData.mSegments, prop, src,
                         fromKeyframe->mKey, fromContext,
                         fromKeyframe->mRule->Declaration(),
                         toKeyframe.mKey, toContext);
        } else {
          if (toKeyframe.mKey != 0.0f) {
            // There's no data for this property at 0%, so use the
            // cascaded value above us.
            if (!styleWithoutAnimation) {
              styleWithoutAnimation = mPresContext->StyleSet()->
                ResolveStyleWithoutAnimation(aTarget, aStyleContext,
                                             eRestyle_AllHintsWithAnimations);
            }
            interpolated = interpolated &&
              BuildSegment(propData.mSegments, prop, src,
                           0.0f, styleWithoutAnimation, nullptr,
                           toKeyframe.mKey, toContext);
          }
        }

        fromContext = toContext;
        fromKeyframe = &toKeyframe;
      }

      if (fromKeyframe->mKey != 1.0f) {
        // There's no data for this property at 100%, so use the
        // cascaded value above us.
        if (!styleWithoutAnimation) {
          styleWithoutAnimation = mPresContext->StyleSet()->
            ResolveStyleWithoutAnimation(aTarget, aStyleContext,
                                         eRestyle_AllHintsWithAnimations);
        }
        interpolated = interpolated &&
          BuildSegment(propData.mSegments, prop, src,
                       fromKeyframe->mKey, fromContext,
                       fromKeyframe->mRule->Declaration(),
                       1.0f, styleWithoutAnimation);
      }

      // If we failed to build any segments due to inability to
      // interpolate, remove the property from the animation.  (It's not
      // clear if this is the right thing to do -- we could run some of
      // the segments, but it's really not clear whether we should skip
      // values (which?) or skip segments, so best to skip the whole
      // thing for now.)
      if (!interpolated) {
        destEffect->Properties().RemoveElementAt(
          destEffect->Properties().Length() - 1);
      }
    }
  }
}

bool
nsAnimationManager::BuildSegment(InfallibleTArray<AnimationPropertySegment>&
                                   aSegments,
                                 nsCSSProperty aProperty,
                                 const StyleAnimation& aAnimation,
                                 float aFromKey, nsStyleContext* aFromContext,
                                 mozilla::css::Declaration* aFromDeclaration,
                                 float aToKey, nsStyleContext* aToContext)
{
  StyleAnimationValue fromValue, toValue, dummyValue;
  if (!ExtractComputedValueForTransition(aProperty, aFromContext, fromValue) ||
      !ExtractComputedValueForTransition(aProperty, aToContext, toValue) ||
      // Check that we can interpolate between these values
      // (If this is ever a performance problem, we could add a
      // CanInterpolate method, but it seems fine for now.)
      !StyleAnimationValue::Interpolate(aProperty, fromValue, toValue,
                                        0.5, dummyValue)) {
    return false;
  }

  AnimationPropertySegment &segment = *aSegments.AppendElement();

  segment.mFromValue = fromValue;
  segment.mToValue = toValue;
  segment.mFromKey = aFromKey;
  segment.mToKey = aToKey;
  const nsTimingFunction *tf;
  if (aFromDeclaration &&
      aFromDeclaration->HasProperty(eCSSProperty_animation_timing_function)) {
    tf = &aFromContext->StyleDisplay()->mAnimations[0].GetTimingFunction();
  } else {
    tf = &aAnimation.GetTimingFunction();
  }
  segment.mTimingFunction.Init(*tf);

  return true;
}

/* static */ void
nsAnimationManager::UpdateCascadeResults(
                      nsStyleContext* aStyleContext,
                      AnimationCollection* aElementAnimations)
{
  /*
   * Figure out which properties we need to examine.
   */

  // size of 2 since we only currently have 2 properties we animate on
  // the compositor
  nsAutoTArray<nsCSSProperty, 2> propertiesToTrack;

  {
    nsCSSPropertySet propertiesToTrackAsSet;

    for (size_t animIdx = aElementAnimations->mAnimations.Length();
         animIdx-- != 0; ) {
      const Animation* anim = aElementAnimations->mAnimations[animIdx];
      const KeyframeEffectReadOnly* effect = anim->GetEffect();
      if (!effect) {
        continue;
      }

      for (size_t propIdx = 0, propEnd = effect->Properties().Length();
           propIdx != propEnd; ++propIdx) {
        const AnimationProperty& prop = effect->Properties()[propIdx];
        // We only bother setting mWinsInCascade for properties that we
        // can animate on the compositor.
        if (nsCSSProps::PropHasFlags(prop.mProperty,
                                     CSS_PROPERTY_CAN_ANIMATE_ON_COMPOSITOR)) {
          if (!propertiesToTrackAsSet.HasProperty(prop.mProperty)) {
            propertiesToTrack.AppendElement(prop.mProperty);
            propertiesToTrackAsSet.AddProperty(prop.mProperty);
          }
        }
      }
    }
  }

  /*
   * Determine whether those properties are set in things that
   * override animations.
   */

  nsCSSPropertySet propertiesOverridden;
  nsRuleNode::ComputePropertiesOverridingAnimation(propertiesToTrack,
                                                   aStyleContext,
                                                   propertiesOverridden);

  /*
   * Set mWinsInCascade based both on what is overridden at levels
   * higher than animations and based on one animation overriding
   * another.
   *
   * We iterate from the last animation to the first, just like we do
   * when calling ComposeStyle from AnimationCollection::EnsureStyleRuleFor.
   * Later animations override earlier ones, so we add properties to the set
   * of overridden properties as we encounter them, if the animation is
   * currently in effect.
   */

  bool changed = false;
  for (size_t animIdx = aElementAnimations->mAnimations.Length();
       animIdx-- != 0; ) {
    CSSAnimation* anim =
      aElementAnimations->mAnimations[animIdx]->AsCSSAnimation();
    KeyframeEffectReadOnly* effect = anim->GetEffect();

    anim->mInEffectForCascadeResults = anim->IsInEffect();

    if (!effect) {
      continue;
    }

    for (size_t propIdx = 0, propEnd = effect->Properties().Length();
         propIdx != propEnd; ++propIdx) {
      AnimationProperty& prop = effect->Properties()[propIdx];
      // We only bother setting mWinsInCascade for properties that we
      // can animate on the compositor.
      if (nsCSSProps::PropHasFlags(prop.mProperty,
                                   CSS_PROPERTY_CAN_ANIMATE_ON_COMPOSITOR)) {
        bool newWinsInCascade =
          !propertiesOverridden.HasProperty(prop.mProperty);
        if (newWinsInCascade != prop.mWinsInCascade) {
          changed = true;
        }
        prop.mWinsInCascade = newWinsInCascade;

        if (prop.mWinsInCascade && anim->mInEffectForCascadeResults) {
          // This animation is in effect right now, so it overrides
          // earlier animations.  (For animations that aren't in effect,
          // we set mWinsInCascade as though they were, but they don't
          // suppress animations lower in the cascade.)
          propertiesOverridden.AddProperty(prop.mProperty);
        }
      }
    }
  }

  if (changed) {
    nsPresContext* presContext = aElementAnimations->mManager->PresContext();
    presContext->RestyleManager()->IncrementAnimationGeneration();
    aElementAnimations->UpdateAnimationGeneration(presContext);
    aElementAnimations->PostUpdateLayerAnimations();

    // Invalidate our style rule.
    aElementAnimations->mNeedsRefreshes = true;
    aElementAnimations->mStyleRuleRefreshTime = TimeStamp();
  }
}

/* virtual */ void
nsAnimationManager::WillRefresh(mozilla::TimeStamp aTime)
{
  MOZ_ASSERT(mPresContext,
             "refresh driver should not notify additional observers "
             "after pres context has been destroyed");
  if (!mPresContext->GetPresShell()) {
    // Someone might be keeping mPresContext alive past the point
    // where it has been torn down; don't bother doing anything in
    // this case.  But do get rid of all our transitions so we stop
    // triggering refreshes.
    RemoveAllElementCollections();
    return;
  }

  FlushAnimations(Can_Throttle);
}

void
nsAnimationManager::FlushAnimations(FlushFlags aFlags)
{
  TimeStamp now = mPresContext->RefreshDriver()->MostRecentRefresh();
  bool didThrottle = false;
  for (PRCList *l = PR_LIST_HEAD(&mElementCollections);
       l != &mElementCollections;
       l = PR_NEXT_LINK(l)) {
    AnimationCollection* collection = static_cast<AnimationCollection*>(l);

    nsAutoAnimationMutationBatch mb(collection->mElement);

    collection->Tick();
    bool canThrottleTick = aFlags == Can_Throttle &&
      collection->CanPerformOnCompositorThread(
        AnimationCollection::CanAnimateFlags(0)) &&
      collection->CanThrottleAnimation(now);

    nsRefPtr<css::AnimValuesStyleRule> oldStyleRule = collection->mStyleRule;
    UpdateStyleAndEvents(collection, now, canThrottleTick
                                          ? EnsureStyleRule_IsThrottled
                                          : EnsureStyleRule_IsNotThrottled);
    if (oldStyleRule != collection->mStyleRule) {
      collection->PostRestyleForAnimation(mPresContext);
    } else {
      didThrottle = true;
    }
  }

  if (didThrottle) {
    mPresContext->Document()->SetNeedStyleFlush();
  }

  MaybeStartOrStopObservingRefreshDriver();

  DispatchEvents(); // may destroy us
}

void
nsAnimationManager::DoDispatchEvents()
{
  EventArray events;
  mPendingEvents.SwapElements(events);
  for (uint32_t i = 0, i_end = events.Length(); i < i_end; ++i) {
    AnimationEventInfo &info = events[i];
    EventDispatcher::Dispatch(info.mElement, mPresContext, &info.mEvent);

    if (!mPresContext) {
      break;
    }
  }
}
