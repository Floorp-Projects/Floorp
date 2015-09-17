/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=2 sw=2 et tw=78:
 * This Source Code Form is subject to the terms of the Mozilla Public
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

#include "gfxUtils.h"
#include "mozilla/dom/TabChild.h"
#include "mozilla/dom/KeyframeEffect.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/layers/PLayerTransaction.h"
#include "nsCSSRendering.h"
#include "nsRenderingContext.h"
#include "nsISelectionController.h"
#include "nsIPresShell.h"
#include "nsRegion.h"
#include "nsStyleStructInlines.h"
#include "nsStyleTransformMatrix.h"
#include "gfxMatrix.h"
#include "gfxPrefs.h"
#include "gfxVR.h"
#include "nsSVGIntegrationUtils.h"
#include "nsSVGUtils.h"
#include "nsLayoutUtils.h"
#include "nsIScrollableFrame.h"
#include "nsIFrameInlines.h"
#include "nsThemeConstants.h"
#include "LayerTreeInvalidation.h"

#include "imgIContainer.h"
#include "BasicLayers.h"
#include "nsBoxFrame.h"
#include "nsViewportFrame.h"
#include "nsSubDocumentFrame.h"
#include "nsSVGEffects.h"
#include "nsSVGElement.h"
#include "nsSVGClipPathFrame.h"
#include "GeckoProfiler.h"
#include "nsAnimationManager.h"
#include "nsTransitionManager.h"
#include "nsViewManager.h"
#include "ImageLayers.h"
#include "ImageContainer.h"
#include "nsCanvasFrame.h"
#include "StickyScrollContainer.h"
#include "mozilla/EventStates.h"
#include "mozilla/LookAndFeel.h"
#include "mozilla/PendingAnimationTracker.h"
#include "mozilla/Preferences.h"
#include "mozilla/UniquePtr.h"
#include "ActiveLayerTracker.h"
#include "nsContentUtils.h"
#include "nsPrintfCString.h"
#include "UnitTransforms.h"
#include "LayersLogging.h"
#include "FrameLayerBuilder.h"
#include "mozilla/EventStateManager.h"
#include "RestyleManager.h"
#include "nsCaret.h"
#include "nsISelection.h"
#include "nsDOMTokenList.h"
#include "mozilla/RuleNodeCacheConditions.h"
#include "nsCSSProps.h"

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

static inline nsIFrame*
GetTransformRootFrame(nsIFrame* aFrame)
{
  return nsLayoutUtils::GetTransformRootFrame(aFrame);
}

static inline CSSAngle
MakeCSSAngle(const nsCSSValue& aValue)
{
  return CSSAngle(aValue.GetAngleValue(), aValue.GetUnit());
}

static void AddTransformFunctions(nsCSSValueList* aList,
                                  nsStyleContext* aContext,
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
    RuleNodeCacheConditions conditions;
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
          array->Item(1), aContext, aPresContext, conditions,
          &aRefBox, &TransformReferenceBox::Width);
        aFunctions.AppendElement(Translation(x, 0, 0));
        break;
      }
      case eCSSKeyword_translatey:
      {
        double y = nsStyleTransformMatrix::ProcessTranslatePart(
          array->Item(1), aContext, aPresContext, conditions,
          &aRefBox, &TransformReferenceBox::Height);
        aFunctions.AppendElement(Translation(0, y, 0));
        break;
      }
      case eCSSKeyword_translatez:
      {
        double z = nsStyleTransformMatrix::ProcessTranslatePart(
          array->Item(1), aContext, aPresContext, conditions,
          nullptr);
        aFunctions.AppendElement(Translation(0, 0, z));
        break;
      }
      case eCSSKeyword_translate:
      {
        double x = nsStyleTransformMatrix::ProcessTranslatePart(
          array->Item(1), aContext, aPresContext, conditions,
          &aRefBox, &TransformReferenceBox::Width);
        // translate(x) is shorthand for translate(x, 0)
        double y = 0;
        if (array->Count() == 3) {
           y = nsStyleTransformMatrix::ProcessTranslatePart(
            array->Item(2), aContext, aPresContext, conditions,
            &aRefBox, &TransformReferenceBox::Height);
        }
        aFunctions.AppendElement(Translation(x, y, 0));
        break;
      }
      case eCSSKeyword_translate3d:
      {
        double x = nsStyleTransformMatrix::ProcessTranslatePart(
          array->Item(1), aContext, aPresContext, conditions,
          &aRefBox, &TransformReferenceBox::Width);
        double y = nsStyleTransformMatrix::ProcessTranslatePart(
          array->Item(2), aContext, aPresContext, conditions,
          &aRefBox, &TransformReferenceBox::Height);
        double z = nsStyleTransformMatrix::ProcessTranslatePart(
          array->Item(3), aContext, aPresContext, conditions,
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
        matrix._41 = array->Item(5).GetFloatValue();
        matrix._42 = array->Item(6).GetFloatValue();
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
        matrix._41 = array->Item(13).GetFloatValue();
        matrix._42 = array->Item(14).GetFloatValue();
        matrix._43 = array->Item(15).GetFloatValue();
        matrix._44 = array->Item(16).GetFloatValue();
        aFunctions.AppendElement(TransformMatrix(matrix));
        break;
      }
      case eCSSKeyword_interpolatematrix:
      {
        Matrix4x4 matrix;
        nsStyleTransformMatrix::ProcessInterpolateMatrix(matrix, array,
                                                         aContext,
                                                         aPresContext,
                                                         conditions,
                                                         aRefBox);
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

static TimingFunction
ToTimingFunction(const ComputedTimingFunction& aCTF)
{
  if (aCTF.GetType() == nsTimingFunction::Function) {
    const nsSMILKeySpline* spline = aCTF.GetFunction();
    return TimingFunction(CubicBezierFunction(spline->X1(), spline->Y1(),
                                              spline->X2(), spline->Y2()));
  }

  uint32_t type = aCTF.GetType() == nsTimingFunction::StepStart ? 1 : 2;
  return TimingFunction(StepFunction(aCTF.GetSteps(), type));
}

static void
AddAnimationForProperty(nsIFrame* aFrame, const AnimationProperty& aProperty,
                        dom::Animation* aAnimation, Layer* aLayer,
                        AnimationData& aData, bool aPending)
{
  MOZ_ASSERT(aLayer->AsContainerLayer(), "Should only animate ContainerLayer");
  MOZ_ASSERT(aAnimation->GetEffect(),
             "Should not be adding an animation without an effect");
  MOZ_ASSERT(!aAnimation->GetCurrentOrPendingStartTime().IsNull() ||
             (aAnimation->GetTimeline() &&
              aAnimation->GetTimeline()->TracksWallclockTime()),
             "Animation should either have a resolved start time or "
             "a timeline that tracks wallclock time");
  nsStyleContext* styleContext = aFrame->StyleContext();
  nsPresContext* presContext = aFrame->PresContext();
  TransformReferenceBox refBox(aFrame);

  layers::Animation* animation =
    aPending ?
    aLayer->AddAnimationForNextTransaction() :
    aLayer->AddAnimation();

  const AnimationTiming& timing = aAnimation->GetEffect()->Timing();
  Nullable<TimeDuration> startTime = aAnimation->GetCurrentOrPendingStartTime();
  animation->startTime() = startTime.IsNull()
                           ? TimeStamp()
                           : aAnimation->GetTimeline()->ToTimeStamp(
                              startTime.Value() + timing.mDelay);
  animation->initialCurrentTime() = aAnimation->GetCurrentTime().Value()
                                    - timing.mDelay;
  animation->duration() = timing.mIterationDuration;
  animation->iterationCount() = timing.mIterationCount;
  animation->direction() = timing.mDirection;
  animation->property() = aProperty.mProperty;
  animation->playbackRate() = aAnimation->PlaybackRate();
  animation->data() = aData;

  for (uint32_t segIdx = 0; segIdx < aProperty.mSegments.Length(); segIdx++) {
    const AnimationPropertySegment& segment = aProperty.mSegments[segIdx];

    AnimationSegment* animSegment = animation->segments().AppendElement();
    if (aProperty.mProperty == eCSSProperty_transform) {
      animSegment->startState() = InfallibleTArray<TransformFunction>();
      animSegment->endState() = InfallibleTArray<TransformFunction>();

      nsCSSValueSharedList* list =
        segment.mFromValue.GetCSSValueSharedListValue();
      AddTransformFunctions(list->mHead, styleContext, presContext, refBox,
                            animSegment->startState().get_ArrayOfTransformFunction());

      list = segment.mToValue.GetCSSValueSharedListValue();
      AddTransformFunctions(list->mHead, styleContext, presContext, refBox,
                            animSegment->endState().get_ArrayOfTransformFunction());
    } else if (aProperty.mProperty == eCSSProperty_opacity) {
      animSegment->startState() = segment.mFromValue.GetFloatValue();
      animSegment->endState() = segment.mToValue.GetFloatValue();
    }

    animSegment->startPortion() = segment.mFromKey;
    animSegment->endPortion() = segment.mToKey;
    animSegment->sampleFn() = ToTimingFunction(segment.mTimingFunction);
  }
}

static void
AddAnimationsForProperty(nsIFrame* aFrame, nsCSSProperty aProperty,
                         AnimationPtrArray& aAnimations,
                         Layer* aLayer, AnimationData& aData,
                         bool aPending)
{
  MOZ_ASSERT(nsCSSProps::PropHasFlags(aProperty,
                                      CSS_PROPERTY_CAN_ANIMATE_ON_COMPOSITOR),
             "inconsistent property flags");

  // Add from first to last (since last overrides)
  for (size_t animIdx = 0; animIdx < aAnimations.Length(); animIdx++) {
    dom::Animation* anim = aAnimations[animIdx];
    if (!anim->IsPlaying()) {
      continue;
    }
    dom::KeyframeEffectReadOnly* effect = anim->GetEffect();
    MOZ_ASSERT(effect, "A playing animation should have an effect");
    const AnimationProperty* property =
      effect->GetAnimationOfProperty(aProperty);
    if (!property) {
      continue;
    }

    // Note that if mWinsInCascade on property was  false,
    // GetAnimationOfProperty returns null instead.
    // This is what we want, since if we have an animation or transition
    // that isn't actually winning in the CSS cascade, we don't want to
    // send it to the compositor.
    // I believe that anything that changes mWinsInCascade should
    // trigger this code again, either because of a restyle that changes
    // the properties in question, or because of the main-thread style
    // update that results when an animation stops being in effect.
    MOZ_ASSERT(property->mWinsInCascade,
               "GetAnimationOfProperty already tested mWinsInCascade");

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
    if (anim->PlayState() == AnimationPlayState::Pending &&
        (!anim->GetTimeline() ||
         !anim->GetTimeline()->TracksWallclockTime())) {
      continue;
    }

    AddAnimationForProperty(aFrame, *property, anim, aLayer, aData, aPending);
    effect->SetIsRunningOnCompositor(aProperty, true);
  }
}

/* static */ void
nsDisplayListBuilder::AddAnimationsAndTransitionsToLayer(Layer* aLayer,
                                                         nsDisplayListBuilder* aBuilder,
                                                         nsDisplayItem* aItem,
                                                         nsIFrame* aFrame,
                                                         nsCSSProperty aProperty)
{
  MOZ_ASSERT(nsCSSProps::PropHasFlags(aProperty,
                                      CSS_PROPERTY_CAN_ANIMATE_ON_COMPOSITOR),
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
  if (aLayer->Manager()->GetBackendType() !=
        layers::LayersBackend::LAYERS_CLIENT) {
    return;
  }

  bool pending = !aBuilder;

  if (pending) {
    aLayer->ClearAnimationsForNextTransaction();
  } else {
    aLayer->ClearAnimations();
  }

  // Update the animation generation on the layer. We need to do this before
  // any early returns since even if we don't add any animations to the
  // layer, we still need to mark it as up-to-date with regards to animations.
  // Otherwise, in RestyleManager we'll notice the discrepancy between the
  // animation generation numbers and update the layer indefinitely.
  uint64_t animationGeneration =
    RestyleManager::GetMaxAnimationGenerationForFrame(aFrame);
  aLayer->SetAnimationGeneration(animationGeneration);

  nsPresContext* presContext = aFrame->PresContext();
  presContext->TransitionManager()->ClearIsRunningOnCompositor(aFrame,
                                                               aProperty);
  presContext->AnimationManager()->ClearIsRunningOnCompositor(aFrame,
                                                              aProperty);
  AnimationCollection* transitions =
    presContext->TransitionManager()->GetAnimationsForCompositor(aFrame,
                                                                 aProperty);
  AnimationCollection* animations =
    presContext->AnimationManager()->GetAnimationsForCompositor(aFrame,
                                                                aProperty);
  if (!animations && !transitions) {
    return;
  }

  // If the frame is not prerendered, bail out.
  // Do this check only during layer construction; during updating the
  // caller is required to check it appropriately.
  if (aItem && !aItem->CanUseAsyncAnimations(aBuilder)) {
    // AnimationManager or TransitionManager need to know that we refused to
    // run this animation asynchronously so that they will not throttle the
    // main thread animation.
    aFrame->Properties().Set(nsIFrame::RefusedAsyncAnimation(),
                            reinterpret_cast<void*>(intptr_t(true)));

    // We need to schedule another refresh driver run so that AnimationManager
    // or TransitionManager get a chance to unthrottle the animation.
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
    Point3D offsetToPerspectiveOrigin =
      nsDisplayTransform::GetDeltaToPerspectiveOrigin(aFrame, scale);
    nscoord perspective = 0.0;
    nsStyleContext* parentStyleContext = aFrame->StyleContext()->GetParent();
    if (parentStyleContext) {
      const nsStyleDisplay* disp = parentStyleContext->StyleDisplay();
      if (disp && disp->mChildPerspective.GetUnit() == eStyleUnit_Coord) {
        perspective = disp->mChildPerspective.GetCoordValue();
      }
    }
    nsPoint origin;
    if (aItem) {
      origin = aItem->ToReferenceFrame();
    } else {
      // transform display items used a reference frame computed from
      // their GetTransformRootFrame().
      nsIFrame* referenceFrame =
        nsLayoutUtils::GetReferenceFrame(GetTransformRootFrame(aFrame));
      origin = aFrame->GetOffsetToCrossDoc(referenceFrame);
    }

    data = TransformData(origin, offsetToTransformOrigin,
                         offsetToPerspectiveOrigin, bounds, perspective,
                         devPixelsToAppUnits);
  } else if (aProperty == eCSSProperty_opacity) {
    data = null_t();
  }

  // When both are running, animations override transitions.  We want
  // to add the ones that override last.
  if (transitions) {
    AddAnimationsForProperty(aFrame, aProperty, transitions->mAnimations,
                             aLayer, data, pending);
  }

  if (animations) {
    AddAnimationsForProperty(aFrame, aProperty, animations->mAnimations,
                             aLayer, data, pending);
  }
}

nsDisplayListBuilder::nsDisplayListBuilder(nsIFrame* aReferenceFrame,
    Mode aMode, bool aBuildCaret)
    : mReferenceFrame(aReferenceFrame),
      mIgnoreScrollFrame(nullptr),
      mLayerEventRegions(nullptr),
      mCurrentTableItem(nullptr),
      mCurrentFrame(aReferenceFrame),
      mCurrentReferenceFrame(aReferenceFrame),
      mCurrentAnimatedGeometryRoot(nullptr),
      mDirtyRect(-1,-1,-1,-1),
      mGlassDisplayItem(nullptr),
      mPendingScrollInfoItems(nullptr),
      mCommittedScrollInfoItems(nullptr),
      mMode(aMode),
      mCurrentScrollParentId(FrameMetrics::NULL_SCROLL_ID),
      mCurrentScrollbarTarget(FrameMetrics::NULL_SCROLL_ID),
      mCurrentScrollbarFlags(0),
      mBuildCaret(aBuildCaret),
      mIgnoreSuppression(false),
      mHadToIgnoreSuppression(false),
      mIsAtRootOfPseudoStackingContext(false),
      mIncludeAllOutOfFlows(false),
      mDescendIntoSubdocuments(true),
      mSelectedFramesOnly(false),
      mAccurateVisibleRegions(false),
      mAllowMergingAndFlattening(true),
      mWillComputePluginGeometry(false),
      mInTransform(false),
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
      mAsyncPanZoomEnabled(nsLayoutUtils::AsyncPanZoomEnabled(aReferenceFrame))
{
  MOZ_COUNT_CTOR(nsDisplayListBuilder);
  PL_InitArenaPool(&mPool, "displayListArena", 1024,
                   std::max(NS_ALIGNMENT_OF(void*),NS_ALIGNMENT_OF(double))-1);
  RecomputeCurrentAnimatedGeometryRoot();

  nsPresContext* pc = aReferenceFrame->PresContext();
  nsIPresShell *shell = pc->PresShell();
  if (pc->IsRenderingOnlySelection()) {
    nsCOMPtr<nsISelectionController> selcon(do_QueryInterface(shell));
    if (selcon) {
      selcon->GetSelection(nsISelectionController::SELECTION_NORMAL,
                           getter_AddRefs(mBoundingSelection));
    }
  }

  nsCSSRendering::BeginFrameTreesLocked();
  PR_STATIC_ASSERT(nsDisplayItem::TYPE_MAX < (1 << nsDisplayItem::TYPE_BITS));
}

static void MarkFrameForDisplay(nsIFrame* aFrame, nsIFrame* aStopAtFrame) {
  for (nsIFrame* f = aFrame; f;
       f = nsLayoutUtils::GetParentOrPlaceholderFor(f)) {
    if (f->GetStateBits() & NS_FRAME_FORCE_DISPLAY_LIST_DESCEND_INTO)
      return;
    f->AddStateBits(NS_FRAME_FORCE_DISPLAY_LIST_DESCEND_INTO);
    if (f == aStopAtFrame) {
      // we've reached a frame that we know will be painted, so we can stop.
      break;
    }
  }
}

void nsDisplayListBuilder::SetContainsBlendMode(uint8_t aBlendMode)
{
  MOZ_ASSERT(aBlendMode != NS_STYLE_BLEND_NORMAL);
  gfxContext::GraphicsOperator op = nsCSSRendering::GetGFXBlendMode(aBlendMode);
  mContainedBlendModes += gfx::CompositionOpForOp(op);
}

bool nsDisplayListBuilder::NeedToForceTransparentSurfaceForItem(nsDisplayItem* aItem)
{
  return aItem == mGlassDisplayItem || aItem->ClearsBackground();
}

void nsDisplayListBuilder::MarkOutOfFlowFrameForDisplay(nsIFrame* aDirtyFrame,
                                                        nsIFrame* aFrame,
                                                        const nsRect& aDirtyRect)
{
  nsRect dirtyRectRelativeToDirtyFrame = aDirtyRect;
  DisplayItemClip clip;
  const DisplayItemClip* clipPtr = nullptr;
  if (nsLayoutUtils::IsFixedPosFrameInDisplayPort(aFrame) &&
      IsPaintingToWindow()) {
    NS_ASSERTION(aDirtyFrame == aFrame->GetParent(), "Dirty frame should be viewport frame");
    // position: fixed items are reflowed into and only drawn inside the
    // viewport, or the scroll position clamping scrollport size, if one is
    // set.
    nsIPresShell* ps = aFrame->PresContext()->PresShell();
    dirtyRectRelativeToDirtyFrame.MoveTo(0, 0);
    if (ps->IsScrollPositionClampingScrollPortSizeSet()) {
      dirtyRectRelativeToDirtyFrame.SizeTo(ps->GetScrollPositionClampingScrollPortSize());
    } else {
      dirtyRectRelativeToDirtyFrame.SizeTo(aDirtyFrame->GetSize());
    }

    // Always clip fixed items to the root scroll frame's scrollport, because
    // we skip setting the clip in ScrollFrameHelper if we have a displayport.
    nsIFrame* rootScroll = aFrame->PresContext()->PresShell()->GetRootScrollFrame();
    if (rootScroll) {
      nsIScrollableFrame* scrollable = do_QueryFrame(rootScroll);

      nsRect clipRect = scrollable->GetScrollPortRect() + ToReferenceFrame(rootScroll);
      nscoord radii[8];
      bool haveRadii = rootScroll->GetPaddingBoxBorderRadii(radii);
      clip.SetTo(clipRect, haveRadii ? radii : nullptr);
      clipPtr = &clip;
    }
  }

  nsRect dirty = dirtyRectRelativeToDirtyFrame - aFrame->GetOffsetTo(aDirtyFrame);
  nsRect overflowRect = aFrame->GetVisualOverflowRect();

  if (aFrame->IsTransformed() &&
      nsLayoutUtils::HasAnimationsForCompositor(aFrame, eCSSProperty_transform)) {
   /**
    * Add a fuzz factor to the overflow rectangle so that elements only just
    * out of view are pulled into the display list, so they can be
    * prerendered if necessary.
    */
    overflowRect.Inflate(nsPresContext::CSSPixelsToAppUnits(32));
  }

  if (!dirty.IntersectRect(dirty, overflowRect))
    return;

  // Combine clips if necessary
  const DisplayItemClip* oldClip = mClipState.GetClipForContainingBlockDescendants();
  if (clipPtr && oldClip) {
    clip.IntersectWith(*oldClip);
  } else if (oldClip) {
    clipPtr = oldClip;
  }

  OutOfFlowDisplayData* data = clipPtr ? new OutOfFlowDisplayData(*clipPtr, dirty)
    : new OutOfFlowDisplayData(dirty);
  aFrame->Properties().Set(nsDisplayListBuilder::OutOfFlowDisplayDataProperty(), data);

  MarkFrameForDisplay(aFrame, aDirtyFrame);
}

static void UnmarkFrameForDisplay(nsIFrame* aFrame) {
  nsPresContext* presContext = aFrame->PresContext();
  presContext->PropertyTable()->
    Delete(aFrame, nsDisplayListBuilder::OutOfFlowDisplayDataProperty());

  for (nsIFrame* f = aFrame; f;
       f = nsLayoutUtils::GetParentOrPlaceholderFor(f)) {
    if (!(f->GetStateBits() & NS_FRAME_FORCE_DISPLAY_LIST_DESCEND_INTO))
      return;
    f->RemoveStateBits(NS_FRAME_FORCE_DISPLAY_LIST_DESCEND_INTO);
  }
}

nsDisplayListBuilder::~nsDisplayListBuilder() {
  NS_ASSERTION(mFramesMarkedForDisplay.Length() == 0,
               "All frames should have been unmarked");
  NS_ASSERTION(mPresShellStates.Length() == 0,
               "All presshells should have been exited");
  NS_ASSERTION(!mCurrentTableItem, "No table item should be active");

  nsCSSRendering::EndFrameTreesLocked();

  for (uint32_t i = 0; i < mDisplayItemClipsToDestroy.Length(); ++i) {
    mDisplayItemClipsToDestroy[i]->DisplayItemClip::~DisplayItemClip();
  }

  PL_FinishArenaPool(&mPool);
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
  nsRefPtr<nsCaret> caret = CurrentPresShellState()->mPresShell->GetCaret();
  return caret;
}

void
nsDisplayListBuilder::EnterPresShell(nsIFrame* aReferenceFrame,
                                     bool aPointerEventsNoneDoc)
{
  PresShellState* state = mPresShellStates.AppendElement();
  state->mPresShell = aReferenceFrame->PresContext()->PresShell();
  state->mCaretFrame = nullptr;
  state->mFirstFrameMarkedForDisplay = mFramesMarkedForDisplay.Length();

  state->mPresShell->UpdateCanvasBackground();

  if (mIsPaintingToWindow) {
    mReferenceFrame->AddPaintedPresShell(state->mPresShell);

    state->mPresShell->IncrementPaintCount();
  }

  bool buildCaret = mBuildCaret;
  if (mIgnoreSuppression || !state->mPresShell->IsPaintingSuppressed()) {
    if (state->mPresShell->IsPaintingSuppressed()) {
      mHadToIgnoreSuppression = true;
    }
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

  nsRefPtr<nsCaret> caret = state->mPresShell->GetCaret();
  state->mCaretFrame = caret->GetPaintGeometry(&state->mCaretRect);
  if (state->mCaretFrame) {
    mFramesMarkedForDisplay.AppendElement(state->mCaretFrame);
    MarkFrameForDisplay(state->mCaretFrame, nullptr);
  }

  nsPresContext* pc = aReferenceFrame->PresContext();
  pc->GetDocShell()->GetWindowDraggingAllowed(&mWindowDraggingAllowed);
  mIsInChromePresContext = pc->IsChrome();
}

void
nsDisplayListBuilder::LeavePresShell(nsIFrame* aReferenceFrame)
{
  NS_ASSERTION(CurrentPresShellState()->mPresShell ==
      aReferenceFrame->PresContext()->PresShell(),
      "Presshell mismatch");

  ResetMarkedFramesForDisplayList();
  mPresShellStates.SetLength(mPresShellStates.Length() - 1);

  if (!mPresShellStates.IsEmpty()) {
    nsPresContext* pc = CurrentPresContext();
    pc->GetDocShell()->GetWindowDraggingAllowed(&mWindowDraggingAllowed);
    mIsInChromePresContext = pc->IsChrome();
  }
}

void
nsDisplayListBuilder::ResetMarkedFramesForDisplayList()
{
  // Unmark and pop off the frames marked for display in this pres shell.
  uint32_t firstFrameForShell = CurrentPresShellState()->mFirstFrameMarkedForDisplay;
  for (uint32_t i = firstFrameForShell;
       i < mFramesMarkedForDisplay.Length(); ++i) {
    UnmarkFrameForDisplay(mFramesMarkedForDisplay[i]);
  }
  mFramesMarkedForDisplay.SetLength(firstFrameForShell);
}

void
nsDisplayListBuilder::MarkFramesForDisplayList(nsIFrame* aDirtyFrame,
                                               const nsFrameList& aFrames,
                                               const nsRect& aDirtyRect) {
  mFramesMarkedForDisplay.SetCapacity(mFramesMarkedForDisplay.Length() + aFrames.GetLength());
  for (nsIFrame* e : aFrames) {
    // Skip the AccessibleCaret frame when building no caret.
    if (!IsBuildingCaret()) {
      nsIContent* content = e->GetContent();
      if (content && content->IsInNativeAnonymousSubtree() && content->IsElement()) {
        ErrorResult rv;
        auto classList = content->AsElement()->ClassList();
        if (classList->Contains(NS_LITERAL_STRING("moz-accessiblecaret"), rv)) {
          continue;
        }
      }
    }
    mFramesMarkedForDisplay.AppendElement(e);
    MarkOutOfFlowFrameForDisplay(aDirtyFrame, e, aDirtyRect);
  }
}

void
nsDisplayListBuilder::MarkPreserve3DFramesForDisplayList(nsIFrame* aDirtyFrame, const nsRect& aDirtyRect)
{
  nsAutoTArray<nsIFrame::ChildList,4> childListArray;
  aDirtyFrame->GetChildLists(&childListArray);
  nsIFrame::ChildListArrayIterator lists(childListArray);
  for (; !lists.IsDone(); lists.Next()) {
    nsFrameList::Enumerator childFrames(lists.CurrentList());
    for (; !childFrames.AtEnd(); childFrames.Next()) {
      nsIFrame *child = childFrames.get();
      if (child->Preserves3D()) {
        mFramesMarkedForDisplay.AppendElement(child);
        nsRect dirty = aDirtyRect - child->GetOffsetTo(aDirtyFrame);

        child->Properties().Set(nsDisplayListBuilder::Preserve3DDirtyRectProperty(),
                           new nsRect(dirty));

        MarkFrameForDisplay(child, aDirtyFrame);
      }
    }
  }
}

void*
nsDisplayListBuilder::Allocate(size_t aSize)
{
  void *tmp;
  PL_ARENA_ALLOCATE(tmp, &mPool, aSize);
  if (!tmp) {
    NS_ABORT_OOM(aSize);
  }
  return tmp;
}

const DisplayItemClip*
nsDisplayListBuilder::AllocateDisplayItemClip(const DisplayItemClip& aOriginal)
{
  void* p = Allocate(sizeof(DisplayItemClip));
  if (!aOriginal.GetRoundedRectCount()) {
    memcpy(p, &aOriginal, sizeof(DisplayItemClip));
    return static_cast<DisplayItemClip*>(p);
  }

  DisplayItemClip* c = new (p) DisplayItemClip(aOriginal);
  mDisplayItemClipsToDestroy.AppendElement(c);
  return c;
}

const nsIFrame*
nsDisplayListBuilder::FindReferenceFrameFor(const nsIFrame *aFrame,
                                            nsPoint* aOffset)
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
  while (parent->GetType() != nsGkAtoms::scrollFrame) {
    cursor = parent;
    if ((parent = nsLayoutUtils::GetCrossDocParentFrame(cursor)) == nullptr) {
      return false;
    }
  }

  nsIScrollableFrame* sf = do_QueryFrame(parent);
  return sf->IsScrollingActive(aBuilder) && sf->GetScrolledFrame() == cursor;
}

bool
nsDisplayListBuilder::IsAnimatedGeometryRoot(nsIFrame* aFrame, nsIFrame** aParent)
{
  if (nsLayoutUtils::IsPopup(aFrame))
    return true;
  if (ActiveLayerTracker::IsOffsetOrMarginStyleAnimated(aFrame))
    return true;
  if (!aFrame->GetParent() &&
      nsLayoutUtils::ViewportHasDisplayPort(aFrame->PresContext())) {
    // Viewport frames in a display port need to be animated geometry roots
    // for background-attachment:fixed elements.
    return true;
  }

  nsIFrame* parent = nsLayoutUtils::GetCrossDocParentFrame(aFrame);
  if (!parent)
    return true;

  nsIAtom* parentType = parent->GetType();
  // Treat the slider thumb as being as an active scrolled root when it wants
  // its own layer so that it can move without repainting.
  if (parentType == nsGkAtoms::sliderFrame && nsLayoutUtils::IsScrollbarThumbLayerized(aFrame)) {
    return true;
  }

  if (aFrame->StyleDisplay()->mPosition == NS_STYLE_POSITION_STICKY &&
      IsStickyFrameActive(this, aFrame, parent))
  {
    return true;
  }

  if (parentType == nsGkAtoms::scrollFrame || parentType == nsGkAtoms::listControlFrame) {
    nsIScrollableFrame* sf = do_QueryFrame(parent);
    if (sf->IsScrollingActive(this) && sf->GetScrolledFrame() == aFrame) {
      return true;
    }
  }

  // Fixed-pos frames are parented by the viewport frame, which has no parent.
  if (nsLayoutUtils::IsFixedPosFrameInDisplayPort(aFrame)) {
    return true;
  }

  if (aParent) {
    *aParent = parent;
  }
  return false;
}

bool
nsDisplayListBuilder::GetCachedAnimatedGeometryRoot(const nsIFrame* aFrame,
                                                    const nsIFrame* aStopAtAncestor,
                                                    nsIFrame** aOutResult)
{
  AnimatedGeometryRootLookup lookup(aFrame, aStopAtAncestor);
  return mAnimatedGeometryRootCache.Get(lookup, aOutResult);
}

static nsIFrame*
ComputeAnimatedGeometryRootFor(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                               const nsIFrame* aStopAtAncestor = nullptr,
                               bool aUseCache = false)
{
  nsIFrame* cursor = aFrame;
  while (cursor != aStopAtAncestor) {
    if (aUseCache) {
      nsIFrame* result;
      if (aBuilder->GetCachedAnimatedGeometryRoot(cursor, aStopAtAncestor, &result)) {
        return result;
      }
    }
    nsIFrame* next;
    if (aBuilder->IsAnimatedGeometryRoot(cursor, &next))
      return cursor;
    cursor = next;
  }
  return cursor;
}

nsIFrame*
nsDisplayListBuilder::FindAnimatedGeometryRootFor(nsIFrame* aFrame, const nsIFrame* aStopAtAncestor)
{
  if (aFrame == mCurrentFrame) {
    return mCurrentAnimatedGeometryRoot;
  }

  nsIFrame* result = ComputeAnimatedGeometryRootFor(this, aFrame, aStopAtAncestor, true);
  AnimatedGeometryRootLookup lookup(aFrame, aStopAtAncestor);
  mAnimatedGeometryRootCache.Put(lookup, result);
  return result;
}

void
nsDisplayListBuilder::RecomputeCurrentAnimatedGeometryRoot()
{
  mCurrentAnimatedGeometryRoot = ComputeAnimatedGeometryRootFor(this, const_cast<nsIFrame *>(mCurrentFrame));
  AnimatedGeometryRootLookup lookup(mCurrentFrame, nullptr);
  mAnimatedGeometryRootCache.Put(lookup, mCurrentAnimatedGeometryRoot);
}

void
nsDisplayListBuilder::AdjustWindowDraggingRegion(nsIFrame* aFrame)
{
  if (!mWindowDraggingAllowed || !IsForPainting()) {
    return;
  }

  Matrix4x4 referenceFrameToRootReferenceFrame;

  // The const_cast is for nsLayoutUtils::GetTransformToAncestor.
  nsIFrame* referenceFrame = const_cast<nsIFrame*>(FindReferenceFrameFor(aFrame));

  if (IsInTransform()) {
    // Only support 2d rectilinear transforms. Transform support is needed for
    // the horizontal flip transform that's applied to the urlbar textbox in
    // RTL mode - it should be able to exclude itself from the draggable region.
    referenceFrameToRootReferenceFrame =
      nsLayoutUtils::GetTransformToAncestor(referenceFrame, mReferenceFrame);
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
  nsRect borderBox = aFrame->GetRectRelativeToSelf().Intersect(mDirtyRect);
  borderBox += ToReferenceFrame(aFrame);
  const DisplayItemClip* clip = ClipState().GetCurrentCombinedClip(this);
  if (clip) {
    borderBox = clip->ApplyNonRoundedIntersection(borderBox);
  }
  if (!borderBox.IsEmpty()) {
    LayoutDeviceRect devPixelBorderBox =
      LayoutDevicePixel::FromAppUnits(borderBox, aFrame->PresContext()->AppUnitsPerDevPixel());
    LayoutDeviceRect transformedDevPixelBorderBox =
      TransformTo<LayoutDevicePixel>(referenceFrameToRootReferenceFrame, devPixelBorderBox);
    transformedDevPixelBorderBox.Round();
    LayoutDeviceIntRect transformedDevPixelBorderBoxInt;
    if (transformedDevPixelBorderBox.ToIntRect(&transformedDevPixelBorderBoxInt)) {
      const nsStyleUserInterface* styleUI = aFrame->StyleUserInterface();
      if (styleUI->mWindowDragging == NS_STYLE_WINDOW_DRAGGING_DRAG) {
        mWindowDraggingRegion.OrWith(LayoutDevicePixel::ToUntyped(transformedDevPixelBorderBoxInt));
      } else {
        mWindowDraggingRegion.SubOut(LayoutDevicePixel::ToUntyped(transformedDevPixelBorderBoxInt));
      }
    }
  }
}

const uint32_t gWillChangeAreaMultiplier = 3;
static uint32_t GetWillChangeCost(nsIFrame* aFrame,
                                  const nsSize& aSize) {
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
  if (mBudgetSet.Contains(aFrame)) {
    return true; // Already accounted
  }

  nsPresContext* key = aFrame->PresContext();
  if (!mWillChangeBudget.Contains(key)) {
    mWillChangeBudget.Put(key, DocumentWillChangeBudget());
  }

  DocumentWillChangeBudget budget;
  mWillChangeBudget.Get(key, &budget);

  nsRect area = aFrame->PresContext()->GetVisibleArea();
  uint32_t budgetLimit = nsPresContext::AppUnitsToIntCSSPixels(area.width) *
    nsPresContext::AppUnitsToIntCSSPixels(area.height);

  uint32_t cost = GetWillChangeCost(aFrame, aSize);
  bool onBudget = (budget.mBudget + cost) /
                    gWillChangeAreaMultiplier < budgetLimit;

  if (onBudget) {
    budget.mBudget += cost;
    mWillChangeBudget.Put(key, budget);
    mBudgetSet.PutEntry(aFrame);
  }

  return onBudget;
}

bool
nsDisplayListBuilder::IsInWillChangeBudget(nsIFrame* aFrame,
                                           const nsSize& aSize) {
  bool onBudget = AddToWillChangeBudget(aFrame, aSize);

  if (!onBudget) {
    nsString usageStr;
    usageStr.AppendInt(GetWillChangeCost(aFrame, aSize));

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

nsDisplayList*
nsDisplayListBuilder::EnterScrollInfoItemHoisting(nsDisplayList* aScrollInfoItemStorage)
{
  MOZ_ASSERT(ShouldBuildScrollInfoItemsForHoisting());
  nsDisplayList* old = mPendingScrollInfoItems;
  mPendingScrollInfoItems = aScrollInfoItemStorage;
  return old;
}

void
nsDisplayListBuilder::LeaveScrollInfoItemHoisting(nsDisplayList* aScrollInfoItemStorage)
{
  MOZ_ASSERT(ShouldBuildScrollInfoItemsForHoisting());
  mPendingScrollInfoItems = aScrollInfoItemStorage;
}

void
nsDisplayListBuilder::AppendNewScrollInfoItemForHoisting(nsDisplayScrollInfoLayer* aScrollInfoItem)
{
  MOZ_ASSERT(ShouldBuildScrollInfoItemsForHoisting());
  mPendingScrollInfoItems->AppendNewToTop(aScrollInfoItem);
}

void
nsDisplayListBuilder::StoreDirtyRectForScrolledContents(const nsIFrame* aScrollableFrame,
                                                        const nsRect& aDirty)
{
  mDirtyRectForScrolledContents.Put(const_cast<nsIFrame*>(aScrollableFrame),
                                    aDirty + ToReferenceFrame(aScrollableFrame));
}

nsRect
nsDisplayListBuilder::GetDirtyRectForScrolledContents(const nsIFrame* aScrollableFrame) const
{
  nsRect result;
  if (!mDirtyRectForScrolledContents.Get(const_cast<nsIFrame*>(aScrollableFrame), &result)) {
    return nsRect();
  }
  return result;
}

bool
nsDisplayListBuilder::IsBuildingLayerEventRegions()
{
  if (mMode == PAINTING) {
    // Note: this is the only place that gets to query LayoutEventRegionsEnabled
    // 'directly' - other code should call this function.
    return gfxPrefs::LayoutEventRegionsEnabledDoNotUseDirectly() ||
           mAsyncPanZoomEnabled;
  }
  return false;
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
nsDisplayList::GetVisibleRect() const {
  nsRect result;
  for (nsDisplayItem* i = GetBottom(); i != nullptr; i = i->GetAbove()) {
    result.UnionRect(result, i->GetVisibleRect());
  }
  return result;
}

bool
nsDisplayList::ComputeVisibilityForRoot(nsDisplayListBuilder* aBuilder,
                                        nsRegion* aVisibleRegion,
                                        nsIFrame* aDisplayPortFrame) {
  PROFILER_LABEL("nsDisplayList", "ComputeVisibilityForRoot",
    js::ProfileEntry::Category::GRAPHICS);

  nsRegion r;
  r.And(*aVisibleRegion, GetBounds(aBuilder));
  return ComputeVisibilityForSublist(aBuilder, aVisibleRegion,
                                     r.GetBounds(), aDisplayPortFrame);
}

static nsRegion
TreatAsOpaque(nsDisplayItem* aItem, nsDisplayListBuilder* aBuilder)
{
  bool snap;
  nsRegion opaque = aItem->GetOpaqueRegion(aBuilder, &snap);
  if (aBuilder->IsForPluginGeometry() &&
      aItem->GetType() != nsDisplayItem::TYPE_LAYER_EVENT_REGIONS)
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
        f->StyleDisplay()->mOpacity != 0.0) {
      opaque = aItem->GetBounds(aBuilder, &snap);
    }
  }
  if (opaque.IsEmpty()) {
    return opaque;
  }
  nsRegion opaqueClipped;
  nsRegionRectIterator iter(opaque);
  for (const nsRect* r = iter.Next(); r; r = iter.Next()) {
    opaqueClipped.Or(opaqueClipped, aItem->GetClip().ApproximateIntersectInward(*r));
  }
  return opaqueClipped;
}

bool
nsDisplayList::ComputeVisibilityForSublist(nsDisplayListBuilder* aBuilder,
                                           nsRegion* aVisibleRegion,
                                           const nsRect& aListVisibleBounds,
                                           nsIFrame* aDisplayPortFrame) {
#ifdef DEBUG
  nsRegion r;
  r.And(*aVisibleRegion, GetBounds(aBuilder));
  NS_ASSERTION(r.GetBounds().IsEqualInterior(aListVisibleBounds),
               "bad aListVisibleBounds");
#endif

  bool anyVisible = false;

  nsAutoTArray<nsDisplayItem*, 512> elements;
  MoveListTo(this, &elements);

  for (int32_t i = elements.Length() - 1; i >= 0; --i) {
    nsDisplayItem* item = elements[i];
    nsRect bounds = item->GetClippedBounds(aBuilder);

    nsRegion itemVisible;
    itemVisible.And(*aVisibleRegion, bounds);
    item->mVisibleRect = itemVisible.GetBounds();

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

/**
 * We paint by executing a layer manager transaction, constructing a
 * single layer representing the display list, and then making it the
 * root of the layer manager, drawing into the PaintedLayers.
 */
already_AddRefed<LayerManager> nsDisplayList::PaintRoot(nsDisplayListBuilder* aBuilder,
                                                        nsRenderingContext* aCtx,
                                                        uint32_t aFlags) {
  PROFILER_LABEL("nsDisplayList", "PaintRoot",
    js::ProfileEntry::Category::GRAPHICS);

  nsRefPtr<LayerManager> layerManager;
  bool widgetTransaction = false;
  bool allowRetaining = false;
  bool doBeginTransaction = true;
  nsView *view = nullptr;
  if (aFlags & PAINT_USE_WIDGET_LAYERS) {
    nsIFrame* rootReferenceFrame = aBuilder->RootReferenceFrame();
    view = rootReferenceFrame->GetView();
    NS_ASSERTION(rootReferenceFrame == nsLayoutUtils::GetDisplayRootFrame(rootReferenceFrame),
                 "Reference frame must be a display root for us to use the layer manager");
    nsIWidget* window = rootReferenceFrame->GetNearestWidget();
    if (window) {
      layerManager = window->GetLayerManager(&allowRetaining);
      if (layerManager) {
        doBeginTransaction = !(aFlags & PAINT_EXISTING_TRANSACTION);
        widgetTransaction = true;
      }
    }
  }
  if (!layerManager) {
    if (!aCtx) {
      NS_WARNING("Nowhere to paint into");
      return nullptr;
    }
    layerManager = new BasicLayerManager(BasicLayerManager::BLM_OFFSCREEN);
  }

  // Store the existing layer builder to reinstate it on return.
  FrameLayerBuilder *oldBuilder = layerManager->GetLayerBuilder();

  FrameLayerBuilder *layerBuilder = new FrameLayerBuilder();
  layerBuilder->Init(aBuilder, layerManager);

  if (aFlags & PAINT_COMPRESSED) {
    layerBuilder->SetLayerTreeCompressionMode();
  }

  if (aFlags & PAINT_FLUSH_LAYERS) {
    FrameLayerBuilder::InvalidateAllLayers(layerManager);
  }

  if (doBeginTransaction) {
    if (aCtx) {
      layerManager->BeginTransactionWithTarget(aCtx->ThebesContext());
    } else {
      layerManager->BeginTransaction();
    }
  }
  if (widgetTransaction) {
    layerBuilder->DidBeginRetainedLayerTransaction(layerManager);
  }

  nsIFrame* frame = aBuilder->RootReferenceFrame();
  nsPresContext* presContext = frame->PresContext();
  nsIPresShell* presShell = presContext->GetPresShell();
  nsRootPresContext* rootPresContext = presContext->GetRootPresContext();

  NotifySubDocInvalidationFunc computeInvalidFunc =
    presContext->MayHavePaintEventListenerInSubDocument() ? nsPresContext::NotifySubDocInvalidation : 0;
  bool computeInvalidRect = (computeInvalidFunc ||
                             (!layerManager->IsCompositingCheap() && layerManager->NeedsWidgetInvalidation())) &&
                            widgetTransaction;

  UniquePtr<LayerProperties> props;
  if (computeInvalidRect) {
    props = Move(LayerProperties::CloneFrom(layerManager->GetRoot()));
  }

  // Clear any FrameMetrics that may have been set on the root layer on a
  // previous paint. This paint will set new metrics if necessary, and if we
  // don't clear the old one here, we may be left with extra metrics.
  if (Layer* root = layerManager->GetRoot()) {
      root->SetFrameMetrics(nsTArray<FrameMetrics>());
  }

  ContainerLayerParameters containerParameters
    (presShell->GetResolution(), presShell->GetResolution());
  nsRefPtr<ContainerLayer> root = layerBuilder->
    BuildContainerLayerFor(aBuilder, layerManager, frame, nullptr, this,
                           containerParameters, nullptr);

  nsIDocument* document = nullptr;
  if (presShell) {
    document = presShell->GetDocument();
  }

  if (!root) {
    layerManager->SetUserData(&gLayerManagerLayerBuilder, oldBuilder);
    return nullptr;
  }
  // Root is being scaled up by the X/Y resolution. Scale it back down.
  root->SetPostScale(1.0f/containerParameters.mXScale,
                     1.0f/containerParameters.mYScale);
  root->SetScaleToResolution(presShell->ScaleToResolution(),
      containerParameters.mXScale);
  if (aBuilder->IsBuildingLayerEventRegions() &&
      nsLayoutUtils::HasDocumentLevelListenersForApzAwareEvents(presShell)) {
    root->SetEventRegionsOverride(EventRegionsOverride::ForceDispatchToContent);
  } else {
    root->SetEventRegionsOverride(EventRegionsOverride::NoOverride);
  }

  // If we're using containerless scrolling, there is still one case where we
  // want the root container layer to have metrics. If the parent process is
  // using XUL windows, there is no root scrollframe, and without explicitly
  // creating metrics there will be no guaranteed top-level APZC.
  bool addMetrics = gfxPrefs::LayoutUseContainersForRootFrames() ||
      (XRE_IsParentProcess() && !presShell->GetRootScrollFrame());

  // Add metrics if there are none in the layer tree with the id (create an id
  // if there isn't one already) of the root scroll frame/root content.
  bool ensureMetricsForRootId =
    nsLayoutUtils::AsyncPanZoomEnabled(frame) &&
    !gfxPrefs::LayoutUseContainersForRootFrames() &&
    aBuilder->IsPaintingToWindow() &&
    !presContext->GetParentPresContext();

  nsIContent* content = nullptr;
  nsIFrame* rootScrollFrame = presShell->GetRootScrollFrame();
  if (rootScrollFrame) {
    content = rootScrollFrame->GetContent();
  } else {
    // If there is no root scroll frame, pick the document element instead.
    // The only case we don't want to do this is in non-APZ fennec, where
    // we want the root xul document to get a null scroll id so that the root
    // content document gets the first non-null scroll id.
#if !defined(MOZ_WIDGET_ANDROID) || defined(MOZ_ANDROID_APZ)
    content = document->GetDocumentElement();
#endif
  }


  if (ensureMetricsForRootId && content) {
    ViewID scrollId = nsLayoutUtils::FindOrCreateIDFor(content);
    if (nsLayoutUtils::ContainsMetricsWithId(root, scrollId)) {
      ensureMetricsForRootId = false;
    }
  }

  if (addMetrics || ensureMetricsForRootId) {
    bool isRootContent = presContext->IsRootContentDocument();

    nsRect viewport(aBuilder->ToReferenceFrame(frame), frame->GetSize());

    root->SetFrameMetrics(
      nsLayoutUtils::ComputeFrameMetrics(frame,
                         rootScrollFrame, content,
                         aBuilder->FindReferenceFrameFor(frame),
                         root, FrameMetrics::NULL_SCROLL_ID, viewport, Nothing(),
                         isRootContent, containerParameters));
  }

  // NS_WARNING is debug-only, so don't even bother checking the conditions in
  // a release build.
#ifdef DEBUG
  bool usingDisplayport = false;
  if (nsIFrame* rootScrollFrame = presShell->GetRootScrollFrame()) {
    nsIContent* content = rootScrollFrame->GetContent();
    if (content) {
      usingDisplayport = nsLayoutUtils::GetDisplayPort(content, nullptr);
    }
  }
  if (usingDisplayport &&
      !(root->GetContentFlags() & Layer::CONTENT_OPAQUE) &&
      SpammyLayoutWarningsEnabled()) {
    // See bug 693938, attachment 567017
    NS_WARNING("Transparent content with displayports can be expensive.");
  }
#endif

  layerManager->SetRoot(root);
  layerBuilder->WillEndTransaction();

  if (widgetTransaction ||
      // SVG-as-an-image docs don't paint as part of the retained layer tree,
      // but they still need the invalidation state bits cleared in order for
      // invalidation for CSS/SMIL animation to work properly.
      (document && document->IsBeingUsedAsImage())) {
    frame->ClearInvalidationStateBits();
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

  // If this is the content process, we ship plugin geometry updates over with layer
  // updates, so calculate that now before we call EndTransaction.
  if (rootPresContext &&
      aBuilder->WillComputePluginGeometry() &&
      XRE_IsContentProcess()) {
    rootPresContext->ComputePluginGeometryUpdates(aBuilder->RootReferenceFrame(), aBuilder, this);
    rootPresContext->CollectPluginGeometryUpdates(layerManager);
  }

  MaybeSetupTransactionIdAllocator(layerManager, view);

  layerManager->EndTransaction(FrameLayerBuilder::DrawPaintedLayer,
                               aBuilder, flags);
  aBuilder->SetIsCompositingCheap(temp);
  layerBuilder->DidEndTransaction();

  if (document && widgetTransaction) {
    TriggerPendingAnimations(document, layerManager->GetAnimationReadyTime());
  }

  nsIntRegion invalid;
  if (props) {
    invalid = props->ComputeDifferences(root, computeInvalidFunc);
  } else if (widgetTransaction) {
    LayerProperties::ClearInvalidations(root);
  }

  bool shouldInvalidate = layerManager->NeedsWidgetInvalidation();
  if (view) {
    if (props) {
      if (!invalid.IsEmpty()) {
        nsIntRect bounds = invalid.GetBounds();
        nsRect rect(presContext->DevPixelsToAppUnits(bounds.x),
                    presContext->DevPixelsToAppUnits(bounds.y),
                    presContext->DevPixelsToAppUnits(bounds.width),
                    presContext->DevPixelsToAppUnits(bounds.height));
        if (shouldInvalidate) {
          view->GetViewManager()->InvalidateViewNoSuppression(view, rect);
        }
        presContext->NotifyInvalidation(bounds, 0);
      }
    } else if (shouldInvalidate) {
      view->GetViewManager()->InvalidateView(view);
    }
  }

  if (aFlags & PAINT_FLUSH_LAYERS) {
    FrameLayerBuilder::InvalidateAllLayers(layerManager);
  }

  layerManager->SetUserData(&gLayerManagerLayerBuilder, oldBuilder);
  return layerManager.forget();
}

uint32_t nsDisplayList::Count() const {
  uint32_t count = 0;
  for (nsDisplayItem* i = GetBottom(); i; i = i->GetAbove()) {
    ++count;
  }
  return count;
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
  return item;
}

void nsDisplayList::DeleteAll() {
  nsDisplayItem* item;
  while ((item = RemoveBottom()) != nullptr) {
    item->~nsDisplayItem();
  }
}

static bool
GetMouseThrough(const nsIFrame* aFrame)
{
  if (!aFrame->IsBoxFrame())
    return false;

  const nsIFrame* frame = aFrame;
  while (frame) {
    if (frame->GetStateBits() & NS_FRAME_MOUSE_THROUGH_ALWAYS) {
      return true;
    } else if (frame->GetStateBits() & NS_FRAME_MOUSE_THROUGH_NEVER) {
      return false;
    }
    frame = nsBox::GetParentBox(frame);
  }
  return false;
}

static bool
IsFrameReceivingPointerEvents(nsIFrame* aFrame)
{
  nsSubDocumentFrame* frame = do_QueryFrame(aFrame);
  if (frame && frame->PassPointerEventsToChildren()) {
    return true;
  }
  return NS_STYLE_POINTER_EVENTS_NONE !=
    aFrame->StyleVisibility()->GetEffectivePointerEvents(aFrame);
}

// A list of frames, and their z depth. Used for sorting
// the results of hit testing.
struct FramesWithDepth
{
  explicit FramesWithDepth(float aDepth) :
    mDepth(aDepth)
  {}

  bool operator<(const FramesWithDepth& aOther) const {
    if (mDepth != aOther.mDepth) {
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
void FlushFramesArray(nsTArray<FramesWithDepth>& aSource, nsTArray<nsIFrame*>* aDest)
{
  if (aSource.IsEmpty()) {
    return;
  }
  aSource.Sort();
  uint32_t length = aSource.Length();
  for (uint32_t i = 0; i < length; i++) {
    aDest->AppendElements(Move(aSource[i].mFrames));
  }
  aSource.Clear();
}

void nsDisplayList::HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
                            nsDisplayItem::HitTestState* aState,
                            nsTArray<nsIFrame*> *aOutFrames) const {
  int32_t itemBufferStart = aState->mItemBuffer.Length();
  nsDisplayItem* item;
  for (item = GetBottom(); item; item = item->GetAbove()) {
    aState->mItemBuffer.AppendElement(item);
  }
  nsAutoTArray<FramesWithDepth, 16> temp;
  for (int32_t i = aState->mItemBuffer.Length() - 1; i >= itemBufferStart; --i) {
    // Pop element off the end of the buffer. We want to shorten the buffer
    // so that recursive calls to HitTest have more buffer space.
    item = aState->mItemBuffer[i];
    aState->mItemBuffer.SetLength(i);

    bool snap;
    nsRect r = item->GetBounds(aBuilder, &snap).Intersect(aRect);
    if (item->GetClip().MayIntersect(r)) {
      nsAutoTArray<nsIFrame*, 16> outFrames;
      item->HitTest(aBuilder, aRect, aState, &outFrames);

      // For 3d transforms with preserve-3d we add hit frames into the temp list
      // so we can sort them later, otherwise we add them directly to the output list.
      nsTArray<nsIFrame*> *writeFrames = aOutFrames;
      if (item->GetType() == nsDisplayItem::TYPE_TRANSFORM &&
          item->Frame()->Preserves3D()) {
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
        // Handle the XUL 'mousethrough' feature and 'pointer-events'.
        if (!GetMouseThrough(f) && IsFrameReceivingPointerEvents(f)) {
          writeFrames->AppendElement(f);
        }
      }
    }
  }
  // Clear any remaining preserve-3d transforms.
  FlushFramesArray(temp, aOutFrames);
  NS_ASSERTION(aState->mItemBuffer.Length() == uint32_t(itemBufferStart),
               "How did we forget to pop some elements?");
}

static void Sort(nsDisplayList* aList, int32_t aCount, nsDisplayList::SortLEQ aCmp,
                 void* aClosure) {
  if (aCount < 2)
    return;

  nsDisplayList list1;
  nsDisplayList list2;
  int i;
  int32_t half = aCount/2;
  bool sorted = true;
  nsDisplayItem* prev = nullptr;
  for (i = 0; i < aCount; ++i) {
    nsDisplayItem* item = aList->RemoveBottom();
    (i < half ? &list1 : &list2)->AppendToTop(item);
    if (sorted && prev && !aCmp(prev, item, aClosure)) {
      sorted = false;
    }
    prev = item;
  }
  if (sorted) {
    aList->AppendToTop(&list1);
    aList->AppendToTop(&list2);
    return;
  }

  Sort(&list1, half, aCmp, aClosure);
  Sort(&list2, aCount - half, aCmp, aClosure);

  for (i = 0; i < aCount; ++i) {
    if (list1.GetBottom() &&
        (!list2.GetBottom() ||
         aCmp(list1.GetBottom(), list2.GetBottom(), aClosure))) {
      aList->AppendToTop(list1.RemoveBottom());
    } else {
      aList->AppendToTop(list2.RemoveBottom());
    }
  }
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

static bool IsContentLEQ(nsDisplayItem* aItem1, nsDisplayItem* aItem2,
                         void* aClosure) {
  nsIContent* commonAncestor = static_cast<nsIContent*>(aClosure);
  // It's possible that the nsIContent for aItem1 or aItem2 is in a subdocument
  // of commonAncestor, because display items for subdocuments have been
  // mixed into the same list. Ensure that we're looking at content
  // in commonAncestor's document.
  nsIDocument* commonAncestorDoc = commonAncestor->OwnerDoc();
  nsIContent* content1 = FindContentInDocument(aItem1, commonAncestorDoc);
  nsIContent* content2 = FindContentInDocument(aItem2, commonAncestorDoc);
  if (!content1 || !content2) {
    NS_ERROR("Document trees are mixed up!");
    // Something weird going on
    return true;
  }
  return nsLayoutUtils::CompareTreePosition(content1, content2, commonAncestor) <= 0;
}

static bool IsCSSOrderLEQ(nsDisplayItem* aItem1, nsDisplayItem* aItem2, void*) {
  nsIFrame* frame1 = aItem1->Frame();
  nsIFrame* frame2 = aItem2->Frame();
  int32_t order1 = frame1 ? frame1->StylePosition()->mOrder : 0;
  int32_t order2 = frame2 ? frame2->StylePosition()->mOrder : 0;
  return order1 <= order2;
}

static bool IsZOrderLEQ(nsDisplayItem* aItem1, nsDisplayItem* aItem2,
                        void* aClosure) {
  // Note that we can't just take the difference of the two
  // z-indices here, because that might overflow a 32-bit int.
  return aItem1->ZIndex() <= aItem2->ZIndex();
}

void nsDisplayList::SortByZOrder(nsDisplayListBuilder* aBuilder) {
  Sort(aBuilder, IsZOrderLEQ, nullptr);
}

void nsDisplayList::SortByContentOrder(nsDisplayListBuilder* aBuilder,
                                       nsIContent* aCommonAncestor) {
  Sort(aBuilder, IsContentLEQ, aCommonAncestor);
}

void nsDisplayList::SortByCSSOrder(nsDisplayListBuilder* aBuilder) {
  Sort(aBuilder, IsCSSOrderLEQ, nullptr);
}

void nsDisplayList::Sort(nsDisplayListBuilder* aBuilder,
                         SortLEQ aCmp, void* aClosure) {
  ::Sort(this, Count(), aCmp, aClosure);
}

nsDisplayItem::nsDisplayItem(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame)
  : mFrame(aFrame)
  , mClip(aBuilder->ClipState().GetCurrentCombinedClip(aBuilder))
#ifdef MOZ_DUMP_PAINTING
  , mPainted(false)
#endif
{
  mReferenceFrame = aBuilder->FindReferenceFrameFor(aFrame, &mToReferenceFrame);
  NS_ASSERTION(aBuilder->GetDirtyRect().width >= 0 ||
               !aBuilder->IsForPainting(), "dirty rect not set");
  // The dirty rect is for mCurrentFrame, so we have to use
  // mCurrentOffsetToReferenceFrame
  mVisibleRect = aBuilder->GetDirtyRect() +
      aBuilder->GetCurrentFrameOffsetToReferenceFrame();
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

/* static */ int32_t
nsDisplayItem::MaxActiveLayers()
{
  static int32_t sMaxLayers = false;
  static bool sMaxLayersCached = false;

  if (!sMaxLayersCached) {
    Preferences::AddIntVarCache(&sMaxLayers, "layers.max-active", -1);
    sMaxLayersCached = true;
  }

  return sMaxLayers;
}

int32_t
nsDisplayItem::ZIndex() const
{
  if (!mFrame->IsAbsPosContaininingBlock() && !mFrame->IsFlexOrGridItem())
    return 0;

  const nsStylePosition* position = mFrame->StylePosition();
  if (position->mZIndex.GetUnit() == eStyleUnit_Integer)
    return position->mZIndex.GetIntValue();

  // sort the auto and 0 elements together
  return 0;
}

bool
nsDisplayItem::ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                 nsRegion* aVisibleRegion)
{
  return !mVisibleRect.IsEmpty() &&
    !IsInvisibleInRect(aVisibleRegion->GetBounds());
}

bool
nsDisplayItem::RecomputeVisibility(nsDisplayListBuilder* aBuilder,
                                   nsRegion* aVisibleRegion) {
  nsRect bounds = GetClippedBounds(aBuilder);

  nsRegion itemVisible;
  itemVisible.And(*aVisibleRegion, bounds);
  mVisibleRect = itemVisible.GetBounds();

  // When we recompute visibility within layers we don't need to
  // expand the visible region for content behind plugins (the plugin
  // is not in the layer).
  if (!ComputeVisibility(aBuilder, aVisibleRegion)) {
    mVisibleRect = nsRect();
    return false;
  }

  nsRegion opaque = TreatAsOpaque(this, aBuilder);
  aBuilder->SubtractFromVisibleRegion(aVisibleRegion, opaque);
  return true;
}

nsRect
nsDisplayItem::GetClippedBounds(nsDisplayListBuilder* aBuilder)
{
  bool snap;
  nsRect r = GetBounds(aBuilder, &snap);
  return GetClip().ApplyNonRoundedIntersection(r);
}

nsRect
nsDisplaySolidColor::GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap)
{
  *aSnap = true;
  return mBounds;
}

void
nsDisplaySolidColor::Paint(nsDisplayListBuilder* aBuilder,
                           nsRenderingContext* aCtx)
{
  int32_t appUnitsPerDevPixel = mFrame->PresContext()->AppUnitsPerDevPixel();
  DrawTarget* drawTarget = aCtx->GetDrawTarget();
  Rect rect =
    NSRectToSnappedRect(mVisibleRect, appUnitsPerDevPixel, *drawTarget);
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

static void
RegisterThemeGeometry(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                      nsITheme::ThemeGeometryType aType)
{
  if (aBuilder->IsInRootChromeDocumentOrPopup() && !aBuilder->IsInTransform()) {
    nsIFrame* displayRoot = nsLayoutUtils::GetDisplayRootFrame(aFrame);
    nsRect borderBox(aFrame->GetOffsetTo(displayRoot), aFrame->GetSize());
    aBuilder->RegisterThemeGeometry(aType,
        borderBox.ToNearestPixels(aFrame->PresContext()->AppUnitsPerDevPixel()));
  }
}

nsDisplayBackgroundImage::nsDisplayBackgroundImage(nsDisplayListBuilder* aBuilder,
                                                   nsIFrame* aFrame,
                                                   uint32_t aLayer,
                                                   const nsStyleBackground* aBackgroundStyle)
  : nsDisplayImageContainer(aBuilder, aFrame)
  , mBackgroundStyle(aBackgroundStyle)
  , mLayer(aLayer)
{
  MOZ_COUNT_CTOR(nsDisplayBackgroundImage);

  mBounds = GetBoundsInternal(aBuilder);
}

nsDisplayBackgroundImage::~nsDisplayBackgroundImage()
{
#ifdef NS_BUILD_REFCNT_LOGGING
  MOZ_COUNT_DTOR(nsDisplayBackgroundImage);
#endif
}

static nsStyleContext* GetBackgroundStyleContext(nsIFrame* aFrame)
{
  nsStyleContext *sc;
  if (!nsCSSRendering::FindBackground(aFrame, &sc)) {
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

    sc = aFrame->StyleContext();
  }
  return sc;
}

/* static */ void
SetBackgroundClipRegion(DisplayListClipState::AutoSaveRestore& aClipState,
                        nsIFrame* aFrame, const nsPoint& aToReferenceFrame,
                        const nsStyleBackground::Layer& aLayer,
                        bool aWillPaintBorder)
{
  nsRect borderBox = nsRect(aToReferenceFrame, aFrame->GetSize());

  nsCSSRendering::BackgroundClipState clip;
  nsCSSRendering::GetBackgroundClip(aLayer, aFrame, *aFrame->StyleBorder(),
                                    borderBox, borderBox, aWillPaintBorder,
                                    aFrame->PresContext()->AppUnitsPerDevPixel(),
                                    &clip);

  if (clip.mHasAdditionalBGClipArea) {
    aClipState.ClipContentDescendants(clip.mAdditionalBGClipArea, clip.mBGClipArea,
                                      clip.mHasRoundedCorners ? clip.mRadii : nullptr);
  } else {
    aClipState.ClipContentDescendants(clip.mBGClipArea, clip.mHasRoundedCorners ? clip.mRadii : nullptr);
  }
}


/*static*/ bool
nsDisplayBackgroundImage::AppendBackgroundItemsToTop(nsDisplayListBuilder* aBuilder,
                                                     nsIFrame* aFrame,
                                                     nsDisplayList* aList)
{
  nsStyleContext* bgSC = nullptr;
  const nsStyleBackground* bg = nullptr;
  nsPresContext* presContext = aFrame->PresContext();
  bool isThemed = aFrame->IsThemed();
  if (!isThemed) {
    bgSC = GetBackgroundStyleContext(aFrame);
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

  const nsStyleBorder* borderStyle = aFrame->StyleBorder();
  bool hasInsetShadow = borderStyle->mBoxShadow &&
                        borderStyle->mBoxShadow->HasShadowWithInset(true);
  bool willPaintBorder = !isThemed && !hasInsetShadow &&
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
    DisplayListClipState::AutoSaveRestore clipState(aBuilder);
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
      SetBackgroundClipRegion(clipState, aFrame, toRef,
                              bg->BottomLayer(),
                              useWillPaintBorderOptimization);
    }
    bgItemList.AppendNewToTop(
        new (aBuilder) nsDisplayBackgroundColor(aBuilder, aFrame, bg,
                                                drawBackgroundColor ? color : NS_RGBA(0, 0, 0, 0)));
  }

  if (isThemed) {
    nsITheme* theme = presContext->GetTheme();
    if (theme->NeedToClearBackgroundBehindWidget(aFrame, aFrame->StyleDisplay()->mAppearance) &&
        aBuilder->IsInRootChromeDocumentOrPopup() && !aBuilder->IsInTransform()) {
      bgItemList.AppendNewToTop(
        new (aBuilder) nsDisplayClearBackground(aBuilder, aFrame));
    }
    nsDisplayThemedBackground* bgItem =
      new (aBuilder) nsDisplayThemedBackground(aBuilder, aFrame);
    bgItemList.AppendNewToTop(bgItem);
    aList->AppendToTop(&bgItemList);
    return true;
  }

  if (!bg) {
    aList->AppendToTop(&bgItemList);
    return false;
  }
 
  bool needBlendContainer = false;

  // Passing bg == nullptr in this macro will result in one iteration with
  // i = 0.
  NS_FOR_VISIBLE_BACKGROUND_LAYERS_BACK_TO_FRONT(i, bg) {
    if (bg->mLayers[i].mImage.IsEmpty()) {
      continue;
    }

    if (bg->mLayers[i].mBlendMode != NS_STYLE_BLEND_NORMAL) {
      needBlendContainer = true;
    }

    DisplayListClipState::AutoSaveRestore clipState(aBuilder);
    if (!aBuilder->IsForEventDelivery()) {
      const nsStyleBackground::Layer& layer = bg->mLayers[i];
      SetBackgroundClipRegion(clipState, aFrame, toRef,
                              layer, willPaintBorder);
    }

    nsDisplayBackgroundImage* bgItem =
      new (aBuilder) nsDisplayBackgroundImage(aBuilder, aFrame, i, bg);
    bgItemList.AppendNewToTop(bgItem);
  }

  if (needBlendContainer) {
    bgItemList.AppendNewToTop(
      new (aBuilder) nsDisplayBlendContainer(aBuilder, aFrame, &bgItemList));
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
nsDisplayBackgroundImage::IsSingleFixedPositionImage(nsDisplayListBuilder* aBuilder,
                                                     const nsRect& aClipRect,
                                                     gfxRect* aDestRect)
{
  if (!mBackgroundStyle)
    return false;

  if (mBackgroundStyle->mLayers.Length() != 1)
    return false;

  nsPresContext* presContext = mFrame->PresContext();
  uint32_t flags = aBuilder->GetBackgroundPaintFlags();
  nsRect borderArea = nsRect(ToReferenceFrame(), mFrame->GetSize());
  const nsStyleBackground::Layer &layer = mBackgroundStyle->mLayers[mLayer];

  if (layer.mAttachment != NS_STYLE_BG_ATTACHMENT_FIXED)
    return false;

  nsBackgroundLayerState state =
    nsCSSRendering::PrepareBackgroundLayer(presContext, mFrame, flags,
                                           borderArea, aClipRect, layer);
  nsImageRenderer* imageRenderer = &state.mImageRenderer;
  // We only care about images here, not gradients.
  if (!imageRenderer->IsRasterImage())
    return false;

  int32_t appUnitsPerDevPixel = presContext->AppUnitsPerDevPixel();
  *aDestRect = nsLayoutUtils::RectToGfxRect(state.mFillArea, appUnitsPerDevPixel);

  return true;
}

bool
nsDisplayBackgroundImage::IsNonEmptyFixedImage() const
{
  return mBackgroundStyle->mLayers[mLayer].mAttachment == NS_STYLE_BG_ATTACHMENT_FIXED &&
         !mBackgroundStyle->mLayers[mLayer].mImage.IsEmpty();
}

bool
nsDisplayBackgroundImage::ShouldFixToViewport(LayerManager* aManager)
{
  // APZ needs background-attachment:fixed images layerized for correctness.
  if (!nsLayoutUtils::UsesAsyncScrolling(mFrame) &&
      aManager && aManager->ShouldAvoidComponentAlphaLayers()) {
    return false;
  }

  // Put background-attachment:fixed background images in their own
  // compositing layer.
  return IsNonEmptyFixedImage();
}

bool
nsDisplayBackgroundImage::CanOptimizeToImageLayer(LayerManager* aManager,
                                                  nsDisplayListBuilder* aBuilder)
{
  if (!mBackgroundStyle) {
    return false;
  }

  nsPresContext* presContext = mFrame->PresContext();
  uint32_t flags = aBuilder->GetBackgroundPaintFlags();
  nsRect borderArea = nsRect(ToReferenceFrame(), mFrame->GetSize());
  const nsStyleBackground::Layer &layer = mBackgroundStyle->mLayers[mLayer];

  nsBackgroundLayerState state =
    nsCSSRendering::PrepareBackgroundLayer(presContext, mFrame, flags,
                                           borderArea, borderArea, layer);
  nsImageRenderer* imageRenderer = &state.mImageRenderer;
  // We only care about images here, not gradients.
  if (!imageRenderer->IsRasterImage()) {
    return false;
  }

  if (!imageRenderer->IsContainerAvailable(aManager, aBuilder)) {
    // The image is not ready to be made into a layer yet.
    return false;
  }

  // We currently can't handle tiled or partial backgrounds.
  if (!state.mDestArea.IsEqualEdges(state.mFillArea)) {
    return false;
  }

  // XXX Ignoring state.mAnchor. ImageLayer drawing snaps mDestArea edges to
  // layer pixel boundaries. This should be OK for now.

  int32_t appUnitsPerDevPixel = presContext->AppUnitsPerDevPixel();
  mDestRect =
    LayoutDeviceRect::FromAppUnits(state.mDestArea, appUnitsPerDevPixel);

  // Ok, we can turn this into a layer if needed.
  mImage = imageRenderer->GetImage();
  MOZ_ASSERT(mImage);

  return true;
}

already_AddRefed<ImageContainer>
nsDisplayBackgroundImage::GetContainer(LayerManager* aManager,
                                       nsDisplayListBuilder *aBuilder)
{
  if (!mImage) {
    MOZ_ASSERT_UNREACHABLE("Must call CanOptimizeToImage() and get true "
                           "before calling GetContainer()");
    return nullptr;
  }

  if (!mImageContainer) {
    // We don't have an ImageContainer yet; get it from mImage.

    uint32_t flags = aBuilder->ShouldSyncDecodeImages()
                   ? imgIContainer::FLAG_SYNC_DECODE
                   : imgIContainer::FLAG_NONE;

    mImageContainer = mImage->GetImageContainer(aManager, flags);
  }

  nsRefPtr<ImageContainer> container = mImageContainer;
  return container.forget();
}

LayerState
nsDisplayBackgroundImage::GetLayerState(nsDisplayListBuilder* aBuilder,
                                        LayerManager* aManager,
                                        const ContainerLayerParameters& aParameters)
{
  bool animated = false;
  if (mBackgroundStyle) {
    const nsStyleBackground::Layer &layer = mBackgroundStyle->mLayers[mLayer];
    const nsStyleImage* image = &layer.mImage;
    if (image->GetType() == eStyleImageType_Image) {
      imgIRequest* imgreq = image->GetImageData();
      nsCOMPtr<imgIContainer> image;
      if (NS_SUCCEEDED(imgreq->GetImage(getter_AddRefs(image))) && image) {
        if (NS_FAILED(image->GetAnimated(&animated))) {
          animated = false;
        }
      }
    }
  }

  if (!animated ||
      !nsLayoutUtils::AnimatedImageLayersEnabled()) {
    if (!aManager->IsCompositingCheap() ||
        !nsLayoutUtils::GPUImageScalingEnabled()) {
      return LAYER_NONE;
    }
  }

  if (!CanOptimizeToImageLayer(aManager, aBuilder)) {
    return LAYER_NONE;
  }

  MOZ_ASSERT(mImage);

  if (!animated) {
    int32_t imageWidth;
    int32_t imageHeight;
    mImage->GetWidth(&imageWidth);
    mImage->GetHeight(&imageHeight);
    NS_ASSERTION(imageWidth != 0 && imageHeight != 0, "Invalid image size!");

    const LayerRect destLayerRect = mDestRect * aParameters.Scale();

    // Calculate the scaling factor for the frame.
    const gfxSize scale = gfxSize(destLayerRect.width / imageWidth,
                                  destLayerRect.height / imageHeight);

    // If we are not scaling at all, no point in separating this into a layer.
    if (scale.width == 1.0f && scale.height == 1.0f) {
      return LAYER_NONE;
    }

    // If the target size is pretty small, no point in using a layer.
    if (destLayerRect.width * destLayerRect.height < 64 * 64) {
      return LAYER_NONE;
    }
  }

  return LAYER_ACTIVE;
}

already_AddRefed<Layer>
nsDisplayBackgroundImage::BuildLayer(nsDisplayListBuilder* aBuilder,
                                     LayerManager* aManager,
                                     const ContainerLayerParameters& aParameters)
{
  nsRefPtr<ImageLayer> layer = static_cast<ImageLayer*>
    (aManager->GetLayerBuilder()->GetLeafLayerFor(aBuilder, this));
  if (!layer) {
    layer = aManager->CreateImageLayer();
    if (!layer)
      return nullptr;
  }
  nsRefPtr<ImageContainer> imageContainer = GetContainer(aManager, aBuilder);
  layer->SetContainer(imageContainer);
  ConfigureLayer(layer, aParameters);
  return layer.forget();
}

void
nsDisplayBackgroundImage::ConfigureLayer(ImageLayer* aLayer,
                                         const ContainerLayerParameters& aParameters)
{
  aLayer->SetFilter(nsLayoutUtils::GetGraphicsFilterForFrame(mFrame));

  MOZ_ASSERT(mImage);
  int32_t imageWidth;
  int32_t imageHeight;
  mImage->GetWidth(&imageWidth);
  mImage->GetHeight(&imageHeight);
  NS_ASSERTION(imageWidth != 0 && imageHeight != 0, "Invalid image size!");

  if (imageWidth > 0 && imageHeight > 0) {
    // We're actually using the ImageContainer. Let our frame know that it
    // should consider itself to have painted successfully.
    nsDisplayBackgroundGeometry::UpdateDrawResult(this, mozilla::image::DrawResult::SUCCESS);
  }

  // XXX(seth): Right now we ignore aParameters.Scale() and
  // aParameters.Offset(), because FrameLayerBuilder already applies
  // aParameters.Scale() via the layer's post-transform, and
  // aParameters.Offset() is always zero.
  MOZ_ASSERT(aParameters.Offset() == LayerIntPoint(0,0));

  const LayoutDevicePoint p = mDestRect.TopLeft();
  Matrix transform = Matrix::Translation(p.x, p.y);
  transform.PreScale(mDestRect.width / imageWidth,
                     mDestRect.height / imageHeight);
  aLayer->SetBaseTransform(gfx::Matrix4x4::From2D(transform));
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
nsDisplayBackgroundImage::GetInsideClipRegion(nsDisplayItem* aItem,
                                              nsPresContext* aPresContext,
                                              uint8_t aClip, const nsRect& aRect,
                                              bool* aSnap)
{
  nsRegion result;
  if (aRect.IsEmpty())
    return result;

  nsIFrame *frame = aItem->Frame();

  nsRect clipRect;
  if (frame->GetType() == nsGkAtoms::canvasFrame) {
    nsCanvasFrame* canvasFrame = static_cast<nsCanvasFrame*>(frame);
    clipRect = canvasFrame->CanvasArea() + aItem->ToReferenceFrame();
  } else {
    switch (aClip) {
    case NS_STYLE_BG_CLIP_BORDER:
      clipRect = nsRect(aItem->ToReferenceFrame(), frame->GetSize());
      break;
    case NS_STYLE_BG_CLIP_PADDING:
      clipRect = frame->GetPaddingRect() - frame->GetPosition() + aItem->ToReferenceFrame();
      break;
    case NS_STYLE_BG_CLIP_CONTENT:
      clipRect = frame->GetContentRectRelativeToSelf() + aItem->ToReferenceFrame();
      break;
    default:
      NS_NOTREACHED("Unknown clip type");
      return result;
    }
  }

  return clipRect.Intersect(aRect);
}

nsRegion
nsDisplayBackgroundImage::GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                                          bool* aSnap) {
  nsRegion result;
  *aSnap = false;

  if (!mBackgroundStyle)
    return result;


  *aSnap = true;

  // For NS_STYLE_BOX_DECORATION_BREAK_SLICE, don't try to optimize here, since
  // this could easily lead to O(N^2) behavior inside InlineBackgroundData,
  // which expects frames to be sent to it in content order, not reverse
  // content order which we'll produce here.
  // Of course, if there's only one frame in the flow, it doesn't matter.
  if (mFrame->StyleBorder()->mBoxDecorationBreak ==
        NS_STYLE_BOX_DECORATION_BREAK_CLONE ||
      (!mFrame->GetPrevContinuation() && !mFrame->GetNextContinuation())) {
    const nsStyleBackground::Layer& layer = mBackgroundStyle->mLayers[mLayer];
    if (layer.mImage.IsOpaque() && layer.mBlendMode == NS_STYLE_BLEND_NORMAL) {
      nsPresContext* presContext = mFrame->PresContext();
      result = GetInsideClipRegion(this, presContext, layer.mClip, mBounds, aSnap);
    }
  }

  return result;
}

bool
nsDisplayBackgroundImage::IsUniform(nsDisplayListBuilder* aBuilder, nscolor* aColor) {
  if (!mBackgroundStyle) {
    *aColor = NS_RGBA(0,0,0,0);
    return true;
  }
  return false;
}

nsRect
nsDisplayBackgroundImage::GetPositioningArea()
{
  if (!mBackgroundStyle) {
    return nsRect();
  }
  nsIFrame* attachedToFrame;
  return nsCSSRendering::ComputeBackgroundPositioningArea(
      mFrame->PresContext(), mFrame,
      nsRect(ToReferenceFrame(), mFrame->GetSize()),
      mBackgroundStyle->mLayers[mLayer],
      &attachedToFrame) + ToReferenceFrame();
}

bool
nsDisplayBackgroundImage::RenderingMightDependOnPositioningAreaSizeChange()
{
  if (!mBackgroundStyle)
    return false;

  nscoord radii[8];
  if (mFrame->GetBorderRadii(radii)) {
    // A change in the size of the positioning area might change the position
    // of the rounded corners.
    return true;
  }

  const nsStyleBackground::Layer &layer = mBackgroundStyle->mLayers[mLayer];
  if (layer.RenderingMightDependOnPositioningAreaSizeChange()) {
    return true;
  }
  return false;
}

static void CheckForBorderItem(nsDisplayItem *aItem, uint32_t& aFlags)
{
  nsDisplayItem* nextItem = aItem->GetAbove();
  while (nextItem && nextItem->GetType() == nsDisplayItem::TYPE_BACKGROUND) {
    nextItem = nextItem->GetAbove();
  }
  if (nextItem && 
      nextItem->Frame() == aItem->Frame() &&
      nextItem->GetType() == nsDisplayItem::TYPE_BORDER) {
    aFlags |= nsCSSRendering::PAINTBG_WILL_PAINT_BORDER;
  }
}

void
nsDisplayBackgroundImage::Paint(nsDisplayListBuilder* aBuilder,
                                nsRenderingContext* aCtx) {
  PaintInternal(aBuilder, aCtx, mVisibleRect, &mBounds);
}

void
nsDisplayBackgroundImage::PaintInternal(nsDisplayListBuilder* aBuilder,
                                        nsRenderingContext* aCtx, const nsRect& aBounds,
                                        nsRect* aClipRect) {
  nsPoint offset = ToReferenceFrame();
  uint32_t flags = aBuilder->GetBackgroundPaintFlags();
  CheckForBorderItem(this, flags);

  mozilla::image::DrawResult result =
    nsCSSRendering::PaintBackground(mFrame->PresContext(), *aCtx, mFrame,
                                    aBounds,
                                    nsRect(offset, mFrame->GetSize()),
                                    flags, aClipRect, mLayer);

  nsDisplayBackgroundGeometry::UpdateDrawResult(this, result);
}

void nsDisplayBackgroundImage::ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                                         const nsDisplayItemGeometry* aGeometry,
                                                         nsRegion* aInvalidRegion)
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

    if (positioningArea.Size() != geometry->mPositioningArea.Size()) {
      NotifyRenderingChanged();
    }
    return;
  }
  if (aBuilder->ShouldSyncDecodeImages()) {
    const nsStyleImage& image = mBackgroundStyle->mLayers[mLayer].mImage;
    if (image.GetType() == eStyleImageType_Image &&
        geometry->ShouldInvalidateToSyncDecodeImages()) {
      aInvalidRegion->Or(*aInvalidRegion, bounds);

      NotifyRenderingChanged();
    }
  }
  if (!bounds.IsEqualInterior(geometry->mBounds)) {
    // Positioning area is unchanged, so invalidate just the change in the
    // painting area.
    aInvalidRegion->Xor(bounds, geometry->mBounds);

    NotifyRenderingChanged();
  }
}

nsRect
nsDisplayBackgroundImage::GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap) {
  *aSnap = true;
  return mBounds;
}

nsRect
nsDisplayBackgroundImage::GetBoundsInternal(nsDisplayListBuilder* aBuilder) {
  nsPresContext* presContext = mFrame->PresContext();

  if (!mBackgroundStyle) {
    return nsRect();
  }

  nsRect borderBox = nsRect(ToReferenceFrame(), mFrame->GetSize());
  nsRect clipRect = borderBox;
  if (mFrame->GetType() == nsGkAtoms::canvasFrame) {
    nsCanvasFrame* frame = static_cast<nsCanvasFrame*>(mFrame);
    clipRect = frame->CanvasArea() + ToReferenceFrame();
  } else if (nsLayoutUtils::UsesAsyncScrolling(mFrame) && IsNonEmptyFixedImage()) {
    // If this is a background-attachment:fixed image, and APZ is enabled,
    // async scrolling could reveal additional areas of the image, so don't
    // clip it beyond clipping to the document's viewport.
    nsIFrame* rootFrame = presContext->PresShell()->GetRootFrame();
    nsRect rootRect = rootFrame->GetRectRelativeToSelf();
    if (nsLayoutUtils::TransformRect(rootFrame, mFrame, rootRect) == nsLayoutUtils::TRANSFORM_SUCCEEDED) {
      clipRect = rootRect + aBuilder->ToReferenceFrame(mFrame);
    }
  }
  const nsStyleBackground::Layer& layer = mBackgroundStyle->mLayers[mLayer];
  return nsCSSRendering::GetBackgroundLayerRect(presContext, mFrame,
                                                borderBox, clipRect, layer,
                                                aBuilder->GetBackgroundPaintFlags());
}

uint32_t
nsDisplayBackgroundImage::GetPerFrameKey()
{
  return (mLayer << nsDisplayItem::TYPE_BITS) |
    nsDisplayItem::GetPerFrameKey();
}

nsDisplayThemedBackground::nsDisplayThemedBackground(nsDisplayListBuilder* aBuilder,
                                                     nsIFrame* aFrame)
  : nsDisplayItem(aBuilder, aFrame)
{
  MOZ_COUNT_CTOR(nsDisplayThemedBackground);

  const nsStyleDisplay* disp = mFrame->StyleDisplay();
  mAppearance = disp->mAppearance;
  mFrame->IsThemed(disp, &mThemeTransparency);

  // Perform necessary RegisterThemeGeometry
  nsITheme* theme = mFrame->PresContext()->GetTheme();
  nsITheme::ThemeGeometryType type =
    theme->ThemeGeometryTypeForWidget(mFrame, disp->mAppearance);
  if (type != nsITheme::eThemeGeometryTypeUnknown) {
    RegisterThemeGeometry(aBuilder, aFrame, type);
  }

  if (disp->mAppearance == NS_THEME_WIN_BORDERLESS_GLASS ||
      disp->mAppearance == NS_THEME_WIN_GLASS) {
    aBuilder->SetGlassDisplayItem(this);
  }

  mBounds = GetBoundsInternal();
}

nsDisplayThemedBackground::~nsDisplayThemedBackground()
{
#ifdef NS_BUILD_REFCNT_LOGGING
  MOZ_COUNT_DTOR(nsDisplayThemedBackground);
#endif
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
  // Assume that any point in our border rect is a hit.
  if (nsRect(ToReferenceFrame(), mFrame->GetSize()).Intersects(aRect)) {
    aOutFrames->AppendElement(mFrame);
  }
}

nsRegion
nsDisplayThemedBackground::GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                                           bool* aSnap) {
  nsRegion result;
  *aSnap = false;

  if (mThemeTransparency == nsITheme::eOpaque) {
    result = nsRect(ToReferenceFrame(), mFrame->GetSize());
  }
  return result;
}

bool
nsDisplayThemedBackground::IsUniform(nsDisplayListBuilder* aBuilder, nscolor* aColor) {
  if (mAppearance == NS_THEME_WIN_BORDERLESS_GLASS ||
      mAppearance == NS_THEME_WIN_GLASS) {
    *aColor = NS_RGBA(0,0,0,0);
    return true;
  }
  return false;
}

bool
nsDisplayThemedBackground::ProvidesFontSmoothingBackgroundColor(nsDisplayListBuilder* aBuilder,
                                                                nscolor* aColor)
{
  nsITheme* theme = mFrame->PresContext()->GetTheme();
  return theme->WidgetProvidesFontSmoothingBackgroundColor(mFrame, mAppearance, aColor);
}

nsRect
nsDisplayThemedBackground::GetPositioningArea()
{
  return nsRect(ToReferenceFrame(), mFrame->GetSize());
}

void
nsDisplayThemedBackground::Paint(nsDisplayListBuilder* aBuilder,
                                 nsRenderingContext* aCtx)
{
  PaintInternal(aBuilder, aCtx, mVisibleRect, nullptr);
}


void
nsDisplayThemedBackground::PaintInternal(nsDisplayListBuilder* aBuilder,
                                         nsRenderingContext* aCtx, const nsRect& aBounds,
                                         nsRect* aClipRect)
{
  // XXXzw this ignores aClipRect.
  nsPresContext* presContext = mFrame->PresContext();
  nsITheme *theme = presContext->GetTheme();
  nsRect borderArea(ToReferenceFrame(), mFrame->GetSize());
  nsRect drawing(borderArea);
  theme->GetWidgetOverflow(presContext->DeviceContext(), mFrame, mAppearance,
                           &drawing);
  drawing.IntersectRect(drawing, aBounds);
  theme->DrawWidgetBackground(aCtx, mFrame, mAppearance, borderArea, drawing);
}

bool nsDisplayThemedBackground::IsWindowActive()
{
  EventStates docState = mFrame->GetContent()->OwnerDoc()->GetDocumentState();
  return !docState.HasState(NS_DOCUMENT_STATE_WINDOW_INACTIVE);
}

void nsDisplayThemedBackground::ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                                          const nsDisplayItemGeometry* aGeometry,
                                                          nsRegion* aInvalidRegion)
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
  nsITheme* theme = mFrame->PresContext()->GetTheme();
  if (theme->WidgetAppearanceDependsOnWindowFocus(mAppearance) &&
      IsWindowActive() != geometry->mWindowIsActive) {
    aInvalidRegion->Or(*aInvalidRegion, bounds);
  }
}

nsRect
nsDisplayThemedBackground::GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap) {
  *aSnap = true;
  return mBounds;
}

nsRect
nsDisplayThemedBackground::GetBoundsInternal() {
  nsPresContext* presContext = mFrame->PresContext();

  nsRect r(nsPoint(0,0), mFrame->GetSize());
  presContext->GetTheme()->
      GetWidgetOverflow(presContext->DeviceContext(), mFrame,
                        mFrame->StyleDisplay()->mAppearance, &r);
  return r + ToReferenceFrame();
}

void
nsDisplayBackgroundColor::ApplyOpacity(nsDisplayListBuilder* aBuilder,
                                       float aOpacity,
                                       const DisplayItemClip* aClip)
{
  NS_ASSERTION(CanApplyOpacity(), "ApplyOpacity should be allowed");
  mColor.a = mColor.a * aOpacity;
  if (aClip) {
    IntersectClip(aBuilder, *aClip);
  }
}

bool
nsDisplayBackgroundColor::CanApplyOpacity() const
{
  return true;
}

void
nsDisplayBackgroundColor::Paint(nsDisplayListBuilder* aBuilder,
                                nsRenderingContext* aCtx)
{
  if (mColor == NS_RGBA(0, 0, 0, 0)) {
    return;
  }

  nsRect borderBox = nsRect(ToReferenceFrame(), mFrame->GetSize());

#if 0
  // See https://bugzilla.mozilla.org/show_bug.cgi?id=1148418#c21 for why this
  // results in a precision induced rounding issue that makes the rect one
  // pixel shorter in rare cases. Disabled in favor of the old code for now.
  // Note that the pref layout.css.devPixelsPerPx needs to be set to 1 to
  // reproduce the bug.
  DrawTarget& aDrawTarget = *aCtx->GetDrawTarget();

  Rect rect = NSRectToSnappedRect(borderBox,
                                  mFrame->PresContext()->AppUnitsPerDevPixel(),
                                  aDrawTarget);
  ColorPattern color(ToDeviceColor(mColor));
  aDrawTarget.FillRect(rect, color);
#else
  gfxContext* ctx = aCtx->ThebesContext();

  gfxRect bounds =
    nsLayoutUtils::RectToGfxRect(borderBox, mFrame->PresContext()->AppUnitsPerDevPixel());

  ctx->SetColor(mColor);
  ctx->NewPath();
  ctx->Rectangle(bounds, true);
  ctx->Fill();
#endif
}

nsRegion
nsDisplayBackgroundColor::GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                                          bool* aSnap)
{
  if (mColor.a != 1) {
    return nsRegion();
  }

  if (!mBackgroundStyle)
    return nsRegion();

  *aSnap = true;

  const nsStyleBackground::Layer& bottomLayer = mBackgroundStyle->BottomLayer();
  nsRect borderBox = nsRect(ToReferenceFrame(), mFrame->GetSize());
  nsPresContext* presContext = mFrame->PresContext();
  return nsDisplayBackgroundImage::GetInsideClipRegion(this, presContext, bottomLayer.mClip, borderBox, aSnap);
}

bool
nsDisplayBackgroundColor::IsUniform(nsDisplayListBuilder* aBuilder, nscolor* aColor)
{
  *aColor = NS_RGBA_FROM_GFXRGBA(mColor);
  return true;
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
  nsRefPtr<ColorLayer> layer = static_cast<ColorLayer*>
    (aManager->GetLayerBuilder()->GetLeafLayerFor(aBuilder, this));
  if (!layer) {
    layer = aManager->CreateColorLayer();
    if (!layer)
      return nullptr;
  }
  layer->SetColor(NS_RGBA(0, 0, 0, 0));
  layer->SetMixBlendMode(gfx::CompositionOp::OP_SOURCE);

  bool snap;
  nsRect bounds = GetBounds(aBuilder, &snap);
  int32_t appUnitsPerDevPixel = mFrame->PresContext()->AppUnitsPerDevPixel();
  layer->SetBounds(bounds.ToNearestPixels(appUnitsPerDevPixel)); // XXX Do we need to respect the parent layer's scale here?

  return layer.forget();
}

nsRect
nsDisplayOutline::GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap) {
  *aSnap = false;
  return mFrame->GetVisualOverflowRectRelativeToSelf() + ToReferenceFrame();
}

void
nsDisplayOutline::Paint(nsDisplayListBuilder* aBuilder,
                        nsRenderingContext* aCtx) {
  // TODO join outlines together
  nsPoint offset = ToReferenceFrame();
  nsCSSRendering::PaintOutline(mFrame->PresContext(), *aCtx, mFrame,
                               mVisibleRect,
                               nsRect(offset, mFrame->GetSize()),
                               mFrame->StyleContext());
}

bool
nsDisplayOutline::IsInvisibleInRect(const nsRect& aRect)
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

void
nsDisplayLayerEventRegions::AddFrame(nsDisplayListBuilder* aBuilder,
                                     nsIFrame* aFrame)
{
  NS_ASSERTION(aBuilder->FindReferenceFrameFor(aFrame) == aBuilder->FindReferenceFrameFor(mFrame),
               "Reference frame mismatch");
  if (aBuilder->IsInsidePointerEventsNoneDoc()) {
    // Somewhere up the parent document chain is a subdocument with pointer-
    // events:none set on it (and without a mozpasspointerevents).
    return;
  }
  if (!aFrame->GetParent()) {
    MOZ_ASSERT(aFrame->GetType() == nsGkAtoms::viewportFrame);
    // Viewport frames are never event targets, other frames, like canvas frames,
    // are the event targets for any regions viewport frames may cover.
    return;
  }
  uint8_t pointerEvents = aFrame->StyleVisibility()->GetEffectivePointerEvents(aFrame);
  if (pointerEvents == NS_STYLE_POINTER_EVENTS_NONE) {
    return;
  }
  if (!aFrame->StyleVisibility()->IsVisible()) {
    return;
  }
  // XXX handle other pointerEvents values for SVG

  // XXX Do something clever here for the common case where the border box
  // is obviously entirely inside mHitRegion.
  nsRect borderBox;
  if (nsLayoutUtils::GetScrollableFrameFor(aFrame)) {
    // If the frame is content of a scrollframe, then we need to pick up the
    // area corresponding to the overflow rect as well. Otherwise the parts of
    // the overflow that are not occupied by descendants get skipped and the
    // APZ code sends touch events to the content underneath instead.
    // See https://bugzilla.mozilla.org/show_bug.cgi?id=1127773#c15.
    borderBox = aFrame->GetScrollableOverflowRect();
  } else {
    borderBox = nsRect(nsPoint(0, 0), aFrame->GetSize());
  }
  borderBox += aBuilder->ToReferenceFrame(aFrame);

  const DisplayItemClip* clip = aBuilder->ClipState().GetCurrentCombinedClip(aBuilder);
  bool borderBoxHasRoundedCorners =
    nsLayoutUtils::HasNonZeroCorner(aFrame->StyleBorder()->mBorderRadius);
  if (clip) {
    borderBox = clip->ApplyNonRoundedIntersection(borderBox);
    if (clip->GetRoundedRectCount() > 0) {
      borderBoxHasRoundedCorners = true;
    }
  }
  if (borderBoxHasRoundedCorners ||
      (aFrame->GetStateBits() & NS_FRAME_SVG_LAYOUT)) {
    mMaybeHitRegion.Or(mMaybeHitRegion, borderBox);
  } else {
    mHitRegion.Or(mHitRegion, borderBox);
  }
  if (aBuilder->GetAncestorHasApzAwareEventHandler()) {
    mDispatchToContentHitRegion.Or(mDispatchToContentHitRegion, borderBox);
  }

  // Touch action region

  uint32_t touchAction = nsLayoutUtils::GetTouchActionFromFrame(aFrame);
  if (touchAction & NS_STYLE_TOUCH_ACTION_NONE) {
    mNoActionRegion.Or(mNoActionRegion, borderBox);
  } else {
    if ((touchAction & NS_STYLE_TOUCH_ACTION_PAN_X)) {
      mHorizontalPanRegion.Or(mHorizontalPanRegion, borderBox);
    }
    if ((touchAction & NS_STYLE_TOUCH_ACTION_PAN_Y)) {
      mVerticalPanRegion.Or(mVerticalPanRegion, borderBox);
    }
  }
}

void
nsDisplayLayerEventRegions::AddInactiveScrollPort(const nsRect& aRect)
{
  mDispatchToContentHitRegion.Or(mDispatchToContentHitRegion, aRect);
}

void
nsDisplayLayerEventRegions::WriteDebugInfo(std::stringstream& aStream)
{
  if (!mHitRegion.IsEmpty()) {
    AppendToString(aStream, mHitRegion, " (hitRegion ", ")");
  }
  if (!mMaybeHitRegion.IsEmpty()) {
    AppendToString(aStream, mMaybeHitRegion, " (maybeHitRegion ", ")");
  }
  if (!mDispatchToContentHitRegion.IsEmpty()) {
    AppendToString(aStream, mDispatchToContentHitRegion, " (dispatchToContentRegion ", ")");
  }
}

nsDisplayCaret::nsDisplayCaret(nsDisplayListBuilder* aBuilder,
                               nsIFrame* aCaretFrame)
  : nsDisplayItem(aBuilder, aCaretFrame)
  , mCaret(aBuilder->GetCaret())
  , mBounds(aBuilder->GetCaretRect() + ToReferenceFrame())
  , mNeedsCustomScrollClip(false)
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
nsDisplayCaret::GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap)
{
  *aSnap = true;
  // The caret returns a rect in the coordinates of mFrame.
  return mBounds;
}

void
nsDisplayCaret::Paint(nsDisplayListBuilder* aBuilder,
                      nsRenderingContext* aCtx) {
  // Note: Because we exist, we know that the caret is visible, so we don't
  // need to check for the caret's visibility.
  mCaret->PaintCaret(aBuilder, *aCtx->GetDrawTarget(), mFrame, ToReferenceFrame());
}

nsDisplayBorder::nsDisplayBorder(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame)
  : nsDisplayItem(aBuilder, aFrame)
{
  MOZ_COUNT_CTOR(nsDisplayBorder);

  mBounds = CalculateBounds(*mFrame->StyleBorder());
}

bool
nsDisplayBorder::IsInvisibleInRect(const nsRect& aRect)
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
                                           nsRegion* aInvalidRegion)
{
  const nsDisplayBorderGeometry* geometry = static_cast<const nsDisplayBorderGeometry*>(aGeometry);
  bool snap;
  if (!geometry->mBounds.IsEqualInterior(GetBounds(aBuilder, &snap)) ||
      !geometry->mContentRect.IsEqualInterior(GetContentRect())) {
    // We can probably get away with only invalidating the difference
    // between the border and padding rects, but the XUL ui at least
    // is apparently painting a background with this?
    aInvalidRegion->Or(GetBounds(aBuilder, &snap), geometry->mBounds);
  }
}
  
void
nsDisplayBorder::Paint(nsDisplayListBuilder* aBuilder,
                       nsRenderingContext* aCtx) {
  nsPoint offset = ToReferenceFrame();
  nsCSSRendering::PaintBorder(mFrame->PresContext(), *aCtx, mFrame,
                              mVisibleRect,
                              nsRect(offset, mFrame->GetSize()),
                              mFrame->StyleContext(),
                              mFrame->GetSkipSides());
}

nsRect
nsDisplayBorder::GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap)
{
  *aSnap = true;
  return mBounds;
}

nsRect
nsDisplayBorder::CalculateBounds(const nsStyleBorder& aStyleBorder)
{
  nsRect borderBounds(ToReferenceFrame(), mFrame->GetSize());
  if (aStyleBorder.IsBorderImageLoaded()) {
    borderBounds.Inflate(aStyleBorder.GetImageOutset());
    return borderBounds;
  } else {
    nsMargin border = aStyleBorder.GetComputedBorder();
    nsRect result;
    if (border.top > 0) {
      result = nsRect(borderBounds.X(), borderBounds.Y(), borderBounds.Width(), border.top);
    }
    if (border.right > 0) {
      result.UnionRect(result, nsRect(borderBounds.XMost() - border.right, borderBounds.Y(), border.right, borderBounds.Height()));
    }
    if (border.bottom > 0) {
      result.UnionRect(result, nsRect(borderBounds.X(), borderBounds.YMost() - border.bottom, borderBounds.Width(), border.bottom));
    }
    if (border.left > 0) {
      result.UnionRect(result, nsRect(borderBounds.X(), borderBounds.Y(), border.left, borderBounds.Height()));
    }

    nscoord radii[8];
    if (mFrame->GetBorderRadii(radii)) {
      if (border.left > 0 || border.top > 0) {
        nsSize cornerSize(radii[NS_CORNER_TOP_LEFT_X], radii[NS_CORNER_TOP_LEFT_Y]);
        result.UnionRect(result, nsRect(borderBounds.TopLeft(), cornerSize));
      }
      if (border.top > 0 || border.right > 0) {
        nsSize cornerSize(radii[NS_CORNER_TOP_RIGHT_X], radii[NS_CORNER_TOP_RIGHT_Y]);
        result.UnionRect(result, nsRect(borderBounds.TopRight() - nsPoint(cornerSize.width, 0), cornerSize));
      }
      if (border.right > 0 || border.bottom > 0) {
        nsSize cornerSize(radii[NS_CORNER_BOTTOM_RIGHT_X], radii[NS_CORNER_BOTTOM_RIGHT_Y]);
        result.UnionRect(result, nsRect(borderBounds.BottomRight() - nsPoint(cornerSize.width, cornerSize.height), cornerSize));
      }
      if (border.bottom > 0 || border.left > 0) {
        nsSize cornerSize(radii[NS_CORNER_BOTTOM_LEFT_X], radii[NS_CORNER_BOTTOM_LEFT_Y]);
        result.UnionRect(result, nsRect(borderBounds.BottomLeft() - nsPoint(0, cornerSize.height), cornerSize));
      }
    }

    return result;
  }
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
  nsRegionRectIterator iter(aRegion);
  while (true) {
    const nsRect* r = iter.Next();
    if (r && !accumulated.IsEmpty() &&
        accumulated.YMost() >= r->y - accumulationMargin) {
      accumulated.UnionRect(accumulated, *r);
      continue;
    }

    if (!accumulated.IsEmpty()) {
      aRects->AppendElement(accumulated);
      accumulated.SetEmpty();
    }

    if (!r)
      break;

    accumulated = *r;
  }
}

void
nsDisplayBoxShadowOuter::Paint(nsDisplayListBuilder* aBuilder,
                               nsRenderingContext* aCtx) {
  nsPoint offset = ToReferenceFrame();
  nsRect borderRect = mFrame->VisualBorderRectRelativeToSelf() + offset;
  nsPresContext* presContext = mFrame->PresContext();
  nsAutoTArray<nsRect,10> rects;
  ComputeDisjointRectangles(mVisibleRegion, &rects);

  PROFILER_LABEL("nsDisplayBoxShadowOuter", "Paint",
    js::ProfileEntry::Category::GRAPHICS);

  for (uint32_t i = 0; i < rects.Length(); ++i) {
    nsCSSRendering::PaintBoxShadowOuter(presContext, *aCtx, mFrame,
                                        borderRect, rects[i], mOpacity);
  }
}

nsRect
nsDisplayBoxShadowOuter::GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap) {
  *aSnap = false;
  return mBounds;
}

nsRect
nsDisplayBoxShadowOuter::GetBoundsInternal() {
  return nsLayoutUtils::GetBoxShadowRectForFrame(mFrame, mFrame->GetSize()) +
         ToReferenceFrame();
}

bool
nsDisplayBoxShadowOuter::IsInvisibleInRect(const nsRect& aRect)
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

  // Store the actual visible region
  mVisibleRegion.And(*aVisibleRegion, mVisibleRect);
  return true;
}

void
nsDisplayBoxShadowOuter::ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                                   const nsDisplayItemGeometry* aGeometry,
                                                   nsRegion* aInvalidRegion)
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
                               nsRenderingContext* aCtx) {
  nsPoint offset = ToReferenceFrame();
  nsRect borderRect = nsRect(offset, mFrame->GetSize());
  nsPresContext* presContext = mFrame->PresContext();
  nsAutoTArray<nsRect,10> rects;
  ComputeDisjointRectangles(mVisibleRegion, &rects);

  PROFILER_LABEL("nsDisplayBoxShadowInner", "Paint",
    js::ProfileEntry::Category::GRAPHICS);

  DrawTarget* drawTarget = aCtx->GetDrawTarget();
  gfxContext* gfx = aCtx->ThebesContext();
  int32_t appUnitsPerDevPixel = mFrame->PresContext()->AppUnitsPerDevPixel();

  for (uint32_t i = 0; i < rects.Length(); ++i) {
    gfx->Save();
    gfx->Clip(NSRectToSnappedRect(rects[i], appUnitsPerDevPixel, *drawTarget));
    nsCSSRendering::PaintBoxShadowInner(presContext, *aCtx, mFrame,
                                        borderRect, rects[i]);
    gfx->Restore();
  }
}

bool
nsDisplayBoxShadowInner::ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                           nsRegion* aVisibleRegion) {
  if (!nsDisplayItem::ComputeVisibility(aBuilder, aVisibleRegion)) {
    return false;
  }

  // Store the actual visible region
  mVisibleRegion.And(*aVisibleRegion, mVisibleRect);
  return true;
}

nsDisplayWrapList::nsDisplayWrapList(nsDisplayListBuilder* aBuilder,
                                     nsIFrame* aFrame, nsDisplayList* aList)
  : nsDisplayItem(aBuilder, aFrame)
  , mOverrideZIndex(0)
  , mHasZIndexOverride(false)
{
  MOZ_COUNT_CTOR(nsDisplayWrapList);

  mBaseVisibleRect = mVisibleRect;

  mList.AppendToTop(aList);
  UpdateBounds(aBuilder);

  if (!aFrame || !aFrame->IsTransformed()) {
    return;
  }

  // If the frame is a preserve-3d parent, then we will create transforms
  // inside this list afterwards (see WrapPreserve3DList in nsFrame.cpp).
  // In this case we will always be outside of the transform, so share
  // our parents reference frame.
  if (aFrame->Preserves3DChildren()) {
    mReferenceFrame = 
      aBuilder->FindReferenceFrameFor(GetTransformRootFrame(aFrame));
    mToReferenceFrame = aFrame->GetOffsetToCrossDoc(mReferenceFrame);
  } else {
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
    //
    // Preserve-3d can cause us to have multiple nsDisplayTransform
    // children.
    nsDisplayItem *i = mList.GetBottom();
    if (i && (!i->GetAbove() || i->GetType() == TYPE_TRANSFORM) &&
        i->Frame() == mFrame) {
      mReferenceFrame = i->ReferenceFrame();
      mToReferenceFrame = i->ToReferenceFrame();
    }
  }
  mVisibleRect = aBuilder->GetDirtyRect() +
      aBuilder->GetCurrentFrameOffsetToReferenceFrame();
}

nsDisplayWrapList::nsDisplayWrapList(nsDisplayListBuilder* aBuilder,
                                     nsIFrame* aFrame, nsDisplayItem* aItem)
  : nsDisplayItem(aBuilder, aFrame)
  , mOverrideZIndex(0)
  , mHasZIndexOverride(false)
{
  MOZ_COUNT_CTOR(nsDisplayWrapList);

  mBaseVisibleRect = mVisibleRect;

  mList.AppendToTop(aItem);
  UpdateBounds(aBuilder);
  
  if (!aFrame || !aFrame->IsTransformed()) {
    return;
  }

  if (aFrame->Preserves3DChildren()) {
    mReferenceFrame = 
      aBuilder->FindReferenceFrameFor(GetTransformRootFrame(aFrame));
    mToReferenceFrame = aFrame->GetOffsetToCrossDoc(mReferenceFrame);
  } else {
    // See the previous nsDisplayWrapList constructor
    if (aItem->Frame() == aFrame) {
      mReferenceFrame = aItem->ReferenceFrame();
      mToReferenceFrame = aItem->ToReferenceFrame();
    }
  }
  mVisibleRect = aBuilder->GetDirtyRect() +
      aBuilder->GetCurrentFrameOffsetToReferenceFrame();
}

nsDisplayWrapList::~nsDisplayWrapList() {
  mList.DeleteAll();

  MOZ_COUNT_DTOR(nsDisplayWrapList);
}

void
nsDisplayWrapList::HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
                           HitTestState* aState, nsTArray<nsIFrame*> *aOutFrames) {
  mList.HitTest(aBuilder, aRect, aState, aOutFrames);
}

nsRect
nsDisplayWrapList::GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap) {
  *aSnap = false;
  return mBounds;
}

bool
nsDisplayWrapList::ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                     nsRegion* aVisibleRegion) {
  // Convert the passed in visible region to our appunits.
  nsRegion visibleRegion;
  // mVisibleRect has been clipped to GetClippedBounds
  visibleRegion.And(*aVisibleRegion, mVisibleRect);
  nsRegion originalVisibleRegion = visibleRegion;

  bool retval =
    mList.ComputeVisibilityForSublist(aBuilder, &visibleRegion,
                                      mVisibleRect);

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
                                   bool* aSnap) {
  *aSnap = false;
  nsRegion result;
  if (mList.IsOpaque()) {
    // Everything within GetBounds that's visible is opaque.
    result = GetBounds(aBuilder, aSnap);
  }
  return result;
}

bool nsDisplayWrapList::IsUniform(nsDisplayListBuilder* aBuilder, nscolor* aColor) {
  // We could try to do something but let's conservatively just return false.
  return false;
}

void nsDisplayWrapList::Paint(nsDisplayListBuilder* aBuilder,
                              nsRenderingContext* aCtx) {
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
                              nsIFrame* aExpectedAnimatedGeometryRootForChildren)
{
  LayerState result = LAYER_INACTIVE;
  for (nsDisplayItem* i = aList.GetBottom(); i; i = i->GetAbove()) {
    if (result == LAYER_INACTIVE &&
        nsLayoutUtils::GetAnimatedGeometryRootFor(i, aBuilder, aManager) !=
          aExpectedAnimatedGeometryRootForChildren) {
      result = LAYER_ACTIVE;
    }

    LayerState state = i->GetLayerState(aBuilder, aManager, aParameters);
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

nsRect nsDisplayWrapList::GetComponentAlphaBounds(nsDisplayListBuilder* aBuilder)
{
  nsRect bounds;
  for (nsDisplayItem* i = mList.GetBottom(); i; i = i->GetAbove()) {
    bounds.UnionRect(bounds, i->GetComponentAlphaBounds(aBuilder));
  }
  return bounds;
}

void
nsDisplayWrapList::SetVisibleRect(const nsRect& aRect)
{
  mVisibleRect = aRect;
}

void
nsDisplayWrapList::SetReferenceFrame(const nsIFrame* aFrame)
{
  mReferenceFrame = aFrame;
  mToReferenceFrame = mFrame->GetOffsetToCrossDoc(mReferenceFrame);
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
                                   nsIFrame* aFrame, nsDisplayList* aList)
    : nsDisplayWrapList(aBuilder, aFrame, aList)
    , mOpacity(aFrame->StyleDisplay()->mOpacity) {
  MOZ_COUNT_CTOR(nsDisplayOpacity);
}

#ifdef NS_BUILD_REFCNT_LOGGING
nsDisplayOpacity::~nsDisplayOpacity() {
  MOZ_COUNT_DTOR(nsDisplayOpacity);
}
#endif

nsRegion nsDisplayOpacity::GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                                           bool* aSnap) {
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
  nsRefPtr<Layer> container = aManager->GetLayerBuilder()->
    BuildContainerLayerFor(aBuilder, aManager, mFrame, this, &mList,
                           aContainerParameters, nullptr,
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
IsItemTooSmallForActiveLayer(nsDisplayItem* aItem)
{
  nsIntRect visibleDevPixels = aItem->GetVisibleRect().ToOutsidePixels(
          aItem->Frame()->PresContext()->AppUnitsPerDevPixel());
  static const int MIN_ACTIVE_LAYER_SIZE_DEV_PIXELS = 16;
  return visibleDevPixels.Size() <
    nsIntSize(MIN_ACTIVE_LAYER_SIZE_DEV_PIXELS, MIN_ACTIVE_LAYER_SIZE_DEV_PIXELS);
}

bool
nsDisplayOpacity::NeedsActiveLayer(nsDisplayListBuilder* aBuilder)
{
  if (ActiveLayerTracker::IsStyleAnimated(aBuilder, mFrame, eCSSProperty_opacity) &&
      !IsItemTooSmallForActiveLayer(this))
    return true;
  if (nsLayoutUtils::HasAnimationsForCompositor(mFrame, eCSSProperty_opacity)) {
    return true;
  }
  return false;
}

void
nsDisplayOpacity::ApplyOpacity(nsDisplayListBuilder* aBuilder,
                             float aOpacity,
                             const DisplayItemClip* aClip)
{
  NS_ASSERTION(CanApplyOpacity(), "ApplyOpacity should be allowed");
  mOpacity = mOpacity * aOpacity;
  if (aClip) {
    IntersectClip(aBuilder, *aClip);
  }
}

bool
nsDisplayOpacity::CanApplyOpacity() const
{
  return true;
}

bool
nsDisplayOpacity::ShouldFlattenAway(nsDisplayListBuilder* aBuilder)
{
  if (NeedsActiveLayer(aBuilder))
    return false;

  nsDisplayItem* child = mList.GetBottom();
  // Only try folding our opacity down if we have at most three children
  // that don't overlap and can all apply the opacity to themselves.
  if (!child) {
    return false;
  }
  struct {
    nsDisplayItem* item;
    nsRect bounds;
  } children[3];
  bool snap;
  uint32_t numChildren = 0;
  for (; numChildren < ArrayLength(children) && child; numChildren++, child = child->GetAbove()) {
    if (!child->CanApplyOpacity()) {
      return false;
    }
    children[numChildren].item = child;
    children[numChildren].bounds = child->GetBounds(aBuilder, &snap);
  }
  if (child) {
    // we have a fourth (or more) child
    return false;
  }

  for (uint32_t i = 0; i < numChildren; i++) {
    for (uint32_t j = i+1; j < numChildren; j++) {
      if (children[i].bounds.Intersects(children[j].bounds)) {
        return false;
      }
    }
  }

  for (uint32_t i = 0; i < numChildren; i++) {
    children[i].item->ApplyOpacity(aBuilder, mOpacity, mClip);
  }
  return true;
}

nsDisplayItem::LayerState
nsDisplayOpacity::GetLayerState(nsDisplayListBuilder* aBuilder,
                                LayerManager* aManager,
                                const ContainerLayerParameters& aParameters) {
  if (NeedsActiveLayer(aBuilder))
    return LAYER_ACTIVE;

  return RequiredLayerStateForChildren(aBuilder, aManager, aParameters, mList,
    nsLayoutUtils::GetAnimatedGeometryRootFor(this, aBuilder, aManager));
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

bool nsDisplayOpacity::TryMerge(nsDisplayListBuilder* aBuilder, nsDisplayItem* aItem) {
  if (aItem->GetType() != TYPE_OPACITY)
    return false;
  // items for the same content element should be merged into a single
  // compositing group
  // aItem->GetUnderlyingFrame() returns non-null because it's nsDisplayOpacity
  if (aItem->Frame()->GetContent() != mFrame->GetContent())
    return false;
  if (aItem->GetClip() != GetClip())
    return false;
  MergeFromTrackingMergedFrames(static_cast<nsDisplayOpacity*>(aItem));
  return true;
}

void
nsDisplayOpacity::WriteDebugInfo(std::stringstream& aStream)
{
  aStream << " (opacity " << mOpacity << ")";
}

nsDisplayMixBlendMode::nsDisplayMixBlendMode(nsDisplayListBuilder* aBuilder,
                                             nsIFrame* aFrame, nsDisplayList* aList,
                                             uint32_t aFlags)
: nsDisplayWrapList(aBuilder, aFrame, aList) {
  MOZ_COUNT_CTOR(nsDisplayMixBlendMode);
}

#ifdef NS_BUILD_REFCNT_LOGGING
nsDisplayMixBlendMode::~nsDisplayMixBlendMode() {
  MOZ_COUNT_DTOR(nsDisplayMixBlendMode);
}
#endif

nsRegion nsDisplayMixBlendMode::GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                                                bool* aSnap) {
  *aSnap = false;
  // We are never considered opaque
  return nsRegion();
}

LayerState
nsDisplayMixBlendMode::GetLayerState(nsDisplayListBuilder* aBuilder,
                                     LayerManager* aManager,
                                     const ContainerLayerParameters& aParameters)
{
  gfxContext::GraphicsOperator op = nsCSSRendering::GetGFXBlendMode(mFrame->StyleDisplay()->mMixBlendMode);
  if (aManager->SupportsMixBlendMode(gfx::CompositionOpForOp(op))) {
    return LAYER_ACTIVE;
  }
  return LAYER_INACTIVE;
}

// nsDisplayMixBlendMode uses layers for rendering
already_AddRefed<Layer>
nsDisplayMixBlendMode::BuildLayer(nsDisplayListBuilder* aBuilder,
                                  LayerManager* aManager,
                                  const ContainerLayerParameters& aContainerParameters) {
  ContainerLayerParameters newContainerParameters = aContainerParameters;
  newContainerParameters.mDisableSubpixelAntialiasingInDescendants = true;

  nsRefPtr<Layer> container = aManager->GetLayerBuilder()->
  BuildContainerLayerFor(aBuilder, aManager, mFrame, this, &mList,
                         newContainerParameters, nullptr);
  if (!container) {
    return nullptr;
  }

  container->DeprecatedSetMixBlendMode(nsCSSRendering::GetGFXBlendMode(mFrame->StyleDisplay()->mMixBlendMode));

  return container.forget();
}

bool nsDisplayMixBlendMode::ComputeVisibility(nsDisplayListBuilder* aBuilder,
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

bool nsDisplayMixBlendMode::TryMerge(nsDisplayListBuilder* aBuilder, nsDisplayItem* aItem) {
  if (aItem->GetType() != TYPE_MIX_BLEND_MODE)
    return false;
  // items for the same content element should be merged into a single
  // compositing group
  // aItem->GetUnderlyingFrame() returns non-null because it's nsDisplayOpacity
  if (aItem->Frame()->GetContent() != mFrame->GetContent())
    return false;
  if (aItem->GetClip() != GetClip())
    return false;
  MergeFromTrackingMergedFrames(static_cast<nsDisplayMixBlendMode*>(aItem));
  return true;
}

nsDisplayBlendContainer::nsDisplayBlendContainer(nsDisplayListBuilder* aBuilder,
                                                 nsIFrame* aFrame, nsDisplayList* aList,
                                                 BlendModeSet& aContainedBlendModes)
    : nsDisplayWrapList(aBuilder, aFrame, aList)
    , mContainedBlendModes(aContainedBlendModes)
    , mCanBeActive(true)
{
  MOZ_COUNT_CTOR(nsDisplayBlendContainer);
}

nsDisplayBlendContainer::nsDisplayBlendContainer(nsDisplayListBuilder* aBuilder,
                                                 nsIFrame* aFrame, nsDisplayList* aList)
    : nsDisplayWrapList(aBuilder, aFrame, aList)
    , mCanBeActive(false)
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

  nsRefPtr<Layer> container = aManager->GetLayerBuilder()->
  BuildContainerLayerFor(aBuilder, aManager, mFrame, this, &mList,
                         newContainerParameters, nullptr);
  if (!container) {
    return nullptr;
  }
  
  container->SetForceIsolatedGroup(true);
  return container.forget();
}

bool nsDisplayBlendContainer::TryMerge(nsDisplayListBuilder* aBuilder, nsDisplayItem* aItem) {
  if (aItem->GetType() != TYPE_BLEND_CONTAINER)
    return false;
  // items for the same content element should be merged into a single
  // compositing group
  // aItem->GetUnderlyingFrame() returns non-null because it's nsDisplayOpacity
  if (aItem->Frame()->GetContent() != mFrame->GetContent())
    return false;
  if (aItem->GetClip() != GetClip())
    return false;
  MergeFromTrackingMergedFrames(static_cast<nsDisplayBlendContainer*>(aItem));
  return true;
}

nsDisplayOwnLayer::nsDisplayOwnLayer(nsDisplayListBuilder* aBuilder,
                                     nsIFrame* aFrame, nsDisplayList* aList,
                                     uint32_t aFlags, ViewID aScrollTarget,
                                     float aScrollbarThumbRatio)
    : nsDisplayWrapList(aBuilder, aFrame, aList)
    , mFlags(aFlags)
    , mScrollTarget(aScrollTarget)
    , mScrollbarThumbRatio(aScrollbarThumbRatio)
{
  MOZ_COUNT_CTOR(nsDisplayOwnLayer);
}

#ifdef NS_BUILD_REFCNT_LOGGING
nsDisplayOwnLayer::~nsDisplayOwnLayer() {
  MOZ_COUNT_DTOR(nsDisplayOwnLayer);
}
#endif

// nsDisplayOpacity uses layers for rendering
already_AddRefed<Layer>
nsDisplayOwnLayer::BuildLayer(nsDisplayListBuilder* aBuilder,
                              LayerManager* aManager,
                              const ContainerLayerParameters& aContainerParameters)
{
  nsRefPtr<ContainerLayer> layer = aManager->GetLayerBuilder()->
    BuildContainerLayerFor(aBuilder, aManager, mFrame, this, &mList,
                           aContainerParameters, nullptr,
                           FrameLayerBuilder::CONTAINER_ALLOW_PULL_BACKGROUND_COLOR);
  if (mFlags & VERTICAL_SCROLLBAR) {
    layer->SetScrollbarData(mScrollTarget, Layer::ScrollDirection::VERTICAL, mScrollbarThumbRatio);
  }
  if (mFlags & HORIZONTAL_SCROLLBAR) {
    layer->SetScrollbarData(mScrollTarget, Layer::ScrollDirection::HORIZONTAL, mScrollbarThumbRatio);
  }
  if (mFlags & SCROLLBAR_CONTAINER) {
    layer->SetIsScrollbarContainer();
  }

  if (mFlags & GENERATE_SUBDOC_INVALIDATIONS) {
    mFrame->PresContext()->SetNotifySubDocInvalidationData(layer);
  }
  return layer.forget();
}

nsDisplaySubDocument::nsDisplaySubDocument(nsDisplayListBuilder* aBuilder,
                                           nsIFrame* aFrame, nsDisplayList* aList,
                                           uint32_t aFlags)
    : nsDisplayOwnLayer(aBuilder, aFrame, aList, aFlags)
    , mScrollParentId(aBuilder->GetCurrentScrollParentId())
{
  MOZ_COUNT_CTOR(nsDisplaySubDocument);
  mForceDispatchToContentRegion =
    aBuilder->IsBuildingLayerEventRegions() &&
    nsLayoutUtils::HasDocumentLevelListenersForApzAwareEvents(aFrame->PresContext()->PresShell());
}

#ifdef NS_BUILD_REFCNT_LOGGING
nsDisplaySubDocument::~nsDisplaySubDocument() {
  MOZ_COUNT_DTOR(nsDisplaySubDocument);
}
#endif

already_AddRefed<Layer>
nsDisplaySubDocument::BuildLayer(nsDisplayListBuilder* aBuilder,
                                 LayerManager* aManager,
                                 const ContainerLayerParameters& aContainerParameters) {
  nsPresContext* presContext = mFrame->PresContext();
  nsIFrame* rootScrollFrame = presContext->PresShell()->GetRootScrollFrame();
  ContainerLayerParameters params = aContainerParameters;
  if ((mFlags & GENERATE_SCROLLABLE_LAYER) &&
      rootScrollFrame->GetContent() &&
      nsLayoutUtils::GetCriticalDisplayPort(rootScrollFrame->GetContent(), nullptr)) {
    params.mInLowPrecisionDisplayPort = true; 
  }

  nsRefPtr<Layer> layer = nsDisplayOwnLayer::BuildLayer(aBuilder, aManager, params);
  layer->AsContainerLayer()->SetEventRegionsOverride(mForceDispatchToContentRegion
    ? EventRegionsOverride::ForceDispatchToContent
    : EventRegionsOverride::NoOverride);
  return layer.forget();
}

UniquePtr<FrameMetrics>
nsDisplaySubDocument::ComputeFrameMetrics(Layer* aLayer,
                                          const ContainerLayerParameters& aContainerParameters)
{
  if (!(mFlags & GENERATE_SCROLLABLE_LAYER)) {
    return UniquePtr<FrameMetrics>(nullptr);
  }

  nsPresContext* presContext = mFrame->PresContext();
  nsIFrame* rootScrollFrame = presContext->PresShell()->GetRootScrollFrame();
  bool isRootContentDocument = presContext->IsRootContentDocument();
  nsIPresShell* presShell = presContext->PresShell();
  ContainerLayerParameters params(
      aContainerParameters.mXScale * presShell->GetResolution(),
      aContainerParameters.mYScale * presShell->GetResolution(),
      nsIntPoint(), aContainerParameters);
  if ((mFlags & GENERATE_SCROLLABLE_LAYER) &&
      rootScrollFrame->GetContent() &&
      nsLayoutUtils::GetCriticalDisplayPort(rootScrollFrame->GetContent(), nullptr)) {
    params.mInLowPrecisionDisplayPort = true;
  }

  nsRect viewport = mFrame->GetRect() -
                    mFrame->GetPosition() +
                    mFrame->GetOffsetToCrossDoc(ReferenceFrame());

  return MakeUnique<FrameMetrics>(
    nsLayoutUtils::ComputeFrameMetrics(
      mFrame, rootScrollFrame, rootScrollFrame->GetContent(), ReferenceFrame(),
      aLayer, mScrollParentId, viewport, Nothing(),
      isRootContentDocument, params));
}

static bool
UseDisplayPortForViewport(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                          nsRect* aDisplayPort = nullptr)
{
  return aBuilder->IsPaintingToWindow() &&
      nsLayoutUtils::ViewportHasDisplayPort(aFrame->PresContext(), aDisplayPort);
}

nsRect
nsDisplaySubDocument::GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap)
{
  bool usingDisplayPort = UseDisplayPortForViewport(aBuilder, mFrame);

  if ((mFlags & GENERATE_SCROLLABLE_LAYER) && usingDisplayPort) {
    *aSnap = false;
    return mFrame->GetRect() + aBuilder->ToReferenceFrame(mFrame);
  }

  return nsDisplayOwnLayer::GetBounds(aBuilder, aSnap);
}

bool
nsDisplaySubDocument::ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                        nsRegion* aVisibleRegion)
{
  nsRect displayport;
  bool usingDisplayPort = UseDisplayPortForViewport(aBuilder, mFrame, &displayport);

  if (!(mFlags & GENERATE_SCROLLABLE_LAYER) || !usingDisplayPort) {
    return nsDisplayWrapList::ComputeVisibility(aBuilder, aVisibleRegion);
  }

  nsRegion childVisibleRegion;
  // The visible region for the children may be much bigger than the hole we
  // are viewing the children from, so that the compositor process has enough
  // content to asynchronously pan while content is being refreshed.
  childVisibleRegion = displayport + mFrame->GetOffsetToCrossDoc(ReferenceFrame());

  nsRect boundedRect =
    childVisibleRegion.GetBounds().Intersect(mList.GetBounds(aBuilder));
  bool visible = mList.ComputeVisibilityForSublist(
    aBuilder, &childVisibleRegion, boundedRect,
    usingDisplayPort ? mFrame : nullptr);

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
nsDisplaySubDocument::ShouldBuildLayerEvenIfInvisible(nsDisplayListBuilder* aBuilder)
{
  bool usingDisplayPort = UseDisplayPortForViewport(aBuilder, mFrame);

  if ((mFlags & GENERATE_SCROLLABLE_LAYER) && usingDisplayPort) {
    return true;
  }

  return nsDisplayOwnLayer::ShouldBuildLayerEvenIfInvisible(aBuilder);
}

nsRegion
nsDisplaySubDocument::GetOpaqueRegion(nsDisplayListBuilder* aBuilder, bool* aSnap)
{
  bool usingDisplayPort = UseDisplayPortForViewport(aBuilder, mFrame);

  if ((mFlags & GENERATE_SCROLLABLE_LAYER) && usingDisplayPort) {
    *aSnap = false;
    return nsRegion();
  }

  return nsDisplayOwnLayer::GetOpaqueRegion(aBuilder, aSnap);
}

nsDisplayResolution::nsDisplayResolution(nsDisplayListBuilder* aBuilder,
                                         nsIFrame* aFrame, nsDisplayList* aList,
                                         uint32_t aFlags)
    : nsDisplaySubDocument(aBuilder, aFrame, aList, aFlags) {
  MOZ_COUNT_CTOR(nsDisplayResolution);
}

#ifdef NS_BUILD_REFCNT_LOGGING
nsDisplayResolution::~nsDisplayResolution() {
  MOZ_COUNT_DTOR(nsDisplayResolution);
}
#endif

already_AddRefed<Layer>
nsDisplayResolution::BuildLayer(nsDisplayListBuilder* aBuilder,
                                LayerManager* aManager,
                                const ContainerLayerParameters& aContainerParameters) {
  nsIPresShell* presShell = mFrame->PresContext()->PresShell();
  ContainerLayerParameters containerParameters(
    presShell->GetResolution(), presShell->GetResolution(), nsIntPoint(),
    aContainerParameters);

  nsRefPtr<Layer> layer = nsDisplaySubDocument::BuildLayer(
    aBuilder, aManager, containerParameters);
  layer->SetPostScale(1.0f / presShell->GetResolution(),
                      1.0f / presShell->GetResolution());
  layer->AsContainerLayer()->SetScaleToResolution(
      presShell->ScaleToResolution(), presShell->GetResolution());
  return layer.forget();
}

nsDisplayStickyPosition::nsDisplayStickyPosition(nsDisplayListBuilder* aBuilder,
                                                 nsIFrame* aFrame,
                                                 nsDisplayList* aList)
  : nsDisplayOwnLayer(aBuilder, aFrame, aList)
{
  MOZ_COUNT_CTOR(nsDisplayStickyPosition);
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
  nsRefPtr<Layer> layer =
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
    mFrame, presContext, aContainerParameters, /* clip is fixed = */ true);

  ViewID scrollId = nsLayoutUtils::FindOrCreateIDFor(
    stickyScrollContainer->ScrollFrame()->GetScrolledFrame()->GetContent());

  float factor = presContext->AppUnitsPerDevPixel();
  nsRect outer;
  nsRect inner;
  stickyScrollContainer->GetScrollRanges(mFrame, &outer, &inner);
  LayerRect stickyOuter(NSAppUnitsToFloatPixels(outer.x, factor) *
                          aContainerParameters.mXScale,
                        NSAppUnitsToFloatPixels(outer.y, factor) *
                          aContainerParameters.mYScale,
                        NSAppUnitsToFloatPixels(outer.width, factor) *
                          aContainerParameters.mXScale,
                        NSAppUnitsToFloatPixels(outer.height, factor) *
                          aContainerParameters.mYScale);
  LayerRect stickyInner(NSAppUnitsToFloatPixels(inner.x, factor) *
                          aContainerParameters.mXScale,
                        NSAppUnitsToFloatPixels(inner.y, factor) *
                          aContainerParameters.mYScale,
                        NSAppUnitsToFloatPixels(inner.width, factor) *
                          aContainerParameters.mXScale,
                        NSAppUnitsToFloatPixels(inner.height, factor) *
                          aContainerParameters.mYScale);
  layer->SetStickyPositionData(scrollId, stickyOuter, stickyInner);

  return layer.forget();
}

bool nsDisplayStickyPosition::TryMerge(nsDisplayListBuilder* aBuilder, nsDisplayItem* aItem) {
  if (aItem->GetType() != TYPE_STICKY_POSITION)
    return false;
  // Items with the same fixed position frame can be merged.
  nsDisplayStickyPosition* other = static_cast<nsDisplayStickyPosition*>(aItem);
  if (other->mFrame != mFrame)
    return false;
  if (aItem->GetClip() != GetClip())
    return false;
  MergeFromTrackingMergedFrames(other);
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
  , mIgnoreIfCompositorSupportsBlending(false)
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

  if (mIgnoreIfCompositorSupportsBlending) {
    // This item was created pessimistically because, during display list
    // building, we encountered a mix blend mode. If our layer manager
    // supports compositing this mix blend mode, we don't actually need to
    // create a scroll info layer.
    if (aManager->SupportsMixBlendModes(mContainedBlendModes)) {
      return nullptr;
    }
  }

  ContainerLayerParameters params = aContainerParameters;
  if (mScrolledFrame->GetContent() &&
      nsLayoutUtils::GetCriticalDisplayPort(mScrolledFrame->GetContent(), nullptr)) {
    params.mInLowPrecisionDisplayPort = true;
  }

  return aManager->GetLayerBuilder()->
    BuildContainerLayerFor(aBuilder, aManager, mFrame, this, &mList,
                           params, nullptr,
                           FrameLayerBuilder::CONTAINER_ALLOW_PULL_BACKGROUND_COLOR);
}

LayerState
nsDisplayScrollInfoLayer::GetLayerState(nsDisplayListBuilder* aBuilder,
                                    LayerManager* aManager,
                                    const ContainerLayerParameters& aParameters)
{
  return LAYER_ACTIVE_EMPTY;
}

UniquePtr<FrameMetrics>
nsDisplayScrollInfoLayer::ComputeFrameMetrics(Layer* aLayer,
                                              const ContainerLayerParameters& aContainerParameters)
{
  ContainerLayerParameters params = aContainerParameters;
  if (mScrolledFrame->GetContent() &&
      nsLayoutUtils::GetCriticalDisplayPort(mScrolledFrame->GetContent(), nullptr)) {
    params.mInLowPrecisionDisplayPort = true; 
  }

  nsRect viewport = mScrollFrame->GetRect() -
                    mScrollFrame->GetPosition() +
                    mScrollFrame->GetOffsetToCrossDoc(ReferenceFrame());

  return UniquePtr<FrameMetrics>(new FrameMetrics(
    nsLayoutUtils::ComputeFrameMetrics(
      mScrolledFrame, mScrollFrame, mScrollFrame->GetContent(),
      ReferenceFrame(), aLayer,
      mScrollParentId, viewport, Nothing(), false, params)));
}

void
nsDisplayScrollInfoLayer::IgnoreIfCompositorSupportsBlending(BlendModeSet aBlendModes)
{
  mContainedBlendModes += aBlendModes;
  mIgnoreIfCompositorSupportsBlending = true;
}

void
nsDisplayScrollInfoLayer::UnsetIgnoreIfCompositorSupportsBlending()
{
  mIgnoreIfCompositorSupportsBlending = false;
}

bool
nsDisplayScrollInfoLayer::ContainedInMixBlendMode() const
{
  return mIgnoreIfCompositorSupportsBlending;
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
                             uint32_t aFlags)
    : nsDisplaySubDocument(aBuilder, aFrame, aList, aFlags)
    , mAPD(aAPD), mParentAPD(aParentAPD) {
  MOZ_COUNT_CTOR(nsDisplayZoom);
}

#ifdef NS_BUILD_REFCNT_LOGGING
nsDisplayZoom::~nsDisplayZoom() {
  MOZ_COUNT_DTOR(nsDisplayZoom);
}
#endif

nsRect nsDisplayZoom::GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap)
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
  visibleRegion.And(*aVisibleRegion, mVisibleRect);
  visibleRegion = visibleRegion.ScaleToOtherAppUnitsRoundOut(mParentAPD, mAPD);
  nsRegion originalVisibleRegion = visibleRegion;

  nsRect transformedVisibleRect =
    mVisibleRect.ScaleToOtherAppUnitsRoundOut(mParentAPD, mAPD);
  bool retval;
  // If we are to generate a scrollable layer we call
  // nsDisplaySubDocument::ComputeVisibility to make the necessary adjustments
  // for ComputeVisibility, it does all it's calculations in the child APD.
  bool usingDisplayPort = UseDisplayPortForViewport(aBuilder, mFrame);
  if (!(mFlags & GENERATE_SCROLLABLE_LAYER) || !usingDisplayPort) {
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
                                       const nsRect& aChildrenVisibleRect,
                                       ComputeTransformFunction aTransformGetter,
                                       uint32_t aIndex) 
  : nsDisplayItem(aBuilder, aFrame)
  , mStoredList(aBuilder, aFrame, aList)
  , mTransformGetter(aTransformGetter)
  , mChildrenVisibleRect(aChildrenVisibleRect)
  , mIndex(aIndex)
{
  MOZ_COUNT_CTOR(nsDisplayTransform);
  MOZ_ASSERT(aFrame, "Must have a frame!");
  MOZ_ASSERT(!aFrame->IsTransformed(), "Can't specify a transform getter for a transformed frame!");
  Init(aBuilder);
}

void
nsDisplayTransform::SetReferenceFrameToAncestor(nsDisplayListBuilder* aBuilder)
{
  mReferenceFrame =
    aBuilder->FindReferenceFrameFor(GetTransformRootFrame(mFrame));
  mToReferenceFrame = mFrame->GetOffsetToCrossDoc(mReferenceFrame);
  mVisibleRect = aBuilder->GetDirtyRect() + mToReferenceFrame;
}

void
nsDisplayTransform::Init(nsDisplayListBuilder* aBuilder)
{
  mStoredList.SetClip(aBuilder, DisplayItemClip::NoClip());
  mStoredList.SetVisibleRect(mChildrenVisibleRect);
  mMaybePrerender = ShouldPrerenderTransformedContent(aBuilder, mFrame);

  const nsStyleDisplay* disp = mFrame->StyleDisplay();
  if ((disp->mWillChangeBitField & NS_STYLE_WILL_CHANGE_TRANSFORM)) {
    // We will only pre-render if this will-change is on budget.
    mMaybePrerender = true;
  }

  if (mMaybePrerender) {
    bool snap;
    mVisibleRect = GetBounds(aBuilder, &snap);
  }
}

nsDisplayTransform::nsDisplayTransform(nsDisplayListBuilder* aBuilder,
                                       nsIFrame *aFrame, nsDisplayList *aList,
                                       const nsRect& aChildrenVisibleRect,
                                       uint32_t aIndex)
  : nsDisplayItem(aBuilder, aFrame)
  , mStoredList(aBuilder, aFrame, aList)
  , mTransformGetter(nullptr)
  , mChildrenVisibleRect(aChildrenVisibleRect)
  , mIndex(aIndex)
{
  MOZ_COUNT_CTOR(nsDisplayTransform);
  MOZ_ASSERT(aFrame, "Must have a frame!");
  SetReferenceFrameToAncestor(aBuilder);
  Init(aBuilder);
}

nsDisplayTransform::nsDisplayTransform(nsDisplayListBuilder* aBuilder,
                                       nsIFrame *aFrame, nsDisplayItem *aItem,
                                       const nsRect& aChildrenVisibleRect,
                                       uint32_t aIndex)
  : nsDisplayItem(aBuilder, aFrame)
  , mStoredList(aBuilder, aFrame, aItem)
  , mTransformGetter(nullptr)
  , mChildrenVisibleRect(aChildrenVisibleRect)
  , mIndex(aIndex)
{
  MOZ_COUNT_CTOR(nsDisplayTransform);
  MOZ_ASSERT(aFrame, "Must have a frame!");
  SetReferenceFrameToAncestor(aBuilder);
  Init(aBuilder);
}

/* Returns the delta specified by the -transform-origin property.
 * This is a positive delta, meaning that it indicates the direction to move
 * to get from (0, 0) of the frame to the transform origin.  This function is
 * called off the main thread.
 */
/* static */ Point3D
nsDisplayTransform::GetDeltaToTransformOrigin(const nsIFrame* aFrame,
                                              float aAppUnitsPerPixel,
                                              const nsRect* aBoundsOverride)
{
  NS_PRECONDITION(aFrame, "Can't get delta for a null frame!");
  NS_PRECONDITION(aFrame->IsTransformed() || aFrame->StyleDisplay()->BackfaceIsHidden(),
                  "Shouldn't get a delta for an untransformed frame!");

  if (!aFrame->IsTransformed()) {
    return Point3D();
  }

  /* For both of the coordinates, if the value of -transform is a
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
  float coords[2];
  TransformReferenceBox::DimensionGetter dimensionGetter[] =
    { &TransformReferenceBox::Width, &TransformReferenceBox::Height };
  TransformReferenceBox::DimensionGetter offsetGetter[] =
    { &TransformReferenceBox::X, &TransformReferenceBox::Y };

  for (uint8_t index = 0; index < 2; ++index) {
    /* If the -transform-origin specifies a percentage, take the percentage
     * of the size of the box.
     */
    const nsStyleCoord &coord = display->mTransformOrigin[index];
    if (coord.GetUnit() == eStyleUnit_Calc) {
      const nsStyleCoord::Calc *calc = coord.GetCalcValue();
      coords[index] =
        NSAppUnitsToFloatPixels((refBox.*dimensionGetter[index])(), aAppUnitsPerPixel) *
          calc->mPercent +
        NSAppUnitsToFloatPixels(calc->mLength, aAppUnitsPerPixel);
    } else if (coord.GetUnit() == eStyleUnit_Percent) {
      coords[index] =
        NSAppUnitsToFloatPixels((refBox.*dimensionGetter[index])(), aAppUnitsPerPixel) *
        coord.GetPercentValue();
    } else {
      MOZ_ASSERT(coord.GetUnit() == eStyleUnit_Coord, "unexpected unit");
      coords[index] =
        NSAppUnitsToFloatPixels(coord.GetCoordValue(), aAppUnitsPerPixel);
    }

    if (aFrame->GetStateBits() & NS_FRAME_SVG_LAYOUT) {
      // SVG frames (unlike other frames) have a reference box that can be (and
      // typically is) offset from the TopLeft() of the frame. We need to
      // account for that here.
      coords[index] +=
        NSAppUnitsToFloatPixels((refBox.*offsetGetter[index])(), aAppUnitsPerPixel);
    }
  }

  return Point3D(coords[0], coords[1],
                 NSAppUnitsToFloatPixels(display->mTransformOrigin[2].GetCoordValue(),
                                         aAppUnitsPerPixel));
}

/* Returns the delta specified by the -moz-perspective-origin property.
 * This is a positive delta, meaning that it indicates the direction to move
 * to get from (0, 0) of the frame to the perspective origin. This function is
 * called off the main thread.
 */
/* static */ Point3D
nsDisplayTransform::GetDeltaToPerspectiveOrigin(const nsIFrame* aFrame,
                                                float aAppUnitsPerPixel)
{
  NS_PRECONDITION(aFrame, "Can't get delta for a null frame!");
  NS_PRECONDITION(aFrame->IsTransformed() || aFrame->StyleDisplay()->BackfaceIsHidden(),
                  "Shouldn't get a delta for an untransformed frame!");

  if (!aFrame->IsTransformed()) {
    return Point3D();
  }

  /* For both of the coordinates, if the value of -moz-perspective-origin is a
   * percentage, it's relative to the size of the frame.  Otherwise, if it's
   * a distance, it's already computed for us!
   */

  //TODO: Should this be using our bounds or the parent's bounds?
  // How do we handle aBoundsOverride in the latter case?
  nsIFrame* cbFrame = aFrame->GetContainingBlock(nsIFrame::SKIP_SCROLLED_FRAME);
  if (!cbFrame) {
    return Point3D();
  }
  const nsStyleDisplay* display = cbFrame->StyleDisplay();
  TransformReferenceBox refBox(cbFrame);

  /* Allows us to access named variables by index. */
  Point3D result;
  result.z = 0.0f;
  gfx::Float* coords[2] = {&result.x, &result.y};
  TransformReferenceBox::DimensionGetter dimensionGetter[] =
    { &TransformReferenceBox::Width, &TransformReferenceBox::Height };

  for (uint8_t index = 0; index < 2; ++index) {
    /* If the -transform-origin specifies a percentage, take the percentage
     * of the size of the box.
     */
    const nsStyleCoord &coord = display->mPerspectiveOrigin[index];
    if (coord.GetUnit() == eStyleUnit_Calc) {
      const nsStyleCoord::Calc *calc = coord.GetCalcValue();
      *coords[index] =
        NSAppUnitsToFloatPixels((refBox.*dimensionGetter[index])(), aAppUnitsPerPixel) *
          calc->mPercent +
        NSAppUnitsToFloatPixels(calc->mLength, aAppUnitsPerPixel);
    } else if (coord.GetUnit() == eStyleUnit_Percent) {
      *coords[index] =
        NSAppUnitsToFloatPixels((refBox.*dimensionGetter[index])(), aAppUnitsPerPixel) *
        coord.GetPercentValue();
    } else {
      MOZ_ASSERT(coord.GetUnit() == eStyleUnit_Coord, "unexpected unit");
      *coords[index] =
        NSAppUnitsToFloatPixels(coord.GetCoordValue(), aAppUnitsPerPixel);
    }
  }

  nsPoint parentOffset = aFrame->GetOffsetTo(cbFrame);
  Point3D gfxOffset(
            NSAppUnitsToFloatPixels(parentOffset.x, aAppUnitsPerPixel),
            NSAppUnitsToFloatPixels(parentOffset.y, aAppUnitsPerPixel),
            0.0f);

  return result - gfxOffset;
}

nsDisplayTransform::FrameTransformProperties::FrameTransformProperties(const nsIFrame* aFrame,
                                                                       float aAppUnitsPerPixel,
                                                                       const nsRect* aBoundsOverride)
  : mFrame(aFrame)
  , mTransformList(aFrame->StyleDisplay()->mSpecifiedTransform)
  , mToTransformOrigin(GetDeltaToTransformOrigin(aFrame, aAppUnitsPerPixel, aBoundsOverride))
  , mChildPerspective(0)
{
  nsIFrame* cbFrame = aFrame->GetContainingBlock(nsIFrame::SKIP_SCROLLED_FRAME);
  if (cbFrame) {
    const nsStyleDisplay* display = cbFrame->StyleDisplay();
    if (display->mChildPerspective.GetUnit() == eStyleUnit_Coord) {
      mChildPerspective = display->mChildPerspective.GetCoordValue();
      // Calling GetDeltaToPerspectiveOrigin can be expensive, so we avoid
      // calling it unnecessarily.
      if (mChildPerspective > 0.0) {
        mToPerspectiveOrigin = GetDeltaToPerspectiveOrigin(aFrame, aAppUnitsPerPixel);
      }
    }
  }
}

/* Wraps up the -transform matrix in a change-of-basis matrix pair that
 * translates from local coordinate space to transform coordinate space, then
 * hands it back.
 */
Matrix4x4
nsDisplayTransform::GetResultingTransformMatrix(const FrameTransformProperties& aProperties,
                                                const nsPoint& aOrigin,
                                                float aAppUnitsPerPixel,
                                                const nsRect* aBoundsOverride,
                                                nsIFrame** aOutAncestor)
{
  return GetResultingTransformMatrixInternal(aProperties, aOrigin, aAppUnitsPerPixel,
                                             aBoundsOverride, aOutAncestor, false);
}
 
Matrix4x4
nsDisplayTransform::GetResultingTransformMatrix(const nsIFrame* aFrame,
                                                const nsPoint& aOrigin,
                                                float aAppUnitsPerPixel,
                                                const nsRect* aBoundsOverride,
                                                nsIFrame** aOutAncestor,
                                                bool aOffsetByOrigin)
{
  FrameTransformProperties props(aFrame,
                                 aAppUnitsPerPixel,
                                 aBoundsOverride);

  return GetResultingTransformMatrixInternal(props, aOrigin, aAppUnitsPerPixel,
                                             aBoundsOverride, aOutAncestor,
                                             aOffsetByOrigin);
}

Matrix4x4
nsDisplayTransform::GetResultingTransformMatrixInternal(const FrameTransformProperties& aProperties,
                                                        const nsPoint& aOrigin,
                                                        float aAppUnitsPerPixel,
                                                        const nsRect* aBoundsOverride,
                                                        nsIFrame** aOutAncestor,
                                                        bool aOffsetByOrigin)
{
  const nsIFrame *frame = aProperties.mFrame;

  if (aOutAncestor) {
    *aOutAncestor = nsLayoutUtils::GetCrossDocParentFrame(frame);
  }

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
  RuleNodeCacheConditions dummy;
  Matrix4x4 result;
  // Call IsSVGTransformed() regardless of the value of
  // disp->mSpecifiedTransform, since we still need any transformFromSVGParent.
  Matrix svgTransform, transformFromSVGParent;
  bool hasSVGTransforms =
    frame && frame->IsSVGTransformed(&svgTransform, &transformFromSVGParent);
  bool hasTransformFromSVGParent =
    hasSVGTransforms && !transformFromSVGParent.IsIdentity();
  /* Transformed frames always have a transform, or are preserving 3d (and might still have perspective!) */
  if (aProperties.mTransformList) {
    result = nsStyleTransformMatrix::ReadTransforms(aProperties.mTransformList->mHead,
                                                    frame ? frame->StyleContext() : nullptr,
                                                    frame ? frame->PresContext() : nullptr,
                                                    dummy, refBox, aAppUnitsPerPixel);
  } else if (hasSVGTransforms) {
    // Correct the translation components for zoom:
    float pixelsPerCSSPx = frame->PresContext()->AppUnitsPerCSSPixel() /
                             aAppUnitsPerPixel;
    svgTransform._31 *= pixelsPerCSSPx;
    svgTransform._32 *= pixelsPerCSSPx;
    result = Matrix4x4::From2D(svgTransform);
  }

  if (aProperties.mChildPerspective > 0.0) {
    Matrix4x4 perspective;
    perspective._34 =
      -1.0 / NSAppUnitsToFloatPixels(aProperties.mChildPerspective, aAppUnitsPerPixel);
    /* At the point when perspective is applied, we have been translated to the transform origin.
     * The translation to the perspective origin is the difference between these values.
     */
    perspective.ChangeBasis(aProperties.GetToPerspectiveOrigin() - aProperties.mToTransformOrigin);
    result = result * perspective;
  }

  /* Account for the -transform-origin property by translating the
   * coordinate space to the new origin.
   */
  Point3D newOrigin =
    Point3D(NSAppUnitsToFloatPixels(aOrigin.x, aAppUnitsPerPixel),
            NSAppUnitsToFloatPixels(aOrigin.y, aAppUnitsPerPixel),
            0.0f);
  Point3D roundedOrigin(hasSVGTransforms ? newOrigin.x : NS_round(newOrigin.x),
                        hasSVGTransforms ? newOrigin.y : NS_round(newOrigin.y),
                        0);

  if (!hasSVGTransforms || !hasTransformFromSVGParent) {
    // This is a simplification of the following |else| block, the
    // simplification being possible because we don't need to apply
    // mToTransformOrigin between two transforms.
    Point3D offsets = roundedOrigin + aProperties.mToTransformOrigin;
    if (aOffsetByOrigin) {
      // We can fold the final translation by roundedOrigin into the first matrix
      // basis change translation. This is more stable against variation due to
      // insufficient floating point precision than reversing the translation
      // afterwards.
      result.PreTranslate(-aProperties.mToTransformOrigin);
      result.PostTranslate(offsets);
    } else {
      result.ChangeBasis(offsets);
    }
  } else {
    Point3D refBoxOffset(NSAppUnitsToFloatPixels(refBox.X(), aAppUnitsPerPixel),
                         NSAppUnitsToFloatPixels(refBox.Y(), aAppUnitsPerPixel),
                         0);
    // We have both a transform and children-only transform. The
    // 'transform-origin' must apply between the two, so we need to apply it
    // now before we apply transformFromSVGParent. Since mToTransformOrigin is
    // relative to the frame's TopLeft(), we need to convert it to SVG user
    // space by subtracting refBoxOffset. (Then after applying
    // transformFromSVGParent we have to reapply refBoxOffset below.)
    result.ChangeBasis(aProperties.mToTransformOrigin - refBoxOffset);

    // Now apply the children-only transforms, converting the translation
    // components to device pixels:
    float pixelsPerCSSPx =
      frame->PresContext()->AppUnitsPerCSSPixel() / aAppUnitsPerPixel;
    transformFromSVGParent._31 *= pixelsPerCSSPx;
    transformFromSVGParent._32 *= pixelsPerCSSPx;
    result = result * Matrix4x4::From2D(transformFromSVGParent);

    // Similar to the code in the |if| block above, but since we've accounted
    // for mToTransformOrigin so we don't include that. We also need to reapply
    // refBoxOffset.
    Point3D offsets = roundedOrigin + refBoxOffset;
    if (aOffsetByOrigin) {
      result.PreTranslate(-refBoxOffset);
      result.PostTranslate(offsets);
    } else {
      result.ChangeBasis(offsets);
    }
  }

  if (frame && frame->Preserves3D()) {
    // Include the transform set on our parent
    NS_ASSERTION(frame->GetParent() &&
                 frame->GetParent()->IsTransformed() &&
                 frame->GetParent()->Preserves3DChildren(),
                 "Preserve3D mismatch!");
    FrameTransformProperties props(frame->GetParent(),
                                   aAppUnitsPerPixel,
                                   nullptr);

    // If this frame isn't transformed (but we exist for backface-visibility),
    // then we're not a reference frame so no offset to origin will be added. Our
    // parent transform however *is* the reference frame, so we pass true for
    // aOffsetByOrigin to convert into the correct coordinate space.
    Matrix4x4 parent =
      GetResultingTransformMatrixInternal(props,
                                          aOrigin - frame->GetPosition(),
                                          aAppUnitsPerPixel, nullptr,
                                          aOutAncestor, !frame->IsTransformed());
    result = result * parent;
  }

  return result;
}

bool
nsDisplayOpacity::CanUseAsyncAnimations(nsDisplayListBuilder* aBuilder)
{
  if (ActiveLayerTracker::IsStyleAnimated(aBuilder, mFrame, eCSSProperty_opacity)) {
    return true;
  }

  if (nsLayoutUtils::IsAnimationLoggingEnabled()) {
    nsCString message;
    message.AppendLiteral("Performance warning: Async animation disabled because frame was not marked active for opacity animation");
    AnimationCollection::LogAsyncAnimationFailure(message,
                                                  Frame()->GetContent());
  }
  return false;
}

bool
nsDisplayTransform::ShouldPrerender(nsDisplayListBuilder* aBuilder) {
  if (!mMaybePrerender) {
    return false;
  }

  if (ShouldPrerenderTransformedContent(aBuilder, mFrame)) {
    return true;
  }

  const nsStyleDisplay* disp = mFrame->StyleDisplay();
  if ((disp->mWillChangeBitField & NS_STYLE_WILL_CHANGE_TRANSFORM) &&
      aBuilder->IsInWillChangeBudget(mFrame, mFrame->GetSize())) {
    return true;
  }

  return false;
}

bool
nsDisplayTransform::CanUseAsyncAnimations(nsDisplayListBuilder* aBuilder)
{
  if (mMaybePrerender) {
    // TODO We need to make sure that if we use async animation we actually
    // pre-render even if we're out of will change budget.
    return true;
  }
  DebugOnly<bool> prerender = ShouldPrerenderTransformedContent(aBuilder, mFrame, true);
  NS_ASSERTION(!prerender, "Something changed under us!");
  return false;
}

/* static */ bool
nsDisplayTransform::ShouldPrerenderTransformedContent(nsDisplayListBuilder* aBuilder,
                                                      nsIFrame* aFrame,
                                                      bool aLogAnimations)
{
  // Elements whose transform has been modified recently, or which
  // have a compositor-animated transform, can be prerendered. An element
  // might have only just had its transform animated in which case
  // the ActiveLayerManager may not have been notified yet.
  if (!ActiveLayerTracker::IsStyleMaybeAnimated(aFrame, eCSSProperty_transform) &&
      !nsLayoutUtils::HasAnimationsForCompositor(aFrame, eCSSProperty_transform)) {
    if (aLogAnimations) {
      nsCString message;
      message.AppendLiteral("Performance warning: Async animation disabled because frame was not marked active for transform animation");
      AnimationCollection::LogAsyncAnimationFailure(message,
                                                    aFrame->GetContent());
    }
    return false;
  }

  nsSize refSize = aBuilder->RootReferenceFrame()->GetSize();
  // Only prerender if the transformed frame's size is <= the
  // reference frame size (~viewport), allowing a 1/8th fuzz factor
  // for shadows, borders, etc.
  refSize += nsSize(refSize.width / 8, refSize.height / 8);
  nsSize frameSize = aFrame->GetVisualOverflowRectRelativeToSelf().Size();
  nscoord maxInAppUnits = nscoord_MAX;
  if (frameSize <= refSize) {
    maxInAppUnits = aFrame->PresContext()->DevPixelsToAppUnits(4096);
    nsRect visual = aFrame->GetVisualOverflowRect();
    if (visual.width <= maxInAppUnits && visual.height <= maxInAppUnits) {
      return true;
    }
  }

  if (aLogAnimations) {
    nsRect visual = aFrame->GetVisualOverflowRect();

    nsCString message;
    message.AppendLiteral("Performance warning: Async animation disabled because frame size (");
    message.AppendInt(nsPresContext::AppUnitsToIntCSSPixels(frameSize.width));
    message.AppendLiteral(", ");
    message.AppendInt(nsPresContext::AppUnitsToIntCSSPixels(frameSize.height));
    message.AppendLiteral(") is bigger than the viewport (");
    message.AppendInt(nsPresContext::AppUnitsToIntCSSPixels(refSize.width));
    message.AppendLiteral(", ");
    message.AppendInt(nsPresContext::AppUnitsToIntCSSPixels(refSize.height));
    message.AppendLiteral(") or the visual rectangle (");
    message.AppendInt(nsPresContext::AppUnitsToIntCSSPixels(visual.width));
    message.AppendLiteral(", ");
    message.AppendInt(nsPresContext::AppUnitsToIntCSSPixels(visual.height));
    message.AppendLiteral(") is larger than the max allowable value (");
    message.AppendInt(nsPresContext::AppUnitsToIntCSSPixels(maxInAppUnits));
    message.Append(')');
    AnimationCollection::LogAsyncAnimationFailure(message,
                                                  aFrame->GetContent());
  }
  return false;
}

/* If the matrix is singular, or a hidden backface is shown, the frame won't be visible or hit. */
static bool IsFrameVisible(nsIFrame* aFrame, const Matrix4x4& aMatrix)
{
  if (aMatrix.IsSingular()) {
    return false;
  }
  if (aFrame->StyleDisplay()->mBackfaceVisibility == NS_STYLE_BACKFACE_VISIBILITY_HIDDEN &&
      aMatrix.IsBackfaceVisible()) {
    return false;
  }
  return true;
}

const Matrix4x4&
nsDisplayTransform::GetTransform()
{
  if (mTransform.IsIdentity()) {
    float scale = mFrame->PresContext()->AppUnitsPerDevPixel();
    Point3D newOrigin =
      Point3D(NSAppUnitsToFloatPixels(mToReferenceFrame.x, scale),
              NSAppUnitsToFloatPixels(mToReferenceFrame.y, scale),
              0.0f);
    if (mTransformGetter) {
      mTransform = mTransformGetter(mFrame, scale);
      mTransform.ChangeBasis(newOrigin.x, newOrigin.y, newOrigin.z);
    } else {
      /**
       * Passing true as the final argument means that we want to shift the
       * coordinates to be relative to our reference frame instead of relative
       * to this frame.
       * When we have preserve-3d, our reference frame is already guaranteed
       * to be an ancestor of the preserve-3d chain, so we only need to do
       * this once.
       */
      mTransform = GetResultingTransformMatrix(mFrame, ToReferenceFrame(),
                                               scale, nullptr, nullptr,
                                               mFrame->IsTransformed());
    }
  }
  return mTransform;
}

bool
nsDisplayTransform::ShouldBuildLayerEvenIfInvisible(nsDisplayListBuilder* aBuilder)
{
  return ShouldPrerender(aBuilder);
}

already_AddRefed<Layer> nsDisplayTransform::BuildLayer(nsDisplayListBuilder *aBuilder,
                                                       LayerManager *aManager,
                                                       const ContainerLayerParameters& aContainerParameters)
{
  const Matrix4x4& newTransformMatrix = GetTransform();

  if (mFrame->StyleDisplay()->mBackfaceVisibility == NS_STYLE_BACKFACE_VISIBILITY_HIDDEN &&
      newTransformMatrix.IsBackfaceVisible()) {
    return nullptr;
  }

  uint32_t flags = ShouldPrerender(aBuilder) ?
    FrameLayerBuilder::CONTAINER_NOT_CLIPPED_BY_ANCESTORS : 0;
  flags |= FrameLayerBuilder::CONTAINER_ALLOW_PULL_BACKGROUND_COLOR;
  nsRefPtr<ContainerLayer> container = aManager->GetLayerBuilder()->
    BuildContainerLayerFor(aBuilder, aManager, mFrame, this, mStoredList.GetChildren(),
                           aContainerParameters, &newTransformMatrix, flags);

  if (!container) {
    return nullptr;
  }

  // Add the preserve-3d flag for this layer, BuildContainerLayerFor clears all flags,
  // so we never need to explicitely unset this flag.
  if (mFrame->Preserves3D() || mFrame->Preserves3DChildren()) {
    container->SetContentFlags(container->GetContentFlags() | Layer::CONTENT_EXTEND_3D_CONTEXT);
  } else {
    container->SetContentFlags(container->GetContentFlags() & ~Layer::CONTENT_EXTEND_3D_CONTEXT);
  }

  nsDisplayListBuilder::AddAnimationsAndTransitionsToLayer(container, aBuilder,
                                                           this, mFrame,
                                                           eCSSProperty_transform);
  if (ShouldPrerender(aBuilder)) {
    container->SetUserData(nsIFrame::LayerIsPrerenderedDataKey(),
                           /*the value is irrelevant*/nullptr);
    container->SetContentFlags(container->GetContentFlags() | Layer::CONTENT_MAY_CHANGE_TRANSFORM);
  } else {
    container->RemoveUserData(nsIFrame::LayerIsPrerenderedDataKey());
    container->SetContentFlags(container->GetContentFlags() & ~Layer::CONTENT_MAY_CHANGE_TRANSFORM);
  }
  return container.forget();
}

nsDisplayItem::LayerState
nsDisplayTransform::GetLayerState(nsDisplayListBuilder* aBuilder,
                                  LayerManager* aManager,
                                  const ContainerLayerParameters& aParameters) {
  // If the transform is 3d, or the layer takes part in preserve-3d sorting
  // then we *always* want this to be an active layer.
  if (!GetTransform().Is2D() || mFrame->Preserves3D()) {
    return LAYER_ACTIVE_FORCE;
  }
  // Here we check if the *post-transform* bounds of this item are big enough
  // to justify an active layer.
  if (ActiveLayerTracker::IsStyleAnimated(aBuilder, mFrame, eCSSProperty_transform) &&
      !IsItemTooSmallForActiveLayer(this))
    return LAYER_ACTIVE;
  if (nsLayoutUtils::HasAnimationsForCompositor(mFrame, eCSSProperty_transform)) {
    return LAYER_ACTIVE;
  }

  const nsStyleDisplay* disp = mFrame->StyleDisplay();
  if ((disp->mWillChangeBitField & NS_STYLE_WILL_CHANGE_TRANSFORM)) {
    return LAYER_ACTIVE;
  }

  // Expect the child display items to have this frame as their animated
  // geometry root (since it will be their reference frame). If they have a
  // different animated geometry root, we'll make this an active layer so the
  // animation can be accelerated.
  return RequiredLayerStateForChildren(aBuilder, aManager, aParameters,
    *mStoredList.GetChildren(), Frame());
}

bool nsDisplayTransform::ComputeVisibility(nsDisplayListBuilder *aBuilder,
                                             nsRegion *aVisibleRegion)
{
  /* As we do this, we need to be sure to
   * untransform the visible rect, since we want everything that's painting to
   * think that it's painting in its original rectangular coordinate space.
   * If we can't untransform, take the entire overflow rect */
  nsRect untransformedVisibleRect;
  if (ShouldPrerender(aBuilder) ||
      !UntransformVisibleRect(aBuilder, &untransformedVisibleRect))
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
  /* Here's how this works:
   * 1. Get the matrix.  If it's singular, abort (clearly we didn't hit
   *    anything).
   * 2. Invert the matrix.
   * 3. Use it to transform the rect into the correct space.
   * 4. Pass that rect down through to the list's version of HitTest.
   */
  // GetTransform always operates in dev pixels.
  float factor = mFrame->PresContext()->AppUnitsPerDevPixel();
  Matrix4x4 matrix = GetTransform();

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
  Matrix4x4 matrix = GetTransform();

  NS_ASSERTION(IsFrameVisible(mFrame, matrix), "We can't have hit a frame that isn't visible!");

  Matrix4x4 inverse = matrix;
  inverse.Invert();
  Point4D point = inverse.ProjectPoint(Point(NSAppUnitsToFloatPixels(aPoint.x, factor),
                                             NSAppUnitsToFloatPixels(aPoint.y, factor)));
  NS_ASSERTION(point.HasPositiveWCoord(), "Why are we trying to get the depth for a point we didn't hit?");

  Point point2d = point.As2DPoint();

  Point3D transformed = matrix * Point3D(point2d.x, point2d.y, 0);
  return transformed.z;
}

/* The bounding rectangle for the object is the overflow rectangle translated
 * by the reference point.
 */
nsRect nsDisplayTransform::GetBounds(nsDisplayListBuilder *aBuilder, bool* aSnap)
{
  nsRect untransformedBounds = MaybePrerender() ?
    mFrame->GetVisualOverflowRectRelativeToSelf() :
    mStoredList.GetBounds(aBuilder, aSnap);
  *aSnap = false;
  // GetTransform always operates in dev pixels.
  float factor = mFrame->PresContext()->AppUnitsPerDevPixel();
  return nsLayoutUtils::MatrixTransformRect(untransformedBounds,
                                            GetTransform(),
                                            factor);
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
nsRegion nsDisplayTransform::GetOpaqueRegion(nsDisplayListBuilder *aBuilder,
                                             bool* aSnap)
{
  *aSnap = false;
  nsRect untransformedVisible;
  // If we're going to prerender all our content, pretend like we
  // don't have opqaue content so that everything under us is rendered
  // as well.  That will increase graphics memory usage if our frame
  // covers the entire window, but it allows our transform to be
  // updated extremely cheaply, without invalidating any other
  // content.
  if (MaybePrerender() ||
      !UntransformVisibleRect(aBuilder, &untransformedVisible)) {
      return nsRegion();
  }

  const Matrix4x4& matrix = GetTransform();

  nsRegion result;
  Matrix matrix2d;
  bool tmpSnap;
  if (matrix.Is2D(&matrix2d) &&
      matrix2d.PreservesAxisAlignedRectangles() &&
      mStoredList.GetOpaqueRegion(aBuilder, &tmpSnap).Contains(untransformedVisible)) {
    result = mVisibleRect.Intersect(GetBounds(aBuilder, &tmpSnap));
  }
  return result;
}

/* The transform is uniform if it fills the entire bounding rect and the
 * wrapped list is uniform.  See GetOpaqueRegion for discussion of why this
 * works.
 */
bool nsDisplayTransform::IsUniform(nsDisplayListBuilder *aBuilder, nscolor* aColor)
{
  nsRect untransformedVisible;
  if (!UntransformVisibleRect(aBuilder, &untransformedVisible)) {
    return false;
  }
  const Matrix4x4& matrix = GetTransform();

  Matrix matrix2d;
  return matrix.Is2D(&matrix2d) &&
         matrix2d.PreservesAxisAlignedRectangles() &&
         mStoredList.GetVisibleRect().Contains(untransformedVisible) &&
         mStoredList.IsUniform(aBuilder, aColor);
}

/* If UNIFIED_CONTINUATIONS is defined, we can merge two display lists that
 * share the same underlying content.  Otherwise, doing so results in graphical
 * glitches.
 */
#ifndef UNIFIED_CONTINUATIONS

bool
nsDisplayTransform::TryMerge(nsDisplayListBuilder *aBuilder,
                             nsDisplayItem *aItem)
{
  return false;
}

#else

bool
nsDisplayTransform::TryMerge(nsDisplayListBuilder *aBuilder,
                             nsDisplayItem *aItem)
{
  NS_PRECONDITION(aItem, "Why did you try merging with a null item?");
  NS_PRECONDITION(aBuilder, "Why did you try merging with a null builder?");

  /* Make sure that we're dealing with two transforms. */
  if (aItem->GetType() != TYPE_TRANSFORM)
    return false;

  /* Check to see that both frames are part of the same content. */
  if (aItem->Frame()->GetContent() != mFrame->GetContent())
    return false;

  if (aItem->GetClip() != GetClip())
    return false;

  /* Now, move everything over to this frame and signal that
   * we merged things!
   */
  mStoredList.MergeFromTrackingMergedFrames(&static_cast<nsDisplayTransform*>(aItem)->mStoredList);
  return true;
}

#endif

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
                                         const nsPoint &aOrigin,
                                         const nsRect* aBoundsOverride)
{
  NS_PRECONDITION(aFrame, "Can't take the transform based on a null frame!");

  float factor = aFrame->PresContext()->AppUnitsPerDevPixel();
  return nsLayoutUtils::MatrixTransformRect
    (aUntransformedBounds,
     GetResultingTransformMatrix(aFrame, aOrigin, factor, aBoundsOverride),
     factor);
}

nsRect nsDisplayTransform::TransformRectOut(const nsRect &aUntransformedBounds,
                                            const nsIFrame* aFrame,
                                            const nsPoint &aOrigin,
                                            const nsRect* aBoundsOverride)
{
  NS_PRECONDITION(aFrame, "Can't take the transform based on a null frame!");

  float factor = aFrame->PresContext()->AppUnitsPerDevPixel();
  return nsLayoutUtils::MatrixTransformRectOut
    (aUntransformedBounds,
     GetResultingTransformMatrix(aFrame, aOrigin, factor, aBoundsOverride),
     factor);
}

bool nsDisplayTransform::UntransformRect(const nsRect &aTransformedBounds,
                                         const nsRect &aChildBounds,
                                         const nsIFrame* aFrame,
                                         const nsPoint &aOrigin,
                                         nsRect *aOutRect)
{
  NS_PRECONDITION(aFrame, "Can't take the transform based on a null frame!");

  float factor = aFrame->PresContext()->AppUnitsPerDevPixel();

  Matrix4x4 transform = GetResultingTransformMatrix(aFrame, aOrigin, factor, nullptr);
  if (transform.IsSingular()) {
    return false;
  }

  Rect result(NSAppUnitsToFloatPixels(aTransformedBounds.x, factor),
              NSAppUnitsToFloatPixels(aTransformedBounds.y, factor),
              NSAppUnitsToFloatPixels(aTransformedBounds.width, factor),
              NSAppUnitsToFloatPixels(aTransformedBounds.height, factor));

  Rect childGfxBounds(NSAppUnitsToFloatPixels(aChildBounds.x, factor),
                      NSAppUnitsToFloatPixels(aChildBounds.y, factor),
                      NSAppUnitsToFloatPixels(aChildBounds.width, factor),
                      NSAppUnitsToFloatPixels(aChildBounds.height, factor));

  result = transform.Inverse().ProjectRectBounds(result, childGfxBounds);
  *aOutRect = nsLayoutUtils::RoundGfxRectToAppRect(ThebesRect(result), factor);
  return true;
}

bool nsDisplayTransform::UntransformVisibleRect(nsDisplayListBuilder* aBuilder,
                                                nsRect *aOutRect)
{
  const Matrix4x4& matrix = GetTransform();
  if (matrix.IsSingular())
    return false;

  // GetTransform always operates in dev pixels.
  float factor = mFrame->PresContext()->AppUnitsPerDevPixel();
  Rect result(NSAppUnitsToFloatPixels(mVisibleRect.x, factor),
              NSAppUnitsToFloatPixels(mVisibleRect.y, factor),
              NSAppUnitsToFloatPixels(mVisibleRect.width, factor),
              NSAppUnitsToFloatPixels(mVisibleRect.height, factor));

  bool snap;
  nsRect childBounds = mStoredList.GetBounds(aBuilder, &snap);
  Rect childGfxBounds(NSAppUnitsToFloatPixels(childBounds.x, factor),
                      NSAppUnitsToFloatPixels(childBounds.y, factor),
                      NSAppUnitsToFloatPixels(childBounds.width, factor),
                      NSAppUnitsToFloatPixels(childBounds.height, factor));

  /* We want to untransform the matrix, so invert the transformation first! */
  result = matrix.Inverse().ProjectRectBounds(result, childGfxBounds);

  *aOutRect = nsLayoutUtils::RoundGfxRectToAppRect(ThebesRect(result), factor);

  return true;
}

void
nsDisplayTransform::WriteDebugInfo(std::stringstream& aStream)
{
  AppendToString(aStream, GetTransform());
}

nsDisplayItemGeometry*
nsCharClipDisplayItem::AllocateGeometry(nsDisplayListBuilder* aBuilder)
{
  return new nsCharClipGeometry(this, aBuilder);
}

void
nsCharClipDisplayItem::ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                         const nsDisplayItemGeometry* aGeometry,
                                         nsRegion* aInvalidRegion)
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
                                         nsIFrame* aFrame, nsDisplayList* aList)
    : nsDisplayWrapList(aBuilder, aFrame, aList),
      mEffectsBounds(aFrame->GetVisualOverflowRectRelativeToSelf())
{
  MOZ_COUNT_CTOR(nsDisplaySVGEffects);
}

#ifdef NS_BUILD_REFCNT_LOGGING
nsDisplaySVGEffects::~nsDisplaySVGEffects()
{
  MOZ_COUNT_DTOR(nsDisplaySVGEffects);
}
#endif

nsDisplayVR::nsDisplayVR(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                         nsDisplayList* aList, mozilla::gfx::VRHMDInfo* aHMD)
  : nsDisplayOwnLayer(aBuilder, aFrame, aList)
  , mHMD(aHMD)
{
}

already_AddRefed<Layer>
nsDisplayVR::BuildLayer(nsDisplayListBuilder* aBuilder,
                        LayerManager* aManager,
                        const ContainerLayerParameters& aContainerParameters)
{
  ContainerLayerParameters newContainerParameters = aContainerParameters;
  uint32_t flags = FrameLayerBuilder::CONTAINER_NOT_CLIPPED_BY_ANCESTORS |
                   FrameLayerBuilder::CONTAINER_ALLOW_PULL_BACKGROUND_COLOR;
  nsRefPtr<ContainerLayer> container = aManager->GetLayerBuilder()->
    BuildContainerLayerFor(aBuilder, aManager, mFrame, this, &mList,
                           newContainerParameters, nullptr, flags);

  container->SetVRHMDInfo(mHMD);
  container->SetUserData(nsIFrame::LayerIsPrerenderedDataKey(),
                         /*the value is irrelevant*/nullptr);

  return container.forget();
}
nsRegion nsDisplaySVGEffects::GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                                              bool* aSnap)
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

void
nsDisplaySVGEffects::PaintAsLayer(nsDisplayListBuilder* aBuilder,
                                  nsRenderingContext* aCtx,
                                  LayerManager* aManager)
{
  nsSVGIntegrationUtils::PaintFramesWithEffects(*aCtx->ThebesContext(), mFrame,
                                                mVisibleRect,
                                                aBuilder, aManager);
}

LayerState
nsDisplaySVGEffects::GetLayerState(nsDisplayListBuilder* aBuilder,
                                   LayerManager* aManager,
                                   const ContainerLayerParameters& aParameters)
{
  return LAYER_SVG_EFFECTS;
}

already_AddRefed<Layer>
nsDisplaySVGEffects::BuildLayer(nsDisplayListBuilder* aBuilder,
                                LayerManager* aManager,
                                const ContainerLayerParameters& aContainerParameters)
{
  const nsIContent* content = mFrame->GetContent();
  bool hasSVGLayout = (mFrame->GetStateBits() & NS_FRAME_SVG_LAYOUT);
  if (hasSVGLayout) {
    nsISVGChildFrame *svgChildFrame = do_QueryFrame(mFrame);
    if (!svgChildFrame || !mFrame->GetContent()->IsSVGElement()) {
      NS_ASSERTION(false, "why?");
      return nullptr;
    }
    if (!static_cast<const nsSVGElement*>(content)->HasValidDimensions()) {
      return nullptr; // The SVG spec says not to draw filters for this
    }
  }

  float opacity = mFrame->StyleDisplay()->mOpacity;
  if (opacity == 0.0f)
    return nullptr;

  nsIFrame* firstFrame =
    nsLayoutUtils::FirstContinuationOrIBSplitSibling(mFrame);
  nsSVGEffects::EffectProperties effectProperties =
    nsSVGEffects::GetEffectProperties(firstFrame);

  bool isOK = effectProperties.HasNoFilterOrHasValidFilter();
  effectProperties.GetClipPathFrame(&isOK);
  effectProperties.GetMaskFrame(&isOK);

  if (!isOK) {
    return nullptr;
  }

  ContainerLayerParameters newContainerParameters = aContainerParameters;
  if (effectProperties.HasValidFilter()) {
    newContainerParameters.mDisableSubpixelAntialiasingInDescendants = true;
  }

  nsRefPtr<ContainerLayer> container = aManager->GetLayerBuilder()->
    BuildContainerLayerFor(aBuilder, aManager, mFrame, this, &mList,
                           newContainerParameters, nullptr);

  return container.forget();
}

bool nsDisplaySVGEffects::ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                              nsRegion* aVisibleRegion) {
  nsPoint offset = ToReferenceFrame();
  nsRect dirtyRect =
    nsSVGIntegrationUtils::GetRequiredSourceForInvalidArea(mFrame,
                                                           mVisibleRect - offset) +
    offset;

  // Our children may be made translucent or arbitrarily deformed so we should
  // not allow them to subtract area from aVisibleRegion.
  nsRegion childrenVisible(dirtyRect);
  nsRect r = dirtyRect.Intersect(mList.GetBounds(aBuilder));
  mList.ComputeVisibilityForSublist(aBuilder, &childrenVisible, r);
  return true;
}

bool nsDisplaySVGEffects::TryMerge(nsDisplayListBuilder* aBuilder, nsDisplayItem* aItem)
{
  if (aItem->GetType() != TYPE_SVG_EFFECTS)
    return false;
  // items for the same content element should be merged into a single
  // compositing group
  // aItem->GetUnderlyingFrame() returns non-null because it's nsDisplaySVGEffects
  if (aItem->Frame()->GetContent() != mFrame->GetContent())
    return false;
  if (aItem->GetClip() != GetClip())
    return false;
  nsDisplaySVGEffects* other = static_cast<nsDisplaySVGEffects*>(aItem);
  MergeFromTrackingMergedFrames(other);
  mEffectsBounds.UnionRect(mEffectsBounds,
    other->mEffectsBounds + other->mFrame->GetOffsetTo(mFrame));
  return true;
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
                                               nsRegion* aInvalidRegion)
{
  const nsDisplaySVGEffectsGeometry* geometry =
    static_cast<const nsDisplaySVGEffectsGeometry*>(aGeometry);
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

#ifdef MOZ_DUMP_PAINTING
void
nsDisplaySVGEffects::PrintEffects(nsACString& aTo)
{
  nsIFrame* firstFrame =
    nsLayoutUtils::FirstContinuationOrIBSplitSibling(mFrame);
  nsSVGEffects::EffectProperties effectProperties =
    nsSVGEffects::GetEffectProperties(firstFrame);
  bool isOK = true;
  nsSVGClipPathFrame *clipPathFrame = effectProperties.GetClipPathFrame(&isOK);
  bool first = true;
  aTo += " effects=(";
  if (mFrame->StyleDisplay()->mOpacity != 1.0f) {
    first = false;
    aTo += nsPrintfCString("opacity(%f)", mFrame->StyleDisplay()->mOpacity);
  }
  if (clipPathFrame) {
    if (!first) {
      aTo += ", ";
    }
    aTo += nsPrintfCString("clip(%s)", clipPathFrame->IsTrivial() ? "trivial" : "non-trivial");
    first = false;
  }
  if (effectProperties.HasValidFilter()) {
    if (!first) {
      aTo += ", ";
    }
    aTo += "filter";
    first = false;
  }
  if (effectProperties.GetMaskFrame(&isOK)) {
    if (!first) {
      aTo += ", ";
    }
    aTo += "mask";
  }
  aTo += ")";
}
#endif

