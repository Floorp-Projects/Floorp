/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/*
 * structures that represent things to be painted (ordered in z-order),
 * used during painting and hit testing
 */

#include "nsDisplayList.h"

#include <stdint.h>
#include <algorithm>
#include <limits>

#include "gfxContext.h"
#include "gfxUtils.h"
#include "mozilla/dom/TabChild.h"
#include "mozilla/dom/KeyframeEffect.h"
#include "mozilla/dom/Selection.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/layers/PLayerTransaction.h"
#include "nsCSSRendering.h"
#include "nsCSSRenderingGradients.h"
#include "nsISelectionController.h"
#include "nsIPresShell.h"
#include "nsRegion.h"
#include "nsStyleStructInlines.h"
#include "nsStyleTransformMatrix.h"
#include "gfxMatrix.h"
#include "gfxPrefs.h"
#include "nsSVGIntegrationUtils.h"
#include "nsSVGUtils.h"
#include "nsLayoutUtils.h"
#include "nsIScrollableFrame.h"
#include "nsIFrameInlines.h"
#include "nsThemeConstants.h"
#include "BorderConsts.h"
#include "LayerTreeInvalidation.h"
#include "mozilla/MathAlgorithms.h"

#include "imgIContainer.h"
#include "BasicLayers.h"
#include "nsBoxFrame.h"
#include "nsImageFrame.h"
#include "nsSubDocumentFrame.h"
#include "SVGObserverUtils.h"
#include "nsSVGElement.h"
#include "nsSVGClipPathFrame.h"
#include "GeckoProfiler.h"
#include "nsViewManager.h"
#include "ImageLayers.h"
#include "ImageContainer.h"
#include "nsCanvasFrame.h"
#include "StickyScrollContainer.h"
#include "mozilla/AnimationPerformanceWarning.h"
#include "mozilla/AnimationUtils.h"
#include "mozilla/EffectCompositor.h"
#include "mozilla/EffectSet.h"
#include "mozilla/EventStates.h"
#include "mozilla/LookAndFeel.h"
#include "mozilla/OperatorNewExtensions.h"
#include "mozilla/PendingAnimationTracker.h"
#include "mozilla/Preferences.h"
#include "mozilla/StyleAnimationValue.h"
#include "mozilla/ServoBindings.h"
#include "mozilla/Telemetry.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Unused.h"
#include "mozilla/ViewportFrame.h"
#include "mozilla/gfx/gfxVars.h"
#include "ActiveLayerTracker.h"
#include "nsContentUtils.h"
#include "nsPrintfCString.h"
#include "UnitTransforms.h"
#include "LayersLogging.h"
#include "FrameLayerBuilder.h"
#include "mozilla/EventStateManager.h"
#include "nsCaret.h"
#include "nsDOMTokenList.h"
#include "nsCSSProps.h"
#include "nsSVGMaskFrame.h"
#include "nsTableCellFrame.h"
#include "nsTableColFrame.h"
#include "nsSliderFrame.h"
#include "ClientLayerManager.h"
#include "mozilla/layers/StackingContextHelper.h"
#include "mozilla/layers/WebRenderBridgeChild.h"
#include "mozilla/layers/WebRenderLayerManager.h"
#include "mozilla/layers/WebRenderMessages.h"
#include "mozilla/layers/WebRenderScrollData.h"

// GetCurrentTime is defined in winbase.h as zero argument macro forwarding to
// GetTickCount().
#ifdef GetCurrentTime
#undef GetCurrentTime
#endif

using namespace mozilla;
using namespace mozilla::layers;
using namespace mozilla::dom;
using namespace mozilla::layout;
using namespace mozilla::gfx;

typedef FrameMetrics::ViewID ViewID;
typedef nsStyleTransformMatrix::TransformReferenceBox TransformReferenceBox;

#ifdef DEBUG
static bool
SpammyLayoutWarningsEnabled()
{
  static bool sValue = false;
  static bool sValueInitialized = false;

  if (!sValueInitialized) {
    Preferences::GetBool("layout.spammy_warnings.enabled", &sValue);
    sValueInitialized = true;
  }

  return sValue;
}
#endif

#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
void AssertUniqueItem(nsDisplayItem* aItem) {
  nsIFrame::DisplayItemArray* items = aItem->Frame()->GetProperty(nsIFrame::DisplayItems());
  if (!items) {
    return;
  }
  for (nsDisplayItem* i : *items) {
    if (i != aItem &&
        !i->HasDeletedFrame() &&
        i->Frame() == aItem->Frame() &&
        i->GetPerFrameKey() == aItem->GetPerFrameKey()) {
      if (i->mPreProcessedItem) {
        continue;
      }
      MOZ_DIAGNOSTIC_ASSERT(false, "Duplicate display item!");
    }
  }
}
#endif

/* static */ bool
ActiveScrolledRoot::IsAncestor(const ActiveScrolledRoot* aAncestor,
                               const ActiveScrolledRoot* aDescendant)
{
  if (!aAncestor) {
    // nullptr is the root
    return true;
  }
  if (Depth(aAncestor) > Depth(aDescendant)) {
    return false;
  }
  const ActiveScrolledRoot* asr = aDescendant;
  while (asr) {
    if (asr == aAncestor) {
      return true;
    }
    asr = asr->mParent;
  }
  return false;
}

/* static */ nsCString
ActiveScrolledRoot::ToString(const ActiveScrolledRoot* aActiveScrolledRoot)
{
  nsAutoCString str;
  for (auto* asr = aActiveScrolledRoot; asr; asr = asr->mParent) {
    str.AppendPrintf("<0x%p>", asr->mScrollableFrame);
    if (asr->mParent) {
      str.AppendLiteral(", ");
    }
  }
  return str;
}

static inline CSSAngle
MakeCSSAngle(const nsCSSValue& aValue)
{
  return CSSAngle(aValue.GetAngleValue(), aValue.GetUnit());
}

static void AddTransformFunctions(const nsCSSValueList* aList,
                                  mozilla::ComputedStyle* aStyle,
                                  nsPresContext* aPresContext,
                                  TransformReferenceBox& aRefBox,
                                  InfallibleTArray<TransformFunction>& aFunctions)
{
  if (aList->mValue.GetUnit() == eCSSUnit_None) {
    return;
  }

  for (const nsCSSValueList* curr = aList; curr; curr = curr->mNext) {
    const nsCSSValue& currElem = curr->mValue;
    NS_ASSERTION(currElem.GetUnit() == eCSSUnit_Function,
                 "Stream should consist solely of functions!");
    nsCSSValue::Array* array = currElem.GetArrayValue();
    switch (nsStyleTransformMatrix::TransformFunctionOf(array)) {
      case eCSSKeyword_rotatex:
      {
        CSSAngle theta = MakeCSSAngle(array->Item(1));
        aFunctions.AppendElement(RotationX(theta));
        break;
      }
      case eCSSKeyword_rotatey:
      {
        CSSAngle theta = MakeCSSAngle(array->Item(1));
        aFunctions.AppendElement(RotationY(theta));
        break;
      }
      case eCSSKeyword_rotatez:
      {
        CSSAngle theta = MakeCSSAngle(array->Item(1));
        aFunctions.AppendElement(RotationZ(theta));
        break;
      }
      case eCSSKeyword_rotate:
      {
        CSSAngle theta = MakeCSSAngle(array->Item(1));
        aFunctions.AppendElement(Rotation(theta));
        break;
      }
      case eCSSKeyword_rotate3d:
      {
        double x = array->Item(1).GetFloatValue();
        double y = array->Item(2).GetFloatValue();
        double z = array->Item(3).GetFloatValue();
        CSSAngle theta = MakeCSSAngle(array->Item(4));
        aFunctions.AppendElement(Rotation3D(x, y, z, theta));
        break;
      }
      case eCSSKeyword_scalex:
      {
        double x = array->Item(1).GetFloatValue();
        aFunctions.AppendElement(Scale(x, 1, 1));
        break;
      }
      case eCSSKeyword_scaley:
      {
        double y = array->Item(1).GetFloatValue();
        aFunctions.AppendElement(Scale(1, y, 1));
        break;
      }
      case eCSSKeyword_scalez:
      {
        double z = array->Item(1).GetFloatValue();
        aFunctions.AppendElement(Scale(1, 1, z));
        break;
      }
      case eCSSKeyword_scale:
      {
        double x = array->Item(1).GetFloatValue();
        // scale(x) is shorthand for scale(x, x);
        double y = array->Count() == 2 ? x : array->Item(2).GetFloatValue();
        aFunctions.AppendElement(Scale(x, y, 1));
        break;
      }
      case eCSSKeyword_scale3d:
      {
        double x = array->Item(1).GetFloatValue();
        double y = array->Item(2).GetFloatValue();
        double z = array->Item(3).GetFloatValue();
        aFunctions.AppendElement(Scale(x, y, z));
        break;
      }
      case eCSSKeyword_translatex:
      {
        double x = nsStyleTransformMatrix::ProcessTranslatePart(
          array->Item(1),
          &aRefBox, &TransformReferenceBox::Width);
        aFunctions.AppendElement(Translation(x, 0, 0));
        break;
      }
      case eCSSKeyword_translatey:
      {
        double y = nsStyleTransformMatrix::ProcessTranslatePart(
          array->Item(1),
          &aRefBox, &TransformReferenceBox::Height);
        aFunctions.AppendElement(Translation(0, y, 0));
        break;
      }
      case eCSSKeyword_translatez:
      {
        double z = nsStyleTransformMatrix::ProcessTranslatePart(
          array->Item(1),
          nullptr);
        aFunctions.AppendElement(Translation(0, 0, z));
        break;
      }
      case eCSSKeyword_translate:
      {
        double x = nsStyleTransformMatrix::ProcessTranslatePart(
          array->Item(1),
          &aRefBox, &TransformReferenceBox::Width);
        // translate(x) is shorthand for translate(x, 0)
        double y = 0;
        if (array->Count() == 3) {
           y = nsStyleTransformMatrix::ProcessTranslatePart(
            array->Item(2),
            &aRefBox, &TransformReferenceBox::Height);
        }
        aFunctions.AppendElement(Translation(x, y, 0));
        break;
      }
      case eCSSKeyword_translate3d:
      {
        double x = nsStyleTransformMatrix::ProcessTranslatePart(
          array->Item(1),
          &aRefBox, &TransformReferenceBox::Width);
        double y = nsStyleTransformMatrix::ProcessTranslatePart(
          array->Item(2),
          &aRefBox, &TransformReferenceBox::Height);
        double z = nsStyleTransformMatrix::ProcessTranslatePart(
          array->Item(3),
          nullptr);

        aFunctions.AppendElement(Translation(x, y, z));
        break;
      }
      case eCSSKeyword_skewx:
      {
        CSSAngle x = MakeCSSAngle(array->Item(1));
        aFunctions.AppendElement(SkewX(x));
        break;
      }
      case eCSSKeyword_skewy:
      {
        CSSAngle y = MakeCSSAngle(array->Item(1));
        aFunctions.AppendElement(SkewY(y));
        break;
      }
      case eCSSKeyword_skew:
      {
        CSSAngle x = MakeCSSAngle(array->Item(1));
        // skew(x) is shorthand for skew(x, 0)
        CSSAngle y(0.0f, eCSSUnit_Degree);
        if (array->Count() == 3) {
          y = MakeCSSAngle(array->Item(2));
        }
        aFunctions.AppendElement(Skew(x, y));
        break;
      }
      case eCSSKeyword_matrix:
      {
        gfx::Matrix4x4 matrix;
        matrix._11 = array->Item(1).GetFloatValue();
        matrix._12 = array->Item(2).GetFloatValue();
        matrix._13 = 0;
        matrix._14 = 0;
        matrix._21 = array->Item(3).GetFloatValue();
        matrix._22 = array->Item(4).GetFloatValue();
        matrix._23 = 0;
        matrix._24 = 0;
        matrix._31 = 0;
        matrix._32 = 0;
        matrix._33 = 1;
        matrix._34 = 0;
        matrix._41 = ProcessTranslatePart(array->Item(5),
                                          &aRefBox, &TransformReferenceBox::Width);
        matrix._42 = ProcessTranslatePart(array->Item(6),
                                          &aRefBox, &TransformReferenceBox::Height);
        matrix._43 = 0;
        matrix._44 = 1;
        aFunctions.AppendElement(TransformMatrix(matrix));
        break;
      }
      case eCSSKeyword_matrix3d:
      {
        gfx::Matrix4x4 matrix;
        matrix._11 = array->Item(1).GetFloatValue();
        matrix._12 = array->Item(2).GetFloatValue();
        matrix._13 = array->Item(3).GetFloatValue();
        matrix._14 = array->Item(4).GetFloatValue();
        matrix._21 = array->Item(5).GetFloatValue();
        matrix._22 = array->Item(6).GetFloatValue();
        matrix._23 = array->Item(7).GetFloatValue();
        matrix._24 = array->Item(8).GetFloatValue();
        matrix._31 = array->Item(9).GetFloatValue();
        matrix._32 = array->Item(10).GetFloatValue();
        matrix._33 = array->Item(11).GetFloatValue();
        matrix._34 = array->Item(12).GetFloatValue();
        matrix._41 = ProcessTranslatePart(array->Item(13),
                                          &aRefBox, &TransformReferenceBox::Width);
        matrix._42 = ProcessTranslatePart(array->Item(14),
                                          &aRefBox, &TransformReferenceBox::Height);
        matrix._43 = ProcessTranslatePart(array->Item(15),
                                          &aRefBox, nullptr);
        matrix._44 = array->Item(16).GetFloatValue();
        aFunctions.AppendElement(TransformMatrix(matrix));
        break;
      }
      case eCSSKeyword_interpolatematrix:
      {
        bool dummy;
        Matrix4x4 matrix;
        nsStyleTransformMatrix::ProcessInterpolateMatrix(matrix, array,
                                                         aRefBox,
                                                         &dummy);
        aFunctions.AppendElement(TransformMatrix(matrix));
        break;
      }
      case eCSSKeyword_accumulatematrix:
      {
        bool dummy;
        Matrix4x4 matrix;
        nsStyleTransformMatrix::ProcessAccumulateMatrix(matrix, array,
                                                        aRefBox,
                                                        &dummy);
        aFunctions.AppendElement(TransformMatrix(matrix));
        break;
      }
      case eCSSKeyword_perspective:
      {
        aFunctions.AppendElement(Perspective(array->Item(1).GetFloatValue()));
        break;
      }
      default:
        NS_ERROR("Function not handled yet!");
    }
  }
}

static void
AddTransformFunctions(const nsCSSValueSharedList* aList,
                      const nsIFrame* aFrame,
                      TransformReferenceBox& aRefBox,
                      layers::Animatable& aAnimatable)
{
  MOZ_ASSERT(aList->mHead);
  AddTransformFunctions(aList->mHead,
                        aFrame->Style(),
                        aFrame->PresContext(),
                        aRefBox,
                        aAnimatable.get_ArrayOfTransformFunction());
}

static TimingFunction
ToTimingFunction(const Maybe<ComputedTimingFunction>& aCTF)
{
  if (aCTF.isNothing()) {
    return TimingFunction(null_t());
  }

  if (aCTF->HasSpline()) {
    const nsSMILKeySpline* spline = aCTF->GetFunction();
    return TimingFunction(CubicBezierFunction(spline->X1(), spline->Y1(),
                                              spline->X2(), spline->Y2()));
  }

  if (aCTF->GetType() == nsTimingFunction::Type::Frames) {
    return TimingFunction(FramesFunction(aCTF->GetFrames()));
  }

  uint32_t type = aCTF->GetType() == nsTimingFunction::Type::StepStart ? 1 : 2;
  return TimingFunction(StepFunction(aCTF->GetSteps(), type));
}

static void
SetAnimatable(nsCSSPropertyID aProperty,
              const AnimationValue& aAnimationValue,
              nsIFrame* aFrame,
              TransformReferenceBox& aRefBox,
              layers::Animatable& aAnimatable)
{
  MOZ_ASSERT(aFrame);

  if (aAnimationValue.IsNull()) {
    aAnimatable = null_t();
    return;
  }

  switch (aProperty) {
    case eCSSProperty_opacity:
      aAnimatable = aAnimationValue.GetOpacity();
      break;
    case eCSSProperty_transform: {
      aAnimatable = InfallibleTArray<TransformFunction>();
      if (aAnimationValue.mServo) {
        RefPtr<nsCSSValueSharedList> list;
        Servo_AnimationValue_GetTransform(aAnimationValue.mServo, &list);
        AddTransformFunctions(list, aFrame, aRefBox, aAnimatable);
      } else {
        MOZ_CRASH("old style system disabled");
      }
      break;
    }
    default:
      MOZ_ASSERT_UNREACHABLE("Unsupported property");
  }
}

static void
AddAnimationForProperty(nsIFrame* aFrame, const AnimationProperty& aProperty,
                        dom::Animation* aAnimation, AnimationInfo& aAnimationInfo,
                        AnimationData& aData, bool aPending)
{
  MOZ_ASSERT(aAnimation->GetEffect(),
             "Should not be adding an animation without an effect");
  MOZ_ASSERT(!aAnimation->GetCurrentOrPendingStartTime().IsNull() ||
             !aAnimation->IsPlaying() ||
             (aAnimation->GetTimeline() &&
              aAnimation->GetTimeline()->TracksWallclockTime()),
             "If the animation has an unresolved start time it should either"
             " be static (so we don't need a start time) or else have a"
             " timeline capable of converting TimeStamps (so we can calculate"
             " one later");

  layers::Animation* animation =
    aPending ?
    aAnimationInfo.AddAnimationForNextTransaction() :
    aAnimationInfo.AddAnimation();

  const TimingParams& timing = aAnimation->GetEffect()->SpecifiedTiming();

  // If we are starting a new transition that replaces an existing transition
  // running on the compositor, it is possible that the animation on the
  // compositor will have advanced ahead of the main thread. If we use as
  // the starting point of the new transition, the current value of the
  // replaced transition as calculated on the main thread using the refresh
  // driver time, the new transition will jump when it starts. Instead, we
  // re-calculate the starting point of the new transition by applying the
  // current TimeStamp to the parameters of the replaced transition.
  //
  // We need to do this here, rather than when we generate the new transition,
  // since after generating the new transition other requestAnimationFrame
  // callbacks may run that introduce further lag between the main thread and
  // the compositor.
  if (aAnimation->AsCSSTransition() &&
      aAnimation->GetEffect() &&
      aAnimation->GetEffect()->AsTransition()) {
    // We update startValue from the replaced transition only if the effect is
    // an ElementPropertyTransition.
    aAnimation->GetEffect()->AsTransition()->
      UpdateStartValueFromReplacedTransition();
  }

  animation->originTime() = !aAnimation->GetTimeline()
                            ? TimeStamp()
                            : aAnimation->GetTimeline()->
                                ToTimeStamp(TimeDuration());

  Nullable<TimeDuration> startTime = aAnimation->GetCurrentOrPendingStartTime();
  if (startTime.IsNull()) {
    animation->startTime() = null_t();
  } else {
    animation->startTime() = startTime.Value();
  }

  animation->holdTime() = aAnimation->GetCurrentTime().Value();

  const ComputedTiming computedTiming =
    aAnimation->GetEffect()->GetComputedTiming();
  animation->delay() = timing.Delay();
  animation->endDelay() = timing.EndDelay();
  animation->duration() = computedTiming.mDuration;
  animation->iterations() = computedTiming.mIterations;
  animation->iterationStart() = computedTiming.mIterationStart;
  animation->direction() = static_cast<uint8_t>(timing.Direction());
  animation->fillMode() = static_cast<uint8_t>(computedTiming.mFill);
  animation->property() = aProperty.mProperty;
  animation->playbackRate() = aAnimation->CurrentOrPendingPlaybackRate();
  animation->previousPlaybackRate() =
    aAnimation->HasPendingPlaybackRate()
      ? aAnimation->PlaybackRate()
      : std::numeric_limits<float>::quiet_NaN();
  animation->data() = aData;
  animation->easingFunction() = ToTimingFunction(timing.TimingFunction());
  animation->iterationComposite() =
    static_cast<uint8_t>(aAnimation->GetEffect()->
                         AsKeyframeEffect()->IterationComposite());
  animation->isNotPlaying() = !aAnimation->IsPlaying();

  TransformReferenceBox refBox(aFrame);

  // If the animation is additive or accumulates, we need to pass its base value
  // to the compositor.

  AnimationValue baseStyle =
    aAnimation->GetEffect()->AsKeyframeEffect()->BaseStyle(aProperty.mProperty);
  if (!baseStyle.IsNull()) {
    SetAnimatable(aProperty.mProperty,
                  baseStyle,
                  aFrame, refBox,
                  animation->baseStyle());
  } else {
    animation->baseStyle() = null_t();
  }

  for (uint32_t segIdx = 0; segIdx < aProperty.mSegments.Length(); segIdx++) {
    const AnimationPropertySegment& segment = aProperty.mSegments[segIdx];

    AnimationSegment* animSegment = animation->segments().AppendElement();
    SetAnimatable(aProperty.mProperty,
                  segment.mFromValue,
                  aFrame, refBox,
                  animSegment->startState());
    SetAnimatable(aProperty.mProperty,
                  segment.mToValue,
                  aFrame, refBox,
                  animSegment->endState());

    animSegment->startPortion() = segment.mFromKey;
    animSegment->endPortion() = segment.mToKey;
    animSegment->startComposite() =
      static_cast<uint8_t>(segment.mFromComposite);
    animSegment->endComposite() =
      static_cast<uint8_t>(segment.mToComposite);
    animSegment->sampleFn() = ToTimingFunction(segment.mTimingFunction);
  }
}

static void
AddAnimationsForProperty(nsIFrame* aFrame, nsDisplayListBuilder* aBuilder,
                         nsDisplayItem* aItem, nsCSSPropertyID aProperty,
                         AnimationInfo& aAnimationInfo, bool aPending,
                         bool aIsForWebRender)
{
  if (aPending) {
    aAnimationInfo.ClearAnimationsForNextTransaction();
  } else {
    aAnimationInfo.ClearAnimations();
  }

  nsIFrame* styleFrame = nsLayoutUtils::GetStyleFrame(aFrame);
  if (!styleFrame) {
    return;
  }

  // Update the animation generation on the layer. We need to do this before
  // any early returns since even if we don't add any animations to the
  // layer, we still need to mark it as up-to-date with regards to animations.
  // Otherwise, in RestyleManager we'll notice the discrepancy between the
  // animation generation numbers and update the layer indefinitely.
  uint64_t animationGeneration =
    // Note that GetAnimationGenerationForFrame() calles EffectSet::GetEffectSet
    // that expects to work with the style frame instead of the primary frame.
    RestyleManager::GetAnimationGenerationForFrame(styleFrame);
  aAnimationInfo.SetAnimationGeneration(animationGeneration);

  EffectCompositor::ClearIsRunningOnCompositor(styleFrame, aProperty);
  nsTArray<RefPtr<dom::Animation>> compositorAnimations =
    EffectCompositor::GetAnimationsForCompositor(styleFrame, aProperty);
  if (compositorAnimations.IsEmpty()) {
    return;
  }

  // If the frame is not prerendered, bail out.
  // Do this check only during layer construction; during updating the
  // caller is required to check it appropriately.
  if (aItem && !aItem->CanUseAsyncAnimations(aBuilder)) {
    // EffectCompositor needs to know that we refused to run this animation
    // asynchronously so that it will not throttle the main thread
    // animation.
    aFrame->SetProperty(nsIFrame::RefusedAsyncAnimationProperty(), true);

    // We need to schedule another refresh driver run so that EffectCompositor
    // gets a chance to unthrottle the animation.
    aFrame->SchedulePaint();
    return;
  }

  AnimationData data;
  if (aProperty == eCSSProperty_transform) {
    // XXX Performance here isn't ideal for SVG. We'd prefer to avoid resolving
    // the dimensions of refBox. That said, we only get here if there are CSS
    // animations or transitions on this element, and that is likely to be a
    // lot rarer than transforms on SVG (the frequency of which drives the need
    // for TransformReferenceBox).
    TransformReferenceBox refBox(aFrame);
    nsRect bounds(0, 0, refBox.Width(), refBox.Height());
    // all data passed directly to the compositor should be in dev pixels
    int32_t devPixelsToAppUnits = aFrame->PresContext()->AppUnitsPerDevPixel();
    float scale = devPixelsToAppUnits;
    Point3D offsetToTransformOrigin =
      nsDisplayTransform::GetDeltaToTransformOrigin(aFrame, scale, &bounds);
    nsPoint origin;
    float scaleX = 1.0f;
    float scaleY = 1.0f;
    bool hasPerspectiveParent = false;
    if (aIsForWebRender) {
      // leave origin empty, because we are sending it separately on the stacking
      // context that we are pushing to WR, and WR will automatically include
      // it when picking up the animated transform values
    } else if (aItem) {
      // This branch is for display items to leverage the cache of
      // nsDisplayListBuilder.
      origin = aItem->ToReferenceFrame();
    } else {
      // This branch is running for restyling.
      // Animations are animated at the coordination of the reference
      // frame outside, not the given frame itself.  The given frame
      // is also reference frame too, so the parent's reference frame
      // are used.
      nsIFrame* referenceFrame =
        nsLayoutUtils::GetReferenceFrame(nsLayoutUtils::GetCrossDocParentFrame(aFrame));
      origin = aFrame->GetOffsetToCrossDoc(referenceFrame);
    }

    data = TransformData(origin, offsetToTransformOrigin,
                         bounds, devPixelsToAppUnits,
                         scaleX, scaleY, hasPerspectiveParent);
  } else if (aProperty == eCSSProperty_opacity) {
    data = null_t();
  }

  MOZ_ASSERT(nsCSSProps::PropHasFlags(aProperty,
                                      CSSPropFlags::CanAnimateOnCompositor),
             "inconsistent property flags");

  // Add from first to last (since last overrides)
  for (size_t animIdx = 0; animIdx < compositorAnimations.Length(); animIdx++) {
    dom::Animation* anim = compositorAnimations[animIdx];
    if (!anim->IsRelevant()) {
      continue;
    }

    dom::KeyframeEffect* keyframeEffect =
      anim->GetEffect() ? anim->GetEffect()->AsKeyframeEffect() : nullptr;
    MOZ_ASSERT(keyframeEffect,
               "A playing animation should have a keyframe effect");
    const AnimationProperty* property =
      keyframeEffect->GetEffectiveAnimationOfProperty(aProperty);
    if (!property) {
      continue;
    }

    // Note that if the property is overridden by !important rules,
    // GetEffectiveAnimationOfProperty returns null instead.
    // This is what we want, since if we have animations overridden by
    // !important rules, we don't want to send them to the compositor.
    MOZ_ASSERT(anim->CascadeLevel() !=
                 EffectCompositor::CascadeLevel::Animations ||
               !EffectSet::GetEffectSet(styleFrame)->PropertiesWithImportantRules()
                  .HasProperty(aProperty),
               "GetEffectiveAnimationOfProperty already tested the property "
               "is not overridden by !important rules");

    // Don't add animations that are pending if their timeline does not
    // track wallclock time. This is because any pending animations on layers
    // will have their start time updated with the current wallclock time.
    // If we can't convert that wallclock time back to an equivalent timeline
    // time, we won't be able to update the content animation and it will end
    // up being out of sync with the layer animation.
    //
    // Currently this only happens when the timeline is driven by a refresh
    // driver under test control. In this case, the next time the refresh
    // driver is advanced it will trigger any pending animations.
    if (anim->Pending() &&
        (anim->GetTimeline() && !anim->GetTimeline()->TracksWallclockTime())) {
      continue;
    }

    AddAnimationForProperty(aFrame, *property, anim, aAnimationInfo, data, aPending);
    keyframeEffect->SetIsRunningOnCompositor(aProperty, true);
  }
}

static bool
GenerateAndPushTextMask(nsIFrame* aFrame, gfxContext* aContext,
                        const nsRect& aFillRect, nsDisplayListBuilder* aBuilder)
{
  if (aBuilder->IsForGenerateGlyphMask() ||
      aBuilder->IsForPaintingSelectionBG()) {
    return false;
  }

  // The main function of enabling background-clip:text property value.
  // When a nsDisplayBackgroundImage detects "text" bg-clip style, it will call
  // this function to
  // 1. Paint background color of the selection text if any.
  // 2. Generate a mask by all descendant text frames
  // 3. Push the generated mask into aContext.
  //
  // TBD: we actually generate display list of aFrame twice here. It's better
  // to reuse the same display list and paint that one twice, one for selection
  // background, one for generating text mask.

  gfxContext* sourceCtx = aContext;
  LayoutDeviceRect bounds =
    LayoutDeviceRect::FromAppUnits(aFillRect,
                                   aFrame->PresContext()->AppUnitsPerDevPixel());

  {
    // Paint text selection background into sourceCtx.
    gfxContextMatrixAutoSaveRestore save(sourceCtx);
    sourceCtx->SetMatrix(sourceCtx->CurrentMatrix().PreTranslate(bounds.TopLeft().ToUnknownPoint()));

    nsLayoutUtils::PaintFrame(aContext, aFrame,
                              nsRect(nsPoint(0, 0), aFrame->GetSize()),
                              NS_RGB(255, 255, 255),
                              nsDisplayListBuilderMode::PAINTING_SELECTION_BACKGROUND);
  }

  // Evaluate required surface size.
  IntRect drawRect =
    RoundedOut(ToRect(sourceCtx->GetClipExtents(gfxContext::eDeviceSpace)));

  Matrix currentMatrix = sourceCtx->CurrentMatrix();
  Matrix maskTransform = currentMatrix *
                         Matrix::Translation(-drawRect.x, -drawRect.y);
  maskTransform.Invert();

  // Create a mask surface.
  RefPtr<DrawTarget> sourceTarget = sourceCtx->GetDrawTarget();
  RefPtr<DrawTarget> maskDT =
    sourceTarget->CreateClippedDrawTarget(drawRect.Size(),
                                          maskTransform * currentMatrix,
                                          SurfaceFormat::A8);
  if (!maskDT || !maskDT->IsValid()) {
    return false;
  }
  RefPtr<gfxContext> maskCtx = gfxContext::CreatePreservingTransformOrNull(maskDT);
  MOZ_ASSERT(maskCtx);
  maskCtx->SetMatrix(Matrix::Translation(bounds.TopLeft().ToUnknownPoint()) *
                     currentMatrix *
                     Matrix::Translation(-drawRect.TopLeft()));

  // Shade text shape into mask A8 surface.
  nsLayoutUtils::PaintFrame(maskCtx, aFrame,
                            nsRect(nsPoint(0, 0), aFrame->GetSize()),
                            NS_RGB(255, 255, 255),
                            nsDisplayListBuilderMode::GENERATE_GLYPH);

  // Push the generated mask into aContext, so that the caller can pop and
  // blend with it.
  RefPtr<SourceSurface> maskSurface = maskDT->Snapshot();
  sourceCtx->PushGroupForBlendBack(gfxContentType::COLOR_ALPHA, 1.0, maskSurface, maskTransform);

  return true;
}

/* static */ void
nsDisplayListBuilder::AddAnimationsAndTransitionsToLayer(Layer* aLayer,
                                                         nsDisplayListBuilder* aBuilder,
                                                         nsDisplayItem* aItem,
                                                         nsIFrame* aFrame,
                                                         nsCSSPropertyID aProperty)
{
  MOZ_ASSERT(nsCSSProps::PropHasFlags(aProperty,
                                      CSSPropFlags::CanAnimateOnCompositor),
             "inconsistent property flags");

  // This function can be called in two ways:  from
  // nsDisplay*::BuildLayer while constructing a layer (with all
  // pointers non-null), or from RestyleManager's handling of
  // UpdateOpacityLayer/UpdateTransformLayer hints.
  MOZ_ASSERT(!aBuilder == !aItem,
             "should only be called in two configurations, with both "
             "aBuilder and aItem, or with neither");
  MOZ_ASSERT(!aItem || aFrame == aItem->Frame(), "frame mismatch");

  // Only send animations to a layer that is actually using
  // off-main-thread compositing.
  LayersBackend backend = aLayer->Manager()->GetBackendType();
  if (!(backend == layers::LayersBackend::LAYERS_CLIENT ||
        backend == layers::LayersBackend::LAYERS_WR)) {
    return;
  }

  bool pending = !aBuilder;
  AnimationInfo& animationInfo = aLayer->GetAnimationInfo();
  AddAnimationsForProperty(aFrame, aBuilder, aItem, aProperty,
                           animationInfo, pending, false);
  animationInfo.TransferMutatedFlagToLayer(aLayer);
}

nsDisplayItem*
nsDisplayListBuilder::MergeItems(nsTArray<nsDisplayItem*>& aMergedItems)
{
  // For merging, we create a temporary item by cloning the last item of the
  // mergeable items list. This ensures that the temporary item will have the
  // correct frame and bounds.
  nsDisplayItem* merged = nullptr;

  for (nsDisplayItem* item : Reversed(aMergedItems)) {
    MOZ_ASSERT(item);

    if (!merged) {
      // Create the temporary item.
      merged = item->Clone(this);
      MOZ_ASSERT(merged);

      AddTemporaryItem(merged);
    } else {
      // Merge the item properties (frame/bounds/etc) with the previously
      // created temporary item.
      MOZ_ASSERT(merged->CanMerge(item));
      merged->Merge(item);
    }

    // Create nsDisplayWrapList that points to the internal display list of the
    // item we are merging. This nsDisplayWrapList is added to the display list
    // of the temporary item.
    merged->MergeDisplayListFromItem(this, item);
  }

  return merged;
}

void
nsDisplayListBuilder::AutoCurrentActiveScrolledRootSetter::SetCurrentActiveScrolledRoot(
    const ActiveScrolledRoot* aActiveScrolledRoot)
{
  MOZ_ASSERT(!mUsed);

  // Set the builder's mCurrentActiveScrolledRoot.
  mBuilder->mCurrentActiveScrolledRoot = aActiveScrolledRoot;

  // We also need to adjust the builder's mCurrentContainerASR.
  // mCurrentContainerASR needs to be an ASR that all the container's
  // contents have finite bounds with respect to. If aActiveScrolledRoot
  // is an ancestor ASR of mCurrentContainerASR, that means we need to
  // set mCurrentContainerASR to aActiveScrolledRoot, because otherwise
  // the items that will be created with aActiveScrolledRoot wouldn't
  // have finite bounds with respect to mCurrentContainerASR. There's one
  // exception, in the case where there's a content clip on the builder
  // that is scrolled by a descendant ASR of aActiveScrolledRoot. This
  // content clip will clip all items that are created while this
  // AutoCurrentActiveScrolledRootSetter exists. This means that the items
  // created during our lifetime will have finite bounds with respect to
  // the content clip's ASR, even if the items' actual ASR is an ancestor
  // of that. And it also means that mCurrentContainerASR only needs to be
  // set to the content clip's ASR and not all the way to aActiveScrolledRoot.
  // This case is tested by fixed-pos-scrolled-clip-opacity-layerize.html
  // and fixed-pos-scrolled-clip-opacity-inside-layerize.html.

  // finiteBoundsASR is the leafmost ASR that all items created during
  // object's lifetime have finite bounds with respect to.
  const ActiveScrolledRoot* finiteBoundsASR = ActiveScrolledRoot::PickDescendant(
    mContentClipASR, aActiveScrolledRoot);

  // mCurrentContainerASR is adjusted so that it's still an ancestor of
  // finiteBoundsASR.
  mBuilder->mCurrentContainerASR = ActiveScrolledRoot::PickAncestor(
    mBuilder->mCurrentContainerASR, finiteBoundsASR);

  // If we are entering out-of-flow content inside a CSS filter, mark
  // scroll frames wrt. which the content is fixed as containing such content.
  if (mBuilder->mFilterASR &&
      ActiveScrolledRoot::IsAncestor(aActiveScrolledRoot, mBuilder->mFilterASR)) {
    for (const ActiveScrolledRoot* asr = mBuilder->mFilterASR;
         asr && asr != aActiveScrolledRoot;
         asr = asr->mParent) {
      asr->mScrollableFrame->SetHasOutOfFlowContentInsideFilter();
    }
  }

  mUsed = true;
}

void
nsDisplayListBuilder::AutoCurrentActiveScrolledRootSetter::InsertScrollFrame(nsIScrollableFrame* aScrollableFrame)
{
  MOZ_ASSERT(!mUsed);
  size_t descendantsEndIndex = mBuilder->mActiveScrolledRoots.Length();
  const ActiveScrolledRoot* parentASR = mBuilder->mCurrentActiveScrolledRoot;
  const ActiveScrolledRoot* asr = mBuilder->AllocateActiveScrolledRoot(parentASR, aScrollableFrame);
  mBuilder->mCurrentActiveScrolledRoot = asr;

  // All child ASRs of parentASR that were created while this
  // AutoCurrentActiveScrolledRootSetter object was on the stack belong to us
  // now. Reparent them to asr.
  for (size_t i = mDescendantsStartIndex; i < descendantsEndIndex; i++) {
    ActiveScrolledRoot* descendantASR = mBuilder->mActiveScrolledRoots[i];
    if (ActiveScrolledRoot::IsAncestor(parentASR, descendantASR)) {
      descendantASR->IncrementDepth();
      if (descendantASR->mParent == parentASR) {
        descendantASR->mParent = asr;
      }
    }
  }

  mUsed = true;
}

nsDisplayListBuilder::nsDisplayListBuilder(nsIFrame* aReferenceFrame,
    nsDisplayListBuilderMode aMode, bool aBuildCaret, bool aRetainingDisplayList)
    : mReferenceFrame(aReferenceFrame),
      mIgnoreScrollFrame(nullptr),
      mCompositorHitTestInfo(nullptr),
      mCurrentTableItem(nullptr),
      mCurrentActiveScrolledRoot(nullptr),
      mCurrentContainerASR(nullptr),
      mCurrentFrame(aReferenceFrame),
      mCurrentReferenceFrame(aReferenceFrame),
      mRootAGR(AnimatedGeometryRoot::CreateAGRForFrame(aReferenceFrame, nullptr, true, aRetainingDisplayList)),
      mCurrentAGR(mRootAGR),
      mUsedAGRBudget(0),
      mDirtyRect(-1,-1,-1,-1),
      mGlassDisplayItem(nullptr),
      mScrollInfoItemsForHoisting(nullptr),
      mActiveScrolledRootForRootScrollframe(nullptr),
      mMode(aMode),
      mCurrentScrollParentId(FrameMetrics::NULL_SCROLL_ID),
      mCurrentScrollbarTarget(FrameMetrics::NULL_SCROLL_ID),
      mSVGEffectsBuildingDepth(0),
      mFilterASR(nullptr),
      mContainsBlendMode(false),
      mIsBuildingScrollbar(false),
      mCurrentScrollbarWillHaveLayer(false),
      mBuildCaret(aBuildCaret),
      mRetainingDisplayList(aRetainingDisplayList),
      mPartialUpdate(false),
      mIgnoreSuppression(false),
      mIsAtRootOfPseudoStackingContext(false),
      mIncludeAllOutOfFlows(false),
      mDescendIntoSubdocuments(true),
      mSelectedFramesOnly(false),
      mAllowMergingAndFlattening(true),
      mWillComputePluginGeometry(false),
      mInTransform(false),
      mInPageSequence(false),
      mIsInChromePresContext(false),
      mSyncDecodeImages(false),
      mIsPaintingToWindow(false),
      mIsCompositingCheap(false),
      mContainsPluginItem(false),
      mAncestorHasApzAwareEventHandler(false),
      mHaveScrollableDisplayPort(false),
      mWindowDraggingAllowed(false),
      mIsBuildingForPopup(nsLayoutUtils::IsPopup(aReferenceFrame)),
      mForceLayerForScrollParent(false),
      mAsyncPanZoomEnabled(nsLayoutUtils::AsyncPanZoomEnabled(aReferenceFrame)),
      mBuildingInvisibleItems(false),
      mHitTestIsForVisibility(false),
      mIsBuilding(false),
      mInInvalidSubtree(false),
      mDisablePartialUpdates(false),
      mPartialBuildFailed(false)
{
  MOZ_COUNT_CTOR(nsDisplayListBuilder);

  mBuildCompositorHitTestInfo = mAsyncPanZoomEnabled && IsForPainting();

  mLessEventRegionItems = gfxPrefs::LessEventRegionItems();

  nsPresContext* pc = aReferenceFrame->PresContext();
  nsIPresShell *shell = pc->PresShell();
  if (pc->IsRenderingOnlySelection()) {
    nsCOMPtr<nsISelectionController> selcon(do_QueryInterface(shell));
    if (selcon) {
      mBoundingSelection =
        selcon->GetSelection(nsISelectionController::SELECTION_NORMAL);
    }
  }

  static_assert(static_cast<uint32_t>(DisplayItemType::TYPE_MAX) < (1 << TYPE_BITS),
                "Check TYPE_MAX should not overflow");
}

void
nsDisplayListBuilder::BeginFrame()
{
  nsCSSRendering::BeginFrameTreesLocked();
  mCurrentAGR = mRootAGR;
  mFrameToAnimatedGeometryRootMap.Put(mReferenceFrame, mRootAGR);

  mIsPaintingToWindow = false;
  mIgnoreSuppression = false;
  mInTransform = false;
  mSyncDecodeImages = false;
}

void
nsDisplayListBuilder::EndFrame()
{
  NS_ASSERTION(!mInInvalidSubtree, "Someone forgot to cleanup mInInvalidSubtree!");
  mFrameToAnimatedGeometryRootMap.Clear();
  mActiveScrolledRoots.Clear();
  FreeClipChains();
  FreeTemporaryItems();
  nsCSSRendering::EndFrameTreesLocked();

  MOZ_ASSERT(!mCompositorHitTestInfo);
}

void
nsDisplayListBuilder::MarkFrameForDisplay(nsIFrame* aFrame, nsIFrame* aStopAtFrame)
{
  mFramesMarkedForDisplay.AppendElement(aFrame);
  for (nsIFrame* f = aFrame; f;
       f = nsLayoutUtils::GetParentOrPlaceholderForCrossDoc(f)) {
    if (f->GetStateBits() & NS_FRAME_FORCE_DISPLAY_LIST_DESCEND_INTO)
      return;
    f->AddStateBits(NS_FRAME_FORCE_DISPLAY_LIST_DESCEND_INTO);
    if (f == aStopAtFrame) {
      // we've reached a frame that we know will be painted, so we can stop.
      break;
    }
  }
}

void
nsDisplayListBuilder::AddFrameMarkedForDisplayIfVisible(nsIFrame* aFrame)
{
  mFramesMarkedForDisplayIfVisible.AppendElement(aFrame);
}

void
nsDisplayListBuilder::MarkFrameForDisplayIfVisible(nsIFrame* aFrame, nsIFrame* aStopAtFrame)
{
  AddFrameMarkedForDisplayIfVisible(aFrame);
  for (nsIFrame* f = aFrame; f;
       f = nsLayoutUtils::GetParentOrPlaceholderForCrossDoc(f)) {
    if (f->ForceDescendIntoIfVisible())
      return;
    f->SetForceDescendIntoIfVisible(true);
    if (f == aStopAtFrame) {
      // we've reached a frame that we know will be painted, so we can stop.
      break;
    }
  }
}

bool nsDisplayListBuilder::NeedToForceTransparentSurfaceForItem(nsDisplayItem* aItem)
{
  return aItem == mGlassDisplayItem || aItem->ClearsBackground();
}

AnimatedGeometryRoot*
nsDisplayListBuilder::WrapAGRForFrame(nsIFrame* aAnimatedGeometryRoot,
                                      bool aIsAsync,
                                      AnimatedGeometryRoot* aParent /* = nullptr */)
{
  DebugOnly<bool> dummy;
  MOZ_ASSERT(IsAnimatedGeometryRoot(aAnimatedGeometryRoot, dummy) == AGR_YES);

  RefPtr<AnimatedGeometryRoot> result;
  if (!mFrameToAnimatedGeometryRootMap.Get(aAnimatedGeometryRoot, &result)) {
    MOZ_ASSERT(nsLayoutUtils::IsAncestorFrameCrossDoc(RootReferenceFrame(), aAnimatedGeometryRoot));
    RefPtr<AnimatedGeometryRoot> parent = aParent;
    if (!parent) {
      nsIFrame* parentFrame = nsLayoutUtils::GetCrossDocParentFrame(aAnimatedGeometryRoot);
      if (parentFrame) {
        bool isAsync;
        nsIFrame* parentAGRFrame = FindAnimatedGeometryRootFrameFor(parentFrame, isAsync);
        parent = WrapAGRForFrame(parentAGRFrame, isAsync);
      }
    }
    result = AnimatedGeometryRoot::CreateAGRForFrame(aAnimatedGeometryRoot, parent, aIsAsync, IsRetainingDisplayList());
    mFrameToAnimatedGeometryRootMap.Put(aAnimatedGeometryRoot, result);
  }
  MOZ_ASSERT(!aParent || result->mParentAGR == aParent);
  return result;
}

AnimatedGeometryRoot*
nsDisplayListBuilder::AnimatedGeometryRootForASR(const ActiveScrolledRoot* aASR)
{
  if (!aASR) {
    return GetRootAnimatedGeometryRoot();
  }
  nsIFrame* scrolledFrame = aASR->mScrollableFrame->GetScrolledFrame();
  return FindAnimatedGeometryRootFor(scrolledFrame);
}

AnimatedGeometryRoot*
nsDisplayListBuilder::FindAnimatedGeometryRootFor(nsIFrame* aFrame)
{
  if (!IsPaintingToWindow()) {
    return mRootAGR;
  }
  if (aFrame == mCurrentFrame) {
    return mCurrentAGR;
  }
  RefPtr<AnimatedGeometryRoot> result;
  if (mFrameToAnimatedGeometryRootMap.Get(aFrame, &result)) {
    return result;
  }

  bool isAsync;
  nsIFrame* agrFrame = FindAnimatedGeometryRootFrameFor(aFrame, isAsync);
  result = WrapAGRForFrame(agrFrame, isAsync);
  mFrameToAnimatedGeometryRootMap.Put(aFrame, result);
  return result;
}

AnimatedGeometryRoot*
nsDisplayListBuilder::FindAnimatedGeometryRootFor(nsDisplayItem* aItem)
{
  if (aItem->ShouldFixToViewport(this)) {
    // Make its active scrolled root be the active scrolled root of
    // the enclosing viewport, since it shouldn't be scrolled by scrolled
    // frames in its document. InvalidateFixedBackgroundFramesFromList in
    // nsGfxScrollFrame will not repaint this item when scrolling occurs.
    nsIFrame* viewportFrame = nsLayoutUtils::GetClosestFrameOfType(
      aItem->Frame(), LayoutFrameType::Viewport, RootReferenceFrame());
    if (viewportFrame) {
      return FindAnimatedGeometryRootFor(viewportFrame);
    }
  }
  return FindAnimatedGeometryRootFor(aItem->Frame());
}

bool nsDisplayListBuilder::MarkOutOfFlowFrameForDisplay(nsIFrame* aDirtyFrame,
                                                        nsIFrame* aFrame)
{
  MOZ_ASSERT(aFrame->GetParent() == aDirtyFrame);
  nsRect dirty;
  nsRect visible =
    OutOfFlowDisplayData::ComputeVisibleRectForFrame(this, aFrame, GetVisibleRect(),
                                                     GetDirtyRect(), &dirty);
  if (!(aFrame->GetStateBits() & NS_FRAME_FORCE_DISPLAY_LIST_DESCEND_INTO) &&
      visible.IsEmpty()) {
    return false;
  }

  // Only MarkFrameForDisplay if we're dirty. If this is a nested out-of-flow frame, then it will
  // also mark any outer frames to ensure that building reaches the dirty feame.
  if (!dirty.IsEmpty() ||
      aFrame->ForceDescendIntoIfVisible()) {
    MarkFrameForDisplay(aFrame, aDirtyFrame);
  }

  return true;
}

static void UnmarkFrameForDisplay(nsIFrame* aFrame, nsIFrame* aStopAtFrame) {
  for (nsIFrame* f = aFrame; f;
       f = nsLayoutUtils::GetParentOrPlaceholderForCrossDoc(f)) {
    if (!(f->GetStateBits() & NS_FRAME_FORCE_DISPLAY_LIST_DESCEND_INTO))
      return;
    f->RemoveStateBits(NS_FRAME_FORCE_DISPLAY_LIST_DESCEND_INTO);
    if (f == aStopAtFrame) {
      // we've reached a frame that we know will be painted, so we can stop.
      break;
    }
  }
}

static void UnmarkFrameForDisplayIfVisible(nsIFrame* aFrame) {
  for (nsIFrame* f = aFrame; f;
       f = nsLayoutUtils::GetParentOrPlaceholderForCrossDoc(f)) {
    if (!f->ForceDescendIntoIfVisible())
      return;
    f->SetForceDescendIntoIfVisible(false);
  }
}

nsDisplayListBuilder::~nsDisplayListBuilder() {
  NS_ASSERTION(mFramesMarkedForDisplay.Length() == 0,
               "All frames should have been unmarked");
  NS_ASSERTION(mFramesWithOOFData.Length() == 0,
               "All OOF data should have been removed");
  NS_ASSERTION(mPresShellStates.Length() == 0,
               "All presshells should have been exited");
  NS_ASSERTION(!mCurrentTableItem, "No table item should be active");

  for (DisplayItemClipChain* c : mClipChainsToDestroy) {
    c->DisplayItemClipChain::~DisplayItemClipChain();
  }

  MOZ_COUNT_DTOR(nsDisplayListBuilder);
}

uint32_t
nsDisplayListBuilder::GetBackgroundPaintFlags() {
  uint32_t flags = 0;
  if (mSyncDecodeImages) {
    flags |= nsCSSRendering::PAINTBG_SYNC_DECODE_IMAGES;
  }
  if (mIsPaintingToWindow) {
    flags |= nsCSSRendering::PAINTBG_TO_WINDOW;
  }
  return flags;
}

void
nsDisplayListBuilder::SubtractFromVisibleRegion(nsRegion* aVisibleRegion,
                                                const nsRegion& aRegion)
{
  if (aRegion.IsEmpty())
    return;

  nsRegion tmp;
  tmp.Sub(*aVisibleRegion, aRegion);
  // Don't let *aVisibleRegion get too complex, but don't let it fluff out
  // to its bounds either, which can be very bad (see bug 516740).
  // Do let aVisibleRegion get more complex if by doing so we reduce its
  // area by at least half.
  if (GetAccurateVisibleRegions() || tmp.GetNumRects() <= 15 ||
      tmp.Area() <= aVisibleRegion->Area()/2) {
    *aVisibleRegion = tmp;
  }
}

nsCaret *
nsDisplayListBuilder::GetCaret() {
  RefPtr<nsCaret> caret = CurrentPresShellState()->mPresShell->GetCaret();
  return caret;
}

void
nsDisplayListBuilder::IncrementPresShellPaintCount(nsIPresShell* aPresShell)
{
  if (mIsPaintingToWindow) {
    mReferenceFrame->AddPaintedPresShell(aPresShell);
    aPresShell->IncrementPaintCount();
  }
}

void
nsDisplayListBuilder::EnterPresShell(nsIFrame* aReferenceFrame,
                                     bool aPointerEventsNoneDoc)
{
  PresShellState* state = mPresShellStates.AppendElement();
  state->mPresShell = aReferenceFrame->PresShell();
  state->mCaretFrame = nullptr;
  state->mFirstFrameMarkedForDisplay = mFramesMarkedForDisplay.Length();
  state->mFirstFrameWithOOFData = mFramesWithOOFData.Length();

  nsIScrollableFrame* sf = state->mPresShell->GetRootScrollFrameAsScrollable();
  if (sf && IsInSubdocument()) {
    // We are forcing a rebuild of nsDisplayCanvasBackgroundColor to make sure
    // that the canvas background color will be set correctly, and that only one
    // unscrollable item will be created.
    // This is done to avoid, for example, a case where only scrollbar frames
    // are invalidated - we would skip creating nsDisplayCanvasBackgroundColor
    // and possibly end up with an extra nsDisplaySolidColor item.
    // We skip this for the root document, since we don't want to use
    // MarkFrameForDisplayIfVisible before ComputeRebuildRegion. We'll
    // do it manually there.
    nsCanvasFrame* canvasFrame = do_QueryFrame(sf->GetScrolledFrame());
    if (canvasFrame) {
      MarkFrameForDisplayIfVisible(canvasFrame, aReferenceFrame);
    }
  }

#ifdef DEBUG
  state->mAutoLayoutPhase.emplace(aReferenceFrame->PresContext(), eLayoutPhase_DisplayListBuilding);
#endif

  state->mPresShell->UpdateCanvasBackground();

  bool buildCaret = mBuildCaret;
  if (mIgnoreSuppression || !state->mPresShell->IsPaintingSuppressed()) {
    state->mIsBackgroundOnly = false;
  } else {
    state->mIsBackgroundOnly = true;
    buildCaret = false;
  }

  bool pointerEventsNone = aPointerEventsNoneDoc;
  if (IsInSubdocument()) {
    pointerEventsNone |= mPresShellStates[mPresShellStates.Length() - 2].mInsidePointerEventsNoneDoc;
  }
  state->mInsidePointerEventsNoneDoc = pointerEventsNone;

  if (!buildCaret)
    return;

  RefPtr<nsCaret> caret = state->mPresShell->GetCaret();
  state->mCaretFrame = caret->GetPaintGeometry(&state->mCaretRect);
  if (state->mCaretFrame) {
    MarkFrameForDisplay(state->mCaretFrame, aReferenceFrame);
  }

  nsPresContext* pc = aReferenceFrame->PresContext();
  nsCOMPtr<nsIDocShell> docShell = pc->GetDocShell();
  if (docShell) {
    docShell->GetWindowDraggingAllowed(&mWindowDraggingAllowed);
  }
  mIsInChromePresContext = pc->IsChrome();
}

// A non-blank paint is a paint that does not just contain the canvas background.
static bool
DisplayListIsNonBlank(nsDisplayList* aList)
{
  for (nsDisplayItem* i = aList->GetBottom(); i != nullptr; i = i->GetAbove()) {
    switch (i->GetType()) {
      case DisplayItemType::TYPE_COMPOSITOR_HITTEST_INFO:
      case DisplayItemType::TYPE_CANVAS_BACKGROUND_COLOR:
      case DisplayItemType::TYPE_CANVAS_BACKGROUND_IMAGE:
        continue;
      case DisplayItemType::TYPE_SOLID_COLOR:
      case DisplayItemType::TYPE_BACKGROUND:
      case DisplayItemType::TYPE_BACKGROUND_COLOR:
        if (i->Frame()->IsCanvasFrame()) {
          continue;
        }
        return true;
      default:
        return true;
    }
  }
  return false;
}

void
nsDisplayListBuilder::LeavePresShell(nsIFrame* aReferenceFrame, nsDisplayList* aPaintedContents)
{
  NS_ASSERTION(CurrentPresShellState()->mPresShell ==
      aReferenceFrame->PresShell(),
      "Presshell mismatch");

  if (mIsPaintingToWindow) {
    nsPresContext* pc = aReferenceFrame->PresContext();
    if (!pc->HadNonBlankPaint()) {
      if (!CurrentPresShellState()->mIsBackgroundOnly &&
          DisplayListIsNonBlank(aPaintedContents)) {
        pc->NotifyNonBlankPaint();
      }
    }
  }

  ResetMarkedFramesForDisplayList(aReferenceFrame);
  mPresShellStates.SetLength(mPresShellStates.Length() - 1);

  if (!mPresShellStates.IsEmpty()) {
    nsPresContext* pc = CurrentPresContext();
    nsCOMPtr<nsIDocShell> docShell = pc->GetDocShell();
    if (docShell) {
      docShell->GetWindowDraggingAllowed(&mWindowDraggingAllowed);
    }
    mIsInChromePresContext = pc->IsChrome();
  } else {
    mCurrentAGR = mRootAGR;

    for (uint32_t i = 0;
       i < mFramesMarkedForDisplayIfVisible.Length(); ++i) {
      UnmarkFrameForDisplayIfVisible(mFramesMarkedForDisplayIfVisible[i]);
    }
    mFramesMarkedForDisplayIfVisible.SetLength(0);
  }
}

void
nsDisplayListBuilder::FreeClipChains()
{
  // Iterate the clip chains from newest to oldest (forward
  // iteration), so that we destroy descendants first which
  // will drop the ref count on their ancestors.
  auto it = mClipChainsToDestroy.begin();

  while(it != mClipChainsToDestroy.end()) {
    DisplayItemClipChain* clip = *it;

    if (!clip->mRefCount) {
      mClipDeduplicator.erase(clip);
      it = mClipChainsToDestroy.erase(it);
      clip->DisplayItemClipChain::~DisplayItemClipChain();
      Destroy(DisplayItemType::TYPE_ZERO, clip);
    } else {
      ++it;
    }
  }
}

void
nsDisplayListBuilder::FreeTemporaryItems()
{
  for (nsDisplayItem* i : mTemporaryItems) {
    // Temporary display items are not added to the frames.
    MOZ_ASSERT(i->Frame());
    i->RemoveFrame(i->Frame());
    i->Destroy(this);
  }

  mTemporaryItems.Clear();
}

void
nsDisplayListBuilder::ResetMarkedFramesForDisplayList(nsIFrame* aReferenceFrame)
{
  // Unmark and pop off the frames marked for display in this pres shell.
  uint32_t firstFrameForShell = CurrentPresShellState()->mFirstFrameMarkedForDisplay;
  for (uint32_t i = firstFrameForShell;
       i < mFramesMarkedForDisplay.Length(); ++i) {
    UnmarkFrameForDisplay(mFramesMarkedForDisplay[i], aReferenceFrame);
  }
  mFramesMarkedForDisplay.SetLength(firstFrameForShell);

  firstFrameForShell = CurrentPresShellState()->mFirstFrameWithOOFData;
  for (uint32_t i = firstFrameForShell; i < mFramesWithOOFData.Length(); ++i) {
    mFramesWithOOFData[i]->DeleteProperty(OutOfFlowDisplayDataProperty());
  }
  mFramesWithOOFData.SetLength(firstFrameForShell);
}

void
nsDisplayListBuilder::ClearFixedBackgroundDisplayData()
{
  CurrentPresShellState()->mFixedBackgroundDisplayData = Nothing();
}

void
nsDisplayListBuilder::MarkFramesForDisplayList(nsIFrame* aDirtyFrame,
                                               const nsFrameList& aFrames)
{
  bool markedFrames = false;
  for (nsIFrame* e : aFrames) {
    // Skip the AccessibleCaret frame when building no caret.
    if (!IsBuildingCaret()) {
      nsIContent* content = e->GetContent();
      if (content && content->IsInNativeAnonymousSubtree() && content->IsElement()) {
        auto classList = content->AsElement()->ClassList();
        if (classList->Contains(NS_LITERAL_STRING("moz-accessiblecaret"))) {
          continue;
        }
      }
    }
    if (MarkOutOfFlowFrameForDisplay(aDirtyFrame, e)) {
      markedFrames = true;
    }
  }

  if (markedFrames) {
    // mClipState.GetClipChainForContainingBlockDescendants can return pointers
    // to objects on the stack, so we need to clone the chain.
    const DisplayItemClipChain* clipChain =
      CopyWholeChain(mClipState.GetClipChainForContainingBlockDescendants());
    const DisplayItemClipChain* combinedClipChain = mClipState.GetCurrentCombinedClipChain(this);
    const ActiveScrolledRoot* asr = mCurrentActiveScrolledRoot;
    OutOfFlowDisplayData* data = new OutOfFlowDisplayData(clipChain, combinedClipChain, asr, GetVisibleRect(), GetDirtyRect());
    aDirtyFrame->SetProperty(nsDisplayListBuilder::OutOfFlowDisplayDataProperty(), data);
    mFramesWithOOFData.AppendElement(aDirtyFrame);
  }

  if (!aDirtyFrame->GetParent()) {
    // This is the viewport frame of aDirtyFrame's presshell.
    // Store the current display data so that it can be used for fixed
    // background images.
    NS_ASSERTION(CurrentPresShellState()->mPresShell ==
        aDirtyFrame->PresShell(),
        "Presshell mismatch");
    MOZ_ASSERT(!CurrentPresShellState()->mFixedBackgroundDisplayData,
               "already traversed this presshell's root frame?");

    const DisplayItemClipChain* clipChain =
      CopyWholeChain(mClipState.GetClipChainForContainingBlockDescendants());
    const DisplayItemClipChain* combinedClipChain = mClipState.GetCurrentCombinedClipChain(this);
    const ActiveScrolledRoot* asr = mCurrentActiveScrolledRoot;
    CurrentPresShellState()->mFixedBackgroundDisplayData.emplace(
      clipChain, combinedClipChain, asr, GetVisibleRect(), GetDirtyRect());
  }
}

/**
 * Mark all preserve-3d children with
 * NS_FRAME_FORCE_DISPLAY_LIST_DESCEND_INTO to make sure
 * nsFrame::BuildDisplayListForChild() would visit them.  Also compute
 * dirty rect for preserve-3d children.
 *
 * @param aDirtyFrame is the frame to mark children extending context.
 */
void
nsDisplayListBuilder::MarkPreserve3DFramesForDisplayList(nsIFrame* aDirtyFrame)
{
  AutoTArray<nsIFrame::ChildList,4> childListArray;
  aDirtyFrame->GetChildLists(&childListArray);
  nsIFrame::ChildListArrayIterator lists(childListArray);
  for (; !lists.IsDone(); lists.Next()) {
    nsFrameList::Enumerator childFrames(lists.CurrentList());
    for (; !childFrames.AtEnd(); childFrames.Next()) {
      nsIFrame *child = childFrames.get();
      if (child->Combines3DTransformWithAncestors()) {
        MarkFrameForDisplay(child, aDirtyFrame);
      }
    }
  }
}

uint32_t gDisplayItemSizes[static_cast<uint32_t>(DisplayItemType::TYPE_MAX)] = { 0 };

void*
nsDisplayListBuilder::Allocate(size_t aSize, DisplayItemType aType)
{
  size_t roundedUpSize = RoundUpPow2(aSize);
  uint_fast8_t type = FloorLog2Size(roundedUpSize);

  MOZ_RELEASE_ASSERT(gDisplayItemSizes[static_cast<uint32_t>(aType)] == type ||
                     gDisplayItemSizes[static_cast<uint32_t>(aType)] == 0);
  gDisplayItemSizes[static_cast<uint32_t>(aType)] = type;
  return mPool.AllocateByCustomID(type, roundedUpSize);
}

void
nsDisplayListBuilder::Destroy(DisplayItemType aType, void* aPtr)
{
  mPool.FreeByCustomID(gDisplayItemSizes[static_cast<uint32_t>(aType)], aPtr);
}

ActiveScrolledRoot*
nsDisplayListBuilder::AllocateActiveScrolledRoot(const ActiveScrolledRoot* aParent,
                                                 nsIScrollableFrame* aScrollableFrame)
{
  RefPtr<ActiveScrolledRoot> asr = ActiveScrolledRoot::CreateASRForFrame(aParent, aScrollableFrame, IsRetainingDisplayList());
  mActiveScrolledRoots.AppendElement(asr);
  return asr;
}

const DisplayItemClipChain*
nsDisplayListBuilder::AllocateDisplayItemClipChain(const DisplayItemClip& aClip,
                                                   const ActiveScrolledRoot* aASR,
                                                   const DisplayItemClipChain* aParent)
{
  MOZ_ASSERT(!(aParent && aParent->mOnStack));
  void* p = Allocate(sizeof(DisplayItemClipChain), DisplayItemType::TYPE_ZERO);
  DisplayItemClipChain* c = new (KnownNotNull, p) DisplayItemClipChain(aClip, aASR, aParent);
#ifdef DEBUG
  c->mOnStack = false;
#endif
  auto result = mClipDeduplicator.insert(c);
  if (!result.second) {
    // An equivalent clip chain item was already created, so let's return that
    // instead. Destroy the one we just created.
    // Note that this can cause clip chains from different coordinate systems to
    // collapse into the same clip chain object, because clip chains do not keep
    // track of the reference frame that they were created in.
    c->DisplayItemClipChain::~DisplayItemClipChain();
    Destroy(DisplayItemType::TYPE_ZERO, c);
    return *(result.first);
  }
  mClipChainsToDestroy.emplace_front(c);
  return c;
}

struct ClipChainItem {
  DisplayItemClip clip;
  const ActiveScrolledRoot* asr;
};

const DisplayItemClipChain*
nsDisplayListBuilder::CreateClipChainIntersection(const DisplayItemClipChain* aAncestor,
                                                  const DisplayItemClipChain* aLeafClip1,
                                                  const DisplayItemClipChain* aLeafClip2)
{
  AutoTArray<ClipChainItem,8> intersectedClips;

  const DisplayItemClipChain* clip1 = aLeafClip1;
  const DisplayItemClipChain* clip2 = aLeafClip2;

  const ActiveScrolledRoot* asr =
    ActiveScrolledRoot::PickDescendant(clip1 ? clip1->mASR : nullptr,
                                       clip2 ? clip2->mASR : nullptr);

  // Build up the intersection from the leaf to the root and put it into
  // intersectedClips. The loop below will convert intersectedClips into an
  // actual DisplayItemClipChain.
  // (We need to do this in two passes because we need the parent clip in order
  // to create the DisplayItemClipChain object, but the parent clip has not
  // been created at that point.)
  while (!aAncestor || asr != aAncestor->mASR) {
    if (clip1 && clip1->mASR == asr) {
      if (clip2 && clip2->mASR == asr) {
        DisplayItemClip intersection = clip1->mClip;
        intersection.IntersectWith(clip2->mClip);
        intersectedClips.AppendElement(ClipChainItem{ intersection, asr });
        clip2 = clip2->mParent;
      } else {
        intersectedClips.AppendElement(ClipChainItem{ clip1->mClip, asr });
      }
      clip1 = clip1->mParent;
    } else if (clip2 && clip2->mASR == asr) {
      intersectedClips.AppendElement(ClipChainItem{ clip2->mClip, asr });
      clip2 = clip2->mParent;
    }
    if (!asr) {
      MOZ_ASSERT(!aAncestor, "We should have exited this loop earlier");
      break;
    }
    asr = asr->mParent;
  }

  // Convert intersectedClips into a DisplayItemClipChain.
  const DisplayItemClipChain* parentSC = aAncestor;
  for (auto& sc : Reversed(intersectedClips)) {
    parentSC = AllocateDisplayItemClipChain(sc.clip, sc.asr, parentSC);
  }
  return parentSC;
}

const DisplayItemClipChain*
nsDisplayListBuilder::CopyWholeChain(const DisplayItemClipChain* aClipChain)
{
  return CreateClipChainIntersection(nullptr, aClipChain, nullptr);
}

const nsIFrame*
nsDisplayListBuilder::FindReferenceFrameFor(const nsIFrame *aFrame,
                                            nsPoint* aOffset) const
{
  if (aFrame == mCurrentFrame) {
    if (aOffset) {
      *aOffset = mCurrentOffsetToReferenceFrame;
    }
    return mCurrentReferenceFrame;
  }
  for (const nsIFrame* f = aFrame; f; f = nsLayoutUtils::GetCrossDocParentFrame(f))
  {
    if (f == mReferenceFrame || f->IsTransformed()) {
      if (aOffset) {
        *aOffset = aFrame->GetOffsetToCrossDoc(f);
      }
      return f;
    }
  }
  if (aOffset) {
    *aOffset = aFrame->GetOffsetToCrossDoc(mReferenceFrame);
  }
  return mReferenceFrame;
}

// Sticky frames are active if their nearest scrollable frame is also active.
static bool
IsStickyFrameActive(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame, nsIFrame* aParent)
{
  MOZ_ASSERT(aFrame->StyleDisplay()->mPosition == NS_STYLE_POSITION_STICKY);

  // Find the nearest scrollframe.
  nsIFrame* cursor = aFrame;
  nsIFrame* parent = aParent;
  if (!parent) {
    parent = nsLayoutUtils::GetCrossDocParentFrame(aFrame);
  }
  while (!parent->IsScrollFrame()) {
    cursor = parent;
    if ((parent = nsLayoutUtils::GetCrossDocParentFrame(cursor)) == nullptr) {
      return false;
    }
  }

  nsIScrollableFrame* sf = do_QueryFrame(parent);
  return sf->IsScrollingActive(aBuilder) && sf->GetScrolledFrame() == cursor;
}

nsDisplayListBuilder::AGRState
nsDisplayListBuilder::IsAnimatedGeometryRoot(nsIFrame* aFrame,
                                             bool& aIsAsync,
                                             nsIFrame** aParent)
{
  aIsAsync = false;
  if (aFrame == mReferenceFrame) {
    aIsAsync = true;
    return AGR_YES;
  }
  if (!IsPaintingToWindow()) {
    if (aParent) {
      *aParent = nsLayoutUtils::GetCrossDocParentFrame(aFrame);
    }
    return AGR_NO;
  }

  nsIFrame* parent = nsLayoutUtils::GetCrossDocParentFrame(aFrame);
  if (!parent) {
    aIsAsync = true;
    return AGR_YES;
  }

  AGRState result = AGR_NO; // Possible to transition from not being an AGR
                            // to being an AGR without a style change.

  LayoutFrameType parentType = parent->Type();

  if (aFrame->IsTransformed()) {
    aIsAsync = EffectCompositor::HasAnimationsForCompositor(aFrame, eCSSProperty_transform);
    result = AGR_YES;
  }

  if (parentType == LayoutFrameType::Scroll ||
      parentType == LayoutFrameType::ListControl) {
    nsIScrollableFrame* sf = do_QueryFrame(parent);
    if (sf->GetScrolledFrame() == aFrame) {
      if (sf->IsScrollingActive(this)) {
        aIsAsync = aIsAsync || sf->IsMaybeAsynchronouslyScrolled();
        result = AGR_YES;
      } else {
        result = AGR_MAYBE;
      }
    }
  }

  // Finished checking all conditions that might set aIsAsync, so we can
  // early return now.
  if (result == AGR_YES) {
    return result;
  }

  if (nsLayoutUtils::IsPopup(aFrame))
    return AGR_YES;
  if (ActiveLayerTracker::IsOffsetStyleAnimated(aFrame)) {
    const bool inBudget = AddToAGRBudget(aFrame);
    if (inBudget) {
      return AGR_YES;
    }
  }
  if (!aFrame->GetParent() &&
      nsLayoutUtils::ViewportHasDisplayPort(aFrame->PresContext())) {
    // Viewport frames in a display port need to be animated geometry roots
    // for background-attachment:fixed elements.
    return AGR_YES;
  }


  // Treat the slider thumb as being as an active scrolled root when it wants
  // its own layer so that it can move without repainting.
  if (parentType == LayoutFrameType::Slider) {
    nsIScrollableFrame* sf = static_cast<nsSliderFrame*>(parent)->GetScrollFrame();
    // The word "Maybe" in IsMaybeScrollingActive might be confusing but we do
    // indeed need to always consider scroll thumbs as AGRs if
    // IsMaybeScrollingActive is true because that is the same condition we use
    // in ScrollFrameHelper::AppendScrollPartsTo to layerize scroll thumbs.
    if (sf && sf->IsMaybeScrollingActive()) {
      return AGR_YES;
    }
    result = AGR_MAYBE;
  }

  if (aFrame->StyleDisplay()->mPosition == NS_STYLE_POSITION_STICKY) {
    if (IsStickyFrameActive(this, aFrame, parent)) {
      return AGR_YES;
    }
    result = AGR_MAYBE;
  }


  // Fixed-pos frames are parented by the viewport frame, which has no parent.
  if (nsLayoutUtils::IsFixedPosFrameInDisplayPort(aFrame)) {
    return AGR_YES;
  }

  if ((aFrame->GetStateBits() & NS_FRAME_MAY_BE_TRANSFORMED) &&
      aFrame->IsFrameOfType(nsIFrame::eSVG)) {
    // For SVG containers, they always have
    // NS_FRAME_MAY_BE_TRANSFORMED bit.  However, they would be
    // affected by the fragement identifiers in the svgView form at
    // runtime without a new ComputedStyle.
    // For example, layout/reftests/svg/fragmentIdentifier-01.xhtml
    //
    // see https://www.w3.org/TR/SVG/linking.html#SVGFragmentIdentifiers
    result = AGR_MAYBE;
  }

  if (aParent) {
    *aParent = parent;
  }
  return result;
}

nsIFrame*
nsDisplayListBuilder::FindAnimatedGeometryRootFrameFor(nsIFrame* aFrame, bool& aIsAsync)
{
  MOZ_ASSERT(nsLayoutUtils::IsAncestorFrameCrossDoc(RootReferenceFrame(), aFrame));
  nsIFrame* cursor = aFrame;
  while (cursor != RootReferenceFrame()) {
    nsIFrame* next;
    if (IsAnimatedGeometryRoot(cursor, aIsAsync, &next) == AGR_YES)
      return cursor;
    cursor = next;
  }
  // Root frame is always an async agr.
  aIsAsync = true;
  return cursor;
}

void
nsDisplayListBuilder::RecomputeCurrentAnimatedGeometryRoot()
{
  bool isAsync;
  if (*mCurrentAGR != mCurrentFrame &&
      IsAnimatedGeometryRoot(const_cast<nsIFrame*>(mCurrentFrame), isAsync) == AGR_YES) {
    AnimatedGeometryRoot* oldAGR = mCurrentAGR;
    mCurrentAGR = WrapAGRForFrame(const_cast<nsIFrame*>(mCurrentFrame), isAsync, mCurrentAGR);

    // Iterate the AGR cache and look for any objects that reference the old AGR and check
    // to see if they need to be updated. AGRs can be in the cache multiple times, so we may
    // end up doing the work multiple times for AGRs that don't change.
    for (auto iter = mFrameToAnimatedGeometryRootMap.Iter(); !iter.Done(); iter.Next()) {
      RefPtr<AnimatedGeometryRoot> cached = iter.UserData();
      if (cached->mParentAGR == oldAGR && cached != mCurrentAGR) {
        // It's possible that this cached AGR struct that has the old AGR as a parent
        // should instead have mCurrentFrame has a parent.
        nsIFrame* parent = FindAnimatedGeometryRootFrameFor(*cached, isAsync);
        MOZ_ASSERT(parent == mCurrentFrame || parent == *oldAGR);
        if (parent == mCurrentFrame) {
          cached->mParentAGR = mCurrentAGR;
        }
      }
    }
  }
}

static nsRect
ApplyAllClipNonRoundedIntersection(const DisplayItemClipChain* aClipChain, const nsRect& aRect)
{
  nsRect result = aRect;
  while (aClipChain) {
    result = aClipChain->mClip.ApplyNonRoundedIntersection(result);
    aClipChain = aClipChain->mParent;
  }
  return result;
}

void
nsDisplayListBuilder::AdjustWindowDraggingRegion(nsIFrame* aFrame)
{
  if (!mWindowDraggingAllowed || !IsForPainting()) {
    return;
  }

  const nsStyleUIReset* styleUI = aFrame->StyleUIReset();
  if (styleUI->mWindowDragging == StyleWindowDragging::Default) {
    // This frame has the default value and doesn't influence the window
    // dragging region.
    return;
  }

  LayoutDeviceToLayoutDeviceMatrix4x4 referenceFrameToRootReferenceFrame;

  // The const_cast is for nsLayoutUtils::GetTransformToAncestor.
  nsIFrame* referenceFrame = const_cast<nsIFrame*>(FindReferenceFrameFor(aFrame));

  if (IsInTransform()) {
    // Only support 2d rectilinear transforms. Transform support is needed for
    // the horizontal flip transform that's applied to the urlbar textbox in
    // RTL mode - it should be able to exclude itself from the draggable region.
    referenceFrameToRootReferenceFrame =
      ViewAs<LayoutDeviceToLayoutDeviceMatrix4x4>(
          nsLayoutUtils::GetTransformToAncestor(referenceFrame, mReferenceFrame).GetMatrix());
    Matrix referenceFrameToRootReferenceFrame2d;
    if (!referenceFrameToRootReferenceFrame.Is2D(&referenceFrameToRootReferenceFrame2d) ||
        !referenceFrameToRootReferenceFrame2d.IsRectilinear()) {
      return;
    }
  } else {
    MOZ_ASSERT(referenceFrame == mReferenceFrame,
               "referenceFrameToRootReferenceFrame needs to be adjusted");
  }

  // We do some basic visibility checking on the frame's border box here.
  // We intersect it both with the current dirty rect and with the current
  // clip. Either one is just a conservative approximation on its own, but
  // their intersection luckily works well enough for our purposes, so that
  // we don't have to do full-blown visibility computations.
  // The most important case we need to handle is the scrolled-off tab:
  // If the tab bar overflows, tab parts that are clipped by the scrollbox
  // should not be allowed to interfere with the window dragging region. Using
  // just the current DisplayItemClip is not enough to cover this case
  // completely because clips are reset while building stacking context
  // contents, so for example we'd fail to clip frames that have a clip path
  // applied to them. But the current dirty rect doesn't get reset in that
  // case, so we use it to make this case work.
  nsRect borderBox = aFrame->GetRectRelativeToSelf().Intersect(mVisibleRect);
  borderBox += ToReferenceFrame(aFrame);
  const DisplayItemClipChain* clip = ClipState().GetCurrentCombinedClipChain(this);
  borderBox = ApplyAllClipNonRoundedIntersection(clip, borderBox);
  if (borderBox.IsEmpty()) {
    return;
  }

  LayoutDeviceRect devPixelBorderBox =
    LayoutDevicePixel::FromAppUnits(borderBox, aFrame->PresContext()->AppUnitsPerDevPixel());

  LayoutDeviceRect transformedDevPixelBorderBox =
    TransformBy(referenceFrameToRootReferenceFrame, devPixelBorderBox);
  transformedDevPixelBorderBox.Round();
  LayoutDeviceIntRect transformedDevPixelBorderBoxInt;

  if (!transformedDevPixelBorderBox.ToIntRect(&transformedDevPixelBorderBoxInt)) {
    return;
  }

  LayoutDeviceIntRegion& region =
    styleUI->mWindowDragging == StyleWindowDragging::Drag
      ? mWindowDraggingRegion : mWindowNoDraggingRegion;

  if (!IsRetainingDisplayList()) {
    region.OrWith(transformedDevPixelBorderBoxInt);
    return;
  }

  mozilla::gfx::IntRect rect(transformedDevPixelBorderBoxInt.ToUnknownRect());
  if (styleUI->mWindowDragging == StyleWindowDragging::Drag) {
    mRetainedWindowDraggingRegion.Add(aFrame, rect);
  } else {
    mRetainedWindowNoDraggingRegion.Add(aFrame, rect);
  }
}

LayoutDeviceIntRegion
nsDisplayListBuilder::GetWindowDraggingRegion() const
{
  LayoutDeviceIntRegion result;
  if (!IsRetainingDisplayList()) {
    result.Sub(mWindowDraggingRegion, mWindowNoDraggingRegion);
    return result;
  }

  LayoutDeviceIntRegion dragRegion =
    mRetainedWindowDraggingRegion.ToLayoutDeviceIntRegion();

  LayoutDeviceIntRegion noDragRegion =
    mRetainedWindowNoDraggingRegion.ToLayoutDeviceIntRegion();

  result.Sub(dragRegion, noDragRegion);
  return result;
}

/**
 * Removes modified frames and rects from |aRegion|.
 */
static void
RemoveModifiedFramesAndRects(nsDisplayListBuilder::WeakFrameRegion& aRegion)
{
  std::vector<WeakFrame>& frames = aRegion.mFrames;
  nsTArray<pixman_box32_t>& rects = aRegion.mRects;

  MOZ_ASSERT(frames.size() == rects.Length());

  uint32_t i = 0;
  uint32_t length = frames.size();

  while(i < length) {
    WeakFrame& frame = frames[i];

    if (!frame.IsAlive() || frame->IsFrameModified()) {
      // To avoid O(n) shifts in the array, move the last element of the array
      // to the current position and decrease the array length. Moving WeakFrame
      // inside of the array causes a new WeakFrame to be created and registered
      // with PresShell. We could avoid this by, for example, using a wrapper
      // class for WeakFrame, or by storing raw  WeakFrame pointers.
      frames[i] = frames[length - 1];
      rects[i] = rects[length - 1];
      length--;
    } else {
      i++;
    }
  }

  frames.resize(length);
  rects.TruncateLength(length);
}

void
nsDisplayListBuilder::RemoveModifiedWindowRegions()
{
  RemoveModifiedFramesAndRects(mRetainedWindowDraggingRegion);
  RemoveModifiedFramesAndRects(mRetainedWindowNoDraggingRegion);
  RemoveModifiedFramesAndRects(mWindowExcludeGlassRegion);
}

void
nsDisplayListBuilder::ClearRetainedWindowRegions()
{
  mRetainedWindowDraggingRegion.Clear();
  mRetainedWindowNoDraggingRegion.Clear();
  mWindowExcludeGlassRegion.Clear();
}

const uint32_t gWillChangeAreaMultiplier = 3;
static uint32_t GetLayerizationCost(const nsSize& aSize) {
  // There's significant overhead for each layer created from Gecko
  // (IPC+Shared Objects) and from the backend (like an OpenGL texture).
  // Therefore we set a minimum cost threshold of a 64x64 area.
  int minBudgetCost = 64 * 64;

  uint32_t budgetCost =
    std::max(minBudgetCost,
      nsPresContext::AppUnitsToIntCSSPixels(aSize.width) *
      nsPresContext::AppUnitsToIntCSSPixels(aSize.height));

  return budgetCost;
}

bool
nsDisplayListBuilder::AddToWillChangeBudget(nsIFrame* aFrame,
                                            const nsSize& aSize) {
  if (mWillChangeBudgetSet.Get(aFrame, nullptr)) {
    return true; // Already accounted
  }

  nsPresContext* key = aFrame->PresContext();
  DocumentWillChangeBudget budget;
  auto willChangeBudgetEntry = mWillChangeBudget.LookupForAdd(key);
  if (willChangeBudgetEntry) {
    // We have an existing entry.
    budget = willChangeBudgetEntry.Data();
  } else {
    budget = DocumentWillChangeBudget();
    willChangeBudgetEntry.OrInsert([&budget] () { return budget; });
  }

  nsRect area = aFrame->PresContext()->GetVisibleArea();
  uint32_t budgetLimit = nsPresContext::AppUnitsToIntCSSPixels(area.width) *
    nsPresContext::AppUnitsToIntCSSPixels(area.height);

  uint32_t cost = GetLayerizationCost(aSize);
  bool onBudget = (budget.mBudget + cost) /
                    gWillChangeAreaMultiplier < budgetLimit;

  if (onBudget) {
    budget.mBudget += cost;
    willChangeBudgetEntry.Data() = budget;
    mWillChangeBudgetSet.Put(aFrame, cost);
    aFrame->SetMayHaveWillChangeBudget(true);
  }

  return onBudget;
}

bool
nsDisplayListBuilder::IsInWillChangeBudget(nsIFrame* aFrame,
                                           const nsSize& aSize) {
  bool onBudget = AddToWillChangeBudget(aFrame, aSize);

  if (!onBudget) {
    nsString usageStr;
    usageStr.AppendInt(GetLayerizationCost(aSize));

    nsString multiplierStr;
    multiplierStr.AppendInt(gWillChangeAreaMultiplier);

    nsString limitStr;
    nsRect area = aFrame->PresContext()->GetVisibleArea();
    uint32_t budgetLimit = nsPresContext::AppUnitsToIntCSSPixels(area.width) *
      nsPresContext::AppUnitsToIntCSSPixels(area.height);
    limitStr.AppendInt(budgetLimit);

    const char16_t* params[] = { multiplierStr.get(), limitStr.get() };
    aFrame->PresContext()->Document()->WarnOnceAbout(
      nsIDocument::eIgnoringWillChangeOverBudget, false,
      params, ArrayLength(params));
  }
  return onBudget;
}

void
nsDisplayListBuilder::ClearWillChangeBudget(nsIFrame* aFrame)
{
  if (!aFrame->MayHaveWillChangeBudget()) {
    return;
  }
  aFrame->SetMayHaveWillChangeBudget(false);

  uint32_t cost = 0;
  if (!mWillChangeBudgetSet.Get(aFrame, &cost)) {
    return;
  }
  mWillChangeBudgetSet.Remove(aFrame);

  DocumentWillChangeBudget& budget =
    mWillChangeBudget.GetOrInsert(aFrame->PresContext());
  MOZ_ASSERT(budget.mBudget >= cost);
  budget.mBudget -= cost;
}

#ifdef MOZ_GFX_OPTIMIZE_MOBILE
const float gAGRBudgetAreaMultiplier = 0.3;
#else
const float gAGRBudgetAreaMultiplier = 3.0;
#endif

bool
nsDisplayListBuilder::AddToAGRBudget(nsIFrame* aFrame)
{
  if (mAGRBudgetSet.Contains(aFrame)) {
    return true;
  }

  const nsPresContext* presContext = aFrame->PresContext()->GetRootPresContext();
  if (!presContext) {
    return false;
  }

  const nsRect area = presContext->GetVisibleArea();
  const uint32_t budgetLimit = gAGRBudgetAreaMultiplier *
    nsPresContext::AppUnitsToIntCSSPixels(area.width) *
    nsPresContext::AppUnitsToIntCSSPixels(area.height);

  const uint32_t cost = GetLayerizationCost(aFrame->GetSize());
  const bool onBudget = mUsedAGRBudget + cost < budgetLimit;

  if (onBudget) {
    mUsedAGRBudget += cost;
    mAGRBudgetSet.PutEntry(aFrame);
  }

  return onBudget;
}

void
nsDisplayListBuilder::EnterSVGEffectsContents(nsDisplayList* aHoistedItemsStorage)
{
  MOZ_ASSERT(mSVGEffectsBuildingDepth >= 0);
  MOZ_ASSERT(aHoistedItemsStorage);
  if (mSVGEffectsBuildingDepth == 0) {
    MOZ_ASSERT(!mScrollInfoItemsForHoisting);
    mScrollInfoItemsForHoisting = aHoistedItemsStorage;
  }
  mSVGEffectsBuildingDepth++;
}

void
nsDisplayListBuilder::ExitSVGEffectsContents()
{
  mSVGEffectsBuildingDepth--;
  MOZ_ASSERT(mSVGEffectsBuildingDepth >= 0);
  MOZ_ASSERT(mScrollInfoItemsForHoisting);
  if (mSVGEffectsBuildingDepth == 0) {
    mScrollInfoItemsForHoisting = nullptr;
  }
}

void
nsDisplayListBuilder::AppendNewScrollInfoItemForHoisting(nsDisplayScrollInfoLayer* aScrollInfoItem)
{
  MOZ_ASSERT(ShouldBuildScrollInfoItemsForHoisting());
  MOZ_ASSERT(mScrollInfoItemsForHoisting);
  mScrollInfoItemsForHoisting->AppendToTop(aScrollInfoItem);
}

static nsRect
GetFrameArea(const nsDisplayListBuilder* aBuilder, const nsIFrame* aFrame)
{
  nsRect area;

  nsIScrollableFrame* scrollFrame = nsLayoutUtils::GetScrollableFrameFor(aFrame);
  if (scrollFrame) {
    // If the frame is content of a scrollframe, then we need to pick up the
    // area corresponding to the overflow rect as well. Otherwise the parts of
    // the overflow that are not occupied by descendants get skipped and the
    // APZ code sends touch events to the content underneath instead.
    // See https://bugzilla.mozilla.org/show_bug.cgi?id=1127773#c15.
    area = aFrame->GetScrollableOverflowRect();
  } else {
    area = nsRect(nsPoint(0, 0), aFrame->GetSize());
  }

  if (!area.IsEmpty()) {
    return area + aBuilder->ToReferenceFrame(aFrame);
  }

  return area;
}

void
nsDisplayListBuilder::BuildCompositorHitTestInfoIfNeeded(nsIFrame* aFrame,
                                                         nsDisplayList* aList,
                                                         const bool aBuildNew)
{
  MOZ_ASSERT(aFrame);
  MOZ_ASSERT(aList);

  if (!BuildCompositorHitTestInfo()) {
    return;
  }

  CompositorHitTestInfo info = aFrame->GetCompositorHitTestInfo(this);
  if (!ShouldBuildCompositorHitTestInfo(aFrame, info, aBuildNew)) {
    // Either the parent hit test info can be reused, or this frame has no hit
    // test flags set.
    return;
  }

  nsDisplayCompositorHitTestInfo* item =
    MakeDisplayItem<nsDisplayCompositorHitTestInfo>(this, aFrame, info);

  SetCompositorHitTestInfo(item);
  aList->AppendToTop(item);
}

bool
nsDisplayListBuilder::ShouldBuildCompositorHitTestInfo(const nsIFrame* aFrame,
                                                       const CompositorHitTestInfo& aInfo,
                                                       const bool aBuildNew) const
{
  MOZ_ASSERT(mBuildCompositorHitTestInfo);

  if (aInfo == CompositorHitTestInfo::eInvisibleToHitTest) {
    return false;
  }

  if (!mCompositorHitTestInfo || !mLessEventRegionItems || aBuildNew) {
    return true;
  }

  if (mCompositorHitTestInfo->HitTestInfo() != aInfo) {
    // Hit test flags are different.
    return true;
  }

  // Create a new item if the parent does not contain the child completely.
  return !mCompositorHitTestInfo->Area().Contains(GetFrameArea(this, aFrame));
}

void nsDisplayListSet::MoveTo(const nsDisplayListSet& aDestination) const
{
  aDestination.BorderBackground()->AppendToTop(BorderBackground());
  aDestination.BlockBorderBackgrounds()->AppendToTop(BlockBorderBackgrounds());
  aDestination.Floats()->AppendToTop(Floats());
  aDestination.Content()->AppendToTop(Content());
  aDestination.PositionedDescendants()->AppendToTop(PositionedDescendants());
  aDestination.Outlines()->AppendToTop(Outlines());
}

static void
MoveListTo(nsDisplayList* aList, nsTArray<nsDisplayItem*>* aElements) {
  nsDisplayItem* item;
  while ((item = aList->RemoveBottom()) != nullptr) {
    aElements->AppendElement(item);
  }
}

nsRect
nsDisplayList::GetBounds(nsDisplayListBuilder* aBuilder) const {
  nsRect bounds;
  for (nsDisplayItem* i = GetBottom(); i != nullptr; i = i->GetAbove()) {
    bounds.UnionRect(bounds, i->GetClippedBounds(aBuilder));
  }
  return bounds;
}

nsRect
nsDisplayList::GetClippedBoundsWithRespectToASR(nsDisplayListBuilder* aBuilder,
                                                const ActiveScrolledRoot* aASR,
                                                nsRect* aBuildingRect) const {
  nsRect bounds;
  for (nsDisplayItem* i = GetBottom(); i != nullptr; i = i->GetAbove()) {
    nsRect r = i->GetClippedBounds(aBuilder);
    if (aASR != i->GetActiveScrolledRoot() && !r.IsEmpty()) {
      if (Maybe<nsRect> clip = i->GetClipWithRespectToASR(aBuilder, aASR)) {
        r = clip.ref();
      }
    }
    if (aBuildingRect) {
      aBuildingRect->UnionRect(*aBuildingRect, i->GetBuildingRect());
    }
    bounds.UnionRect(bounds, r);
  }
  return bounds;
}

nsRect
nsDisplayList::GetBuildingRect() const {
  nsRect result;
  for (nsDisplayItem* i = GetBottom(); i != nullptr; i = i->GetAbove()) {
    result.UnionRect(result, i->GetBuildingRect());
  }
  return result;
}

bool
nsDisplayList::ComputeVisibilityForRoot(nsDisplayListBuilder* aBuilder,
                                        nsRegion* aVisibleRegion) {
  AUTO_PROFILER_LABEL("nsDisplayList::ComputeVisibilityForRoot", GRAPHICS);

  nsRegion r;
  const ActiveScrolledRoot* rootASR = nullptr;
  if (gfxPrefs::LayoutUseContainersForRootFrames()) {
    rootASR = aBuilder->ActiveScrolledRootForRootScrollframe();
  }
  r.And(*aVisibleRegion, GetClippedBoundsWithRespectToASR(aBuilder, rootASR));
  return ComputeVisibilityForSublist(aBuilder, aVisibleRegion, r.GetBounds());
}

static nsRegion
TreatAsOpaque(nsDisplayItem* aItem, nsDisplayListBuilder* aBuilder)
{
  bool snap;
  nsRegion opaque = aItem->GetOpaqueRegion(aBuilder, &snap);
  if (aBuilder->IsForPluginGeometry() &&
      aItem->GetType() != DisplayItemType::TYPE_COMPOSITOR_HITTEST_INFO)
  {
    // Treat all leaf chrome items as opaque, unless their frames are opacity:0.
    // Since opacity:0 frames generate an nsDisplayOpacity, that item will
    // not be treated as opaque here, so opacity:0 chrome content will be
    // effectively ignored, as it should be.
    // We treat leaf chrome items as opaque to ensure that they cover
    // content plugins, for security reasons.
    // Non-leaf chrome items don't render contents of their own so shouldn't
    // be treated as opaque (and their bounds is just the union of their
    // children, which might be a large area their contents don't really cover).
    nsIFrame* f = aItem->Frame();
    if (f->PresContext()->IsChrome() && !aItem->GetChildren() &&
        f->StyleEffects()->mOpacity != 0.0) {
      opaque = aItem->GetBounds(aBuilder, &snap);
    }
  }
  if (opaque.IsEmpty()) {
    return opaque;
  }
  nsRegion opaqueClipped;
  for (auto iter = opaque.RectIter(); !iter.Done(); iter.Next()) {
    opaqueClipped.Or(opaqueClipped,
                     aItem->GetClip().ApproximateIntersectInward(iter.Get()));
  }
  return opaqueClipped;
}

bool
nsDisplayList::ComputeVisibilityForSublist(nsDisplayListBuilder* aBuilder,
                                           nsRegion* aVisibleRegion,
                                           const nsRect& aListVisibleBounds)
{
#ifdef DEBUG
  nsRegion r;
  r.And(*aVisibleRegion, GetBounds(aBuilder));
  // XXX this fails sometimes:
  NS_WARNING_ASSERTION(r.GetBounds().IsEqualInterior(aListVisibleBounds),
                       "bad aListVisibleBounds");
#endif

  bool anyVisible = false;

  AutoTArray<nsDisplayItem*, 512> elements;
  MoveListTo(this, &elements);

  for (int32_t i = elements.Length() - 1; i >= 0; --i) {
    nsDisplayItem* item = elements[i];

    if (item->ForceNotVisible() && !item->GetSameCoordinateSystemChildren()) {
      NS_ASSERTION(item->GetBuildingRect().IsEmpty(),
        "invisible items should have empty vis rect");
      item->SetPaintRect(nsRect());
    } else {
      nsRect bounds = item->GetClippedBounds(aBuilder);

      nsRegion itemVisible;
      itemVisible.And(*aVisibleRegion, bounds);
      item->SetPaintRect(itemVisible.GetBounds());
    }

    if (item->ComputeVisibility(aBuilder, aVisibleRegion)) {
      anyVisible = true;

      nsRegion opaque = TreatAsOpaque(item, aBuilder);
      // Subtract opaque item from the visible region
      aBuilder->SubtractFromVisibleRegion(aVisibleRegion, opaque);
    }
    AppendToBottom(item);
  }

  mIsOpaque = !aVisibleRegion->Intersects(aListVisibleBounds);
  return anyVisible;
}

static bool
TriggerPendingAnimationsOnSubDocuments(nsIDocument* aDocument, void* aReadyTime)
{
  PendingAnimationTracker* tracker = aDocument->GetPendingAnimationTracker();
  if (tracker) {
    nsIPresShell* shell = aDocument->GetShell();
    // If paint-suppression is in effect then we haven't finished painting
    // this document yet so we shouldn't start animations
    if (!shell || !shell->IsPaintingSuppressed()) {
      const TimeStamp& readyTime = *static_cast<TimeStamp*>(aReadyTime);
      tracker->TriggerPendingAnimationsOnNextTick(readyTime);
    }
  }
  aDocument->EnumerateSubDocuments(TriggerPendingAnimationsOnSubDocuments,
                                   aReadyTime);
  return true;
}

static void
TriggerPendingAnimations(nsIDocument* aDocument,
                       const TimeStamp& aReadyTime) {
  MOZ_ASSERT(!aReadyTime.IsNull(),
             "Animation ready time is not set. Perhaps we're using a layer"
             " manager that doesn't update it");
  TriggerPendingAnimationsOnSubDocuments(aDocument,
                                         const_cast<TimeStamp*>(&aReadyTime));
}

LayerManager*
nsDisplayListBuilder::GetWidgetLayerManager(nsView** aView)
{
  if (aView) {
    *aView = RootReferenceFrame()->GetView();
  }
  if (RootReferenceFrame() != nsLayoutUtils::GetDisplayRootFrame(RootReferenceFrame())) {
    return nullptr;
  }
  nsIWidget* window = RootReferenceFrame()->GetNearestWidget();
  if (window) {
    return window->GetLayerManager();
  }
  return nullptr;
}

FrameLayerBuilder*
nsDisplayList::BuildLayers(nsDisplayListBuilder* aBuilder,
                           LayerManager* aLayerManager,
                           uint32_t aFlags,
                           bool aIsWidgetTransaction)
{
  nsIFrame* frame = aBuilder->RootReferenceFrame();
  nsPresContext* presContext = frame->PresContext();
  nsIPresShell* presShell = presContext->PresShell();

  FrameLayerBuilder *layerBuilder = new FrameLayerBuilder();
  layerBuilder->Init(aBuilder, aLayerManager);

  if (aFlags & PAINT_COMPRESSED) {
    layerBuilder->SetLayerTreeCompressionMode();
  }

  RefPtr<ContainerLayer> root;
  {
    AUTO_PROFILER_TRACING("Paint", "LayerBuilding");

    if (XRE_IsContentProcess() && gfxPrefs::AlwaysPaint()) {
      FrameLayerBuilder::InvalidateAllLayers(aLayerManager);
    }

    if (aIsWidgetTransaction) {
      layerBuilder->DidBeginRetainedLayerTransaction(aLayerManager);
    }

    // Clear any ScrollMetadata that may have been set on the root layer on a
    // previous paint. This paint will set new metrics if necessary, and if we
    // don't clear the old one here, we may be left with extra metrics.
    if (Layer* rootLayer = aLayerManager->GetRoot()) {
      rootLayer->SetScrollMetadata(nsTArray<ScrollMetadata>());
    }

    ContainerLayerParameters containerParameters
      (presShell->GetResolution(), presShell->GetResolution());

    {
      PaintTelemetry::AutoRecord record(PaintTelemetry::Metric::Layerization);

      root = layerBuilder->
        BuildContainerLayerFor(aBuilder, aLayerManager, frame, nullptr, this,
                               containerParameters, nullptr);

      if (!record.GetStart().IsNull() && gfxPrefs::LayersDrawFPS()) {
        if (PaintTiming* pt = ClientLayerManager::MaybeGetPaintTiming(aLayerManager)) {
          pt->flbMs() = (TimeStamp::Now() - record.GetStart()).ToMilliseconds();
        }
      }
    }

    if (!root) {
      return nullptr;
    }
    // Root is being scaled up by the X/Y resolution. Scale it back down.
    root->SetPostScale(1.0f/containerParameters.mXScale,
                       1.0f/containerParameters.mYScale);
    root->SetScaleToResolution(presShell->ScaleToResolution(),
        containerParameters.mXScale);

    auto callback = [root](FrameMetrics::ViewID aScrollId) -> bool {
      return nsLayoutUtils::ContainsMetricsWithId(root, aScrollId);
    };
    if (Maybe<ScrollMetadata> rootMetadata = nsLayoutUtils::GetRootMetadata(
          aBuilder, root->Manager(), containerParameters, callback)) {
      root->SetScrollMetadata(rootMetadata.value());
    }

    // NS_WARNING is debug-only, so don't even bother checking the conditions in
    // a release build.
#ifdef DEBUG
    bool usingDisplayport = false;
    if (nsIFrame* rootScrollFrame = presShell->GetRootScrollFrame()) {
      nsIContent* content = rootScrollFrame->GetContent();
      if (content) {
        usingDisplayport = nsLayoutUtils::HasDisplayPort(content);
      }
    }
    if (usingDisplayport &&
        !(root->GetContentFlags() & Layer::CONTENT_OPAQUE) &&
        SpammyLayoutWarningsEnabled()) {
      // See bug 693938, attachment 567017
      NS_WARNING("Transparent content with displayports can be expensive.");
    }
#endif

    aLayerManager->SetRoot(root);
    layerBuilder->WillEndTransaction();
  }
  return layerBuilder;
}

/**
 * We paint by executing a layer manager transaction, constructing a
 * single layer representing the display list, and then making it the
 * root of the layer manager, drawing into the PaintedLayers.
 */
already_AddRefed<LayerManager> nsDisplayList::PaintRoot(nsDisplayListBuilder* aBuilder,
                                                        gfxContext* aCtx,
                                                        uint32_t aFlags)
{
  AUTO_PROFILER_LABEL("nsDisplayList::PaintRoot", GRAPHICS);

  RefPtr<LayerManager> layerManager;
  bool widgetTransaction = false;
  bool doBeginTransaction = true;
  nsView *view = nullptr;
  if (aFlags & PAINT_USE_WIDGET_LAYERS) {
    layerManager = aBuilder->GetWidgetLayerManager(&view);
    if (layerManager) {
      doBeginTransaction = !(aFlags & PAINT_EXISTING_TRANSACTION);
      widgetTransaction = true;
    }
  }
  if (!layerManager) {
    if (!aCtx) {
      NS_WARNING("Nowhere to paint into");
      return nullptr;
    }
    layerManager = new BasicLayerManager(BasicLayerManager::BLM_OFFSCREEN);
  }

  nsIFrame* frame = aBuilder->RootReferenceFrame();
  nsPresContext* presContext = frame->PresContext();
  nsIPresShell* presShell = presContext->PresShell();
  nsIDocument* document = presShell->GetDocument();

  if (layerManager->GetBackendType() == layers::LayersBackend::LAYERS_WR) {
    if (doBeginTransaction) {
      if (aCtx) {
        if (!layerManager->BeginTransactionWithTarget(aCtx)) {
          return nullptr;
        }
      } else {
        if (!layerManager->BeginTransaction()) {
          return nullptr;
        }
      }
    }

    bool prevIsCompositingCheap =
      aBuilder->SetIsCompositingCheap(layerManager->IsCompositingCheap());
    MaybeSetupTransactionIdAllocator(layerManager, presContext);

    bool sent = false;
    if (aFlags & PAINT_IDENTICAL_DISPLAY_LIST) {
      sent = layerManager->EndEmptyTransaction();
    }

    if (!sent) {
      // Windowed plugins are not supported with WebRender enabled.
      // But PluginGeometry needs to be updated to show plugin.
      // Windowed plugins are going to be removed by Bug 1296400.
      nsRootPresContext* rootPresContext = presContext->GetRootPresContext();
      if (rootPresContext && XRE_IsContentProcess()) {
        if (aBuilder->WillComputePluginGeometry()) {
          rootPresContext->ComputePluginGeometryUpdates(aBuilder->RootReferenceFrame(), aBuilder, this);
        }
        // This must be called even if PluginGeometryUpdates were not computed.
        rootPresContext->CollectPluginGeometryUpdates(layerManager);
      }

      WebRenderLayerManager* wrManager = static_cast<WebRenderLayerManager*>(layerManager.get());

      nsIDocShell* docShell = presContext->GetDocShell();
      nsTArray<wr::WrFilterOp> wrFilters;
      gfx::Matrix5x4* colorMatrix = nsDocShell::Cast(docShell)->GetColorMatrix();
      if (colorMatrix) {
        wr::WrFilterOp gs = {
          wr::WrFilterOpType::ColorMatrix
        };
        MOZ_ASSERT(sizeof(gs.matrix) == sizeof(colorMatrix->components));
        memcpy(&(gs.matrix), colorMatrix->components, sizeof(gs.matrix));
        wrFilters.AppendElement(gs);
      }

      wrManager->EndTransactionWithoutLayer(this, aBuilder, wrFilters);
    }

    // For layers-free mode, we check the invalidation state bits in the EndTransaction.
    // So we clear the invalidation state bits after EndTransaction.
    if (widgetTransaction ||
        // SVG-as-an-image docs don't paint as part of the retained layer tree,
        // but they still need the invalidation state bits cleared in order for
        // invalidation for CSS/SMIL animation to work properly.
        (document && document->IsBeingUsedAsImage())) {
      frame->ClearInvalidationStateBits();
    }

    aBuilder->SetIsCompositingCheap(prevIsCompositingCheap);
    if (document && widgetTransaction) {
      TriggerPendingAnimations(document, layerManager->GetAnimationReadyTime());
    }

    if (presContext->RefreshDriver()->HasScheduleFlush()) {
      presContext->NotifyInvalidation(layerManager->GetLastTransactionId(), frame->GetRect());
    }

    return layerManager.forget();
  }

  NotifySubDocInvalidationFunc computeInvalidFunc =
    presContext->MayHavePaintEventListenerInSubDocument() ? nsPresContext::NotifySubDocInvalidation : 0;

  UniquePtr<LayerProperties> props;

  bool computeInvalidRect = (computeInvalidFunc ||
                             (!layerManager->IsCompositingCheap() && layerManager->NeedsWidgetInvalidation())) &&
                            widgetTransaction;

  if (computeInvalidRect) {
    props = LayerProperties::CloneFrom(layerManager->GetRoot());
  }

  if (doBeginTransaction) {
    if (aCtx) {
      if (!layerManager->BeginTransactionWithTarget(aCtx)) {
        return nullptr;
      }
    } else {
      if (!layerManager->BeginTransaction()) {
        return nullptr;
      }
    }
  }

  bool temp = aBuilder->SetIsCompositingCheap(layerManager->IsCompositingCheap());
  LayerManager::EndTransactionFlags flags = LayerManager::END_DEFAULT;
  if (layerManager->NeedsWidgetInvalidation()) {
    if (aFlags & PAINT_NO_COMPOSITE) {
      flags = LayerManager::END_NO_COMPOSITE;
    }
  } else {
    // Client layer managers never composite directly, so
    // we don't need to worry about END_NO_COMPOSITE.
    if (aBuilder->WillComputePluginGeometry()) {
      flags = LayerManager::END_NO_REMOTE_COMPOSITE;
    }
  }

  MaybeSetupTransactionIdAllocator(layerManager, presContext);

  // Store the existing layer builder to reinstate it on return.
  FrameLayerBuilder *oldBuilder = layerManager->GetLayerBuilder();
  FrameLayerBuilder *layerBuilder = nullptr;

  bool sent = false;
  if (aFlags & PAINT_IDENTICAL_DISPLAY_LIST) {
    sent = layerManager->EndEmptyTransaction(flags);
  }

  if (!sent) {
    layerBuilder = BuildLayers(aBuilder, layerManager,
                               aFlags, widgetTransaction);

    if (!layerBuilder) {
      layerManager->SetUserData(&gLayerManagerLayerBuilder, oldBuilder);
      return nullptr;
    }

    // If this is the content process, we ship plugin geometry updates over with layer
    // updates, so calculate that now before we call EndTransaction.
    nsRootPresContext* rootPresContext = presContext->GetRootPresContext();
    if (rootPresContext && XRE_IsContentProcess()) {
      if (aBuilder->WillComputePluginGeometry()) {
        rootPresContext->ComputePluginGeometryUpdates(aBuilder->RootReferenceFrame(), aBuilder, this);
      }
      // The layer system caches plugin configuration information for forwarding
      // with layer updates which needs to get set during reflow. This must be
      // called even if there are no windowed plugins in the page.
      rootPresContext->CollectPluginGeometryUpdates(layerManager);
    }

    layerManager->EndTransaction(FrameLayerBuilder::DrawPaintedLayer,
                                 aBuilder, flags);
    layerBuilder->DidEndTransaction();
  }

  if (widgetTransaction ||
      // SVG-as-an-image docs don't paint as part of the retained layer tree,
      // but they still need the invalidation state bits cleared in order for
      // invalidation for CSS/SMIL animation to work properly.
      (document && document->IsBeingUsedAsImage())) {
    frame->ClearInvalidationStateBits();
  }

  aBuilder->SetIsCompositingCheap(temp);

  if (document && widgetTransaction) {
    TriggerPendingAnimations(document, layerManager->GetAnimationReadyTime());
  }

  nsIntRegion invalid;
  bool areaOverflowed = false;
  if (props) {
    if (!props->ComputeDifferences(layerManager->GetRoot(), invalid, computeInvalidFunc)) {
      areaOverflowed = true;
    }
  } else if (widgetTransaction) {
    LayerProperties::ClearInvalidations(layerManager->GetRoot());
  }

  bool shouldInvalidate = layerManager->NeedsWidgetInvalidation();
  if (view) {
    if (props && !areaOverflowed) {
      if (!invalid.IsEmpty()) {
        nsIntRect bounds = invalid.GetBounds();
        nsRect rect(presContext->DevPixelsToAppUnits(bounds.x),
                    presContext->DevPixelsToAppUnits(bounds.y),
                    presContext->DevPixelsToAppUnits(bounds.width),
                    presContext->DevPixelsToAppUnits(bounds.height));
        if (shouldInvalidate) {
          view->GetViewManager()->InvalidateViewNoSuppression(view, rect);
        }
        presContext->NotifyInvalidation(layerManager->GetLastTransactionId(), bounds);
      }
    } else if (shouldInvalidate) {
      view->GetViewManager()->InvalidateView(view);
    }
  }

  layerManager->SetUserData(&gLayerManagerLayerBuilder, oldBuilder);
  return layerManager.forget();
}

nsDisplayItem* nsDisplayList::RemoveBottom() {
  nsDisplayItem* item = mSentinel.mAbove;
  if (!item)
    return nullptr;
  mSentinel.mAbove = item->mAbove;
  if (item == mTop) {
    // must have been the only item
    mTop = &mSentinel;
  }
  item->mAbove = nullptr;
  mLength--;
  return item;
}

void nsDisplayList::DeleteAll(nsDisplayListBuilder* aBuilder) {
  nsDisplayItem* item;
  while ((item = RemoveBottom()) != nullptr) {
    item->Destroy(aBuilder);
  }
}

static bool
GetMouseThrough(const nsIFrame* aFrame)
{
  if (!aFrame->IsXULBoxFrame())
    return false;

  const nsIFrame* frame = aFrame;
  while (frame) {
    if (frame->GetStateBits() & NS_FRAME_MOUSE_THROUGH_ALWAYS) {
      return true;
    } else if (frame->GetStateBits() & NS_FRAME_MOUSE_THROUGH_NEVER) {
      return false;
    }
    frame = nsBox::GetParentXULBox(frame);
  }
  return false;
}

static bool
IsFrameReceivingPointerEvents(nsIFrame* aFrame)
{
  return NS_STYLE_POINTER_EVENTS_NONE !=
    aFrame->StyleUserInterface()->GetEffectivePointerEvents(aFrame);
}

// A list of frames, and their z depth. Used for sorting
// the results of hit testing.
struct FramesWithDepth
{
  explicit FramesWithDepth(float aDepth) :
    mDepth(aDepth)
  {}

  bool operator<(const FramesWithDepth& aOther) const {
    if (!FuzzyEqual(mDepth, aOther.mDepth, 0.1f)) {
      // We want to sort so that the shallowest item (highest depth value) is first
      return mDepth > aOther.mDepth;
    }
    return this < &aOther;
  }
  bool operator==(const FramesWithDepth& aOther) const {
    return this == &aOther;
  }

  float mDepth;
  nsTArray<nsIFrame*> mFrames;
};

// Sort the frames by depth and then moves all the contained frames to the destination
static void
FlushFramesArray(nsTArray<FramesWithDepth>& aSource, nsTArray<nsIFrame*>* aDest)
{
  if (aSource.IsEmpty()) {
    return;
  }
  aSource.Sort();
  uint32_t length = aSource.Length();
  for (uint32_t i = 0; i < length; i++) {
    aDest->AppendElements(std::move(aSource[i].mFrames));
  }
  aSource.Clear();
}

void nsDisplayList::HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
                            nsDisplayItem::HitTestState* aState,
                            nsTArray<nsIFrame*> *aOutFrames) const {
  nsDisplayItem* item;

  if (aState->mInPreserves3D) {
    // Collect leaves of the current 3D rendering context.
    for (item = GetBottom(); item; item = item->GetAbove()) {
      auto itemType = item->GetType();
      if (itemType != DisplayItemType::TYPE_TRANSFORM ||
          !static_cast<nsDisplayTransform*>(item)->IsLeafOf3DContext()) {
        item->HitTest(aBuilder, aRect, aState, aOutFrames);
      } else {
        // One of leaves in the current 3D rendering context.
        aState->mItemBuffer.AppendElement(item);
      }
    }
    return;
  }

  int32_t itemBufferStart = aState->mItemBuffer.Length();
  for (item = GetBottom(); item; item = item->GetAbove()) {
    aState->mItemBuffer.AppendElement(item);
  }

  AutoTArray<FramesWithDepth, 16> temp;
  for (int32_t i = aState->mItemBuffer.Length() - 1; i >= itemBufferStart; --i) {
    // Pop element off the end of the buffer. We want to shorten the buffer
    // so that recursive calls to HitTest have more buffer space.
    item = aState->mItemBuffer[i];
    aState->mItemBuffer.SetLength(i);

    bool snap;
    nsRect r = item->GetBounds(aBuilder, &snap).Intersect(aRect);
    auto itemType = item->GetType();
    bool same3DContext =
      (itemType == DisplayItemType::TYPE_TRANSFORM &&
       static_cast<nsDisplayTransform*>(item)->IsParticipating3DContext()) ||
      (itemType == DisplayItemType::TYPE_PERSPECTIVE &&
       item->Frame()->Extend3DContext());
    if (same3DContext &&
        (itemType != DisplayItemType::TYPE_TRANSFORM ||
         !static_cast<nsDisplayTransform*>(item)->IsLeafOf3DContext())) {
      if (!item->GetClip().MayIntersect(aRect)) {
        continue;
      }
      AutoTArray<nsIFrame*, 1> neverUsed;
      // Start gethering leaves of the 3D rendering context, and
      // append leaves at the end of mItemBuffer.  Leaves are
      // processed at following iterations.
      aState->mInPreserves3D = true;
      item->HitTest(aBuilder, aRect, aState, &neverUsed);
      aState->mInPreserves3D = false;
      i = aState->mItemBuffer.Length();
      continue;
    }
    if (same3DContext || item->GetClip().MayIntersect(r)) {
      AutoTArray<nsIFrame*, 16> outFrames;
      item->HitTest(aBuilder, aRect, aState, &outFrames);

      // For 3d transforms with preserve-3d we add hit frames into the temp list
      // so we can sort them later, otherwise we add them directly to the output list.
      nsTArray<nsIFrame*> *writeFrames = aOutFrames;
      if (item->GetType() == DisplayItemType::TYPE_TRANSFORM &&
          static_cast<nsDisplayTransform*>(item)->IsLeafOf3DContext()) {
        if (outFrames.Length()) {
          nsDisplayTransform *transform = static_cast<nsDisplayTransform*>(item);
          nsPoint point = aRect.TopLeft();
          // A 1x1 rect means a point, otherwise use the center of the rect
          if (aRect.width != 1 || aRect.height != 1) {
            point = aRect.Center();
          }
          temp.AppendElement(FramesWithDepth(transform->GetHitDepthAtPoint(aBuilder, point)));
          writeFrames = &temp[temp.Length() - 1].mFrames;
        }
      } else {
        // We may have just finished a run of consecutive preserve-3d transforms,
        // so flush these into the destination array before processing our frame list.
        FlushFramesArray(temp, aOutFrames);
      }

      for (uint32_t j = 0; j < outFrames.Length(); j++) {
        nsIFrame *f = outFrames.ElementAt(j);
        // Filter out some frames depending on the type of hittest
        // we are doing. For visibility tests, pass through all frames.
        // For pointer tests, only pass through frames that are styled
        // to receive pointer events.
        if (aBuilder->HitTestIsForVisibility() ||
            (!GetMouseThrough(f) && IsFrameReceivingPointerEvents(f))) {
          writeFrames->AppendElement(f);
        }
      }

      if (aBuilder->HitTestIsForVisibility() &&
          item->GetOpaqueRegion(aBuilder, &snap).Contains(aRect)) {
        // We're exiting early, so pop the remaining items off the buffer.
        aState->mItemBuffer.SetLength(itemBufferStart);
        break;
      }
    }
  }
  // Clear any remaining preserve-3d transforms.
  FlushFramesArray(temp, aOutFrames);
  NS_ASSERTION(aState->mItemBuffer.Length() == uint32_t(itemBufferStart),
               "How did we forget to pop some elements?");
}

static nsIContent* FindContentInDocument(nsDisplayItem* aItem, nsIDocument* aDoc) {
  nsIFrame* f = aItem->Frame();
  while (f) {
    nsPresContext* pc = f->PresContext();
    if (pc->Document() == aDoc) {
      return f->GetContent();
    }
    f = nsLayoutUtils::GetCrossDocParentFrame(pc->PresShell()->GetRootFrame());
  }
  return nullptr;
}

struct ZSortItem {
  nsDisplayItem* item;
  int32_t zIndex;

  explicit ZSortItem(nsDisplayItem* aItem)
    : item(aItem), zIndex(aItem->ZIndex()) {}

  operator nsDisplayItem*() {
    return item;
  }
};

struct ZOrderComparator {
  bool operator()(const ZSortItem& aLeft, const ZSortItem& aRight) const {
    // Note that we can't just take the difference of the two
    // z-indices here, because that might overflow a 32-bit int.
    return aLeft.zIndex < aRight.zIndex;
  }
};

void nsDisplayList::SortByZOrder() {
  Sort<ZSortItem>(ZOrderComparator());
}

struct ContentComparator {
  nsIContent* mCommonAncestor;

  explicit ContentComparator(nsIContent* aCommonAncestor)
    : mCommonAncestor(aCommonAncestor) {}

  bool operator()(nsDisplayItem* aLeft, nsDisplayItem* aRight) const {
    // It's possible that the nsIContent for aItem1 or aItem2 is in a subdocument
    // of commonAncestor, because display items for subdocuments have been
    // mixed into the same list. Ensure that we're looking at content
    // in commonAncestor's document.
    nsIDocument* commonAncestorDoc = mCommonAncestor->OwnerDoc();
    nsIContent* content1 = FindContentInDocument(aLeft, commonAncestorDoc);
    nsIContent* content2 = FindContentInDocument(aRight, commonAncestorDoc);
    if (!content1 || !content2) {
      NS_ERROR("Document trees are mixed up!");
      // Something weird going on
      return true;
    }
    return nsLayoutUtils::CompareTreePosition(content1, content2, mCommonAncestor) < 0;
  }
};

void nsDisplayList::SortByContentOrder(nsIContent* aCommonAncestor) {
  Sort<nsDisplayItem*>(ContentComparator(aCommonAncestor));
}

nsDisplayItem::nsDisplayItem(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame)
 : nsDisplayItem(aBuilder, aFrame,
                 aBuilder->CurrentActiveScrolledRoot())
{}

nsDisplayItem::nsDisplayItem(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                             const ActiveScrolledRoot* aActiveScrolledRoot,
                             bool aAnonymous)
  : mFrame(aFrame)
  , mActiveScrolledRoot(aActiveScrolledRoot)
  , mAnimatedGeometryRoot(nullptr)
  , mForceNotVisible(aBuilder->IsBuildingInvisibleItems())
  , mDisableSubpixelAA(false)
  , mReusedItem(false)
  , mBackfaceHidden(mFrame->In3DContextAndBackfaceIsHidden())
  , mPaintRectValid(false)
#ifdef MOZ_DUMP_PAINTING
  , mPainted(false)
#endif
{
  MOZ_COUNT_CTOR(nsDisplayItem);
  if (aBuilder->IsRetainingDisplayList() && !aAnonymous) {
    mFrame->AddDisplayItem(this);
  }
  mReferenceFrame = aBuilder->FindReferenceFrameFor(aFrame, &mToReferenceFrame);
  // This can return the wrong result if the item override ShouldFixToViewport(),
  // the item needs to set it again in its constructor.
  mAnimatedGeometryRoot = aBuilder->FindAnimatedGeometryRootFor(aFrame);
  MOZ_ASSERT(nsLayoutUtils::IsAncestorFrameCrossDoc(aBuilder->RootReferenceFrame(),
                                                    *mAnimatedGeometryRoot), "Bad");
  NS_ASSERTION(aBuilder->GetVisibleRect().width >= 0 ||
               !aBuilder->IsForPainting(), "visible rect not set");

  SetClipChain(aBuilder->ClipState().GetCurrentCombinedClipChain(aBuilder), true);

  // The visible rect is for mCurrentFrame, so we have to use
  // mCurrentOffsetToReferenceFrame
  nsRect visible = aBuilder->GetVisibleRect() +
    aBuilder->GetCurrentFrameOffsetToReferenceFrame();
  SetBuildingRect(visible);
}

/* static */ bool
nsDisplayItem::ForceActiveLayers()
{
  static bool sForce = false;
  static bool sForceCached = false;

  if (!sForceCached) {
    Preferences::AddBoolVarCache(&sForce, "layers.force-active", false);
    sForceCached = true;
  }

  return sForce;
}

static int32_t ZIndexForFrame(nsIFrame* aFrame)
{
  if (!aFrame->IsAbsPosContainingBlock() && !aFrame->IsFlexOrGridItem())
    return 0;

  const nsStylePosition* position = aFrame->StylePosition();
  if (position->mZIndex.GetUnit() == eStyleUnit_Integer)
    return position->mZIndex.GetIntValue();

  // sort the auto and 0 elements together
  return 0;
}

int32_t
nsDisplayItem::ZIndex() const
{
  return ZIndexForFrame(mFrame);
}

bool
nsDisplayItem::ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                 nsRegion* aVisibleRegion)
{
  return !GetPaintRect().IsEmpty() &&
    !IsInvisibleInRect(aVisibleRegion->GetBounds());
}

bool
nsDisplayItem::RecomputeVisibility(nsDisplayListBuilder* aBuilder,
                                   nsRegion* aVisibleRegion,
                                   bool aUseClipBounds) {
  if (mForceNotVisible && !GetSameCoordinateSystemChildren()) {
    // mForceNotVisible wants to ensure that this display item doesn't render
    // anything itself. If this item has contents, then we obviously want to
    // render those, so we don't need this check in that case.
    NS_ASSERTION(GetBuildingRect().IsEmpty(),
      "invisible items without children should have empty vis rect");
    SetPaintRect(nsRect());
  } else {
    nsRect bounds;
    if (aUseClipBounds) {
      bounds = GetClippedBounds(aBuilder);
    } else {
      bool snap;
      bounds = GetBounds(aBuilder, &snap);
    }

    nsRegion itemVisible;
    itemVisible.And(*aVisibleRegion, bounds);
    SetPaintRect(itemVisible.GetBounds());
  }

  // When we recompute visibility within layers we don't need to
  // expand the visible region for content behind plugins (the plugin
  // is not in the layer).
  if (!ComputeVisibility(aBuilder, aVisibleRegion)) {
    SetPaintRect(nsRect());
    return false;
  }

  nsRegion opaque = TreatAsOpaque(this, aBuilder);
  aBuilder->SubtractFromVisibleRegion(aVisibleRegion, opaque);
  return true;
}

void
nsDisplayItem::SetClipChain(const DisplayItemClipChain* aClipChain,
                            bool aStore)
{
  mClipChain = aClipChain;
  mClip = DisplayItemClipChain::ClipForASR(aClipChain, mActiveScrolledRoot);

  if (aStore) {
    mState.mClipChain = mClipChain;
    mState.mClip = mClip;
  }
}

Maybe<nsRect>
nsDisplayItem::GetClipWithRespectToASR(nsDisplayListBuilder* aBuilder,
                                       const ActiveScrolledRoot* aASR) const
{
  if (const DisplayItemClip* clip = DisplayItemClipChain::ClipForASR(GetClipChain(), aASR)) {
    return Some(clip->GetClipRect());
  }
#ifdef DEBUG
  if (!gfxPrefs::LayoutUseContainersForRootFrames()) {
    MOZ_ASSERT(false, "item should have finite clip with respect to aASR");
  }
#endif
  return Nothing();
}

void
nsDisplayItem::FuseClipChainUpTo(nsDisplayListBuilder* aBuilder,
                                 const ActiveScrolledRoot* aASR)
{
  const DisplayItemClipChain* sc = mClipChain;
  DisplayItemClip mergedClip;
  while (sc && ActiveScrolledRoot::PickDescendant(aASR, sc->mASR) == sc->mASR) {
    mergedClip.IntersectWith(sc->mClip);
    sc = sc->mParent;
  }
  if (mergedClip.HasClip()) {
    mClipChain = aBuilder->AllocateDisplayItemClipChain(mergedClip, aASR, sc);
    mClip = &mClipChain->mClip;
  } else {
    mClipChain = nullptr;
    mClip = nullptr;
  }
}

bool
nsDisplayItem::ShouldUseAdvancedLayer(LayerManager* aManager, PrefFunc aFunc) const
{
  return CanUseAdvancedLayer(aManager) ? aFunc() : false;
}

bool
nsDisplayItem::CanUseAdvancedLayer(LayerManager* aManager) const
{
  return gfxPrefs::LayersAdvancedBasicLayerEnabled() ||
    !aManager ||
    aManager->GetBackendType() == layers::LayersBackend::LAYERS_WR;
}

static const DisplayItemClipChain*
FindCommonAncestorClipForIntersection(const DisplayItemClipChain* aOne,
                                      const DisplayItemClipChain* aTwo)
{
  for (const ActiveScrolledRoot* asr =
         ActiveScrolledRoot::PickDescendant(aOne->mASR, aTwo->mASR);
       asr;
       asr = asr->mParent) {
    if (aOne == aTwo) {
      return aOne;
    }
    if (aOne->mASR == asr) {
      aOne = aOne->mParent;
    }
    if (aTwo->mASR == asr) {
      aTwo = aTwo->mParent;
    }
    if (!aOne) {
      return aTwo;
    }
    if (!aTwo) {
      return aOne;
    }
  }
  return nullptr;
}

void
nsDisplayItem::IntersectClip(nsDisplayListBuilder* aBuilder,
                             const DisplayItemClipChain* aOther,
                             bool aStore)
{
  if (!aOther || mClipChain == aOther) {
    return;
  }

  // aOther might be a reference to a clip on the stack. We need to make sure
  // that CreateClipChainIntersection will allocate the actual intersected
  // clip in the builder's arena, so for the mClipChain == nullptr case,
  // we supply nullptr as the common ancestor so that CreateClipChainIntersection
  // clones the whole chain.
  const DisplayItemClipChain* ancestorClip =
    mClipChain ? FindCommonAncestorClipForIntersection(mClipChain, aOther) : nullptr;

  SetClipChain(aBuilder->CreateClipChainIntersection(ancestorClip, mClipChain, aOther),
               aStore);
}

nsRect
nsDisplayItem::GetClippedBounds(nsDisplayListBuilder* aBuilder) const
{
  bool snap;
  nsRect r = GetBounds(aBuilder, &snap);
  return GetClip().ApplyNonRoundedIntersection(r);
}

nsRect
nsDisplaySolidColor::GetBounds(nsDisplayListBuilder* aBuilder,
                               bool* aSnap) const
{
  *aSnap = true;
  return mBounds;
}

LayerState
nsDisplaySolidColor::GetLayerState(nsDisplayListBuilder* aBuilder,
                                   LayerManager* aManager,
                                   const ContainerLayerParameters& aParameters)
{
  if (ForceActiveLayers()) {
    return LAYER_ACTIVE;
  }
  return LAYER_NONE;
}

already_AddRefed<Layer>
nsDisplaySolidColor::BuildLayer(nsDisplayListBuilder* aBuilder,
                                LayerManager* aManager,
                                const ContainerLayerParameters& aContainerParameters)
{
  RefPtr<ColorLayer> layer = static_cast<ColorLayer*>
    (aManager->GetLayerBuilder()->GetLeafLayerFor(aBuilder, this));
  if (!layer) {
    layer = aManager->CreateColorLayer();
    if (!layer) {
      return nullptr;
    }
  }
  layer->SetColor(gfx::Color::FromABGR(mColor));

  const int32_t appUnitsPerDevPixel = mFrame->PresContext()->AppUnitsPerDevPixel();
  layer->SetBounds(mBounds.ToNearestPixels(appUnitsPerDevPixel));
  layer->SetBaseTransform(gfx::Matrix4x4::Translation(aContainerParameters.mOffset.x,
                                                      aContainerParameters.mOffset.y, 0));

  return layer.forget();
}

void
nsDisplaySolidColor::Paint(nsDisplayListBuilder* aBuilder,
                           gfxContext* aCtx)
{
  int32_t appUnitsPerDevPixel = mFrame->PresContext()->AppUnitsPerDevPixel();
  DrawTarget* drawTarget = aCtx->GetDrawTarget();
  Rect rect =
    NSRectToSnappedRect(GetPaintRect(), appUnitsPerDevPixel, *drawTarget);
  drawTarget->FillRect(rect, ColorPattern(ToDeviceColor(mColor)));
}

void
nsDisplaySolidColor::WriteDebugInfo(std::stringstream& aStream)
{
  aStream << " (rgba "
          << (int)NS_GET_R(mColor) << ","
          << (int)NS_GET_G(mColor) << ","
          << (int)NS_GET_B(mColor) << ","
          << (int)NS_GET_A(mColor) << ")";
}

bool
nsDisplaySolidColor::CreateWebRenderCommands(mozilla::wr::DisplayListBuilder& aBuilder,
                                             mozilla::wr::IpcResourceUpdateQueue& aResources,
                                             const StackingContextHelper& aSc,
                                             mozilla::layers::WebRenderLayerManager* aManager,
                                             nsDisplayListBuilder* aDisplayListBuilder)
{
  LayoutDeviceRect bounds = LayoutDeviceRect::FromAppUnits(
        GetPaintRect(), mFrame->PresContext()->AppUnitsPerDevPixel());
  wr::LayoutRect roundedRect = wr::ToRoundedLayoutRect(bounds);

  aBuilder.PushRect(roundedRect,
                    roundedRect,
                    !BackfaceIsHidden(),
                    wr::ToColorF(ToDeviceColor(mColor)));

  return true;
}

nsRect
nsDisplaySolidColorRegion::GetBounds(nsDisplayListBuilder* aBuilder,
                                     bool* aSnap) const
{
  *aSnap = true;
  return mRegion.GetBounds();
}

void
nsDisplaySolidColorRegion::Paint(nsDisplayListBuilder* aBuilder,
                                 gfxContext* aCtx)
{
  int32_t appUnitsPerDevPixel = mFrame->PresContext()->AppUnitsPerDevPixel();
  DrawTarget* drawTarget = aCtx->GetDrawTarget();
  ColorPattern color(mColor);
  for (auto iter = mRegion.RectIter(); !iter.Done(); iter.Next()) {
    Rect rect =
      NSRectToSnappedRect(iter.Get(), appUnitsPerDevPixel, *drawTarget);
    drawTarget->FillRect(rect, color);
  }
}

void
nsDisplaySolidColorRegion::WriteDebugInfo(std::stringstream& aStream)
{
  aStream << " (rgba "
          << int(mColor.r * 255) << ","
          << int(mColor.g * 255) << ","
          << int(mColor.b * 255) << ","
          << mColor.a << ")";
}

bool
nsDisplaySolidColorRegion::CreateWebRenderCommands(mozilla::wr::DisplayListBuilder& aBuilder,
                                                   mozilla::wr::IpcResourceUpdateQueue& aResources,
                                                   const StackingContextHelper& aSc,
                                                   mozilla::layers::WebRenderLayerManager* aManager,
                                                   nsDisplayListBuilder* aDisplayListBuilder)
{
  for (auto iter = mRegion.RectIter(); !iter.Done(); iter.Next()) {
    nsRect rect = iter.Get();
    LayoutDeviceRect layerRects = LayoutDeviceRect::FromAppUnits(
      rect, mFrame->PresContext()->AppUnitsPerDevPixel());
    wr::LayoutRect roundedRect = wr::ToRoundedLayoutRect(layerRects);
    aBuilder.PushRect(roundedRect,
                      roundedRect,
                      !BackfaceIsHidden(),
                      wr::ToColorF(ToDeviceColor(mColor)));
  }

  return true;
}

static void
RegisterThemeGeometry(nsDisplayListBuilder* aBuilder, nsDisplayItem* aItem,
                      nsIFrame* aFrame, nsITheme::ThemeGeometryType aType)
{
  if (aBuilder->IsInChromeDocumentOrPopup() && !aBuilder->IsInTransform()) {
    nsIFrame* displayRoot = nsLayoutUtils::GetDisplayRootFrame(aFrame);
    nsPoint offset = aBuilder->IsInSubdocument() ? aBuilder->ToReferenceFrame(aFrame)
                                                 : aFrame->GetOffsetTo(displayRoot);
    nsRect borderBox = nsRect(offset, aFrame->GetSize());
    aBuilder->RegisterThemeGeometry(aType, aItem,
      LayoutDeviceIntRect::FromUnknownRect(
        borderBox.ToNearestPixels(
          aFrame->PresContext()->AppUnitsPerDevPixel())));
  }
}

// Return the bounds of the viewport relative to |aFrame|'s reference frame.
// Returns Nothing() if transforming into |aFrame|'s coordinate space fails.
static Maybe<nsRect>
GetViewportRectRelativeToReferenceFrame(nsDisplayListBuilder* aBuilder,
                                        nsIFrame* aFrame)
{
  nsIFrame* rootFrame = aFrame->PresShell()->GetRootFrame();
  nsRect rootRect = rootFrame->GetRectRelativeToSelf();
  if (nsLayoutUtils::TransformRect(rootFrame, aFrame, rootRect) == nsLayoutUtils::TRANSFORM_SUCCEEDED) {
    return Some(rootRect + aBuilder->ToReferenceFrame(aFrame));
  }
  return Nothing();
}

/* static */ nsDisplayBackgroundImage::InitData
nsDisplayBackgroundImage::GetInitData(nsDisplayListBuilder* aBuilder,
                                      nsIFrame* aFrame,
                                      uint32_t aLayer,
                                      const nsRect& aBackgroundRect,
                                      ComputedStyle* aBackgroundStyle)
{
  nsPresContext* presContext = aFrame->PresContext();
  uint32_t flags = aBuilder->GetBackgroundPaintFlags();
  const nsStyleImageLayers::Layer &layer = aBackgroundStyle->StyleBackground()->mImage.mLayers[aLayer];

  bool isTransformedFixed;
  nsBackgroundLayerState state =
    nsCSSRendering::PrepareImageLayer(presContext, aFrame, flags,
                                      aBackgroundRect, aBackgroundRect, layer,
                                      &isTransformedFixed);

  // background-attachment:fixed is treated as background-attachment:scroll
  // if it's affected by a transform.
  // See https://www.w3.org/Bugs/Public/show_bug.cgi?id=17521.
  bool shouldTreatAsFixed =
    layer.mAttachment == StyleImageLayerAttachment::Fixed && !isTransformedFixed;

  bool shouldFixToViewport = shouldTreatAsFixed && !layer.mImage.IsEmpty();
  bool isRasterImage = state.mImageRenderer.IsRasterImage();
  nsCOMPtr<imgIContainer> image;
  if (isRasterImage) {
    image = state.mImageRenderer.GetImage();
  }
  return InitData{
    aBuilder, aFrame, aBackgroundStyle, image, aBackgroundRect,
    state.mFillArea, state.mDestArea, aLayer, isRasterImage,
    shouldFixToViewport
  };
}

nsDisplayBackgroundImage::nsDisplayBackgroundImage(nsDisplayListBuilder* aBuilder,
                                                   const InitData& aInitData,
                                                   nsIFrame* aFrameForBounds)
  : nsDisplayImageContainer(aBuilder, aInitData.frame)
  , mBackgroundStyle(aInitData.backgroundStyle)
  , mImage(aInitData.image)
  , mDependentFrame(nullptr)
  , mBackgroundRect(aInitData.backgroundRect)
  , mFillRect(aInitData.fillArea)
  , mDestRect(aInitData.destArea)
  , mLayer(aInitData.layer)
  , mIsRasterImage(aInitData.isRasterImage)
  , mShouldFixToViewport(aInitData.shouldFixToViewport)
  , mImageFlags(0)
{
  MOZ_COUNT_CTOR(nsDisplayBackgroundImage);

  mBounds = GetBoundsInternal(aInitData.builder, aFrameForBounds);
  if (mShouldFixToViewport) {
    mAnimatedGeometryRoot = aInitData.builder->FindAnimatedGeometryRootFor(this);

    // Expand the item's visible rect to cover the entire bounds, limited to the
    // viewport rect. This is necessary because the background's clip can move
    // asynchronously.
    if (Maybe<nsRect> viewportRect = GetViewportRectRelativeToReferenceFrame(aInitData.builder, mFrame)) {
      SetBuildingRect(mBounds.Intersect(*viewportRect));
    }
  }
}

nsDisplayBackgroundImage::~nsDisplayBackgroundImage()
{
#ifdef NS_BUILD_REFCNT_LOGGING
  MOZ_COUNT_DTOR(nsDisplayBackgroundImage);
#endif
  if (mDependentFrame) {
    mDependentFrame->RemoveDisplayItem(this);
  }
}

static nsIFrame* GetBackgroundComputedStyleFrame(nsIFrame* aFrame)
{
  nsIFrame* f;
  if (!nsCSSRendering::FindBackgroundFrame(aFrame, &f)) {
    // We don't want to bail out if moz-appearance is set on a root
    // node. If it has a parent content node, bail because it's not
    // a root, other wise keep going in order to let the theme stuff
    // draw the background. The canvas really should be drawing the
    // bg, but there's no way to hook that up via css.
    if (!aFrame->StyleDisplay()->mAppearance) {
      return nullptr;
    }

    nsIContent* content = aFrame->GetContent();
    if (!content || content->GetParent()) {
      return nullptr;
    }

    f = aFrame;
  }
  return f;
}

static void
SetBackgroundClipRegion(DisplayListClipState::AutoSaveRestore& aClipState,
                        nsIFrame* aFrame, const nsPoint& aToReferenceFrame,
                        const nsStyleImageLayers::Layer& aLayer,
                        const nsRect& aBackgroundRect,
                        bool aWillPaintBorder)
{
  nsCSSRendering::ImageLayerClipState clip;
  nsCSSRendering::GetImageLayerClip(aLayer, aFrame, *aFrame->StyleBorder(),
                                    aBackgroundRect, aBackgroundRect, aWillPaintBorder,
                                    aFrame->PresContext()->AppUnitsPerDevPixel(),
                                    &clip);

  if (clip.mHasAdditionalBGClipArea) {
    aClipState.ClipContentDescendants(clip.mAdditionalBGClipArea, clip.mBGClipArea,
                                      clip.mHasRoundedCorners ? clip.mRadii : nullptr);
  } else {
    aClipState.ClipContentDescendants(clip.mBGClipArea, clip.mHasRoundedCorners ? clip.mRadii : nullptr);
  }
}

/**
 * This is used for the find bar highlighter overlay. It's only accessible
 * through the AnonymousContent API, so it's not exposed to general web pages.
 */
static bool
SpecialCutoutRegionCase(nsDisplayListBuilder* aBuilder,
                        nsIFrame* aFrame,
                        const nsRect& aBackgroundRect,
                        nsDisplayList* aList,
                        nscolor aColor)
{
  nsIContent* content = aFrame->GetContent();
  if (!content) {
    return false;
  }

  void* cutoutRegion = content->GetProperty(nsGkAtoms::cutoutregion);
  if (!cutoutRegion) {
    return false;
  }

  if (NS_GET_A(aColor) == 0) {
    return true;
  }

  nsRegion region;
  region.Sub(aBackgroundRect, *static_cast<nsRegion*>(cutoutRegion));
  region.MoveBy(aBuilder->ToReferenceFrame(aFrame));
  aList->AppendToTop(
    MakeDisplayItem<nsDisplaySolidColorRegion>(aBuilder, aFrame, region, aColor));

  return true;
}


/*static*/ bool
nsDisplayBackgroundImage::AppendBackgroundItemsToTop(nsDisplayListBuilder* aBuilder,
                                                     nsIFrame* aFrame,
                                                     const nsRect& aBackgroundRect,
                                                     nsDisplayList* aList,
                                                     bool aAllowWillPaintBorderOptimization,
                                                     ComputedStyle* aComputedStyle,
                                                     const nsRect& aBackgroundOriginRect,
                                                     nsIFrame* aSecondaryReferenceFrame)
{
  ComputedStyle* bgSC = aComputedStyle;
  const nsStyleBackground* bg = nullptr;
  nsRect bgRect = aBackgroundRect + aBuilder->ToReferenceFrame(aFrame);
  nsRect bgOriginRect = bgRect;
  if (!aBackgroundOriginRect.IsEmpty()) {
    bgOriginRect = aBackgroundOriginRect + aBuilder->ToReferenceFrame(aFrame);
  }
  nsPresContext* presContext = aFrame->PresContext();
  bool isThemed = aFrame->IsThemed();
  nsIFrame* dependentFrame = nullptr;
  if (!isThemed) {
    if (!bgSC) {
      dependentFrame = GetBackgroundComputedStyleFrame(aFrame);
      if (dependentFrame) {
        bgSC = dependentFrame->Style();
        if (dependentFrame == aFrame) {
          dependentFrame = nullptr;
        }
      }
    }
    if (bgSC) {
      bg = bgSC->StyleBackground();
    }
  }

  bool drawBackgroundColor = false;
  // Dummy initialisation to keep Valgrind/Memcheck happy.
  // See bug 1122375 comment 1.
  nscolor color = NS_RGBA(0,0,0,0);
  if (!nsCSSRendering::IsCanvasFrame(aFrame) && bg) {
    bool drawBackgroundImage;
    color =
      nsCSSRendering::DetermineBackgroundColor(presContext, bgSC, aFrame,
                                               drawBackgroundImage, drawBackgroundColor);
  }

  if (SpecialCutoutRegionCase(aBuilder, aFrame, aBackgroundRect, aList, color)) {
    return false;
  }

  const nsStyleBorder* borderStyle = aFrame->StyleBorder();
  const nsStyleEffects* effectsStyle = aFrame->StyleEffects();
  bool hasInsetShadow = effectsStyle->mBoxShadow &&
                        effectsStyle->mBoxShadow->HasShadowWithInset(true);
  bool willPaintBorder = aAllowWillPaintBorderOptimization &&
                         !isThemed && !hasInsetShadow &&
                         borderStyle->HasBorder();

  nsPoint toRef = aBuilder->ToReferenceFrame(aFrame);

  // An auxiliary list is necessary in case we have background blending; if that
  // is the case, background items need to be wrapped by a blend container to
  // isolate blending to the background
  nsDisplayList bgItemList;
  // Even if we don't actually have a background color to paint, we may still need
  // to create an item for hit testing.
  if ((drawBackgroundColor && color != NS_RGBA(0,0,0,0)) ||
      aBuilder->IsForEventDelivery()) {
    Maybe<DisplayListClipState::AutoSaveRestore> clipState;
    nsRect bgColorRect = bgRect;
    if (bg && !aBuilder->IsForEventDelivery()) {
      // Disable the will-paint-border optimization for background
      // colors with no border-radius. Enabling it for background colors
      // doesn't help much (there are no tiling issues) and clipping the
      // background breaks detection of the element's border-box being
      // opaque. For nonzero border-radius we still need it because we
      // want to inset the background if possible to avoid antialiasing
      // artifacts along the rounded corners.
      bool useWillPaintBorderOptimization = willPaintBorder &&
          nsLayoutUtils::HasNonZeroCorner(borderStyle->mBorderRadius);

      nsCSSRendering::ImageLayerClipState clip;
      nsCSSRendering::GetImageLayerClip(bg->BottomLayer(), aFrame, *aFrame->StyleBorder(),
                                        bgRect, bgRect, useWillPaintBorderOptimization,
                                        aFrame->PresContext()->AppUnitsPerDevPixel(),
                                        &clip);

      bgColorRect = bgColorRect.Intersect(clip.mBGClipArea);
      if (clip.mHasAdditionalBGClipArea) {
        bgColorRect = bgColorRect.Intersect(clip.mAdditionalBGClipArea);
      }
      if (clip.mHasRoundedCorners) {
        clipState.emplace(aBuilder);
        clipState->ClipContentDescendants(clip.mBGClipArea, clip.mRadii);
      }
    }
    nsDisplayBackgroundColor *bgItem;
    if (aSecondaryReferenceFrame) {
      bgItem =
          MakeDisplayItem<nsDisplayTableBackgroundColor>(aBuilder, aSecondaryReferenceFrame, bgColorRect, bgSC,
                                                       drawBackgroundColor ? color : NS_RGBA(0, 0, 0, 0),
                                                       aFrame);
    } else {
      bgItem =
          MakeDisplayItem<nsDisplayBackgroundColor>(aBuilder, aFrame, bgColorRect, bgSC,
                                                  drawBackgroundColor ? color : NS_RGBA(0, 0, 0, 0));
    }
    bgItem->SetDependentFrame(aBuilder, dependentFrame);
    bgItemList.AppendToTop(bgItem);
  }

  if (isThemed) {
    nsITheme* theme = presContext->GetTheme();
    if (theme->NeedToClearBackgroundBehindWidget(aFrame, aFrame->StyleDisplay()->mAppearance) &&
        aBuilder->IsInChromeDocumentOrPopup() && !aBuilder->IsInTransform()) {
      bgItemList.AppendToTop(
        MakeDisplayItem<nsDisplayClearBackground>(aBuilder, aFrame));
    }
    if (aSecondaryReferenceFrame) {
      nsDisplayTableThemedBackground* bgItem =
        MakeDisplayItem<nsDisplayTableThemedBackground>(aBuilder,
                                                      aSecondaryReferenceFrame,
                                                      bgRect,
                                                      aFrame);
      bgItem->Init(aBuilder);
      bgItemList.AppendToTop(bgItem);
    } else {
      nsDisplayThemedBackground* bgItem =
        MakeDisplayItem<nsDisplayThemedBackground>(aBuilder, aFrame, bgRect);
      bgItem->Init(aBuilder);
      bgItemList.AppendToTop(bgItem);
    }
    aList->AppendToTop(&bgItemList);
    return true;
  }

  if (!bg) {
    aList->AppendToTop(&bgItemList);
    return false;
  }

  const ActiveScrolledRoot* asr =
    aBuilder->CurrentActiveScrolledRoot();

  bool needBlendContainer = false;

  // Passing bg == nullptr in this macro will result in one iteration with
  // i = 0.
  NS_FOR_VISIBLE_IMAGE_LAYERS_BACK_TO_FRONT(i, bg->mImage) {
    if (bg->mImage.mLayers[i].mImage.IsEmpty()) {
      continue;
    }

    if (bg->mImage.mLayers[i].mBlendMode != NS_STYLE_BLEND_NORMAL) {
      needBlendContainer = true;
    }

    DisplayListClipState::AutoSaveRestore clipState(aBuilder);
    if (!aBuilder->IsForEventDelivery()) {
      const nsStyleImageLayers::Layer& layer = bg->mImage.mLayers[i];
      SetBackgroundClipRegion(clipState, aFrame, toRef,
                              layer, bgRect, willPaintBorder);
    }

    nsDisplayList thisItemList;
    nsDisplayBackgroundImage::InitData bgData =
      nsDisplayBackgroundImage::GetInitData(aBuilder, aFrame, i, bgOriginRect, bgSC);

    if (bgData.shouldFixToViewport) {

      auto* displayData = aBuilder->GetCurrentFixedBackgroundDisplayData();
      nsDisplayListBuilder::AutoBuildingDisplayList
        buildingDisplayList(aBuilder, aFrame, aBuilder->GetVisibleRect(), aBuilder->GetDirtyRect(), false);

      nsDisplayListBuilder::AutoCurrentActiveScrolledRootSetter asrSetter(aBuilder);
      if (displayData) {
        asrSetter.SetCurrentActiveScrolledRoot(
          displayData->mContainingBlockActiveScrolledRoot);
        if (nsLayoutUtils::UsesAsyncScrolling(aFrame)) {
          // Override the dirty rect on the builder to be the dirty rect of
          // the viewport.
          // displayData->mDirtyRect is relative to the presshell's viewport
          // frame (the root frame), and we need it to be relative to aFrame.
          nsIFrame* rootFrame = aBuilder->CurrentPresShellState()->mPresShell->GetRootFrame();
          // There cannot be any transforms between aFrame and rootFrame
          // because then bgData.shouldFixToViewport would have been false.
          nsRect visibleRect = displayData->mVisibleRect + aFrame->GetOffsetTo(rootFrame);
          aBuilder->SetVisibleRect(visibleRect);
          nsRect dirtyRect = displayData->mDirtyRect + aFrame->GetOffsetTo(rootFrame);
          aBuilder->SetDirtyRect(dirtyRect);
        }
      }
      nsDisplayBackgroundImage* bgItem = nullptr;
      {
        // The clip is captured by the nsDisplayFixedPosition, so clear the
        // clip for the nsDisplayBackgroundImage inside.
        DisplayListClipState::AutoSaveRestore bgImageClip(aBuilder);
        bgImageClip.Clear();
        if (aSecondaryReferenceFrame) {
          nsDisplayBackgroundImage::InitData tableData = bgData;
          nsIFrame* styleFrame = tableData.frame;
          tableData.frame = aSecondaryReferenceFrame;
          bgItem = MakeDisplayItem<nsDisplayTableBackgroundImage>(aBuilder, tableData, styleFrame);
        } else {
          bgItem = MakeDisplayItem<nsDisplayBackgroundImage>(aBuilder, bgData);
        }
      }
      bgItem->SetDependentFrame(aBuilder, dependentFrame);
      if (aSecondaryReferenceFrame) {
        thisItemList.AppendToTop(
          nsDisplayTableFixedPosition::CreateForFixedBackground(aBuilder,
                                                                aSecondaryReferenceFrame,
                                                                bgItem,
                                                                i,
                                                                aFrame));
      } else {
        thisItemList.AppendToTop(
          nsDisplayFixedPosition::CreateForFixedBackground(aBuilder, aFrame, bgItem, i));
      }

    } else {
      nsDisplayBackgroundImage* bgItem;
      if (aSecondaryReferenceFrame) {
        nsDisplayBackgroundImage::InitData tableData = bgData;
        nsIFrame* styleFrame = tableData.frame;
        tableData.frame = aSecondaryReferenceFrame;

        bgItem = MakeDisplayItem<nsDisplayTableBackgroundImage>(aBuilder, tableData, styleFrame);
      } else {
        bgItem = MakeDisplayItem<nsDisplayBackgroundImage>(aBuilder, bgData);
      }
      bgItem->SetDependentFrame(aBuilder, dependentFrame);
      thisItemList.AppendToTop(bgItem);
    }

    if (bg->mImage.mLayers[i].mBlendMode != NS_STYLE_BLEND_NORMAL) {
      DisplayListClipState::AutoSaveRestore blendClip(aBuilder);
      // asr is scrolled. Even if we wrap a fixed background layer, that's
      // fine, because the item will have a scrolled clip that limits the
      // item with respect to asr.
      if (aSecondaryReferenceFrame) {
        thisItemList.AppendToTop(
          MakeDisplayItem<nsDisplayTableBlendMode>(aBuilder, aSecondaryReferenceFrame, &thisItemList,
                                                 bg->mImage.mLayers[i].mBlendMode,
                                                 asr, i + 1, aFrame));
      } else {
        thisItemList.AppendToTop(
          MakeDisplayItem<nsDisplayBlendMode>(aBuilder, aFrame, &thisItemList,
                                            bg->mImage.mLayers[i].mBlendMode,
                                            asr, i + 1));
      }
    }
    bgItemList.AppendToTop(&thisItemList);
  }

  if (needBlendContainer) {
    DisplayListClipState::AutoSaveRestore blendContainerClip(aBuilder);
    if (aSecondaryReferenceFrame) {
      bgItemList.AppendToTop(
        nsDisplayTableBlendContainer::CreateForBackgroundBlendMode(aBuilder, aSecondaryReferenceFrame,
                                                                   &bgItemList, asr, aFrame));
    } else {
      bgItemList.AppendToTop(
        nsDisplayBlendContainer::CreateForBackgroundBlendMode(aBuilder, aFrame, &bgItemList, asr));
    }
  }

  aList->AppendToTop(&bgItemList);
  return false;
}

// Check that the rounded border of aFrame, added to aToReferenceFrame,
// intersects aRect.  Assumes that the unrounded border has already
// been checked for intersection.
static bool
RoundedBorderIntersectsRect(nsIFrame* aFrame,
                            const nsPoint& aFrameToReferenceFrame,
                            const nsRect& aTestRect)
{
  if (!nsRect(aFrameToReferenceFrame, aFrame->GetSize()).Intersects(aTestRect))
    return false;

  nscoord radii[8];
  return !aFrame->GetBorderRadii(radii) ||
         nsLayoutUtils::RoundedRectIntersectsRect(nsRect(aFrameToReferenceFrame,
                                                  aFrame->GetSize()),
                                                  radii, aTestRect);
}

// Returns TRUE if aContainedRect is guaranteed to be contained in
// the rounded rect defined by aRoundedRect and aRadii. Complex cases are
// handled conservatively by returning FALSE in some situations where
// a more thorough analysis could return TRUE.
//
// See also RoundedRectIntersectsRect.
static bool RoundedRectContainsRect(const nsRect& aRoundedRect,
                                    const nscoord aRadii[8],
                                    const nsRect& aContainedRect) {
  nsRegion rgn = nsLayoutUtils::RoundedRectIntersectRect(aRoundedRect, aRadii, aContainedRect);
  return rgn.Contains(aContainedRect);
}

bool
nsDisplayBackgroundImage::CanOptimizeToImageLayer(LayerManager* aManager,
                                                  nsDisplayListBuilder* aBuilder)
{
  if (!mBackgroundStyle) {
    return false;
  }

  // We currently can't handle tiled backgrounds.
  if (!mDestRect.Contains(mFillRect)) {
    return false;
  }

  // For 'contain' and 'cover', we allow any pixel of the image to be sampled
  // because there isn't going to be any spriting/atlasing going on.
  const nsStyleImageLayers::Layer &layer = mBackgroundStyle->StyleBackground()->mImage.mLayers[mLayer];
  bool allowPartialImages =
    (layer.mSize.mWidthType == nsStyleImageLayers::Size::eContain ||
     layer.mSize.mWidthType == nsStyleImageLayers::Size::eCover);
  if (!allowPartialImages && !mFillRect.Contains(mDestRect)) {
    return false;
  }

  return nsDisplayImageContainer::CanOptimizeToImageLayer(aManager, aBuilder);
}

nsRect
nsDisplayBackgroundImage::GetDestRect() const
{
  return mDestRect;
}

already_AddRefed<imgIContainer>
nsDisplayBackgroundImage::GetImage()
{
  nsCOMPtr<imgIContainer> image = mImage;
  return image.forget();
}

nsDisplayBackgroundImage::ImageLayerization
nsDisplayBackgroundImage::ShouldCreateOwnLayer(nsDisplayListBuilder* aBuilder,
                                               LayerManager* aManager)
{
  if (ForceActiveLayers()) {
    return WHENEVER_POSSIBLE;
  }

  nsIFrame* backgroundStyleFrame = nsCSSRendering::FindBackgroundStyleFrame(StyleFrame());
  if (ActiveLayerTracker::IsBackgroundPositionAnimated(aBuilder,
                                                       backgroundStyleFrame)) {
    return WHENEVER_POSSIBLE;
  }

  if (nsLayoutUtils::AnimatedImageLayersEnabled() && mBackgroundStyle) {
    const nsStyleImageLayers::Layer &layer = mBackgroundStyle->StyleBackground()->mImage.mLayers[mLayer];
    const nsStyleImage* image = &layer.mImage;
    if (image->GetType() == eStyleImageType_Image) {
      imgIRequest* imgreq = image->GetImageData();
      nsCOMPtr<imgIContainer> image;
      if (imgreq &&
          NS_SUCCEEDED(imgreq->GetImage(getter_AddRefs(image))) &&
          image) {
        bool animated = false;
        if (NS_SUCCEEDED(image->GetAnimated(&animated)) && animated) {
          return WHENEVER_POSSIBLE;
        }
      }
    }
  }

  if (nsLayoutUtils::GPUImageScalingEnabled() &&
      aManager->IsCompositingCheap()) {
    return ONLY_FOR_SCALING;
  }

  return NO_LAYER_NEEDED;
}

static void CheckForBorderItem(nsDisplayItem *aItem, uint32_t& aFlags)
{
  nsDisplayItem* nextItem = aItem->GetAbove();
  while (nextItem && nextItem->GetType() == DisplayItemType::TYPE_BACKGROUND) {
    nextItem = nextItem->GetAbove();
  }
  if (nextItem &&
      nextItem->Frame() == aItem->Frame() &&
      nextItem->GetType() == DisplayItemType::TYPE_BORDER) {
    aFlags |= nsCSSRendering::PAINTBG_WILL_PAINT_BORDER;
  }
}

LayerState
nsDisplayBackgroundImage::GetLayerState(nsDisplayListBuilder* aBuilder,
                                        LayerManager* aManager,
                                        const ContainerLayerParameters& aParameters)
{
  mImageFlags = aBuilder->GetBackgroundPaintFlags();
  CheckForBorderItem(this, mImageFlags);

  ImageLayerization shouldLayerize = ShouldCreateOwnLayer(aBuilder, aManager);
  if (shouldLayerize == NO_LAYER_NEEDED) {
    // We can skip the call to CanOptimizeToImageLayer if we don't want a
    // layer anyway.
    return LAYER_NONE;
  }

  if (CanOptimizeToImageLayer(aManager, aBuilder)) {
    if (shouldLayerize == WHENEVER_POSSIBLE) {
      return LAYER_ACTIVE;
    }

    MOZ_ASSERT(shouldLayerize == ONLY_FOR_SCALING, "unhandled ImageLayerization value?");

    MOZ_ASSERT(mImage);
    int32_t imageWidth;
    int32_t imageHeight;
    mImage->GetWidth(&imageWidth);
    mImage->GetHeight(&imageHeight);
    NS_ASSERTION(imageWidth != 0 && imageHeight != 0, "Invalid image size!");

    int32_t appUnitsPerDevPixel = mFrame->PresContext()->AppUnitsPerDevPixel();
    LayoutDeviceRect destRect = LayoutDeviceRect::FromAppUnits(GetDestRect(), appUnitsPerDevPixel);

    const LayerRect destLayerRect = destRect * aParameters.Scale();

    // Calculate the scaling factor for the frame.
    const gfxSize scale = gfxSize(destLayerRect.width / imageWidth,
                                  destLayerRect.height / imageHeight);

    if ((scale.width != 1.0f || scale.height != 1.0f) &&
        (destLayerRect.width * destLayerRect.height >= 64 * 64)) {
      // Separate this image into a layer.
      // There's no point in doing this if we are not scaling at all or if the
      // target size is pretty small.
      return LAYER_ACTIVE;
    }
  }

  return LAYER_NONE;
}

already_AddRefed<Layer>
nsDisplayBackgroundImage::BuildLayer(nsDisplayListBuilder* aBuilder,
                                     LayerManager* aManager,
                                     const ContainerLayerParameters& aParameters)
{
  RefPtr<ImageLayer> layer = static_cast<ImageLayer*>
    (aManager->GetLayerBuilder()->GetLeafLayerFor(aBuilder, this));
  if (!layer) {
    layer = aManager->CreateImageLayer();
    if (!layer)
      return nullptr;
  }
  RefPtr<ImageContainer> imageContainer = GetContainer(aManager, aBuilder);
  layer->SetContainer(imageContainer);
  ConfigureLayer(layer, aParameters);
  return layer.forget();
}

bool
nsDisplayBackgroundImage::CanBuildWebRenderDisplayItems(LayerManager* aManager, nsDisplayListBuilder* aDisplayListBuilder)
{
  if (aDisplayListBuilder) {
    mImageFlags = aDisplayListBuilder->GetBackgroundPaintFlags();
  }

  return mBackgroundStyle->StyleBackground()->mImage.mLayers[mLayer].mClip != StyleGeometryBox::Text &&
         nsCSSRendering::CanBuildWebRenderDisplayItemsForStyleImageLayer(aManager,
                                                                         *StyleFrame()->PresContext(),
                                                                         StyleFrame(),
                                                                         mBackgroundStyle->StyleBackground(),
                                                                         mLayer,
                                                                         mImageFlags);
}

bool
nsDisplayBackgroundImage::CreateWebRenderCommands(mozilla::wr::DisplayListBuilder& aBuilder,
                                                  mozilla::wr::IpcResourceUpdateQueue& aResources,
                                                  const StackingContextHelper& aSc,
                                                  WebRenderLayerManager* aManager,
                                                  nsDisplayListBuilder* aDisplayListBuilder)
{
  ContainerLayerParameters parameter;
  if (!CanBuildWebRenderDisplayItems(aManager, aDisplayListBuilder)) {
    return false;
  }

  CheckForBorderItem(this, mImageFlags);
  nsCSSRendering::PaintBGParams params =
    nsCSSRendering::PaintBGParams::ForSingleLayer(*StyleFrame()->PresContext(),
                                                  GetPaintRect(), mBackgroundRect,
                                                  StyleFrame(), mImageFlags, mLayer,
                                                  CompositionOp::OP_OVER);
  params.bgClipRect = &mBounds;
  ImgDrawResult result =
    nsCSSRendering::BuildWebRenderDisplayItemsForStyleImageLayer(params, aBuilder, aResources, aSc, aManager, this);
  nsDisplayBackgroundGeometry::UpdateDrawResult(this, result);

  return true;
}

void
nsDisplayBackgroundImage::HitTest(nsDisplayListBuilder* aBuilder,
                                  const nsRect& aRect,
                                  HitTestState* aState,
                                  nsTArray<nsIFrame*> *aOutFrames)
{
  if (RoundedBorderIntersectsRect(mFrame, ToReferenceFrame(), aRect)) {
    aOutFrames->AppendElement(mFrame);
  }
}

bool
nsDisplayBackgroundImage::ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                            nsRegion* aVisibleRegion)
{
  if (!nsDisplayItem::ComputeVisibility(aBuilder, aVisibleRegion)) {
    return false;
  }

  // Return false if the background was propagated away from this
  // frame. We don't want this display item to show up and confuse
  // anything.
  return mBackgroundStyle;
}

/* static */ nsRegion
nsDisplayBackgroundImage::GetInsideClipRegion(const nsDisplayItem* aItem,
                                              StyleGeometryBox aClip,
                                              const nsRect& aRect,
                                              const nsRect& aBackgroundRect)
{
  nsRegion result;
  if (aRect.IsEmpty())
    return result;

  nsIFrame *frame = aItem->Frame();

  nsRect clipRect = aBackgroundRect;
  if (frame->IsCanvasFrame()) {
    nsCanvasFrame* canvasFrame = static_cast<nsCanvasFrame*>(frame);
    clipRect = canvasFrame->CanvasArea() + aItem->ToReferenceFrame();
  } else if (aClip == StyleGeometryBox::PaddingBox ||
             aClip == StyleGeometryBox::ContentBox) {
    nsMargin border = frame->GetUsedBorder();
    if (aClip == StyleGeometryBox::ContentBox) {
      border += frame->GetUsedPadding();
    }
    border.ApplySkipSides(frame->GetSkipSides());
    clipRect.Deflate(border);
  }

  return clipRect.Intersect(aRect);
}

nsRegion
nsDisplayBackgroundImage::GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                                          bool* aSnap) const
{
  nsRegion result;
  *aSnap = false;

  if (!mBackgroundStyle)
    return result;

  *aSnap = true;

  // For StyleBoxDecorationBreak::Slice, don't try to optimize here, since
  // this could easily lead to O(N^2) behavior inside InlineBackgroundData,
  // which expects frames to be sent to it in content order, not reverse
  // content order which we'll produce here.
  // Of course, if there's only one frame in the flow, it doesn't matter.
  if (mFrame->StyleBorder()->mBoxDecorationBreak ==
        StyleBoxDecorationBreak::Clone ||
      (!mFrame->GetPrevContinuation() && !mFrame->GetNextContinuation())) {
    const nsStyleImageLayers::Layer& layer = mBackgroundStyle->StyleBackground()->mImage.mLayers[mLayer];
    if (layer.mImage.IsOpaque() && layer.mBlendMode == NS_STYLE_BLEND_NORMAL &&
        layer.mRepeat.mXRepeat != StyleImageLayerRepeat::Space &&
        layer.mRepeat.mYRepeat != StyleImageLayerRepeat::Space &&
        layer.mClip != StyleGeometryBox::Text) {
      result = GetInsideClipRegion(this, layer.mClip, mBounds, mBackgroundRect);
    }
  }

  return result;
}

Maybe<nscolor>
nsDisplayBackgroundImage::IsUniform(nsDisplayListBuilder* aBuilder) const
{
  if (!mBackgroundStyle) {
    return Some(NS_RGBA(0,0,0,0));
  }
  return Nothing();
}

nsRect
nsDisplayBackgroundImage::GetPositioningArea() const
{
  if (!mBackgroundStyle) {
    return nsRect();
  }
  nsIFrame* attachedToFrame;
  bool transformedFixed;
  return nsCSSRendering::ComputeImageLayerPositioningArea(
      mFrame->PresContext(), mFrame,
      mBackgroundRect,
      mBackgroundStyle->StyleBackground()->mImage.mLayers[mLayer],
      &attachedToFrame,
      &transformedFixed) + ToReferenceFrame();
}

bool
nsDisplayBackgroundImage::RenderingMightDependOnPositioningAreaSizeChange() const
{
  if (!mBackgroundStyle)
    return false;

  nscoord radii[8];
  if (mFrame->GetBorderRadii(radii)) {
    // A change in the size of the positioning area might change the position
    // of the rounded corners.
    return true;
  }

  const nsStyleImageLayers::Layer &layer = mBackgroundStyle->StyleBackground()->mImage.mLayers[mLayer];
  if (layer.RenderingMightDependOnPositioningAreaSizeChange()) {
    return true;
  }
  return false;
}

void
nsDisplayBackgroundImage::Paint(nsDisplayListBuilder* aBuilder,
                                gfxContext* aCtx) {
  PaintInternal(aBuilder, aCtx, GetPaintRect(), &mBounds);
}

void
nsDisplayBackgroundImage::PaintInternal(nsDisplayListBuilder* aBuilder,
                                        gfxContext* aCtx, const nsRect& aBounds,
                                        nsRect* aClipRect) {
  gfxContext* ctx = aCtx;
  StyleGeometryBox clip = mBackgroundStyle->StyleBackground()->mImage.mLayers[mLayer].mClip;

  if (clip == StyleGeometryBox::Text) {
    if (!GenerateAndPushTextMask(StyleFrame(), aCtx, mBackgroundRect, aBuilder)) {
      return;
    }
  }

  nsCSSRendering::PaintBGParams params =
    nsCSSRendering::PaintBGParams::ForSingleLayer(*StyleFrame()->PresContext(),
                                                  aBounds, mBackgroundRect,
                                                  StyleFrame(), mImageFlags, mLayer,
                                                  CompositionOp::OP_OVER);
  params.bgClipRect = aClipRect;
  ImgDrawResult result = nsCSSRendering::PaintStyleImageLayer(params, *aCtx);

  if (clip == StyleGeometryBox::Text) {
    ctx->PopGroupAndBlend();
  }

  nsDisplayBackgroundGeometry::UpdateDrawResult(this, result);
}

void
nsDisplayBackgroundImage::ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                                    const nsDisplayItemGeometry* aGeometry,
                                                    nsRegion* aInvalidRegion) const
{
  if (!mBackgroundStyle) {
    return;
  }

  const nsDisplayBackgroundGeometry* geometry = static_cast<const nsDisplayBackgroundGeometry*>(aGeometry);

  bool snap;
  nsRect bounds = GetBounds(aBuilder, &snap);
  nsRect positioningArea = GetPositioningArea();
  if (positioningArea.TopLeft() != geometry->mPositioningArea.TopLeft() ||
      (positioningArea.Size() != geometry->mPositioningArea.Size() &&
       RenderingMightDependOnPositioningAreaSizeChange())) {
    // Positioning area changed in a way that could cause everything to change,
    // so invalidate everything (both old and new painting areas).
    aInvalidRegion->Or(bounds, geometry->mBounds);
    return;
  }
  if (!mDestRect.IsEqualInterior(geometry->mDestRect)) {
    // Dest area changed in a way that could cause everything to change,
    // so invalidate everything (both old and new painting areas).
    aInvalidRegion->Or(bounds, geometry->mBounds);
    return;
  }
  if (aBuilder->ShouldSyncDecodeImages()) {
    const nsStyleImage& image = mBackgroundStyle->StyleBackground()->mImage.mLayers[mLayer].mImage;
    if (image.GetType() == eStyleImageType_Image &&
        geometry->ShouldInvalidateToSyncDecodeImages()) {
      aInvalidRegion->Or(*aInvalidRegion, bounds);
    }
  }
  if (!bounds.IsEqualInterior(geometry->mBounds)) {
    // Positioning area is unchanged, so invalidate just the change in the
    // painting area.
    aInvalidRegion->Xor(bounds, geometry->mBounds);
  }
}

nsRect
nsDisplayBackgroundImage::GetBounds(nsDisplayListBuilder* aBuilder,
                                    bool* aSnap) const
{
  *aSnap = true;
  return mBounds;
}

nsRect
nsDisplayBackgroundImage::GetBoundsInternal(nsDisplayListBuilder* aBuilder,
                                            nsIFrame* aFrameForBounds)
{
  // This allows nsDisplayTableBackgroundImage to change the frame used for
  // bounds calculation.
  nsIFrame* frame = aFrameForBounds ? aFrameForBounds : mFrame;

  nsPresContext* presContext = frame->PresContext();

  if (!mBackgroundStyle) {
    return nsRect();
  }

  nsRect clipRect = mBackgroundRect;
  if (frame->IsCanvasFrame()) {
    nsCanvasFrame* canvasFrame = static_cast<nsCanvasFrame*>(frame);
    clipRect = canvasFrame->CanvasArea() + ToReferenceFrame();
  }
  const nsStyleImageLayers::Layer& layer = mBackgroundStyle->StyleBackground()->mImage.mLayers[mLayer];
  return nsCSSRendering::GetBackgroundLayerRect(presContext, frame,
                                                mBackgroundRect, clipRect, layer,
                                                aBuilder->GetBackgroundPaintFlags());
}

nsDisplayTableBackgroundImage::nsDisplayTableBackgroundImage(nsDisplayListBuilder* aBuilder,
                                                             const InitData& aData,
                                                             nsIFrame* aCellFrame)
  : nsDisplayBackgroundImage(aBuilder, aData, aCellFrame)
  , mStyleFrame(aCellFrame)
  , mTableType(GetTableTypeFromFrame(mStyleFrame))
{
  if (aBuilder->IsRetainingDisplayList()) {
    mStyleFrame->AddDisplayItem(this);
  }
}

nsDisplayTableBackgroundImage::~nsDisplayTableBackgroundImage()
{
  if (mStyleFrame) {
    mStyleFrame->RemoveDisplayItem(this);
  }
}

bool
nsDisplayTableBackgroundImage::IsInvalid(nsRect& aRect) const
{
  bool result = mStyleFrame ? mStyleFrame->IsInvalid(aRect) : false;
  aRect += ToReferenceFrame();
  return result;
}

nsDisplayThemedBackground::nsDisplayThemedBackground(nsDisplayListBuilder* aBuilder,
                                                     nsIFrame* aFrame,
                                                     const nsRect& aBackgroundRect)
  : nsDisplayItem(aBuilder, aFrame)
  , mBackgroundRect(aBackgroundRect)
{
  MOZ_COUNT_CTOR(nsDisplayThemedBackground);
}

nsDisplayThemedBackground::~nsDisplayThemedBackground()
{
#ifdef NS_BUILD_REFCNT_LOGGING
  MOZ_COUNT_DTOR(nsDisplayThemedBackground);
#endif
}

void
nsDisplayThemedBackground::Init(nsDisplayListBuilder* aBuilder)
{
  const nsStyleDisplay* disp = StyleFrame()->StyleDisplay();
  mAppearance = disp->mAppearance;
  StyleFrame()->IsThemed(disp, &mThemeTransparency);

  // Perform necessary RegisterThemeGeometry
  nsITheme* theme = StyleFrame()->PresContext()->GetTheme();
  nsITheme::ThemeGeometryType type =
    theme->ThemeGeometryTypeForWidget(StyleFrame(), disp->mAppearance);
  if (type != nsITheme::eThemeGeometryTypeUnknown) {
    RegisterThemeGeometry(aBuilder, this, StyleFrame(), type);
  }

  if (disp->mAppearance == NS_THEME_WIN_BORDERLESS_GLASS ||
      disp->mAppearance == NS_THEME_WIN_GLASS) {
    aBuilder->SetGlassDisplayItem(this);
  }

  mBounds = GetBoundsInternal();
}

void
nsDisplayThemedBackground::WriteDebugInfo(std::stringstream& aStream)
{
  aStream << " (themed, appearance:" << (int)mAppearance << ")";
}

void
nsDisplayThemedBackground::HitTest(nsDisplayListBuilder* aBuilder,
                                  const nsRect& aRect,
                                  HitTestState* aState,
                                  nsTArray<nsIFrame*> *aOutFrames)
{
  // Assume that any point in our background rect is a hit.
  if (mBackgroundRect.Intersects(aRect)) {
    aOutFrames->AppendElement(mFrame);
  }
}

nsRegion
nsDisplayThemedBackground::GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                                           bool* aSnap) const
{
  nsRegion result;
  *aSnap = false;

  if (mThemeTransparency == nsITheme::eOpaque) {
    result = mBackgroundRect;
  }
  return result;
}

Maybe<nscolor>
nsDisplayThemedBackground::IsUniform(nsDisplayListBuilder* aBuilder) const
{
  if (mAppearance == NS_THEME_WIN_BORDERLESS_GLASS ||
      mAppearance == NS_THEME_WIN_GLASS) {
    return Some(NS_RGBA(0,0,0,0));
  }
  return Nothing();
}

nsRect
nsDisplayThemedBackground::GetPositioningArea() const
{
  return mBackgroundRect;
}

void
nsDisplayThemedBackground::Paint(nsDisplayListBuilder* aBuilder,
                                 gfxContext* aCtx)
{
  PaintInternal(aBuilder, aCtx, GetPaintRect(), nullptr);
}


void
nsDisplayThemedBackground::PaintInternal(nsDisplayListBuilder* aBuilder,
                                         gfxContext* aCtx, const nsRect& aBounds,
                                         nsRect* aClipRect)
{
  // XXXzw this ignores aClipRect.
  nsPresContext* presContext = StyleFrame()->PresContext();
  nsITheme *theme = presContext->GetTheme();
  nsRect drawing(mBackgroundRect);
  theme->GetWidgetOverflow(presContext->DeviceContext(), StyleFrame(), mAppearance,
                           &drawing);
  drawing.IntersectRect(drawing, aBounds);
  theme->DrawWidgetBackground(aCtx, StyleFrame(), mAppearance, mBackgroundRect, drawing);
}


bool
nsDisplayThemedBackground::CreateWebRenderCommands(mozilla::wr::DisplayListBuilder& aBuilder,
                                                   mozilla::wr::IpcResourceUpdateQueue& aResources,
                                                   const StackingContextHelper& aSc,
                                                   mozilla::layers::WebRenderLayerManager* aManager,
                                                   nsDisplayListBuilder* aDisplayListBuilder)
{
  nsITheme *theme = StyleFrame()->PresContext()->GetTheme();
  return theme->CreateWebRenderCommandsForWidget(aBuilder, aResources, aSc, aManager,
                                                 StyleFrame(), mAppearance, mBackgroundRect);
}

bool
nsDisplayThemedBackground::IsWindowActive() const
{
  EventStates docState = mFrame->GetContent()->OwnerDoc()->GetDocumentState();
  return !docState.HasState(NS_DOCUMENT_STATE_WINDOW_INACTIVE);
}

void
nsDisplayThemedBackground::ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                                     const nsDisplayItemGeometry* aGeometry,
                                                     nsRegion* aInvalidRegion) const
{
  const nsDisplayThemedBackgroundGeometry* geometry = static_cast<const nsDisplayThemedBackgroundGeometry*>(aGeometry);

  bool snap;
  nsRect bounds = GetBounds(aBuilder, &snap);
  nsRect positioningArea = GetPositioningArea();
  if (!positioningArea.IsEqualInterior(geometry->mPositioningArea)) {
    // Invalidate everything (both old and new painting areas).
    aInvalidRegion->Or(bounds, geometry->mBounds);
    return;
  }
  if (!bounds.IsEqualInterior(geometry->mBounds)) {
    // Positioning area is unchanged, so invalidate just the change in the
    // painting area.
    aInvalidRegion->Xor(bounds, geometry->mBounds);
  }
  nsITheme* theme = StyleFrame()->PresContext()->GetTheme();
  if (theme->WidgetAppearanceDependsOnWindowFocus(mAppearance) &&
      IsWindowActive() != geometry->mWindowIsActive) {
    aInvalidRegion->Or(*aInvalidRegion, bounds);
  }
}

nsRect
nsDisplayThemedBackground::GetBounds(nsDisplayListBuilder* aBuilder,
                                     bool* aSnap) const
{
  *aSnap = true;
  return mBounds;
}

nsRect
nsDisplayThemedBackground::GetBoundsInternal() {
  nsPresContext* presContext = mFrame->PresContext();

  nsRect r = mBackgroundRect - ToReferenceFrame();
  presContext->GetTheme()->
      GetWidgetOverflow(presContext->DeviceContext(), mFrame,
                        mFrame->StyleDisplay()->mAppearance, &r);
  return r + ToReferenceFrame();
}

void
nsDisplayImageContainer::ConfigureLayer(ImageLayer* aLayer,
                                        const ContainerLayerParameters& aParameters)
{
  aLayer->SetSamplingFilter(nsLayoutUtils::GetSamplingFilterForFrame(mFrame));

  nsCOMPtr<imgIContainer> image = GetImage();
  MOZ_ASSERT(image);
  int32_t imageWidth;
  int32_t imageHeight;
  image->GetWidth(&imageWidth);
  image->GetHeight(&imageHeight);
  NS_ASSERTION(imageWidth != 0 && imageHeight != 0, "Invalid image size!");

  if (imageWidth > 0 && imageHeight > 0) {
    // We're actually using the ImageContainer. Let our frame know that it
    // should consider itself to have painted successfully.
    UpdateDrawResult(ImgDrawResult::SUCCESS);
  }

  // XXX(seth): Right now we ignore aParameters.Scale() and
  // aParameters.Offset(), because FrameLayerBuilder already applies
  // aParameters.Scale() via the layer's post-transform, and
  // aParameters.Offset() is always zero.
  MOZ_ASSERT(aParameters.Offset() == LayerIntPoint(0,0));

  // It's possible (for example, due to downscale-during-decode) that the
  // ImageContainer this ImageLayer is holding has a different size from the
  // intrinsic size of the image. For this reason we compute the transform using
  // the ImageContainer's size rather than the image's intrinsic size.
  // XXX(seth): In reality, since the size of the ImageContainer may change
  // asynchronously, this is not enough. Bug 1183378 will provide a more
  // complete fix, but this solution is safe in more cases than simply relying
  // on the intrinsic size.
  IntSize containerSize = aLayer->GetContainer()
                        ? aLayer->GetContainer()->GetCurrentSize()
                        : IntSize(imageWidth, imageHeight);

  const int32_t factor = mFrame->PresContext()->AppUnitsPerDevPixel();
  const LayoutDeviceRect destRect(
    LayoutDeviceIntRect::FromAppUnitsToNearest(GetDestRect(), factor));

  const LayoutDevicePoint p = destRect.TopLeft();
  Matrix transform = Matrix::Translation(p.x, p.y);
  transform.PreScale(destRect.width / containerSize.width,
                     destRect.height / containerSize.height);
  aLayer->SetBaseTransform(gfx::Matrix4x4::From2D(transform));
}

already_AddRefed<ImageContainer>
nsDisplayImageContainer::GetContainer(LayerManager* aManager,
                                      nsDisplayListBuilder *aBuilder)
{
  nsCOMPtr<imgIContainer> image = GetImage();
  if (!image) {
    MOZ_ASSERT_UNREACHABLE("Must call CanOptimizeToImage() and get true "
                           "before calling GetContainer()");
    return nullptr;
  }

  uint32_t flags = imgIContainer::FLAG_ASYNC_NOTIFY;
  if (aBuilder->ShouldSyncDecodeImages()) {
    flags |= imgIContainer::FLAG_SYNC_DECODE;
  }

  RefPtr<ImageContainer> container = image->GetImageContainer(aManager, flags);
  if (!container || !container->HasCurrentImage()) {
    return nullptr;
  }

  return container.forget();
}

bool
nsDisplayImageContainer::CanOptimizeToImageLayer(LayerManager* aManager,
                                                 nsDisplayListBuilder* aBuilder)
{
  uint32_t flags = aBuilder->ShouldSyncDecodeImages()
                 ? imgIContainer::FLAG_SYNC_DECODE
                 : imgIContainer::FLAG_NONE;

  nsCOMPtr<imgIContainer> image = GetImage();
  if (!image) {
    return false;
  }

  if (!image->IsImageContainerAvailable(aManager, flags)) {
    return false;
  }

  int32_t imageWidth;
  int32_t imageHeight;
  image->GetWidth(&imageWidth);
  image->GetHeight(&imageHeight);

  if (imageWidth == 0 || imageHeight == 0) {
    NS_ASSERTION(false, "invalid image size");
    return false;
  }

  const int32_t factor = mFrame->PresContext()->AppUnitsPerDevPixel();
  const LayoutDeviceRect destRect(
    LayoutDeviceIntRect::FromAppUnitsToNearest(GetDestRect(), factor));

  // Calculate the scaling factor for the frame.
  const gfxSize scale = gfxSize(destRect.width / imageWidth,
                                destRect.height / imageHeight);

  if (scale.width < 0.34 || scale.height < 0.34) {
    // This would look awful as long as we can't use high-quality downscaling
    // for image layers (bug 803703), so don't turn this into an image layer.
    return false;
  }

  if (mFrame->IsImageFrame()) {
    // Image layer doesn't support draw focus ring for image map.
    nsImageFrame* f = static_cast<nsImageFrame*>(mFrame);
    if (f->HasImageMap()) {
      return false;
    }
  }

  return true;
}

void
nsDisplayBackgroundColor::ApplyOpacity(nsDisplayListBuilder* aBuilder,
                                       float aOpacity,
                                       const DisplayItemClipChain* aClip)
{
  NS_ASSERTION(CanApplyOpacity(), "ApplyOpacity should be allowed");
  mColor.a = mColor.a * aOpacity;
  IntersectClip(aBuilder, aClip, false);
}

bool
nsDisplayBackgroundColor::CanApplyOpacity() const
{
  return true;
}

LayerState
nsDisplayBackgroundColor::GetLayerState(nsDisplayListBuilder* aBuilder,
                                        LayerManager* aManager,
                                        const ContainerLayerParameters& aParameters)
{
  StyleGeometryBox clip = mBackgroundStyle->StyleBackground()->mImage.mLayers[0].mClip;
  if (ForceActiveLayers() && clip != StyleGeometryBox::Text) {
    return LAYER_ACTIVE;
  }
  return LAYER_NONE;
}

already_AddRefed<Layer>
nsDisplayBackgroundColor::BuildLayer(nsDisplayListBuilder* aBuilder,
                                     LayerManager* aManager,
                                     const ContainerLayerParameters& aContainerParameters)
{
  if (mColor == Color()) {
    return nullptr;
  }

  RefPtr<ColorLayer> layer = static_cast<ColorLayer*>
    (aManager->GetLayerBuilder()->GetLeafLayerFor(aBuilder, this));
  if (!layer) {
    layer = aManager->CreateColorLayer();
    if (!layer)
      return nullptr;
  }
  layer->SetColor(mColor);

  int32_t appUnitsPerDevPixel = mFrame->PresContext()->AppUnitsPerDevPixel();
  layer->SetBounds(mBackgroundRect.ToNearestPixels(appUnitsPerDevPixel));
  layer->SetBaseTransform(gfx::Matrix4x4::Translation(aContainerParameters.mOffset.x,
                                                      aContainerParameters.mOffset.y, 0));

  return layer.forget();
}

bool
nsDisplayBackgroundColor::CreateWebRenderCommands(mozilla::wr::DisplayListBuilder& aBuilder,
                                                  mozilla::wr::IpcResourceUpdateQueue& aResources,
                                                  const StackingContextHelper& aSc,
                                                  mozilla::layers::WebRenderLayerManager* aManager,
                                                  nsDisplayListBuilder* aDisplayListBuilder)
{
  if (mColor == Color()) {
    return true;
  }

  StyleGeometryBox clip = mBackgroundStyle->StyleBackground()->mImage.mLayers[0].mClip;
  if (clip == StyleGeometryBox::Text) {
    return false;
  }

  LayoutDeviceRect bounds = LayoutDeviceRect::FromAppUnits(
        mBackgroundRect, mFrame->PresContext()->AppUnitsPerDevPixel());
  wr::LayoutRect roundedRect = wr::ToRoundedLayoutRect(bounds);

  aBuilder.PushRect(roundedRect,
                    roundedRect,
                    !BackfaceIsHidden(),
                    wr::ToColorF(ToDeviceColor(mColor)));

  return true;
}

void
nsDisplayBackgroundColor::Paint(nsDisplayListBuilder* aBuilder,
                                gfxContext* aCtx)
{
  if (mColor == Color()) {
    return;
  }

#if 0
  // See https://bugzilla.mozilla.org/show_bug.cgi?id=1148418#c21 for why this
  // results in a precision induced rounding issue that makes the rect one
  // pixel shorter in rare cases. Disabled in favor of the old code for now.
  // Note that the pref layout.css.devPixelsPerPx needs to be set to 1 to
  // reproduce the bug.
  //
  // TODO:
  // This new path does not include support for background-clip:text; need to
  // be fixed if/when we switch to this new code path.

  DrawTarget& aDrawTarget = *aCtx->GetDrawTarget();

  Rect rect = NSRectToSnappedRect(mBackgroundRect,
                                  mFrame->PresContext()->AppUnitsPerDevPixel(),
                                  aDrawTarget);
  ColorPattern color(ToDeviceColor(mColor));
  aDrawTarget.FillRect(rect, color);
#else
  gfxContext* ctx = aCtx;
  gfxRect bounds =
    nsLayoutUtils::RectToGfxRect(mBackgroundRect,
                                 mFrame->PresContext()->AppUnitsPerDevPixel());

  StyleGeometryBox clip = mBackgroundStyle->StyleBackground()->mImage.mLayers[0].mClip;
  if (clip == StyleGeometryBox::Text) {
    if (!GenerateAndPushTextMask(mFrame, aCtx, mBackgroundRect, aBuilder)) {
      return;
    }

    ctx->SetColor(mColor);
    ctx->NewPath();
    ctx->Rectangle(bounds, true);
    ctx->Fill();
    ctx->PopGroupAndBlend();
    return;
  }

  ctx->SetColor(mColor);
  ctx->NewPath();
  ctx->Rectangle(bounds, true);
  ctx->Fill();
#endif
}

nsRegion
nsDisplayBackgroundColor::GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                                          bool* aSnap) const
{
  *aSnap = false;

  if (mColor.a != 1) {
    return nsRegion();
  }

  if (!mBackgroundStyle)
    return nsRegion();


  const nsStyleImageLayers::Layer& bottomLayer = mBackgroundStyle->StyleBackground()->BottomLayer();
  if (bottomLayer.mClip == StyleGeometryBox::Text) {
    return nsRegion();
  }

  *aSnap = true;
  return nsDisplayBackgroundImage::GetInsideClipRegion(this, bottomLayer.mClip,
                                                       mBackgroundRect, mBackgroundRect);
}

Maybe<nscolor>
nsDisplayBackgroundColor::IsUniform(nsDisplayListBuilder* aBuilder) const
{
  return Some(mColor.ToABGR());
}

void
nsDisplayBackgroundColor::HitTest(nsDisplayListBuilder* aBuilder,
                                  const nsRect& aRect,
                                  HitTestState* aState,
                                  nsTArray<nsIFrame*> *aOutFrames)
{
  if (!RoundedBorderIntersectsRect(mFrame, ToReferenceFrame(), aRect)) {
    // aRect doesn't intersect our border-radius curve.
    return;
  }

  aOutFrames->AppendElement(mFrame);
}

void
nsDisplayBackgroundColor::WriteDebugInfo(std::stringstream& aStream)
{
  aStream << " (rgba " << mColor.r << "," << mColor.g << ","
          << mColor.b << "," << mColor.a << ")";
}

already_AddRefed<Layer>
nsDisplayClearBackground::BuildLayer(nsDisplayListBuilder* aBuilder,
                                     LayerManager* aManager,
                                     const ContainerLayerParameters& aParameters)
{
  RefPtr<ColorLayer> layer = static_cast<ColorLayer*>
    (aManager->GetLayerBuilder()->GetLeafLayerFor(aBuilder, this));
  if (!layer) {
    layer = aManager->CreateColorLayer();
    if (!layer)
      return nullptr;
  }
  layer->SetColor(Color());
  layer->SetMixBlendMode(gfx::CompositionOp::OP_SOURCE);

  bool snap;
  nsRect bounds = GetBounds(aBuilder, &snap);
  int32_t appUnitsPerDevPixel = mFrame->PresContext()->AppUnitsPerDevPixel();
  layer->SetBounds(bounds.ToNearestPixels(appUnitsPerDevPixel)); // XXX Do we need to respect the parent layer's scale here?

  return layer.forget();
}

bool
nsDisplayClearBackground::CreateWebRenderCommands(mozilla::wr::DisplayListBuilder& aBuilder,
                                                  mozilla::wr::IpcResourceUpdateQueue& aResources,
                                                  const StackingContextHelper& aSc,
                                                  mozilla::layers::WebRenderLayerManager* aManager,
                                                  nsDisplayListBuilder* aDisplayListBuilder)
{
  LayoutDeviceRect bounds = LayoutDeviceRect::FromAppUnits(
    nsRect(ToReferenceFrame(), mFrame->GetSize()),
    mFrame->PresContext()->AppUnitsPerDevPixel());

  aBuilder.PushClearRect(wr::ToRoundedLayoutRect(bounds));

  return true;
}

nsRect
nsDisplayOutline::GetBounds(nsDisplayListBuilder* aBuilder,
                            bool* aSnap) const
{
  *aSnap = false;
  return mFrame->GetVisualOverflowRectRelativeToSelf() + ToReferenceFrame();
}

void
nsDisplayOutline::Paint(nsDisplayListBuilder* aBuilder,
                        gfxContext* aCtx) {
  // TODO join outlines together
  MOZ_ASSERT(mFrame->StyleOutline()->ShouldPaintOutline(),
             "Should have not created a nsDisplayOutline!");

  nsPoint offset = ToReferenceFrame();
  nsCSSRendering::PaintOutline(mFrame->PresContext(), *aCtx, mFrame,
                               GetPaintRect(),
                               nsRect(offset, mFrame->GetSize()),
                               mFrame->Style());
}

bool
nsDisplayOutline::CreateWebRenderCommands(mozilla::wr::DisplayListBuilder& aBuilder,
                                          mozilla::wr::IpcResourceUpdateQueue& aResources,
                                          const StackingContextHelper& aSc,
                                          mozilla::layers::WebRenderLayerManager* aManager,
                                          nsDisplayListBuilder* aDisplayListBuilder)
{
  ContainerLayerParameters parameter;

  uint8_t outlineStyle = mFrame->Style()->StyleOutline()->mOutlineStyle;
  if (outlineStyle == NS_STYLE_BORDER_STYLE_AUTO && nsLayoutUtils::IsOutlineStyleAutoEnabled()) {
      nsITheme* theme = mFrame->PresContext()->GetTheme();
      if (theme && theme->ThemeSupportsWidget(mFrame->PresContext(), mFrame,
                                              NS_THEME_FOCUS_OUTLINE)) {
        return false;
      }
  }

  nsPoint offset = ToReferenceFrame();

  mozilla::Maybe<nsCSSBorderRenderer> borderRenderer =
    nsCSSRendering::CreateBorderRendererForOutline(mFrame->PresContext(),
                                                   nullptr, mFrame,
                                                   GetPaintRect(),
                                                   nsRect(offset, mFrame->GetSize()),
                                                   mFrame->Style());

  if (!borderRenderer) {
    // No border renderer means "there is no outline".
    // Paint nothing and return success.
    return true;
  }

  borderRenderer->CreateWebRenderCommands(this, aBuilder, aResources, aSc);
  return true;
}

bool
nsDisplayOutline::IsInvisibleInRect(const nsRect& aRect) const
{
  const nsStyleOutline* outline = mFrame->StyleOutline();
  nsRect borderBox(ToReferenceFrame(), mFrame->GetSize());
  if (borderBox.Contains(aRect) &&
      !nsLayoutUtils::HasNonZeroCorner(outline->mOutlineRadius)) {
    if (outline->mOutlineOffset >= 0) {
      // aRect is entirely inside the border-rect, and the outline isn't
      // rendered inside the border-rect, so the outline is not visible.
      return true;
    }
  }

  return false;
}

void
nsDisplayEventReceiver::HitTest(nsDisplayListBuilder* aBuilder,
                                const nsRect& aRect,
                                HitTestState* aState,
                                nsTArray<nsIFrame*> *aOutFrames)
{
  if (!RoundedBorderIntersectsRect(mFrame, ToReferenceFrame(), aRect)) {
    // aRect doesn't intersect our border-radius curve.
    return;
  }

  aOutFrames->AppendElement(mFrame);
}

bool
nsDisplayEventReceiver::CreateWebRenderCommands(mozilla::wr::DisplayListBuilder& aBuilder,
                                                mozilla::wr::IpcResourceUpdateQueue& aResources,
                                                const StackingContextHelper& aSc,
                                                mozilla::layers::WebRenderLayerManager* aManager,
                                                nsDisplayListBuilder* aDisplayListBuilder)
{
  // This display item should never be getting created when building a display
  // list for WebRender consumption, so this function should never get called.
  MOZ_ASSERT(false);
  return true;
}

nsDisplayCompositorHitTestInfo::nsDisplayCompositorHitTestInfo(nsDisplayListBuilder* aBuilder,
                                                               nsIFrame* aFrame,
                                                               mozilla::gfx::CompositorHitTestInfo aHitTestInfo,
                                                               uint32_t aIndex,
                                                               const mozilla::Maybe<nsRect>& aArea)
  : nsDisplayEventReceiver(aBuilder, aFrame)
  , mHitTestInfo(aHitTestInfo)
  , mIndex(aIndex)
  , mAppUnitsPerDevPixel(mFrame->PresContext()->AppUnitsPerDevPixel())
{
  MOZ_COUNT_CTOR(nsDisplayCompositorHitTestInfo);
  // We should never even create this display item if we're not building
  // compositor hit-test info or if the computed hit info indicated the
  // frame is invisible to hit-testing
  MOZ_ASSERT(aBuilder->BuildCompositorHitTestInfo());
  MOZ_ASSERT(mHitTestInfo != mozilla::gfx::CompositorHitTestInfo::eInvisibleToHitTest);

  if (aBuilder->GetCurrentScrollbarDirection().isSome()) {
    // In the case of scrollbar frames, we use the scrollbar's target scrollframe
    // instead of the scrollframe with which the scrollbar actually moves.
    MOZ_ASSERT(mHitTestInfo & CompositorHitTestInfo::eScrollbar);
    mScrollTarget = Some(aBuilder->GetCurrentScrollbarTarget());
  }

  if (aArea.isSome()) {
    mArea = *aArea;
  } else {
    mArea = GetFrameArea(aBuilder, aFrame);
  }
}

bool
nsDisplayCompositorHitTestInfo::CreateWebRenderCommands(mozilla::wr::DisplayListBuilder& aBuilder,
                                                        mozilla::wr::IpcResourceUpdateQueue& aResources,
                                                        const StackingContextHelper& aSc,
                                                        mozilla::layers::WebRenderLayerManager* aManager,
                                                        nsDisplayListBuilder* aDisplayListBuilder)
{
  if (mArea.IsEmpty()) {
    return true;
  }

  // XXX: eventually this scrollId computation and the SetHitTestInfo
  // call will get moved out into the WR display item iteration code so that
  // we don't need to do it as often, and so that we can do it for other
  // display item types as well (reducing the need for as many instances of
  // this display item).
  FrameMetrics::ViewID scrollId = mScrollTarget.valueOrFrom(
      [&]() -> FrameMetrics::ViewID {
          const ActiveScrolledRoot* asr = GetActiveScrolledRoot();
          Maybe<FrameMetrics::ViewID> fixedTarget =
              aBuilder.GetContainingFixedPosScrollTarget(asr);
          if (fixedTarget) {
            return *fixedTarget;
          }
          if (asr) {
            return asr->GetViewId();
          }
          return FrameMetrics::NULL_SCROLL_ID;
      });

  // Insert a transparent rectangle with the hit-test info
  aBuilder.SetHitTestInfo(scrollId, mHitTestInfo);

  const LayoutDeviceRect devRect =
    LayoutDeviceRect::FromAppUnits(mArea, mAppUnitsPerDevPixel);

  const wr::LayoutRect rect = wr::ToRoundedLayoutRect(devRect);

  aBuilder.PushRect(rect, rect, !BackfaceIsHidden(), wr::ToColorF(gfx::Color()));
  aBuilder.ClearHitTestInfo();

  return true;
}

void
nsDisplayCompositorHitTestInfo::WriteDebugInfo(std::stringstream& aStream)
{
  aStream << nsPrintfCString(" (hitTestInfo 0x%x)", (int)mHitTestInfo).get();
  AppendToString(aStream, mArea, " hitTestArea");
}

uint32_t
nsDisplayCompositorHitTestInfo::GetPerFrameKey() const
{
  return (mIndex << TYPE_BITS) | nsDisplayItem::GetPerFrameKey();
}

int32_t
nsDisplayCompositorHitTestInfo::ZIndex() const
{
  return mOverrideZIndex ? *mOverrideZIndex : nsDisplayItem::ZIndex();
}

void
nsDisplayCompositorHitTestInfo::SetOverrideZIndex(int32_t aZIndex)
{
  mOverrideZIndex = Some(aZIndex);
}

nsDisplayCaret::nsDisplayCaret(nsDisplayListBuilder* aBuilder,
                               nsIFrame* aCaretFrame)
  : nsDisplayItem(aBuilder, aCaretFrame)
  , mCaret(aBuilder->GetCaret())
  , mBounds(aBuilder->GetCaretRect() + ToReferenceFrame())
{
  MOZ_COUNT_CTOR(nsDisplayCaret);
}

#ifdef NS_BUILD_REFCNT_LOGGING
nsDisplayCaret::~nsDisplayCaret()
{
  MOZ_COUNT_DTOR(nsDisplayCaret);
}
#endif

nsRect
nsDisplayCaret::GetBounds(nsDisplayListBuilder* aBuilder,
                          bool* aSnap) const
{
  *aSnap = true;
  // The caret returns a rect in the coordinates of mFrame.
  return mBounds;
}

void
nsDisplayCaret::Paint(nsDisplayListBuilder* aBuilder,
                      gfxContext* aCtx) {
  // Note: Because we exist, we know that the caret is visible, so we don't
  // need to check for the caret's visibility.
  mCaret->PaintCaret(*aCtx->GetDrawTarget(), mFrame, ToReferenceFrame());
}

bool
nsDisplayCaret::CreateWebRenderCommands(mozilla::wr::DisplayListBuilder& aBuilder,
                                        mozilla::wr::IpcResourceUpdateQueue& aResources,
                                        const StackingContextHelper& aSc,
                                        mozilla::layers::WebRenderLayerManager* aManager,
                                        nsDisplayListBuilder* aDisplayListBuilder)
{
  using namespace mozilla::layers;
  int32_t contentOffset;
  nsIFrame* frame = mCaret->GetFrame(&contentOffset);
  if (!frame) {
    return true;
  }
  NS_ASSERTION(frame == mFrame, "We're referring different frame");

  int32_t appUnitsPerDevPixel = frame->PresContext()->AppUnitsPerDevPixel();

  nsRect caretRect;
  nsRect hookRect;
  mCaret->ComputeCaretRects(frame, contentOffset, &caretRect, &hookRect);

  gfx::Color color = ToDeviceColor(frame->GetCaretColorAt(contentOffset));
  LayoutDeviceRect devCaretRect = LayoutDeviceRect::FromAppUnits(
    caretRect + ToReferenceFrame(), appUnitsPerDevPixel);
  LayoutDeviceRect devHookRect = LayoutDeviceRect::FromAppUnits(
    hookRect + ToReferenceFrame(), appUnitsPerDevPixel);

  wr::LayoutRect caret = wr::ToRoundedLayoutRect(devCaretRect);
  wr::LayoutRect hook = wr::ToRoundedLayoutRect(devHookRect);

  // Note, WR will pixel snap anything that is layout aligned.
  aBuilder.PushRect(caret,
                    caret,
                    !BackfaceIsHidden(),
                    wr::ToColorF(color));

  if (!devHookRect.IsEmpty()) {
    aBuilder.PushRect(hook,
                      hook,
                      !BackfaceIsHidden(),
                      wr::ToColorF(color));
  }
  return true;
}

nsDisplayBorder::nsDisplayBorder(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame)
  : nsDisplayItem(aBuilder, aFrame)
  , mBorderIsEmpty(false)
{
  MOZ_COUNT_CTOR(nsDisplayBorder);

  mBounds = CalculateBounds<nsRect>(*mFrame->StyleBorder());
}

bool
nsDisplayBorder::IsInvisibleInRect(const nsRect& aRect) const
{
  nsRect paddingRect = mFrame->GetPaddingRect() - mFrame->GetPosition() +
    ToReferenceFrame();
  const nsStyleBorder *styleBorder;
  if (paddingRect.Contains(aRect) &&
      !(styleBorder = mFrame->StyleBorder())->IsBorderImageLoaded() &&
      !nsLayoutUtils::HasNonZeroCorner(styleBorder->mBorderRadius)) {
    // aRect is entirely inside the content rect, and no part
    // of the border is rendered inside the content rect, so we are not
    // visible
    // Skip this if there's a border-image (which draws a background
    // too) or if there is a border-radius (which makes the border draw
    // further in).
    return true;
  }

  return false;
}

nsDisplayItemGeometry*
nsDisplayBorder::AllocateGeometry(nsDisplayListBuilder* aBuilder)
{
  return new nsDisplayBorderGeometry(this, aBuilder);
}

void
nsDisplayBorder::ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                           const nsDisplayItemGeometry* aGeometry,
                                           nsRegion* aInvalidRegion) const
{
  const nsDisplayBorderGeometry* geometry = static_cast<const nsDisplayBorderGeometry*>(aGeometry);
  bool snap;

  if (!geometry->mBounds.IsEqualInterior(GetBounds(aBuilder, &snap))) {
    // We can probably get away with only invalidating the difference
    // between the border and padding rects, but the XUL ui at least
    // is apparently painting a background with this?
    aInvalidRegion->Or(GetBounds(aBuilder, &snap), geometry->mBounds);
  }

  if (aBuilder->ShouldSyncDecodeImages() &&
      geometry->ShouldInvalidateToSyncDecodeImages()) {
    aInvalidRegion->Or(*aInvalidRegion, GetBounds(aBuilder, &snap));
  }
}

LayerState
nsDisplayBorder::GetLayerState(nsDisplayListBuilder* aBuilder,
                               LayerManager* aManager,
                               const ContainerLayerParameters& aParameters)
{
  if (!ShouldUseAdvancedLayer(aManager, gfxPrefs::LayersAllowBorderLayers)) {
    return LAYER_NONE;
  }

  mBorderIsEmpty = false;
  nsPoint offset = ToReferenceFrame();
  Maybe<nsCSSBorderRenderer> br =
    nsCSSRendering::CreateBorderRenderer(mFrame->PresContext(),
                                         nullptr,
                                         mFrame,
                                         nsRect(),
                                         nsRect(offset, mFrame->GetSize()),
                                         mFrame->Style(),
                                         &mBorderIsEmpty,
                                         mFrame->GetSkipSides());

  mBorderRenderer = Nothing();
  mBorderImageRenderer = Nothing();
  if (!br) {
    if (mBorderIsEmpty) {
      return LAYER_ACTIVE;
    }
    return LAYER_NONE;
  }

  if (!br->AllBordersSolid()) {
    return LAYER_NONE;
  }

  // We don't support this yet as we don't copy the values to
  // the layer, and BasicBorderLayer doesn't support it yet.
  if (!br->mNoBorderRadius) {
    return LAYER_NONE;
  }

  // We copy these values correctly to the layer, but BasicBorderLayer
  // won't render them
  if (!br->AreBorderSideFinalStylesSame(eSideBitsAll) ||
      !br->AllBordersSameWidth()) {
    return LAYER_NONE;
  }

  NS_FOR_CSS_SIDES(i) {
    if (br->mBorderStyles[i] == NS_STYLE_BORDER_STYLE_SOLID) {
      mColors[i] = ToDeviceColor(br->mBorderColors[i]);
      mWidths[i] = br->mBorderWidths[i];
      mBorderStyles[i] = br->mBorderStyles[i];
    } else {
      mWidths[i] = 0;
    }
  }
  NS_FOR_CSS_FULL_CORNERS(corner) {
    mCorners[corner] = LayerSize(br->mBorderRadii[corner].width, br->mBorderRadii[corner].height);
  }

  mRect = ViewAs<LayerPixel>(br->mOuterRect);
  return LAYER_ACTIVE;
}

already_AddRefed<Layer>
nsDisplayBorder::BuildLayer(nsDisplayListBuilder* aBuilder,
                            LayerManager* aManager,
                            const ContainerLayerParameters& aContainerParameters)
{
  if (mBorderIsEmpty) {
    return nullptr;
  }

  RefPtr<BorderLayer> layer = static_cast<BorderLayer*>
    (aManager->GetLayerBuilder()->GetLeafLayerFor(aBuilder, this));
  if (!layer) {
    layer = aManager->CreateBorderLayer();
    if (!layer)
      return nullptr;
  }
  layer->SetRect(mRect);
  layer->SetCornerRadii(mCorners);
  layer->SetColors(mColors);
  layer->SetWidths(mWidths);
  layer->SetStyles(mBorderStyles);
  layer->SetBaseTransform(gfx::Matrix4x4::Translation(aContainerParameters.mOffset.x,
                                                      aContainerParameters.mOffset.y, 0));
  return layer.forget();
}

bool
nsDisplayBorder::CreateWebRenderCommands(mozilla::wr::DisplayListBuilder& aBuilder,
                                         mozilla::wr::IpcResourceUpdateQueue& aResources,
                                         const StackingContextHelper& aSc,
                                         mozilla::layers::WebRenderLayerManager* aManager,
                                         nsDisplayListBuilder* aDisplayListBuilder)
{
  nsRect rect = nsRect(ToReferenceFrame(), mFrame->GetSize());

  return nsCSSRendering::CreateWebRenderCommandsForBorder(this,
                                                          mFrame,
                                                          rect,
                                                          aBuilder,
                                                          aResources,
                                                          aSc,
                                                          aManager,
                                                          aDisplayListBuilder);
};

void
nsDisplayBorder::CreateBorderImageWebRenderCommands(mozilla::wr::DisplayListBuilder& aBuilder,
                                                    mozilla::wr::IpcResourceUpdateQueue& aResources,
                                                    const StackingContextHelper& aSc,
                                                    mozilla::layers::WebRenderLayerManager* aManager,
                                                    nsDisplayListBuilder* aDisplayListBuilder)
{
  MOZ_ASSERT(mBorderImageRenderer);
  if (!mBorderImageRenderer->mImageRenderer.IsReady()) {
    return;
  }

  float widths[4];
  float slice[4];
  float outset[4];
  const int32_t appUnitsPerDevPixel = mFrame->PresContext()->AppUnitsPerDevPixel();
  NS_FOR_CSS_SIDES(i) {
    slice[i] = (float)(mBorderImageRenderer->mSlice.Side(i)) / appUnitsPerDevPixel;
    widths[i] = (float)(mBorderImageRenderer->mWidths.Side(i)) / appUnitsPerDevPixel;
    outset[i] = (float)(mBorderImageRenderer->mImageOutset.Side(i)) / appUnitsPerDevPixel;
  }

  LayoutDeviceRect destRect = LayoutDeviceRect::FromAppUnits(
    mBorderImageRenderer->mArea, appUnitsPerDevPixel);
  wr::LayoutRect dest = wr::ToRoundedLayoutRect(destRect);

  wr::LayoutRect clip = dest;
  if (!mBorderImageRenderer->mClip.IsEmpty()) {
    LayoutDeviceRect clipRect = LayoutDeviceRect::FromAppUnits(
      mBorderImageRenderer->mClip, appUnitsPerDevPixel);
    clip = wr::ToRoundedLayoutRect(clipRect);
  }

  switch (mBorderImageRenderer->mImageRenderer.GetType()) {
    case eStyleImageType_Image:
    {
      uint32_t flags = imgIContainer::FLAG_ASYNC_NOTIFY;
      if (aDisplayListBuilder->IsPaintingToWindow()) {
        flags |= imgIContainer::FLAG_HIGH_QUALITY_SCALING;
      }
      if (aDisplayListBuilder->ShouldSyncDecodeImages()) {
        flags |= imgIContainer::FLAG_SYNC_DECODE;
      }

      RefPtr<imgIContainer> img = mBorderImageRenderer->mImageRenderer.GetImage();
      Maybe<SVGImageContext> svgContext;
      gfx::IntSize decodeSize =
        nsLayoutUtils::ComputeImageContainerDrawingParameters(img, mFrame, destRect,
                                                              aSc, flags, svgContext);
      RefPtr<layers::ImageContainer> container =
        img->GetImageContainerAtSize(aManager, decodeSize, svgContext, flags);
      if (!container) {
        return;
      }

      gfx::IntSize size;
      Maybe<wr::ImageKey> key = aManager->CommandBuilder().CreateImageKey(this, container, aBuilder,
                                                                          aResources, aSc, size, Nothing());
      if (key.isNothing()) {
        return;
      }

      aBuilder.PushBorderImage(dest,
                               clip,
                               !BackfaceIsHidden(),
                               wr::ToBorderWidths(widths[0], widths[1], widths[2], widths[3]),
                               key.value(),
                               (float)(mBorderImageRenderer->mImageSize.width) / appUnitsPerDevPixel,
                               (float)(mBorderImageRenderer->mImageSize.height) / appUnitsPerDevPixel,
                               wr::ToSideOffsets2D_u32(slice[0], slice[1], slice[2], slice[3]),
                               wr::ToSideOffsets2D_f32(outset[0], outset[1], outset[2], outset[3]),
                               wr::ToRepeatMode(mBorderImageRenderer->mRepeatModeHorizontal),
                               wr::ToRepeatMode(mBorderImageRenderer->mRepeatModeVertical));
      break;
    }
    case eStyleImageType_Gradient:
    {
      RefPtr<nsStyleGradient> gradientData = mBorderImageRenderer->mImageRenderer.GetGradientData();
      nsCSSGradientRenderer renderer =
        nsCSSGradientRenderer::Create(mFrame->PresContext(), mFrame->Style(),
                                      gradientData, mBorderImageRenderer->mImageSize);

      wr::ExtendMode extendMode;
      nsTArray<wr::GradientStop> stops;
      LayoutDevicePoint lineStart;
      LayoutDevicePoint lineEnd;
      LayoutDeviceSize gradientRadius;
      renderer.BuildWebRenderParameters(1.0, extendMode, stops, lineStart, lineEnd, gradientRadius);

      if (gradientData->mShape == NS_STYLE_GRADIENT_SHAPE_LINEAR) {
        LayoutDevicePoint startPoint = LayoutDevicePoint(dest.origin.x, dest.origin.y) + lineStart;
        LayoutDevicePoint endPoint = LayoutDevicePoint(dest.origin.x, dest.origin.y) + lineEnd;

        aBuilder.PushBorderGradient(dest,
                                    clip,
                                    !BackfaceIsHidden(),
                                    wr::ToBorderWidths(widths[0], widths[1], widths[2], widths[3]),
                                    wr::ToLayoutPoint(startPoint),
                                    wr::ToLayoutPoint(endPoint),
                                    stops,
                                    extendMode,
                                    wr::ToSideOffsets2D_f32(outset[0], outset[1], outset[2], outset[3]));
      } else {
        aBuilder.PushBorderRadialGradient(dest,
                                          clip,
                                          !BackfaceIsHidden(),
                                          wr::ToBorderWidths(widths[0], widths[1], widths[2], widths[3]),
                                          wr::ToLayoutPoint(lineStart),
                                          wr::ToLayoutSize(gradientRadius),
                                          stops,
                                          extendMode,
                                          wr::ToSideOffsets2D_f32(outset[0], outset[1], outset[2], outset[3]));
      }
      break;
    }
    default:
      MOZ_ASSERT_UNREACHABLE("Unsupport border image type");
  }
}

void
nsDisplayBorder::Paint(nsDisplayListBuilder* aBuilder,
                       gfxContext* aCtx) {
  nsPoint offset = ToReferenceFrame();

  PaintBorderFlags flags = aBuilder->ShouldSyncDecodeImages()
                         ? PaintBorderFlags::SYNC_DECODE_IMAGES
                         : PaintBorderFlags();

  ImgDrawResult result =
    nsCSSRendering::PaintBorder(mFrame->PresContext(), *aCtx, mFrame,
                                GetPaintRect(),
                                nsRect(offset, mFrame->GetSize()),
                                mFrame->Style(),
                                flags,
                                mFrame->GetSkipSides());

  nsDisplayBorderGeometry::UpdateDrawResult(this, result);
}

nsRect
nsDisplayBorder::GetBounds(nsDisplayListBuilder* aBuilder,
                           bool* aSnap) const
{
  *aSnap = true;
  return mBounds;
}

// Given a region, compute a conservative approximation to it as a list
// of rectangles that aren't vertically adjacent (i.e., vertically
// adjacent or overlapping rectangles are combined).
// Right now this is only approximate, some vertically overlapping rectangles
// aren't guaranteed to be combined.
static void
ComputeDisjointRectangles(const nsRegion& aRegion,
                          nsTArray<nsRect>* aRects) {
  nscoord accumulationMargin = nsPresContext::CSSPixelsToAppUnits(25);
  nsRect accumulated;

  for (auto iter = aRegion.RectIter(); !iter.Done(); iter.Next()) {
    const nsRect& r = iter.Get();
    if (accumulated.IsEmpty()) {
      accumulated = r;
      continue;
    }

    if (accumulated.YMost() >= r.y - accumulationMargin) {
      accumulated.UnionRect(accumulated, r);
    } else {
      aRects->AppendElement(accumulated);
      accumulated = r;
    }
  }

  // Finish the in-flight rectangle, if there is one.
  if (!accumulated.IsEmpty()) {
    aRects->AppendElement(accumulated);
  }
}

void
nsDisplayBoxShadowOuter::Paint(nsDisplayListBuilder* aBuilder,
                               gfxContext* aCtx) {
  nsPoint offset = ToReferenceFrame();
  nsRect borderRect = mFrame->VisualBorderRectRelativeToSelf() + offset;
  nsPresContext* presContext = mFrame->PresContext();
  AutoTArray<nsRect,10> rects;
  ComputeDisjointRectangles(mVisibleRegion, &rects);

  AUTO_PROFILER_LABEL("nsDisplayBoxShadowOuter::Paint", GRAPHICS);

  for (uint32_t i = 0; i < rects.Length(); ++i) {
    nsCSSRendering::PaintBoxShadowOuter(presContext, *aCtx, mFrame,
                                        borderRect, rects[i], mOpacity);
  }
}

nsRect
nsDisplayBoxShadowOuter::GetBounds(nsDisplayListBuilder* aBuilder,
                                   bool* aSnap) const
{
  *aSnap = false;
  return mBounds;
}

nsRect
nsDisplayBoxShadowOuter::GetBoundsInternal() {
  return nsLayoutUtils::GetBoxShadowRectForFrame(mFrame, mFrame->GetSize()) +
         ToReferenceFrame();
}

bool
nsDisplayBoxShadowOuter::IsInvisibleInRect(const nsRect& aRect) const
{
  nsPoint origin = ToReferenceFrame();
  nsRect frameRect(origin, mFrame->GetSize());
  if (!frameRect.Contains(aRect))
    return false;

  // the visible region is entirely inside the border-rect, and box shadows
  // never render within the border-rect (unless there's a border radius).
  nscoord twipsRadii[8];
  bool hasBorderRadii = mFrame->GetBorderRadii(twipsRadii);
  if (!hasBorderRadii)
    return true;

  return RoundedRectContainsRect(frameRect, twipsRadii, aRect);
}

bool
nsDisplayBoxShadowOuter::ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                           nsRegion* aVisibleRegion) {
  if (!nsDisplayItem::ComputeVisibility(aBuilder, aVisibleRegion)) {
    return false;
  }

  mVisibleRegion.And(*aVisibleRegion, GetPaintRect());
  return true;
}

bool
nsDisplayBoxShadowOuter::CanBuildWebRenderDisplayItems()
{
  nsCSSShadowArray* shadows = mFrame->StyleEffects()->mBoxShadow;
  if (!shadows) {
    return false;
  }

  bool hasBorderRadius;
  bool nativeTheme =
      nsCSSRendering::HasBoxShadowNativeTheme(mFrame, hasBorderRadius);

  // We don't support native themed things yet like box shadows around
  // input buttons.
  if (nativeTheme) {
    return false;
  }

  return true;
}

bool
nsDisplayBoxShadowOuter::CreateWebRenderCommands(mozilla::wr::DisplayListBuilder& aBuilder,
                                                 mozilla::wr::IpcResourceUpdateQueue& aResources,
                                                 const StackingContextHelper& aSc,
                                                 mozilla::layers::WebRenderLayerManager* aManager,
                                                 nsDisplayListBuilder* aDisplayListBuilder)
{
  if (!CanBuildWebRenderDisplayItems()) {
    return false;
  }

  int32_t appUnitsPerDevPixel = mFrame->PresContext()->AppUnitsPerDevPixel();
  nsPoint offset = ToReferenceFrame();
  nsRect borderRect = mFrame->VisualBorderRectRelativeToSelf() + offset;
  AutoTArray<nsRect,10> rects;
  bool snap;
  nsRect bounds = GetBounds(aDisplayListBuilder, &snap);
  ComputeDisjointRectangles(bounds, &rects);

  bool hasBorderRadius;
  bool nativeTheme = nsCSSRendering::HasBoxShadowNativeTheme(mFrame,
                                                             hasBorderRadius);

  // Don't need the full size of the shadow rect like we do in
  // nsCSSRendering since WR takes care of calculations for blur
  // and spread radius.
  nsRect frameRect = nsCSSRendering::GetShadowRect(borderRect,
                                                    nativeTheme,
                                                    mFrame);

  RectCornerRadii borderRadii;
  if (hasBorderRadius) {
    hasBorderRadius = nsCSSRendering::GetBorderRadii(frameRect,
                                                     borderRect,
                                                     mFrame,
                                                     borderRadii);
  }

  // Everything here is in app units, change to device units.
  for (uint32_t i = 0; i < rects.Length(); ++i) {
    LayoutDeviceRect clipRect = LayoutDeviceRect::FromAppUnits(
        rects[i], appUnitsPerDevPixel);
    nsCSSShadowArray* shadows = mFrame->StyleEffects()->mBoxShadow;
    MOZ_ASSERT(shadows);

    for (uint32_t j = shadows->Length(); j  > 0; j--) {
      nsCSSShadowItem* shadow = shadows->ShadowAt(j - 1);
      if (shadow->mInset) {
        continue;
      }

      float blurRadius = float(shadow->mRadius) / float(appUnitsPerDevPixel);
      gfx::Color shadowColor = nsCSSRendering::GetShadowColor(shadow,
                                                              mFrame,
                                                              mOpacity);

      // We don't move the shadow rect here since WR does it for us
      // Now translate everything to device pixels.
      nsRect shadowRect = frameRect;
      LayoutDevicePoint shadowOffset = LayoutDevicePoint::FromAppUnits(
          nsPoint(shadow->mXOffset, shadow->mYOffset),
          appUnitsPerDevPixel);

      LayoutDeviceRect deviceBox = LayoutDeviceRect::FromAppUnits(
          shadowRect, appUnitsPerDevPixel);
      wr::LayoutRect deviceBoxRect = wr::ToRoundedLayoutRect(deviceBox);
      wr::LayoutRect deviceClipRect = wr::ToRoundedLayoutRect(clipRect);

      LayoutDeviceSize zeroSize;
      wr::BorderRadius borderRadius = wr::ToBorderRadius(zeroSize, zeroSize,
                                                         zeroSize, zeroSize);
      if (hasBorderRadius) {
        borderRadius = wr::ToBorderRadius(
          LayoutDeviceSize::FromUnknownSize(borderRadii.TopLeft()),
          LayoutDeviceSize::FromUnknownSize(borderRadii.TopRight()),
          LayoutDeviceSize::FromUnknownSize(borderRadii.BottomLeft()),
          LayoutDeviceSize::FromUnknownSize(borderRadii.BottomRight()));
      }

      float spreadRadius = float(shadow->mSpread) / float(appUnitsPerDevPixel);

      aBuilder.PushBoxShadow(deviceBoxRect,
                             deviceClipRect,
                             !BackfaceIsHidden(),
                             deviceBoxRect,
                             wr::ToLayoutVector2D(shadowOffset),
                             wr::ToColorF(shadowColor),
                             blurRadius,
                             spreadRadius,
                             borderRadius,
                             wr::BoxShadowClipMode::Outset);
    }
  }

  return true;
}

void
nsDisplayBoxShadowOuter::ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                                   const nsDisplayItemGeometry* aGeometry,
                                                   nsRegion* aInvalidRegion) const
{
  const nsDisplayBoxShadowOuterGeometry* geometry =
    static_cast<const nsDisplayBoxShadowOuterGeometry*>(aGeometry);
  bool snap;
  if (!geometry->mBounds.IsEqualInterior(GetBounds(aBuilder, &snap)) ||
      !geometry->mBorderRect.IsEqualInterior(GetBorderRect()) ||
      mOpacity != geometry->mOpacity) {
    nsRegion oldShadow, newShadow;
    nscoord dontCare[8];
    bool hasBorderRadius = mFrame->GetBorderRadii(dontCare);
    if (hasBorderRadius) {
      // If we have rounded corners then we need to invalidate the frame area
      // too since we paint into it.
      oldShadow = geometry->mBounds;
      newShadow = GetBounds(aBuilder, &snap);
    } else {
      oldShadow.Sub(geometry->mBounds, geometry->mBorderRect);
      newShadow.Sub(GetBounds(aBuilder, &snap), GetBorderRect());
    }
    aInvalidRegion->Or(oldShadow, newShadow);
  }
}


void
nsDisplayBoxShadowInner::Paint(nsDisplayListBuilder* aBuilder,
                               gfxContext* aCtx) {
  nsPoint offset = ToReferenceFrame();
  nsRect borderRect = nsRect(offset, mFrame->GetSize());
  nsPresContext* presContext = mFrame->PresContext();
  AutoTArray<nsRect,10> rects;
  ComputeDisjointRectangles(mVisibleRegion, &rects);

  AUTO_PROFILER_LABEL("nsDisplayBoxShadowInner::Paint", GRAPHICS);

  DrawTarget* drawTarget = aCtx->GetDrawTarget();
  gfxContext* gfx = aCtx;
  int32_t appUnitsPerDevPixel = mFrame->PresContext()->AppUnitsPerDevPixel();

  for (uint32_t i = 0; i < rects.Length(); ++i) {
    gfx->Save();
    gfx->Clip(NSRectToSnappedRect(rects[i], appUnitsPerDevPixel, *drawTarget));
    nsCSSRendering::PaintBoxShadowInner(presContext, *aCtx, mFrame, borderRect);
    gfx->Restore();
  }
}

bool
nsDisplayBoxShadowInner::CanCreateWebRenderCommands(nsDisplayListBuilder* aBuilder,
                                                    nsIFrame* aFrame,
                                                    nsPoint aReferenceOffset)
{
  nsCSSShadowArray *shadows = aFrame->StyleEffects()->mBoxShadow;
  if (!shadows) {
    // Means we don't have to paint anything
    return true;
  }

  bool hasBorderRadius;
  bool nativeTheme =
    nsCSSRendering::HasBoxShadowNativeTheme(aFrame, hasBorderRadius);

  // We don't support native themed things yet like box shadows around
  // input buttons.
  if (nativeTheme) {
    return false;
  }

  return true;
}

/* static */ void
nsDisplayBoxShadowInner::CreateInsetBoxShadowWebRenderCommands(mozilla::wr::DisplayListBuilder& aBuilder,
                                                               const StackingContextHelper& aSc,
                                                               nsRegion& aVisibleRegion,
                                                               nsIFrame* aFrame,
                                                               const nsRect aBorderRect)
{
  if (!nsCSSRendering::ShouldPaintBoxShadowInner(aFrame)) {
    return;
  }

  int32_t appUnitsPerDevPixel = aFrame->PresContext()->AppUnitsPerDevPixel();

  AutoTArray<nsRect,10> rects;
  ComputeDisjointRectangles(aVisibleRegion, &rects);

  nsCSSShadowArray* shadows = aFrame->StyleEffects()->mBoxShadow;

  for (uint32_t i = 0; i < rects.Length(); ++i) {
    LayoutDeviceRect clipRect = LayoutDeviceRect::FromAppUnits(
        rects[i], appUnitsPerDevPixel);

    for (uint32_t i = shadows->Length(); i > 0; --i) {
      nsCSSShadowItem* shadowItem = shadows->ShadowAt(i - 1);
      if (!shadowItem->mInset) {
        continue;
      }

      nsRect shadowRect =
        nsCSSRendering::GetBoxShadowInnerPaddingRect(aFrame, aBorderRect);
      RectCornerRadii innerRadii;
      nsCSSRendering::GetShadowInnerRadii(aFrame, aBorderRect, innerRadii);

      // Now translate everything to device pixels.
      LayoutDeviceRect deviceBoxRect = LayoutDeviceRect::FromAppUnits(
          shadowRect, appUnitsPerDevPixel);
      wr::LayoutRect deviceClipRect = wr::ToRoundedLayoutRect(clipRect);
      Color shadowColor = nsCSSRendering::GetShadowColor(shadowItem, aFrame, 1.0);

      LayoutDevicePoint shadowOffset = LayoutDevicePoint::FromAppUnits(
          nsPoint(shadowItem->mXOffset, shadowItem->mYOffset),
          appUnitsPerDevPixel);

      float blurRadius = float(shadowItem->mRadius) / float(appUnitsPerDevPixel);

      wr::BorderRadius borderRadius = wr::ToBorderRadius(
        LayoutDeviceSize::FromUnknownSize(innerRadii.TopLeft()),
        LayoutDeviceSize::FromUnknownSize(innerRadii.TopRight()),
        LayoutDeviceSize::FromUnknownSize(innerRadii.BottomLeft()),
        LayoutDeviceSize::FromUnknownSize(innerRadii.BottomRight()));
      // NOTE: Any spread radius > 0 will render nothing. WR Bug.
      float spreadRadius = float(shadowItem->mSpread) / float(appUnitsPerDevPixel);

      aBuilder.PushBoxShadow(wr::ToLayoutRect(deviceBoxRect),
                             deviceClipRect,
                             !aFrame->BackfaceIsHidden(),
                             wr::ToLayoutRect(deviceBoxRect),
                             wr::ToLayoutVector2D(shadowOffset),
                             wr::ToColorF(shadowColor),
                             blurRadius,
                             spreadRadius,
                             borderRadius,
                             wr::BoxShadowClipMode::Inset
                             );
    }
  }
}

bool
nsDisplayBoxShadowInner::CreateWebRenderCommands(mozilla::wr::DisplayListBuilder& aBuilder,
                                                 mozilla::wr::IpcResourceUpdateQueue& aResources,
                                                 const StackingContextHelper& aSc,
                                                 mozilla::layers::WebRenderLayerManager* aManager,
                                                 nsDisplayListBuilder* aDisplayListBuilder)
{
  if (!CanCreateWebRenderCommands(aDisplayListBuilder, mFrame, ToReferenceFrame())) {
    return false;
  }

  bool snap;
  nsRegion visible = GetBounds(aDisplayListBuilder, &snap);
  nsPoint offset = ToReferenceFrame();
  nsRect borderRect = nsRect(offset, mFrame->GetSize());
  nsDisplayBoxShadowInner::CreateInsetBoxShadowWebRenderCommands(aBuilder, aSc, visible,
                                                                 mFrame, borderRect);

  return true;
}

bool
nsDisplayBoxShadowInner::ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                           nsRegion* aVisibleRegion) {
  if (!nsDisplayItem::ComputeVisibility(aBuilder, aVisibleRegion)) {
    return false;
  }

  mVisibleRegion.And(*aVisibleRegion, GetPaintRect());
  return true;
}

nsDisplayWrapList::nsDisplayWrapList(nsDisplayListBuilder* aBuilder,
                                     nsIFrame* aFrame, nsDisplayList* aList, bool aAnonymous)
  : nsDisplayWrapList(aBuilder, aFrame, aList,
                      aBuilder->CurrentActiveScrolledRoot(), false, 0, aAnonymous)
{}

nsDisplayWrapList::nsDisplayWrapList(nsDisplayListBuilder* aBuilder,
                                     nsIFrame* aFrame, nsDisplayList* aList,
                                     const ActiveScrolledRoot* aActiveScrolledRoot,
                                     bool aClearClipChain,
                                     uint32_t aIndex,
                                     bool aAnonymous)
  : nsDisplayItem(aBuilder, aFrame, aActiveScrolledRoot, aAnonymous)
  , mFrameActiveScrolledRoot(aBuilder->CurrentActiveScrolledRoot())
  , mOverrideZIndex(0)
  , mIndex(aIndex)
  , mHasZIndexOverride(false)
  , mClearingClipChain(aClearClipChain)
{
  MOZ_COUNT_CTOR(nsDisplayWrapList);

  mBaseBuildingRect = GetBuildingRect();

  mListPtr = &mList;
  mListPtr->AppendToTop(aList);
  UpdateBounds(aBuilder);

  if (!aFrame || !aFrame->IsTransformed()) {
    return;
  }

  // If we're a transformed frame, then we need to find out if we're inside
  // the nsDisplayTransform or outside of it. Frames inside the transform
  // need mReferenceFrame == mFrame, outside needs the next ancestor
  // reference frame.
  // If we're inside the transform, then the nsDisplayItem constructor
  // will have done the right thing.
  // If we're outside the transform, then we should have only one child
  // (since nsDisplayTransform wraps all actual content), and that child
  // will have the correct reference frame set (since nsDisplayTransform
  // handles this explictly).
  nsDisplayItem *i = mListPtr->GetBottom();
  if (i && (!i->GetAbove() || i->GetType() == DisplayItemType::TYPE_TRANSFORM) &&
      i->Frame() == mFrame) {
    mReferenceFrame = i->ReferenceFrame();
    mToReferenceFrame = i->ToReferenceFrame();
  }

  nsRect visible = aBuilder->GetVisibleRect() +
    aBuilder->GetCurrentFrameOffsetToReferenceFrame();

  SetBuildingRect(visible);
}

nsDisplayWrapList::nsDisplayWrapList(nsDisplayListBuilder* aBuilder,
                                     nsIFrame* aFrame, nsDisplayItem* aItem, bool aAnonymous)
  : nsDisplayItem(aBuilder, aFrame, aBuilder->CurrentActiveScrolledRoot(), aAnonymous)
  , mOverrideZIndex(0)
  , mIndex(0)
  , mHasZIndexOverride(false)
{
  MOZ_COUNT_CTOR(nsDisplayWrapList);

  mBaseBuildingRect = GetBuildingRect();

  mListPtr = &mList;
  mListPtr->AppendToTop(aItem);
  UpdateBounds(aBuilder);

  if (!aFrame || !aFrame->IsTransformed()) {
    return;
  }

  // See the previous nsDisplayWrapList constructor
  if (aItem->Frame() == aFrame) {
    mReferenceFrame = aItem->ReferenceFrame();
    mToReferenceFrame = aItem->ToReferenceFrame();
  }

  nsRect visible = aBuilder->GetVisibleRect() +
    aBuilder->GetCurrentFrameOffsetToReferenceFrame();

  SetBuildingRect(visible);
}

nsDisplayWrapList::~nsDisplayWrapList() {
  MOZ_COUNT_DTOR(nsDisplayWrapList);
}

void
nsDisplayWrapList::MergeDisplayListFromItem(nsDisplayListBuilder* aBuilder,
                                            const nsDisplayItem* aItem)
{
  const nsDisplayWrapList* wrappedItem = aItem->AsDisplayWrapList();
  MOZ_ASSERT(wrappedItem);

  // Create a new nsDisplayWrapList using a copy-constructor. This is done
  // to preserve the information about bounds.
  nsDisplayWrapList* wrapper = MakeDisplayItem<nsDisplayWrapList>(aBuilder, *wrappedItem);

  // Set the display list pointer of the new wrapper item to the display list
  // of the wrapped item.
  wrapper->mListPtr = wrappedItem->mListPtr;

  mListPtr->AppendToBottom(wrapper);
}

void
nsDisplayWrapList::HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
                           HitTestState* aState, nsTArray<nsIFrame*> *aOutFrames) {
  mListPtr->HitTest(aBuilder, aRect, aState, aOutFrames);
}

nsRect
nsDisplayWrapList::GetBounds(nsDisplayListBuilder* aBuilder,
                             bool* aSnap) const
{
  *aSnap = false;
  return mBounds;
}

bool
nsDisplayWrapList::ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                     nsRegion* aVisibleRegion) {
  // Convert the passed in visible region to our appunits.
  nsRegion visibleRegion;
  // mVisibleRect has been clipped to GetClippedBounds
  visibleRegion.And(*aVisibleRegion, GetPaintRect());
  nsRegion originalVisibleRegion = visibleRegion;

  bool retval =
    mListPtr->ComputeVisibilityForSublist(aBuilder,
                                          &visibleRegion,
                                          GetPaintRect());
  nsRegion removed;
  // removed = originalVisibleRegion - visibleRegion
  removed.Sub(originalVisibleRegion, visibleRegion);
  // aVisibleRegion = aVisibleRegion - removed (modulo any simplifications
  // SubtractFromVisibleRegion does)
  aBuilder->SubtractFromVisibleRegion(aVisibleRegion, removed);

  return retval;
}

nsRegion
nsDisplayWrapList::GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                                   bool* aSnap) const
{
  *aSnap = false;
  nsRegion result;
  if (mListPtr->IsOpaque()) {
    // Everything within GetBounds that's visible is opaque.
    result = GetBounds(aBuilder, aSnap);
  } else if (aBuilder->HitTestIsForVisibility()) {
    // If we care about an accurate opaque region, iterate the display list
    // and build up a region of opaque bounds.
    nsDisplayItem* item = mList.GetBottom();
    while (item) {
      result.OrWith(item->GetOpaqueRegion(aBuilder, aSnap));
      item = item->GetAbove();
    }
  }
  *aSnap = false;
  return result;
}

Maybe<nscolor>
nsDisplayWrapList::IsUniform(nsDisplayListBuilder* aBuilder) const
{
  // We could try to do something but let's conservatively just return Nothing.
  return Nothing();
}

void nsDisplayWrapList::Paint(nsDisplayListBuilder* aBuilder,
                              gfxContext* aCtx) {
  NS_ERROR("nsDisplayWrapList should have been flattened away for painting");
}

/**
 * Returns true if all descendant display items can be placed in the same
 * PaintedLayer --- GetLayerState returns LAYER_INACTIVE or LAYER_NONE,
 * and they all have the expected animated geometry root.
 */
static LayerState
RequiredLayerStateForChildren(nsDisplayListBuilder* aBuilder,
                              LayerManager* aManager,
                              const ContainerLayerParameters& aParameters,
                              const nsDisplayList& aList,
                              AnimatedGeometryRoot* aExpectedAnimatedGeometryRootForChildren)
{
  LayerState result = LAYER_INACTIVE;
  for (nsDisplayItem* i = aList.GetBottom(); i; i = i->GetAbove()) {
    if (result == LAYER_INACTIVE &&
        i->GetAnimatedGeometryRoot() != aExpectedAnimatedGeometryRootForChildren) {
      result = LAYER_ACTIVE;
    }

    LayerState state = i->GetLayerState(aBuilder, aManager, aParameters);
    if (state == LAYER_ACTIVE &&
        (i->GetType() == DisplayItemType::TYPE_BLEND_MODE ||
         i->GetType() == DisplayItemType::TYPE_TABLE_BLEND_MODE)) {
      // nsDisplayBlendMode always returns LAYER_ACTIVE to ensure that the
      // blending operation happens in the intermediate surface of its parent
      // display item (usually an nsDisplayBlendContainer). But this does not
      // mean that it needs all its ancestor display items to become active.
      // So we ignore its layer state and look at its children instead.
      state = RequiredLayerStateForChildren(aBuilder, aManager, aParameters,
        *i->GetSameCoordinateSystemChildren(), i->GetAnimatedGeometryRoot());
    }
    if ((state == LAYER_ACTIVE || state == LAYER_ACTIVE_FORCE) &&
        state > result) {
      result = state;
    }
    if (state == LAYER_ACTIVE_EMPTY && state > result) {
      result = LAYER_ACTIVE_FORCE;
    }
    if (state == LAYER_NONE) {
      nsDisplayList* list = i->GetSameCoordinateSystemChildren();
      if (list) {
        LayerState childState =
          RequiredLayerStateForChildren(aBuilder, aManager, aParameters, *list,
              aExpectedAnimatedGeometryRootForChildren);
        if (childState > result) {
          result = childState;
        }
      }
    }
  }
  return result;
}

nsRect nsDisplayWrapList::GetComponentAlphaBounds(nsDisplayListBuilder* aBuilder) const
{
  nsRect bounds;
  for (nsDisplayItem* i = mListPtr->GetBottom(); i; i = i->GetAbove()) {
    bounds.UnionRect(bounds, i->GetComponentAlphaBounds(aBuilder));
  }
  return bounds;
}

void
nsDisplayWrapList::SetReferenceFrame(const nsIFrame* aFrame)
{
  mReferenceFrame = aFrame;
  mToReferenceFrame = mFrame->GetOffsetToCrossDoc(mReferenceFrame);
}

bool
nsDisplayWrapList::CreateWebRenderCommands(mozilla::wr::DisplayListBuilder& aBuilder,
                                           mozilla::wr::IpcResourceUpdateQueue& aResources,
                                           const StackingContextHelper& aSc,
                                           mozilla::layers::WebRenderLayerManager* aManager,
                                           nsDisplayListBuilder* aDisplayListBuilder)
{
  aManager->CommandBuilder().CreateWebRenderCommandsFromDisplayList(GetChildren(),
                                                                    this,
                                                                    aDisplayListBuilder,
                                                                    aSc,
                                                                    aBuilder,
                                                                    aResources);
  return true;
}

static nsresult
WrapDisplayList(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                nsDisplayList* aList, nsDisplayWrapper* aWrapper) {
  if (!aList->GetTop())
    return NS_OK;
  nsDisplayItem* item = aWrapper->WrapList(aBuilder, aFrame, aList);
  if (!item)
    return NS_ERROR_OUT_OF_MEMORY;
  // aList was emptied
  aList->AppendToTop(item);
  return NS_OK;
}

static nsresult
WrapEachDisplayItem(nsDisplayListBuilder* aBuilder,
                    nsDisplayList* aList, nsDisplayWrapper* aWrapper) {
  nsDisplayList newList;
  nsDisplayItem* item;
  while ((item = aList->RemoveBottom())) {
    item = aWrapper->WrapItem(aBuilder, item);
    if (!item)
      return NS_ERROR_OUT_OF_MEMORY;
    newList.AppendToTop(item);
  }
  // aList was emptied
  aList->AppendToTop(&newList);
  return NS_OK;
}

nsresult nsDisplayWrapper::WrapLists(nsDisplayListBuilder* aBuilder,
    nsIFrame* aFrame, const nsDisplayListSet& aIn, const nsDisplayListSet& aOut)
{
  nsresult rv = WrapListsInPlace(aBuilder, aFrame, aIn);
  NS_ENSURE_SUCCESS(rv, rv);

  if (&aOut == &aIn)
    return NS_OK;
  aOut.BorderBackground()->AppendToTop(aIn.BorderBackground());
  aOut.BlockBorderBackgrounds()->AppendToTop(aIn.BlockBorderBackgrounds());
  aOut.Floats()->AppendToTop(aIn.Floats());
  aOut.Content()->AppendToTop(aIn.Content());
  aOut.PositionedDescendants()->AppendToTop(aIn.PositionedDescendants());
  aOut.Outlines()->AppendToTop(aIn.Outlines());
  return NS_OK;
}

nsresult nsDisplayWrapper::WrapListsInPlace(nsDisplayListBuilder* aBuilder,
    nsIFrame* aFrame, const nsDisplayListSet& aLists)
{
  nsresult rv;
  if (WrapBorderBackground()) {
    // Our border-backgrounds are in-flow
    rv = WrapDisplayList(aBuilder, aFrame, aLists.BorderBackground(), this);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  // Our block border-backgrounds are in-flow
  rv = WrapDisplayList(aBuilder, aFrame, aLists.BlockBorderBackgrounds(), this);
  NS_ENSURE_SUCCESS(rv, rv);
  // The floats are not in flow
  rv = WrapEachDisplayItem(aBuilder, aLists.Floats(), this);
  NS_ENSURE_SUCCESS(rv, rv);
  // Our child content is in flow
  rv = WrapDisplayList(aBuilder, aFrame, aLists.Content(), this);
  NS_ENSURE_SUCCESS(rv, rv);
  // The positioned descendants may not be in-flow
  rv = WrapEachDisplayItem(aBuilder, aLists.PositionedDescendants(), this);
  NS_ENSURE_SUCCESS(rv, rv);
  // The outlines may not be in-flow
  return WrapEachDisplayItem(aBuilder, aLists.Outlines(), this);
}

nsDisplayOpacity::nsDisplayOpacity(nsDisplayListBuilder* aBuilder,
                                   nsIFrame* aFrame, nsDisplayList* aList,
                                   const ActiveScrolledRoot* aActiveScrolledRoot,
                                   bool aForEventsAndPluginsOnly)
    : nsDisplayWrapList(aBuilder, aFrame, aList, aActiveScrolledRoot, true)
    , mOpacity(aFrame->StyleEffects()->mOpacity)
    , mForEventsAndPluginsOnly(aForEventsAndPluginsOnly)
    , mOpacityAppliedToChildren(false)
{
  MOZ_COUNT_CTOR(nsDisplayOpacity);
  mState.mOpacity = mOpacity;
}

#ifdef NS_BUILD_REFCNT_LOGGING
nsDisplayOpacity::~nsDisplayOpacity() {
  MOZ_COUNT_DTOR(nsDisplayOpacity);
}
#endif

nsRegion nsDisplayOpacity::GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                                           bool* aSnap) const
{
  *aSnap = false;
  // The only time where mOpacity == 1.0 should be when we have will-change.
  // We could report this as opaque then but when the will-change value starts
  // animating the element would become non opaque and could cause repaints.
  return nsRegion();
}

// nsDisplayOpacity uses layers for rendering
already_AddRefed<Layer>
nsDisplayOpacity::BuildLayer(nsDisplayListBuilder* aBuilder,
                             LayerManager* aManager,
                             const ContainerLayerParameters& aContainerParameters) {
  ContainerLayerParameters params = aContainerParameters;
  params.mForEventsAndPluginsOnly = mForEventsAndPluginsOnly;
  RefPtr<Layer> container = aManager->GetLayerBuilder()->
    BuildContainerLayerFor(aBuilder, aManager, mFrame, this, &mList,
                           params, nullptr,
                           FrameLayerBuilder::CONTAINER_ALLOW_PULL_BACKGROUND_COLOR);
  if (!container)
    return nullptr;

  container->SetOpacity(mOpacity);
  nsDisplayListBuilder::AddAnimationsAndTransitionsToLayer(container, aBuilder,
                                                           this, mFrame,
                                                           eCSSProperty_opacity);
  return container.forget();
}

/**
 * This doesn't take into account layer scaling --- the layer may be
 * rendered at a higher (or lower) resolution, affecting the retained layer
 * size --- but this should be good enough.
 */
static bool
IsItemTooSmallForActiveLayer(nsIFrame* aFrame)
{
  nsIntRect visibleDevPixels = aFrame->GetVisualOverflowRectRelativeToSelf().ToOutsidePixels(
          aFrame->PresContext()->AppUnitsPerDevPixel());
  return visibleDevPixels.Size() <
    nsIntSize(gfxPrefs::LayoutMinActiveLayerSize(),
              gfxPrefs::LayoutMinActiveLayerSize());
}

/* static */ bool
nsDisplayOpacity::NeedsActiveLayer(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame)
{
  if (EffectCompositor::HasAnimationsForCompositor(aFrame,
                                                   eCSSProperty_opacity) ||
      (ActiveLayerTracker::IsStyleAnimated(aBuilder, aFrame,
                                           eCSSProperty_opacity) &&
       !IsItemTooSmallForActiveLayer(aFrame))) {
    return true;
  }
  return false;
}

void
nsDisplayOpacity::ApplyOpacity(nsDisplayListBuilder* aBuilder,
                             float aOpacity,
                             const DisplayItemClipChain* aClip)
{
  NS_ASSERTION(CanApplyOpacity(), "ApplyOpacity should be allowed");
  mOpacity = mOpacity * aOpacity;
  IntersectClip(aBuilder, aClip, false);
}

bool
nsDisplayOpacity::CanApplyOpacity() const
{
  return !EffectCompositor::HasAnimationsForCompositor(mFrame,
                                                       eCSSProperty_opacity);
}

/**
 * Recursively iterates through |aList| and collects at most |aMaxChildCount|
 * display item pointers to items that return true for CanApplyOpacity().
 * The item pointers are added to |aArray|.
 *
 * LayerEventRegions and WrapList items are ignored.
 *
 * We need to do this recursively, because the child display items might contain
 * nested nsDisplayWrapLists.
 *
 * Returns false if there are more than |aMaxChildCount| items, or if an item
 * that returns false for CanApplyOpacity() is encountered.
 * Otherwise returns true.
 */
static bool
CollectItemsWithOpacity(nsDisplayList* aList,
                     nsTArray<nsDisplayItem*>& aArray,
                     const size_t aMaxChildCount)
{
  for (nsDisplayItem* i = aList->GetBottom(); i; i = i->GetAbove()) {
    DisplayItemType type = i->GetType();
    nsDisplayList* children = i->GetChildren();

    // Descend only into wraplists.
    if (type == DisplayItemType::TYPE_WRAP_LIST && children) {
      // The current display item has children, process them first.
      if (!CollectItemsWithOpacity(children, aArray, aMaxChildCount)) {
        return false;
      }
    }

    if (type == DisplayItemType::TYPE_COMPOSITOR_HITTEST_INFO ||
        type == DisplayItemType::TYPE_WRAP_LIST) {
      continue;
    }

    if (!i->CanApplyOpacity() || aArray.Length() == aMaxChildCount) {
      return false;
    }

    aArray.AppendElement(i);
  }

  return true;
}

bool
nsDisplayOpacity::ApplyOpacityToChildren(nsDisplayListBuilder* aBuilder)
{
  // Only try folding our opacity down if we have at most kMaxChildCount
  // children that don't overlap and can all apply the opacity to themselves.
  static const size_t kMaxChildCount = 3;

  // Iterate through the child display list and copy at most kMaxChildCount
  // child display item pointers to a temporary list.
  AutoTArray<nsDisplayItem*, kMaxChildCount> items;
  if (!CollectItemsWithOpacity(&mList, items, kMaxChildCount)) {
    return false;
  }

  struct {
    nsDisplayItem* item;
    nsRect bounds;
  } children[kMaxChildCount];

  bool snap;
  size_t childCount = 0;
  for (nsDisplayItem* item : items) {
    children[childCount].item = item;
    children[childCount].bounds = item->GetBounds(aBuilder, &snap);
    childCount++;
  }

  for (size_t i = 0; i < childCount; i++) {
    for (size_t j = i+1; j < childCount; j++) {
      if (children[i].bounds.Intersects(children[j].bounds)) {
        return false;
      }
    }
  }

  for (uint32_t i = 0; i < childCount; i++) {
    children[i].item->ApplyOpacity(aBuilder, mOpacity, mClipChain);
  }

  mOpacityAppliedToChildren = true;
  return true;
}

bool
nsDisplayOpacity::ShouldFlattenAway(nsDisplayListBuilder* aBuilder)
{
  // ShouldFlattenAway() should be called only once during painting.
  MOZ_ASSERT(!mOpacityAppliedToChildren);

  if (mFrame->GetPrevContinuation() || mFrame->GetNextContinuation() ||
      mFrame->HasAnyStateBits(NS_FRAME_PART_OF_IBSPLIT)) {
    // If we've been split, then we might need to merge, so
    // don't flatten us away.
    return false;
  }

  if (NeedsActiveLayer(aBuilder, mFrame) || mOpacity == 0.0) {
    // If our opacity is zero then we'll discard all descendant display items
    // except for layer event regions, so there's no point in doing this
    // optimization (and if we do do it, then invalidations of those descendants
    // might trigger repainting).
    return false;
  }

  if (mList.IsEmpty()) {
    return false;
  }

  // Return true if we successfully applied opacity to child items, or if
  // WebRender is not in use. In the latter case, the opacity gets flattened and
  // applied during layer building.
  return ApplyOpacityToChildren(aBuilder) || !gfxVars::UseWebRender();
}

nsDisplayItem::LayerState
nsDisplayOpacity::GetLayerState(nsDisplayListBuilder* aBuilder,
                                LayerManager* aManager,
                                const ContainerLayerParameters& aParameters) {
  // If we only created this item so that we'd get correct nsDisplayEventRegions for child
  // frames, then force us to inactive to avoid unnecessary layerization changes for content
  // that won't ever be painted.
  if (mForEventsAndPluginsOnly) {
    MOZ_ASSERT(mOpacity == 0);
    return LAYER_INACTIVE;
  }

  if (NeedsActiveLayer(aBuilder, mFrame)) {
    // Returns LAYER_ACTIVE_FORCE to avoid flatterning the layer for async
    // animations.
    return LAYER_ACTIVE_FORCE;
  }

  return RequiredLayerStateForChildren(aBuilder, aManager, aParameters, mList, GetAnimatedGeometryRoot());
}

bool
nsDisplayOpacity::ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                    nsRegion* aVisibleRegion) {
  // Our children are translucent so we should not allow them to subtract
  // area from aVisibleRegion. We do need to find out what is visible under
  // our children in the temporary compositing buffer, because if our children
  // paint our entire bounds opaquely then we don't need an alpha channel in
  // the temporary compositing buffer.
  nsRect bounds = GetClippedBounds(aBuilder);
  nsRegion visibleUnderChildren;
  visibleUnderChildren.And(*aVisibleRegion, bounds);
  return
    nsDisplayWrapList::ComputeVisibility(aBuilder, &visibleUnderChildren);
}

void
nsDisplayOpacity::ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                            const nsDisplayItemGeometry* aGeometry,
                                            nsRegion* aInvalidRegion) const
{
  const nsDisplayOpacityGeometry* geometry =
    static_cast<const nsDisplayOpacityGeometry*>(aGeometry);

  bool snap;
  if (mOpacity != geometry->mOpacity) {
    aInvalidRegion->Or(GetBounds(aBuilder, &snap), geometry->mBounds);
  }
}

void
nsDisplayOpacity::WriteDebugInfo(std::stringstream& aStream)
{
  aStream << " (opacity " << mOpacity << ")";
}

bool
nsDisplayOpacity::CreateWebRenderCommands(mozilla::wr::DisplayListBuilder& aBuilder,
                                          mozilla::wr::IpcResourceUpdateQueue& aResources,
                                          const StackingContextHelper& aSc,
                                          mozilla::layers::WebRenderLayerManager* aManager,
                                          nsDisplayListBuilder* aDisplayListBuilder)
{
  float* opacityForSC = &mOpacity;

  RefPtr<WebRenderAnimationData> animationData = aManager->CommandBuilder().CreateOrRecycleWebRenderUserData<WebRenderAnimationData>(this);
  AnimationInfo& animationInfo = animationData->GetAnimationInfo();
  AddAnimationsForProperty(Frame(), aDisplayListBuilder,
                           this, eCSSProperty_opacity,
                           animationInfo, false, true);
  animationInfo.StartPendingAnimations(aManager->GetAnimationReadyTime());

  // Note that animationsId can be 0 (uninitialized in AnimationInfo) if there
  // are no active animations.
  uint64_t animationsId = animationInfo.GetCompositorAnimationsId();
  wr::WrAnimationProperty prop;

  if (!animationInfo.GetAnimations().IsEmpty()) {
    prop.id = animationsId;
    prop.effect_type = wr::WrAnimationType::Opacity;


    OpAddCompositorAnimations
      anim(CompositorAnimations(animationInfo.GetAnimations(), animationsId));
    aManager->WrBridge()->AddWebRenderParentCommand(anim);
    aManager->AddActiveCompositorAnimationId(animationsId);
  } else if (animationsId) {
    aManager->AddCompositorAnimationsIdForDiscard(animationsId);
    animationsId = 0;
  }

  nsTArray<mozilla::wr::WrFilterOp> filters;
  StackingContextHelper sc(aSc, aBuilder, filters, LayoutDeviceRect(), nullptr,
                           animationsId ? &prop : nullptr,
                           opacityForSC);

  aManager->CommandBuilder().CreateWebRenderCommandsFromDisplayList(&mList,
                                                                    this,
                                                                    aDisplayListBuilder,
                                                                    sc,
                                                                    aBuilder,
                                                                    aResources);
  return true;
}

nsDisplayBlendMode::nsDisplayBlendMode(nsDisplayListBuilder* aBuilder,
                                             nsIFrame* aFrame, nsDisplayList* aList,
                                             uint8_t aBlendMode,
                                             const ActiveScrolledRoot* aActiveScrolledRoot,
                                             uint32_t aIndex)
  : nsDisplayWrapList(aBuilder, aFrame, aList, aActiveScrolledRoot, true)
  , mBlendMode(aBlendMode)
  , mIndex(aIndex)
{
  MOZ_COUNT_CTOR(nsDisplayBlendMode);
}

#ifdef NS_BUILD_REFCNT_LOGGING
nsDisplayBlendMode::~nsDisplayBlendMode() {
  MOZ_COUNT_DTOR(nsDisplayBlendMode);
}
#endif

nsRegion nsDisplayBlendMode::GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                                                bool* aSnap) const
{
  *aSnap = false;
  // We are never considered opaque
  return nsRegion();
}

LayerState
nsDisplayBlendMode::GetLayerState(nsDisplayListBuilder* aBuilder,
                                     LayerManager* aManager,
                                     const ContainerLayerParameters& aParameters)
{
  return LAYER_ACTIVE;
}

bool
nsDisplayBlendMode::CreateWebRenderCommands(mozilla::wr::DisplayListBuilder& aBuilder,
                                            mozilla::wr::IpcResourceUpdateQueue& aResources,
                                            const StackingContextHelper& aSc,
                                            mozilla::layers::WebRenderLayerManager* aManager,
                                            nsDisplayListBuilder* aDisplayListBuilder)
{
  nsTArray<mozilla::wr::WrFilterOp> filters;
  StackingContextHelper sc(aSc, aBuilder, filters, LayoutDeviceRect(), nullptr,
                           nullptr, nullptr, nullptr, nullptr,
                           nsCSSRendering::GetGFXBlendMode(mBlendMode));

  return nsDisplayWrapList::CreateWebRenderCommands(aBuilder,aResources, sc,
                                                    aManager, aDisplayListBuilder);
}

// nsDisplayBlendMode uses layers for rendering
already_AddRefed<Layer>
nsDisplayBlendMode::BuildLayer(nsDisplayListBuilder* aBuilder,
                                  LayerManager* aManager,
                                  const ContainerLayerParameters& aContainerParameters) {
  ContainerLayerParameters newContainerParameters = aContainerParameters;
  newContainerParameters.mDisableSubpixelAntialiasingInDescendants = true;

  RefPtr<Layer> container = aManager->GetLayerBuilder()->
  BuildContainerLayerFor(aBuilder, aManager, mFrame, this, &mList,
                         newContainerParameters, nullptr);
  if (!container) {
    return nullptr;
  }

  container->SetMixBlendMode(nsCSSRendering::GetGFXBlendMode(mBlendMode));

  return container.forget();
}

mozilla::gfx::CompositionOp
nsDisplayBlendMode::BlendMode()
{
  return nsCSSRendering::GetGFXBlendMode(mBlendMode);
}

bool
nsDisplayBlendMode::ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                              nsRegion* aVisibleRegion) {
  // Our children are need their backdrop so we should not allow them to subtract
  // area from aVisibleRegion. We do need to find out what is visible under
  // our children in the temporary compositing buffer, because if our children
  // paint our entire bounds opaquely then we don't need an alpha channel in
  // the temporary compositing buffer.
  nsRect bounds = GetClippedBounds(aBuilder);
  nsRegion visibleUnderChildren;
  visibleUnderChildren.And(*aVisibleRegion, bounds);
  return nsDisplayWrapList::ComputeVisibility(aBuilder, &visibleUnderChildren);
}

bool
nsDisplayBlendMode::CanMerge(const nsDisplayItem* aItem) const
{
  // Items for the same content element should be merged into a single
  // compositing group.
  if (!HasSameTypeAndClip(aItem) || !HasSameContent(aItem)) {
    return false;
  }

  const nsDisplayBlendMode* item =
    static_cast<const nsDisplayBlendMode*>(aItem);

  if (item->mIndex != 0 || mIndex != 0) {
    // Don't merge background-blend-mode items
    return false;
  }

  return true;
}

/* static */ nsDisplayBlendContainer*
nsDisplayBlendContainer::CreateForMixBlendMode(nsDisplayListBuilder* aBuilder,
                                               nsIFrame* aFrame, nsDisplayList* aList,
                                               const ActiveScrolledRoot* aActiveScrolledRoot)
{
  return MakeDisplayItem<nsDisplayBlendContainer>(aBuilder, aFrame, aList, aActiveScrolledRoot, false);
}

/* static */ nsDisplayBlendContainer*
nsDisplayBlendContainer::CreateForBackgroundBlendMode(nsDisplayListBuilder* aBuilder,
                                                      nsIFrame* aFrame, nsDisplayList* aList,
                                                      const ActiveScrolledRoot* aActiveScrolledRoot)
{
  return MakeDisplayItem<nsDisplayBlendContainer>(aBuilder, aFrame, aList, aActiveScrolledRoot, true);
}

nsDisplayBlendContainer::nsDisplayBlendContainer(nsDisplayListBuilder* aBuilder,
                                                 nsIFrame* aFrame, nsDisplayList* aList,
                                                 const ActiveScrolledRoot* aActiveScrolledRoot,
                                                 bool aIsForBackground)
    : nsDisplayWrapList(aBuilder, aFrame, aList, aActiveScrolledRoot, true)
    , mIsForBackground(aIsForBackground)
{
  MOZ_COUNT_CTOR(nsDisplayBlendContainer);
}

#ifdef NS_BUILD_REFCNT_LOGGING
nsDisplayBlendContainer::~nsDisplayBlendContainer() {
  MOZ_COUNT_DTOR(nsDisplayBlendContainer);
}
#endif

// nsDisplayBlendContainer uses layers for rendering
already_AddRefed<Layer>
nsDisplayBlendContainer::BuildLayer(nsDisplayListBuilder* aBuilder,
                                    LayerManager* aManager,
                                    const ContainerLayerParameters& aContainerParameters) {
  // turn off anti-aliasing in the parent stacking context because it changes
  // how the group is initialized.
  ContainerLayerParameters newContainerParameters = aContainerParameters;
  newContainerParameters.mDisableSubpixelAntialiasingInDescendants = true;

  RefPtr<Layer> container = aManager->GetLayerBuilder()->
  BuildContainerLayerFor(aBuilder, aManager, mFrame, this, &mList,
                         newContainerParameters, nullptr);
  if (!container) {
    return nullptr;
  }

  container->SetForceIsolatedGroup(true);
  return container.forget();
}

LayerState
nsDisplayBlendContainer::GetLayerState(nsDisplayListBuilder* aBuilder,
                                       LayerManager* aManager,
                                       const ContainerLayerParameters& aParameters)
{
  return RequiredLayerStateForChildren(aBuilder, aManager, aParameters, mList, GetAnimatedGeometryRoot());
}

bool
nsDisplayBlendContainer::CreateWebRenderCommands(mozilla::wr::DisplayListBuilder& aBuilder,
                                                 mozilla::wr::IpcResourceUpdateQueue& aResources,
                                                 const StackingContextHelper& aSc,
                                                 mozilla::layers::WebRenderLayerManager* aManager,
                                                 nsDisplayListBuilder* aDisplayListBuilder)
{
  StackingContextHelper sc(aSc, aBuilder);

  return nsDisplayWrapList::CreateWebRenderCommands(aBuilder, aResources, sc,
                                                    aManager, aDisplayListBuilder);
}

/* static */ nsDisplayTableBlendContainer*
nsDisplayTableBlendContainer::CreateForBackgroundBlendMode(nsDisplayListBuilder* aBuilder,
                                                           nsIFrame* aFrame, nsDisplayList* aList,
                                                           const ActiveScrolledRoot* aActiveScrolledRoot,
                                                           nsIFrame* aAncestorFrame)
{
  return MakeDisplayItem<nsDisplayTableBlendContainer>(aBuilder, aFrame, aList, aActiveScrolledRoot, true, aAncestorFrame);
}

nsDisplayOwnLayer::nsDisplayOwnLayer(nsDisplayListBuilder* aBuilder,
                                     nsIFrame* aFrame, nsDisplayList* aList,
                                     const ActiveScrolledRoot* aActiveScrolledRoot,
                                     nsDisplayOwnLayerFlags aFlags,
                                     const ScrollbarData& aScrollbarData,
                                     bool aForceActive,
                                     bool aClearClipChain)
    : nsDisplayWrapList(aBuilder, aFrame, aList, aActiveScrolledRoot, aClearClipChain)
    , mFlags(aFlags)
    , mScrollbarData(aScrollbarData)
    , mForceActive(aForceActive)
    , mWrAnimationId(0)
{
  MOZ_COUNT_CTOR(nsDisplayOwnLayer);

  // For scroll thumb layers, override the AGR to be the thumb's AGR rather
  // than the AGR for mFrame (which is the slider frame).
  if (IsScrollThumbLayer()) {
    if (nsIFrame* thumbFrame = nsBox::GetChildXULBox(mFrame)) {
      mAnimatedGeometryRoot = aBuilder->FindAnimatedGeometryRootFor(thumbFrame);
    }
  }
}

#ifdef NS_BUILD_REFCNT_LOGGING
nsDisplayOwnLayer::~nsDisplayOwnLayer() {
  MOZ_COUNT_DTOR(nsDisplayOwnLayer);
}
#endif

LayerState
nsDisplayOwnLayer::GetLayerState(nsDisplayListBuilder* aBuilder,
                                 LayerManager* aManager,
                                 const ContainerLayerParameters& aParameters)
{
  if (mForceActive) {
    return mozilla::LAYER_ACTIVE_FORCE;
  }

  return RequiredLayerStateForChildren(aBuilder, aManager, aParameters, mList, mAnimatedGeometryRoot);
}

bool
nsDisplayOwnLayer::IsScrollThumbLayer() const
{
  return mScrollbarData.mScrollbarLayerType == layers::ScrollbarLayerType::Thumb;
}

bool
nsDisplayOwnLayer::IsScrollbarContainer() const
{
  return mScrollbarData.mScrollbarLayerType == layers::ScrollbarLayerType::Container;
}

bool
nsDisplayOwnLayer::ShouldBuildLayerEvenIfInvisible(nsDisplayListBuilder* aBuilder) const
{
  // Render scroll thumb layers even if they are invisible, because async
  // scrolling might bring them into view.
  return IsScrollThumbLayer();
}

// nsDisplayOpacity uses layers for rendering
already_AddRefed<Layer>
nsDisplayOwnLayer::BuildLayer(nsDisplayListBuilder* aBuilder,
                              LayerManager* aManager,
                              const ContainerLayerParameters& aContainerParameters)
{
  RefPtr<ContainerLayer> layer = aManager->GetLayerBuilder()->
    BuildContainerLayerFor(aBuilder, aManager, mFrame, this, &mList,
                           aContainerParameters, nullptr,
                           FrameLayerBuilder::CONTAINER_ALLOW_PULL_BACKGROUND_COLOR);

  if (IsScrollThumbLayer() || IsScrollbarContainer()) {
    layer->SetScrollbarData(mScrollbarData);
  }

  if (mFlags & nsDisplayOwnLayerFlags::eGenerateSubdocInvalidations) {
    mFrame->PresContext()->SetNotifySubDocInvalidationData(layer);
  }
  return layer.forget();
}

bool
nsDisplayOwnLayer::CreateWebRenderCommands(mozilla::wr::DisplayListBuilder& aBuilder,
                                           mozilla::wr::IpcResourceUpdateQueue& aResources,
                                           const StackingContextHelper& aSc,
                                           WebRenderLayerManager* aManager,
                                           nsDisplayListBuilder* aDisplayListBuilder)
{
  if (!aManager->AsyncPanZoomEnabled() || !IsScrollThumbLayer()) {
    return nsDisplayWrapList::CreateWebRenderCommands(aBuilder, aResources, aSc,
                                                      aManager, aDisplayListBuilder);
  }

  // APZ is enabled and this is a scroll thumb, so we need to create and
  // set an animation id. That way APZ can move this scrollthumb around as
  // needed.
  RefPtr<WebRenderAnimationData> animationData = aManager->CommandBuilder().CreateOrRecycleWebRenderUserData<WebRenderAnimationData>(this);
  AnimationInfo& animationInfo = animationData->GetAnimationInfo();
  animationInfo.EnsureAnimationsId();
  mWrAnimationId = animationInfo.GetCompositorAnimationsId();


  wr::WrAnimationProperty prop;
  prop.id = mWrAnimationId;
  prop.effect_type = wr::WrAnimationType::Transform;

  StackingContextHelper sc(aSc, aBuilder, nsTArray<wr::WrFilterOp>(),
                           LayoutDeviceRect(), nullptr, &prop);

  nsDisplayWrapList::CreateWebRenderCommands(aBuilder, aResources, sc,
                                             aManager, aDisplayListBuilder);
  return true;
}

bool
nsDisplayOwnLayer::UpdateScrollData(mozilla::layers::WebRenderScrollData* aData,
                                    mozilla::layers::WebRenderLayerScrollData* aLayerData)
{
  bool ret = false;

  if (IsScrollThumbLayer() || IsScrollbarContainer()) {
    ret = true;
    if (aLayerData) {
      aLayerData->SetScrollbarData(mScrollbarData);
      if (IsScrollThumbLayer()) {
        aLayerData->SetScrollbarAnimationId(mWrAnimationId);
      }
    }
  }
  return ret;
}

void
nsDisplayOwnLayer::WriteDebugInfo(std::stringstream& aStream)
{
  aStream << nsPrintfCString(" (flags 0x%x) (scrolltarget %" PRIu64 ")", (int)mFlags, mScrollbarData.mTargetViewId).get();
}

nsDisplaySubDocument::nsDisplaySubDocument(nsDisplayListBuilder* aBuilder,
                                           nsIFrame* aFrame,
                                           nsSubDocumentFrame* aSubDocFrame,
                                           nsDisplayList* aList,
                                           nsDisplayOwnLayerFlags aFlags)
    : nsDisplayOwnLayer(aBuilder, aFrame, aList, aBuilder->CurrentActiveScrolledRoot(), aFlags)
    , mScrollParentId(aBuilder->GetCurrentScrollParentId())
    , mShouldFlatten(false)
    , mSubDocFrame(aSubDocFrame)
{
  MOZ_COUNT_CTOR(nsDisplaySubDocument);

  // The SubDocument display item is conceptually outside the viewport frame,
  // so in cases where the viewport frame is an AGR, the SubDocument's AGR
  // should be not the viewport frame itself, but its parent AGR.
  if (*mAnimatedGeometryRoot == mFrame && mAnimatedGeometryRoot->mParentAGR) {
    mAnimatedGeometryRoot = mAnimatedGeometryRoot->mParentAGR;
  }

  if (mSubDocFrame && mSubDocFrame != mFrame) {
    mSubDocFrame->AddDisplayItem(this);
  }
}

nsDisplaySubDocument::~nsDisplaySubDocument() {
  MOZ_COUNT_DTOR(nsDisplaySubDocument);
  if (mSubDocFrame) {
    mSubDocFrame->RemoveDisplayItem(this);
  }
}

nsIFrame*
nsDisplaySubDocument::FrameForInvalidation() const
{
  return mSubDocFrame ? mSubDocFrame : mFrame;
}

bool
nsDisplaySubDocument::HasDeletedFrame() const
{
  return !mSubDocFrame || nsDisplayItem::HasDeletedFrame();
}

void
nsDisplaySubDocument::RemoveFrame(nsIFrame* aFrame)
{
  if (aFrame == mSubDocFrame) {
    mSubDocFrame = nullptr;
  }
  nsDisplayItem::RemoveFrame(aFrame);
}

void
nsDisplaySubDocument::Disown()
{
  if (mFrame) {
    mFrame->RemoveDisplayItem(this);
    mFrame = nullptr;
  }
  if (mSubDocFrame) {
    mSubDocFrame->RemoveDisplayItem(this);
    mSubDocFrame = nullptr;
  }
}

UniquePtr<ScrollMetadata>
nsDisplaySubDocument::ComputeScrollMetadata(LayerManager* aLayerManager,
                                            const ContainerLayerParameters& aContainerParameters)
{
  if (!(mFlags & nsDisplayOwnLayerFlags::eGenerateScrollableLayer)) {
    return UniquePtr<ScrollMetadata>(nullptr);
  }

  nsPresContext* presContext = mFrame->PresContext();
  nsIFrame* rootScrollFrame = presContext->PresShell()->GetRootScrollFrame();
  bool isRootContentDocument = presContext->IsRootContentDocument();
  nsIPresShell* presShell = presContext->PresShell();
  ContainerLayerParameters params(
      aContainerParameters.mXScale * presShell->GetResolution(),
      aContainerParameters.mYScale * presShell->GetResolution(),
      nsIntPoint(), aContainerParameters);

  nsRect viewport = mFrame->GetRect() -
                    mFrame->GetPosition() +
                    mFrame->GetOffsetToCrossDoc(ReferenceFrame());

  return MakeUnique<ScrollMetadata>(
    nsLayoutUtils::ComputeScrollMetadata(
      mFrame, rootScrollFrame, rootScrollFrame->GetContent(), ReferenceFrame(),
      aLayerManager, mScrollParentId, viewport, Nothing(),
      isRootContentDocument, params));
}

static bool
UseDisplayPortForViewport(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame)
{
  return aBuilder->IsPaintingToWindow() &&
      nsLayoutUtils::ViewportHasDisplayPort(aFrame->PresContext());
}

nsRect
nsDisplaySubDocument::GetBounds(nsDisplayListBuilder* aBuilder,
                                bool* aSnap) const
{
  bool usingDisplayPort = UseDisplayPortForViewport(aBuilder, mFrame);

  if ((mFlags & nsDisplayOwnLayerFlags::eGenerateScrollableLayer) && usingDisplayPort) {
    *aSnap = false;
    return mFrame->GetRect() + aBuilder->ToReferenceFrame(mFrame);
  }

  return nsDisplayOwnLayer::GetBounds(aBuilder, aSnap);
}

bool
nsDisplaySubDocument::ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                        nsRegion* aVisibleRegion)
{
  bool usingDisplayPort = UseDisplayPortForViewport(aBuilder, mFrame);

  if (!(mFlags & nsDisplayOwnLayerFlags::eGenerateScrollableLayer) || !usingDisplayPort) {
    return nsDisplayWrapList::ComputeVisibility(aBuilder, aVisibleRegion);
  }

  nsRect displayport;
  nsIFrame* rootScrollFrame = mFrame->PresShell()->GetRootScrollFrame();
  MOZ_ASSERT(rootScrollFrame);
  Unused << nsLayoutUtils::GetDisplayPort(rootScrollFrame->GetContent(), &displayport,
    RelativeTo::ScrollFrame);

  nsRegion childVisibleRegion;
  // The visible region for the children may be much bigger than the hole we
  // are viewing the children from, so that the compositor process has enough
  // content to asynchronously pan while content is being refreshed.
  childVisibleRegion = displayport + mFrame->GetOffsetToCrossDoc(ReferenceFrame());

  nsRect boundedRect =
    childVisibleRegion.GetBounds().Intersect(
      mList.GetClippedBoundsWithRespectToASR(aBuilder, mActiveScrolledRoot));
  bool visible = mList.ComputeVisibilityForSublist(
    aBuilder, &childVisibleRegion, boundedRect);

  // If APZ is enabled then don't allow this computation to influence
  // aVisibleRegion, on the assumption that the layer can be asynchronously
  // scrolled so we'll definitely need all the content under it.
  if (!nsLayoutUtils::UsesAsyncScrolling(mFrame)) {
    bool snap;
    nsRect bounds = GetBounds(aBuilder, &snap);
    nsRegion removed;
    removed.Sub(bounds, childVisibleRegion);

    aBuilder->SubtractFromVisibleRegion(aVisibleRegion, removed);
  }

  return visible;
}

bool
nsDisplaySubDocument::ShouldBuildLayerEvenIfInvisible(nsDisplayListBuilder* aBuilder) const
{
  bool usingDisplayPort = UseDisplayPortForViewport(aBuilder, mFrame);

  if ((mFlags & nsDisplayOwnLayerFlags::eGenerateScrollableLayer) && usingDisplayPort) {
    return true;
  }

  return nsDisplayOwnLayer::ShouldBuildLayerEvenIfInvisible(aBuilder);
}

nsRegion
nsDisplaySubDocument::GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                                      bool* aSnap) const
{
  bool usingDisplayPort = UseDisplayPortForViewport(aBuilder, mFrame);

  if ((mFlags & nsDisplayOwnLayerFlags::eGenerateScrollableLayer) && usingDisplayPort) {
    *aSnap = false;
    return nsRegion();
  }

  return nsDisplayOwnLayer::GetOpaqueRegion(aBuilder, aSnap);
}

nsDisplayResolution::nsDisplayResolution(nsDisplayListBuilder* aBuilder,
                                         nsIFrame* aFrame, nsDisplayList* aList,
                                         nsDisplayOwnLayerFlags aFlags)
    : nsDisplaySubDocument(aBuilder, aFrame, nullptr, aList, aFlags) {
  MOZ_COUNT_CTOR(nsDisplayResolution);
}

#ifdef NS_BUILD_REFCNT_LOGGING
nsDisplayResolution::~nsDisplayResolution() {
  MOZ_COUNT_DTOR(nsDisplayResolution);
}
#endif

void
nsDisplayResolution::HitTest(nsDisplayListBuilder* aBuilder,
                             const nsRect& aRect,
                             HitTestState* aState,
                             nsTArray<nsIFrame*> *aOutFrames)
{
  nsIPresShell* presShell = mFrame->PresShell();
  nsRect rect = aRect.RemoveResolution(presShell->ScaleToResolution() ? presShell->GetResolution () : 1.0f);
  mList.HitTest(aBuilder, rect, aState, aOutFrames);
}

already_AddRefed<Layer>
nsDisplayResolution::BuildLayer(nsDisplayListBuilder* aBuilder,
                                LayerManager* aManager,
                                const ContainerLayerParameters& aContainerParameters) {
  nsIPresShell* presShell = mFrame->PresShell();
  ContainerLayerParameters containerParameters(
    presShell->GetResolution(), presShell->GetResolution(), nsIntPoint(),
    aContainerParameters);

  RefPtr<Layer> layer = nsDisplaySubDocument::BuildLayer(
    aBuilder, aManager, containerParameters);
  layer->SetPostScale(1.0f / presShell->GetResolution(),
                      1.0f / presShell->GetResolution());
  layer->AsContainerLayer()->SetScaleToResolution(
      presShell->ScaleToResolution(), presShell->GetResolution());
  return layer.forget();
}

nsDisplayFixedPosition::nsDisplayFixedPosition(nsDisplayListBuilder* aBuilder,
                                               nsIFrame* aFrame,
                                               nsDisplayList* aList,
                                               const ActiveScrolledRoot* aActiveScrolledRoot,
                                               const ActiveScrolledRoot* aContainerASR)
  : nsDisplayOwnLayer(aBuilder, aFrame, aList, aActiveScrolledRoot)
  , mIndex(0)
  , mIsFixedBackground(false)
  , mContainerASR(aContainerASR)
{
  MOZ_COUNT_CTOR(nsDisplayFixedPosition);
  Init(aBuilder);
}

nsDisplayFixedPosition::nsDisplayFixedPosition(nsDisplayListBuilder* aBuilder,
                                               nsIFrame* aFrame,
                                               nsDisplayList* aList,
                                               uint32_t aIndex)
  : nsDisplayOwnLayer(aBuilder, aFrame, aList, aBuilder->CurrentActiveScrolledRoot())
  , mIndex(aIndex)
  , mIsFixedBackground(true)
  , mContainerASR(nullptr) // XXX maybe this should be something?
{
  MOZ_COUNT_CTOR(nsDisplayFixedPosition);
  Init(aBuilder);
}

void
nsDisplayFixedPosition::Init(nsDisplayListBuilder* aBuilder)
{
  mAnimatedGeometryRootForScrollMetadata = mAnimatedGeometryRoot;
  if (ShouldFixToViewport(aBuilder)) {
    mAnimatedGeometryRoot = aBuilder->FindAnimatedGeometryRootFor(this);
  }
}

/* static */ nsDisplayFixedPosition*
nsDisplayFixedPosition::CreateForFixedBackground(nsDisplayListBuilder* aBuilder,
                                                 nsIFrame* aFrame,
                                                 nsDisplayBackgroundImage* aImage,
                                                 uint32_t aIndex)
{
  nsDisplayList temp;
  temp.AppendToTop(aImage);

  return MakeDisplayItem<nsDisplayFixedPosition>(aBuilder, aFrame, &temp, aIndex + 1);
}


#ifdef NS_BUILD_REFCNT_LOGGING
nsDisplayFixedPosition::~nsDisplayFixedPosition() {
  MOZ_COUNT_DTOR(nsDisplayFixedPosition);
}
#endif

already_AddRefed<Layer>
nsDisplayFixedPosition::BuildLayer(nsDisplayListBuilder* aBuilder,
                                   LayerManager* aManager,
                                   const ContainerLayerParameters& aContainerParameters) {
  RefPtr<Layer> layer =
    nsDisplayOwnLayer::BuildLayer(aBuilder, aManager, aContainerParameters);

  layer->SetIsFixedPosition(true);

  nsPresContext* presContext = mFrame->PresContext();
  nsIFrame* fixedFrame = mIsFixedBackground ? presContext->PresShell()->GetRootFrame() : mFrame;

  const nsIFrame* viewportFrame = fixedFrame->GetParent();
  // anchorRect will be in the container's coordinate system (aLayer's parent layer).
  // This is the same as the display items' reference frame.
  nsRect anchorRect;
  if (viewportFrame) {
    // Fixed position frames are reflowed into the scroll-port size if one has
    // been set.
    if (presContext->PresShell()->IsScrollPositionClampingScrollPortSizeSet()) {
      anchorRect.SizeTo(presContext->PresShell()->GetScrollPositionClampingScrollPortSize());
    } else {
      anchorRect.SizeTo(viewportFrame->GetSize());
    }
  } else {
    // A display item directly attached to the viewport.
    // For background-attachment:fixed items, the anchor point is always the
    // top-left of the viewport currently.
    viewportFrame = fixedFrame;
  }
  // The anchorRect top-left is always the viewport top-left.
  anchorRect.MoveTo(viewportFrame->GetOffsetToCrossDoc(ReferenceFrame()));

  nsLayoutUtils::SetFixedPositionLayerData(layer,
      viewportFrame, anchorRect, fixedFrame, presContext, aContainerParameters);

  return layer.forget();
}

ViewID
nsDisplayFixedPosition::GetScrollTargetId()
{
  if (mContainerASR && !nsLayoutUtils::IsReallyFixedPos(mFrame)) {
    return mContainerASR->GetViewId();
  }
  return nsLayoutUtils::ScrollIdForRootScrollFrame(mFrame->PresContext());
}

bool
nsDisplayFixedPosition::CreateWebRenderCommands(mozilla::wr::DisplayListBuilder& aBuilder,
                                                mozilla::wr::IpcResourceUpdateQueue& aResources,
                                                const StackingContextHelper& aSc,
                                                mozilla::layers::WebRenderLayerManager* aManager,
                                                nsDisplayListBuilder* aDisplayListBuilder)
{
  // We install this RAII scrolltarget tracker so that any
  // nsDisplayCompositorHitTestInfo items inside this fixed-pos item (and that
  // share the same ASR as this item) use the correct scroll target. That way
  // attempts to scroll on those items will scroll the root scroll frame.
  mozilla::wr::DisplayListBuilder::FixedPosScrollTargetTracker tracker(
      aBuilder,
      GetActiveScrolledRoot(),
      GetScrollTargetId());
  return nsDisplayOwnLayer::CreateWebRenderCommands(aBuilder, aResources, aSc,
      aManager, aDisplayListBuilder);
}

bool
nsDisplayFixedPosition::UpdateScrollData(mozilla::layers::WebRenderScrollData* aData,
                                         mozilla::layers::WebRenderLayerScrollData* aLayerData)
{
  if (aLayerData) {
    aLayerData->SetFixedPositionScrollContainerId(GetScrollTargetId());
  }
  return nsDisplayOwnLayer::UpdateScrollData(aData, aLayerData) | true;
}

void
nsDisplayFixedPosition::WriteDebugInfo(std::stringstream& aStream)
{
  aStream << nsPrintfCString(" (containerASR %s) (scrolltarget %" PRIu64 ")",
      ActiveScrolledRoot::ToString(mContainerASR).get(),
      GetScrollTargetId()).get();
}

TableType
GetTableTypeFromFrame(nsIFrame* aFrame)
{
  if (aFrame->IsTableFrame()) {
    return TableType::TABLE;
  }

  if (aFrame->IsTableColFrame()) {
    return TableType::TABLE_COL;
  }

  if (aFrame->IsTableColGroupFrame()) {
    return TableType::TABLE_COL_GROUP;
  }

  if (aFrame->IsTableRowFrame()) {
    return TableType::TABLE_ROW;
  }

  if (aFrame->IsTableRowGroupFrame()) {
    return TableType::TABLE_ROW_GROUP;
  }

  if (aFrame->IsTableCellFrame()) {
    return TableType::TABLE_CELL;
  }

  MOZ_ASSERT_UNREACHABLE("Invalid frame.");
  return TableType::TABLE;
}

nsDisplayTableFixedPosition::nsDisplayTableFixedPosition(nsDisplayListBuilder* aBuilder,
                                                         nsIFrame* aFrame,
                                                         nsDisplayList* aList,
                                                         uint32_t aIndex,
                                                         nsIFrame* aAncestorFrame)
  : nsDisplayFixedPosition(aBuilder, aFrame, aList, aIndex)
  , mAncestorFrame(aAncestorFrame)
  , mTableType(GetTableTypeFromFrame(aAncestorFrame))
{
  if (aBuilder->IsRetainingDisplayList()) {
    mAncestorFrame->AddDisplayItem(this);
  }
}

/* static */ nsDisplayTableFixedPosition*
nsDisplayTableFixedPosition::CreateForFixedBackground(nsDisplayListBuilder* aBuilder,
                                                      nsIFrame* aFrame,
                                                      nsDisplayBackgroundImage* aImage,
                                                      uint32_t aIndex,
                                                      nsIFrame* aAncestorFrame)
{
  nsDisplayList temp;
  temp.AppendToTop(aImage);

  return MakeDisplayItem<nsDisplayTableFixedPosition>(aBuilder, aFrame, &temp, aIndex + 1, aAncestorFrame);
}

nsDisplayStickyPosition::nsDisplayStickyPosition(nsDisplayListBuilder* aBuilder,
                                                 nsIFrame* aFrame,
                                                 nsDisplayList* aList,
                                                 const ActiveScrolledRoot* aActiveScrolledRoot,
                                                 const ActiveScrolledRoot* aContainerASR)
  : nsDisplayOwnLayer(aBuilder, aFrame, aList, aActiveScrolledRoot)
  , mContainerASR(aContainerASR)
{
  MOZ_COUNT_CTOR(nsDisplayStickyPosition);
}

void
nsDisplayStickyPosition::SetClipChain(const DisplayItemClipChain* aClipChain,
                                      bool aStore)
{
  mClipChain = aClipChain;
  mClip = nullptr;

  MOZ_ASSERT(!mClip, "There should never be a clip on this item because no clip moves with it.");

  if (aStore) {
    mState.mClipChain = aClipChain;
    mState.mClip = mClip;
  }
}

#ifdef NS_BUILD_REFCNT_LOGGING
nsDisplayStickyPosition::~nsDisplayStickyPosition() {
  MOZ_COUNT_DTOR(nsDisplayStickyPosition);
}
#endif

already_AddRefed<Layer>
nsDisplayStickyPosition::BuildLayer(nsDisplayListBuilder* aBuilder,
                                    LayerManager* aManager,
                                    const ContainerLayerParameters& aContainerParameters) {
  RefPtr<Layer> layer =
    nsDisplayOwnLayer::BuildLayer(aBuilder, aManager, aContainerParameters);

  StickyScrollContainer* stickyScrollContainer = StickyScrollContainer::
    GetStickyScrollContainerForFrame(mFrame);
  if (!stickyScrollContainer) {
    return layer.forget();
  }

  nsIFrame* scrollFrame = do_QueryFrame(stickyScrollContainer->ScrollFrame());
  nsPresContext* presContext = scrollFrame->PresContext();

  // Sticky position frames whose scroll frame is the root scroll frame are
  // reflowed into the scroll-port size if one has been set.
  nsSize scrollFrameSize = scrollFrame->GetSize();
  if (scrollFrame == presContext->PresShell()->GetRootScrollFrame() &&
      presContext->PresShell()->IsScrollPositionClampingScrollPortSizeSet()) {
    scrollFrameSize = presContext->PresShell()->
      GetScrollPositionClampingScrollPortSize();
  }

  nsLayoutUtils::SetFixedPositionLayerData(layer, scrollFrame,
    nsRect(scrollFrame->GetOffsetToCrossDoc(ReferenceFrame()), scrollFrameSize),
    mFrame, presContext, aContainerParameters);

  ViewID scrollId = nsLayoutUtils::FindOrCreateIDFor(
    stickyScrollContainer->ScrollFrame()->GetScrolledFrame()->GetContent());

  float factor = presContext->AppUnitsPerDevPixel();
  nsRectAbsolute outer;
  nsRectAbsolute inner;
  stickyScrollContainer->GetScrollRanges(mFrame, &outer, &inner);
  LayerRectAbsolute stickyOuter(NSAppUnitsToFloatPixels(outer.X(), factor) *
                          aContainerParameters.mXScale,
                        NSAppUnitsToFloatPixels(outer.Y(), factor) *
                          aContainerParameters.mYScale,
                        NSAppUnitsToFloatPixels(outer.XMost(), factor) *
                          aContainerParameters.mXScale,
                        NSAppUnitsToFloatPixels(outer.YMost(), factor) *
                          aContainerParameters.mYScale);
  LayerRectAbsolute stickyInner(NSAppUnitsToFloatPixels(inner.X(), factor) *
                          aContainerParameters.mXScale,
                        NSAppUnitsToFloatPixels(inner.Y(), factor) *
                          aContainerParameters.mYScale,
                        NSAppUnitsToFloatPixels(inner.XMost(), factor) *
                          aContainerParameters.mXScale,
                        NSAppUnitsToFloatPixels(inner.YMost(), factor) *
                          aContainerParameters.mYScale);
  layer->SetStickyPositionData(scrollId, stickyOuter, stickyInner);

  return layer.forget();
}

// Returns the smallest distance from "0" to the range [min, max] where
// min <= max.
static nscoord DistanceToRange(nscoord min, nscoord max) {
  MOZ_ASSERT(min <= max);
  if (max < 0) {
    return max;
  }
  if (min > 0) {
    return min;
  }
  MOZ_ASSERT(min <= 0 && max >= 0);
  return 0;
}

bool
nsDisplayStickyPosition::CreateWebRenderCommands(mozilla::wr::DisplayListBuilder& aBuilder,
                                                 mozilla::wr::IpcResourceUpdateQueue& aResources,
                                                 const StackingContextHelper& aSc,
                                                 WebRenderLayerManager* aManager,
                                                 nsDisplayListBuilder* aDisplayListBuilder)
{
  StickyScrollContainer* stickyScrollContainer = StickyScrollContainer::GetStickyScrollContainerForFrame(mFrame);
  if (stickyScrollContainer) {
    // If there's no ASR for the scrollframe that this sticky item is attached
    // to, then don't create a WR sticky item for it either. Trying to do so
    // will end in sadness because WR will interpret some coordinates as
    // relative to the nearest enclosing scrollframe, which will correspond
    // to the nearest ancestor ASR on the gecko side. That ASR will not be the
    // same as the scrollframe this sticky item is actually supposed to be
    // attached to, thus the sadness.
    // Not sending WR the sticky item is ok, because the enclosing scrollframe
    // will never be asynchronously scrolled. Instead we will always position
    // the sticky items correctly on the gecko side and WR will never need to
    // adjust their position itself.
    if (!stickyScrollContainer->ScrollFrame()->IsMaybeAsynchronouslyScrolled()) {
      stickyScrollContainer = nullptr;
    }
  }

  if (stickyScrollContainer) {
    float auPerDevPixel = mFrame->PresContext()->AppUnitsPerDevPixel();

    bool snap;
    nsRect itemBounds = GetBounds(aDisplayListBuilder, &snap);

    Maybe<float> topMargin;
    Maybe<float> rightMargin;
    Maybe<float> bottomMargin;
    Maybe<float> leftMargin;
    wr::StickyOffsetBounds vBounds = { 0.0, 0.0 };
    wr::StickyOffsetBounds hBounds = { 0.0, 0.0 };
    nsPoint appliedOffset;

    nsRectAbsolute outer;
    nsRectAbsolute inner;
    stickyScrollContainer->GetScrollRanges(mFrame, &outer, &inner);

    nsIFrame* scrollFrame = do_QueryFrame(stickyScrollContainer->ScrollFrame());
    nsPoint offset = scrollFrame->GetOffsetToCrossDoc(ReferenceFrame());

    // Adjust the scrollPort coordinates to be relative to the reference frame,
    // so that it is in the same space as everything else.
    nsRect scrollPort = stickyScrollContainer->ScrollFrame()->GetScrollPortRect();
    scrollPort += offset;

    // The following computations make more sense upon understanding the
    // semantics of "inner" and "outer", which is explained in the comment on
    // SetStickyPositionData in Layers.h.

    if (outer.YMost() != inner.YMost()) {
      // Question: How far will itemBounds.y be from the top of the scrollport
      // when we have scrolled from the current scroll position of "0" to
      // reach the range [inner.YMost(), outer.YMost()] where the item gets
      // stuck?
      // Answer: the current distance is "itemBounds.y - scrollPort.y". That
      // needs to be adjusted by the distance to the range. If the distance is
      // negative (i.e. inner.YMost() <= outer.YMost() < 0) then we would be
      // scrolling upwards (decreasing scroll offset) to reach that range,
      // which would increase itemBounds.y and make it farther away from the
      // top of the scrollport. So in that case the adjustment is -distance.
      // If the distance is positive (0 < inner.YMost() <= outer.YMost()) then
      // we would be scrolling downwards, itemBounds.y would decrease, and we
      // again need to adjust by -distance. If we are already in the range
      // then no adjustment is needed and distance is 0 so again using
      // -distance works.
      nscoord distance = DistanceToRange(inner.YMost(), outer.YMost());
      topMargin = Some(NSAppUnitsToFloatPixels(itemBounds.y - scrollPort.y - distance, auPerDevPixel));
      // Question: What is the maximum positive ("downward") offset that WR
      // will have to apply to this item in order to prevent the item from
      // visually moving?
      // Answer: Since the item is "sticky" in the range [inner.YMost(), outer.YMost()],
      // the maximum offset will be the size of the range, which is
      // outer.YMost() - inner.YMost().
      vBounds.max = NSAppUnitsToFloatPixels(outer.YMost() - inner.YMost(), auPerDevPixel);
      // Question: how much of an offset has layout already applied to the item?
      // Answer: if we are
      // (a) inside the sticky range (inner.YMost() < 0 <= outer.YMost()), or
      // (b) past the sticky range (inner.YMost() < outer.YMost() < 0)
      // then layout has already applied some offset to the position of the
      // item. The amount of the adjustment is |0 - inner.YMost()| in case (a)
      // and |outer.YMost() - inner.YMost()| in case (b).
      if (inner.YMost() < 0) {
        appliedOffset.y = std::min(0, outer.YMost()) - inner.YMost();
        MOZ_ASSERT(appliedOffset.y > 0);
      }
    }
    if (outer.Y() != inner.Y()) {
      // Similar logic as in the previous section, but this time we care about
      // the distance from itemBounds.YMost() to scrollPort.YMost().
      nscoord distance = DistanceToRange(outer.Y(), inner.Y());
      bottomMargin = Some(NSAppUnitsToFloatPixels(scrollPort.YMost() - itemBounds.YMost() + distance, auPerDevPixel));
      // And here WR will be moving the item upwards rather than downwards so
      // again things are inverted from the previous block.
      vBounds.min = NSAppUnitsToFloatPixels(outer.Y() - inner.Y(), auPerDevPixel);
      // We can't have appliedOffset be both positive and negative, and the top
      // adjustment takes priority. So here we only update appliedOffset.y if
      // it wasn't set by the top-sticky case above.
      if (appliedOffset.y == 0 && inner.Y() > 0) {
        appliedOffset.y = std::max(0, outer.Y()) - inner.Y();
        MOZ_ASSERT(appliedOffset.y < 0);
      }
    }
    // Same as above, but for the x-axis
    if (outer.XMost() != inner.XMost()) {
      nscoord distance = DistanceToRange(inner.XMost(), outer.XMost());
      leftMargin = Some(NSAppUnitsToFloatPixels(itemBounds.x - scrollPort.x - distance, auPerDevPixel));
      hBounds.max = NSAppUnitsToFloatPixels(outer.XMost() - inner.XMost(), auPerDevPixel);
      if (inner.XMost() < 0) {
        appliedOffset.x = std::min(0, outer.XMost()) - inner.XMost();
        MOZ_ASSERT(appliedOffset.x > 0);
      }
    }
    if (outer.X() != inner.X()) {
      nscoord distance = DistanceToRange(outer.X(), inner.X());
      rightMargin = Some(NSAppUnitsToFloatPixels(scrollPort.XMost() - itemBounds.XMost() + distance, auPerDevPixel));
      hBounds.min = NSAppUnitsToFloatPixels(outer.X() - inner.X(), auPerDevPixel);
      if (appliedOffset.x == 0 && inner.X() > 0) {
        appliedOffset.x = std::max(0, outer.X()) - inner.X();
        MOZ_ASSERT(appliedOffset.x < 0);
      }
    }

    LayoutDeviceRect bounds = LayoutDeviceRect::FromAppUnits(itemBounds, auPerDevPixel);
    wr::LayoutVector2D applied = {
      NSAppUnitsToFloatPixels(appliedOffset.x, auPerDevPixel),
      NSAppUnitsToFloatPixels(appliedOffset.y, auPerDevPixel)
    };
    wr::WrClipId id = aBuilder.DefineStickyFrame(wr::ToRoundedLayoutRect(bounds),
        topMargin.ptrOr(nullptr), rightMargin.ptrOr(nullptr),
        bottomMargin.ptrOr(nullptr), leftMargin.ptrOr(nullptr),
        vBounds, hBounds, applied);

    aBuilder.PushClip(id);
    aManager->CommandBuilder().PushOverrideForASR(mContainerASR, Some(id));
  }

  nsDisplayOwnLayer::CreateWebRenderCommands(aBuilder, aResources, aSc,
      aManager, aDisplayListBuilder);

  if (stickyScrollContainer) {
    aManager->CommandBuilder().PopOverrideForASR(mContainerASR);
    aBuilder.PopClip();
  }

  return true;
}

nsDisplayScrollInfoLayer::nsDisplayScrollInfoLayer(
  nsDisplayListBuilder* aBuilder,
  nsIFrame* aScrolledFrame,
  nsIFrame* aScrollFrame)
  : nsDisplayWrapList(aBuilder, aScrollFrame)
  , mScrollFrame(aScrollFrame)
  , mScrolledFrame(aScrolledFrame)
  , mScrollParentId(aBuilder->GetCurrentScrollParentId())
{
#ifdef NS_BUILD_REFCNT_LOGGING
  MOZ_COUNT_CTOR(nsDisplayScrollInfoLayer);
#endif
}

#ifdef NS_BUILD_REFCNT_LOGGING
nsDisplayScrollInfoLayer::~nsDisplayScrollInfoLayer()
{
  MOZ_COUNT_DTOR(nsDisplayScrollInfoLayer);
}
#endif

already_AddRefed<Layer>
nsDisplayScrollInfoLayer::BuildLayer(nsDisplayListBuilder* aBuilder,
                                     LayerManager* aManager,
                                     const ContainerLayerParameters& aContainerParameters)
{
  // In general for APZ with event-regions we no longer have a need for
  // scrollinfo layers. However, in some cases, there might be content that
  // cannot be layerized, and so needs to scroll synchronously. To handle those
  // cases, we still want to generate scrollinfo layers.

  return aManager->GetLayerBuilder()->
    BuildContainerLayerFor(aBuilder, aManager, mFrame, this, &mList,
                           aContainerParameters, nullptr,
                           FrameLayerBuilder::CONTAINER_ALLOW_PULL_BACKGROUND_COLOR);
}

LayerState
nsDisplayScrollInfoLayer::GetLayerState(nsDisplayListBuilder* aBuilder,
                                    LayerManager* aManager,
                                    const ContainerLayerParameters& aParameters)
{
  return LAYER_ACTIVE_EMPTY;
}

UniquePtr<ScrollMetadata>
nsDisplayScrollInfoLayer::ComputeScrollMetadata(LayerManager* aLayerManager,
                                                const ContainerLayerParameters& aContainerParameters)
{
  nsRect viewport = mScrollFrame->GetRect() -
                    mScrollFrame->GetPosition() +
                    mScrollFrame->GetOffsetToCrossDoc(ReferenceFrame());

  ScrollMetadata metadata = nsLayoutUtils::ComputeScrollMetadata(
      mScrolledFrame, mScrollFrame, mScrollFrame->GetContent(),
      ReferenceFrame(), aLayerManager,
      mScrollParentId, viewport, Nothing(), false, aContainerParameters);
  metadata.GetMetrics().SetIsScrollInfoLayer(true);

  return UniquePtr<ScrollMetadata>(new ScrollMetadata(metadata));
}

bool
nsDisplayScrollInfoLayer::UpdateScrollData(mozilla::layers::WebRenderScrollData* aData,
                                           mozilla::layers::WebRenderLayerScrollData* aLayerData)
{
  if (aLayerData) {
    UniquePtr<ScrollMetadata> metadata =
      ComputeScrollMetadata(aData->GetManager(), ContainerLayerParameters());
    MOZ_ASSERT(aData);
    MOZ_ASSERT(metadata);
    aLayerData->AppendScrollMetadata(*aData, *metadata);
  }
  return true;
}

void
nsDisplayScrollInfoLayer::WriteDebugInfo(std::stringstream& aStream)
{
  aStream << " (scrollframe " << mScrollFrame
          << " scrolledFrame " << mScrolledFrame << ")";
}

nsDisplayZoom::nsDisplayZoom(nsDisplayListBuilder* aBuilder,
                             nsIFrame* aFrame, nsDisplayList* aList,
                             int32_t aAPD, int32_t aParentAPD,
                             nsDisplayOwnLayerFlags aFlags)
    : nsDisplaySubDocument(aBuilder, aFrame, nullptr, aList, aFlags)
    , mAPD(aAPD), mParentAPD(aParentAPD) {
  MOZ_COUNT_CTOR(nsDisplayZoom);
}

#ifdef NS_BUILD_REFCNT_LOGGING
nsDisplayZoom::~nsDisplayZoom() {
  MOZ_COUNT_DTOR(nsDisplayZoom);
}
#endif

nsRect nsDisplayZoom::GetBounds(nsDisplayListBuilder* aBuilder,
                                bool* aSnap) const
{
  nsRect bounds = nsDisplaySubDocument::GetBounds(aBuilder, aSnap);
  *aSnap = false;
  return bounds.ScaleToOtherAppUnitsRoundOut(mAPD, mParentAPD);
}

void nsDisplayZoom::HitTest(nsDisplayListBuilder *aBuilder,
                            const nsRect& aRect,
                            HitTestState *aState,
                            nsTArray<nsIFrame*> *aOutFrames)
{
  nsRect rect;
  // A 1x1 rect indicates we are just hit testing a point, so pass down a 1x1
  // rect as well instead of possibly rounding the width or height to zero.
  if (aRect.width == 1 && aRect.height == 1) {
    rect.MoveTo(aRect.TopLeft().ScaleToOtherAppUnits(mParentAPD, mAPD));
    rect.width = rect.height = 1;
  } else {
    rect = aRect.ScaleToOtherAppUnitsRoundOut(mParentAPD, mAPD);
  }
  mList.HitTest(aBuilder, rect, aState, aOutFrames);
}

bool nsDisplayZoom::ComputeVisibility(nsDisplayListBuilder *aBuilder,
                                      nsRegion *aVisibleRegion)
{
  // Convert the passed in visible region to our appunits.
  nsRegion visibleRegion;
  // mVisibleRect has been clipped to GetClippedBounds
  visibleRegion.And(*aVisibleRegion, GetPaintRect());
  visibleRegion = visibleRegion.ScaleToOtherAppUnitsRoundOut(mParentAPD, mAPD);
  nsRegion originalVisibleRegion = visibleRegion;

  nsRect transformedVisibleRect =
    GetPaintRect().ScaleToOtherAppUnitsRoundOut(mParentAPD, mAPD);
  bool retval;
  // If we are to generate a scrollable layer we call
  // nsDisplaySubDocument::ComputeVisibility to make the necessary adjustments
  // for ComputeVisibility, it does all it's calculations in the child APD.
  bool usingDisplayPort = UseDisplayPortForViewport(aBuilder, mFrame);
  if (!(mFlags & nsDisplayOwnLayerFlags::eGenerateScrollableLayer) || !usingDisplayPort) {
    retval =
      mList.ComputeVisibilityForSublist(aBuilder, &visibleRegion,
                                        transformedVisibleRect);
  } else {
    retval =
      nsDisplaySubDocument::ComputeVisibility(aBuilder, &visibleRegion);
  }

  nsRegion removed;
  // removed = originalVisibleRegion - visibleRegion
  removed.Sub(originalVisibleRegion, visibleRegion);
  // Convert removed region to parent appunits.
  removed = removed.ScaleToOtherAppUnitsRoundIn(mAPD, mParentAPD);
  // aVisibleRegion = aVisibleRegion - removed (modulo any simplifications
  // SubtractFromVisibleRegion does)
  aBuilder->SubtractFromVisibleRegion(aVisibleRegion, removed);

  return retval;
}

///////////////////////////////////////////////////
// nsDisplayTransform Implementation
//

// Write #define UNIFIED_CONTINUATIONS here and in
// TransformReferenceBox::Initialize to have the transform property try
// to transform content with continuations as one unified block instead of
// several smaller ones.  This is currently disabled because it doesn't work
// correctly, since when the frames are initially being reflowed, their
// continuations all compute their bounding rects independently of each other
// and consequently get the wrong value.  Write #define DEBUG_HIT here to have
// the nsDisplayTransform class dump out a bunch of information about hit
// detection.
#undef  UNIFIED_CONTINUATIONS
#undef  DEBUG_HIT

nsDisplayTransform::nsDisplayTransform(nsDisplayListBuilder* aBuilder,
                                       nsIFrame *aFrame, nsDisplayList *aList,
                                       const nsRect& aChildrenBuildingRect,
                                       ComputeTransformFunction aTransformGetter,
                                       uint32_t aIndex)
  : nsDisplayItem(aBuilder, aFrame)
  , mStoredList(aBuilder, aFrame, aList)
  , mTransformGetter(aTransformGetter)
  , mAnimatedGeometryRootForChildren(mAnimatedGeometryRoot)
  , mAnimatedGeometryRootForScrollMetadata(mAnimatedGeometryRoot)
  , mChildrenBuildingRect(aChildrenBuildingRect)
  , mIndex(aIndex)
  , mNoExtendContext(false)
  , mIsTransformSeparator(false)
  , mTransformPreserves3DInited(false)
  , mAllowAsyncAnimation(false)
{
  MOZ_COUNT_CTOR(nsDisplayTransform);
  MOZ_ASSERT(aFrame, "Must have a frame!");
  Init(aBuilder);
}

void
nsDisplayTransform::SetReferenceFrameToAncestor(nsDisplayListBuilder* aBuilder)
{
  if (mFrame == aBuilder->RootReferenceFrame()) {
    return;
  }
  nsIFrame *outerFrame = nsLayoutUtils::GetCrossDocParentFrame(mFrame);
  mReferenceFrame =
    aBuilder->FindReferenceFrameFor(outerFrame);
  mToReferenceFrame = mFrame->GetOffsetToCrossDoc(mReferenceFrame);
  if (nsLayoutUtils::IsFixedPosFrameInDisplayPort(mFrame)) {
    // This is an odd special case. If we are both IsFixedPosFrameInDisplayPort
    // and transformed that we are our own AGR parent.
    // We want our frame to be our AGR because FrameLayerBuilder uses our AGR to
    // determine if we are inside a fixed pos subtree. If we use the outer AGR
    // from outside the fixed pos subtree FLB can't tell that we are fixed pos.
    mAnimatedGeometryRoot = mAnimatedGeometryRootForChildren;
  } else if (mFrame->StyleDisplay()->mPosition == NS_STYLE_POSITION_STICKY &&
             IsStickyFrameActive(aBuilder, mFrame, nullptr)) {
    // Similar to the IsFixedPosFrameInDisplayPort case we are our own AGR.
    // We are inside the sticky position, so our AGR is the sticky positioned
    // frame, which is our AGR, not the parent AGR.
    mAnimatedGeometryRoot = mAnimatedGeometryRootForChildren;
  } else if (mAnimatedGeometryRoot->mParentAGR) {
    mAnimatedGeometryRootForScrollMetadata = mAnimatedGeometryRoot->mParentAGR;
    if (!MayBeAnimated(aBuilder)) {
      // If we're an animated transform then we want the same AGR as our children
      // so that FrameLayerBuilder knows that this layer moves with the transform
      // and won't compute occlusions. If we're not animated then use our parent
      // AGR so that inactive transform layers can go in the same PaintedLayer as
      // surrounding content.
      mAnimatedGeometryRoot = mAnimatedGeometryRoot->mParentAGR;
    }
  }

  SetBuildingRect(aBuilder->GetVisibleRect() + mToReferenceFrame);
}

void
nsDisplayTransform::Init(nsDisplayListBuilder* aBuilder)
{
  mHasBounds = false;
  mStoredList.SetClipChain(nullptr, true);
  mStoredList.SetBuildingRect(mChildrenBuildingRect);
}

nsDisplayTransform::nsDisplayTransform(nsDisplayListBuilder* aBuilder,
                                       nsIFrame *aFrame, nsDisplayList *aList,
                                       const nsRect& aChildrenBuildingRect,
                                       uint32_t aIndex,
                                       bool aAllowAsyncAnimation)
  : nsDisplayItem(aBuilder, aFrame)
  , mStoredList(aBuilder, aFrame, aList)
  , mTransformGetter(nullptr)
  , mAnimatedGeometryRootForChildren(mAnimatedGeometryRoot)
  , mAnimatedGeometryRootForScrollMetadata(mAnimatedGeometryRoot)
  , mChildrenBuildingRect(aChildrenBuildingRect)
  , mIndex(aIndex)
  , mNoExtendContext(false)
  , mIsTransformSeparator(false)
  , mTransformPreserves3DInited(false)
  , mAllowAsyncAnimation(aAllowAsyncAnimation)
{
  MOZ_COUNT_CTOR(nsDisplayTransform);
  MOZ_ASSERT(aFrame, "Must have a frame!");
  SetReferenceFrameToAncestor(aBuilder);
  Init(aBuilder);
  UpdateBoundsFor3D(aBuilder);
}

nsDisplayTransform::nsDisplayTransform(nsDisplayListBuilder* aBuilder,
                                       nsIFrame *aFrame, nsDisplayList *aList,
                                       const nsRect& aChildrenBuildingRect,
                                       const Matrix4x4& aTransform,
                                       uint32_t aIndex)
  : nsDisplayItem(aBuilder, aFrame)
  , mStoredList(aBuilder, aFrame, aList)
  , mTransform(Some(aTransform))
  , mTransformGetter(nullptr)
  , mAnimatedGeometryRootForChildren(mAnimatedGeometryRoot)
  , mAnimatedGeometryRootForScrollMetadata(mAnimatedGeometryRoot)
  , mChildrenBuildingRect(aChildrenBuildingRect)
  , mIndex(aIndex)
  , mNoExtendContext(false)
  , mIsTransformSeparator(true)
  , mTransformPreserves3DInited(false)
  , mAllowAsyncAnimation(false)
{
  MOZ_COUNT_CTOR(nsDisplayTransform);
  MOZ_ASSERT(aFrame, "Must have a frame!");
  Init(aBuilder);
  UpdateBoundsFor3D(aBuilder);
}

/* Returns the delta specified by the transform-origin property.
 * This is a positive delta, meaning that it indicates the direction to move
 * to get from (0, 0) of the frame to the transform origin.  This function is
 * called off the main thread.
 */
/* static */ Point3D
nsDisplayTransform::GetDeltaToTransformOrigin(const nsIFrame* aFrame,
                                              float aAppUnitsPerPixel,
                                              const nsRect* aBoundsOverride)
{
  MOZ_ASSERT(aFrame, "Can't get delta for a null frame!");
  MOZ_ASSERT(aFrame->IsTransformed() ||
             aFrame->BackfaceIsHidden() ||
             aFrame->Combines3DTransformWithAncestors(),
             "Shouldn't get a delta for an untransformed frame!");

  if (!aFrame->IsTransformed()) {
    return Point3D();
  }

  /* For both of the coordinates, if the value of transform is a
   * percentage, it's relative to the size of the frame.  Otherwise, if it's
   * a distance, it's already computed for us!
   */
  const nsStyleDisplay* display = aFrame->StyleDisplay();
  // We don't use aBoundsOverride for SVG since we need to account for
  // refBox.X/Y(). This happens to work because ReflowSVG sets the frame's
  // mRect before calling FinishAndStoreOverflow so we don't need the override.
  TransformReferenceBox refBox;
  if (aBoundsOverride &&
      !(aFrame->GetStateBits() & NS_FRAME_SVG_LAYOUT)) {
    refBox.Init(aBoundsOverride->Size());
  } else {
    refBox.Init(aFrame);
  }

  /* Allows us to access dimension getters by index. */
  float transformOrigin[2];
  TransformReferenceBox::DimensionGetter dimensionGetter[] =
    { &TransformReferenceBox::Width, &TransformReferenceBox::Height };
  TransformReferenceBox::DimensionGetter offsetGetter[] =
    { &TransformReferenceBox::X, &TransformReferenceBox::Y };

  for (uint8_t index = 0; index < 2; ++index) {
    /* If the transform-origin specifies a percentage, take the percentage
     * of the size of the box.
     */
    const nsStyleCoord& originValue  = display->mTransformOrigin[index];
    if (originValue.GetUnit() == eStyleUnit_Calc) {
      const nsStyleCoord::Calc *calc = originValue.GetCalcValue();
      transformOrigin[index] =
        NSAppUnitsToFloatPixels((refBox.*dimensionGetter[index])(), aAppUnitsPerPixel) *
          calc->mPercent +
        NSAppUnitsToFloatPixels(calc->mLength, aAppUnitsPerPixel);
    } else if (originValue.GetUnit() == eStyleUnit_Percent) {
      transformOrigin[index] =
        NSAppUnitsToFloatPixels((refBox.*dimensionGetter[index])(), aAppUnitsPerPixel) *
        originValue.GetPercentValue();
    } else {
      MOZ_ASSERT(originValue.GetUnit() == eStyleUnit_Coord,
                 "unexpected unit");
      transformOrigin[index] =
        NSAppUnitsToFloatPixels(originValue.GetCoordValue(),
                                aAppUnitsPerPixel);
    }

    if (aFrame->GetStateBits() & NS_FRAME_SVG_LAYOUT) {
      // SVG frames (unlike other frames) have a reference box that can be (and
      // typically is) offset from the TopLeft() of the frame. We need to
      // account for that here.
      transformOrigin[index] +=
        NSAppUnitsToFloatPixels((refBox.*offsetGetter[index])(), aAppUnitsPerPixel);
    }
  }

  return Point3D(transformOrigin[0], transformOrigin[1],
                 NSAppUnitsToFloatPixels(display->mTransformOrigin[2].GetCoordValue(),
                                         aAppUnitsPerPixel));
}

/* static */ bool
nsDisplayTransform::ComputePerspectiveMatrix(const nsIFrame* aFrame,
                                             float aAppUnitsPerPixel,
                                             Matrix4x4& aOutMatrix)
{
  MOZ_ASSERT(aFrame, "Can't get delta for a null frame!");
  MOZ_ASSERT(aFrame->IsTransformed() ||
             aFrame->BackfaceIsHidden() ||
             aFrame->Combines3DTransformWithAncestors(),
             "Shouldn't get a delta for an untransformed frame!");
  MOZ_ASSERT(aOutMatrix.IsIdentity(), "Must have a blank output matrix");

  if (!aFrame->IsTransformed()) {
    return false;
  }

  /* Find our containing block, which is the element that provides the
   * value for perspective we need to use
   */

  //TODO: Is it possible that the cbFrame's bounds haven't been set correctly yet
  // (similar to the aBoundsOverride case for GetResultingTransformMatrix)?
  nsIFrame* cbFrame = aFrame->GetContainingBlock(nsIFrame::SKIP_SCROLLED_FRAME);
  if (!cbFrame) {
    return false;
  }

  /* Grab the values for perspective and perspective-origin (if present) */

  const nsStyleDisplay* cbDisplay = cbFrame->StyleDisplay();
  if (cbDisplay->mChildPerspective.GetUnit() != eStyleUnit_Coord) {
    return false;
  }
  nscoord perspective = cbDisplay->mChildPerspective.GetCoordValue();
  if (perspective < std::numeric_limits<Float>::epsilon()) {
    return true;
  }

  TransformReferenceBox refBox(cbFrame);

  Point perspectiveOrigin =
    nsStyleTransformMatrix::Convert2DPosition(cbDisplay->mPerspectiveOrigin,
                                              refBox, aAppUnitsPerPixel);

  /* GetOffsetTo computes the offset required to move from 0,0 in cbFrame to 0,0
   * in aFrame. Although we actually want the inverse of this, it's faster to
   * compute this way.
   */
  nsPoint frameToCbOffset = -aFrame->GetOffsetTo(cbFrame);
  Point frameToCbGfxOffset(
            NSAppUnitsToFloatPixels(frameToCbOffset.x, aAppUnitsPerPixel),
            NSAppUnitsToFloatPixels(frameToCbOffset.y, aAppUnitsPerPixel));

  /* Move the perspective origin to be relative to aFrame, instead of relative
   * to the containing block which is how it was specified in the style system.
   */
  perspectiveOrigin += frameToCbGfxOffset;

  aOutMatrix._34 =
    -1.0 / NSAppUnitsToFloatPixels(perspective, aAppUnitsPerPixel);

  aOutMatrix.ChangeBasis(Point3D(perspectiveOrigin.x, perspectiveOrigin.y, 0));
  return true;
}

nsDisplayTransform::FrameTransformProperties::FrameTransformProperties(const nsIFrame* aFrame,
                                                                       float aAppUnitsPerPixel,
                                                                       const nsRect* aBoundsOverride)
  : mFrame(aFrame)
  , mTransformList(aFrame->StyleDisplay()->GetCombinedTransform())
  , mToTransformOrigin(GetDeltaToTransformOrigin(aFrame, aAppUnitsPerPixel, aBoundsOverride))
{
}

/* Wraps up the transform matrix in a change-of-basis matrix pair that
 * translates from local coordinate space to transform coordinate space, then
 * hands it back.
 */
Matrix4x4
nsDisplayTransform::GetResultingTransformMatrix(const FrameTransformProperties& aProperties,
                                                const nsPoint& aOrigin,
                                                float aAppUnitsPerPixel,
                                                uint32_t aFlags,
                                                const nsRect* aBoundsOverride)
{
  return GetResultingTransformMatrixInternal(aProperties, aOrigin, aAppUnitsPerPixel,
                                             aFlags, aBoundsOverride);
}

Matrix4x4
nsDisplayTransform::GetResultingTransformMatrix(const nsIFrame* aFrame,
                                                const nsPoint& aOrigin,
                                                float aAppUnitsPerPixel,
                                                uint32_t aFlags,
                                                const nsRect* aBoundsOverride)
{
  FrameTransformProperties props(aFrame,
                                 aAppUnitsPerPixel,
                                 aBoundsOverride);

  return GetResultingTransformMatrixInternal(props, aOrigin, aAppUnitsPerPixel,
                                             aFlags, aBoundsOverride);
}

Matrix4x4
nsDisplayTransform::GetResultingTransformMatrixInternal(const FrameTransformProperties& aProperties,
                                                        const nsPoint& aOrigin,
                                                        float aAppUnitsPerPixel,
                                                        uint32_t aFlags,
                                                        const nsRect* aBoundsOverride)
{
  const nsIFrame *frame = aProperties.mFrame;
  NS_ASSERTION(frame || !(aFlags & INCLUDE_PERSPECTIVE), "Must have a frame to compute perspective!");

  // Get the underlying transform matrix:

  // We don't use aBoundsOverride for SVG since we need to account for
  // refBox.X/Y(). This happens to work because ReflowSVG sets the frame's
  // mRect before calling FinishAndStoreOverflow so we don't need the override.
  TransformReferenceBox refBox;
  if (aBoundsOverride &&
      (!frame || !(frame->GetStateBits() & NS_FRAME_SVG_LAYOUT))) {
    refBox.Init(aBoundsOverride->Size());
  } else {
    refBox.Init(frame);
  }

  /* Get the matrix, then change its basis to factor in the origin. */
  bool dummyBool;
  Matrix4x4 result;
  // Call IsSVGTransformed() regardless of the value of
  // disp->mSpecifiedTransform, since we still need any
  // parentsChildrenOnlyTransform.
  Matrix svgTransform, parentsChildrenOnlyTransform;
  bool hasSVGTransforms =
    frame && frame->IsSVGTransformed(&svgTransform,
                                     &parentsChildrenOnlyTransform);
  /* Transformed frames always have a transform, or are preserving 3d (and might still have perspective!) */
  if (aProperties.mTransformList) {
    result = nsStyleTransformMatrix::ReadTransforms(aProperties.mTransformList->mHead,
                                                    refBox, aAppUnitsPerPixel,
                                                    &dummyBool);
  } else if (hasSVGTransforms) {
    // Correct the translation components for zoom:
    float pixelsPerCSSPx = frame->PresContext()->AppUnitsPerCSSPixel() /
                             aAppUnitsPerPixel;
    svgTransform._31 *= pixelsPerCSSPx;
    svgTransform._32 *= pixelsPerCSSPx;
    result = Matrix4x4::From2D(svgTransform);
  }

  // Apply any translation due to 'transform-origin' and/or 'transform-box':
  result.ChangeBasis(aProperties.mToTransformOrigin);

  // See the comment for nsSVGContainerFrame::HasChildrenOnlyTransform for
  // an explanation of what children-only transforms are.
  bool parentHasChildrenOnlyTransform =
    hasSVGTransforms && !parentsChildrenOnlyTransform.IsIdentity();

  if (parentHasChildrenOnlyTransform) {
    float pixelsPerCSSPx =
      frame->PresContext()->AppUnitsPerCSSPixel() / aAppUnitsPerPixel;
    parentsChildrenOnlyTransform._31 *= pixelsPerCSSPx;
    parentsChildrenOnlyTransform._32 *= pixelsPerCSSPx;

    Point3D frameOffset(
      NSAppUnitsToFloatPixels(-frame->GetPosition().x, aAppUnitsPerPixel),
      NSAppUnitsToFloatPixels(-frame->GetPosition().y, aAppUnitsPerPixel),
      0);
    Matrix4x4 parentsChildrenOnlyTransform3D =
      Matrix4x4::From2D(parentsChildrenOnlyTransform).ChangeBasis(frameOffset);

    result *= parentsChildrenOnlyTransform3D;
  }

  Matrix4x4 perspectiveMatrix;
  bool hasPerspective = aFlags & INCLUDE_PERSPECTIVE;
  if (hasPerspective) {
    if (ComputePerspectiveMatrix(frame, aAppUnitsPerPixel, perspectiveMatrix)) {
      result *= perspectiveMatrix;
    }
  }

  if ((aFlags & INCLUDE_PRESERVE3D_ANCESTORS) &&
      frame && frame->Combines3DTransformWithAncestors()) {
    // Include the transform set on our parent
    nsIFrame* parentFrame = frame->GetInFlowParent();
    NS_ASSERTION(parentFrame && parentFrame->IsTransformed() &&
                 parentFrame->Extend3DContext(),
                 "Preserve3D mismatch!");
    FrameTransformProperties props(parentFrame,
                                   aAppUnitsPerPixel,
                                   nullptr);

    uint32_t flags = aFlags & (INCLUDE_PRESERVE3D_ANCESTORS|INCLUDE_PERSPECTIVE);

    // If this frame isn't transformed (but we exist for backface-visibility),
    // then we're not a reference frame so no offset to origin will be added.
    // Otherwise we need to manually translate into our parent's coordinate
    // space.
    if (frame->IsTransformed()) {
      nsLayoutUtils::PostTranslate(result, frame->GetPosition(), aAppUnitsPerPixel, !hasSVGTransforms);
    }
    Matrix4x4 parent =
      GetResultingTransformMatrixInternal(props,
                                          nsPoint(0, 0),
                                          aAppUnitsPerPixel, flags,
                                          nullptr);
    result = result * parent;
  }

  if (aFlags & OFFSET_BY_ORIGIN) {
    nsLayoutUtils::PostTranslate(result, aOrigin, aAppUnitsPerPixel, !hasSVGTransforms);
  }

  return result;
}

bool
nsDisplayOpacity::CanUseAsyncAnimations(nsDisplayListBuilder* aBuilder)
{
  if (ActiveLayerTracker::IsStyleAnimated(aBuilder, mFrame, eCSSProperty_opacity)) {
    return true;
  }

  EffectCompositor::SetPerformanceWarning(
    mFrame, eCSSProperty_opacity,
    AnimationPerformanceWarning(
      AnimationPerformanceWarning::Type::OpacityFrameInactive));

  return false;
}

bool
nsDisplayTransform::CanUseAsyncAnimations(nsDisplayListBuilder* aBuilder)
{
  return mAllowAsyncAnimation;
}

/* static */ auto
nsDisplayTransform::ShouldPrerenderTransformedContent(nsDisplayListBuilder* aBuilder,
                                                      nsIFrame* aFrame,
                                                      nsRect* aDirtyRect) -> PrerenderDecision
{
  // Elements whose transform has been modified recently, or which
  // have a compositor-animated transform, can be prerendered. An element
  // might have only just had its transform animated in which case
  // the ActiveLayerManager may not have been notified yet.
  if (!ActiveLayerTracker::IsStyleMaybeAnimated(aFrame, eCSSProperty_transform) &&
      !EffectCompositor::HasAnimationsForCompositor(aFrame,
                                                    eCSSProperty_transform)) {
    EffectCompositor::SetPerformanceWarning(
      aFrame, eCSSProperty_transform,
      AnimationPerformanceWarning(
        AnimationPerformanceWarning::Type::TransformFrameInactive));

    return NoPrerender;
  }

  // We should not allow prerender if any ancestor container element has
  // mask/clip-path effects.
  //
  // With prerender and async transform animation, we do not need to restyle an
  // animated element to respect position changes, since that transform is done
  // by layer animation. As a result, the container element is not aware of
  // position change of that containing element and loses the chance to update
  // the content of mask/clip-path.
  //
  // Why do we need to update a mask? This is relative to how we generate a
  // mask layer in ContainerState::SetupMaskLayerForCSSMask. While creating a
  // mask layer, to reduce memory usage, we did not choose the size of the
  // masked element as mask size. Instead, we read the union of bounds of all
  // children display items by nsDisplayWrapList::GetBounds, which is smaller
  // than or equal to the masked element's boundary, and use it as the position
  // size of the mask layer. That union bounds is actually affected by the
  // geometry of the animated element. To keep the content of mask up to date,
  // forbidding of prerender is required.
  for (nsIFrame* container = nsLayoutUtils::GetCrossDocParentFrame(aFrame);
       container; container = nsLayoutUtils::GetCrossDocParentFrame(container)) {
    const nsStyleSVGReset *svgReset = container->StyleSVGReset();
    if (svgReset->HasMask() || svgReset->HasClipPath()) {
      return NoPrerender;
    }
  }

  // If the incoming dirty rect already contains the entire overflow area,
  // we are already rendering the entire content.
  nsRect overflow = aFrame->GetVisualOverflowRectRelativeToSelf();
  if (aDirtyRect->Contains(overflow)) {
    return FullPrerender;
  }

  float viewportRatioX = gfxPrefs::AnimationPrerenderViewportRatioLimitX();
  float viewportRatioY = gfxPrefs::AnimationPrerenderViewportRatioLimitY();
  uint32_t absoluteLimitX = gfxPrefs::AnimationPrerenderAbsoluteLimitX();
  uint32_t absoluteLimitY = gfxPrefs::AnimationPrerenderAbsoluteLimitY();
  nsSize refSize = aBuilder->RootReferenceFrame()->GetSize();
  // Only prerender if the transformed frame's size is <= a multiple of the
  // reference frame size (~viewport), and less than an absolute limit.
  // Both the ratio and the absolute limit are configurable.
  nsSize relativeLimit(nscoord(refSize.width * viewportRatioX),
                       nscoord(refSize.height * viewportRatioY));
  nsSize absoluteLimit(aFrame->PresContext()->DevPixelsToAppUnits(absoluteLimitX),
                       aFrame->PresContext()->DevPixelsToAppUnits(absoluteLimitY));
  nsSize maxSize = Min(relativeLimit, absoluteLimit);

  const auto transform = nsLayoutUtils::GetTransformToAncestor(aFrame,
    nsLayoutUtils::GetDisplayRootFrame(aFrame));
  const gfxRect transformedBounds = transform.TransformAndClipBounds(
    gfxRect(overflow.x, overflow.y, overflow.width, overflow.height),
    gfxRect::MaxIntRect());
  const nsSize frameSize = nsSize(transformedBounds.width, transformedBounds.height);

  uint64_t maxLimitArea = uint64_t(maxSize.width) * maxSize.height;
  uint64_t frameArea = uint64_t(frameSize.width) * frameSize.height;
  if (frameArea <= maxLimitArea && frameSize <= absoluteLimit) {
    *aDirtyRect = overflow;
    return FullPrerender;
  } else if (gfxPrefs::PartiallyPrerenderAnimatedContent()) {
    *aDirtyRect = nsLayoutUtils::ComputePartialPrerenderArea(*aDirtyRect, overflow, maxSize);
    return PartialPrerender;
  }

  if (frameArea > maxLimitArea) {
    uint64_t appUnitsPerPixel = nsPresContext::AppUnitsPerCSSPixel();
    EffectCompositor::SetPerformanceWarning(
      aFrame, eCSSProperty_transform,
      AnimationPerformanceWarning(
        AnimationPerformanceWarning::Type::ContentTooLargeArea,
        {
          int(frameArea / (appUnitsPerPixel * appUnitsPerPixel)),
          int(maxLimitArea / (appUnitsPerPixel * appUnitsPerPixel)),
        }));
  } else {
    EffectCompositor::SetPerformanceWarning(
      aFrame, eCSSProperty_transform,
      AnimationPerformanceWarning(
        AnimationPerformanceWarning::Type::ContentTooLarge,
        {
          nsPresContext::AppUnitsToIntCSSPixels(frameSize.width),
          nsPresContext::AppUnitsToIntCSSPixels(frameSize.height),
          nsPresContext::AppUnitsToIntCSSPixels(relativeLimit.width),
          nsPresContext::AppUnitsToIntCSSPixels(relativeLimit.height),
          nsPresContext::AppUnitsToIntCSSPixels(absoluteLimit.width),
          nsPresContext::AppUnitsToIntCSSPixels(absoluteLimit.height),
        }));
  }

  return NoPrerender;
}

/* If the matrix is singular, or a hidden backface is shown, the frame won't be visible or hit. */
static bool IsFrameVisible(nsIFrame* aFrame, const Matrix4x4& aMatrix)
{
  if (aMatrix.IsSingular()) {
    return false;
  }
  if (aFrame->BackfaceIsHidden() && aMatrix.IsBackfaceVisible()) {
    return false;
  }
  return true;
}

const Matrix4x4Flagged&
nsDisplayTransform::GetTransform() const
{
  if (mTransform) {
    return *mTransform;
  }

  float scale = mFrame->PresContext()->AppUnitsPerDevPixel();

  if (mTransformGetter) {
    mTransform.emplace(mTransformGetter(mFrame, scale));
    Point3D newOrigin =
      Point3D(NSAppUnitsToFloatPixels(mToReferenceFrame.x, scale),
              NSAppUnitsToFloatPixels(mToReferenceFrame.y, scale),
              0.0f);
    mTransform->ChangeBasis(newOrigin.x, newOrigin.y, newOrigin.z);
  } else if (!mIsTransformSeparator) {
    DebugOnly<bool> isReference =
      mFrame->IsTransformed() ||
      mFrame->Combines3DTransformWithAncestors() || mFrame->Extend3DContext();
    MOZ_ASSERT(isReference);
    mTransform.emplace(
      GetResultingTransformMatrix(mFrame, ToReferenceFrame(),
                                  scale, INCLUDE_PERSPECTIVE|OFFSET_BY_ORIGIN));
  } else {
    // Use identity matrix
    mTransform.emplace();
  }

  return *mTransform;
}

const Matrix4x4Flagged&
nsDisplayTransform::GetInverseTransform() const
{
  if (mInverseTransform) {
    return *mInverseTransform;
  }

  MOZ_ASSERT(!GetTransform().IsSingular());

  mInverseTransform.emplace(GetTransform().Inverse());

  return *mInverseTransform;
}

Matrix4x4
nsDisplayTransform::GetTransformForRendering(LayoutDevicePoint* aOutOrigin)
{
  if (!mFrame->HasPerspective() || mTransformGetter || mIsTransformSeparator) {
    if (!mTransformGetter && !mIsTransformSeparator && aOutOrigin) {
      // If aOutOrigin is provided, put the offset to origin into it, because
      // we need to keep it separate for webrender. The combination of
      // *aOutOrigin and the returned matrix here should always be equivalent
      // to what GetTransform() would have returned.
      float scale = mFrame->PresContext()->AppUnitsPerDevPixel();
      *aOutOrigin = LayoutDevicePoint::FromAppUnits(ToReferenceFrame(), scale);
      return GetResultingTransformMatrix(mFrame, nsPoint(0, 0), scale, INCLUDE_PERSPECTIVE);
    }
    return GetTransform().GetMatrix();
  }
  MOZ_ASSERT(!mTransformGetter);

  float scale = mFrame->PresContext()->AppUnitsPerDevPixel();
  // Don't include perspective transform, or the offset to origin, since
  // nsDisplayPerspective will handle both of those.
  return GetResultingTransformMatrix(mFrame, ToReferenceFrame(), scale, 0);
}

const Matrix4x4&
nsDisplayTransform::GetAccumulatedPreserved3DTransform(nsDisplayListBuilder* aBuilder)
{
  MOZ_ASSERT(!mFrame->Extend3DContext() || IsLeafOf3DContext());
  // XXX: should go back to fix mTransformGetter.
  if (!mTransformPreserves3DInited) {
    mTransformPreserves3DInited = true;
    if (!IsLeafOf3DContext()) {
      mTransformPreserves3D = GetTransform().GetMatrix();
      return mTransformPreserves3D;
    }

    const nsIFrame* establisher; // Establisher of the 3D rendering context.
    for (establisher = mFrame;
         establisher && establisher->Combines3DTransformWithAncestors();
         establisher = establisher->GetInFlowParent()) {
    }
    const nsIFrame* establisherReference =
      aBuilder->FindReferenceFrameFor(nsLayoutUtils::GetCrossDocParentFrame(establisher));

    nsPoint offset = establisher->GetOffsetToCrossDoc(establisherReference);
    float scale = mFrame->PresContext()->AppUnitsPerDevPixel();
    uint32_t flags = INCLUDE_PRESERVE3D_ANCESTORS|INCLUDE_PERSPECTIVE|OFFSET_BY_ORIGIN;
    mTransformPreserves3D =
      GetResultingTransformMatrix(mFrame, offset, scale, flags);
  }
  return mTransformPreserves3D;
}

bool
nsDisplayTransform::ShouldBuildLayerEvenIfInvisible(nsDisplayListBuilder* aBuilder) const
{
  // The visible rect of a Preserves-3D frame is just an intermediate
  // result.  It should always build a layer to make sure it is
  // rendering correctly.
  return MayBeAnimated(aBuilder) || mFrame->Combines3DTransformWithAncestors();
}

bool
nsDisplayTransform::CreateWebRenderCommands(mozilla::wr::DisplayListBuilder& aBuilder,
                                            mozilla::wr::IpcResourceUpdateQueue& aResources,
                                            const StackingContextHelper& aSc,
                                            WebRenderLayerManager* aManager,
                                            nsDisplayListBuilder* aDisplayListBuilder)
{
  // We want to make sure we don't pollute the transform property in the WR
  // stacking context by including the position of this frame (relative to the
  // parent reference frame). We need to keep those separate; the position of
  // this frame goes into the stacking context bounds while the transform goes
  // into the transform.
  LayoutDevicePoint position;
  Matrix4x4 newTransformMatrix = GetTransformForRendering(&position);

  gfx::Matrix4x4* transformForSC = &newTransformMatrix;
  if (newTransformMatrix.IsIdentity()) {
    // If the transform is an identity transform, strip it out so that WR
    // doesn't turn this stacking context into a reference frame, as it
    // affects positioning. Bug 1345577 tracks a better fix.
    transformForSC = nullptr;
  }

  RefPtr<WebRenderAnimationData> animationData = aManager->CommandBuilder().CreateOrRecycleWebRenderUserData<WebRenderAnimationData>(this);

  AnimationInfo& animationInfo = animationData->GetAnimationInfo();
  AddAnimationsForProperty(Frame(), aDisplayListBuilder,
                           this, eCSSProperty_transform,
                           animationInfo, false, true);
  animationInfo.StartPendingAnimations(aManager->GetAnimationReadyTime());

  // Note that animationsId can be 0 (uninitialized in AnimationInfo) if there
  // are no active animations.
  uint64_t animationsId = animationInfo.GetCompositorAnimationsId();
  wr::WrAnimationProperty prop;
  if (!animationInfo.GetAnimations().IsEmpty()) {
    prop.id = animationsId;
    prop.effect_type = wr::WrAnimationType::Transform;

    OpAddCompositorAnimations
      anim(CompositorAnimations(animationInfo.GetAnimations(), animationsId));
    aManager->WrBridge()->AddWebRenderParentCommand(anim);
    aManager->AddActiveCompositorAnimationId(animationsId);
  } else if (animationsId) {
    aManager->AddCompositorAnimationsIdForDiscard(animationsId);
    animationsId = 0;
  }

  nsTArray<mozilla::wr::WrFilterOp> filters;
  Maybe<nsDisplayTransform*> deferredTransformItem;
  if (!mFrame->HasPerspective()) {
    // If it has perspective, we create a new scroll data via the
    // UpdateScrollData call because that scenario is more complex. Otherwise
    // we can just stash the transform on the StackingContextHelper and
    // apply it to any scroll data that are created inside this
    // nsDisplayTransform.
    deferredTransformItem = Some(this);
  }

  // If it looks like we're animated, we should rasterize in local space
  // (disabling subpixel-aa and global pixel snapping)
  bool rasterizeLocally = ActiveLayerTracker::IsStyleMaybeAnimated(
    Frame(), eCSSProperty_transform);

  StackingContextHelper sc(aSc,
                           aBuilder,
                           filters,
                           LayoutDeviceRect(position, LayoutDeviceSize()),
                           &newTransformMatrix,
                           animationsId ? &prop : nullptr,
                           nullptr,
                           transformForSC,
                           nullptr,
                           gfx::CompositionOp::OP_OVER,
                           !BackfaceIsHidden(),
                           mFrame->Extend3DContext() && !mNoExtendContext,
                           deferredTransformItem,
                           nullptr,
                           rasterizeLocally);

  return mStoredList.CreateWebRenderCommands(aBuilder, aResources, sc,
                                             aManager, aDisplayListBuilder);
}

bool
nsDisplayTransform::UpdateScrollData(mozilla::layers::WebRenderScrollData* aData,
                                     mozilla::layers::WebRenderLayerScrollData* aLayerData)
{
  if (!mFrame->HasPerspective()) {
    // This case is handled in CreateWebRenderCommands by stashing the transform
    // on the stacking context.
    return false;
  }
  if (aLayerData) {
    aLayerData->SetTransform(GetTransform().GetMatrix());
    aLayerData->SetTransformIsPerspective(true);
  }
  return true;
}

already_AddRefed<Layer> nsDisplayTransform::BuildLayer(nsDisplayListBuilder *aBuilder,
                                                       LayerManager *aManager,
                                                       const ContainerLayerParameters& aContainerParameters)
{
  // While generating a glyph mask, the transform vector of the root frame had
  // been applied into the target context, so stop applying it again here.
  const bool shouldSkipTransform =
    (aBuilder->RootReferenceFrame() == mFrame) &&
    (aBuilder->IsForGenerateGlyphMask() || aBuilder->IsForPaintingSelectionBG());

  /* For frames without transform, it would not be removed for
   * backface hidden here.  But, it would be removed by the init
   * function of nsDisplayTransform.
   */
  const Matrix4x4 newTransformMatrix =
    shouldSkipTransform ? Matrix4x4(): GetTransformForRendering();

  uint32_t flags = FrameLayerBuilder::CONTAINER_ALLOW_PULL_BACKGROUND_COLOR;
  RefPtr<ContainerLayer> container = aManager->GetLayerBuilder()->
    BuildContainerLayerFor(aBuilder, aManager, mFrame, this, mStoredList.GetChildren(),
                           aContainerParameters, &newTransformMatrix, flags);

  if (!container) {
    return nullptr;
  }

  // Add the preserve-3d flag for this layer, BuildContainerLayerFor clears all flags,
  // so we never need to explicitely unset this flag.
  if (mFrame->Extend3DContext() && !mNoExtendContext) {
    container->SetContentFlags(container->GetContentFlags() | Layer::CONTENT_EXTEND_3D_CONTEXT);
  } else {
    container->SetContentFlags(container->GetContentFlags() & ~Layer::CONTENT_EXTEND_3D_CONTEXT);
  }

  if (mAllowAsyncAnimation) {
    mFrame->SetProperty(nsIFrame::RefusedAsyncAnimationProperty(), false);
  }

  nsDisplayListBuilder::AddAnimationsAndTransitionsToLayer(container, aBuilder,
                                                           this, mFrame,
                                                           eCSSProperty_transform);
  if (mAllowAsyncAnimation && MayBeAnimated(aBuilder)) {
    // Only allow async updates to the transform if we're an animated layer, since that's what
    // triggers us to set the correct AGR in the constructor and makes sure FrameLayerBuilder
    // won't compute occlusions for this layer.
    container->SetUserData(nsIFrame::LayerIsPrerenderedDataKey(),
                           /*the value is irrelevant*/nullptr);
    container->SetContentFlags(container->GetContentFlags() | Layer::CONTENT_MAY_CHANGE_TRANSFORM);
  } else {
    container->RemoveUserData(nsIFrame::LayerIsPrerenderedDataKey());
    container->SetContentFlags(container->GetContentFlags() & ~Layer::CONTENT_MAY_CHANGE_TRANSFORM);
  }
  return container.forget();
}

bool
nsDisplayTransform::MayBeAnimated(nsDisplayListBuilder* aBuilder) const
{
  // If EffectCompositor::HasAnimationsForCompositor() is true then we can
  // completely bypass the main thread for this animation, so it is always
  // worthwhile.
  // For ActiveLayerTracker::IsStyleAnimated() cases the main thread is
  // already involved so there is less to be gained.
  // Therefore we check that the *post-transform* bounds of this item are
  // big enough to justify an active layer.
  if (EffectCompositor::HasAnimationsForCompositor(mFrame,
                                                   eCSSProperty_transform) ||
      (ActiveLayerTracker::IsStyleAnimated(aBuilder,
                                           mFrame,
                                           eCSSProperty_transform) &&
       !IsItemTooSmallForActiveLayer(mFrame))) {
    return true;
  }
  return false;
}

nsDisplayItem::LayerState
nsDisplayTransform::GetLayerState(nsDisplayListBuilder* aBuilder,
                                  LayerManager* aManager,
                                  const ContainerLayerParameters& aParameters) {
  // If the transform is 3d, the layer takes part in preserve-3d
  // sorting, or the layer is a separator then we *always* want this
  // to be an active layer.
  // Checking HasPerspective() is needed to handle perspective value 0 when
  // the transform is 2D.
  if (!GetTransform().Is2D() || mFrame->Combines3DTransformWithAncestors() ||
      mIsTransformSeparator || mFrame->HasPerspective()) {
    return LAYER_ACTIVE_FORCE;
  }

  if (MayBeAnimated(aBuilder)) {
    // Returns LAYER_ACTIVE_FORCE to avoid flatterning the layer for async
    // animations.
    return LAYER_ACTIVE_FORCE;
  }

  // Expect the child display items to have this frame as their animated
  // geometry root (since it will be their reference frame). If they have a
  // different animated geometry root, we'll make this an active layer so the
  // animation can be accelerated.
  return RequiredLayerStateForChildren(aBuilder, aManager, aParameters,
    *mStoredList.GetChildren(), mAnimatedGeometryRootForChildren);
}

bool nsDisplayTransform::ComputeVisibility(nsDisplayListBuilder *aBuilder,
                                             nsRegion *aVisibleRegion)
{
  // nsDisplayTransform::GetBounds() returns an empty rect in nested 3d context.
  // Calling mStoredList.RecomputeVisibility below for such transform causes the
  // child display items to end up with empty visible rect.
  // We avoid this by bailing out always if we are dealing with a 3d context.
  if (mFrame->Extend3DContext() || mFrame->Combines3DTransformWithAncestors()) {
    return true;
  }

  /* As we do this, we need to be sure to
   * untransform the visible rect, since we want everything that's painting to
   * think that it's painting in its original rectangular coordinate space.
   * If we can't untransform, take the entire overflow rect */
  nsRect untransformedVisibleRect;
  if (!UntransformPaintRect(aBuilder, &untransformedVisibleRect))
  {
    untransformedVisibleRect = mFrame->GetVisualOverflowRectRelativeToSelf();
  }
  nsRegion untransformedVisible = untransformedVisibleRect;
  // Call RecomputeVisiblity instead of ComputeVisibility since
  // nsDisplayItem::ComputeVisibility should only be called from
  // nsDisplayList::ComputeVisibility (which sets mVisibleRect on the item)
  mStoredList.RecomputeVisibility(aBuilder, &untransformedVisible);
  return true;
}

#ifdef DEBUG_HIT
#include <time.h>
#endif

/* HitTest does some fun stuff with matrix transforms to obtain the answer. */
void nsDisplayTransform::HitTest(nsDisplayListBuilder *aBuilder,
                                 const nsRect& aRect,
                                 HitTestState *aState,
                                 nsTArray<nsIFrame*> *aOutFrames)
{
  if (aState->mInPreserves3D) {
    mStoredList.HitTest(aBuilder, aRect, aState, aOutFrames);
    return;
  }

  /* Here's how this works:
   * 1. Get the matrix.  If it's singular, abort (clearly we didn't hit
   *    anything).
   * 2. Invert the matrix.
   * 3. Use it to transform the rect into the correct space.
   * 4. Pass that rect down through to the list's version of HitTest.
   */
  // GetTransform always operates in dev pixels.
  float factor = mFrame->PresContext()->AppUnitsPerDevPixel();
  Matrix4x4 matrix = GetAccumulatedPreserved3DTransform(aBuilder);

  if (!IsFrameVisible(mFrame, matrix)) {
    return;
  }

  /* We want to go from transformed-space to regular space.
   * Thus we have to invert the matrix, which normally does
   * the reverse operation (e.g. regular->transformed)
   */

  /* Now, apply the transform and pass it down the channel. */
  matrix.Invert();
  nsRect resultingRect;
  if (aRect.width == 1 && aRect.height == 1) {
    // Magic width/height indicating we're hit testing a point, not a rect
    Point4D point = matrix.ProjectPoint(Point(NSAppUnitsToFloatPixels(aRect.x, factor),
                                              NSAppUnitsToFloatPixels(aRect.y, factor)));
    if (!point.HasPositiveWCoord()) {
      return;
    }

    Point point2d = point.As2DPoint();

    resultingRect = nsRect(NSFloatPixelsToAppUnits(float(point2d.x), factor),
                           NSFloatPixelsToAppUnits(float(point2d.y), factor),
                           1, 1);

  } else {
    Rect originalRect(NSAppUnitsToFloatPixels(aRect.x, factor),
                      NSAppUnitsToFloatPixels(aRect.y, factor),
                      NSAppUnitsToFloatPixels(aRect.width, factor),
                      NSAppUnitsToFloatPixels(aRect.height, factor));


    bool snap;
    nsRect childBounds = mStoredList.GetBounds(aBuilder, &snap);
    Rect childGfxBounds(NSAppUnitsToFloatPixels(childBounds.x, factor),
                        NSAppUnitsToFloatPixels(childBounds.y, factor),
                        NSAppUnitsToFloatPixels(childBounds.width, factor),
                        NSAppUnitsToFloatPixels(childBounds.height, factor));

    Rect rect = matrix.ProjectRectBounds(originalRect, childGfxBounds);

    resultingRect = nsRect(NSFloatPixelsToAppUnits(float(rect.X()), factor),
                           NSFloatPixelsToAppUnits(float(rect.Y()), factor),
                           NSFloatPixelsToAppUnits(float(rect.Width()), factor),
                           NSFloatPixelsToAppUnits(float(rect.Height()), factor));
  }

  if (resultingRect.IsEmpty()) {
    return;
  }


#ifdef DEBUG_HIT
  printf("Frame: %p\n", dynamic_cast<void *>(mFrame));
  printf("  Untransformed point: (%f, %f)\n", resultingRect.X(), resultingRect.Y());
  uint32_t originalFrameCount = aOutFrames.Length();
#endif

  mStoredList.HitTest(aBuilder, resultingRect, aState, aOutFrames);

#ifdef DEBUG_HIT
  if (originalFrameCount != aOutFrames.Length())
    printf("  Hit! Time: %f, first frame: %p\n", static_cast<double>(clock()),
           dynamic_cast<void *>(aOutFrames.ElementAt(0)));
  printf("=== end of hit test ===\n");
#endif

}

float
nsDisplayTransform::GetHitDepthAtPoint(nsDisplayListBuilder* aBuilder, const nsPoint& aPoint)
{
  // GetTransform always operates in dev pixels.
  float factor = mFrame->PresContext()->AppUnitsPerDevPixel();
  Matrix4x4 matrix = GetAccumulatedPreserved3DTransform(aBuilder);

  NS_ASSERTION(IsFrameVisible(mFrame, matrix),
               "We can't have hit a frame that isn't visible!");

  Matrix4x4 inverse = matrix;
  inverse.Invert();
  Point4D point = inverse.ProjectPoint(Point(NSAppUnitsToFloatPixels(aPoint.x, factor),
                                             NSAppUnitsToFloatPixels(aPoint.y, factor)));

  Point point2d = point.As2DPoint();

  Point3D transformed = matrix.TransformPoint(Point3D(point2d.x, point2d.y, 0));
  return transformed.z;
}

/* The bounding rectangle for the object is the overflow rectangle translated
 * by the reference point.
 */
nsRect
nsDisplayTransform::GetBounds(nsDisplayListBuilder* aBuilder,
                              bool* aSnap) const
{
  *aSnap = false;

  if (mHasBounds) {
    return mBounds;
  }

  if (mFrame->Extend3DContext() && !mIsTransformSeparator) {
    return nsRect();
  }

  nsRect untransformedBounds = mStoredList.GetBounds(aBuilder, aSnap);
  // GetTransform always operates in dev pixels.
  float factor = mFrame->PresContext()->AppUnitsPerDevPixel();
  mBounds = nsLayoutUtils::MatrixTransformRect(untransformedBounds,
                                               GetTransform(),
                                               factor);
  mHasBounds = true;
  return mBounds;
}

void
nsDisplayTransform::ComputeBounds(nsDisplayListBuilder* aBuilder)
{
  MOZ_ASSERT(mFrame->Extend3DContext() || IsLeafOf3DContext());

  /* For some cases, the transform would make an empty bounds, but it
   * may be turned back again to get a non-empty bounds.  We should
   * not depend on transforming bounds level by level.
   *
   * Here, it applies accumulated transforms on the leaf frames of the
   * 3d rendering context, and track and accmulate bounds at
   * nsDisplayListBuilder.
   */
  nsDisplayListBuilder::AutoAccumulateTransform accTransform(aBuilder);

  accTransform.Accumulate(GetTransform().GetMatrix());

  if (!IsLeafOf3DContext()) {
    // Do not dive into another 3D context.
    mStoredList.DoUpdateBoundsPreserves3D(aBuilder);
  }

  /* For Preserves3D, it is bounds of only children as leaf frames.
   * For non-leaf frames, their bounds are accumulated and kept at
   * nsDisplayListBuilder.
   */
  bool snap;
  nsRect untransformedBounds = mStoredList.GetBounds(aBuilder, &snap);
  // GetTransform always operates in dev pixels.
  float factor = mFrame->PresContext()->AppUnitsPerDevPixel();
  nsRect rect =
    nsLayoutUtils::MatrixTransformRect(untransformedBounds,
                                       accTransform.GetCurrentTransform(),
                                       factor);

  aBuilder->AccumulateRect(rect);
}

/* The transform is opaque iff the transform consists solely of scales and
 * translations and if the underlying content is opaque.  Thus if the transform
 * is of the form
 *
 * |a c e|
 * |b d f|
 * |0 0 1|
 *
 * We need b and c to be zero.
 *
 * We also need to check whether the underlying opaque content completely fills
 * our visible rect. We use UntransformRect which expands to the axis-aligned
 * bounding rect, but that's OK since if
 * mStoredList.GetVisibleRect().Contains(untransformedVisible), then it
 * certainly contains the actual (non-axis-aligned) untransformed rect.
 */
nsRegion
nsDisplayTransform::GetOpaqueRegion(nsDisplayListBuilder *aBuilder,
                                             bool* aSnap) const
{
  *aSnap = false;
  nsRect untransformedVisible;
  if (!UntransformBuildingRect(aBuilder, &untransformedVisible)) {
      return nsRegion();
  }

  const Matrix4x4Flagged& matrix = GetTransform();

  nsRegion result;
  Matrix matrix2d;
  bool tmpSnap;
  if (matrix.Is2D(&matrix2d) &&
      matrix2d.PreservesAxisAlignedRectangles() &&
      mStoredList.GetOpaqueRegion(aBuilder, &tmpSnap).Contains(untransformedVisible)) {
    result = GetBuildingRect().Intersect(GetBounds(aBuilder, &tmpSnap));
  }
  return result;
}

/* TransformRect takes in as parameters a rectangle (in app space) and returns
 * the smallest rectangle (in app space) containing the transformed image of
 * that rectangle.  That is, it takes the four corners of the rectangle,
 * transforms them according to the matrix associated with the specified frame,
 * then returns the smallest rectangle containing the four transformed points.
 *
 * @param aUntransformedBounds The rectangle (in app units) to transform.
 * @param aFrame The frame whose transformation should be applied.
 * @param aOrigin The delta from the frame origin to the coordinate space origin
 * @param aBoundsOverride (optional) Force the frame bounds to be the
 *        specified bounds.
 * @return The smallest rectangle containing the image of the transformed
 *         rectangle.
 */
nsRect nsDisplayTransform::TransformRect(const nsRect &aUntransformedBounds,
                                         const nsIFrame* aFrame,
                                         const nsRect* aBoundsOverride)
{
  MOZ_ASSERT(aFrame, "Can't take the transform based on a null frame!");

  float factor = aFrame->PresContext()->AppUnitsPerDevPixel();

  uint32_t flags = INCLUDE_PERSPECTIVE|OFFSET_BY_ORIGIN|INCLUDE_PRESERVE3D_ANCESTORS;
  return nsLayoutUtils::MatrixTransformRect
    (aUntransformedBounds,
     GetResultingTransformMatrix(aFrame, nsPoint(0, 0), factor, flags, aBoundsOverride),
     factor);
}

bool nsDisplayTransform::UntransformRect(const nsRect &aTransformedBounds,
                                         const nsRect &aChildBounds,
                                         const nsIFrame* aFrame,
                                         nsRect *aOutRect)
{
  MOZ_ASSERT(aFrame, "Can't take the transform based on a null frame!");

  float factor = aFrame->PresContext()->AppUnitsPerDevPixel();

  uint32_t flags = INCLUDE_PERSPECTIVE|OFFSET_BY_ORIGIN|INCLUDE_PRESERVE3D_ANCESTORS;

  Matrix4x4 transform = GetResultingTransformMatrix(aFrame, nsPoint(0, 0), factor, flags);
  if (transform.IsSingular()) {
    return false;
  }

  RectDouble result(NSAppUnitsToFloatPixels(aTransformedBounds.x, factor),
                    NSAppUnitsToFloatPixels(aTransformedBounds.y, factor),
                    NSAppUnitsToFloatPixels(aTransformedBounds.width, factor),
                    NSAppUnitsToFloatPixels(aTransformedBounds.height, factor));

  RectDouble childGfxBounds(NSAppUnitsToFloatPixels(aChildBounds.x, factor),
                            NSAppUnitsToFloatPixels(aChildBounds.y, factor),
                            NSAppUnitsToFloatPixels(aChildBounds.width, factor),
                            NSAppUnitsToFloatPixels(aChildBounds.height, factor));

  result = transform.Inverse().ProjectRectBounds(result, childGfxBounds);
  *aOutRect = nsLayoutUtils::RoundGfxRectToAppRect(ThebesRect(result), factor);
  return true;
}

bool nsDisplayTransform::UntransformRect(nsDisplayListBuilder* aBuilder,
                                         const nsRect& aRect,
                                         nsRect *aOutRect) const
{
  if (GetTransform().IsSingular()) {
    return false;
  }

  // GetTransform always operates in dev pixels.
  float factor = mFrame->PresContext()->AppUnitsPerDevPixel();
  RectDouble result(NSAppUnitsToFloatPixels(aRect.x, factor),
                    NSAppUnitsToFloatPixels(aRect.y, factor),
                    NSAppUnitsToFloatPixels(aRect.width, factor),
                    NSAppUnitsToFloatPixels(aRect.height, factor));

  bool snap;
  nsRect childBounds = mStoredList.GetBounds(aBuilder, &snap);
  RectDouble childGfxBounds(NSAppUnitsToFloatPixels(childBounds.x, factor),
                            NSAppUnitsToFloatPixels(childBounds.y, factor),
                            NSAppUnitsToFloatPixels(childBounds.width, factor),
                            NSAppUnitsToFloatPixels(childBounds.height, factor));

  /* We want to untransform the matrix, so invert the transformation first! */
  result = GetInverseTransform().ProjectRectBounds(result, childGfxBounds);

  *aOutRect = nsLayoutUtils::RoundGfxRectToAppRect(ThebesRect(result), factor);

  return true;
}

void
nsDisplayTransform::WriteDebugInfo(std::stringstream& aStream)
{
  AppendToString(aStream, GetTransform().GetMatrix());
  if (IsTransformSeparator()) {
    aStream << " transform-separator";
  }
  if (IsLeafOf3DContext()) {
    aStream << " 3d-context-leaf";
  }
  if (mFrame->Extend3DContext()) {
    aStream << " extends-3d-context";
  }
  if (mFrame->Combines3DTransformWithAncestors()) {
    aStream << " combines-3d-with-ancestors";
  }
}

nsDisplayPerspective::nsDisplayPerspective(nsDisplayListBuilder* aBuilder,
                                           nsIFrame* aFrame,
                                           nsDisplayList* aList)
  : nsDisplayItem(aBuilder, aFrame)
  , mList(aBuilder, aFrame, aList, true)
{
  MOZ_ASSERT(mList.GetChildren()->Count() == 1);
  MOZ_ASSERT(mList.GetChildren()->GetTop()->GetType() == DisplayItemType::TYPE_TRANSFORM);
  mAnimatedGeometryRoot = aBuilder->FindAnimatedGeometryRootFor(mFrame->GetContainingBlock(nsIFrame::SKIP_SCROLLED_FRAME));
}

already_AddRefed<Layer>
nsDisplayPerspective::BuildLayer(nsDisplayListBuilder *aBuilder,
                                 LayerManager *aManager,
                                 const ContainerLayerParameters& aContainerParameters)
{
  float appUnitsPerPixel = mFrame->PresContext()->AppUnitsPerDevPixel();

  Matrix4x4 perspectiveMatrix;
  DebugOnly<bool> hasPerspective =
    nsDisplayTransform::ComputePerspectiveMatrix(mFrame, appUnitsPerPixel,
                                                 perspectiveMatrix);
  MOZ_ASSERT(hasPerspective, "Why did we create nsDisplayPerspective?");

  /*
   * ClipListToRange can remove our child after we were created.
   */
  if (!mList.GetChildren()->GetTop()) {
    return nullptr;
  }

  /*
   * The resulting matrix is still in the coordinate space of the transformed
   * frame. Append a translation to the reference frame coordinates.
   */
  nsDisplayTransform* transform =
    static_cast<nsDisplayTransform*>(mList.GetChildren()->GetTop());

  Point3D newOrigin =
    Point3D(NSAppUnitsToFloatPixels(transform->ToReferenceFrame().x, appUnitsPerPixel),
            NSAppUnitsToFloatPixels(transform->ToReferenceFrame().y, appUnitsPerPixel),
            0.0f);
  Point3D roundedOrigin(NS_round(newOrigin.x),
                        NS_round(newOrigin.y),
                        0);

  perspectiveMatrix.PostTranslate(roundedOrigin);

  RefPtr<ContainerLayer> container = aManager->GetLayerBuilder()->
    BuildContainerLayerFor(aBuilder, aManager, mFrame, this, mList.GetChildren(),
                           aContainerParameters, &perspectiveMatrix, 0);

  if (!container) {
    return nullptr;
  }

  // Sort of a lie, but we want to pretend that the perspective layer extends a 3d context
  // so that it gets its transform combined with children. Might need a better name that reflects
  // this use case and isn't specific to preserve-3d.
  container->SetContentFlags(container->GetContentFlags() | Layer::CONTENT_EXTEND_3D_CONTEXT);
  container->SetTransformIsPerspective(true);

  return container.forget();
}

LayerState
nsDisplayPerspective::GetLayerState(nsDisplayListBuilder* aBuilder,
                                    LayerManager* aManager,
                                    const ContainerLayerParameters& aParameters)
{
  return LAYER_ACTIVE_FORCE;
}

bool
nsDisplayPerspective::CreateWebRenderCommands(mozilla::wr::DisplayListBuilder& aBuilder,
                                              mozilla::wr::IpcResourceUpdateQueue& aResources,
                                              const StackingContextHelper& aSc,
                                              WebRenderLayerManager* aManager,
                                              nsDisplayListBuilder* aDisplayListBuilder)
{
  float appUnitsPerPixel = mFrame->PresContext()->AppUnitsPerDevPixel();
  Matrix4x4 perspectiveMatrix;
  DebugOnly<bool> hasPerspective =
    nsDisplayTransform::ComputePerspectiveMatrix(mFrame, appUnitsPerPixel,
                                                 perspectiveMatrix);
  MOZ_ASSERT(hasPerspective, "Why did we create nsDisplayPerspective?");

  /*
   * ClipListToRange can remove our child after we were created.
   */
  if (!mList.GetChildren()->GetTop()) {
    return false;
  }

  /*
   * The resulting matrix is still in the coordinate space of the transformed
   * frame. Append a translation to the reference frame coordinates.
   */
  nsDisplayTransform* transform =
    static_cast<nsDisplayTransform*>(mList.GetChildren()->GetTop());

  Point3D newOrigin =
    Point3D(NSAppUnitsToFloatPixels(transform->ToReferenceFrame().x, appUnitsPerPixel),
            NSAppUnitsToFloatPixels(transform->ToReferenceFrame().y, appUnitsPerPixel),
            0.0f);
  Point3D roundedOrigin(NS_round(newOrigin.x),
                        NS_round(newOrigin.y),
                        0);

  gfx::Matrix4x4 transformForSC = gfx::Matrix4x4::Translation(roundedOrigin);

  nsTArray<mozilla::wr::WrFilterOp> filters;
  StackingContextHelper sc(aSc,
                           aBuilder,
                           filters,
                           LayoutDeviceRect(),
                           nullptr,
                           nullptr,
                           nullptr,
                           &transformForSC,
                           &perspectiveMatrix,
                           gfx::CompositionOp::OP_OVER,
                           !BackfaceIsHidden(),
                           true);

  return mList.CreateWebRenderCommands(aBuilder, aResources, sc,
                                       aManager, aDisplayListBuilder);
}

nsDisplayItemGeometry*
nsCharClipDisplayItem::AllocateGeometry(nsDisplayListBuilder* aBuilder)
{
  return new nsCharClipGeometry(this, aBuilder);
}

void
nsCharClipDisplayItem::ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                         const nsDisplayItemGeometry* aGeometry,
                                         nsRegion* aInvalidRegion) const
{
  const nsCharClipGeometry* geometry = static_cast<const nsCharClipGeometry*>(aGeometry);

  bool snap;
  nsRect newRect = geometry->mBounds;
  nsRect oldRect = GetBounds(aBuilder, &snap);
  if (mVisIStartEdge != geometry->mVisIStartEdge ||
      mVisIEndEdge != geometry->mVisIEndEdge ||
      !oldRect.IsEqualInterior(newRect) ||
      !geometry->mBorderRect.IsEqualInterior(GetBorderRect())) {
    aInvalidRegion->Or(oldRect, newRect);
  }
}

nsDisplaySVGEffects::nsDisplaySVGEffects(nsDisplayListBuilder* aBuilder,
                                         nsIFrame* aFrame, nsDisplayList* aList,
                                         bool aHandleOpacity,
                                         const ActiveScrolledRoot* aActiveScrolledRoot,
                                         bool aClearClipChain)
  : nsDisplayWrapList(aBuilder, aFrame, aList, aActiveScrolledRoot, aClearClipChain)
  , mHandleOpacity(aHandleOpacity)
{
  MOZ_COUNT_CTOR(nsDisplaySVGEffects);
}

nsDisplaySVGEffects::nsDisplaySVGEffects(nsDisplayListBuilder* aBuilder,
                                         nsIFrame* aFrame, nsDisplayList* aList,
                                         bool aHandleOpacity)
  : nsDisplayWrapList(aBuilder, aFrame, aList)
  , mHandleOpacity(aHandleOpacity)
{
  MOZ_COUNT_CTOR(nsDisplaySVGEffects);
}

#ifdef NS_BUILD_REFCNT_LOGGING
nsDisplaySVGEffects::~nsDisplaySVGEffects()
{
  MOZ_COUNT_DTOR(nsDisplaySVGEffects);
}
#endif

nsRegion nsDisplaySVGEffects::GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                                              bool* aSnap) const
{
  *aSnap = false;
  return nsRegion();
}

void
nsDisplaySVGEffects::HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
                             HitTestState* aState, nsTArray<nsIFrame*> *aOutFrames)
{
  nsPoint rectCenter(aRect.x + aRect.width / 2, aRect.y + aRect.height / 2);
  if (nsSVGIntegrationUtils::HitTestFrameForEffects(mFrame,
      rectCenter - ToReferenceFrame())) {
    mList.HitTest(aBuilder, aRect, aState, aOutFrames);
  }
}

gfxRect
nsDisplaySVGEffects::BBoxInUserSpace() const
{
  return nsSVGUtils::GetBBox(mFrame);
}

gfxPoint
nsDisplaySVGEffects::UserSpaceOffset() const
{
  return nsSVGUtils::FrameSpaceInCSSPxToUserSpaceOffset(mFrame);
}

void
nsDisplaySVGEffects::ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                               const nsDisplayItemGeometry* aGeometry,
                                               nsRegion* aInvalidRegion) const
{
  const nsDisplaySVGEffectGeometry* geometry =
    static_cast<const nsDisplaySVGEffectGeometry*>(aGeometry);
  bool snap;
  nsRect bounds = GetBounds(aBuilder, &snap);
  if (geometry->mFrameOffsetToReferenceFrame != ToReferenceFrame() ||
      geometry->mUserSpaceOffset != UserSpaceOffset() ||
      !geometry->mBBox.IsEqualInterior(BBoxInUserSpace())) {
    // Filter and mask output can depend on the location of the frame's user
    // space and on the frame's BBox. We need to invalidate if either of these
    // change relative to the reference frame.
    // Invalidations from our inactive layer manager are not enough to catch
    // some of these cases because filters can produce output even if there's
    // nothing in the filter input.
    aInvalidRegion->Or(bounds, geometry->mBounds);
  }
}

bool nsDisplaySVGEffects::ValidateSVGFrame()
{
  const nsIContent* content = mFrame->GetContent();
  bool hasSVGLayout = (mFrame->GetStateBits() & NS_FRAME_SVG_LAYOUT);
  if (hasSVGLayout) {
    nsSVGDisplayableFrame* svgFrame = do_QueryFrame(mFrame);
    if (!svgFrame || !mFrame->GetContent()->IsSVGElement()) {
      NS_ASSERTION(false, "why?");
      return false;
    }
    if (!static_cast<const nsSVGElement*>(content)->HasValidDimensions()) {
      return false; // The SVG spec says not to draw filters for this
    }
  }

  return true;
}

static IntRect
ComputeClipExtsInDeviceSpace(gfxContext& aCtx)
{
  // Get the clip extents in device space.
  gfxRect clippedFrameSurfaceRect =
    aCtx.GetClipExtents(gfxContext::eDeviceSpace);
  clippedFrameSurfaceRect.RoundOut();

  IntRect result;
  ToRect(clippedFrameSurfaceRect).ToIntRect(&result);
  return mozilla::gfx::Factory::CheckSurfaceSize(result.Size()) ? result
                                                                : IntRect();
}

typedef nsSVGIntegrationUtils::PaintFramesParams PaintFramesParams;

static void
ComputeMaskGeometry(PaintFramesParams& aParams)
{
  // Properties are added lazily and may have been removed by a restyle, so
  // make sure all applicable ones are set again.
  nsIFrame* firstFrame =
    nsLayoutUtils::FirstContinuationOrIBSplitSibling(aParams.frame);

  const nsStyleSVGReset *svgReset = firstFrame->StyleSVGReset();

  SVGObserverUtils::EffectProperties effectProperties =
    SVGObserverUtils::GetEffectProperties(firstFrame);
  nsTArray<nsSVGMaskFrame *> maskFrames = effectProperties.GetMaskFrames();

  if (maskFrames.Length() == 0) {
    return;
  }

  gfxContext& ctx = aParams.ctx;
  nsIFrame* frame = aParams.frame;

  nsPoint offsetToUserSpace =
      nsLayoutUtils::ComputeOffsetToUserSpace(aParams.builder, aParams.frame);

  gfxPoint devPixelOffsetToUserSpace =
    nsLayoutUtils::PointToGfxPoint(offsetToUserSpace,
                                   frame->PresContext()->AppUnitsPerDevPixel());

  gfxContextMatrixAutoSaveRestore matSR(&ctx);
  ctx.SetMatrixDouble(ctx.CurrentMatrixDouble().PreTranslate(devPixelOffsetToUserSpace));

  // Convert boaderArea and dirtyRect to user space.
  int32_t appUnitsPerDevPixel = frame->PresContext()->AppUnitsPerDevPixel();
  nsRect userSpaceBorderArea = aParams.borderArea - offsetToUserSpace;
  nsRect userSpaceDirtyRect = aParams.dirtyRect - offsetToUserSpace;

  // Union all mask layer rectangles in user space.
  gfxRect maskInUserSpace;
  for (size_t i = 0; i < maskFrames.Length() ; i++) {
    nsSVGMaskFrame* maskFrame = maskFrames[i];
    gfxRect currentMaskSurfaceRect;

    if (maskFrame) {
      currentMaskSurfaceRect = maskFrame->GetMaskArea(aParams.frame);
    } else {
      nsCSSRendering::ImageLayerClipState clipState;
      nsCSSRendering::GetImageLayerClip(svgReset->mMask.mLayers[i],
                                       frame,
                                       *frame->StyleBorder(),
                                       userSpaceBorderArea,
                                       userSpaceDirtyRect,
                                       false, /* aWillPaintBorder */
                                       appUnitsPerDevPixel,
                                       &clipState);
      currentMaskSurfaceRect = clipState.mDirtyRectInDevPx;
    }

    maskInUserSpace = maskInUserSpace.Union(currentMaskSurfaceRect);
  }

  gfxContextAutoSaveRestore autoSR;

  if (!maskInUserSpace.IsEmpty()) {
    autoSR.SetContext(&ctx);
    ctx.Clip(maskInUserSpace);
  }

  IntRect result = ComputeClipExtsInDeviceSpace(ctx);
  aParams.maskRect = result;
}

nsDisplayMask::nsDisplayMask(nsDisplayListBuilder* aBuilder,
                             nsIFrame* aFrame, nsDisplayList* aList,
                             bool aHandleOpacity,
                             const ActiveScrolledRoot* aActiveScrolledRoot)
  : nsDisplaySVGEffects(aBuilder, aFrame, aList, aHandleOpacity, aActiveScrolledRoot, true)
{
  MOZ_COUNT_CTOR(nsDisplayMask);

  nsPresContext* presContext = mFrame->PresContext();
  uint32_t flags = aBuilder->GetBackgroundPaintFlags() |
                   nsCSSRendering::PAINTBG_MASK_IMAGE;
  const nsStyleSVGReset *svgReset = aFrame->StyleSVGReset();
  NS_FOR_VISIBLE_IMAGE_LAYERS_BACK_TO_FRONT(i, svgReset->mMask) {
    if (!svgReset->mMask.mLayers[i].mImage.IsResolved()) {
      continue;
    }
    bool isTransformedFixed;
    nsBackgroundLayerState state =
      nsCSSRendering::PrepareImageLayer(presContext, aFrame, flags,
                                        mFrame->GetRectRelativeToSelf(),
                                        mFrame->GetRectRelativeToSelf(),
                                        svgReset->mMask.mLayers[i],
                                        &isTransformedFixed);
    mDestRects.AppendElement(state.mDestArea);
  }
}

#ifdef NS_BUILD_REFCNT_LOGGING
nsDisplayMask::~nsDisplayMask()
{
  MOZ_COUNT_DTOR(nsDisplayMask);
}
#endif

static bool
CanMergeDisplayMaskFrame(nsIFrame* aFrame)
{
  // Do not merge items for box-decoration-break:clone elements,
  // since each box should have its own mask in that case.
  if (aFrame->StyleBorder()->mBoxDecorationBreak ==
        mozilla::StyleBoxDecorationBreak::Clone) {
    return false;
  }

  // Do not merge if either frame has a mask. Continuation frames should apply
  // the mask independently (just like nsDisplayBackgroundImage).
  if (aFrame->StyleSVGReset()->HasMask()) {
    return false;
  }

  return true;
}

bool
nsDisplayMask::CanMerge(const nsDisplayItem* aItem) const
{
  // Items for the same content element should be merged into a single
  // compositing group.
  if (!HasSameTypeAndClip(aItem) || !HasSameContent(aItem)) {
    return false;
  }

  return CanMergeDisplayMaskFrame(mFrame) &&
         CanMergeDisplayMaskFrame(aItem->Frame());
}

already_AddRefed<Layer>
nsDisplayMask::BuildLayer(nsDisplayListBuilder* aBuilder,
                          LayerManager* aManager,
                          const ContainerLayerParameters& aContainerParameters)
{
  if (!ValidateSVGFrame()) {
    return nullptr;
  }

  if (mFrame->StyleEffects()->mOpacity == 0.0f && mHandleOpacity) {
    return nullptr;
  }

  nsIFrame* firstFrame =
    nsLayoutUtils::FirstContinuationOrIBSplitSibling(mFrame);
  SVGObserverUtils::EffectProperties effectProperties =
    SVGObserverUtils::GetEffectProperties(firstFrame);

  if (effectProperties.HasInvalidClipPath() ||
      effectProperties.HasInvalidMask()) {
    return nullptr;
  }

  RefPtr<ContainerLayer> container = aManager->GetLayerBuilder()->
    BuildContainerLayerFor(aBuilder, aManager, mFrame, this, &mList,
                           aContainerParameters, nullptr);

  return container.forget();
}

bool
nsDisplayMask::PaintMask(nsDisplayListBuilder* aBuilder,
                         gfxContext* aMaskContext)
{
  MOZ_ASSERT(aMaskContext->GetDrawTarget()->GetFormat() == SurfaceFormat::A8);

  imgDrawingParams imgParmas(aBuilder->ShouldSyncDecodeImages()
                             ? imgIContainer::FLAG_SYNC_DECODE
                             : imgIContainer::FLAG_SYNC_DECODE_IF_FAST);
  nsRect borderArea = nsRect(ToReferenceFrame(), mFrame->GetSize());
  nsSVGIntegrationUtils::PaintFramesParams params(*aMaskContext,
                                                  mFrame,  GetBuildingRect(),
                                                  borderArea, aBuilder,
                                                  nullptr,
                                                  mHandleOpacity, imgParmas);
  ComputeMaskGeometry(params);
  nsSVGIntegrationUtils::PaintMask(params);

  nsDisplayMaskGeometry::UpdateDrawResult(this, imgParmas.result);

  return imgParmas.result == mozilla::image::ImgDrawResult::SUCCESS;
}

LayerState
nsDisplayMask::GetLayerState(nsDisplayListBuilder* aBuilder,
                             LayerManager* aManager,
                             const ContainerLayerParameters& aParameters)
{
  if (CanPaintOnMaskLayer(aManager)) {
    LayerState result = RequiredLayerStateForChildren(aBuilder, aManager,
        aParameters, mList, GetAnimatedGeometryRoot());
    // When we're not active, FrameLayerBuilder will call PaintAsLayer()
    // on us during painting. In that case we don't want a mask layer to
    // be created, because PaintAsLayer() takes care of applying the mask.
    // So we return LAYER_SVG_EFFECTS instead of LAYER_INACTIVE so that
    // FrameLayerBuilder doesn't set a mask layer on our layer.
    return result == LAYER_INACTIVE ? LAYER_SVG_EFFECTS : result;
  }

  return LAYER_SVG_EFFECTS;
}

bool nsDisplayMask::CanPaintOnMaskLayer(LayerManager* aManager)
{
  if (!nsSVGIntegrationUtils::IsMaskResourceReady(mFrame)) {
    return false;
  }

  if (gfxPrefs::DrawMaskLayer()) {
    return false;
  }

  return true;
}

bool nsDisplayMask::ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                      nsRegion* aVisibleRegion)
{
  // Our children may be made translucent or arbitrarily deformed so we should
  // not allow them to subtract area from aVisibleRegion.
  nsRegion childrenVisible(GetPaintRect());
  nsRect r = GetPaintRect().Intersect(mList.GetBounds(aBuilder));
  mList.ComputeVisibilityForSublist(aBuilder, &childrenVisible, r);
  return true;
}

void
nsDisplayMask::ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                         const nsDisplayItemGeometry* aGeometry,
                                         nsRegion* aInvalidRegion) const
{
  nsDisplaySVGEffects::ComputeInvalidationRegion(aBuilder, aGeometry,
                                                 aInvalidRegion);

  const nsDisplayMaskGeometry* geometry =
    static_cast<const nsDisplayMaskGeometry*>(aGeometry);
  bool snap;
  nsRect bounds = GetBounds(aBuilder, &snap);

  if (mFrame->StyleEffects()->mOpacity != geometry->mOpacity ||
      mHandleOpacity != geometry->mHandleOpacity) {
    aInvalidRegion->Or(*aInvalidRegion, bounds);
  }

  if (mDestRects.Length() != geometry->mDestRects.Length()) {
    aInvalidRegion->Or(bounds, geometry->mBounds);
  } else {
    for (size_t i = 0; i < mDestRects.Length(); i++) {
      if (!mDestRects[i].IsEqualInterior(geometry->mDestRects[i])) {
        aInvalidRegion->Or(bounds, geometry->mBounds);
        break;
      }
    }
  }

  if (aBuilder->ShouldSyncDecodeImages() &&
      geometry->ShouldInvalidateToSyncDecodeImages()) {
    const nsStyleSVGReset *svgReset = mFrame->StyleSVGReset();
    NS_FOR_VISIBLE_IMAGE_LAYERS_BACK_TO_FRONT(i, svgReset->mMask) {
      const nsStyleImage& image = svgReset->mMask.mLayers[i].mImage;
      if (image.GetType() == eStyleImageType_Image ) {
        aInvalidRegion->Or(*aInvalidRegion, bounds);
        break;
      }
    }
  }
}

void
nsDisplayMask::PaintAsLayer(nsDisplayListBuilder* aBuilder,
                            gfxContext* aCtx,
                            LayerManager* aManager)
{
  // Clip the drawing target by mVisibleRect, which contains the visible
  // region of the target frame and its out-of-flow and inflow descendants.
  gfxContext* context = aCtx;

  Rect bounds =
    NSRectToRect(GetPaintRect(), mFrame->PresContext()->AppUnitsPerDevPixel());
  bounds.RoundOut();
  context->Clip(bounds);

  imgDrawingParams imgParams(aBuilder->ShouldSyncDecodeImages()
                             ? imgIContainer::FLAG_SYNC_DECODE
                             : imgIContainer::FLAG_SYNC_DECODE_IF_FAST);
  nsRect borderArea = nsRect(ToReferenceFrame(), mFrame->GetSize());
  nsSVGIntegrationUtils::PaintFramesParams params(*aCtx,
                                                  mFrame,  GetPaintRect(),
                                                  borderArea, aBuilder,
                                                  aManager,
                                                  mHandleOpacity, imgParams);

  ComputeMaskGeometry(params);

  nsSVGIntegrationUtils::PaintMaskAndClipPath(params);

  context->PopClip();

  nsDisplayMaskGeometry::UpdateDrawResult(this, imgParams.result);
}

bool
nsDisplayMask::CreateWebRenderCommands(mozilla::wr::DisplayListBuilder& aBuilder,
                                       mozilla::wr::IpcResourceUpdateQueue& aResources,
                                       const StackingContextHelper& aSc,
                                       mozilla::layers::WebRenderLayerManager* aManager,
                                       nsDisplayListBuilder* aDisplayListBuilder)
{
  bool snap;
  float appUnitsPerDevPixel = mFrame->PresContext()->AppUnitsPerDevPixel();
  nsRect displayBound = GetBounds(aDisplayListBuilder, &snap);
  LayoutDeviceRect bounds = LayoutDeviceRect::FromAppUnits(displayBound, appUnitsPerDevPixel);

  Maybe<wr::WrImageMask> mask = aManager->CommandBuilder().BuildWrMaskImage(this, aBuilder, aResources,
                                                                            aSc, aDisplayListBuilder,
                                                                            bounds);
  Maybe<StackingContextHelper> layer;
  const StackingContextHelper* sc = &aSc;
  if (mask) {
    auto layoutBounds = wr::ToRoundedLayoutRect(bounds);
    wr::WrClipId clipId = aBuilder.DefineClip(Nothing(),
        layoutBounds, nullptr, mask.ptr());

    // Create a new stacking context to attach the mask to, ensuring the mask is
    // applied to the aggregate, and not the individual elements.

    // The stacking context shouldn't have any offset.
    bounds.MoveTo(0, 0);

    layer.emplace(aSc,
                  aBuilder,
                  /*aFilters: */ nsTArray<wr::WrFilterOp>(),
                  /*aBounds: */ bounds,
                  /*aBoundTransform: */ nullptr,
                  /*aAnimation: */ nullptr,
                  /*aOpacity: */ nullptr,
                  /*aTransform: */ nullptr,
                  /*aPerspective: */ nullptr,
                  /*aMixBlendMode: */ gfx::CompositionOp::OP_OVER,
                  /*aBackfaceVisible: */ true,
                  /*aIsPreserve3D: */ false,
                  /*aTransformForScrollData: */ Nothing(),
                  /*aClipNodeId: */ &clipId);
    sc = layer.ptr();
    aManager->CommandBuilder().PushOverrideForASR(GetActiveScrolledRoot(), Some(clipId));
  }

  nsDisplaySVGEffects::CreateWebRenderCommands(aBuilder, aResources, *sc, aManager, aDisplayListBuilder);

  if (mask) {
    aManager->CommandBuilder().PopOverrideForASR(GetActiveScrolledRoot());
  }

  return true;
}

Maybe<nsRect>
nsDisplayMask::GetClipWithRespectToASR(nsDisplayListBuilder* aBuilder,
                                       const ActiveScrolledRoot* aASR) const
{
  if (const DisplayItemClip* clip = DisplayItemClipChain::ClipForASR(GetClipChain(), aASR)) {
    return Some(clip->GetClipRect());
  }
  // This item does not have a clip with respect to |aASR|. However, we
  // might still have finite bounds with respect to |aASR|. Check our
  // children.
  nsDisplayList* childList = GetSameCoordinateSystemChildren();
  if (childList) {
    return Some(childList->GetClippedBoundsWithRespectToASR(aBuilder, aASR));
  }
#ifdef DEBUG
  if (!gfxPrefs::LayoutUseContainersForRootFrames()) {
    MOZ_ASSERT(false, "item should have finite clip with respect to aASR");
  }
#endif
  return Nothing();
}



#ifdef MOZ_DUMP_PAINTING
void
nsDisplayMask::PrintEffects(nsACString& aTo)
{
  nsIFrame* firstFrame =
    nsLayoutUtils::FirstContinuationOrIBSplitSibling(mFrame);
  SVGObserverUtils::EffectProperties effectProperties =
    SVGObserverUtils::GetEffectProperties(firstFrame);
  nsSVGClipPathFrame *clipPathFrame = effectProperties.GetClipPathFrame();
  bool first = true;
  aTo += " effects=(";
  if (mFrame->StyleEffects()->mOpacity != 1.0f && mHandleOpacity) {
    first = false;
    aTo += nsPrintfCString("opacity(%f)", mFrame->StyleEffects()->mOpacity);
  }
  if (clipPathFrame) {
    if (!first) {
      aTo += ", ";
    }
    aTo += nsPrintfCString("clip(%s)", clipPathFrame->IsTrivial() ? "trivial" : "non-trivial");
    first = false;
  }
  const nsStyleSVGReset *style = mFrame->StyleSVGReset();
  if (style->HasClipPath() && !clipPathFrame) {
    if (!first) {
      aTo += ", ";
    }
    aTo += "clip(basic-shape)";
    first = false;
  }

  nsTArray<nsSVGMaskFrame*> masks = effectProperties.GetMaskFrames();
  if (!masks.IsEmpty() && masks[0]) {
    if (!first) {
      aTo += ", ";
    }
    aTo += "mask";
  }
  aTo += ")";
}
#endif

nsDisplayFilter::nsDisplayFilter(nsDisplayListBuilder* aBuilder,
                                 nsIFrame* aFrame, nsDisplayList* aList,
                                 bool aHandleOpacity)
  : nsDisplaySVGEffects(aBuilder, aFrame, aList, aHandleOpacity),
    mEffectsBounds(aFrame->GetVisualOverflowRectRelativeToSelf())
{
  MOZ_COUNT_CTOR(nsDisplayFilter);
}

#ifdef NS_BUILD_REFCNT_LOGGING
nsDisplayFilter::~nsDisplayFilter()
{
  MOZ_COUNT_DTOR(nsDisplayFilter);
}
#endif

already_AddRefed<Layer>
nsDisplayFilter::BuildLayer(nsDisplayListBuilder* aBuilder,
                           LayerManager* aManager,
                           const ContainerLayerParameters& aContainerParameters)
{
  if (!ValidateSVGFrame()) {
    return nullptr;
  }

  if (mFrame->StyleEffects()->mOpacity == 0.0f && mHandleOpacity) {
    return nullptr;
  }

  nsIFrame* firstFrame =
    nsLayoutUtils::FirstContinuationOrIBSplitSibling(mFrame);
  SVGObserverUtils::EffectProperties effectProperties =
    SVGObserverUtils::GetEffectProperties(firstFrame);

  if (effectProperties.HasInvalidFilter()) {
    return nullptr;
  }

  MOZ_ASSERT(effectProperties.mFilter && mFrame->StyleEffects()->HasFilters(),
             "By getting here, we must have valid CSS filters.");

  ContainerLayerParameters newContainerParameters = aContainerParameters;
  newContainerParameters.mDisableSubpixelAntialiasingInDescendants = true;

  RefPtr<ContainerLayer> container = aManager->GetLayerBuilder()->
    BuildContainerLayerFor(aBuilder, aManager, mFrame, this, &mList,
                           newContainerParameters, nullptr);
  return container.forget();
}

LayerState
nsDisplayFilter::GetLayerState(nsDisplayListBuilder* aBuilder,
                               LayerManager* aManager,
                               const ContainerLayerParameters& aParameters)
{
  return LAYER_SVG_EFFECTS;
}

bool
nsDisplayFilter::ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                   nsRegion* aVisibleRegion)
{
  nsPoint offset = ToReferenceFrame();
  nsRect dirtyRect =
    nsSVGIntegrationUtils::GetRequiredSourceForInvalidArea(mFrame,
                                                           GetPaintRect() - offset) +
    offset;

  // Our children may be made translucent or arbitrarily deformed so we should
  // not allow them to subtract area from aVisibleRegion.
  nsRegion childrenVisible(dirtyRect);
  nsRect r = dirtyRect.Intersect(
    mList.GetClippedBoundsWithRespectToASR(aBuilder, mActiveScrolledRoot));
  mList.ComputeVisibilityForSublist(aBuilder, &childrenVisible, r);
  return true;
}

void
nsDisplayFilter::ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                           const nsDisplayItemGeometry* aGeometry,
                                           nsRegion* aInvalidRegion) const
{
  nsDisplaySVGEffects::ComputeInvalidationRegion(aBuilder, aGeometry,
                                                 aInvalidRegion);

  const nsDisplayFilterGeometry* geometry =
    static_cast<const nsDisplayFilterGeometry*>(aGeometry);

  if (aBuilder->ShouldSyncDecodeImages() &&
      geometry->ShouldInvalidateToSyncDecodeImages()) {
    bool snap;
    nsRect bounds = GetBounds(aBuilder, &snap);
    aInvalidRegion->Or(*aInvalidRegion, bounds);
  }
}

void
nsDisplayFilter::PaintAsLayer(nsDisplayListBuilder* aBuilder,
                              gfxContext* aCtx,
                              LayerManager* aManager)
{
  imgDrawingParams imgParams(aBuilder->ShouldSyncDecodeImages()
                             ? imgIContainer::FLAG_SYNC_DECODE
                             : imgIContainer::FLAG_SYNC_DECODE_IF_FAST);
  nsRect borderArea = nsRect(ToReferenceFrame(), mFrame->GetSize());
  nsSVGIntegrationUtils::PaintFramesParams params(*aCtx,
                                                  mFrame,  GetPaintRect(),
                                                  borderArea, aBuilder,
                                                  aManager,
                                                  mHandleOpacity, imgParams);
  nsSVGIntegrationUtils::PaintFilter(params);
  nsDisplayFilterGeometry::UpdateDrawResult(this, imgParams.result);
}

static float
ClampStdDeviation(float aStdDeviation)
{
  // Cap software blur radius for performance reasons.
  return std::min(std::max(0.0f, aStdDeviation), 100.0f);
}

bool
nsDisplayFilter::CreateWebRenderCommands(mozilla::wr::DisplayListBuilder& aBuilder,
                                         mozilla::wr::IpcResourceUpdateQueue& aResources,
                                         const StackingContextHelper& aSc,
                                         mozilla::layers::WebRenderLayerManager* aManager,
                                         nsDisplayListBuilder* aDisplayListBuilder)
{
  if (mFrame->IsFrameOfType(nsIFrame::eSVG)) {
    return false;
  }

  // Due to differences in the way that WebRender filters operate
  // only the brightness and contrast filters use that path. We
  // can gradually enable more filters as WebRender bugs are fixed.
  nsTArray<mozilla::wr::WrFilterOp> wrFilters;
  const nsTArray<nsStyleFilter>& filters = mFrame->StyleEffects()->mFilters;
  for (const nsStyleFilter& filter : filters) {
    switch (filter.GetType()) {
      case NS_STYLE_FILTER_BRIGHTNESS:
      case NS_STYLE_FILTER_CONTRAST:
      case NS_STYLE_FILTER_GRAYSCALE:
      case NS_STYLE_FILTER_INVERT:
      case NS_STYLE_FILTER_OPACITY:
      case NS_STYLE_FILTER_SATURATE:
      case NS_STYLE_FILTER_SEPIA: {
        mozilla::wr::WrFilterOp filterOp = {
          wr::ToWrFilterOpType(filter.GetType()),
          filter.GetFilterParameter().GetFactorOrPercentValue(),
        };
        wrFilters.AppendElement(filterOp);
        break;
      }
    case NS_STYLE_FILTER_HUE_ROTATE: {
        mozilla::wr::WrFilterOp filterOp = {
          wr::ToWrFilterOpType(filter.GetType()),
          (float)filter.GetFilterParameter().GetAngleValueInDegrees(),
        };
        wrFilters.AppendElement(filterOp);
        break;
      }
      case NS_STYLE_FILTER_BLUR: {
        float appUnitsPerDevPixel = mFrame->PresContext()->AppUnitsPerDevPixel();
        mozilla::wr::WrFilterOp filterOp = {
          wr::ToWrFilterOpType(filter.GetType()),
          ClampStdDeviation(
            NSAppUnitsToFloatPixels(
              filter.GetFilterParameter().GetCoordValue(),
              appUnitsPerDevPixel)),
        };
        wrFilters.AppendElement(filterOp);
        break;
      }
      case NS_STYLE_FILTER_DROP_SHADOW: {
        float appUnitsPerDevPixel = mFrame->PresContext()->AppUnitsPerDevPixel();
        nsCSSShadowArray* shadows = filter.GetDropShadow();
        if (!shadows || shadows->Length() != 1) {
          MOZ_ASSERT_UNREACHABLE("Exactly one drop shadow should have been "
                                 "parsed.");
          return false;
        }

        nsCSSShadowItem* shadow = shadows->ShadowAt(0);
        nscolor color = shadow->mColor.CalcColor(mFrame);

        mozilla::wr::WrFilterOp filterOp = {
          wr::ToWrFilterOpType(filter.GetType()),
          NSAppUnitsToFloatPixels(shadow->mRadius, appUnitsPerDevPixel),
          {
            NSAppUnitsToFloatPixels(shadow->mXOffset, appUnitsPerDevPixel),
            NSAppUnitsToFloatPixels(shadow->mYOffset, appUnitsPerDevPixel),
          },
          {
            NS_GET_R(color) / 255.0f,
            NS_GET_G(color) / 255.0f,
            NS_GET_B(color) / 255.0f,
            NS_GET_A(color) / 255.0f,
          }
        };

        wrFilters.AppendElement(filterOp);
        break;
      }
      default:
        return false;
    }
  }

  float opacity = mFrame->StyleEffects()->mOpacity;
  StackingContextHelper sc(aSc, aBuilder, wrFilters, LayoutDeviceRect(), nullptr,
                           nullptr, opacity != 1.0f && mHandleOpacity ? &opacity : nullptr);

  nsDisplaySVGEffects::CreateWebRenderCommands(aBuilder, aResources, sc, aManager, aDisplayListBuilder);
  return true;
}

#ifdef MOZ_DUMP_PAINTING
void
nsDisplayFilter::PrintEffects(nsACString& aTo)
{
  nsIFrame* firstFrame =
    nsLayoutUtils::FirstContinuationOrIBSplitSibling(mFrame);
  SVGObserverUtils::EffectProperties effectProperties =
    SVGObserverUtils::GetEffectProperties(firstFrame);
  bool first = true;
  aTo += " effects=(";
  if (mFrame->StyleEffects()->mOpacity != 1.0f && mHandleOpacity) {
    first = false;
    aTo += nsPrintfCString("opacity(%f)", mFrame->StyleEffects()->mOpacity);
  }
  if (effectProperties.HasValidFilter()) {
    if (!first) {
      aTo += ", ";
    }
    aTo += "filter";
  }
  aTo += ")";
}
#endif

nsDisplaySVGWrapper::nsDisplaySVGWrapper(nsDisplayListBuilder* aBuilder,
                                         nsIFrame* aFrame, nsDisplayList* aList)
    : nsDisplayWrapList(aBuilder, aFrame, aList)
{
  MOZ_COUNT_CTOR(nsDisplaySVGWrapper);
}

#ifdef NS_BUILD_REFCNT_LOGGING
nsDisplaySVGWrapper::~nsDisplaySVGWrapper() {
  MOZ_COUNT_DTOR(nsDisplaySVGWrapper);
}
#endif

LayerState
nsDisplaySVGWrapper::GetLayerState(nsDisplayListBuilder* aBuilder,
                                   LayerManager* aManager,
                                   const ContainerLayerParameters& aParameters)
{
  RefPtr<LayerManager> layerManager = aBuilder->GetWidgetLayerManager();
  if (layerManager && layerManager->GetBackendType() == layers::LayersBackend::LAYERS_WR) {
    return LAYER_ACTIVE_FORCE;
  }
  return LAYER_NONE;
}

bool
nsDisplaySVGWrapper::ShouldFlattenAway(nsDisplayListBuilder* aBuilder)
{
  RefPtr<LayerManager> layerManager = aBuilder->GetWidgetLayerManager();
  if (layerManager && layerManager->GetBackendType() == layers::LayersBackend::LAYERS_WR) {
    return false;
  }
  return true;
}

already_AddRefed<Layer>
nsDisplaySVGWrapper::BuildLayer(nsDisplayListBuilder* aBuilder,
                                LayerManager* aManager,
                                const ContainerLayerParameters& aContainerParameters)
{
  ContainerLayerParameters newContainerParameters = aContainerParameters;
  newContainerParameters.mDisableSubpixelAntialiasingInDescendants = true;

  RefPtr<ContainerLayer> container = aManager->GetLayerBuilder()->
    BuildContainerLayerFor(aBuilder, aManager, mFrame, this, &mList,
                           newContainerParameters, nullptr);

  return container.forget();
}

bool
nsDisplaySVGWrapper::CreateWebRenderCommands(mozilla::wr::DisplayListBuilder& aBuilder,
                                             mozilla::wr::IpcResourceUpdateQueue& aResources,
                                             const StackingContextHelper& aSc,
                                             mozilla::layers::WebRenderLayerManager* aManager,
                                             nsDisplayListBuilder* aDisplayListBuilder)
{
  if (gfxPrefs::WebRenderBlobInvalidation()) {
    return nsDisplayWrapList::CreateWebRenderCommands(aBuilder,
                                             aResources,
                                             aSc,
                                             aManager,
                                             aDisplayListBuilder);
  } else {
    return false;
  }
}

namespace mozilla {

uint32_t PaintTelemetry::sPaintLevel = 0;
uint32_t PaintTelemetry::sMetricLevel = 0;
EnumeratedArray<PaintTelemetry::Metric,
                PaintTelemetry::Metric::COUNT,
                double> PaintTelemetry::sMetrics;

PaintTelemetry::AutoRecordPaint::AutoRecordPaint()
{
  // Don't record nested paints.
  if (sPaintLevel++ > 0) {
    return;
  }

  // Reset metrics for a new paint.
  for (auto& metric : sMetrics) {
    metric = 0.0;
  }
  mStart = TimeStamp::Now();
}

PaintTelemetry::AutoRecordPaint::~AutoRecordPaint()
{
  MOZ_ASSERT(sPaintLevel != 0);
  if (--sPaintLevel > 0) {
    return;
  }

  // If we're in multi-process mode, don't include paint times for the parent
  // process.
  if (gfxVars::BrowserTabsRemoteAutostart() && XRE_IsParentProcess()) {
    return;
  }

  double totalMs = (TimeStamp::Now() - mStart).ToMilliseconds();

  // Record the total time.
  Telemetry::Accumulate(Telemetry::CONTENT_PAINT_TIME, static_cast<uint32_t>(totalMs));

  // Helpers for recording large/small paints.
  auto recordLarge = [=](const nsCString& aKey, double aDurationMs) -> void {
    MOZ_ASSERT(aDurationMs <= totalMs);
    uint32_t amount = static_cast<int32_t>((aDurationMs / totalMs) * 100.0);
    Telemetry::Accumulate(Telemetry::CONTENT_LARGE_PAINT_PHASE_WEIGHT, aKey, amount);
  };
  auto recordSmall = [=](const nsCString& aKey, double aDurationMs) -> void {
    MOZ_ASSERT(aDurationMs <= totalMs);
    uint32_t amount = static_cast<int32_t>((aDurationMs / totalMs) * 100.0);
    Telemetry::Accumulate(Telemetry::CONTENT_SMALL_PAINT_PHASE_WEIGHT, aKey, amount);
  };

  double dlMs = sMetrics[Metric::DisplayList];
  double flbMs = sMetrics[Metric::Layerization];
  double frMs = sMetrics[Metric::FlushRasterization];
  double rMs = sMetrics[Metric::Rasterization];

  // If the total time was >= 16ms, then it's likely we missed a frame due to
  // painting. We bucket these metrics separately.
  if (totalMs >= 16.0) {
    recordLarge(NS_LITERAL_CSTRING("dl"), dlMs);
    recordLarge(NS_LITERAL_CSTRING("flb"), flbMs);
    recordLarge(NS_LITERAL_CSTRING("fr"), frMs);
    recordLarge(NS_LITERAL_CSTRING("r"), rMs);
  } else {
    recordSmall(NS_LITERAL_CSTRING("dl"), dlMs);
    recordSmall(NS_LITERAL_CSTRING("flb"), flbMs);
    recordSmall(NS_LITERAL_CSTRING("fr"), frMs);
    recordSmall(NS_LITERAL_CSTRING("r"), rMs);
  }
}

PaintTelemetry::AutoRecord::AutoRecord(Metric aMetric)
 : mMetric(aMetric)
{
  // Don't double-record anything nested.
  if (sMetricLevel++ > 0) {
    return;
  }

  // Don't record inside nested paints, or outside of paints.
  if (sPaintLevel != 1) {
    return;
  }

  mStart = TimeStamp::Now();
}

PaintTelemetry::AutoRecord::~AutoRecord()
{
  MOZ_ASSERT(sMetricLevel != 0);

  sMetricLevel--;
  if (mStart.IsNull()) {
    return;
  }

  sMetrics[mMetric] += (TimeStamp::Now() - mStart).ToMilliseconds();
}

} // namespace mozilla
