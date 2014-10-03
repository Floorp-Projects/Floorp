/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAnimationManager.h"
#include "nsTransitionManager.h"

#include "mozilla/EventDispatcher.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/StyleAnimationValue.h"
#include "mozilla/dom/AnimationPlayer.h"

#include "nsPresContext.h"
#include "nsRuleProcessorData.h"
#include "nsStyleSet.h"
#include "nsStyleChangeList.h"
#include "nsCSSRules.h"
#include "RestyleManager.h"
#include "nsLayoutUtils.h"
#include "nsIFrame.h"
#include "nsIDocument.h"
#include <math.h>

using namespace mozilla;
using namespace mozilla::css;
using mozilla::dom::Animation;
using mozilla::dom::AnimationPlayer;

void
nsAnimationManager::UpdateStyleAndEvents(AnimationPlayerCollection*
                                           aCollection,
                                         TimeStamp aRefreshTime,
                                         EnsureStyleRuleFlags aFlags)
{
  aCollection->EnsureStyleRuleFor(aRefreshTime, aFlags);
  GetEventsForCurrentTime(aCollection, mPendingEvents);
  CheckNeedsRefresh();
}

void
nsAnimationManager::GetEventsForCurrentTime(AnimationPlayerCollection*
                                              aCollection,
                                            EventArray& aEventsToDispatch)
{
  for (size_t playerIdx = aCollection->mPlayers.Length(); playerIdx-- != 0; ) {
    AnimationPlayer* player = aCollection->mPlayers[playerIdx];
    Animation* anim = player->GetSource();
    if (!anim) {
      continue;
    }

    ComputedTiming computedTiming = anim->GetComputedTiming();

    switch (computedTiming.mPhase) {
      case ComputedTiming::AnimationPhase_Null:
      case ComputedTiming::AnimationPhase_Before:
        // Do nothing
        break;

      case ComputedTiming::AnimationPhase_Active:
        // Dispatch 'animationstart' or 'animationiteration' when needed.
        if (computedTiming.mCurrentIteration != anim->LastNotification()) {
          // Notify 'animationstart' even if a negative delay puts us
          // past the first iteration.
          // Note that when somebody changes the animation-duration
          // dynamically, this will fire an extra iteration event
          // immediately in many cases.  It's not clear to me if that's the
          // right thing to do.
          uint32_t message =
            anim->LastNotification() == Animation::LAST_NOTIFICATION_NONE
                                        ? NS_ANIMATION_START
                                        : NS_ANIMATION_ITERATION;
          anim->SetLastNotification(computedTiming.mCurrentIteration);
          TimeDuration iterationStart =
            anim->Timing().mIterationDuration *
            computedTiming.mCurrentIteration;
          TimeDuration elapsedTime =
            std::max(iterationStart, anim->InitialAdvance());
          AnimationEventInfo ei(aCollection->mElement, player->Name(), message,
                                StickyTimeDuration(elapsedTime),
                                aCollection->PseudoElement());
          aEventsToDispatch.AppendElement(ei);
        }
        break;

      case ComputedTiming::AnimationPhase_After:
        // If we skipped the animation interval entirely, dispatch
        // 'animationstart' first
        if (anim->LastNotification() == Animation::LAST_NOTIFICATION_NONE) {
          // Notifying for start of 0th iteration.
          // (This is overwritten below but we set it here to maintain
          // internal consistency.)
          anim->SetLastNotification(0);
          StickyTimeDuration elapsedTime =
            std::min(StickyTimeDuration(anim->InitialAdvance()),
                     computedTiming.mActiveDuration);
          AnimationEventInfo ei(aCollection->mElement,
                                player->Name(), NS_ANIMATION_START,
                                elapsedTime, aCollection->PseudoElement());
          aEventsToDispatch.AppendElement(ei);
        }
        // Dispatch 'animationend' when needed.
        if (anim->LastNotification() != Animation::LAST_NOTIFICATION_END) {
          anim->SetLastNotification(Animation::LAST_NOTIFICATION_END);
          AnimationEventInfo ei(aCollection->mElement,
                                player->Name(), NS_ANIMATION_END,
                                computedTiming.mActiveDuration,
                                aCollection->PseudoElement());
          aEventsToDispatch.AppendElement(ei);
        }
        break;
    }
  }
}

AnimationPlayerCollection*
nsAnimationManager::GetAnimationPlayers(dom::Element *aElement,
                                        nsCSSPseudoElements::Type aPseudoType,
                                        bool aCreateIfNeeded)
{
  if (!aCreateIfNeeded && PR_CLIST_IS_EMPTY(&mElementCollections)) {
    // Early return for the most common case.
    return nullptr;
  }

  nsIAtom *propName;
  if (aPseudoType == nsCSSPseudoElements::ePseudo_NotPseudoElement) {
    propName = nsGkAtoms::animationsProperty;
  } else if (aPseudoType == nsCSSPseudoElements::ePseudo_before) {
    propName = nsGkAtoms::animationsOfBeforeProperty;
  } else if (aPseudoType == nsCSSPseudoElements::ePseudo_after) {
    propName = nsGkAtoms::animationsOfAfterProperty;
  } else {
    NS_ASSERTION(!aCreateIfNeeded,
                 "should never try to create transitions for pseudo "
                 "other than :before or :after");
    return nullptr;
  }
  AnimationPlayerCollection* collection =
    static_cast<AnimationPlayerCollection*>(aElement->GetProperty(propName));
  if (!collection && aCreateIfNeeded) {
    // FIXME: Consider arena-allocating?
    collection =
      new AnimationPlayerCollection(aElement, propName, this,
        mPresContext->RefreshDriver()->MostRecentRefresh());
    nsresult rv =
      aElement->SetProperty(propName, collection,
                            &AnimationPlayerCollection::PropertyDtor, false);
    if (NS_FAILED(rv)) {
      NS_WARNING("SetProperty failed");
      delete collection;
      return nullptr;
    }
    if (propName == nsGkAtoms::animationsProperty) {
      aElement->SetMayHaveAnimations();
    }

    AddElementCollection(collection);
  }

  return collection;
}

/* virtual */ void
nsAnimationManager::RulesMatching(ElementRuleProcessorData* aData)
{
  NS_ABORT_IF_FALSE(aData->mPresContext == mPresContext,
                    "pres context mismatch");
  nsIStyleRule *rule =
    GetAnimationRule(aData->mElement,
                     nsCSSPseudoElements::ePseudo_NotPseudoElement);
  if (rule) {
    aData->mRuleWalker->Forward(rule);
  }
}

/* virtual */ void
nsAnimationManager::RulesMatching(PseudoElementRuleProcessorData* aData)
{
  NS_ABORT_IF_FALSE(aData->mPresContext == mPresContext,
                    "pres context mismatch");
  if (aData->mPseudoType != nsCSSPseudoElements::ePseudo_before &&
      aData->mPseudoType != nsCSSPseudoElements::ePseudo_after) {
    return;
  }

  // FIXME: Do we really want to be the only thing keeping a
  // pseudo-element alive?  I *think* the non-animation restyle should
  // handle that, but should add a test.
  nsIStyleRule *rule = GetAnimationRule(aData->mElement, aData->mPseudoType);
  if (rule) {
    aData->mRuleWalker->Forward(rule);
  }
}

/* virtual */ void
nsAnimationManager::RulesMatching(AnonBoxRuleProcessorData* aData)
{
}

#ifdef MOZ_XUL
/* virtual */ void
nsAnimationManager::RulesMatching(XULTreeRuleProcessorData* aData)
{
}
#endif

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
  // FIXME (bug 960465): This test should go away.
  if (!mPresContext->RestyleManager()->IsProcessingAnimationStyleChange()) {
    if (!mPresContext->IsDynamic()) {
      // For print or print preview, ignore animations.
      return nullptr;
    }

    // Everything that causes our animation data to change triggers a
    // style change, which in turn triggers a non-animation restyle.
    // Likewise, when we initially construct frames, we're not in a
    // style change, but also not in an animation restyle.

    const nsStyleDisplay* disp = aStyleContext->StyleDisplay();
    AnimationPlayerCollection* collection =
      GetAnimationPlayers(aElement, aStyleContext->GetPseudoType(), false);
    if (!collection &&
        disp->mAnimationNameCount == 1 &&
        disp->mAnimations[0].GetName().IsEmpty()) {
      return nullptr;
    }

    // build the animations list
    dom::AnimationTimeline* timeline = aElement->OwnerDoc()->Timeline();
    AnimationPlayerPtrArray newPlayers;
    BuildAnimations(aStyleContext, aElement, timeline, newPlayers);

    if (newPlayers.IsEmpty()) {
      if (collection) {
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
      if (!collection->mPlayers.IsEmpty()) {

        Nullable<TimeDuration> now = timeline->GetCurrentTimeDuration();

        for (size_t newIdx = newPlayers.Length(); newIdx-- != 0;) {
          AnimationPlayer* newPlayer = newPlayers[newIdx];

          // Find the matching animation with this name in the old list
          // of animations.  We iterate through both lists in a backwards
          // direction which means that if there are more animations in
          // the new list of animations with a given name than in the old
          // list, it will be the animations towards the of the beginning of
          // the list that do not match and are treated as new animations.
          nsRefPtr<AnimationPlayer> oldPlayer;
          size_t oldIdx = collection->mPlayers.Length();
          while (oldIdx-- != 0) {
            AnimationPlayer* a = collection->mPlayers[oldIdx];
            if (a->Name() == newPlayer->Name()) {
              oldPlayer = a;
              break;
            }
          }
          if (!oldPlayer) {
            continue;
          }

          // Update the old from the new so we can keep the original object
          // identity (and any expando properties attached to it).
          if (oldPlayer->GetSource() && newPlayer->GetSource()) {
            Animation* oldAnim = oldPlayer->GetSource();
            Animation* newAnim = newPlayer->GetSource();
            oldAnim->Timing() = newAnim->Timing();
            oldAnim->Properties() = newAnim->Properties();
          }

          // Reset compositor state so animation will be re-synchronized.
          oldPlayer->mIsRunningOnCompositor = false;

          // Handle changes in play state.
          if (!oldPlayer->IsPaused() && newPlayer->IsPaused()) {
            // Start pause at current time.
            oldPlayer->mHoldTime = oldPlayer->GetCurrentTimeDuration();
          } else if (oldPlayer->IsPaused() && !newPlayer->IsPaused()) {
            if (now.IsNull()) {
              oldPlayer->mStartTime.SetNull();
            } else {
              oldPlayer->mStartTime.SetValue(now.Value() -
                                               oldPlayer->mHoldTime.Value());
            }
            oldPlayer->mHoldTime.SetNull();
          }
          oldPlayer->mPlayState = newPlayer->mPlayState;

          // Replace new animation with the (updated) old one and remove the
          // old one from the array so we don't try to match it any more.
          //
          // Although we're doing this while iterating this is safe because
          // we're not changing the length of newPlayers and we've finished
          // iterating over the list of old iterations.
          newPlayer = nullptr;
          newPlayers.ReplaceElementAt(newIdx, oldPlayer);
          collection->mPlayers.RemoveElementAt(oldIdx);
        }
      }
    } else {
      collection =
        GetAnimationPlayers(aElement, aStyleContext->GetPseudoType(), true);
    }
    collection->mPlayers.SwapElements(newPlayers);
    collection->mNeedsRefreshes = true;
    collection->Tick();

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
                                    dom::AnimationTimeline* aTimeline,
                                    AnimationPlayerPtrArray& aPlayers)
{
  NS_ABORT_IF_FALSE(aPlayers.IsEmpty(), "expect empty array");

  ResolvedStyleCache resolvedStyles;

  const nsStyleDisplay *disp = aStyleContext->StyleDisplay();
  Nullable<TimeDuration> now = aTimeline->GetCurrentTimeDuration();

  for (size_t animIdx = 0, animEnd = disp->mAnimationNameCount;
       animIdx != animEnd; ++animIdx) {
    const StyleAnimation& src = disp->mAnimations[animIdx];

    // CSS Animations whose animation-name does not match a @keyframes rule do
    // not generate animation events. This includes when the animation-name is
    // "none" which is represented by an empty name in the StyleAnimation.
    // Since such animations neither affect style nor dispatch events, we do
    // not generate a corresponding AnimationPlayer for them.
    nsCSSKeyframesRule* rule =
      src.GetName().IsEmpty()
      ? nullptr
      : mPresContext->StyleSet()->KeyframesRuleForName(mPresContext,
                                                       src.GetName());
    if (!rule) {
      continue;
    }

    nsRefPtr<AnimationPlayer> dest =
      *aPlayers.AppendElement(new AnimationPlayer(aTimeline));

    AnimationTiming timing;
    timing.mIterationDuration =
      TimeDuration::FromMilliseconds(src.GetDuration());
    timing.mDelay = TimeDuration::FromMilliseconds(src.GetDelay());
    timing.mIterationCount = src.GetIterationCount();
    timing.mDirection = src.GetDirection();
    timing.mFillMode = src.GetFillMode();

    nsRefPtr<Animation> destAnim =
      new Animation(mPresContext->Document(), aTarget,
                    aStyleContext->GetPseudoType(), timing, src.GetName());
    dest->SetSource(destAnim);

    dest->mStartTime = now;
    dest->mPlayState = src.GetPlayState();
    if (dest->IsPaused()) {
      dest->mHoldTime.SetValue(TimeDuration(0));
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
      NS_ABORT_IF_FALSE(cssRule, "must have rule");
      NS_ABORT_IF_FALSE(cssRule->GetType() == css::Rule::KEYFRAME_RULE,
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

      AnimationProperty &propData = *destAnim->Properties().AppendElement();
      propData.mProperty = prop;

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
            interpolated = interpolated &&
              BuildSegment(propData.mSegments, prop, src,
                           0.0f, aStyleContext, nullptr,
                           toKeyframe.mKey, toContext);
          }
        }

        fromContext = toContext;
        fromKeyframe = &toKeyframe;
      }

      if (fromKeyframe->mKey != 1.0f) {
        // There's no data for this property at 100%, so use the
        // cascaded value above us.
        interpolated = interpolated &&
          BuildSegment(propData.mSegments, prop, src,
                       fromKeyframe->mKey, fromContext,
                       fromKeyframe->mRule->Declaration(),
                       1.0f, aStyleContext);
      }

      // If we failed to build any segments due to inability to
      // interpolate, remove the property from the animation.  (It's not
      // clear if this is the right thing to do -- we could run some of
      // the segments, but it's really not clear whether we should skip
      // values (which?) or skip segments, so best to skip the whole
      // thing for now.)
      if (!interpolated) {
        destAnim->Properties().RemoveElementAt(
          destAnim->Properties().Length() - 1);
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

nsIStyleRule*
nsAnimationManager::GetAnimationRule(mozilla::dom::Element* aElement,
                                     nsCSSPseudoElements::Type aPseudoType)
{
  NS_ABORT_IF_FALSE(
    aPseudoType == nsCSSPseudoElements::ePseudo_NotPseudoElement ||
    aPseudoType == nsCSSPseudoElements::ePseudo_before ||
    aPseudoType == nsCSSPseudoElements::ePseudo_after,
    "forbidden pseudo type");

  if (!mPresContext->IsDynamic()) {
    // For print or print preview, ignore animations.
    return nullptr;
  }

  AnimationPlayerCollection* collection =
    GetAnimationPlayers(aElement, aPseudoType, false);
  if (!collection) {
    return nullptr;
  }

  RestyleManager* restyleManager = mPresContext->RestyleManager();
  if (restyleManager->SkipAnimationRules()) {
    // During the non-animation part of processing restyles, we don't
    // add the animation rule.

    if (collection->mStyleRule && restyleManager->PostAnimationRestyles()) {
      collection->PostRestyleForAnimation(mPresContext);
    }

    return nullptr;
  }

  NS_WARN_IF_FALSE(!collection->mNeedsRefreshes ||
                   collection->mStyleRuleRefreshTime ==
                     mPresContext->RefreshDriver()->MostRecentRefresh(),
                   "should already have refreshed style rule");

  return collection->mStyleRule;
}

/* virtual */ void
nsAnimationManager::WillRefresh(mozilla::TimeStamp aTime)
{
  NS_ABORT_IF_FALSE(mPresContext,
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
nsAnimationManager::AddElementCollection(
  AnimationPlayerCollection* aCollection)
{
  if (!mObservingRefreshDriver) {
    NS_ASSERTION(
      static_cast<AnimationPlayerCollection*>(aCollection)->mNeedsRefreshes,
      "Added data which doesn't need refreshing?");
    // We need to observe the refresh driver.
    mPresContext->RefreshDriver()->AddRefreshObserver(this, Flush_Style);
    mObservingRefreshDriver = true;
  }

  PR_INSERT_BEFORE(aCollection, &mElementCollections);
}

void
nsAnimationManager::CheckNeedsRefresh()
{
  for (PRCList *l = PR_LIST_HEAD(&mElementCollections);
       l != &mElementCollections;
       l = PR_NEXT_LINK(l)) {
    if (static_cast<AnimationPlayerCollection*>(l)->mNeedsRefreshes) {
      if (!mObservingRefreshDriver) {
        mPresContext->RefreshDriver()->AddRefreshObserver(this, Flush_Style);
        mObservingRefreshDriver = true;
      }
      return;
    }
  }
  if (mObservingRefreshDriver) {
    mObservingRefreshDriver = false;
    mPresContext->RefreshDriver()->RemoveRefreshObserver(this, Flush_Style);
  }
}

void
nsAnimationManager::FlushAnimations(FlushFlags aFlags)
{
  // FIXME: check that there's at least one style rule that's not
  // in its "done" state, and if there isn't, remove ourselves from
  // the refresh driver (but leave the animations!).
  TimeStamp now = mPresContext->RefreshDriver()->MostRecentRefresh();
  bool didThrottle = false;
  for (PRCList *l = PR_LIST_HEAD(&mElementCollections);
       l != &mElementCollections;
       l = PR_NEXT_LINK(l)) {
    AnimationPlayerCollection* collection =
      static_cast<AnimationPlayerCollection*>(l);
    collection->Tick();
    bool canThrottleTick = aFlags == Can_Throttle &&
      collection->CanPerformOnCompositorThread(
        AnimationPlayerCollection::CanAnimateFlags(0)) &&
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
