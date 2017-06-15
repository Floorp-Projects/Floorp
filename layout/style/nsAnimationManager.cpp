/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAnimationManager.h"
#include "nsTransitionManager.h"
#include "mozilla/dom/CSSAnimationBinding.h"

#include "mozilla/AnimationTarget.h"
#include "mozilla/EffectCompositor.h"
#include "mozilla/EffectSet.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/ServoStyleSet.h"
#include "mozilla/StyleAnimationValue.h"
#include "mozilla/dom/DocumentTimeline.h"
#include "mozilla/dom/KeyframeEffectReadOnly.h"

#include "nsPresContext.h"
#include "nsStyleSet.h"
#include "nsStyleChangeList.h"
#include "nsContentUtils.h"
#include "nsCSSRules.h"
#include "mozilla/GeckoRestyleManager.h"
#include "nsLayoutUtils.h"
#include "nsIFrame.h"
#include "nsIDocument.h"
#include "nsDOMMutationObserver.h"
#include "nsIPresShell.h"
#include "nsIPresShellInlines.h"
#include <algorithm> // std::stable_sort
#include <math.h>

using namespace mozilla;
using namespace mozilla::css;
using mozilla::dom::Animation;
using mozilla::dom::AnimationPlayState;
using mozilla::dom::KeyframeEffectReadOnly;
using mozilla::dom::CSSAnimation;

typedef mozilla::ComputedTiming::AnimationPhase AnimationPhase;

namespace {

struct AnimationEventParams {
  EventMessage mMessage;
  StickyTimeDuration mElapsedTime;
  TimeStamp mTimeStamp;
};

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
CSSAnimation::QueueEvents(StickyTimeDuration aActiveTime)
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

  dom::Element* owningElement;
  CSSPseudoElementType owningPseudoType;
  mOwningElement.GetElement(owningElement, owningPseudoType);
  MOZ_ASSERT(owningElement, "Owning element should be set");

  // Get the nsAnimationManager so we can queue events on it
  nsPresContext* presContext =
    nsContentUtils::GetContextForContent(owningElement);
  if (!presContext) {
    return;
  }
  nsAnimationManager* manager = presContext->AnimationManager();

  const StickyTimeDuration zeroDuration;
  uint64_t currentIteration = 0;
  ComputedTiming::AnimationPhase currentPhase;
  StickyTimeDuration intervalStartTime;
  StickyTimeDuration intervalEndTime;
  StickyTimeDuration iterationStartTime;

  if (!mEffect) {
    currentPhase = GetAnimationPhaseWithoutEffect
      <ComputedTiming::AnimationPhase>(*this);
  } else {
    ComputedTiming computedTiming = mEffect->GetComputedTiming();
    currentPhase = computedTiming.mPhase;
    currentIteration = computedTiming.mCurrentIteration;
    if (currentPhase == mPreviousPhase &&
        currentIteration == mPreviousIteration) {
      return;
    }
    intervalStartTime =
      std::max(std::min(StickyTimeDuration(-mEffect->SpecifiedTiming().mDelay),
                        computedTiming.mActiveDuration),
               zeroDuration);
    intervalEndTime =
      std::max(std::min((EffectEnd() - mEffect->SpecifiedTiming().mDelay),
                        computedTiming.mActiveDuration),
               zeroDuration);

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

  AutoTArray<AnimationEventParams, 2> events;

  // Handle cancel event first
  if ((mPreviousPhase != AnimationPhase::Idle &&
       mPreviousPhase != AnimationPhase::After) &&
      currentPhase == AnimationPhase::Idle) {
    TimeStamp activeTimeStamp = ElapsedTimeToTimeStamp(aActiveTime);
    events.AppendElement(AnimationEventParams{ eAnimationCancel,
                                               aActiveTime,
                                               activeTimeStamp });
  }

  switch (mPreviousPhase) {
    case AnimationPhase::Idle:
    case AnimationPhase::Before:
      if (currentPhase == AnimationPhase::Active) {
        events.AppendElement(AnimationEventParams{ eAnimationStart,
                                                   intervalStartTime,
                                                   startTimeStamp });
      } else if (currentPhase == AnimationPhase::After) {
        events.AppendElement(AnimationEventParams{ eAnimationStart,
                                                   intervalStartTime,
                                                   startTimeStamp });
        events.AppendElement(AnimationEventParams{ eAnimationEnd,
                                                   intervalEndTime,
                                                   endTimeStamp });
      }
      break;
    case AnimationPhase::Active:
      if (currentPhase == AnimationPhase::Before) {
        events.AppendElement(AnimationEventParams{ eAnimationEnd,
                                                   intervalStartTime,
                                                   startTimeStamp });
      } else if (currentPhase == AnimationPhase::Active) {
        // The currentIteration must have changed or element we would have
        // returned early above.
        MOZ_ASSERT(currentIteration != mPreviousIteration);
        events.AppendElement(AnimationEventParams{ eAnimationIteration,
                                                   iterationStartTime,
                                                   iterationTimeStamp });
      } else if (currentPhase == AnimationPhase::After) {
        events.AppendElement(AnimationEventParams{ eAnimationEnd,
                                                   intervalEndTime,
                                                   endTimeStamp });
      }
      break;
    case AnimationPhase::After:
      if (currentPhase == AnimationPhase::Before) {
        events.AppendElement(AnimationEventParams{ eAnimationStart,
                                                   intervalEndTime,
                                                   startTimeStamp});
        events.AppendElement(AnimationEventParams{ eAnimationEnd,
                                                   intervalStartTime,
                                                   endTimeStamp });
      } else if (currentPhase == AnimationPhase::Active) {
        events.AppendElement(AnimationEventParams{ eAnimationStart,
                                                   intervalEndTime,
                                                   endTimeStamp });
      }
      break;
  }
  mPreviousPhase = currentPhase;
  mPreviousIteration = currentIteration;

  for (const AnimationEventParams& event : events){
    manager->QueueEvent(
               AnimationEventInfo(owningElement, owningPseudoType,
                                  event.mMessage, mAnimationName,
                                  event.mElapsedTime, event.mTimeStamp,
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

class MOZ_STACK_CLASS ServoCSSAnimationBuilder final {
public:
  explicit ServoCSSAnimationBuilder(const ServoComputedValues* aComputedValues)
    : mComputedValues(aComputedValues)
  {
    MOZ_ASSERT(aComputedValues);
  }

  bool BuildKeyframes(nsPresContext* aPresContext,
                      const StyleAnimation& aSrc,
                      nsTArray<Keyframe>& aKeyframes)
  {
    ServoStyleSet* styleSet = aPresContext->StyleSet()->AsServo();
    MOZ_ASSERT(styleSet);
    const nsTimingFunction& timingFunction = aSrc.GetTimingFunction();
    return styleSet->GetKeyframesForName(aSrc.GetName(),
                                         timingFunction,
                                         mComputedValues,
                                         aKeyframes);
  }
  void SetKeyframes(KeyframeEffectReadOnly& aEffect,
                    nsTArray<Keyframe>&& aKeyframes)
  {
    aEffect.SetKeyframes(Move(aKeyframes), mComputedValues);
  }

private:
  const ServoComputedValues* mComputedValues;
};

class MOZ_STACK_CLASS GeckoCSSAnimationBuilder final {
public:
  GeckoCSSAnimationBuilder(nsStyleContext* aStyleContext,
                           const NonOwningAnimationTarget& aTarget)
    : mStyleContext(aStyleContext)
    , mTarget(aTarget)
  {
    MOZ_ASSERT(aStyleContext);
    MOZ_ASSERT(aTarget.mElement);
  }

  bool BuildKeyframes(nsPresContext* aPresContext,
                      const StyleAnimation& aSrc,
                      nsTArray<Keyframe>& aKeyframs);
  void SetKeyframes(KeyframeEffectReadOnly& aEffect,
                    nsTArray<Keyframe>&& aKeyframes)
  {
    aEffect.SetKeyframes(Move(aKeyframes), mStyleContext);
  }

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
    nsCSSPropertyIDSet aAnimatedProperties,
    nsCSSPropertyIDSet aPropertiesSetAtStart,
    nsCSSPropertyIDSet aPropertiesSetAtEnd,
    const Maybe<ComputedTimingFunction>& aInheritedTimingFunction,
    nsTArray<Keyframe>& aKeyframes);

  RefPtr<nsStyleContext> mStyleContext;
  NonOwningAnimationTarget mTarget;

  ResolvedStyleCache mResolvedStyles;
};

static Maybe<ComputedTimingFunction>
ConvertTimingFunction(const nsTimingFunction& aTimingFunction);

template<class BuilderType>
static void
UpdateOldAnimationPropertiesWithNew(
    CSSAnimation& aOld,
    TimingParams& aNewTiming,
    nsTArray<Keyframe>&& aNewKeyframes,
    bool aNewIsStylePaused,
    BuilderType& aBuilder)
{
  bool animationChanged = false;

  // Update the old from the new so we can keep the original object
  // identity (and any expando properties attached to it).
  if (aOld.GetEffect()) {
    dom::AnimationEffectReadOnly* oldEffect = aOld.GetEffect();
    animationChanged = oldEffect->SpecifiedTiming() != aNewTiming;
    oldEffect->SetSpecifiedTiming(aNewTiming);

    KeyframeEffectReadOnly* oldKeyframeEffect = oldEffect->AsKeyframeEffect();
    if (oldKeyframeEffect) {
      aBuilder.SetKeyframes(*oldKeyframeEffect, Move(aNewKeyframes));
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
template<class BuilderType>
static already_AddRefed<CSSAnimation>
BuildAnimation(nsPresContext* aPresContext,
               const NonOwningAnimationTarget& aTarget,
               const StyleAnimation& aSrc,
               BuilderType& aBuilder,
               nsAnimationManager::CSSAnimationCollection* aCollection)
{
  MOZ_ASSERT(aPresContext);

  nsTArray<Keyframe> keyframes;
  if (!aBuilder.BuildKeyframes(aPresContext, aSrc, keyframes)) {
    return nullptr;
  }

  TimingParams timing = TimingParamsFromCSSParams(aSrc.GetDuration(),
                                                  aSrc.GetDelay(),
                                                  aSrc.GetIterationCount(),
                                                  aSrc.GetDirection(),
                                                  aSrc.GetFillMode());

  bool isStylePaused =
    aSrc.GetPlayState() == NS_STYLE_ANIMATION_PLAY_STATE_PAUSED;

  // Find the matching animation with animation name in the old list
  // of animations and remove the matched animation from the list.
  RefPtr<CSSAnimation> oldAnim =
    PopExistingAnimation(aSrc.GetName(), aCollection);

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
                                        Move(keyframes),
                                        isStylePaused,
                                        aBuilder);
    return oldAnim.forget();
  }

  // mTarget is non-null here, so we emplace it directly.
  Maybe<OwningAnimationTarget> target;
  target.emplace(aTarget.mElement, aTarget.mPseudoType);
  KeyframeEffectParams effectOptions;
  RefPtr<KeyframeEffectReadOnly> effect =
    new KeyframeEffectReadOnly(aPresContext->Document(), target, timing,
                               effectOptions);

  aBuilder.SetKeyframes(*effect, Move(keyframes));

  RefPtr<CSSAnimation> animation =
    new CSSAnimation(aPresContext->Document()->GetScopeObject(),
                     aSrc.GetName());
  animation->SetOwningElement(
    OwningElementRef(*aTarget.mElement, aTarget.mPseudoType));

  animation->SetTimelineNoUpdate(aTarget.mElement->OwnerDoc()->Timeline());
  animation->SetEffectNoUpdate(effect);

  if (isStylePaused) {
    animation->PauseFromStyle();
  } else {
    animation->PlayFromStyle();
  }

  return animation.forget();
}

bool
GeckoCSSAnimationBuilder::BuildKeyframes(nsPresContext* aPresContext,
                                         const StyleAnimation& aSrc,
                                         nsTArray<Keyframe>& aKeyframes)
{
  MOZ_ASSERT(aPresContext);
  MOZ_ASSERT(aPresContext->StyleSet()->IsGecko());

  nsCSSKeyframesRule* rule =
    aPresContext->StyleSet()->AsGecko()->KeyframesRuleForName(aSrc.GetName());
  if (!rule) {
    return false;
  }

  aKeyframes = BuildAnimationFrames(aPresContext, aSrc, rule);

  return true;
}

nsTArray<Keyframe>
GeckoCSSAnimationBuilder::BuildAnimationFrames(nsPresContext* aPresContext,
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
    // Bug 1293490: We should also match composite modes here.
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
  FillInMissingKeyframeValues(animatedProperties, propertiesSetAtStart,
                              propertiesSetAtEnd, inheritedTimingFunction,
                              keyframes);

  return keyframes;
}

Maybe<ComputedTimingFunction>
GeckoCSSAnimationBuilder::GetKeyframeTimingFunction(
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
GeckoCSSAnimationBuilder::GetKeyframePropertyValues(
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

    StyleAnimationValue computedValue;
    if (!StyleAnimationValue::ExtractComputedValue(prop, styleContext,
                                                   computedValue)) {
      continue;
    }

    nsCSSValue propertyValue;
    DebugOnly<bool> uncomputeResult =
      StyleAnimationValue::UncomputeValue(prop, Move(computedValue),
                                          propertyValue);
    MOZ_ASSERT(uncomputeResult,
               "Unable to get specified value from computed value");
    MOZ_ASSERT(propertyValue.GetUnit() != eCSSUnit_Null,
               "Not expecting to read invalid properties");

    result.AppendElement(Move(PropertyValuePair(prop, Move(propertyValue))));
    aAnimatedProperties.AddProperty(prop);
  }

  return result;
}

void
GeckoCSSAnimationBuilder::FillInMissingKeyframeValues(
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
      !nsAnimationManager::FindMatchingKeyframe(aKeyframes,
                                                0.0,
                                                aInheritedTimingFunction,
                                                startKeyframeIndex)) {
    Keyframe newKeyframe;
    newKeyframe.mOffset.emplace(0.0);
    newKeyframe.mTimingFunction = aInheritedTimingFunction;
    aKeyframes.InsertElementAt(startKeyframeIndex, Move(newKeyframe));
  }

  // Find/create the keyframe to add end values to
  size_t endKeyframeIndex = kNotSet;
  if (!aAnimatedProperties.Equals(aPropertiesSetAtEnd)) {
    if (!nsAnimationManager::FindMatchingKeyframe(Reversed(aKeyframes),
                                                  1.0,
                                                  aInheritedTimingFunction,
                                                  endKeyframeIndex)) {
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
      // An uninitialized nsCSSValue represents the underlying value.
      PropertyValuePair propertyValue(prop, Move(nsCSSValue()));
      startKeyframe->mPropertyValues.AppendElement(Move(propertyValue));
    }
    if (endKeyframe && !aPropertiesSetAtEnd.HasProperty(prop)) {
      // An uninitialized nsCSSValue represents the underlying value.
      PropertyValuePair propertyValue(prop, Move(nsCSSValue()));
      endKeyframe->mPropertyValues.AppendElement(Move(propertyValue));
    }
  }
}

template<class BuilderType>
static nsAnimationManager::OwningCSSAnimationPtrArray
BuildAnimations(nsPresContext* aPresContext,
                const NonOwningAnimationTarget& aTarget,
                const nsStyleAutoArray<StyleAnimation>& aStyleAnimations,
                uint32_t aStyleAnimationNameCount,
                BuilderType& aBuilder,
                nsAnimationManager::CSSAnimationCollection* aCollection)
{
  nsAnimationManager::OwningCSSAnimationPtrArray result;

  for (size_t animIdx = aStyleAnimationNameCount; animIdx-- != 0;) {
    const StyleAnimation& src = aStyleAnimations[animIdx];

    // CSS Animations whose animation-name does not match a @keyframes rule do
    // not generate animation events. This includes when the animation-name is
    // "none" which is represented by an empty name in the StyleAnimation.
    // Since such animations neither affect style nor dispatch events, we do
    // not generate a corresponding CSSAnimation for them.
    if (src.GetName().IsEmpty()) {
      continue;
    }

    RefPtr<CSSAnimation> dest = BuildAnimation(aPresContext,
                                               aTarget,
                                               src,
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
nsAnimationManager::UpdateAnimations(nsStyleContext* aStyleContext,
                                     mozilla::dom::Element* aElement)
{
  MOZ_ASSERT(mPresContext->IsDynamic(),
             "Should not update animations for print or print preview");
  MOZ_ASSERT(aElement->IsInComposedDoc(),
             "Should not update animations that are not attached to the "
             "document tree");

  if (aStyleContext->IsInDisplayNoneSubtree()) {
    StopAnimationsForElement(aElement, aStyleContext->GetPseudoType());
    return;
  }

  NonOwningAnimationTarget target(aElement, aStyleContext->GetPseudoType());
  GeckoCSSAnimationBuilder builder(aStyleContext, target);

  const nsStyleDisplay* disp = aStyleContext->StyleDisplay();
  DoUpdateAnimations(target, *disp, builder);
}

void
nsAnimationManager::UpdateAnimations(
  dom::Element* aElement,
  CSSPseudoElementType aPseudoType,
  const ServoComputedValues* aComputedValues)
{
  MOZ_ASSERT(mPresContext->IsDynamic(),
             "Should not update animations for print or print preview");
  MOZ_ASSERT(aElement->IsInComposedDoc(),
             "Should not update animations that are not attached to the "
             "document tree");

  if (!aComputedValues) {
    // If we are in a display:none subtree we will have no computed values.
    // Since CSS animations should not run in display:none subtrees we should
    // stop (actually, destroy) any animations on this element here.
    StopAnimationsForElement(aElement, aPseudoType);
    return;
  }

  NonOwningAnimationTarget target(aElement, aPseudoType);
  ServoCSSAnimationBuilder builder(aComputedValues);

  const nsStyleDisplay *disp =
    Servo_GetStyleDisplay(aComputedValues);
  DoUpdateAnimations(target, *disp, builder);
}

template<class BuilderType>
void
nsAnimationManager::DoUpdateAnimations(
  const NonOwningAnimationTarget& aTarget,
  const nsStyleDisplay& aStyleDisplay,
  BuilderType& aBuilder)
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
      aStyleDisplay.mAnimations[0].GetName().IsEmpty()) {
    return;
  }

  nsAutoAnimationMutationBatch mb(aTarget.mElement->OwnerDoc());

  // Build the updated animations list, extracting matching animations from
  // the existing collection as we go.
  OwningCSSAnimationPtrArray newAnimations;
  newAnimations = BuildAnimations(mPresContext,
                                  aTarget,
                                  aStyleDisplay.mAnimations,
                                  aStyleDisplay.mAnimationNameCount,
                                  aBuilder,
                                  collection);

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
    newAnimations[newAnimIdx]->CancelFromStyle();
  }
}
