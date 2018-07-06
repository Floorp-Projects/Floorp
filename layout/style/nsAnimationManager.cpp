/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAnimationManager.h"
#include "nsTransitionManager.h"
#include "mozilla/dom/CSSAnimationBinding.h"

#include "mozilla/AnimationEventDispatcher.h"
#include "mozilla/AnimationTarget.h"
#include "mozilla/EffectCompositor.h"
#include "mozilla/EffectSet.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/ServoStyleSet.h"
#include "mozilla/StyleAnimationValue.h"
#include "mozilla/dom/AnimationEffect.h"
#include "mozilla/dom/DocumentTimeline.h"
#include "mozilla/dom/KeyframeEffect.h"

#include "nsPresContext.h"
#include "nsStyleChangeList.h"
#include "nsContentUtils.h"
#include "nsLayoutUtils.h"
#include "nsIFrame.h"
#include "nsIDocument.h"
#include "nsDOMMutationObserver.h"
#include "nsIPresShell.h"
#include "nsIPresShellInlines.h"
#include "nsRFPService.h"
#include <algorithm> // std::stable_sort
#include <math.h>

using namespace mozilla;
using namespace mozilla::css;
using mozilla::dom::Animation;
using mozilla::dom::AnimationEffect;
using mozilla::dom::AnimationPlayState;
using mozilla::dom::KeyframeEffect;
using mozilla::dom::CSSAnimation;

typedef mozilla::ComputedTiming::AnimationPhase AnimationPhase;

////////////////////////// CSSAnimation ////////////////////////////

JSObject*
CSSAnimation::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return dom::CSSAnimation_Binding::Wrap(aCx, this, aGivenProto);
}

mozilla::dom::Promise*
CSSAnimation::GetReady(ErrorResult& aRv)
{
  FlushUnanimatedStyle();
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
  FlushUnanimatedStyle();
  return Animation::PlayStateFromJS();
}

bool
CSSAnimation::PendingFromJS() const
{
  // Flush style since, for example, if the animation-play-state was just
  // changed its possible we should now be pending.
  FlushUnanimatedStyle();
  return Animation::PendingFromJS();
}

void
CSSAnimation::PlayFromJS(ErrorResult& aRv)
{
  // Note that flushing style below might trigger calls to
  // PlayFromStyle()/PauseFromStyle() on this object.
  FlushUnanimatedStyle();
  Animation::PlayFromJS(aRv);
}

void
CSSAnimation::PlayFromStyle()
{
  mIsStylePaused = false;
  if (!mPauseShouldStick) {
    ErrorResult rv;
    Animation::Play(rv, Animation::LimitBehavior::Continue);
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
  Animation::Pause(rv);
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
CSSAnimation::Tick()
{
  Animation::Tick();
  QueueEvents();
}

bool
CSSAnimation::HasLowerCompositeOrderThan(const CSSAnimation& aOther) const
{
  MOZ_ASSERT(IsTiedToMarkup() && aOther.IsTiedToMarkup(),
             "Should only be called for CSS animations that are sorted "
             "as CSS animations (i.e. tied to CSS markup)");

  // 0. Object-equality case
  if (&aOther == this) {
    return false;
  }

  // 1. Sort by document order
  if (!mOwningElement.Equals(aOther.mOwningElement)) {
    return mOwningElement.LessThan(aOther.mOwningElement);
  }

  // 2. (Same element and pseudo): Sort by position in animation-name
  return mAnimationIndex < aOther.mAnimationIndex;
}

void
CSSAnimation::QueueEvents(const StickyTimeDuration& aActiveTime)
{
  // If the animation is pending, we ignore animation events until we finish
  // pending.
  if (mPendingState != PendingState::NotPending) {
    return;
  }

  // CSS animations dispatch events at their owning element. This allows
  // script to repurpose a CSS animation to target a different element,
  // to use a group effect (which has no obvious "target element"), or
  // to remove the animation effect altogether whilst still getting
  // animation events.
  //
  // It does mean, however, that for a CSS animation that has no owning
  // element (e.g. it was created using the CSSAnimation constructor or
  // disassociated from CSS) no events are fired. If it becomes desirable
  // for these animations to still fire events we should spec the concept
  // of the "original owning element" or "event target" and allow script
  // to set it when creating a CSSAnimation object.
  if (!mOwningElement.IsSet()) {
    return;
  }

  nsPresContext* presContext = mOwningElement.GetPresContext();
  if (!presContext) {
    return;
  }

  uint64_t currentIteration = 0;
  ComputedTiming::AnimationPhase currentPhase;
  StickyTimeDuration intervalStartTime;
  StickyTimeDuration intervalEndTime;
  StickyTimeDuration iterationStartTime;

  if (!mEffect) {
    currentPhase = GetAnimationPhaseWithoutEffect
      <ComputedTiming::AnimationPhase>(*this);
    if (currentPhase == mPreviousPhase) {
      return;
    }
  } else {
    ComputedTiming computedTiming = mEffect->GetComputedTiming();
    currentPhase = computedTiming.mPhase;
    currentIteration = computedTiming.mCurrentIteration;
    if (currentPhase == mPreviousPhase &&
        currentIteration == mPreviousIteration) {
      return;
    }
    intervalStartTime = IntervalStartTime(computedTiming.mActiveDuration);
    intervalEndTime = IntervalEndTime(computedTiming.mActiveDuration);

    uint64_t iterationBoundary = mPreviousIteration > currentIteration
                                 ? currentIteration + 1
                                 : currentIteration;
    iterationStartTime  =
      computedTiming.mDuration.MultDouble(
        (iterationBoundary - computedTiming.mIterationStart));
  }

  TimeStamp startTimeStamp     = ElapsedTimeToTimeStamp(intervalStartTime);
  TimeStamp endTimeStamp       = ElapsedTimeToTimeStamp(intervalEndTime);
  TimeStamp iterationTimeStamp = ElapsedTimeToTimeStamp(iterationStartTime);

  AutoTArray<AnimationEventInfo, 2> events;

  auto appendAnimationEvent = [&](EventMessage aMessage,
                                  const StickyTimeDuration& aElapsedTime,
                                  const TimeStamp& aScheduledEventTimeStamp) {
    double elapsedTime = aElapsedTime.ToSeconds();
    if (aMessage == eAnimationCancel) {
      // 0 is an inappropriate value for this callsite. What we need to do is
      // use a single random value for all increasing times reportable.
      // That is to say, whenever elapsedTime goes negative (because an
      // animation restarts, something rewinds the animation, or otherwise)
      // a new random value for the mix-in must be generated.
      elapsedTime = nsRFPService::ReduceTimePrecisionAsSecs(elapsedTime, 0, TimerPrecisionType::RFPOnly);
    }
    events.AppendElement(AnimationEventInfo(mAnimationName,
                                            mOwningElement.Target(),
                                            aMessage,
                                            elapsedTime,
                                            aScheduledEventTimeStamp,
                                            this));
  };

  // Handle cancel event first
  if ((mPreviousPhase != AnimationPhase::Idle &&
       mPreviousPhase != AnimationPhase::After) &&
      currentPhase == AnimationPhase::Idle) {
    appendAnimationEvent(eAnimationCancel,
                         aActiveTime,
                         GetTimelineCurrentTimeAsTimeStamp());
  }

  switch (mPreviousPhase) {
    case AnimationPhase::Idle:
    case AnimationPhase::Before:
      if (currentPhase == AnimationPhase::Active) {
        appendAnimationEvent(eAnimationStart,
                             intervalStartTime,
                             startTimeStamp);
      } else if (currentPhase == AnimationPhase::After) {
        appendAnimationEvent(eAnimationStart,
                             intervalStartTime,
                             startTimeStamp);
        appendAnimationEvent(eAnimationEnd, intervalEndTime, endTimeStamp);
      }
      break;
    case AnimationPhase::Active:
      if (currentPhase == AnimationPhase::Before) {
        appendAnimationEvent(eAnimationEnd, intervalStartTime, startTimeStamp);
      } else if (currentPhase == AnimationPhase::Active) {
        // The currentIteration must have changed or element we would have
        // returned early above.
        MOZ_ASSERT(currentIteration != mPreviousIteration);
        appendAnimationEvent(eAnimationIteration,
                             iterationStartTime,
                             iterationTimeStamp);
      } else if (currentPhase == AnimationPhase::After) {
        appendAnimationEvent(eAnimationEnd, intervalEndTime, endTimeStamp);
      }
      break;
    case AnimationPhase::After:
      if (currentPhase == AnimationPhase::Before) {
        appendAnimationEvent(eAnimationStart, intervalEndTime, startTimeStamp);
        appendAnimationEvent(eAnimationEnd, intervalStartTime, endTimeStamp);
      } else if (currentPhase == AnimationPhase::Active) {
        appendAnimationEvent(eAnimationStart, intervalEndTime, endTimeStamp);
      }
      break;
  }
  mPreviousPhase = currentPhase;
  mPreviousIteration = currentIteration;

  if (!events.IsEmpty()) {
    presContext->AnimationEventDispatcher()->QueueEvents(std::move(events));
  }
}

void
CSSAnimation::UpdateTiming(SeekFlag aSeekFlag, SyncNotifyFlag aSyncNotifyFlag)
{
  if (mNeedsNewAnimationIndexWhenRun &&
      PlayState() != AnimationPlayState::Idle) {
    mAnimationIndex = sNextAnimationIndex++;
    mNeedsNewAnimationIndexWhenRun = false;
  }

  Animation::UpdateTiming(aSeekFlag, aSyncNotifyFlag);
}

////////////////////////// nsAnimationManager ////////////////////////////

// Find the matching animation by |aName| in the old list
// of animations and remove the matched animation from the list.
static already_AddRefed<CSSAnimation>
PopExistingAnimation(const nsAtom* aName,
                     nsAnimationManager::CSSAnimationCollection* aCollection)
{
  if (!aCollection) {
    return nullptr;
  }

  // Animations are stored in reverse order to how they appear in the
  // animation-name property. However, we want to match animations beginning
  // from the end of the animation-name list, so we iterate *forwards*
  // through the collection.
  for (size_t idx = 0, length = aCollection->mAnimations.Length();
       idx != length; ++ idx) {
    CSSAnimation* cssAnim = aCollection->mAnimations[idx];
    if (cssAnim->AnimationName() == aName) {
      RefPtr<CSSAnimation> match = cssAnim;
      aCollection->mAnimations.RemoveElementAt(idx);
      return match.forget();
    }
  }

  return nullptr;
}


class MOZ_STACK_CLASS ServoCSSAnimationBuilder final {
public:
  explicit ServoCSSAnimationBuilder(const ComputedStyle* aComputedStyle)
    : mComputedStyle(aComputedStyle)
  {
    MOZ_ASSERT(aComputedStyle);
  }

  bool BuildKeyframes(const Element& aElement,
                      nsPresContext* aPresContext,
                      nsAtom* aName,
                      const nsTimingFunction& aTimingFunction,
                      nsTArray<Keyframe>& aKeyframes)
  {
    return aPresContext->StyleSet()->GetKeyframesForName(
        aElement,
        *mComputedStyle,
        aName,
        aTimingFunction,
        aKeyframes);
  }
  void SetKeyframes(KeyframeEffect& aEffect, nsTArray<Keyframe>&& aKeyframes)
  {
    aEffect.SetKeyframes(std::move(aKeyframes), mComputedStyle);
  }

  // Currently all the animation building code in this file is based on
  // assumption that creating and removing animations should *not* trigger
  // additional restyles since those changes will be handled within the same
  // restyle.
  //
  // While that is true for the Gecko style backend, it is not true for the
  // Servo style backend where we want restyles to be triggered so that we
  // perform a second animation restyle where we will incorporate the changes
  // arising from creating and removing animations.
  //
  // Fortunately, our attempts to avoid posting extra restyles as part of the
  // processing here are imperfect and most of the time we happen to post
  // them anyway. Occasionally, however, we don't. For example, we don't post
  // a restyle when we create a new animation whose an animation index matches
  // the default value it was given already (which is typically only true when
  // the CSSAnimation we create is the first Animation created in a particular
  // content process).
  //
  // As a result, when we are using the Servo backend, whenever we have an added
  // or removed animation we need to explicitly trigger a restyle.
  //
  // This code should eventually disappear along with the Gecko style backend
  // and we should simply call Play() / Pause() / Cancel() etc. which will
  // post the required restyles.
  void NotifyNewOrRemovedAnimation(const Animation& aAnimation)
  {
    dom::AnimationEffect* effect = aAnimation.GetEffect();
    if (!effect) {
      return;
    }

    KeyframeEffect* keyframeEffect = effect->AsKeyframeEffect();
    if (!keyframeEffect) {
      return;
    }

    keyframeEffect->RequestRestyle(EffectCompositor::RestyleType::Standard);
  }

private:
  const ComputedStyle* mComputedStyle;
};


static void
UpdateOldAnimationPropertiesWithNew(
    CSSAnimation& aOld,
    TimingParams& aNewTiming,
    nsTArray<Keyframe>&& aNewKeyframes,
    bool aNewIsStylePaused,
    ServoCSSAnimationBuilder& aBuilder)
{
  bool animationChanged = false;

  // Update the old from the new so we can keep the original object
  // identity (and any expando properties attached to it).
  if (aOld.GetEffect()) {
    dom::AnimationEffect* oldEffect = aOld.GetEffect();
    animationChanged = oldEffect->SpecifiedTiming() != aNewTiming;
    oldEffect->SetSpecifiedTiming(aNewTiming);

    KeyframeEffect* oldKeyframeEffect = oldEffect->AsKeyframeEffect();
    if (oldKeyframeEffect) {
      aBuilder.SetKeyframes(*oldKeyframeEffect, std::move(aNewKeyframes));
    }
  }

  // Handle changes in play state. If the animation is idle, however,
  // changes to animation-play-state should *not* restart it.
  if (aOld.PlayState() != AnimationPlayState::Idle) {
    // CSSAnimation takes care of override behavior so that,
    // for example, if the author has called pause(), that will
    // override the animation-play-state.
    // (We should check aNew->IsStylePaused() but that requires
    //  downcasting to CSSAnimation and we happen to know that
    //  aNew will only ever be paused by calling PauseFromStyle
    //  making IsPausedOrPausing synonymous in this case.)
    if (!aOld.IsStylePaused() && aNewIsStylePaused) {
      aOld.PauseFromStyle();
      animationChanged = true;
    } else if (aOld.IsStylePaused() && !aNewIsStylePaused) {
      aOld.PlayFromStyle();
      animationChanged = true;
    }
  }

  // Updating the effect timing above might already have caused the
  // animation to become irrelevant so only add a changed record if
  // the animation is still relevant.
  if (animationChanged && aOld.IsRelevant()) {
    nsNodeUtils::AnimationChanged(&aOld);
  }
}

// Returns a new animation set up with given StyleAnimation.
// Or returns an existing animation matching StyleAnimation's name updated
// with the new StyleAnimation.
static already_AddRefed<CSSAnimation>
BuildAnimation(nsPresContext* aPresContext,
               const NonOwningAnimationTarget& aTarget,
               const nsStyleDisplay& aStyleDisplay,
               uint32_t animIdx,
               ServoCSSAnimationBuilder& aBuilder,
               nsAnimationManager::CSSAnimationCollection* aCollection)
{
  MOZ_ASSERT(aPresContext);

  nsAtom* animationName = aStyleDisplay.GetAnimationName(animIdx);
  nsTArray<Keyframe> keyframes;
  if (!aBuilder.BuildKeyframes(*aTarget.mElement,
                               aPresContext,
                               animationName,
                               aStyleDisplay.GetAnimationTimingFunction(animIdx),
                               keyframes)) {
    return nullptr;
  }

  TimingParams timing =
    TimingParamsFromCSSParams(aStyleDisplay.GetAnimationDuration(animIdx),
                              aStyleDisplay.GetAnimationDelay(animIdx),
                              aStyleDisplay.GetAnimationIterationCount(animIdx),
                              aStyleDisplay.GetAnimationDirection(animIdx),
                              aStyleDisplay.GetAnimationFillMode(animIdx));

  bool isStylePaused =
    aStyleDisplay.GetAnimationPlayState(animIdx) ==
      NS_STYLE_ANIMATION_PLAY_STATE_PAUSED;

  // Find the matching animation with animation name in the old list
  // of animations and remove the matched animation from the list.
  RefPtr<CSSAnimation> oldAnim =
    PopExistingAnimation(animationName, aCollection);

  if (oldAnim) {
    // Copy over the start times and (if still paused) pause starts
    // for each animation (matching on name only) that was also in the
    // old list of animations.
    // This means that we honor dynamic changes, which isn't what the
    // spec says to do, but WebKit seems to honor at least some of
    // them.  See
    // http://lists.w3.org/Archives/Public/www-style/2011Apr/0079.html
    // In order to honor what the spec said, we'd copy more data over.
    UpdateOldAnimationPropertiesWithNew(*oldAnim,
                                        timing,
                                        std::move(keyframes),
                                        isStylePaused,
                                        aBuilder);
    return oldAnim.forget();
  }

  // mTarget is non-null here, so we emplace it directly.
  Maybe<OwningAnimationTarget> target;
  target.emplace(aTarget.mElement, aTarget.mPseudoType);
  KeyframeEffectParams effectOptions;
  RefPtr<KeyframeEffect> effect =
    new KeyframeEffect(aPresContext->Document(), target, timing, effectOptions);

  aBuilder.SetKeyframes(*effect, std::move(keyframes));

  RefPtr<CSSAnimation> animation =
    new CSSAnimation(aPresContext->Document()->GetScopeObject(), animationName);
  animation->SetOwningElement(
    OwningElementRef(*aTarget.mElement, aTarget.mPseudoType));

  animation->SetTimelineNoUpdate(aTarget.mElement->OwnerDoc()->Timeline());
  animation->SetEffectNoUpdate(effect);

  if (isStylePaused) {
    animation->PauseFromStyle();
  } else {
    animation->PlayFromStyle();
  }

  aBuilder.NotifyNewOrRemovedAnimation(*animation);

  return animation.forget();
}


static nsAnimationManager::OwningCSSAnimationPtrArray
BuildAnimations(nsPresContext* aPresContext,
                const NonOwningAnimationTarget& aTarget,
                const nsStyleDisplay& aStyleDisplay,
                ServoCSSAnimationBuilder& aBuilder,
                nsAnimationManager::CSSAnimationCollection* aCollection,
                nsTHashtable<nsRefPtrHashKey<nsAtom>>& aReferencedAnimations)
{
  nsAnimationManager::OwningCSSAnimationPtrArray result;

  for (size_t animIdx = aStyleDisplay.mAnimationNameCount; animIdx-- != 0;) {
    nsAtom* name = aStyleDisplay.GetAnimationName(animIdx);
    // CSS Animations whose animation-name does not match a @keyframes rule do
    // not generate animation events. This includes when the animation-name is
    // "none" which is represented by an empty name in the StyleAnimation.
    // Since such animations neither affect style nor dispatch events, we do
    // not generate a corresponding CSSAnimation for them.
    if (name == nsGkAtoms::_empty) {
      continue;
    }

    aReferencedAnimations.PutEntry(name);
    RefPtr<CSSAnimation> dest = BuildAnimation(aPresContext,
                                               aTarget,
                                               aStyleDisplay,
                                               animIdx,
                                               aBuilder,
                                               aCollection);
    if (!dest) {
      continue;
    }

    dest->SetAnimationIndex(static_cast<uint64_t>(animIdx));
    result.AppendElement(dest);
  }
  return result;
}


void
nsAnimationManager::UpdateAnimations(
  dom::Element* aElement,
  CSSPseudoElementType aPseudoType,
  const ComputedStyle* aComputedStyle)
{
  MOZ_ASSERT(mPresContext->IsDynamic(),
             "Should not update animations for print or print preview");
  MOZ_ASSERT(aElement->IsInComposedDoc(),
             "Should not update animations that are not attached to the "
             "document tree");

  const nsStyleDisplay* disp = aComputedStyle
    ? aComputedStyle->ComputedData()->GetStyleDisplay()
    : nullptr;

  if (!disp || disp->mDisplay == StyleDisplay::None) {
    // If we are in a display:none subtree we will have no computed values.
    // However, if we are on the root of display:none subtree, the computed
    // values might not have been cleared yet.
    // In either case, since CSS animations should not run in display:none
    // subtrees we should stop (actually, destroy) any animations on this
    // element here.
    StopAnimationsForElement(aElement, aPseudoType);
    return;
  }

  NonOwningAnimationTarget target(aElement, aPseudoType);
  ServoCSSAnimationBuilder builder(aComputedStyle);

  DoUpdateAnimations(target, *disp, builder);
}

void
nsAnimationManager::DoUpdateAnimations(
  const NonOwningAnimationTarget& aTarget,
  const nsStyleDisplay& aStyleDisplay,
  ServoCSSAnimationBuilder& aBuilder)
{
  // Everything that causes our animation data to change triggers a
  // style change, which in turn triggers a non-animation restyle.
  // Likewise, when we initially construct frames, we're not in a
  // style change, but also not in an animation restyle.

  CSSAnimationCollection* collection =
    CSSAnimationCollection::GetAnimationCollection(aTarget.mElement,
                                                   aTarget.mPseudoType);
  if (!collection &&
      aStyleDisplay.mAnimationNameCount == 1 &&
      aStyleDisplay.mAnimations[0].GetName() == nsGkAtoms::_empty) {
    return;
  }

  nsAutoAnimationMutationBatch mb(aTarget.mElement->OwnerDoc());

  // Build the updated animations list, extracting matching animations from
  // the existing collection as we go.
  OwningCSSAnimationPtrArray newAnimations =
    BuildAnimations(mPresContext,
                    aTarget,
                    aStyleDisplay,
                    aBuilder,
                    collection,
                    mMaybeReferencedAnimations);

  if (newAnimations.IsEmpty()) {
    if (collection) {
      collection->Destroy();
    }
    return;
  }

  if (!collection) {
    bool createdCollection = false;
    collection =
      CSSAnimationCollection::GetOrCreateAnimationCollection(
        aTarget.mElement, aTarget.mPseudoType, &createdCollection);
    if (!collection) {
      MOZ_ASSERT(!createdCollection, "outparam should agree with return value");
      NS_WARNING("allocating collection failed");
      return;
    }

    if (createdCollection) {
      AddElementCollection(collection);
    }
  }
  collection->mAnimations.SwapElements(newAnimations);

  // Cancel removed animations
  for (size_t newAnimIdx = newAnimations.Length(); newAnimIdx-- != 0; ) {
    aBuilder.NotifyNewOrRemovedAnimation(*newAnimations[newAnimIdx]);
    newAnimations[newAnimIdx]->CancelFromStyle();
  }
}
