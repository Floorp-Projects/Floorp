/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAnimationManager.h"
#include "nsTransitionManager.h"
#include "mozilla/dom/CSSAnimationBinding.h"

#include "mozilla/EffectCompositor.h"
#include "mozilla/EffectSet.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/StyleAnimationValue.h"
#include "mozilla/dom/DocumentTimeline.h"
#include "mozilla/dom/KeyframeEffectReadOnly.h"

#include "nsPresContext.h"
#include "nsStyleSet.h"
#include "nsStyleChangeList.h"
#include "nsCSSRules.h"
#include "mozilla/RestyleManager.h"
#include "nsLayoutUtils.h"
#include "nsIFrame.h"
#include "nsIDocument.h"
#include "nsDOMMutationObserver.h"
#include <algorithm> // std::stable_sort
#include <math.h>

using namespace mozilla;
using namespace mozilla::css;
using mozilla::dom::Animation;
using mozilla::dom::AnimationPlayState;
using mozilla::dom::KeyframeEffectReadOnly;
using mozilla::dom::CSSAnimation;

namespace {

// Pair of an event message and elapsed time used when determining the set of
// events to queue.
typedef Pair<EventMessage, StickyTimeDuration> EventPair;

} // anonymous namespace

////////////////////////// CSSAnimation ////////////////////////////

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
    PlayNoUpdate(rv, Animation::LimitBehavior::Continue);
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
  PauseNoUpdate(rv);
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
CSSAnimation::QueueEvents()
{
  if (!mEffect) {
    return;
  }

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

  dom::Element* owningElement;
  CSSPseudoElementType owningPseudoType;
  mOwningElement.GetElement(owningElement, owningPseudoType);
  MOZ_ASSERT(owningElement, "Owning element should be set");

  // Get the nsAnimationManager so we can queue events on it
  nsPresContext* presContext = mOwningElement.GetRenderedPresContext();
  if (!presContext) {
    return;
  }
  nsAnimationManager* manager = presContext->AnimationManager();

  ComputedTiming computedTiming = mEffect->GetComputedTiming();

  if (computedTiming.mPhase == ComputedTiming::AnimationPhase::Null) {
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
    computedTiming.mPhase == ComputedTiming::AnimationPhase::Active;
  bool isSameIteration =
         computedTiming.mCurrentIteration == mPreviousPhaseOrIteration;
  bool skippedActivePhase =
    (mPreviousPhaseOrIteration == PREVIOUS_PHASE_BEFORE &&
     computedTiming.mPhase == ComputedTiming::AnimationPhase::After) ||
    (mPreviousPhaseOrIteration == PREVIOUS_PHASE_AFTER &&
     computedTiming.mPhase == ComputedTiming::AnimationPhase::Before);
  bool skippedFirstIteration =
    isActive &&
    mPreviousPhaseOrIteration == PREVIOUS_PHASE_BEFORE &&
    computedTiming.mCurrentIteration > 0;

  MOZ_ASSERT(!skippedActivePhase || (!isActive && !wasActive),
             "skippedActivePhase only makes sense if we were & are inactive");

  if (computedTiming.mPhase == ComputedTiming::AnimationPhase::Before) {
    mPreviousPhaseOrIteration = PREVIOUS_PHASE_BEFORE;
  } else if (computedTiming.mPhase == ComputedTiming::AnimationPhase::Active) {
    mPreviousPhaseOrIteration = computedTiming.mCurrentIteration;
  } else if (computedTiming.mPhase == ComputedTiming::AnimationPhase::After) {
    mPreviousPhaseOrIteration = PREVIOUS_PHASE_AFTER;
  }

  AutoTArray<EventPair, 2> events;
  StickyTimeDuration initialAdvance = StickyTimeDuration(InitialAdvance());
  StickyTimeDuration iterationStart = computedTiming.mDuration *
                                      computedTiming.mCurrentIteration;
  const StickyTimeDuration& activeDuration = computedTiming.mActiveDuration;

  if (skippedFirstIteration) {
    // Notify animationstart and animationiteration in same tick.
    events.AppendElement(EventPair(eAnimationStart, initialAdvance));
    events.AppendElement(EventPair(eAnimationIteration,
                                   std::max(iterationStart, initialAdvance)));
  } else if (!wasActive && isActive) {
    events.AppendElement(EventPair(eAnimationStart, initialAdvance));
  } else if (wasActive && !isActive) {
    events.AppendElement(EventPair(eAnimationEnd, activeDuration));
  } else if (wasActive && isActive && !isSameIteration) {
    events.AppendElement(EventPair(eAnimationIteration, iterationStart));
  } else if (skippedActivePhase) {
    events.AppendElement(EventPair(eAnimationStart,
                                   std::min(initialAdvance, activeDuration)));
    events.AppendElement(EventPair(eAnimationEnd, activeDuration));
  } else {
    return; // No events need to be sent
  }

  for (const EventPair& pair : events){
    manager->QueueEvent(
               AnimationEventInfo(owningElement, owningPseudoType,
                                  pair.first(), mAnimationName,
                                  pair.second(),
                                  ElapsedTimeToTimeStamp(pair.second()),
                                  this));
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

TimeStamp
CSSAnimation::ElapsedTimeToTimeStamp(const StickyTimeDuration&
                                       aElapsedTime) const
{
  return AnimationTimeToTimeStamp(aElapsedTime +
                                  mEffect->SpecifiedTiming().mDelay);
}

////////////////////////// nsAnimationManager ////////////////////////////

NS_IMPL_CYCLE_COLLECTION(nsAnimationManager, mEventDispatcher)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(nsAnimationManager, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(nsAnimationManager, Release)

// Find the matching animation by |aName| in the old list
// of animations and remove the matched animation from the list.
static already_AddRefed<CSSAnimation>
PopExistingAnimation(const nsAString& aName,
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

static void
UpdateOldAnimationPropertiesWithNew(
    CSSAnimation& aOld,
    TimingParams& aNewTiming,
    nsTArray<Keyframe>& aNewKeyframes,
    bool aNewIsStylePaused,
    nsStyleContext* aStyleContext)
{
  bool animationChanged = false;

  // Update the old from the new so we can keep the original object
  // identity (and any expando properties attached to it).
  if (aOld.GetEffect()) {
    AnimationEffectReadOnly* oldEffect = aOld.GetEffect();
    animationChanged = oldEffect->SpecifiedTiming() != aNewTiming;
    oldEffect->SetSpecifiedTiming(aNewTiming);

    KeyframeEffectReadOnly* oldKeyframeEffect = oldEffect->AsKeyframeEffect();
    if (oldKeyframeEffect) {
      oldKeyframeEffect->SetKeyframes(Move(aNewKeyframes), aStyleContext);
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

void
nsAnimationManager::UpdateAnimations(nsStyleContext* aStyleContext,
                                     mozilla::dom::Element* aElement)
{
  MOZ_ASSERT(mPresContext->IsDynamic(),
             "Should not update animations for print or print preview");
  MOZ_ASSERT(aElement->IsInComposedDoc(),
             "Should not update animations that are not attached to the "
             "document tree");

  // Everything that causes our animation data to change triggers a
  // style change, which in turn triggers a non-animation restyle.
  // Likewise, when we initially construct frames, we're not in a
  // style change, but also not in an animation restyle.

  const nsStyleDisplay* disp = aStyleContext->StyleDisplay();
  CSSAnimationCollection* collection =
    CSSAnimationCollection::GetAnimationCollection(aElement,
                                                   aStyleContext->
                                                     GetPseudoType());
  if (!collection &&
      disp->mAnimationNameCount == 1 &&
      disp->mAnimations[0].GetName().IsEmpty()) {
    return;
  }

  nsAutoAnimationMutationBatch mb(aElement->OwnerDoc());

  // Build the updated animations list, extracting matching animations from
  // the existing collection as we go.
  OwningCSSAnimationPtrArray newAnimations;
  if (!aStyleContext->IsInDisplayNoneSubtree()) {
    BuildAnimations(aStyleContext, aElement, collection, newAnimations);
  }

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
        aElement, aStyleContext->GetPseudoType(), &createdCollection);
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
    newAnimations[newAnimIdx]->CancelFromStyle();
  }

  // We don't actually dispatch the pending events now.  We'll either
  // dispatch them the next time we get a refresh driver notification
  // or the next time somebody calls
  // nsPresShell::FlushPendingNotifications.
  if (mEventDispatcher.HasQueuedEvents()) {
    mPresContext->Document()->SetNeedStyleFlush();
  }
}

void
nsAnimationManager::StopAnimationsForElement(
  mozilla::dom::Element* aElement,
  mozilla::CSSPseudoElementType aPseudoType)
{
  MOZ_ASSERT(aElement);
  CSSAnimationCollection* collection =
    CSSAnimationCollection::GetAnimationCollection(aElement, aPseudoType);
  if (!collection) {
    return;
  }

  nsAutoAnimationMutationBatch mb(aElement->OwnerDoc());
  collection->Destroy();
}

class ResolvedStyleCache {
public:
  ResolvedStyleCache() : mCache() {}
  nsStyleContext* Get(nsPresContext *aPresContext,
                      nsStyleContext *aParentStyleContext,
                      Declaration* aKeyframeDeclaration);

private:
  nsRefPtrHashtable<nsPtrHashKey<Declaration>, nsStyleContext> mCache;
};

nsStyleContext*
ResolvedStyleCache::Get(nsPresContext *aPresContext,
                        nsStyleContext *aParentStyleContext,
                        Declaration* aKeyframeDeclaration)
{
  // FIXME (spec):  The css3-animations spec isn't very clear about how
  // properties are resolved when they have values that depend on other
  // properties (e.g., values in 'em').  I presume that they're resolved
  // relative to the other styles of the element.  The question is
  // whether they are resolved relative to other animations:  I assume
  // that they're not, since that would prevent us from caching a lot of
  // data that we'd really like to cache (in particular, the
  // StyleAnimationValue values in AnimationPropertySegment).
  nsStyleContext *result = mCache.GetWeak(aKeyframeDeclaration);
  if (!result) {
    aKeyframeDeclaration->SetImmutable();
    // The spec says that !important declarations should just be ignored
    MOZ_ASSERT(!aKeyframeDeclaration->HasImportantData(),
               "Keyframe rule has !important data");

    nsCOMArray<nsIStyleRule> rules;
    rules.AppendObject(aKeyframeDeclaration);
    MOZ_ASSERT(aPresContext->StyleSet()->IsGecko(),
               "ServoStyleSet should not use nsAnimationManager for "
               "animations");
    RefPtr<nsStyleContext> resultStrong = aPresContext->StyleSet()->AsGecko()->
      ResolveStyleByAddingRules(aParentStyleContext, rules);
    mCache.Put(aKeyframeDeclaration, resultStrong);
    result = resultStrong;
  }
  return result;
}

class MOZ_STACK_CLASS CSSAnimationBuilder final {
public:
  CSSAnimationBuilder(nsStyleContext* aStyleContext,
                      dom::Element* aTarget,
                      nsAnimationManager::CSSAnimationCollection* aCollection)
    : mStyleContext(aStyleContext)
    , mTarget(aTarget)
    , mCollection(aCollection)
  {
    MOZ_ASSERT(aStyleContext);
    MOZ_ASSERT(aTarget);
    mTimeline = mTarget->OwnerDoc()->Timeline();
  }

  // Returns a new animation set up with given StyleAnimation and
  // keyframe rules.
  // Or returns an existing animation matching StyleAnimation's name updated
  // with the new StyleAnimation and keyframe rules.
  already_AddRefed<CSSAnimation>
  Build(nsPresContext* aPresContext,
        const StyleAnimation& aSrc,
        const nsCSSKeyframesRule* aRule);

private:
  nsTArray<Keyframe> BuildAnimationFrames(nsPresContext* aPresContext,
                                          const StyleAnimation& aSrc,
                                          const nsCSSKeyframesRule* aRule);
  Maybe<ComputedTimingFunction> GetKeyframeTimingFunction(
    nsPresContext* aPresContext,
    nsCSSKeyframeRule* aKeyframeRule,
    const Maybe<ComputedTimingFunction>& aInheritedTimingFunction);
  nsTArray<PropertyValuePair> GetKeyframePropertyValues(
    nsPresContext* aPresContext,
    nsCSSKeyframeRule* aKeyframeRule,
    nsCSSPropertyIDSet& aAnimatedProperties);
  void FillInMissingKeyframeValues(
    nsPresContext* aPresContext,
    nsCSSPropertyIDSet aAnimatedProperties,
    nsCSSPropertyIDSet aPropertiesSetAtStart,
    nsCSSPropertyIDSet aPropertiesSetAtEnd,
    const Maybe<ComputedTimingFunction>& aInheritedTimingFunction,
    nsTArray<Keyframe>& aKeyframes);
  void AppendProperty(nsPresContext* aPresContext,
                      nsCSSPropertyID aProperty,
                      nsTArray<PropertyValuePair>& aPropertyValues);
  nsCSSValue GetComputedValue(nsPresContext* aPresContext,
                              nsCSSPropertyID aProperty);

  static TimingParams TimingParamsFrom(
    const StyleAnimation& aStyleAnimation)
  {
    TimingParams timing;

    timing.mDuration.emplace(StickyTimeDuration::FromMilliseconds(
                               aStyleAnimation.GetDuration()));
    timing.mDelay = TimeDuration::FromMilliseconds(aStyleAnimation.GetDelay());
    timing.mIterations = aStyleAnimation.GetIterationCount();
    MOZ_ASSERT(timing.mIterations >= 0.0 && !IsNaN(timing.mIterations),
               "mIterations should be nonnegative & finite, as ensured by "
               "CSSParser");
    timing.mDirection = aStyleAnimation.GetDirection();
    timing.mFill = aStyleAnimation.GetFillMode();

    return timing;
  }

  RefPtr<nsStyleContext> mStyleContext;
  RefPtr<dom::Element> mTarget;
  RefPtr<dom::DocumentTimeline> mTimeline;

  ResolvedStyleCache mResolvedStyles;
  RefPtr<nsStyleContext> mStyleWithoutAnimation;
  // Existing collection, nullptr if the target element has no animations.
  nsAnimationManager::CSSAnimationCollection* mCollection;
};

static Maybe<ComputedTimingFunction>
ConvertTimingFunction(const nsTimingFunction& aTimingFunction);

already_AddRefed<CSSAnimation>
CSSAnimationBuilder::Build(nsPresContext* aPresContext,
                           const StyleAnimation& aSrc,
                           const nsCSSKeyframesRule* aRule)
{
  MOZ_ASSERT(aPresContext);
  MOZ_ASSERT(aRule);

  TimingParams timing = TimingParamsFrom(aSrc);

  nsTArray<Keyframe> keyframes =
    BuildAnimationFrames(aPresContext, aSrc, aRule);

  bool isStylePaused =
    aSrc.GetPlayState() == NS_STYLE_ANIMATION_PLAY_STATE_PAUSED;

  // Find the matching animation with animation name in the old list
  // of animations and remove the matched animation from the list.
  RefPtr<CSSAnimation> oldAnim =
    PopExistingAnimation(aSrc.GetName(), mCollection);

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
                                        keyframes,
                                        isStylePaused,
                                        mStyleContext);
    return oldAnim.forget();
  }

  // mTarget is non-null here, so we emplace it directly.
  Maybe<OwningAnimationTarget> target;
  target.emplace(mTarget, mStyleContext->GetPseudoType());
  KeyframeEffectParams effectOptions;
  RefPtr<KeyframeEffectReadOnly> effect =
    new KeyframeEffectReadOnly(aPresContext->Document(), target, timing,
                               effectOptions);

  effect->SetKeyframes(Move(keyframes), mStyleContext);

  RefPtr<CSSAnimation> animation =
    new CSSAnimation(aPresContext->Document()->GetScopeObject(),
                     aSrc.GetName());
  animation->SetOwningElement(
    OwningElementRef(*mTarget, mStyleContext->GetPseudoType()));

  animation->SetTimelineNoUpdate(mTimeline);
  animation->SetEffectNoUpdate(effect);

  if (isStylePaused) {
    animation->PauseFromStyle();
  } else {
    animation->PlayFromStyle();
  }

  return animation.forget();
}

nsTArray<Keyframe>
CSSAnimationBuilder::BuildAnimationFrames(nsPresContext* aPresContext,
                                          const StyleAnimation& aSrc,
                                          const nsCSSKeyframesRule* aRule)
{
  // Ideally we'd like to build up a set of Keyframe objects that more-or-less
  // reflect the keyframes as-specified in the @keyframes rule(s) so that
  // authors get something intuitive when they call anim.effect.getKeyframes().
  //
  // That, however, proves to be difficult because the way CSS declarations are
  // processed differs from how we are able to represent keyframes as
  // JavaScript objects in the Web Animations API.
  //
  // For example,
  //
  //   { margin: 10px; margin-left: 20px }
  //
  // could be represented as:
  //
  //   { margin: '10px', marginLeft: '20px' }
  //
  // BUT:
  //
  //   { margin-left: 20px; margin: 10px }
  //
  // would be represented as:
  //
  //   { margin: '10px' }
  //
  // Likewise,
  //
  //   { margin-left: 20px; margin-left: 30px }
  //
  // would be represented as:
  //
  //   { marginLeft: '30px' }
  //
  // As such, the mapping between source @keyframes and the Keyframe objects
  // becomes obscured. The deviation is even more significant when we consider
  // cascading between @keyframes rules and variable references in shorthand
  // properties.
  //
  // We could, perhaps, produce a mapping that makes sense most of the time
  // but it would be complex and need to be specified and implemented
  // interoperably. Instead, for now, for CSS Animations (and CSS Transitions,
  // for that matter) we resolve values on @keyframes down to computed values
  // (thereby expanding shorthands and variable references) and then pick up the
  // last value for each longhand property at each offset.

  // FIXME: There is a pending spec change to make multiple @keyframes
  // rules with the same name cascade but we don't support that yet.

  Maybe<ComputedTimingFunction> inheritedTimingFunction =
    ConvertTimingFunction(aSrc.GetTimingFunction());

  // First, make up Keyframe objects for each rule
  nsTArray<Keyframe> keyframes;
  nsCSSPropertyIDSet animatedProperties;

  for (auto ruleIdx = 0, ruleEnd = aRule->StyleRuleCount();
       ruleIdx != ruleEnd; ++ruleIdx) {
    css::Rule* cssRule = aRule->GetStyleRuleAt(ruleIdx);
    MOZ_ASSERT(cssRule, "must have rule");
    MOZ_ASSERT(cssRule->GetType() == css::Rule::KEYFRAME_RULE,
               "must be keyframe rule");
    nsCSSKeyframeRule* keyframeRule = static_cast<nsCSSKeyframeRule*>(cssRule);

    const nsTArray<float>& keys = keyframeRule->GetKeys();
    for (float key : keys) {
      if (key < 0.0f || key > 1.0f) {
        continue;
      }

      Keyframe keyframe;
      keyframe.mOffset.emplace(key);
      keyframe.mTimingFunction =
        GetKeyframeTimingFunction(aPresContext, keyframeRule,
                                  inheritedTimingFunction);
      keyframe.mPropertyValues =
        GetKeyframePropertyValues(aPresContext, keyframeRule,
                                  animatedProperties);

      keyframes.AppendElement(Move(keyframe));
    }
  }

  // Next, stable sort by offset
  std::stable_sort(keyframes.begin(), keyframes.end(),
                   [](const Keyframe& a, const Keyframe& b)
                   {
                     return a.mOffset < b.mOffset;
                   });

  // Then walk backwards through the keyframes and drop overridden properties.
  nsCSSPropertyIDSet propertiesSetAtCurrentOffset;
  nsCSSPropertyIDSet propertiesSetAtStart;
  nsCSSPropertyIDSet propertiesSetAtEnd;
  double currentOffset = -1.0;
  for (size_t keyframeIdx = keyframes.Length();
       keyframeIdx > 0;
       --keyframeIdx) {
    Keyframe& keyframe = keyframes[keyframeIdx - 1];
    MOZ_ASSERT(keyframe.mOffset, "Should have filled in the offset");

    if (keyframe.mOffset.value() != currentOffset) {
      propertiesSetAtCurrentOffset.Empty();
      currentOffset = keyframe.mOffset.value();
    }

    // Get the set of properties from this keyframe that have not
    // already been set at this offset.
    nsTArray<PropertyValuePair> uniquePropertyValues;
    uniquePropertyValues.SetCapacity(keyframe.mPropertyValues.Length());
    for (const PropertyValuePair& pair : keyframe.mPropertyValues) {
      if (!propertiesSetAtCurrentOffset.HasProperty(pair.mProperty)) {
        uniquePropertyValues.AppendElement(pair);
        propertiesSetAtCurrentOffset.AddProperty(pair.mProperty);

        if (currentOffset == 0.0) {
          propertiesSetAtStart.AddProperty(pair.mProperty);
        } else if (currentOffset == 1.0) {
          propertiesSetAtEnd.AddProperty(pair.mProperty);
        }
      }
    }

    // If we have a keyframe at the same offset with the same timing
    // function we should merge our (unique) values into it.
    // Otherwise, we should update the existing keyframe with only the
    // unique properties.
    //
    // Bug 1216843: We should also match composite modes here.
    Keyframe* existingKeyframe = nullptr;
    // Don't bother searching for an existing keyframe if we don't
    // have anything to contribute to it.
    if (!uniquePropertyValues.IsEmpty()) {
      for (size_t i = keyframeIdx; i < keyframes.Length(); i++) {
        Keyframe& kf = keyframes[i];
        if (kf.mOffset.value() != currentOffset) {
          break;
        }
        if (kf.mTimingFunction == keyframe.mTimingFunction) {
          existingKeyframe = &kf;
          break;
        }
      }
    }

    if (existingKeyframe) {
      existingKeyframe->
        mPropertyValues.AppendElements(Move(uniquePropertyValues));
      keyframe.mPropertyValues.Clear();
    } else {
      keyframe.mPropertyValues.SwapElements(uniquePropertyValues);
    }

    // Check for a now-empty keyframe
    if (keyframe.mPropertyValues.IsEmpty()) {
      keyframes.RemoveElementAt(keyframeIdx - 1);
      // existingKeyframe might dangle now
    }
  }

  // Finally, we need to look for any animated properties that have an
  // implicit 'to' or 'from' value and fill in the appropriate keyframe
  // with the current computed style.
  FillInMissingKeyframeValues(aPresContext, animatedProperties,
                              propertiesSetAtStart, propertiesSetAtEnd,
                              inheritedTimingFunction, keyframes);

  return keyframes;
}

Maybe<ComputedTimingFunction>
CSSAnimationBuilder::GetKeyframeTimingFunction(
    nsPresContext* aPresContext,
    nsCSSKeyframeRule* aKeyframeRule,
    const Maybe<ComputedTimingFunction>& aInheritedTimingFunction)
{
  Maybe<ComputedTimingFunction> result;

  if (aKeyframeRule->Declaration() &&
      aKeyframeRule->Declaration()->HasProperty(
        eCSSProperty_animation_timing_function)) {
    RefPtr<nsStyleContext> keyframeRuleContext =
      mResolvedStyles.Get(aPresContext, mStyleContext,
                          aKeyframeRule->Declaration());
    const nsTimingFunction& tf = keyframeRuleContext->StyleDisplay()->
      mAnimations[0].GetTimingFunction();
    result = ConvertTimingFunction(tf);
  } else {
    result = aInheritedTimingFunction;
  }

  return result;
}

static Maybe<ComputedTimingFunction>
ConvertTimingFunction(const nsTimingFunction& aTimingFunction)
{
  Maybe<ComputedTimingFunction> result;

  if (aTimingFunction.mType != nsTimingFunction::Type::Linear) {
    result.emplace();
    result->Init(aTimingFunction);
  }

  return result;
}

nsTArray<PropertyValuePair>
CSSAnimationBuilder::GetKeyframePropertyValues(
    nsPresContext* aPresContext,
    nsCSSKeyframeRule* aKeyframeRule,
    nsCSSPropertyIDSet& aAnimatedProperties)
{
  nsTArray<PropertyValuePair> result;
  RefPtr<nsStyleContext> styleContext =
    mResolvedStyles.Get(aPresContext, mStyleContext,
                        aKeyframeRule->Declaration());

  for (nsCSSPropertyID prop = nsCSSPropertyID(0);
       prop < eCSSProperty_COUNT_no_shorthands;
       prop = nsCSSPropertyID(prop + 1)) {
    if (nsCSSProps::kAnimTypeTable[prop] == eStyleAnimType_None ||
        !aKeyframeRule->Declaration()->HasNonImportantValueFor(prop)) {
      continue;
    }

    PropertyValuePair pair;
    pair.mProperty = prop;

    StyleAnimationValue computedValue;
    if (!StyleAnimationValue::ExtractComputedValue(prop, styleContext,
                                                   computedValue)) {
      continue;
    }
    DebugOnly<bool> uncomputeResult =
      StyleAnimationValue::UncomputeValue(prop, Move(computedValue),
                                          pair.mValue);
    MOZ_ASSERT(uncomputeResult,
               "Unable to get specified value from computed value");
    MOZ_ASSERT(pair.mValue.GetUnit() != eCSSUnit_Null,
               "Not expecting to read invalid properties");

    result.AppendElement(Move(pair));
    aAnimatedProperties.AddProperty(prop);
  }

  return result;
}

// Utility function to walk through |aIter| to find the Keyframe with
// matching offset and timing function but stopping as soon as the offset
// differs from |aOffset| (i.e. it assumes a sorted iterator).
//
// If a matching Keyframe is found,
//   Returns true and sets |aIndex| to the index of the matching Keyframe
//   within |aIter|.
//
// If no matching Keyframe is found,
//   Returns false and sets |aIndex| to the index in the iterator of the
//   first Keyframe with an offset differing to |aOffset| or, if the end
//   of the iterator is reached, sets |aIndex| to the index after the last
//   Keyframe.
template <class IterType>
static bool
FindMatchingKeyframe(
    IterType&& aIter,
    double aOffset,
    const Maybe<ComputedTimingFunction>& aTimingFunctionToMatch,
    size_t& aIndex)
{
  aIndex = 0;
  for (Keyframe& keyframe : aIter) {
    if (keyframe.mOffset.value() != aOffset) {
      break;
    }
    if (keyframe.mTimingFunction == aTimingFunctionToMatch) {
      return true;
    }
    ++aIndex;
  }
  return false;
}

void
CSSAnimationBuilder::FillInMissingKeyframeValues(
    nsPresContext* aPresContext,
    nsCSSPropertyIDSet aAnimatedProperties,
    nsCSSPropertyIDSet aPropertiesSetAtStart,
    nsCSSPropertyIDSet aPropertiesSetAtEnd,
    const Maybe<ComputedTimingFunction>& aInheritedTimingFunction,
    nsTArray<Keyframe>& aKeyframes)
{
  static const size_t kNotSet = static_cast<size_t>(-1);

  // Find/create the keyframe to add start values to
  size_t startKeyframeIndex = kNotSet;
  if (!aAnimatedProperties.Equals(aPropertiesSetAtStart) &&
      !FindMatchingKeyframe(aKeyframes, 0.0, aInheritedTimingFunction,
                            startKeyframeIndex)) {
    Keyframe newKeyframe;
    newKeyframe.mOffset.emplace(0.0);
    newKeyframe.mTimingFunction = aInheritedTimingFunction;
    aKeyframes.InsertElementAt(startKeyframeIndex, Move(newKeyframe));
  }

  // Find/create the keyframe to add end values to
  size_t endKeyframeIndex = kNotSet;
  if (!aAnimatedProperties.Equals(aPropertiesSetAtEnd)) {
    if (!FindMatchingKeyframe(Reversed(aKeyframes), 1.0,
                              aInheritedTimingFunction, endKeyframeIndex)) {
      Keyframe newKeyframe;
      newKeyframe.mOffset.emplace(1.0);
      newKeyframe.mTimingFunction = aInheritedTimingFunction;
      aKeyframes.AppendElement(Move(newKeyframe));
      endKeyframeIndex = aKeyframes.Length() - 1;
    } else {
      // endKeyframeIndex is currently a count from the end of the array
      // so we need to reverse it.
      endKeyframeIndex = aKeyframes.Length() - 1 - endKeyframeIndex;
    }
  }

  if (startKeyframeIndex == kNotSet && endKeyframeIndex == kNotSet) {
    return;
  }

  // Now that we have finished manipulating aKeyframes, it is safe to
  // take pointers to its elements.
  Keyframe* startKeyframe = startKeyframeIndex == kNotSet
                            ? nullptr : &aKeyframes[startKeyframeIndex];
  Keyframe* endKeyframe   = endKeyframeIndex == kNotSet
                            ? nullptr : &aKeyframes[endKeyframeIndex];

  // Iterate through all properties and fill-in missing values
  for (nsCSSPropertyID prop = nsCSSPropertyID(0);
       prop < eCSSProperty_COUNT_no_shorthands;
       prop = nsCSSPropertyID(prop + 1)) {
    if (!aAnimatedProperties.HasProperty(prop)) {
      continue;
    }

    if (startKeyframe && !aPropertiesSetAtStart.HasProperty(prop)) {
      AppendProperty(aPresContext, prop, startKeyframe->mPropertyValues);
    }
    if (endKeyframe && !aPropertiesSetAtEnd.HasProperty(prop)) {
      AppendProperty(aPresContext, prop, endKeyframe->mPropertyValues);
    }
  }
}

void
CSSAnimationBuilder::AppendProperty(
    nsPresContext* aPresContext,
    nsCSSPropertyID aProperty,
    nsTArray<PropertyValuePair>& aPropertyValues)
{
  PropertyValuePair propertyValue;
  propertyValue.mProperty = aProperty;
  propertyValue.mValue = GetComputedValue(aPresContext, aProperty);

  aPropertyValues.AppendElement(Move(propertyValue));
}

nsCSSValue
CSSAnimationBuilder::GetComputedValue(nsPresContext* aPresContext,
                                      nsCSSPropertyID aProperty)
{
  nsCSSValue result;
  StyleAnimationValue computedValue;

  if (!mStyleWithoutAnimation) {
    MOZ_ASSERT(aPresContext->StyleSet()->IsGecko(),
               "ServoStyleSet should not use nsAnimationManager for "
               "animations");
    mStyleWithoutAnimation = aPresContext->StyleSet()->AsGecko()->
      ResolveStyleWithoutAnimation(mTarget, mStyleContext,
                                   eRestyle_AllHintsWithAnimations);
  }

  if (StyleAnimationValue::ExtractComputedValue(aProperty,
                                                mStyleWithoutAnimation,
                                                computedValue)) {
    DebugOnly<bool> uncomputeResult =
      StyleAnimationValue::UncomputeValue(aProperty, Move(computedValue),
                                          result);
    MOZ_ASSERT(uncomputeResult,
               "Unable to get specified value from computed value");
  }

  // If we hit this assertion, it probably means we are fetching a value from
  // the computed style that we don't know how to represent as
  // a StyleAnimationValue.
  MOZ_ASSERT(result.GetUnit() != eCSSUnit_Null, "Got null computed value");

  return result;
}

void
nsAnimationManager::BuildAnimations(nsStyleContext* aStyleContext,
                                    dom::Element* aTarget,
                                    CSSAnimationCollection* aCollection,
                                    OwningCSSAnimationPtrArray& aAnimations)
{
  MOZ_ASSERT(aAnimations.IsEmpty(), "expect empty array");

  const nsStyleDisplay *disp = aStyleContext->StyleDisplay();

  CSSAnimationBuilder builder(aStyleContext, aTarget, aCollection);

  for (size_t animIdx = disp->mAnimationNameCount; animIdx-- != 0;) {
    const StyleAnimation& src = disp->mAnimations[animIdx];

    // CSS Animations whose animation-name does not match a @keyframes rule do
    // not generate animation events. This includes when the animation-name is
    // "none" which is represented by an empty name in the StyleAnimation.
    // Since such animations neither affect style nor dispatch events, we do
    // not generate a corresponding CSSAnimation for them.
    MOZ_ASSERT(mPresContext->StyleSet()->IsGecko(),
               "ServoStyleSet should not use nsAnimationManager for "
               "animations");
    nsCSSKeyframesRule* rule =
      src.GetName().IsEmpty()
      ? nullptr
      : mPresContext->StyleSet()->AsGecko()->KeyframesRuleForName(src.GetName());
    if (!rule) {
      continue;
    }

    RefPtr<CSSAnimation> dest = builder.Build(mPresContext, src, rule);
    dest->SetAnimationIndex(static_cast<uint64_t>(animIdx));
    aAnimations.AppendElement(dest);
  }
}

