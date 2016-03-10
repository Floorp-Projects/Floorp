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
#include "mozilla/dom/KeyframeEffect.h"

#include "nsPresContext.h"
#include "nsStyleSet.h"
#include "nsStyleChangeList.h"
#include "nsCSSRules.h"
#include "mozilla/RestyleManager.h"
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

  MOZ_ASSERT(!skippedActivePhase || (!isActive && !wasActive),
             "skippedActivePhase only makes sense if we were & are inactive");

  if (computedTiming.mPhase == ComputedTiming::AnimationPhase::Before) {
    mPreviousPhaseOrIteration = PREVIOUS_PHASE_BEFORE;
  } else if (computedTiming.mPhase == ComputedTiming::AnimationPhase::Active) {
    mPreviousPhaseOrIteration = computedTiming.mCurrentIteration;
  } else if (computedTiming.mPhase == ComputedTiming::AnimationPhase::After) {
    mPreviousPhaseOrIteration = PREVIOUS_PHASE_AFTER;
  }

  EventMessage message;

  if (!wasActive && isActive) {
    message = eAnimationStart;
  } else if (wasActive && !isActive) {
    message = eAnimationEnd;
  } else if (wasActive && isActive && !isSameIteration) {
    message = eAnimationIteration;
  } else if (skippedActivePhase) {
    // First notifying for start of 0th iteration by appending an
    // 'animationstart':
    StickyTimeDuration elapsedTime =
      std::min(StickyTimeDuration(InitialAdvance()),
               computedTiming.mActiveDuration);
    manager->QueueEvent(AnimationEventInfo(owningElement, owningPseudoType,
                                           eAnimationStart, mAnimationName,
                                           elapsedTime,
                                           ElapsedTimeToTimeStamp(elapsedTime),
                                           this));
    // Then have the shared code below append an 'animationend':
    message = eAnimationEnd;
  } else {
    return; // No events need to be sent
  }

  StickyTimeDuration elapsedTime;

  if (message == eAnimationStart || message == eAnimationIteration) {
    StickyTimeDuration iterationStart = computedTiming.mDuration *
                                          computedTiming.mCurrentIteration;
    elapsedTime = std::max(iterationStart, StickyTimeDuration(InitialAdvance()));
  } else {
    MOZ_ASSERT(message == eAnimationEnd);
    elapsedTime = computedTiming.mActiveDuration;
  }

  manager->QueueEvent(AnimationEventInfo(owningElement, owningPseudoType,
                                         message, mAnimationName, elapsedTime,
                                         ElapsedTimeToTimeStamp(elapsedTime),
                                         this));
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
  // Initializes to null. We always return this object to benefit from
  // return-value-optimization.
  TimeStamp result;

  // Currently we may dispatch animationstart events before resolving
  // mStartTime if we have a delay <= 0. This will change in bug 1134163
  // but until then we should just use the latest refresh driver time as
  // the event timestamp in that case.
  if (!mEffect || mStartTime.IsNull()) {
    nsPresContext* presContext = GetPresContext();
    if (presContext) {
      result = presContext->RefreshDriver()->MostRecentRefresh();
    }
    return result;
  }

  result = AnimationTimeToTimeStamp(aElapsedTime +
                                    mEffect->SpecifiedTiming().mDelay);
  return result;
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
    InfallibleTArray<AnimationProperty>& aNewProperties,
    bool aNewIsStylePaused)
{
  bool animationChanged = false;

  // Update the old from the new so we can keep the original object
  // identity (and any expando properties attached to it).
  if (aOld.GetEffect()) {
    KeyframeEffectReadOnly* oldEffect = aOld.GetEffect();
    animationChanged =
      oldEffect->SpecifiedTiming() != aNewTiming;
    oldEffect->SetSpecifiedTiming(aNewTiming);
    animationChanged |=
      oldEffect->UpdateProperties(aNewProperties);
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

  if (collection) {
    EffectSet* effectSet =
      EffectSet::GetEffectSet(aElement, aStyleContext->GetPseudoType());
    if (effectSet) {
      effectSet->UpdateAnimationGeneration(mPresContext);
    }
  } else {
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

  EffectCompositor::UpdateCascadeResults(aElement,
                                         aStyleContext->GetPseudoType(),
                                         aStyleContext);

  mPresContext->EffectCompositor()->
    MaybeUpdateAnimationRule(aElement,
                             aStyleContext->GetPseudoType(),
                             EffectCompositor::CascadeLevel::Animations);

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
  void BuildAnimationProperties(nsPresContext* aPresContext,
                                const StyleAnimation& aSrc,
                                const nsCSSKeyframesRule* aRule,
                                InfallibleTArray<AnimationProperty>& aResult);
  bool BuildSegment(InfallibleTArray<mozilla::AnimationPropertySegment>&
                      aSegments,
                    nsCSSProperty aProperty,
                    const mozilla::StyleAnimation& aAnimation,
                    float aFromKey, nsStyleContext* aFromContext,
                    mozilla::css::Declaration* aFromDeclaration,
                    float aToKey, nsStyleContext* aToContext);

  static TimingParams TimingParamsFrom(
    const StyleAnimation& aStyleAnimation)
  {
    TimingParams timing;

    timing.mDuration.emplace(StickyTimeDuration::FromMilliseconds(
			       aStyleAnimation.GetDuration()));
    timing.mDelay = TimeDuration::FromMilliseconds(aStyleAnimation.GetDelay());
    timing.mIterations = aStyleAnimation.GetIterationCount();
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

already_AddRefed<CSSAnimation>
CSSAnimationBuilder::Build(nsPresContext* aPresContext,
                           const StyleAnimation& aSrc,
                           const nsCSSKeyframesRule* aRule)
{
  MOZ_ASSERT(aPresContext);
  MOZ_ASSERT(aRule);

  TimingParams timing = TimingParamsFrom(aSrc);

  InfallibleTArray<AnimationProperty> animationProperties;
  BuildAnimationProperties(aPresContext, aSrc, aRule, animationProperties);

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
                                        animationProperties,
                                        isStylePaused);
    return oldAnim.forget();
  }

  RefPtr<KeyframeEffectReadOnly> effect =
    new KeyframeEffectReadOnly(aPresContext->Document(), mTarget,
                               mStyleContext->GetPseudoType(), timing);

  effect->Properties() = Move(animationProperties);

  RefPtr<CSSAnimation> animation =
    new CSSAnimation(aPresContext->Document()->GetScopeObject(),
                     aSrc.GetName());
  animation->SetOwningElement(
    OwningElementRef(*mTarget, mStyleContext->GetPseudoType()));

  animation->SetTimeline(mTimeline);
  animation->SetEffect(effect);

  if (isStylePaused) {
    animation->PauseFromStyle();
  } else {
    animation->PlayFromStyle();
  }
  // FIXME: Bug 1134163 - We shouldn't queue animationstart events
  // until the animation is actually ready to run. However, we
  // currently have some tests that assume that these events are
  // dispatched within the same tick as the animation is added
  // so we need to queue up any animationstart events from newly-created
  // animations.
  animation->QueueEvents();

  return animation.forget();
}

void
CSSAnimationBuilder::BuildAnimationProperties(
  nsPresContext* aPresContext,
  const StyleAnimation& aSrc,
  const nsCSSKeyframesRule* aRule,
  InfallibleTArray<AnimationProperty>& aResult)
{
  // While current drafts of css3-animations say that later keyframes
  // with the same key entirely replace earlier ones (no cascading),
  // this is a bad idea and contradictory to the rest of CSS.  So
  // we're going to keep all the keyframes for each key and then do
  // the replacement on a per-property basis rather than a per-rule
  // basis, just like everything else in CSS.

  AutoTArray<KeyframeData, 16> sortedKeyframes;

  for (uint32_t ruleIdx = 0, ruleEnd = aRule->StyleRuleCount();
       ruleIdx != ruleEnd; ++ruleIdx) {
    css::Rule* cssRule = aRule->GetStyleRuleAt(ruleIdx);
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
    return;
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
    AutoTArray<uint32_t, 16> keyframesWithProperty;
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

    AnimationProperty &propData = *aResult.AppendElement();
    propData.mProperty = prop;

    KeyframeData *fromKeyframe = nullptr;
    RefPtr<nsStyleContext> fromContext;
    bool interpolated = true;
    for (uint32_t wpIdx = 0, wpEnd = keyframesWithProperty.Length();
         wpIdx != wpEnd; ++wpIdx) {
      uint32_t kfIdx = keyframesWithProperty[wpIdx];
      KeyframeData &toKeyframe = sortedKeyframes[kfIdx];

      RefPtr<nsStyleContext> toContext =
        mResolvedStyles.Get(aPresContext, mStyleContext,
                            toKeyframe.mRule->Declaration());

      if (fromKeyframe) {
        interpolated = interpolated &&
          BuildSegment(propData.mSegments, prop, aSrc,
                       fromKeyframe->mKey, fromContext,
                       fromKeyframe->mRule->Declaration(),
                       toKeyframe.mKey, toContext);
      } else {
        if (toKeyframe.mKey != 0.0f) {
          // There's no data for this property at 0%, so use the
          // cascaded value above us.
          if (!mStyleWithoutAnimation) {
            MOZ_ASSERT(aPresContext->StyleSet()->IsGecko(),
                       "ServoStyleSet should not use nsAnimationManager for "
                       "animations");
            mStyleWithoutAnimation = aPresContext->StyleSet()->AsGecko()->
              ResolveStyleWithoutAnimation(mTarget, mStyleContext,
                                           eRestyle_AllHintsWithAnimations);
          }
          interpolated = interpolated &&
            BuildSegment(propData.mSegments, prop, aSrc,
                         0.0f, mStyleWithoutAnimation, nullptr,
                         toKeyframe.mKey, toContext);
        }
      }

      fromContext = toContext;
      fromKeyframe = &toKeyframe;
    }

    if (fromKeyframe->mKey != 1.0f) {
      // There's no data for this property at 100%, so use the
      // cascaded value above us.
      if (!mStyleWithoutAnimation) {
        MOZ_ASSERT(aPresContext->StyleSet()->IsGecko(),
                   "ServoStyleSet should not use nsAnimationManager for "
                   "animations");
        mStyleWithoutAnimation = aPresContext->StyleSet()->AsGecko()->
          ResolveStyleWithoutAnimation(mTarget, mStyleContext,
                                       eRestyle_AllHintsWithAnimations);
      }
      interpolated = interpolated &&
        BuildSegment(propData.mSegments, prop, aSrc,
                     fromKeyframe->mKey, fromContext,
                     fromKeyframe->mRule->Declaration(),
                     1.0f, mStyleWithoutAnimation);
    }

    // If we failed to build any segments due to inability to
    // interpolate, remove the property from the animation.  (It's not
    // clear if this is the right thing to do -- we could run some of
    // the segments, but it's really not clear whether we should skip
    // values (which?) or skip segments, so best to skip the whole
    // thing for now.)
    if (!interpolated) {
      aResult.RemoveElementAt(aResult.Length() - 1);
    }
  }
}

bool
CSSAnimationBuilder::BuildSegment(InfallibleTArray<AnimationPropertySegment>&
                                   aSegments,
                                  nsCSSProperty aProperty,
                                  const StyleAnimation& aAnimation,
                                  float aFromKey, nsStyleContext* aFromContext,
                                  mozilla::css::Declaration* aFromDeclaration,
                                  float aToKey, nsStyleContext* aToContext)
{
  StyleAnimationValue fromValue, toValue, dummyValue;
  if (!CommonAnimationManager<CSSAnimation>::ExtractComputedValueForTransition(
        aProperty, aFromContext, fromValue) ||
      !CommonAnimationManager<CSSAnimation>::ExtractComputedValueForTransition(
        aProperty, aToContext, toValue) ||
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
  if (tf->mType != nsTimingFunction::Type::Linear) {
    ComputedTimingFunction computedTimingFunction;
    computedTimingFunction.Init(*tf);
    segment.mTimingFunction = Some(computedTimingFunction);
  }

  return true;
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

