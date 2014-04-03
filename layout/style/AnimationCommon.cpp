/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AnimationCommon.h"
#include "nsTransitionManager.h"
#include "nsAnimationManager.h"

#include "gfxPlatform.h"
#include "nsRuleData.h"
#include "nsCSSValue.h"
#include "nsStyleContext.h"
#include "nsIFrame.h"
#include "nsLayoutUtils.h"
#include "mozilla/LookAndFeel.h"
#include "Layers.h"
#include "FrameLayerBuilder.h"
#include "nsDisplayList.h"
#include "mozilla/MemoryReporting.h"
#include "RestyleManager.h"
#include "nsStyleSet.h"
#include "nsStyleChangeList.h"


using mozilla::layers::Layer;

namespace mozilla {
namespace css {

/* static */ bool
IsGeometricProperty(nsCSSProperty aProperty)
{
  switch (aProperty) {
    case eCSSProperty_bottom:
    case eCSSProperty_height:
    case eCSSProperty_left:
    case eCSSProperty_right:
    case eCSSProperty_top:
    case eCSSProperty_width:
      return true;
    default:
      return false;
  }
}

CommonAnimationManager::CommonAnimationManager(nsPresContext *aPresContext)
  : mPresContext(aPresContext)
{
  PR_INIT_CLIST(&mElementData);
}

CommonAnimationManager::~CommonAnimationManager()
{
  NS_ABORT_IF_FALSE(!mPresContext, "Disconnect should have been called");
}

void
CommonAnimationManager::Disconnect()
{
  // Content nodes might outlive the transition or animation manager.
  RemoveAllElementData();

  mPresContext = nullptr;
}

void
CommonAnimationManager::RemoveAllElementData()
{
  while (!PR_CLIST_IS_EMPTY(&mElementData)) {
    CommonElementAnimationData *head =
      static_cast<CommonElementAnimationData*>(PR_LIST_HEAD(&mElementData));
    head->Destroy();
  }
}

/*
 * nsISupports implementation
 */

NS_IMPL_ISUPPORTS1(CommonAnimationManager, nsIStyleRuleProcessor)

nsRestyleHint
CommonAnimationManager::HasStateDependentStyle(StateRuleProcessorData* aData)
{
  return nsRestyleHint(0);
}

nsRestyleHint
CommonAnimationManager::HasStateDependentStyle(PseudoElementStateRuleProcessorData* aData)
{
  return nsRestyleHint(0);
}

bool
CommonAnimationManager::HasDocumentStateDependentStyle(StateRuleProcessorData* aData)
{
  return false;
}

nsRestyleHint
CommonAnimationManager::HasAttributeDependentStyle(AttributeRuleProcessorData* aData)
{
  return nsRestyleHint(0);
}

/* virtual */ bool
CommonAnimationManager::MediumFeaturesChanged(nsPresContext* aPresContext)
{
  return false;
}

/* virtual */ size_t
CommonAnimationManager::SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
{
  // Measurement of the following members may be added later if DMD finds it is
  // worthwhile:
  // - mElementData
  //
  // The following members are not measured
  // - mPresContext, because it's non-owning

  return 0;
}

/* virtual */ size_t
CommonAnimationManager::SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
{
  return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
}

/* static */ bool
CommonAnimationManager::ExtractComputedValueForTransition(
                          nsCSSProperty aProperty,
                          nsStyleContext* aStyleContext,
                          nsStyleAnimation::Value& aComputedValue)
{
  bool result =
    nsStyleAnimation::ExtractComputedValue(aProperty, aStyleContext,
                                           aComputedValue);
  if (aProperty == eCSSProperty_visibility) {
    NS_ABORT_IF_FALSE(aComputedValue.GetUnit() ==
                        nsStyleAnimation::eUnit_Enumerated,
                      "unexpected unit");
    aComputedValue.SetIntValue(aComputedValue.GetIntValue(),
                               nsStyleAnimation::eUnit_Visibility);
  }
  return result;
}

already_AddRefed<nsStyleContext>
CommonAnimationManager::ReparentContent(nsIContent* aContent,
                                        nsStyleContext* aParentStyle)
{
  nsStyleSet* styleSet = mPresContext->PresShell()->StyleSet();
  nsIFrame* primaryFrame = nsLayoutUtils::GetStyleFrame(aContent);
  if (!primaryFrame) {
    return nullptr;
  }

  dom::Element* element = aContent->IsElement()
                          ? aContent->AsElement()
                          : nullptr;

  nsRefPtr<nsStyleContext> newStyle =
    styleSet->ReparentStyleContext(primaryFrame->StyleContext(),
                                   aParentStyle, element);
  primaryFrame->SetStyleContext(newStyle);
  ReparentBeforeAndAfter(element, primaryFrame, newStyle, styleSet);

  return newStyle.forget();
}

/* static */ void
CommonAnimationManager::ReparentBeforeAndAfter(dom::Element* aElement,
                                               nsIFrame* aPrimaryFrame,
                                               nsStyleContext* aNewStyle,
                                               nsStyleSet* aStyleSet)
{
  if (nsIFrame* before = nsLayoutUtils::GetBeforeFrame(aPrimaryFrame)) {
    nsRefPtr<nsStyleContext> beforeStyle =
      aStyleSet->ReparentStyleContext(before->StyleContext(),
                                     aNewStyle, aElement);
    before->SetStyleContext(beforeStyle);
  }
  if (nsIFrame* after = nsLayoutUtils::GetBeforeFrame(aPrimaryFrame)) {
    nsRefPtr<nsStyleContext> afterStyle =
      aStyleSet->ReparentStyleContext(after->StyleContext(),
                                     aNewStyle, aElement);
    after->SetStyleContext(afterStyle);
  }
}

nsStyleContext*
CommonAnimationManager::UpdateThrottledStyle(dom::Element* aElement,
                                             nsStyleContext* aParentStyle,
                                             nsStyleChangeList& aChangeList)
{
  NS_ASSERTION(mPresContext->TransitionManager()->GetElementTransitions(
                 aElement,
                 nsCSSPseudoElements::ePseudo_NotPseudoElement,
                 false) ||
               mPresContext->AnimationManager()->GetElementAnimations(
                 aElement,
                 nsCSSPseudoElements::ePseudo_NotPseudoElement,
                 false), "element not animated");

  nsIFrame* primaryFrame = nsLayoutUtils::GetStyleFrame(aElement);
  if (!primaryFrame) {
    return nullptr;
  }

  nsStyleContext* oldStyle = primaryFrame->StyleContext();
  nsRuleNode* ruleNode = oldStyle->RuleNode();
  nsTArray<nsStyleSet::RuleAndLevel> rules;
  do {
    if (ruleNode->IsRoot()) {
      break;
    }

    nsStyleSet::RuleAndLevel curRule;
    curRule.mLevel = ruleNode->GetLevel();

    if (curRule.mLevel == nsStyleSet::eAnimationSheet) {
      ElementAnimations* ea =
        mPresContext->AnimationManager()->GetElementAnimations(
          aElement,
          oldStyle->GetPseudoType(),
          false);
      NS_ASSERTION(ea,
        "Rule has level eAnimationSheet without animation on manager");

      mPresContext->AnimationManager()->EnsureStyleRuleFor(ea);
      curRule.mRule = ea->mStyleRule;
    } else if (curRule.mLevel == nsStyleSet::eTransitionSheet) {
      ElementTransitions *et =
        mPresContext->TransitionManager()->GetElementTransitions(
          aElement,
          oldStyle->GetPseudoType(),
          false);
      NS_ASSERTION(et,
        "Rule has level eTransitionSheet without transition on manager");

      et->EnsureStyleRuleFor(mPresContext->RefreshDriver()->MostRecentRefresh());
      curRule.mRule = et->mStyleRule;
    } else {
      curRule.mRule = ruleNode->GetRule();
    }

    if (curRule.mRule) {
      rules.AppendElement(curRule);
    }
  } while ((ruleNode = ruleNode->GetParent()));

  nsRefPtr<nsStyleContext> newStyle = mPresContext->PresShell()->StyleSet()->
    ResolveStyleForRules(aParentStyle, oldStyle, rules);

  // We absolutely must call CalcStyleDifference in order to ensure the
  // new context has all the structs cached that the old context had.
  // We also need it for processing of the changes.
  nsChangeHint styleChange =
    oldStyle->CalcStyleDifference(newStyle, nsChangeHint(0));
  aChangeList.AppendChange(primaryFrame, primaryFrame->GetContent(),
                           styleChange);

  primaryFrame->SetStyleContext(newStyle);

  ReparentBeforeAndAfter(aElement, primaryFrame, newStyle,
                         mPresContext->PresShell()->StyleSet());

  return newStyle;
}

NS_IMPL_ISUPPORTS1(AnimValuesStyleRule, nsIStyleRule)

/* virtual */ void
AnimValuesStyleRule::MapRuleInfoInto(nsRuleData* aRuleData)
{
  nsStyleContext *contextParent = aRuleData->mStyleContext->GetParent();
  if (contextParent && contextParent->HasPseudoElementData()) {
    // Don't apply transitions or animations to things inside of
    // pseudo-elements.
    // FIXME (Bug 522599): Add tests for this.
    return;
  }

  for (uint32_t i = 0, i_end = mPropertyValuePairs.Length(); i < i_end; ++i) {
    PropertyValuePair &cv = mPropertyValuePairs[i];
    if (aRuleData->mSIDs & nsCachedStyleData::GetBitForSID(
                             nsCSSProps::kSIDTable[cv.mProperty]))
    {
      nsCSSValue *prop = aRuleData->ValueFor(cv.mProperty);
      if (prop->GetUnit() == eCSSUnit_Null) {
#ifdef DEBUG
        bool ok =
#endif
          nsStyleAnimation::UncomputeValue(cv.mProperty, cv.mValue, *prop);
        NS_ABORT_IF_FALSE(ok, "could not store computed value");
      }
    }
  }
}

#ifdef DEBUG
/* virtual */ void
AnimValuesStyleRule::List(FILE* out, int32_t aIndent) const
{
  for (int32_t index = aIndent; --index >= 0; ) fputs("  ", out);
  fputs("[anim values] { ", out);
  for (uint32_t i = 0, i_end = mPropertyValuePairs.Length(); i < i_end; ++i) {
    const PropertyValuePair &pair = mPropertyValuePairs[i];
    nsAutoString value;
    nsStyleAnimation::UncomputeValue(pair.mProperty, pair.mValue, value);
    fprintf(out, "%s: %s; ", nsCSSProps::GetStringValue(pair.mProperty).get(),
                             NS_ConvertUTF16toUTF8(value).get());
  }
  fputs("}\n", out);
}
#endif

void
ComputedTimingFunction::Init(const nsTimingFunction &aFunction)
{
  mType = aFunction.mType;
  if (mType == nsTimingFunction::Function) {
    mTimingFunction.Init(aFunction.mFunc.mX1, aFunction.mFunc.mY1,
                         aFunction.mFunc.mX2, aFunction.mFunc.mY2);
  } else {
    mSteps = aFunction.mSteps;
  }
}

static inline double
StepEnd(uint32_t aSteps, double aPortion)
{
  NS_ABORT_IF_FALSE(0.0 <= aPortion && aPortion <= 1.0, "out of range");
  uint32_t step = uint32_t(aPortion * aSteps); // floor
  return double(step) / double(aSteps);
}

double
ComputedTimingFunction::GetValue(double aPortion) const
{
  switch (mType) {
    case nsTimingFunction::Function:
      return mTimingFunction.GetSplineValue(aPortion);
    case nsTimingFunction::StepStart:
      // There are diagrams in the spec that seem to suggest this check
      // and the bounds point should not be symmetric with StepEnd, but
      // should actually step up at rather than immediately after the
      // fraction points.  However, we rely on rounding negative values
      // up to zero, so we can't do that.  And it's not clear the spec
      // really meant it.
      return 1.0 - StepEnd(mSteps, 1.0 - aPortion);
    default:
      NS_ABORT_IF_FALSE(false, "bad type");
      // fall through
    case nsTimingFunction::StepEnd:
      return StepEnd(mSteps, aPortion);
  }
}

} /* end sub-namespace css */

bool
StyleAnimation::IsRunningAt(TimeStamp aTime) const
{
  if (IsPaused() || mIterationDuration.ToMilliseconds() <= 0.0 ||
      mStartTime.IsNull()) {
    return false;
  }

  double iterationsElapsed = ElapsedDurationAt(aTime) / mIterationDuration;
  return 0.0 <= iterationsElapsed && iterationsElapsed < mIterationCount;
}

bool
StyleAnimation::HasAnimationOfProperty(nsCSSProperty aProperty) const
{
  for (uint32_t propIdx = 0, propEnd = mProperties.Length();
       propIdx != propEnd; ++propIdx) {
    if (aProperty == mProperties[propIdx].mProperty) {
      return true;
    }
  }
  return false;
}

namespace css {

bool
CommonElementAnimationData::CanAnimatePropertyOnCompositor(const dom::Element *aElement,
                                                           nsCSSProperty aProperty,
                                                           CanAnimateFlags aFlags)
{
  bool shouldLog = nsLayoutUtils::IsAnimationLoggingEnabled();
  if (!gfxPlatform::OffMainThreadCompositingEnabled()) {
    if (shouldLog) {
      nsCString message;
      message.AppendLiteral("Performance warning: Compositor disabled");
      LogAsyncAnimationFailure(message);
    }
    return false;
  }

  nsIFrame* frame = nsLayoutUtils::GetStyleFrame(aElement);
  if (IsGeometricProperty(aProperty)) {
    if (shouldLog) {
      nsCString message;
      message.AppendLiteral("Performance warning: Async animation of geometric property '");
      message.Append(nsCSSProps::GetStringValue(aProperty));
      message.AppendLiteral("' is disabled");
      LogAsyncAnimationFailure(message, aElement);
    }
    return false;
  }
  if (aProperty == eCSSProperty_transform) {
    if (frame->Preserves3D() &&
        frame->Preserves3DChildren()) {
      if (shouldLog) {
        nsCString message;
        message.AppendLiteral("Gecko bug: Async animation of 'preserve-3d' transforms is not supported.  See bug 779598");
        LogAsyncAnimationFailure(message, aElement);
      }
      return false;
    }
    if (frame->IsSVGTransformed()) {
      if (shouldLog) {
        nsCString message;
        message.AppendLiteral("Gecko bug: Async 'transform' animations of frames with SVG transforms is not supported.  See bug 779599");
        LogAsyncAnimationFailure(message, aElement);
      }
      return false;
    }
    if (aFlags & CanAnimate_HasGeometricProperty) {
      if (shouldLog) {
        nsCString message;
        message.AppendLiteral("Performance warning: Async animation of 'transform' not possible due to presence of geometric properties");
        LogAsyncAnimationFailure(message, aElement);
      }
      return false;
    }
  }
  bool enabled = nsLayoutUtils::AreAsyncAnimationsEnabled();
  if (!enabled && shouldLog) {
    nsCString message;
    message.AppendLiteral("Performance warning: Async animations are disabled");
    LogAsyncAnimationFailure(message);
  }
  bool propertyAllowed = (aProperty == eCSSProperty_transform) ||
                         (aProperty == eCSSProperty_opacity) ||
                         (aFlags & CanAnimate_AllowPartial);
  return enabled && propertyAllowed;
}

/* static */ bool
CommonElementAnimationData::IsCompositorAnimationDisabledForFrame(nsIFrame* aFrame)
{
  void* prop = aFrame->Properties().Get(nsIFrame::RefusedAsyncAnimation());
  return bool(reinterpret_cast<intptr_t>(prop));
}

/* static */ void
CommonElementAnimationData::LogAsyncAnimationFailure(nsCString& aMessage,
                                                     const nsIContent* aContent)
{
  if (aContent) {
    aMessage.AppendLiteral(" [");
    aMessage.Append(nsAtomCString(aContent->Tag()));

    nsIAtom* id = aContent->GetID();
    if (id) {
      aMessage.AppendLiteral(" with id '");
      aMessage.Append(nsAtomCString(aContent->GetID()));
      aMessage.AppendLiteral("'");
    }
    aMessage.AppendLiteral("]");
  }
  aMessage.AppendLiteral("\n");
  printf_stderr(aMessage.get());
}

bool
CommonElementAnimationData::CanThrottleTransformChanges(TimeStamp aTime)
{
  if (!nsLayoutUtils::AreAsyncAnimationsEnabled()) {
    return false;
  }

  // If we know that the animation cannot cause overflow,
  // we can just disable flushes for this animation.

  // If we don't show scrollbars, we don't care about overflow.
  if (LookAndFeel::GetInt(LookAndFeel::eIntID_ShowHideScrollbars) == 0) {
    return true;
  }

  // If this animation can cause overflow, we can throttle some of the ticks.
  if ((aTime - mStyleRuleRefreshTime) < TimeDuration::FromMilliseconds(200)) {
    return true;
  }

  // If the nearest scrollable ancestor has overflow:hidden,
  // we don't care about overflow.
  nsIScrollableFrame* scrollable = nsLayoutUtils::GetNearestScrollableFrame(
                                     nsLayoutUtils::GetStyleFrame(mElement));
  if (!scrollable) {
    return true;
  }

  ScrollbarStyles ss = scrollable->GetScrollbarStyles();
  if (ss.mVertical == NS_STYLE_OVERFLOW_HIDDEN &&
      ss.mHorizontal == NS_STYLE_OVERFLOW_HIDDEN &&
      scrollable->GetLogicalScrollPosition() == nsPoint(0, 0)) {
    return true;
  }

  return false;
}

bool
CommonElementAnimationData::CanThrottleAnimation(TimeStamp aTime)
{
  nsIFrame* frame = nsLayoutUtils::GetStyleFrame(mElement);
  if (!frame) {
    return false;
  }

  bool hasTransform = HasAnimationOfProperty(eCSSProperty_transform);
  bool hasOpacity = HasAnimationOfProperty(eCSSProperty_opacity);
  if (hasOpacity) {
    Layer* layer = FrameLayerBuilder::GetDedicatedLayer(
                     frame, nsDisplayItem::TYPE_OPACITY);
    if (!layer || mAnimationGeneration > layer->GetAnimationGeneration()) {
      return false;
    }
  }

  if (!hasTransform) {
    return true;
  }

  Layer* layer = FrameLayerBuilder::GetDedicatedLayer(
                   frame, nsDisplayItem::TYPE_TRANSFORM);
  if (!layer || mAnimationGeneration > layer->GetAnimationGeneration()) {
    return false;
  }

  return CanThrottleTransformChanges(aTime);
}

void 
CommonElementAnimationData::UpdateAnimationGeneration(nsPresContext* aPresContext)
{
  mAnimationGeneration =
    aPresContext->RestyleManager()->GetAnimationGeneration();
}

}
}
