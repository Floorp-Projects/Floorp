/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAnimationManager.h"
#include "nsTransitionManager.h"

#include "mozilla/EventDispatcher.h"
#include "mozilla/MemoryReporting.h"

#include "nsPresContext.h"
#include "nsRuleProcessorData.h"
#include "nsStyleSet.h"
#include "nsStyleChangeList.h"
#include "nsCSSRules.h"
#include "RestyleManager.h"
#include "nsStyleAnimation.h"
#include "nsLayoutUtils.h"
#include "nsIFrame.h"
#include "nsIDocument.h"
#include "ActiveLayerTracker.h"
#include <math.h>

using namespace mozilla;
using namespace mozilla::css;

ElementAnimations::ElementAnimations(mozilla::dom::Element *aElement,
                                     nsIAtom *aElementProperty,
                                     nsAnimationManager *aAnimationManager,
                                     TimeStamp aNow)
  : CommonElementAnimationData(aElement, aElementProperty,
                               aAnimationManager, aNow),
    mNeedsRefreshes(true)
{
}

static void
ElementAnimationsPropertyDtor(void           *aObject,
                              nsIAtom        *aPropertyName,
                              void           *aPropertyValue,
                              void           *aData)
{
  ElementAnimations *ea = static_cast<ElementAnimations*>(aPropertyValue);
#ifdef DEBUG
  NS_ABORT_IF_FALSE(!ea->mCalledPropertyDtor, "can't call dtor twice");
  ea->mCalledPropertyDtor = true;
#endif
  delete ea;
}

void
ElementAnimations::EnsureStyleRuleFor(TimeStamp aRefreshTime,
                                      bool aIsThrottled)
{
  if (!mNeedsRefreshes) {
    mStyleRuleRefreshTime = aRefreshTime;
    return;
  }

  // If we're performing animations on the compositor thread, then we can skip
  // most of the work in this method. But even if we are throttled, then we
  // have to do the work if an animation is ending in order to get correct end
  // of animation behaviour (the styles of the animation disappear, or the fill
  // mode behaviour). This loop checks for any finishing animations and forces
  // the style recalculation if we find any.
  if (aIsThrottled) {
    for (uint32_t animIdx = mAnimations.Length(); animIdx-- != 0; ) {
      ElementAnimation* anim = mAnimations[animIdx];

      if (anim->mProperties.IsEmpty() ||
          anim->mTiming.mIterationDuration.ToMilliseconds() <= 0.0) {
        continue;
      }

      // The ElapsedDurationAt() call here handles pausing.  But:
      // FIXME: avoid recalculating every time when paused.
      TimeDuration elapsedDuration = anim->ElapsedDurationAt(aRefreshTime);
      ComputedTiming computedTiming =
        ElementAnimation::GetComputedTimingAt(elapsedDuration, anim->mTiming);

      // XXX We shouldn't really be using mLastNotification as a general
      // indicator that the animation has finished, it should be reserved for
      // events. If we use it differently in the future this use might need
      // changing.
      if (!anim->mIsRunningOnCompositor ||
          (computedTiming.mPhase == ComputedTiming::AnimationPhase_After &&
           anim->mLastNotification != ElementAnimation::LAST_NOTIFICATION_END))
      {
        aIsThrottled = false;
        break;
      }
    }
  }

  if (aIsThrottled) {
    return;
  }

  // mStyleRule may be null and valid, if we have no style to apply.
  if (mStyleRuleRefreshTime.IsNull() ||
      mStyleRuleRefreshTime != aRefreshTime) {
    mStyleRuleRefreshTime = aRefreshTime;
    mStyleRule = nullptr;
    // We'll set mNeedsRefreshes to true below in all cases where we need them.
    mNeedsRefreshes = false;

    // FIXME(spec): assume that properties in higher animations override
    // those in lower ones.
    // Therefore, we iterate from last animation to first.
    nsCSSPropertySet properties;

    for (uint32_t animIdx = mAnimations.Length(); animIdx-- != 0; ) {
      ElementAnimation* anim = mAnimations[animIdx];

      if (anim->mProperties.IsEmpty() ||
          anim->mTiming.mIterationDuration.ToMilliseconds() <= 0.0) {
        // The animation isn't active or filling at this time.
        continue;
      }

      // The ElapsedDurationAt() call here handles pausing.  But:
      // FIXME: avoid recalculating every time when paused.
      TimeDuration elapsedDuration = anim->ElapsedDurationAt(aRefreshTime);
      ComputedTiming computedTiming =
        ElementAnimation::GetComputedTimingAt(elapsedDuration, anim->mTiming);

      if ((computedTiming.mPhase == ComputedTiming::AnimationPhase_Before ||
           computedTiming.mPhase == ComputedTiming::AnimationPhase_Active) &&
          !anim->IsPaused()) {
        mNeedsRefreshes = true;
      }

      // If the time fraction is null, we don't have fill data for the current
      // time so we shouldn't animate.
      if (computedTiming.mTimeFraction == ComputedTiming::kNullTimeFraction)
        continue;

      NS_ABORT_IF_FALSE(0.0 <= computedTiming.mTimeFraction &&
                        computedTiming.mTimeFraction <= 1.0,
                        "timing fraction should be in [0-1]");

      for (uint32_t propIdx = 0, propEnd = anim->mProperties.Length();
           propIdx != propEnd; ++propIdx)
      {
        const AnimationProperty &prop = anim->mProperties[propIdx];

        NS_ABORT_IF_FALSE(prop.mSegments[0].mFromKey == 0.0,
                          "incorrect first from key");
        NS_ABORT_IF_FALSE(prop.mSegments[prop.mSegments.Length() - 1].mToKey
                            == 1.0,
                          "incorrect last to key");

        if (properties.HasProperty(prop.mProperty)) {
          // A later animation already set this property.
          continue;
        }
        properties.AddProperty(prop.mProperty);

        NS_ABORT_IF_FALSE(prop.mSegments.Length() > 0,
                          "property should not be in animations if it "
                          "has no segments");

        // FIXME: Maybe cache the current segment?
        const AnimationPropertySegment *segment = prop.mSegments.Elements(),
                               *segmentEnd = segment + prop.mSegments.Length();
        while (segment->mToKey < computedTiming.mTimeFraction) {
          NS_ABORT_IF_FALSE(segment->mFromKey < segment->mToKey,
                            "incorrect keys");
          ++segment;
          if (segment == segmentEnd) {
            NS_ABORT_IF_FALSE(false, "incorrect time fraction");
            break; // in order to continue in outer loop (just below)
          }
          NS_ABORT_IF_FALSE(segment->mFromKey == (segment-1)->mToKey,
                            "incorrect keys");
        }
        if (segment == segmentEnd) {
          continue;
        }
        NS_ABORT_IF_FALSE(segment->mFromKey < segment->mToKey,
                          "incorrect keys");
        NS_ABORT_IF_FALSE(segment >= prop.mSegments.Elements() &&
                          size_t(segment - prop.mSegments.Elements()) <
                            prop.mSegments.Length(),
                          "out of array bounds");

        if (!mStyleRule) {
          // Allocate the style rule now that we know we have animation data.
          mStyleRule = new css::AnimValuesStyleRule();
        }

        double positionInSegment =
          (computedTiming.mTimeFraction - segment->mFromKey) /
          (segment->mToKey - segment->mFromKey);
        double valuePosition =
          segment->mTimingFunction.GetValue(positionInSegment);

        nsStyleAnimation::Value *val =
          mStyleRule->AddEmptyValue(prop.mProperty);

#ifdef DEBUG
        bool result =
#endif
          nsStyleAnimation::Interpolate(prop.mProperty,
                                        segment->mFromValue, segment->mToValue,
                                        valuePosition, *val);
        NS_ABORT_IF_FALSE(result, "interpolate must succeed now");
      }
    }
  }
}

void
ElementAnimations::GetEventsAt(TimeStamp aRefreshTime,
                               EventArray& aEventsToDispatch)
{
  for (uint32_t animIdx = mAnimations.Length(); animIdx-- != 0; ) {
    ElementAnimation* anim = mAnimations[animIdx];

    // We should *not* skip animations with zero duration (bug 1004365) or
    // those with no keyframes (bug 1004377).
    // We will fix this separately but for now this is necessary since
    // ElementAnimation::GetComputedTimingAt does not yet handle
    // zero-duration iterations.
    if (anim->mProperties.IsEmpty() ||
        anim->mTiming.mIterationDuration.ToMilliseconds() <= 0.0) {
      // The animation isn't active or filling at this time.
      continue;
    }

    TimeDuration elapsedDuration = anim->ElapsedDurationAt(aRefreshTime);
    ComputedTiming computedTiming =
      ElementAnimation::GetComputedTimingAt(elapsedDuration, anim->mTiming);

    switch (computedTiming.mPhase) {
      case ComputedTiming::AnimationPhase_Before:
        // Do nothing
        break;

      case ComputedTiming::AnimationPhase_Active:
        // Dispatch 'animationstart' or 'animationiteration' when needed.
        if (computedTiming.mCurrentIteration != anim->mLastNotification) {
          // Notify 'animationstart' even if a negative delay puts us
          // past the first iteration.
          // Note that when somebody changes the animation-duration
          // dynamically, this will fire an extra iteration event
          // immediately in many cases.  It's not clear to me if that's the
          // right thing to do.
          uint32_t message =
            anim->mLastNotification == ElementAnimation::LAST_NOTIFICATION_NONE
              ? NS_ANIMATION_START : NS_ANIMATION_ITERATION;

          anim->mLastNotification = computedTiming.mCurrentIteration;
          TimeDuration iterationStart =
            anim->mTiming.mIterationDuration *
            computedTiming.mCurrentIteration;
          TimeDuration elapsedTime =
            std::max(iterationStart, anim->InitialAdvance());
          AnimationEventInfo ei(mElement, anim->mName, message,
                                elapsedTime, PseudoElement());
          aEventsToDispatch.AppendElement(ei);
        }
        break;

      case ComputedTiming::AnimationPhase_After:
        TimeDuration activeDuration = anim->ActiveDuration();
        // If we skipped the animation interval entirely, dispatch
        // 'animationstart' first
        if (anim->mLastNotification ==
            ElementAnimation::LAST_NOTIFICATION_NONE) {
          // Notifying for start of 0th iteration.
          // (This is overwritten below but we set it here to maintain
          // internal consistency.)
          anim->mLastNotification = 0;
          TimeDuration elapsedTime =
            std::min(anim->InitialAdvance(), activeDuration);
          AnimationEventInfo ei(mElement, anim->mName, NS_ANIMATION_START,
                                elapsedTime, PseudoElement());
          aEventsToDispatch.AppendElement(ei);
        }
        // Dispatch 'animationend' when needed.
        if (anim->mLastNotification !=
            ElementAnimation::LAST_NOTIFICATION_END) {
          anim->mLastNotification = ElementAnimation::LAST_NOTIFICATION_END;
          AnimationEventInfo ei(mElement, anim->mName, NS_ANIMATION_END,
                                activeDuration, PseudoElement());
          aEventsToDispatch.AppendElement(ei);
        }
        break;
    }
  }
}

bool
ElementAnimations::HasAnimationOfProperty(nsCSSProperty aProperty) const
{
  for (uint32_t animIdx = mAnimations.Length(); animIdx-- != 0; ) {
    const ElementAnimation* anim = mAnimations[animIdx];
    if (anim->HasAnimationOfProperty(aProperty)) {
      return true;
    }
  }
  return false;
}

bool
ElementAnimations::CanPerformOnCompositorThread(CanAnimateFlags aFlags) const
{
  nsIFrame* frame = nsLayoutUtils::GetStyleFrame(mElement);
  if (!frame) {
    return false;
  }

  if (mElementProperty != nsGkAtoms::animationsProperty) {
    if (nsLayoutUtils::IsAnimationLoggingEnabled()) {
      nsCString message;
      message.AppendLiteral("Gecko bug: Async animation of pseudoelements not supported.  See bug 771367 (");
      message.Append(nsAtomCString(mElementProperty));
      message.Append(')');
      LogAsyncAnimationFailure(message, mElement);
    }
    return false;
  }

  TimeStamp now = frame->PresContext()->RefreshDriver()->MostRecentRefresh();

  for (uint32_t animIdx = mAnimations.Length(); animIdx-- != 0; ) {
    const ElementAnimation* anim = mAnimations[animIdx];
    for (uint32_t propIdx = 0, propEnd = anim->mProperties.Length();
         propIdx != propEnd; ++propIdx) {
      if (IsGeometricProperty(anim->mProperties[propIdx].mProperty) &&
          anim->IsRunningAt(now)) {
        aFlags = CanAnimateFlags(aFlags | CanAnimate_HasGeometricProperty);
        break;
      }
    }
  }

  bool hasOpacity = false;
  bool hasTransform = false;
  for (uint32_t animIdx = mAnimations.Length(); animIdx-- != 0; ) {
    const ElementAnimation* anim = mAnimations[animIdx];
    if (!anim->IsRunningAt(now)) {
      continue;
    }

    for (uint32_t propIdx = 0, propEnd = anim->mProperties.Length();
         propIdx != propEnd; ++propIdx) {
      const AnimationProperty& prop = anim->mProperties[propIdx];
      if (!CanAnimatePropertyOnCompositor(mElement,
                                          prop.mProperty,
                                          aFlags) ||
          IsCompositorAnimationDisabledForFrame(frame)) {
        return false;
      }
      if (prop.mProperty == eCSSProperty_opacity) {
        hasOpacity = true;
      } else if (prop.mProperty == eCSSProperty_transform) {
        hasTransform = true;
      }
    }
  }
  // This animation can be done on the compositor.  Mark the frame as active, in
  // case we are able to throttle this animation.
  if (hasOpacity) {
    ActiveLayerTracker::NotifyAnimated(frame, eCSSProperty_opacity);
  }
  if (hasTransform) {
    ActiveLayerTracker::NotifyAnimated(frame, eCSSProperty_transform);
  }
  return true;
}

ElementAnimations*
nsAnimationManager::GetElementAnimations(dom::Element *aElement,
                                         nsCSSPseudoElements::Type aPseudoType,
                                         bool aCreateIfNeeded)
{
  if (!aCreateIfNeeded && PR_CLIST_IS_EMPTY(&mElementData)) {
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
  ElementAnimations *ea = static_cast<ElementAnimations*>(
                             aElement->GetProperty(propName));
  if (!ea && aCreateIfNeeded) {
    // FIXME: Consider arena-allocating?
    ea = new ElementAnimations(aElement, propName, this,
           mPresContext->RefreshDriver()->MostRecentRefresh());
    nsresult rv = aElement->SetProperty(propName, ea,
                                        ElementAnimationsPropertyDtor, false);
    if (NS_FAILED(rv)) {
      NS_WARNING("SetProperty failed");
      delete ea;
      return nullptr;
    }
    if (propName == nsGkAtoms::animationsProperty) {
      aElement->SetMayHaveAnimations();
    }

    AddElementData(ea);
  }

  return ea;
}


void
nsAnimationManager::EnsureStyleRuleFor(ElementAnimations* aEA)
{
  TimeStamp refreshTime = mPresContext->RefreshDriver()->MostRecentRefresh();
  aEA->EnsureStyleRuleFor(refreshTime, false);
  aEA->GetEventsAt(refreshTime, mPendingEvents);
  CheckNeedsRefresh();
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
  if (!mPresContext->IsProcessingAnimationStyleChange()) {
    if (!mPresContext->IsDynamic()) {
      // For print or print preview, ignore animations.
      return nullptr;
    }

    // Everything that causes our animation data to change triggers a
    // style change, which in turn triggers a non-animation restyle.
    // Likewise, when we initially construct frames, we're not in a
    // style change, but also not in an animation restyle.

    const nsStyleDisplay *disp = aStyleContext->StyleDisplay();
    ElementAnimations *ea =
      GetElementAnimations(aElement, aStyleContext->GetPseudoType(), false);
    if (!ea &&
        disp->mAnimationNameCount == 1 &&
        disp->mAnimations[0].GetName().IsEmpty()) {
      return nullptr;
    }

    // build the animations list
    ElementAnimationPtrArray newAnimations;
    BuildAnimations(aStyleContext, newAnimations);

    if (newAnimations.IsEmpty()) {
      if (ea) {
        ea->Destroy();
      }
      return nullptr;
    }

    TimeStamp refreshTime = mPresContext->RefreshDriver()->MostRecentRefresh();

    if (ea) {
      ea->mStyleRule = nullptr;
      ea->mStyleRuleRefreshTime = TimeStamp();
      ea->UpdateAnimationGeneration(mPresContext);

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
      if (!ea->mAnimations.IsEmpty()) {
        for (uint32_t newIdx = 0, newEnd = newAnimations.Length();
             newIdx != newEnd; ++newIdx) {
          nsRefPtr<ElementAnimation> newAnim = newAnimations[newIdx];

          // Find the matching animation with this name in the old list
          // of animations.  Because of this code, they must all have
          // the same start time, though they might differ in pause
          // state.  So if a page uses multiple copies of the same
          // animation in one element's animation list, and gives them
          // different pause states, they, well, get what they deserve.
          // We'll use the last one since it's more likely to be the one
          // doing something.
          const ElementAnimation* oldAnim = nullptr;
          for (uint32_t oldIdx = ea->mAnimations.Length(); oldIdx-- != 0; ) {
            const ElementAnimation* a = ea->mAnimations[oldIdx];
            if (a->mName == newAnim->mName) {
              oldAnim = a;
              break;
            }
          }
          if (!oldAnim) {
            continue;
          }

          newAnim->mStartTime = oldAnim->mStartTime;
          newAnim->mLastNotification = oldAnim->mLastNotification;

          if (oldAnim->IsPaused()) {
            if (newAnim->IsPaused()) {
              // Copy pause start just like start time.
              newAnim->mPauseStart = oldAnim->mPauseStart;
            } else {
              // Handle change in pause state by adjusting start
              // time to unpause.
              newAnim->mStartTime += refreshTime - oldAnim->mPauseStart;
            }
          }
        }
      }
    } else {
      ea = GetElementAnimations(aElement, aStyleContext->GetPseudoType(),
                                true);
    }
    ea->mAnimations.SwapElements(newAnimations);
    ea->mNeedsRefreshes = true;

    ea->EnsureStyleRuleFor(refreshTime, false);
    ea->GetEventsAt(refreshTime, mPendingEvents);
    CheckNeedsRefresh();
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

class PercentageHashKey : public PLDHashEntryHdr
{
public:
  typedef const float& KeyType;
  typedef const float* KeyTypePointer;

  PercentageHashKey(KeyTypePointer aKey) : mValue(*aKey) { }
  PercentageHashKey(const PercentageHashKey& toCopy) : mValue(toCopy.mValue) { }
  ~PercentageHashKey() { }

  KeyType GetKey() const { return mValue; }
  bool KeyEquals(KeyTypePointer aKey) const { return *aKey == mValue; }

  static KeyTypePointer KeyToPointer(KeyType aKey) { return &aKey; }
  static PLDHashNumber HashKey(KeyTypePointer aKey) {
    static_assert(sizeof(PLDHashNumber) == sizeof(uint32_t),
                  "this hash function assumes PLDHashNumber is uint32_t");
    static_assert(PLDHashNumber(-1) > PLDHashNumber(0),
                  "this hash function assumes PLDHashNumber is uint32_t");
    float key = *aKey;
    NS_ABORT_IF_FALSE(0.0f <= key && key <= 1.0f, "out of range");
    return PLDHashNumber(key * UINT32_MAX);
  }
  enum { ALLOW_MEMMOVE = true };

private:
  const float mValue;
};

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
  ResolvedStyleCache() : mCache(16) {}
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
  // nsStyleAnimation::Value values in AnimationPropertySegment).
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
                                    ElementAnimationPtrArray& aAnimations)
{
  NS_ABORT_IF_FALSE(aAnimations.IsEmpty(), "expect empty array");

  ResolvedStyleCache resolvedStyles;

  const nsStyleDisplay *disp = aStyleContext->StyleDisplay();
  TimeStamp now = mPresContext->RefreshDriver()->MostRecentRefresh();
  for (uint32_t animIdx = 0, animEnd = disp->mAnimationNameCount;
       animIdx != animEnd; ++animIdx) {
    const nsAnimation& src = disp->mAnimations[animIdx];
    nsRefPtr<ElementAnimation> dest =
      *aAnimations.AppendElement(new ElementAnimation());

    dest->mName = src.GetName();

    dest->mTiming.mIterationDuration =
      TimeDuration::FromMilliseconds(src.GetDuration());
    dest->mTiming.mIterationCount = src.GetIterationCount();
    dest->mTiming.mDirection = src.GetDirection();
    dest->mTiming.mFillMode = src.GetFillMode();

    dest->mStartTime = now;
    dest->mPlayState = src.GetPlayState();
    if (dest->IsPaused()) {
      dest->mPauseStart = now;
    } else {
      dest->mPauseStart = TimeStamp();
    }
    dest->mDelay = TimeDuration::FromMilliseconds(src.GetDelay());

    nsCSSKeyframesRule* rule =
      mPresContext->StyleSet()->KeyframesRuleForName(mPresContext,
                                                     dest->mName);
    if (!rule) {
      // no segments
      continue;
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
        // (And PercentageHashKey currently assumes we either ignore or
        // clamp them.)
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

      AnimationProperty &propData = *dest->mProperties.AppendElement();
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
        dest->mProperties.RemoveElementAt(dest->mProperties.Length() - 1);
      }
    }

    aAnimations.AppendElement(dest);
  }
}

bool
nsAnimationManager::BuildSegment(InfallibleTArray<AnimationPropertySegment>&
                                   aSegments,
                                 nsCSSProperty aProperty,
                                 const nsAnimation& aAnimation,
                                 float aFromKey, nsStyleContext* aFromContext,
                                 mozilla::css::Declaration* aFromDeclaration,
                                 float aToKey, nsStyleContext* aToContext)
{
  nsStyleAnimation::Value fromValue, toValue, dummyValue;
  if (!ExtractComputedValueForTransition(aProperty, aFromContext, fromValue) ||
      !ExtractComputedValueForTransition(aProperty, aToContext, toValue) ||
      // Check that we can interpolate between these values
      // (If this is ever a performance problem, we could add a
      // CanInterpolate method, but it seems fine for now.)
      !nsStyleAnimation::Interpolate(aProperty, fromValue, toValue,
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

  ElementAnimations *ea =
    GetElementAnimations(aElement, aPseudoType, false);
  if (!ea) {
    return nullptr;
  }

  if (mPresContext->IsProcessingRestyles() &&
      !mPresContext->IsProcessingAnimationStyleChange()) {
    // During the non-animation part of processing restyles, we don't
    // add the animation rule.

    if (ea->mStyleRule) {
      ea->PostRestyleForAnimation(mPresContext);
    }

    return nullptr;
  }

  NS_WARN_IF_FALSE(!ea->mNeedsRefreshes ||
                   ea->mStyleRuleRefreshTime ==
                     mPresContext->RefreshDriver()->MostRecentRefresh(),
                   "should already have refreshed style rule");

  return ea->mStyleRule;
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
    RemoveAllElementData();
    return;
  }

  FlushAnimations(Can_Throttle);
}

void
nsAnimationManager::AddElementData(CommonElementAnimationData* aData)
{
  if (!mObservingRefreshDriver) {
    NS_ASSERTION(static_cast<ElementAnimations*>(aData)->mNeedsRefreshes,
                 "Added data which doesn't need refreshing?");
    // We need to observe the refresh driver.
    mPresContext->RefreshDriver()->AddRefreshObserver(this, Flush_Style);
    mObservingRefreshDriver = true;
  }

  PR_INSERT_BEFORE(aData, &mElementData);
}

void
nsAnimationManager::CheckNeedsRefresh()
{
  for (PRCList *l = PR_LIST_HEAD(&mElementData); l != &mElementData;
       l = PR_NEXT_LINK(l)) {
    if (static_cast<ElementAnimations*>(l)->mNeedsRefreshes) {
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
  for (PRCList *l = PR_LIST_HEAD(&mElementData); l != &mElementData;
       l = PR_NEXT_LINK(l)) {
    ElementAnimations *ea = static_cast<ElementAnimations*>(l);
    bool canThrottleTick = aFlags == Can_Throttle &&
      ea->CanPerformOnCompositorThread(
        CommonElementAnimationData::CanAnimateFlags(0)) &&
      ea->CanThrottleAnimation(now);

    nsRefPtr<css::AnimValuesStyleRule> oldStyleRule = ea->mStyleRule;
    ea->EnsureStyleRuleFor(now, canThrottleTick);
    ea->GetEventsAt(now, mPendingEvents);
    CheckNeedsRefresh();
    if (oldStyleRule != ea->mStyleRule) {
      ea->PostRestyleForAnimation(mPresContext);
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

void
nsAnimationManager::UpdateThrottledStylesForSubtree(nsIContent* aContent,
                                                nsStyleContext* aParentStyle,
                                                nsStyleChangeList& aChangeList)
{
  dom::Element* element;
  if (aContent->IsElement()) {
    element = aContent->AsElement();
  } else {
    element = nullptr;
  }

  nsRefPtr<nsStyleContext> newStyle;

  ElementAnimations* ea;
  if (element &&
      (ea = GetElementAnimations(element,
                                 nsCSSPseudoElements::ePseudo_NotPseudoElement,
                                 false))) {
    // re-resolve our style
    newStyle = UpdateThrottledStyle(element, aParentStyle, aChangeList);
    // remove the current transition from the working set
    ea->mFlushGeneration = mPresContext->RefreshDriver()->MostRecentRefresh();
  } else {
    newStyle = ReparentContent(aContent, aParentStyle);
  }

  // walk the children
  if (newStyle) {
    for (nsIContent *child = aContent->GetFirstChild(); child;
         child = child->GetNextSibling()) {
      UpdateThrottledStylesForSubtree(child, newStyle, aChangeList);
    }
  }
}

IMPL_UPDATE_ALL_THROTTLED_STYLES_INTERNAL(nsAnimationManager,
                                          GetElementAnimations)

void
nsAnimationManager::UpdateAllThrottledStyles()
{
  if (PR_CLIST_IS_EMPTY(&mElementData)) {
    // no throttled animations, leave early
    mPresContext->TickLastUpdateThrottledAnimationStyle();
    return;
  }

  if (mPresContext->ThrottledAnimationStyleIsUpToDate()) {
    // throttled transitions are up to date, leave early
    return;
  }

  mPresContext->TickLastUpdateThrottledAnimationStyle();

  UpdateAllThrottledStylesInternal();
}

