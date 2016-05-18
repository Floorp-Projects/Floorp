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
#include "nsViewManager.h"
#include "ImageLayers.h"
#include "ImageContainer.h"
#include "nsCanvasFrame.h"
#include "StickyScrollContainer.h"
#include "mozilla/AnimationPerformanceWarning.h"
#include "mozilla/AnimationUtils.h"
#include "mozilla/EffectCompositor.h"
#include "mozilla/EventStates.h"
#include "mozilla/LookAndFeel.h"
#include "mozilla/PendingAnimationTracker.h"
#include "mozilla/Poison.h"
#include "mozilla/Preferences.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/unused.h"
#include "ActiveLayerTracker.h"
#include "nsContentUtils.h"
#include "nsPrintfCString.h"
#include "UnitTransforms.h"
#include "LayersLogging.h"
#include "FrameLayerBuilder.h"
#include "mozilla/EventStateManager.h"
#include "mozilla/RestyleManager.h"
#include "nsCaret.h"
#include "nsISelection.h"
#include "nsDOMTokenList.h"
#include "mozilla/RuleNodeCacheConditions.h"
#include "nsCSSProps.h"
#include "nsPluginFrame.h"
#include "DisplayItemScrollClip.h"

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

void*
AnimatedGeometryRoot::operator new(size_t aSize, nsDisplayListBuilder* aBuilder)
{
  return aBuilder->Allocate(aSize);
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
        bool dummy;
        Matrix4x4 matrix;
        nsStyleTransformMatrix::ProcessInterpolateMatrix(matrix, array,
                                                         aContext,
                                                         aPresContext,
                                                         conditions,
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

  uint32_t type = aCTF->GetType() == nsTimingFunction::Type::StepStart ? 1 : 2;
  return TimingFunction(StepFunction(aCTF->GetSteps(), type));
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

  const TimingParams& timing = aAnimation->GetEffect()->SpecifiedTiming();
  const ComputedTiming computedTiming =
    aAnimation->GetEffect()->GetComputedTiming();
  Nullable<TimeDuration> startTime = aAnimation->GetCurrentOrPendingStartTime();
  animation->startTime() = startTime.IsNull()
                           ? TimeStamp()
                           : aAnimation->AnimationTimeToTimeStamp(
                              StickyTimeDuration(timing.mDelay));
  animation->initialCurrentTime() = aAnimation->GetCurrentTime().Value()
                                    - timing.mDelay;
  animation->duration() = computedTiming.mDuration;
  animation->iterations() = computedTiming.mIterations;
  animation->iterationStart() = computedTiming.mIterationStart;
  animation->direction() = static_cast<uint32_t>(timing.mDirection);
  animation->property() = aProperty.mProperty;
  animation->playbackRate() = aAnimation->PlaybackRate();
  animation->data() = aData;
  animation->easingFunction() = ToTimingFunction(timing.mFunction);

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
                         nsTArray<RefPtr<dom::Animation>>& aAnimations,
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

static void
GenerateAndPushTextMask(nsIFrame* aFrame, nsRenderingContext* aContext,
                        const nsRect& aFillRect)
{
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

  gfxContext* sourceCtx = aContext->ThebesContext();
  gfxRect bounds =
    nsLayoutUtils::RectToGfxRect(aFillRect,
                                 aFrame->PresContext()->AppUnitsPerDevPixel());

  {
    // Paint text selection background into sourceCtx.
    gfxContextMatrixAutoSaveRestore save(sourceCtx);
    sourceCtx->SetMatrix(sourceCtx->CurrentMatrix().Translate(bounds.TopLeft()));

    nsLayoutUtils::PaintFrame(aContext, aFrame,
                              nsRect(nsPoint(0, 0), aFrame->GetSize()),
                              NS_RGB(255, 255, 255),
                              nsDisplayListBuilderMode::PAINTING_SELECTION_BACKGROUND);
  }

  // Evaluate required surface size.
  IntRect drawRect;
  {
    gfxContextMatrixAutoSaveRestore matRestore(sourceCtx);

    sourceCtx->SetMatrix(gfxMatrix());
    gfxRect clipRect = sourceCtx->GetClipExtents();
    drawRect = RoundedOut(ToRect(clipRect));
  }

  // Create a mask surface.
  RefPtr<DrawTarget> sourceTarget = sourceCtx->GetDrawTarget();
  RefPtr<DrawTarget> maskDT =
    sourceTarget->CreateSimilarDrawTarget(drawRect.Size(),
                                          SurfaceFormat::A8);
  if (!maskDT) {
    NS_ABORT_OOM(drawRect.width * drawRect.height);
  }
  RefPtr<gfxContext> maskCtx = gfxContext::ForDrawTargetWithTransform(maskDT);
  gfxMatrix currentMatrix = sourceCtx->CurrentMatrix();
  maskCtx->SetMatrix(gfxMatrix::Translation(bounds.TopLeft()) *
                     currentMatrix *
                     gfxMatrix::Translation(-drawRect.TopLeft()));

  // Shade text shape into mask A8 surface.
  nsRenderingContext rc(maskCtx);
  nsLayoutUtils::PaintFrame(&rc, aFrame,
                            nsRect(nsPoint(0, 0), aFrame->GetSize()),
                            NS_RGB(255, 255, 255),
                            nsDisplayListBuilderMode::GENERATE_GLYPH);

  // Push the generated mask into aContext, so that the caller can pop and
  // blend with it.
  Matrix maskTransform = ToMatrix(currentMatrix) *
                         Matrix::Translation(-drawRect.x, -drawRect.y);
  maskTransform.Invert();

  RefPtr<SourceSurface> maskSurface = maskDT->Snapshot();
  sourceCtx->PushGroupForBlendBack(gfxContentType::COLOR_ALPHA, 1.0, maskSurface, maskTransform);
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
    RestyleManager::GetAnimationGenerationForFrame(aFrame);
  aLayer->SetAnimationGeneration(animationGeneration);

  EffectCompositor::ClearIsRunningOnCompositor(aFrame, aProperty);
  nsTArray<RefPtr<dom::Animation>> compositorAnimations =
    EffectCompositor::GetAnimationsForCompositor(aFrame, aProperty);
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
    aFrame->Properties().Set(nsIFrame::RefusedAsyncAnimationProperty(), true);

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
    if (aItem) {
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
                         bounds, devPixelsToAppUnits);
  } else if (aProperty == eCSSProperty_opacity) {
    data = null_t();
  }

  AddAnimationsForProperty(aFrame, aProperty, compositorAnimations,
                           aLayer, data, pending);
}

nsDisplayListBuilder::nsDisplayListBuilder(nsIFrame* aReferenceFrame,
    nsDisplayListBuilderMode aMode, bool aBuildCaret)
    : mReferenceFrame(aReferenceFrame),
      mIgnoreScrollFrame(nullptr),
      mLayerEventRegions(nullptr),
      mCurrentTableItem(nullptr),
      mCurrentFrame(aReferenceFrame),
      mCurrentReferenceFrame(aReferenceFrame),
      mCurrentAGR(&mRootAGR),
      mRootAGR(aReferenceFrame, nullptr),
      mUsedAGRBudget(0),
      mDirtyRect(-1,-1,-1,-1),
      mGlassDisplayItem(nullptr),
      mScrollInfoItemsForHoisting(nullptr),
      mMode(aMode),
      mCurrentScrollParentId(FrameMetrics::NULL_SCROLL_ID),
      mCurrentScrollbarTarget(FrameMetrics::NULL_SCROLL_ID),
      mCurrentScrollbarFlags(0),
      mPerspectiveItemIndex(0),
      mSVGEffectsBuildingDepth(0),
      mContainsBlendMode(false),
      mIsBuildingScrollbar(false),
      mCurrentScrollbarWillHaveLayer(false),
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
  PL_InitArenaPool(&mPool, "displayListArena", 4096,
                   std::max(NS_ALIGNMENT_OF(void*),NS_ALIGNMENT_OF(double))-1);

  nsPresContext* pc = aReferenceFrame->PresContext();
  nsIPresShell *shell = pc->PresShell();
  if (pc->IsRenderingOnlySelection()) {
    nsCOMPtr<nsISelectionController> selcon(do_QueryInterface(shell));
    if (selcon) {
      selcon->GetSelection(nsISelectionController::SELECTION_NORMAL,
                           getter_AddRefs(mBoundingSelection));
    }
  }

  mFrameToAnimatedGeometryRootMap.Put(aReferenceFrame, &mRootAGR);

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

bool nsDisplayListBuilder::NeedToForceTransparentSurfaceForItem(nsDisplayItem* aItem)
{
  return aItem == mGlassDisplayItem || aItem->ClearsBackground();
}

AnimatedGeometryRoot*
nsDisplayListBuilder::WrapAGRForFrame(nsIFrame* aAnimatedGeometryRoot,
                                      AnimatedGeometryRoot* aParent /* = nullptr */)
{
  MOZ_ASSERT(IsAnimatedGeometryRoot(aAnimatedGeometryRoot));

  AnimatedGeometryRoot* result = nullptr;
  if (!mFrameToAnimatedGeometryRootMap.Get(aAnimatedGeometryRoot, &result)) {
    MOZ_ASSERT(nsLayoutUtils::IsAncestorFrameCrossDoc(RootReferenceFrame(), aAnimatedGeometryRoot));
    AnimatedGeometryRoot* parent = aParent;
    if (!parent) {
      nsIFrame* parentFrame = nsLayoutUtils::GetCrossDocParentFrame(aAnimatedGeometryRoot);
      if (parentFrame) {
        nsIFrame* parentAGRFrame = FindAnimatedGeometryRootFrameFor(parentFrame);
        parent = WrapAGRForFrame(parentAGRFrame);
      }
    }
    result = new (this) AnimatedGeometryRoot(aAnimatedGeometryRoot, parent);
    mFrameToAnimatedGeometryRootMap.Put(aAnimatedGeometryRoot, result);
  }
  MOZ_ASSERT(!aParent || result->mParentAGR == aParent);
  return result;
}

AnimatedGeometryRoot*
nsDisplayListBuilder::FindAnimatedGeometryRootFor(nsIFrame* aFrame)
{
  if (!IsPaintingToWindow()) {
    return &mRootAGR;
  }
  if (aFrame == mCurrentFrame) {
    return mCurrentAGR;
  }
  AnimatedGeometryRoot* result = nullptr;
  if (mFrameToAnimatedGeometryRootMap.Get(aFrame, &result)) {
    return result;
  }

  nsIFrame* agrFrame = FindAnimatedGeometryRootFrameFor(aFrame);
  result = WrapAGRForFrame(agrFrame);
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
    nsIFrame* viewportFrame =
      nsLayoutUtils::GetClosestFrameOfType(aItem->Frame(), nsGkAtoms::viewportFrame, RootReferenceFrame());
    if (viewportFrame) {
      return FindAnimatedGeometryRootFor(viewportFrame);
    }
  }
  return FindAnimatedGeometryRootFor(aItem->Frame());
}


void nsDisplayListBuilder::MarkOutOfFlowFrameForDisplay(nsIFrame* aDirtyFrame,
                                                        nsIFrame* aFrame,
                                                        const nsRect& aDirtyRect)
{
  nsRect dirtyRectRelativeToDirtyFrame = aDirtyRect;
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
  }
  nsRect dirty = dirtyRectRelativeToDirtyFrame - aFrame->GetOffsetTo(aDirtyFrame);
  nsRect overflowRect = aFrame->GetVisualOverflowRect();

  if (aFrame->IsTransformed() &&
      EffectCompositor::HasAnimationsForCompositor(aFrame,
                                                   eCSSProperty_transform)) {
   /**
    * Add a fuzz factor to the overflow rectangle so that elements only just
    * out of view are pulled into the display list, so they can be
    * prerendered if necessary.
    */
    overflowRect.Inflate(nsPresContext::CSSPixelsToAppUnits(32));
  }

  if (!dirty.IntersectRect(dirty, overflowRect))
    return;

  const DisplayItemClip* oldClip = mClipState.GetClipForContainingBlockDescendants();
  const DisplayItemScrollClip* sc = mClipState.GetCurrentInnermostScrollClip();
  OutOfFlowDisplayData* data = new OutOfFlowDisplayData(oldClip, sc, dirty);
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

  for (DisplayItemClip* c : mDisplayItemClipsToDestroy) {
    c->DisplayItemClip::~DisplayItemClip();
  }
  for (DisplayItemScrollClip* c : mScrollClipsToDestroy) {
    c->DisplayItemScrollClip::~DisplayItemScrollClip();
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
  RefPtr<nsCaret> caret = CurrentPresShellState()->mPresShell->GetCaret();
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

  RefPtr<nsCaret> caret = state->mPresShell->GetCaret();
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
        auto classList = content->AsElement()->ClassList();
        if (classList->Contains(NS_LITERAL_STRING("moz-accessiblecaret"))) {
          continue;
        }
      }
    }
    mFramesMarkedForDisplay.AppendElement(e);
    MarkOutOfFlowFrameForDisplay(aDirtyFrame, e, aDirtyRect);
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
        mFramesMarkedForDisplay.AppendElement(child);
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

DisplayItemScrollClip*
nsDisplayListBuilder::AllocateDisplayItemScrollClip(const DisplayItemScrollClip* aParent,
                                                    nsIScrollableFrame* aScrollableFrame,
                                                    const DisplayItemClip* aClip,
                                                    bool aIsAsyncScrollable)
{
  void* p = Allocate(sizeof(DisplayItemScrollClip));
  DisplayItemScrollClip* c =
    new (p) DisplayItemScrollClip(aParent, aScrollableFrame, aClip, aIsAsyncScrollable);
  mScrollClipsToDestroy.AppendElement(c);
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
  if (!parent) {
    parent = nsLayoutUtils::GetCrossDocParentFrame(aFrame);
  }
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
  if (aFrame == mReferenceFrame) {
    return true;
  }
  if (!IsPaintingToWindow()) {
    if (aParent) {
      *aParent = nsLayoutUtils::GetCrossDocParentFrame(aFrame);
    }
    return false;
  }

  if (nsLayoutUtils::IsPopup(aFrame))
    return true;
  if (ActiveLayerTracker::IsOffsetOrMarginStyleAnimated(aFrame)) {
    const bool inBudget = AddToAGRBudget(aFrame);
    if (inBudget) {
      return true;
    }
  }
  if (!aFrame->GetParent() &&
      nsLayoutUtils::ViewportHasDisplayPort(aFrame->PresContext())) {
    // Viewport frames in a display port need to be animated geometry roots
    // for background-attachment:fixed elements.
    return true;
  }
  if (aFrame->IsTransformed()) {
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

nsIFrame*
nsDisplayListBuilder::FindAnimatedGeometryRootFrameFor(nsIFrame* aFrame)
{
  MOZ_ASSERT(nsLayoutUtils::IsAncestorFrameCrossDoc(RootReferenceFrame(), aFrame));
  nsIFrame* cursor = aFrame;
  while (cursor != RootReferenceFrame()) {
    nsIFrame* next;
    if (IsAnimatedGeometryRoot(cursor, &next))
      return cursor;
    cursor = next;
  }
  return cursor;
}

void
nsDisplayListBuilder::RecomputeCurrentAnimatedGeometryRoot()
{
  if (*mCurrentAGR != mCurrentFrame &&
      IsAnimatedGeometryRoot(const_cast<nsIFrame*>(mCurrentFrame))) {
    AnimatedGeometryRoot* oldAGR = mCurrentAGR;
    mCurrentAGR = WrapAGRForFrame(const_cast<nsIFrame*>(mCurrentFrame), mCurrentAGR);

    // Iterate the AGR cache and look for any objects that reference the old AGR and check
    // to see if they need to be updated. AGRs can be in the cache multiple times, so we may
    // end up doing the work multiple times for AGRs that don't change.
    for (auto iter = mFrameToAnimatedGeometryRootMap.Iter(); !iter.Done(); iter.Next()) {
      AnimatedGeometryRoot* cached = iter.UserData();
      if (cached->mParentAGR == oldAGR && cached != mCurrentAGR) {
        // It's possible that this cached AGR struct that has the old AGR as a parent
        // should instead have mCurrentFrame has a parent.
        nsIFrame* parent = FindAnimatedGeometryRootFrameFor(*cached);
        MOZ_ASSERT(parent == mCurrentFrame || parent == *oldAGR);
        if (parent == mCurrentFrame) {
          cached->mParentAGR = mCurrentAGR;
        }
      }
    }
  }
}

void
nsDisplayListBuilder::AdjustWindowDraggingRegion(nsIFrame* aFrame)
{
  if (!mWindowDraggingAllowed || !IsForPainting()) {
    return;
  }

  const nsStyleUIReset* styleUI = aFrame->StyleUIReset();
  if (styleUI->mWindowDragging == NS_STYLE_WINDOW_DRAGGING_DEFAULT) {
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
          nsLayoutUtils::GetTransformToAncestor(referenceFrame, mReferenceFrame));
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
      TransformBy(referenceFrameToRootReferenceFrame, devPixelBorderBox);
    transformedDevPixelBorderBox.Round();
    LayoutDeviceIntRect transformedDevPixelBorderBoxInt;
    if (transformedDevPixelBorderBox.ToIntRect(&transformedDevPixelBorderBoxInt)) {
      if (styleUI->mWindowDragging == NS_STYLE_WINDOW_DRAGGING_DRAG) {
        mWindowDraggingRegion.OrWith(transformedDevPixelBorderBoxInt);
      } else {
        mWindowNoDraggingRegion.OrWith(transformedDevPixelBorderBoxInt);
      }
    }
  }
}

LayoutDeviceIntRegion
nsDisplayListBuilder::GetWindowDraggingRegion() const
{
  LayoutDeviceIntRegion result;
  result.Sub(mWindowDraggingRegion, mWindowNoDraggingRegion);;
  return result;
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
  if (mWillChangeBudgetSet.Contains(aFrame)) {
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

  uint32_t cost = GetLayerizationCost(aSize);
  bool onBudget = (budget.mBudget + cost) /
                    gWillChangeAreaMultiplier < budgetLimit;

  if (onBudget) {
    budget.mBudget += cost;
    mWillChangeBudget.Put(key, budget);
    mWillChangeBudgetSet.PutEntry(aFrame);
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
  mScrollInfoItemsForHoisting->AppendNewToTop(aScrollInfoItem);
}

bool
nsDisplayListBuilder::IsBuildingLayerEventRegions()
{
  if (IsPaintingToWindow()) {
    // Note: this function and LayerEventRegionsEnabled are the only places
    // that get to query LayoutEventRegionsEnabled 'directly' - other code
    // should call this function.
    return gfxPrefs::LayoutEventRegionsEnabledDoNotUseDirectly() ||
           mAsyncPanZoomEnabled;
  }
  return false;
}

/* static */ bool
nsDisplayListBuilder::LayerEventRegionsEnabled()
{
  // Note: this function and IsBuildingLayerEventRegions are the only places
  // that get to query LayoutEventRegionsEnabled 'directly' - other code
  // should call this function.
  return gfxPrefs::LayoutEventRegionsEnabledDoNotUseDirectly() ||
         gfxPlatform::AsyncPanZoomEnabled();
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
nsDisplayList::GetScrollClippedBoundsUpTo(nsDisplayListBuilder* aBuilder,
                                          const DisplayItemScrollClip* aIncludeScrollClipsUpTo) const {
  nsRect bounds;
  for (nsDisplayItem* i = GetBottom(); i != nullptr; i = i->GetAbove()) {
    nsRect r = i->GetClippedBounds(aBuilder);
    if (r.IsEmpty()) {
      continue;
    }
    for (auto* sc = i->ScrollClip(); sc && sc != aIncludeScrollClipsUpTo; sc = sc->mParent) {
      if (sc->mClip && sc->mClip->HasClip()) {
        if (sc->mIsAsyncScrollable) {
          // Assume the item can move anywhere in the scroll clip's clip rect.
          r = sc->mClip->GetClipRect();
        } else {
          r = sc->mClip->ApplyNonRoundedIntersection(r);
        }
      }
    }
    bounds.UnionRect(bounds, r);
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
                                        nsRegion* aVisibleRegion) {
  PROFILER_LABEL("nsDisplayList", "ComputeVisibilityForRoot",
    js::ProfileEntry::Category::GRAPHICS);

  nsRegion r;
  r.And(*aVisibleRegion, GetBounds(aBuilder));
  return ComputeVisibilityForSublist(aBuilder, aVisibleRegion, r.GetBounds());
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
  NS_ASSERTION(r.GetBounds().IsEqualInterior(aListVisibleBounds),
               "bad aListVisibleBounds");
#endif

  bool anyVisible = false;

  AutoTArray<nsDisplayItem*, 512> elements;
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

LayerManager*
nsDisplayListBuilder::GetWidgetLayerManager(nsView** aView, bool* aAllowRetaining)
{
  nsView* view = RootReferenceFrame()->GetView();
  if (aView) {
    *aView = view;
  }
  if (RootReferenceFrame() != nsLayoutUtils::GetDisplayRootFrame(RootReferenceFrame())) {
    return nullptr;
  }
  nsIWidget* window = RootReferenceFrame()->GetNearestWidget();
  if (window) {
    return window->GetLayerManager(aAllowRetaining);
  }
  return nullptr;
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

  RefPtr<LayerManager> layerManager;
  bool widgetTransaction = false;
  bool allowRetaining = false;
  bool doBeginTransaction = true;
  nsView *view = nullptr;
  if (aFlags & PAINT_USE_WIDGET_LAYERS) {
    layerManager = aBuilder->GetWidgetLayerManager(&view, &allowRetaining);
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

  // Store the existing layer builder to reinstate it on return.
  FrameLayerBuilder *oldBuilder = layerManager->GetLayerBuilder();

  FrameLayerBuilder *layerBuilder = new FrameLayerBuilder();
  layerBuilder->Init(aBuilder, layerManager);

  if (aFlags & PAINT_COMPRESSED) {
    layerBuilder->SetLayerTreeCompressionMode();
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

  // Clear any ScrollMetadata that may have been set on the root layer on a
  // previous paint. This paint will set new metrics if necessary, and if we
  // don't clear the old one here, we may be left with extra metrics.
  if (Layer* root = layerManager->GetRoot()) {
      root->SetScrollMetadata(nsTArray<ScrollMetadata>());
  }

  ContainerLayerParameters containerParameters
    (presShell->GetResolution(), presShell->GetResolution());
  RefPtr<ContainerLayer> root = layerBuilder->
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

    root->SetScrollMetadata(
      nsLayoutUtils::ComputeScrollMetadata(frame,
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
  if (rootPresContext && XRE_IsContentProcess()) {
    if (aBuilder->WillComputePluginGeometry()) {
      rootPresContext->ComputePluginGeometryUpdates(aBuilder->RootReferenceFrame(), aBuilder, this);
    }
    // The layer system caches plugin configuration information for forwarding
    // with layer updates which needs to get set during reflow. This must be
    // called even if there are no windowed plugins in the page.
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
  nsSubDocumentFrame* frame = do_QueryFrame(aFrame);
  if (frame && frame->PassPointerEventsToChildren()) {
    return true;
  }
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
  nsDisplayItem* item;

  if (aState->mInPreserves3D) {
    // Collect leaves of the current 3D rendering context.
    for (item = GetBottom(); item; item = item->GetAbove()) {
      auto itemType = item->GetType();
      if (itemType != nsDisplayItem::TYPE_TRANSFORM ||
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
      (itemType == nsDisplayItem::TYPE_TRANSFORM &&
       static_cast<nsDisplayTransform*>(item)->IsParticipating3DContext()) ||
      ((itemType == nsDisplayItem::TYPE_PERSPECTIVE ||
        itemType == nsDisplayItem::TYPE_OPACITY) &&
       static_cast<nsDisplayPerspective*>(item)->Frame()->Extend3DContext());
    if (same3DContext &&
        !static_cast<nsDisplayTransform*>(item)->IsLeafOf3DContext()) {
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
      if (item->GetType() == nsDisplayItem::TYPE_TRANSFORM &&
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

static bool IsZOrderLEQ(nsDisplayItem* aItem1, nsDisplayItem* aItem2,
                        void* aClosure) {
  {
    // TEMPORARY debugging code for bug 1265280
    nsIFrame* f2 = aItem2->Frame();
    if (uintptr_t(f2->StyleContext()) == mozPoisonValue()) {
      NS_RUNTIMEABORT(nsPrintfCString("bad display item %p type %s frame %p",
                                      aItem2, aItem2->Name(), f2).get());
    }
  }
  // Note that we can't just take the difference of the two
  // z-indices here, because that might overflow a 32-bit int.
  return aItem1->ZIndex() <= aItem2->ZIndex();
}

void nsDisplayList::SortByZOrder() {
  Sort(IsZOrderLEQ, nullptr);
}

void nsDisplayList::SortByContentOrder(nsIContent* aCommonAncestor) {
  Sort(IsContentLEQ, aCommonAncestor);
}

void nsDisplayList::Sort(SortLEQ aCmp, void* aClosure) {
  ::Sort(this, Count(), aCmp, aClosure);
}

nsDisplayItem::nsDisplayItem(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame)
 : nsDisplayItem(aBuilder, aFrame, aBuilder->ClipState().GetCurrentInnermostScrollClip())
{}

nsDisplayItem::nsDisplayItem(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                             const DisplayItemScrollClip* aScrollClip)
  : mFrame(aFrame)
  , mClip(aBuilder->ClipState().GetCurrentCombinedClip(aBuilder))
  , mScrollClip(aScrollClip)
  , mAnimatedGeometryRoot(nullptr)
#ifdef MOZ_DUMP_PAINTING
  , mPainted(false)
#endif
{
  mReferenceFrame = aBuilder->FindReferenceFrameFor(aFrame, &mToReferenceFrame);
  // This can return the wrong result if the item override ShouldFixToViewport(),
  // the item needs to set it again in its constructor.
  mAnimatedGeometryRoot = aBuilder->FindAnimatedGeometryRootFor(aFrame);
  MOZ_ASSERT(nsLayoutUtils::IsAncestorFrameCrossDoc(aBuilder->RootReferenceFrame(),
                                                    *mAnimatedGeometryRoot), "Bad");
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

static int32_t ZIndexForFrame(nsIFrame* aFrame)
{
  if (!aFrame->IsAbsPosContaininingBlock() && !aFrame->IsFlexOrGridItem())
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
      LayoutDeviceIntRect::FromUnknownRect(
        borderBox.ToNearestPixels(
          aFrame->PresContext()->AppUnitsPerDevPixel())));
  }
}

nsDisplayBackgroundImage::nsDisplayBackgroundImage(nsDisplayListBuilder* aBuilder,
                                                   nsIFrame* aFrame,
                                                   uint32_t aLayer,
                                                   const nsRect& aBackgroundRect,
                                                   const nsStyleBackground* aBackgroundStyle)
  : nsDisplayImageContainer(aBuilder, aFrame)
  , mBackgroundStyle(aBackgroundStyle)
  , mBackgroundRect(aBackgroundRect)
  , mLayer(aLayer)
  , mIsRasterImage(false)
{
  MOZ_COUNT_CTOR(nsDisplayBackgroundImage);

  nsPresContext* presContext = mFrame->PresContext();
  uint32_t flags = aBuilder->GetBackgroundPaintFlags();
  const nsStyleImageLayers::Layer &layer = mBackgroundStyle->mImage.mLayers[mLayer];

  bool isTransformedFixed;
  nsBackgroundLayerState state =
    nsCSSRendering::PrepareImageLayer(presContext, mFrame, flags,
                                      mBackgroundRect, mBackgroundRect, layer,
                                      &isTransformedFixed);
  mShouldTreatAsFixed = ComputeShouldTreatAsFixed(isTransformedFixed);

  mBounds = GetBoundsInternal(aBuilder);
  if (ShouldFixToViewport(aBuilder)) {
    mAnimatedGeometryRoot = aBuilder->FindAnimatedGeometryRootFor(this);
  }

  mFillRect = state.mFillArea;
  mDestRect = state.mDestArea;

  nsImageRenderer* imageRenderer = &state.mImageRenderer;
  // We only care about images here, not gradients.
  if (imageRenderer->IsRasterImage()) {
    mIsRasterImage = true;
    mImage = imageRenderer->GetImage();
  }
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


/*static*/ bool
nsDisplayBackgroundImage::AppendBackgroundItemsToTop(nsDisplayListBuilder* aBuilder,
                                                     nsIFrame* aFrame,
                                                     const nsRect& aBackgroundRect,
                                                     nsDisplayList* aList,
                                                     bool aAllowWillPaintBorderOptimization)
{
  if (aBuilder->IsForGenerateGlyphMask() ||
      aBuilder->IsForPaintingSelectionBG()) {
    return true;
  }

  nsStyleContext* bgSC = nullptr;
  const nsStyleBackground* bg = nullptr;
  nsRect bgRect = aBackgroundRect + aBuilder->ToReferenceFrame(aFrame);
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
                              bg->BottomLayer(), bgRect,
                              useWillPaintBorderOptimization);
    }
    bgItemList.AppendNewToTop(
        new (aBuilder) nsDisplayBackgroundColor(aBuilder, aFrame, bgRect, bg,
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
      new (aBuilder) nsDisplayThemedBackground(aBuilder, aFrame, bgRect);
    bgItemList.AppendNewToTop(bgItem);
    aList->AppendToTop(&bgItemList);
    return true;
  }

  if (!bg) {
    aList->AppendToTop(&bgItemList);
    return false;
  }

  const DisplayItemScrollClip* scrollClip =
    aBuilder->ClipState().GetCurrentInnermostScrollClip();

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
    nsDisplayBackgroundImage* bgItem =
      new (aBuilder) nsDisplayBackgroundImage(aBuilder, aFrame, i, bgRect, bg);

    if (bgItem->ShouldFixToViewport(aBuilder)) {
      thisItemList.AppendNewToTop(
        nsDisplayFixedPosition::CreateForFixedBackground(aBuilder, aFrame, bgItem, i));
    } else {
      thisItemList.AppendNewToTop(bgItem);
    }

    if (bg->mImage.mLayers[i].mBlendMode != NS_STYLE_BLEND_NORMAL) {
      thisItemList.AppendNewToTop(
        new (aBuilder) nsDisplayBlendMode(aBuilder, aFrame, &thisItemList,
                                          bg->mImage.mLayers[i].mBlendMode,
                                          scrollClip, i + 1));
    }
    bgItemList.AppendToTop(&thisItemList);
  }

  if (needBlendContainer) {
    bgItemList.AppendNewToTop(
      nsDisplayBlendContainer::CreateForBackgroundBlendMode(aBuilder, aFrame, &bgItemList, scrollClip));
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
nsDisplayBackgroundImage::ShouldTreatAsFixed() const
{
  return mShouldTreatAsFixed;
}

bool
nsDisplayBackgroundImage::ComputeShouldTreatAsFixed(bool isTransformedFixed) const
{
  if (!mBackgroundStyle)
    return false;

  const nsStyleImageLayers::Layer &layer = mBackgroundStyle->mImage.mLayers[mLayer];
  if (layer.mAttachment != NS_STYLE_IMAGELAYER_ATTACHMENT_FIXED)
    return false;

  // background-attachment:fixed is treated as background-attachment:scroll
  // if it's affected by a transform.
  // See https://www.w3.org/Bugs/Public/show_bug.cgi?id=17521.
  return !isTransformedFixed;
}

bool
nsDisplayBackgroundImage::IsNonEmptyFixedImage() const
{
  return ShouldTreatAsFixed() &&
         !mBackgroundStyle->mImage.mLayers[mLayer].mImage.IsEmpty();
}

bool
nsDisplayBackgroundImage::ShouldFixToViewport(nsDisplayListBuilder* aBuilder)
{
  // APZ needs background-attachment:fixed images layerized for correctness.
  RefPtr<LayerManager> layerManager = aBuilder->GetWidgetLayerManager();
  if (!nsLayoutUtils::UsesAsyncScrolling(mFrame) &&
      layerManager && layerManager->ShouldAvoidComponentAlphaLayers()) {
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

  // We currently can't handle tiled backgrounds.
  if (!mDestRect.Contains(mFillRect)) {
    return false;
  }

  // For 'contain' and 'cover', we allow any pixel of the image to be sampled
  // because there isn't going to be any spriting/atlasing going on.
  const nsStyleImageLayers::Layer &layer = mBackgroundStyle->mImage.mLayers[mLayer];
  bool allowPartialImages =
    (layer.mSize.mWidthType == nsStyleImageLayers::Size::eContain ||
     layer.mSize.mWidthType == nsStyleImageLayers::Size::eCover);
  if (!allowPartialImages && !mFillRect.Contains(mDestRect)) {
    return false;
  }

  return nsDisplayImageContainer::CanOptimizeToImageLayer(aManager, aBuilder);
}

nsRect
nsDisplayBackgroundImage::GetDestRect()
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
  nsIFrame* backgroundStyleFrame = nsCSSRendering::FindBackgroundStyleFrame(mFrame);
  if (ActiveLayerTracker::IsBackgroundPositionAnimated(aBuilder,
                                                       backgroundStyleFrame)) {
    return WHENEVER_POSSIBLE;
  }

  if (nsLayoutUtils::AnimatedImageLayersEnabled() && mBackgroundStyle) {
    const nsStyleImageLayers::Layer &layer = mBackgroundStyle->mImage.mLayers[mLayer];
    const nsStyleImage* image = &layer.mImage;
    if (image->GetType() == eStyleImageType_Image) {
      imgIRequest* imgreq = image->GetImageData();
      nsCOMPtr<imgIContainer> image;
      if (NS_SUCCEEDED(imgreq->GetImage(getter_AddRefs(image))) && image) {
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

LayerState
nsDisplayBackgroundImage::GetLayerState(nsDisplayListBuilder* aBuilder,
                                        LayerManager* aManager,
                                        const ContainerLayerParameters& aParameters)
{
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
                                              uint8_t aClip,
                                              const nsRect& aRect,
                                              const nsRect& aBackgroundRect)
{
  nsRegion result;
  if (aRect.IsEmpty())
    return result;

  nsIFrame *frame = aItem->Frame();

  nsRect clipRect = aBackgroundRect;
  if (frame->GetType() == nsGkAtoms::canvasFrame) {
    nsCanvasFrame* canvasFrame = static_cast<nsCanvasFrame*>(frame);
    clipRect = canvasFrame->CanvasArea() + aItem->ToReferenceFrame();
  } else if (aClip == NS_STYLE_IMAGELAYER_CLIP_PADDING ||
             aClip == NS_STYLE_IMAGELAYER_CLIP_CONTENT) {
    nsMargin border = frame->GetUsedBorder();
    if (aClip == NS_STYLE_IMAGELAYER_CLIP_CONTENT) {
      border += frame->GetUsedPadding();
    }
    border.ApplySkipSides(frame->GetSkipSides());
    clipRect.Deflate(border);
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
    const nsStyleImageLayers::Layer& layer = mBackgroundStyle->mImage.mLayers[mLayer];
    if (layer.mImage.IsOpaque() && layer.mBlendMode == NS_STYLE_BLEND_NORMAL &&
        layer.mClip != NS_STYLE_IMAGELAYER_CLIP_TEXT) {
      result = GetInsideClipRegion(this, layer.mClip, mBounds, mBackgroundRect);
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
  bool transformedFixed;
  return nsCSSRendering::ComputeImageLayerPositioningArea(
      mFrame->PresContext(), mFrame,
      mBackgroundRect,
      mBackgroundStyle->mImage.mLayers[mLayer],
      &attachedToFrame,
      &transformedFixed) + ToReferenceFrame();
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

  const nsStyleImageLayers::Layer &layer = mBackgroundStyle->mImage.mLayers[mLayer];
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
  uint32_t flags = aBuilder->GetBackgroundPaintFlags();
  CheckForBorderItem(this, flags);

  gfxContext* ctx = aCtx->ThebesContext();
  uint8_t clip = mBackgroundStyle->mImage.mLayers[mLayer].mClip;

  if (clip == NS_STYLE_IMAGELAYER_CLIP_TEXT) {
    GenerateAndPushTextMask(mFrame, aCtx, mBackgroundRect);
  }

  image::DrawResult result =
    nsCSSRendering::PaintBackground(mFrame->PresContext(), *aCtx, mFrame,
                                    aBounds,
                                    mBackgroundRect,
                                    flags, aClipRect, mLayer,
                                    CompositionOp::OP_OVER);

  if (clip == NS_STYLE_IMAGELAYER_CLIP_TEXT) {
    ctx->PopGroupAndBlend();
  }

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
  if (!mDestRect.IsEqualInterior(geometry->mDestRect)) {
    // Dest area changed in a way that could cause everything to change,
    // so invalidate everything (both old and new painting areas).
    aInvalidRegion->Or(bounds, geometry->mBounds);
    NotifyRenderingChanged();
    return;
  }
  if (aBuilder->ShouldSyncDecodeImages()) {
    const nsStyleImage& image = mBackgroundStyle->mImage.mLayers[mLayer].mImage;
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

  nsRect clipRect = mBackgroundRect;
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
      clipRect = clipRect.Union(rootRect + aBuilder->ToReferenceFrame(mFrame));
    }
  }
  const nsStyleImageLayers::Layer& layer = mBackgroundStyle->mImage.mLayers[mLayer];
  return nsCSSRendering::GetBackgroundLayerRect(presContext, mFrame,
                                                mBackgroundRect, clipRect, layer,
                                                aBuilder->GetBackgroundPaintFlags());
}

uint32_t
nsDisplayBackgroundImage::GetPerFrameKey()
{
  return (mLayer << nsDisplayItem::TYPE_BITS) |
    nsDisplayItem::GetPerFrameKey();
}

nsDisplayThemedBackground::nsDisplayThemedBackground(nsDisplayListBuilder* aBuilder,
                                                     nsIFrame* aFrame,
                                                     const nsRect& aBackgroundRect)
  : nsDisplayItem(aBuilder, aFrame)
  , mBackgroundRect(aBackgroundRect)
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
  // Assume that any point in our background rect is a hit.
  if (mBackgroundRect.Intersects(aRect)) {
    aOutFrames->AppendElement(mFrame);
  }
}

nsRegion
nsDisplayThemedBackground::GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                                           bool* aSnap) {
  nsRegion result;
  *aSnap = false;

  if (mThemeTransparency == nsITheme::eOpaque) {
    result = mBackgroundRect;
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
nsDisplayThemedBackground::ProvidesFontSmoothingBackgroundColor(nscolor* aColor)
{
  nsITheme* theme = mFrame->PresContext()->GetTheme();
  return theme->WidgetProvidesFontSmoothingBackgroundColor(mFrame, mAppearance, aColor);
}

nsRect
nsDisplayThemedBackground::GetPositioningArea()
{
  return mBackgroundRect;
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
  nsRect drawing(mBackgroundRect);
  theme->GetWidgetOverflow(presContext->DeviceContext(), mFrame, mAppearance,
                           &drawing);
  drawing.IntersectRect(drawing, aBounds);
  theme->DrawWidgetBackground(aCtx, mFrame, mAppearance, mBackgroundRect, drawing);
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
  aLayer->SetFilter(nsLayoutUtils::GetGraphicsFilterForFrame(mFrame));

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
    nsDisplayBackgroundGeometry::UpdateDrawResult(this,
                                                  image::DrawResult::SUCCESS);
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
  const LayoutDeviceRect destRect =
    LayoutDeviceRect::FromAppUnits(GetDestRect(), factor);

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

  uint32_t flags = aBuilder->ShouldSyncDecodeImages()
                 ? imgIContainer::FLAG_SYNC_DECODE
                 : imgIContainer::FLAG_NONE;

  return image->GetImageContainer(aManager, flags);
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
  const LayoutDeviceRect destRect =
    LayoutDeviceRect::FromAppUnits(GetDestRect(), factor);

  // Calculate the scaling factor for the frame.
  const gfxSize scale = gfxSize(destRect.width / imageWidth,
                                destRect.height / imageHeight);

  if (scale.width < 0.34 || scale.height < 0.34) {
    // This would look awful as long as we can't use high-quality downscaling
    // for image layers (bug 803703), so don't turn this into an image layer.
    return false;
  }

  return true;
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
  gfxContext* ctx = aCtx->ThebesContext();
  gfxRect bounds =
    nsLayoutUtils::RectToGfxRect(mBackgroundRect,
                                 mFrame->PresContext()->AppUnitsPerDevPixel());

  uint8_t clip = mBackgroundStyle->mImage.mLayers[0].mClip;
  if (clip == NS_STYLE_IMAGELAYER_CLIP_TEXT) {
    GenerateAndPushTextMask(mFrame, aCtx, mBackgroundRect);
    ctx->SetColor(mColor);
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
                                          bool* aSnap)
{
  *aSnap = false;

  if (mColor.a != 1) {
    return nsRegion();
  }

  if (!mBackgroundStyle)
    return nsRegion();

  *aSnap = true;

  const nsStyleImageLayers::Layer& bottomLayer = mBackgroundStyle->BottomLayer();
  return nsDisplayBackgroundImage::GetInsideClipRegion(this, bottomLayer.mClip,
                                                       mBackgroundRect, mBackgroundRect);
}

bool
nsDisplayBackgroundColor::IsUniform(nsDisplayListBuilder* aBuilder, nscolor* aColor)
{
  *aColor = mColor.ToABGR();
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

  uint8_t pointerEvents =
    aFrame->StyleUserInterface()->GetEffectivePointerEvents(aFrame);
  if (pointerEvents == NS_STYLE_POINTER_EVENTS_NONE) {
    return;
  }
  bool simpleRegions = aFrame->HasAnyStateBits(NS_FRAME_SIMPLE_EVENT_REGIONS);
  if (!simpleRegions) {
    if (!aFrame->StyleVisibility()->IsVisible()) {
      return;
    }
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

  bool borderBoxHasRoundedCorners = false;
  if (!simpleRegions) {
    if (nsLayoutUtils::HasNonZeroCorner(aFrame->StyleBorder()->mBorderRadius)) {
      borderBoxHasRoundedCorners = true;
    } else {
      aFrame->AddStateBits(NS_FRAME_SIMPLE_EVENT_REGIONS);
    }
  }

  const DisplayItemClip* clip = aBuilder->ClipState().GetCurrentCombinedClip(aBuilder);
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

  if (aBuilder->IsBuildingNonLayerizedScrollbar() ||
      aBuilder->GetAncestorHasApzAwareEventHandler())
  {
    // Scrollbars may be painted into a layer below the actual layer they will
    // scroll, and therefore wheel events may be dispatched to the outer frame
    // instead of the intended scrollframe. To address this, we force a d-t-c
    // region on scrollbar frames that won't be placed in their own layer. See
    // bug 1213324 for details.
    mDispatchToContentHitRegion.Or(mDispatchToContentHitRegion, borderBox);
  } else if (aFrame->GetType() == nsGkAtoms::objectFrame) {
    // If the frame is a plugin frame and wants to handle wheel events as
    // default action, we should add the frame to dispatch-to-content region.
    nsPluginFrame* pluginFrame = do_QueryFrame(aFrame);
    if (pluginFrame && pluginFrame->WantsToHandleWheelEventAsDefaultAction()) {
      mDispatchToContentHitRegion.Or(mDispatchToContentHitRegion, borderBox);
    }
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
  mHitRegion.Or(mHitRegion, aRect);
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
  mCaret->PaintCaret(*aCtx->GetDrawTarget(), mFrame, ToReferenceFrame());
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

  if (aBuilder->ShouldSyncDecodeImages() &&
      geometry->ShouldInvalidateToSyncDecodeImages()) {
    aInvalidRegion->Or(*aInvalidRegion, GetBounds(aBuilder, &snap));
  }
}

void
nsDisplayBorder::Paint(nsDisplayListBuilder* aBuilder,
                       nsRenderingContext* aCtx) {
  nsPoint offset = ToReferenceFrame();

  PaintBorderFlags flags = aBuilder->ShouldSyncDecodeImages()
                         ? PaintBorderFlags::SYNC_DECODE_IMAGES
                         : PaintBorderFlags();

  image::DrawResult result =
    nsCSSRendering::PaintBorder(mFrame->PresContext(), *aCtx, mFrame,
                                mVisibleRect,
                                nsRect(offset, mFrame->GetSize()),
                                mFrame->StyleContext(),
                                flags,
                                mFrame->GetSkipSides());

  nsDisplayBorderGeometry::UpdateDrawResult(this, result);
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
                               nsRenderingContext* aCtx) {
  nsPoint offset = ToReferenceFrame();
  nsRect borderRect = mFrame->VisualBorderRectRelativeToSelf() + offset;
  nsPresContext* presContext = mFrame->PresContext();
  AutoTArray<nsRect,10> rects;
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
  AutoTArray<nsRect,10> rects;
  ComputeDisjointRectangles(mVisibleRegion, &rects);

  PROFILER_LABEL("nsDisplayBoxShadowInner", "Paint",
    js::ProfileEntry::Category::GRAPHICS);

  DrawTarget* drawTarget = aCtx->GetDrawTarget();
  gfxContext* gfx = aCtx->ThebesContext();
  int32_t appUnitsPerDevPixel = mFrame->PresContext()->AppUnitsPerDevPixel();

  for (uint32_t i = 0; i < rects.Length(); ++i) {
    gfx->Save();
    gfx->Clip(NSRectToSnappedRect(rects[i], appUnitsPerDevPixel, *drawTarget));
    nsCSSRendering::PaintBoxShadowInner(presContext, *aCtx, mFrame, borderRect);
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
  : nsDisplayWrapList(aBuilder, aFrame, aList,
                      aBuilder->ClipState().GetCurrentInnermostScrollClip())
{}

nsDisplayWrapList::nsDisplayWrapList(nsDisplayListBuilder* aBuilder,
                                     nsIFrame* aFrame, nsDisplayList* aList,
                                     const DisplayItemScrollClip* aScrollClip)
  : nsDisplayItem(aBuilder, aFrame, aScrollClip)
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
  nsDisplayItem *i = mList.GetBottom();
  if (i && (!i->GetAbove() || i->GetType() == TYPE_TRANSFORM) &&
      i->Frame() == mFrame) {
    mReferenceFrame = i->ReferenceFrame();
    mToReferenceFrame = i->ToReferenceFrame();
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

  // See the previous nsDisplayWrapList constructor
  if (aItem->Frame() == aFrame) {
    mReferenceFrame = aItem->ReferenceFrame();
    mToReferenceFrame = aItem->ToReferenceFrame();
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
    mList.ComputeVisibilityForSublist(aBuilder, &visibleRegion, mVisibleRect);

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
                              AnimatedGeometryRoot* aExpectedAnimatedGeometryRootForChildren)
{
  LayerState result = LAYER_INACTIVE;
  for (nsDisplayItem* i = aList.GetBottom(); i; i = i->GetAbove()) {
    if (result == LAYER_INACTIVE &&
        i->GetAnimatedGeometryRoot() != aExpectedAnimatedGeometryRootForChildren) {
      result = LAYER_ACTIVE;
    }

    LayerState state = i->GetLayerState(aBuilder, aManager, aParameters);
    if (state == LAYER_ACTIVE && i->GetType() == nsDisplayItem::TYPE_BLEND_MODE) {
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
                                   nsIFrame* aFrame, nsDisplayList* aList,
                                   const DisplayItemScrollClip* aScrollClip,
                                   bool aForEventsOnly)
    : nsDisplayWrapList(aBuilder, aFrame, aList, aScrollClip)
    , mOpacity(aFrame->StyleEffects()->mOpacity)
    , mForEventsOnly(aForEventsOnly)
    , mParticipatesInPreserve3D(false)
{
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
  ContainerLayerParameters params = aContainerParameters;
  params.mForEventsOnly = mForEventsOnly;
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

  if (mParticipatesInPreserve3D) {
    container->SetContentFlags(container->GetContentFlags() | Layer::CONTENT_EXTEND_3D_CONTEXT);
  } else {
    container->SetContentFlags(container->GetContentFlags() & ~Layer::CONTENT_EXTEND_3D_CONTEXT);
  }
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
  nsIntRect visibleDevPixels = aItem->Frame()->GetVisualOverflowRectRelativeToSelf().ToOutsidePixels(
          aItem->Frame()->PresContext()->AppUnitsPerDevPixel());
  static const int MIN_ACTIVE_LAYER_SIZE_DEV_PIXELS = 16;
  return visibleDevPixels.Size() <
    nsIntSize(MIN_ACTIVE_LAYER_SIZE_DEV_PIXELS, MIN_ACTIVE_LAYER_SIZE_DEV_PIXELS);
}

static void
SetAnimationPerformanceWarningForTooSmallItem(nsDisplayItem* aItem,
                                              nsCSSProperty aProperty)
{
  // We use ToNearestPixels() here since ToOutsidePixels causes some sort of
  // errors. See https://bugzilla.mozilla.org/show_bug.cgi?id=1258904#c19
  nsIntRect visibleDevPixels = aItem->Frame()->GetVisualOverflowRectRelativeToSelf().ToNearestPixels(
          aItem->Frame()->PresContext()->AppUnitsPerDevPixel());

  // Set performance warning only if the visible dev pixels is not empty
  // because dev pixels is empty if the frame has 'preserve-3d' style.
  if (visibleDevPixels.IsEmpty()) {
    return;
  }

  EffectCompositor::SetPerformanceWarning(aItem->Frame(), aProperty,
      AnimationPerformanceWarning(
        AnimationPerformanceWarning::Type::ContentTooSmall,
        { visibleDevPixels.Width(), visibleDevPixels.Height() }));
}

bool
nsDisplayOpacity::NeedsActiveLayer(nsDisplayListBuilder* aBuilder)
{
  if (ActiveLayerTracker::IsStyleAnimated(aBuilder, mFrame,
                                          eCSSProperty_opacity) ||
      EffectCompositor::HasAnimationsForCompositor(mFrame,
                                                   eCSSProperty_opacity)) {
    if (!IsItemTooSmallForActiveLayer(this)) {
      return true;
    }
    SetAnimationPerformanceWarningForTooSmallItem(this, eCSSProperty_opacity);
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
  if (NeedsActiveLayer(aBuilder) || mOpacity == 0.0) {
    // If our opacity is zero then we'll discard all descendant display items
    // except for layer event regions, so there's no point in doing this
    // optimization (and if we do do it, then invalidations of those descendants
    // might trigger repainting).
    return false;
  }

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
    if (child->GetType() == nsDisplayItem::TYPE_LAYER_EVENT_REGIONS) {
      numChildren--;
      continue;
    }
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
  // If we only created this item so that we'd get correct nsDisplayEventRegions for child
  // frames, then force us to inactive to avoid unnecessary layerization changes for content
  // that won't ever be painted.
  if (mForEventsOnly) {
    MOZ_ASSERT(mOpacity == 0);
    return LAYER_INACTIVE;
  }

  if (NeedsActiveLayer(aBuilder)) {
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

bool nsDisplayOpacity::TryMerge(nsDisplayItem* aItem) {
  if (aItem->GetType() != TYPE_OPACITY)
    return false;
  // items for the same content element should be merged into a single
  // compositing group
  // aItem->GetUnderlyingFrame() returns non-null because it's nsDisplayOpacity
  if (aItem->Frame()->GetContent() != mFrame->GetContent())
    return false;
  if (aItem->GetClip() != GetClip())
    return false;
  if (aItem->ScrollClip() != ScrollClip())
    return false;
  MergeFromTrackingMergedFrames(static_cast<nsDisplayOpacity*>(aItem));
  return true;
}

void
nsDisplayOpacity::WriteDebugInfo(std::stringstream& aStream)
{
  aStream << " (opacity " << mOpacity << ")";
}

nsDisplayBlendMode::nsDisplayBlendMode(nsDisplayListBuilder* aBuilder,
                                             nsIFrame* aFrame, nsDisplayList* aList,
                                             uint8_t aBlendMode,
                                             const DisplayItemScrollClip* aScrollClip,
                                             uint32_t aIndex)
  : nsDisplayWrapList(aBuilder, aFrame, aList, aScrollClip)
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
                                                bool* aSnap) {
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

bool nsDisplayBlendMode::ComputeVisibility(nsDisplayListBuilder* aBuilder,
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

bool nsDisplayBlendMode::TryMerge(nsDisplayItem* aItem) {
  if (aItem->GetType() != TYPE_BLEND_MODE)
    return false;
  nsDisplayBlendMode* item = static_cast<nsDisplayBlendMode*>(aItem);
  // items for the same content element should be merged into a single
  // compositing group
  if (item->Frame()->GetContent() != mFrame->GetContent())
    return false;
  if (item->mIndex != 0 || mIndex != 0)
    return false; // don't merge background-blend-mode items
  if (item->GetClip() != GetClip())
    return false;
  if (item->ScrollClip() != ScrollClip())
    return false;
  MergeFromTrackingMergedFrames(item);
  return true;
}

/* static */ nsDisplayBlendContainer*
nsDisplayBlendContainer::CreateForMixBlendMode(nsDisplayListBuilder* aBuilder,
                                               nsIFrame* aFrame, nsDisplayList* aList,
                                               const DisplayItemScrollClip* aScrollClip)
{
  return new (aBuilder) nsDisplayBlendContainer(aBuilder, aFrame, aList, aScrollClip, false);
}

/* static */ nsDisplayBlendContainer*
nsDisplayBlendContainer::CreateForBackgroundBlendMode(nsDisplayListBuilder* aBuilder,
                                                      nsIFrame* aFrame, nsDisplayList* aList,
                                                      const DisplayItemScrollClip* aScrollClip)
{
  return new (aBuilder) nsDisplayBlendContainer(aBuilder, aFrame, aList, aScrollClip, true);
}

nsDisplayBlendContainer::nsDisplayBlendContainer(nsDisplayListBuilder* aBuilder,
                                                 nsIFrame* aFrame, nsDisplayList* aList,
                                                 const DisplayItemScrollClip* aScrollClip,
                                                 bool aIsForBackground)
    : nsDisplayWrapList(aBuilder, aFrame, aList, aScrollClip)
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

bool nsDisplayBlendContainer::TryMerge(nsDisplayItem* aItem) {
  if (aItem->GetType() != TYPE_BLEND_CONTAINER)
    return false;
  // items for the same content element should be merged into a single
  // compositing group
  // aItem->GetUnderlyingFrame() returns non-null because it's nsDisplayOpacity
  if (aItem->Frame()->GetContent() != mFrame->GetContent())
    return false;
  if (aItem->GetClip() != GetClip())
    return false;
  if (aItem->ScrollClip() != ScrollClip())
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
  RefPtr<ContainerLayer> layer = aManager->GetLayerBuilder()->
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
      nsLayoutUtils::HasCriticalDisplayPort(rootScrollFrame->GetContent())) {
    params.mInLowPrecisionDisplayPort = true;
  }

  RefPtr<Layer> layer = nsDisplayOwnLayer::BuildLayer(aBuilder, aManager, params);
  layer->AsContainerLayer()->SetEventRegionsOverride(mForceDispatchToContentRegion
    ? EventRegionsOverride::ForceDispatchToContent
    : EventRegionsOverride::NoOverride);
  return layer.forget();
}

UniquePtr<ScrollMetadata>
nsDisplaySubDocument::ComputeScrollMetadata(Layer* aLayer,
                                            const ContainerLayerParameters& aContainerParameters)
{
  if (!(mFlags & GENERATE_SCROLLABLE_LAYER)) {
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
  if ((mFlags & GENERATE_SCROLLABLE_LAYER) &&
      rootScrollFrame->GetContent() &&
      nsLayoutUtils::HasCriticalDisplayPort(rootScrollFrame->GetContent())) {
    params.mInLowPrecisionDisplayPort = true;
  }

  nsRect viewport = mFrame->GetRect() -
                    mFrame->GetPosition() +
                    mFrame->GetOffsetToCrossDoc(ReferenceFrame());

  return MakeUnique<ScrollMetadata>(
    nsLayoutUtils::ComputeScrollMetadata(
      mFrame, rootScrollFrame, rootScrollFrame->GetContent(), ReferenceFrame(),
      aLayer, mScrollParentId, viewport, Nothing(),
      isRootContentDocument, params));
}

static bool
UseDisplayPortForViewport(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame)
{
  return aBuilder->IsPaintingToWindow() &&
      nsLayoutUtils::ViewportHasDisplayPort(aFrame->PresContext());
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
  bool usingDisplayPort = UseDisplayPortForViewport(aBuilder, mFrame);

  if (!(mFlags & GENERATE_SCROLLABLE_LAYER) || !usingDisplayPort) {
    return nsDisplayWrapList::ComputeVisibility(aBuilder, aVisibleRegion);
  }

  nsRect displayport;
  nsIFrame* rootScrollFrame = mFrame->PresContext()->PresShell()->GetRootScrollFrame();
  MOZ_ASSERT(rootScrollFrame);
  Unused << nsLayoutUtils::GetDisplayPort(rootScrollFrame->GetContent(), &displayport,
    RelativeTo::ScrollFrame);

  nsRegion childVisibleRegion;
  // The visible region for the children may be much bigger than the hole we
  // are viewing the children from, so that the compositor process has enough
  // content to asynchronously pan while content is being refreshed.
  childVisibleRegion = displayport + mFrame->GetOffsetToCrossDoc(ReferenceFrame());

  nsRect boundedRect =
    childVisibleRegion.GetBounds().Intersect(mList.GetBounds(aBuilder));
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

void
nsDisplayResolution::HitTest(nsDisplayListBuilder* aBuilder,
                             const nsRect& aRect,
                             HitTestState* aState,
                             nsTArray<nsIFrame*> *aOutFrames)
{
  nsIPresShell* presShell = mFrame->PresContext()->PresShell();
  nsRect rect = aRect.RemoveResolution(presShell->ScaleToResolution() ? presShell->GetResolution () : 1.0f);
  mList.HitTest(aBuilder, rect, aState, aOutFrames);
}

already_AddRefed<Layer>
nsDisplayResolution::BuildLayer(nsDisplayListBuilder* aBuilder,
                                LayerManager* aManager,
                                const ContainerLayerParameters& aContainerParameters) {
  nsIPresShell* presShell = mFrame->PresContext()->PresShell();
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
                                               nsDisplayList* aList)
  : nsDisplayOwnLayer(aBuilder, aFrame, aList)
  , mIndex(0)
  , mIsFixedBackground(false)
{
  MOZ_COUNT_CTOR(nsDisplayFixedPosition);
  Init(aBuilder);
}

nsDisplayFixedPosition::nsDisplayFixedPosition(nsDisplayListBuilder* aBuilder,
                                               nsIFrame* aFrame,
                                               nsDisplayList* aList,
                                               uint32_t aIndex)
  : nsDisplayOwnLayer(aBuilder, aFrame, aList)
  , mIndex(aIndex)
  , mIsFixedBackground(true)
{
  MOZ_COUNT_CTOR(nsDisplayFixedPosition);
  Init(aBuilder);
}

void
nsDisplayFixedPosition::Init(nsDisplayListBuilder* aBuilder)
{
  bool snap;
  mVisibleRect = GetBounds(aBuilder, &snap);
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
  // Clear clipping on the child item, since we will apply it to the
  // fixed position item as well.
  aImage->SetClip(aBuilder, DisplayItemClip());
  aImage->SetScrollClip(nullptr);

  nsDisplayList temp;
  temp.AppendToTop(aImage);

  return new (aBuilder) nsDisplayFixedPosition(aBuilder, aFrame, &temp, aIndex + 1);
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

  nsPresContext* presContext = Frame()->PresContext();
  nsIFrame* fixedFrame = mIsFixedBackground ? presContext->PresShell()->GetRootFrame() : Frame();

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

bool nsDisplayFixedPosition::TryMerge(nsDisplayItem* aItem) {
  if (aItem->GetType() != TYPE_FIXED_POSITION)
    return false;
  // Items with the same fixed position frame can be merged.
  nsDisplayFixedPosition* other = static_cast<nsDisplayFixedPosition*>(aItem);
  if (other->mFrame != mFrame)
    return false;
  if (aItem->GetClip() != GetClip())
    return false;
  MergeFromTrackingMergedFrames(other);
  return true;
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

bool nsDisplayStickyPosition::TryMerge(nsDisplayItem* aItem) {
  if (aItem->GetType() != TYPE_STICKY_POSITION)
    return false;
  // Items with the same fixed position frame can be merged.
  nsDisplayStickyPosition* other = static_cast<nsDisplayStickyPosition*>(aItem);
  if (other->mFrame != mFrame)
    return false;
  if (aItem->GetClip() != GetClip())
    return false;
  if (aItem->ScrollClip() != ScrollClip())
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

  ContainerLayerParameters params = aContainerParameters;
  if (mScrolledFrame->GetContent() &&
      nsLayoutUtils::HasCriticalDisplayPort(mScrolledFrame->GetContent())) {
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

UniquePtr<ScrollMetadata>
nsDisplayScrollInfoLayer::ComputeScrollMetadata(Layer* aLayer,
                                                const ContainerLayerParameters& aContainerParameters)
{
  ContainerLayerParameters params = aContainerParameters;
  if (mScrolledFrame->GetContent() &&
      nsLayoutUtils::HasCriticalDisplayPort(mScrolledFrame->GetContent())) {
    params.mInLowPrecisionDisplayPort = true;
  }

  nsRect viewport = mScrollFrame->GetRect() -
                    mScrollFrame->GetPosition() +
                    mScrollFrame->GetOffsetToCrossDoc(ReferenceFrame());

  ScrollMetadata metadata = nsLayoutUtils::ComputeScrollMetadata(
      mScrolledFrame, mScrollFrame, mScrollFrame->GetContent(),
      ReferenceFrame(), aLayer,
      mScrollParentId, viewport, Nothing(), false, params);
  metadata.GetMetrics().SetIsScrollInfoLayer(true);

  return UniquePtr<ScrollMetadata>(new ScrollMetadata(metadata));
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
  , mAnimatedGeometryRootForChildren(mAnimatedGeometryRoot)
  , mAnimatedGeometryRootForScrollMetadata(mAnimatedGeometryRoot)
  , mChildrenVisibleRect(aChildrenVisibleRect)
  , mIndex(aIndex)
  , mNoExtendContext(false)
  , mIsTransformSeparator(false)
  , mTransformPreserves3DInited(false)
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
  mVisibleRect = aBuilder->GetDirtyRect() + mToReferenceFrame;
}

void
nsDisplayTransform::Init(nsDisplayListBuilder* aBuilder)
{
  mHasBounds = false;
  mStoredList.SetClip(aBuilder, DisplayItemClip::NoClip());
  mStoredList.SetVisibleRect(mChildrenVisibleRect);
  // If this is true, then BuildDisplayListForStacking context should have already
  // set the correct visible region and made sure we built all the necessary
  // descendant display items.
  mPrerender = ShouldPrerenderTransformedContent(aBuilder, mFrame);
}

nsDisplayTransform::nsDisplayTransform(nsDisplayListBuilder* aBuilder,
                                       nsIFrame *aFrame, nsDisplayList *aList,
                                       const nsRect& aChildrenVisibleRect,
                                       uint32_t aIndex)
  : nsDisplayItem(aBuilder, aFrame)
  , mStoredList(aBuilder, aFrame, aList)
  , mTransformGetter(nullptr)
  , mAnimatedGeometryRootForChildren(mAnimatedGeometryRoot)
  , mAnimatedGeometryRootForScrollMetadata(mAnimatedGeometryRoot)
  , mChildrenVisibleRect(aChildrenVisibleRect)
  , mIndex(aIndex)
  , mNoExtendContext(false)
  , mIsTransformSeparator(false)
  , mTransformPreserves3DInited(false)
{
  MOZ_COUNT_CTOR(nsDisplayTransform);
  MOZ_ASSERT(aFrame, "Must have a frame!");
  SetReferenceFrameToAncestor(aBuilder);
  Init(aBuilder);
  UpdateBoundsFor3D(aBuilder);
}

nsDisplayTransform::nsDisplayTransform(nsDisplayListBuilder* aBuilder,
                                       nsIFrame *aFrame, nsDisplayItem *aItem,
                                       const nsRect& aChildrenVisibleRect,
                                       uint32_t aIndex)
  : nsDisplayItem(aBuilder, aFrame)
  , mStoredList(aBuilder, aFrame, aItem)
  , mTransformGetter(nullptr)
  , mAnimatedGeometryRootForChildren(mAnimatedGeometryRoot)
  , mAnimatedGeometryRootForScrollMetadata(mAnimatedGeometryRoot)
  , mChildrenVisibleRect(aChildrenVisibleRect)
  , mIndex(aIndex)
  , mNoExtendContext(false)
  , mIsTransformSeparator(false)
  , mTransformPreserves3DInited(false)
{
  MOZ_COUNT_CTOR(nsDisplayTransform);
  MOZ_ASSERT(aFrame, "Must have a frame!");
  SetReferenceFrameToAncestor(aBuilder);
  Init(aBuilder);
}

nsDisplayTransform::nsDisplayTransform(nsDisplayListBuilder* aBuilder,
                                       nsIFrame *aFrame, nsDisplayList *aList,
                                       const nsRect& aChildrenVisibleRect,
                                       const Matrix4x4& aTransform,
                                       uint32_t aIndex)
  : nsDisplayItem(aBuilder, aFrame)
  , mStoredList(aBuilder, aFrame, aList)
  , mTransform(aTransform)
  , mTransformGetter(nullptr)
  , mAnimatedGeometryRootForChildren(mAnimatedGeometryRoot)
  , mAnimatedGeometryRootForScrollMetadata(mAnimatedGeometryRoot)
  , mChildrenVisibleRect(aChildrenVisibleRect)
  , mIndex(aIndex)
  , mNoExtendContext(false)
  , mIsTransformSeparator(true)
  , mTransformPreserves3DInited(false)
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
  NS_PRECONDITION(aFrame, "Can't get delta for a null frame!");
  NS_PRECONDITION(aFrame->IsTransformed() ||
                  aFrame->StyleDisplay()->BackfaceIsHidden() ||
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
  float coords[2];
  TransformReferenceBox::DimensionGetter dimensionGetter[] =
    { &TransformReferenceBox::Width, &TransformReferenceBox::Height };
  TransformReferenceBox::DimensionGetter offsetGetter[] =
    { &TransformReferenceBox::X, &TransformReferenceBox::Y };

  for (uint8_t index = 0; index < 2; ++index) {
    /* If the transform-origin specifies a percentage, take the percentage
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

/* static */ bool
nsDisplayTransform::ComputePerspectiveMatrix(const nsIFrame* aFrame,
                                             float aAppUnitsPerPixel,
                                             Matrix4x4& aOutMatrix)
{
  NS_PRECONDITION(aFrame, "Can't get delta for a null frame!");
  NS_PRECONDITION(aFrame->IsTransformed() ||
                  aFrame->StyleDisplay()->BackfaceIsHidden() ||
                  aFrame->Combines3DTransformWithAncestors(),
                  "Shouldn't get a delta for an untransformed frame!");
  NS_PRECONDITION(aOutMatrix.IsIdentity(), "Must have a blank output matrix");

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
  if (perspective <= 0) {
    return true;
  }

  TransformReferenceBox refBox(cbFrame);

  /* Allows us to access named variables by index. */
  Point3D perspectiveOrigin;
  gfx::Float* coords[2] = {&perspectiveOrigin.x, &perspectiveOrigin.y};
  TransformReferenceBox::DimensionGetter dimensionGetter[] =
    { &TransformReferenceBox::Width, &TransformReferenceBox::Height };

  /* For both of the coordinates, if the value of perspective-origin is a
   * percentage, it's relative to the size of the frame.  Otherwise, if it's
   * a distance, it's already computed for us!
   */
  for (uint8_t index = 0; index < 2; ++index) {
    /* If the -transform-origin specifies a percentage, take the percentage
     * of the size of the box.
     */
    const nsStyleCoord &coord = cbDisplay->mPerspectiveOrigin[index];
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

  /* GetOffsetTo computes the offset required to move from 0,0 in cbFrame to 0,0
   * in aFrame. Although we actually want the inverse of this, it's faster to
   * compute this way.
   */
  nsPoint frameToCbOffset = -aFrame->GetOffsetTo(cbFrame);
  Point3D frameToCbGfxOffset(
            NSAppUnitsToFloatPixels(frameToCbOffset.x, aAppUnitsPerPixel),
            NSAppUnitsToFloatPixels(frameToCbOffset.y, aAppUnitsPerPixel),
            0.0f);

  /* Move the perspective origin to be relative to aFrame, instead of relative
   * to the containing block which is how it was specified in the style system.
   */
  perspectiveOrigin += frameToCbGfxOffset;

  aOutMatrix._34 =
    -1.0 / NSAppUnitsToFloatPixels(perspective, aAppUnitsPerPixel);
  aOutMatrix.ChangeBasis(perspectiveOrigin);
  return true;
}

nsDisplayTransform::FrameTransformProperties::FrameTransformProperties(const nsIFrame* aFrame,
                                                                       float aAppUnitsPerPixel,
                                                                       const nsRect* aBoundsOverride)
  : mFrame(aFrame)
  , mTransformList(aFrame->StyleDisplay()->mSpecifiedTransform)
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
                                                const nsRect* aBoundsOverride,
                                                nsIFrame** aOutAncestor)
{
  return GetResultingTransformMatrixInternal(aProperties, aOrigin, aAppUnitsPerPixel,
                                             aFlags, aBoundsOverride, aOutAncestor);
}

Matrix4x4
nsDisplayTransform::GetResultingTransformMatrix(const nsIFrame* aFrame,
                                                const nsPoint& aOrigin,
                                                float aAppUnitsPerPixel,
                                                uint32_t aFlags,
                                                const nsRect* aBoundsOverride,
                                                nsIFrame** aOutAncestor)
{
  FrameTransformProperties props(aFrame,
                                 aAppUnitsPerPixel,
                                 aBoundsOverride);

  return GetResultingTransformMatrixInternal(props, aOrigin, aAppUnitsPerPixel,
                                             aFlags, aBoundsOverride, aOutAncestor);
}

Matrix4x4
nsDisplayTransform::GetResultingTransformMatrixInternal(const FrameTransformProperties& aProperties,
                                                        const nsPoint& aOrigin,
                                                        float aAppUnitsPerPixel,
                                                        uint32_t aFlags,
                                                        const nsRect* aBoundsOverride,
                                                        nsIFrame** aOutAncestor)
{
  const nsIFrame *frame = aProperties.mFrame;
  NS_ASSERTION(frame || !(aFlags & INCLUDE_PERSPECTIVE), "Must have a frame to compute perspective!");
  MOZ_ASSERT((aFlags & (OFFSET_BY_ORIGIN|BASIS_AT_ORIGIN)) != (OFFSET_BY_ORIGIN|BASIS_AT_ORIGIN),
             "Can't specify offset by origin as well as basis at origin!");

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
  bool dummyBool;
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
                                                    dummy, refBox, aAppUnitsPerPixel,
                                                    &dummyBool);
  } else if (hasSVGTransforms) {
    // Correct the translation components for zoom:
    float pixelsPerCSSPx = frame->PresContext()->AppUnitsPerCSSPixel() /
                             aAppUnitsPerPixel;
    svgTransform._31 *= pixelsPerCSSPx;
    svgTransform._32 *= pixelsPerCSSPx;
    result = Matrix4x4::From2D(svgTransform);
  }

  /* Account for the transform-origin property by translating the
   * coordinate space to the new origin.
   */
  Point3D newOrigin =
    Point3D(NSAppUnitsToFloatPixels(aOrigin.x, aAppUnitsPerPixel),
            NSAppUnitsToFloatPixels(aOrigin.y, aAppUnitsPerPixel),
            0.0f);
  Point3D roundedOrigin(hasSVGTransforms ? newOrigin.x : NS_round(newOrigin.x),
                        hasSVGTransforms ? newOrigin.y : NS_round(newOrigin.y),
                        0);

  Matrix4x4 perspectiveMatrix;
  bool hasPerspective = aFlags & INCLUDE_PERSPECTIVE;
  if (hasPerspective) {
    hasPerspective = ComputePerspectiveMatrix(frame, aAppUnitsPerPixel,
                                              perspectiveMatrix);
  }

  if (!hasSVGTransforms || !hasTransformFromSVGParent) {
    // This is a simplification of the following |else| block, the
    // simplification being possible because we don't need to apply
    // mToTransformOrigin between two transforms.
    if ((aFlags & OFFSET_BY_ORIGIN) &&
        !hasPerspective) {
      // We can fold the final translation by roundedOrigin into the first matrix
      // basis change translation. This is more stable against variation due to
      // insufficient floating point precision than reversing the translation
      // afterwards.
      result.PreTranslate(-aProperties.mToTransformOrigin);
      result.PostTranslate(roundedOrigin + aProperties.mToTransformOrigin);
    } else {
      result.ChangeBasis(aProperties.mToTransformOrigin);
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
    if ((aFlags & OFFSET_BY_ORIGIN) &&
        !hasPerspective) {
      result.PreTranslate(-refBoxOffset);
      result.PostTranslate(roundedOrigin + refBoxOffset);
    } else {
      result.ChangeBasis(refBoxOffset);
    }
  }

  if (hasPerspective) {
    result = result * perspectiveMatrix;

    if (aFlags & OFFSET_BY_ORIGIN) {
      result.PostTranslate(roundedOrigin);
    }
  }

  if (aFlags & BASIS_AT_ORIGIN) {
    result.ChangeBasis(roundedOrigin);
  }

  if ((aFlags & INCLUDE_PRESERVE3D_ANCESTORS) &&
      frame && frame->Combines3DTransformWithAncestors()) {
    // Include the transform set on our parent
    NS_ASSERTION(frame->GetParent() &&
                 frame->GetParent()->IsTransformed() &&
                 frame->GetParent()->Extend3DContext(),
                 "Preserve3D mismatch!");
    FrameTransformProperties props(frame->GetParent(),
                                   aAppUnitsPerPixel,
                                   nullptr);

    // If this frame isn't transformed (but we exist for backface-visibility),
    // then we're not a reference frame so no offset to origin will be added. Our
    // parent transform however *is* the reference frame, so we pass
    // OFFSET_BY_ORIGIN to convert into the correct coordinate space.
    uint32_t flags = aFlags & (INCLUDE_PRESERVE3D_ANCESTORS|INCLUDE_PERSPECTIVE);
    if (!frame->IsTransformed()) {
      flags |= OFFSET_BY_ORIGIN;
    } else {
      flags |= BASIS_AT_ORIGIN;
    }
    Matrix4x4 parent =
      GetResultingTransformMatrixInternal(props,
                                          aOrigin - frame->GetPosition(),
                                          aAppUnitsPerPixel, flags,
                                          nullptr, aOutAncestor);
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

  EffectCompositor::SetPerformanceWarning(
    mFrame, eCSSProperty_transform,
    AnimationPerformanceWarning(
      AnimationPerformanceWarning::Type::OpacityFrameInactive));

  return false;
}

bool
nsDisplayTransform::ShouldPrerender() {
  return mPrerender;
}

bool
nsDisplayTransform::CanUseAsyncAnimations(nsDisplayListBuilder* aBuilder)
{
  return mPrerender;
}

/* static */ bool
nsDisplayTransform::ShouldPrerenderTransformedContent(nsDisplayListBuilder* aBuilder,
                                                      nsIFrame* aFrame)
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

  nsRect visual = aFrame->GetVisualOverflowRect();


  EffectCompositor::SetPerformanceWarning(
    aFrame, eCSSProperty_transform,
    AnimationPerformanceWarning(
      AnimationPerformanceWarning::Type::ContentTooLarge,
      {
        nsPresContext::AppUnitsToIntCSSPixels(frameSize.width),
        nsPresContext::AppUnitsToIntCSSPixels(frameSize.height),
        nsPresContext::AppUnitsToIntCSSPixels(refSize.width),
        nsPresContext::AppUnitsToIntCSSPixels(refSize.height),
        nsPresContext::AppUnitsToIntCSSPixels(visual.width),
        nsPresContext::AppUnitsToIntCSSPixels(visual.height),
        nsPresContext::AppUnitsToIntCSSPixels(maxInAppUnits)
      }));
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
    } else if (!mIsTransformSeparator) {
      DebugOnly<bool> isReference =
        mFrame->IsTransformed() ||
        mFrame->Combines3DTransformWithAncestors() || mFrame->Extend3DContext();
      MOZ_ASSERT(isReference);
      mTransform =
        GetResultingTransformMatrix(mFrame, ToReferenceFrame(),
                                    scale, INCLUDE_PERSPECTIVE|OFFSET_BY_ORIGIN);
    }
  }
  return mTransform;
}

Matrix4x4
nsDisplayTransform::GetTransformForRendering()
{
  if (!mFrame->HasPerspective() || mTransformGetter || mIsTransformSeparator) {
    return GetTransform();
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
      mTransformPreserves3D = GetTransform();
      return mTransformPreserves3D;
    }

    const nsIFrame* establisher; // Establisher of the 3D rendering context.
    for (establisher = mFrame;
         establisher && establisher->Combines3DTransformWithAncestors();
         establisher = nsLayoutUtils::GetCrossDocParentFrame(establisher)) {
    }
    establisher = nsLayoutUtils::GetCrossDocParentFrame(establisher);
    const nsIFrame* establisherReference =
      aBuilder->FindReferenceFrameFor(establisher);

    nsPoint offset = mFrame->GetOffsetToCrossDoc(establisherReference);
    float scale = mFrame->PresContext()->AppUnitsPerDevPixel();
    uint32_t flags = INCLUDE_PRESERVE3D_ANCESTORS|INCLUDE_PERSPECTIVE|OFFSET_BY_ORIGIN;
    mTransformPreserves3D =
      GetResultingTransformMatrix(mFrame, offset, scale, flags);
  }
  return mTransformPreserves3D;
}

bool
nsDisplayTransform::ShouldBuildLayerEvenIfInvisible(nsDisplayListBuilder* aBuilder)
{
  // The visible rect of a Preserves-3D frame is just an intermediate
  // result.  It should always build a layer to make sure it is
  // rendering correctly.
  return ShouldPrerender() || mFrame->Combines3DTransformWithAncestors();
}

already_AddRefed<Layer> nsDisplayTransform::BuildLayer(nsDisplayListBuilder *aBuilder,
                                                       LayerManager *aManager,
                                                       const ContainerLayerParameters& aContainerParameters)
{
  /* For frames without transform, it would not be removed for
   * backface hidden here.  But, it would be removed by the init
   * function of nsDisplayTransform.
   */
  const Matrix4x4& newTransformMatrix = GetTransformForRendering();

  uint32_t flags = ShouldPrerender() ?
    FrameLayerBuilder::CONTAINER_NOT_CLIPPED_BY_ANCESTORS : 0;
  flags |= FrameLayerBuilder::CONTAINER_ALLOW_PULL_BACKGROUND_COLOR;
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

  nsDisplayListBuilder::AddAnimationsAndTransitionsToLayer(container, aBuilder,
                                                           this, mFrame,
                                                           eCSSProperty_transform);
  if (ShouldPrerender()) {
    if (MayBeAnimated(aBuilder)) {
      // Only allow async updates to the transform if we're an animated layer, since that's what
      // triggers us to set the correct AGR in the constructor and makes sure FrameLayerBuilder
      // won't compute occlusions for this layer.
      container->SetUserData(nsIFrame::LayerIsPrerenderedDataKey(),
                             /*the value is irrelevant*/nullptr);
    }
    container->SetContentFlags(container->GetContentFlags() | Layer::CONTENT_MAY_CHANGE_TRANSFORM);
  } else {
    container->RemoveUserData(nsIFrame::LayerIsPrerenderedDataKey());
    container->SetContentFlags(container->GetContentFlags() & ~Layer::CONTENT_MAY_CHANGE_TRANSFORM);
  }
  return container.forget();
}

bool
nsDisplayTransform::MayBeAnimated(nsDisplayListBuilder* aBuilder)
{
  // Here we check if the *post-transform* bounds of this item are big enough
  // to justify an active layer.
  if (ActiveLayerTracker::IsStyleAnimated(aBuilder,
                                          mFrame,
                                          eCSSProperty_transform) ||
      EffectCompositor::HasAnimationsForCompositor(mFrame,
                                                   eCSSProperty_transform)) {
    if (!IsItemTooSmallForActiveLayer(this)) {
      return true;
    }
    SetAnimationPerformanceWarningForTooSmallItem(this, eCSSProperty_transform);
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
  if (!GetTransform().Is2D() || mFrame->Combines3DTransformWithAncestors() ||
      mIsTransformSeparator) {
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
  /* As we do this, we need to be sure to
   * untransform the visible rect, since we want everything that's painting to
   * think that it's painting in its original rectangular coordinate space.
   * If we can't untransform, take the entire overflow rect */
  nsRect untransformedVisibleRect;
  if (ShouldPrerender() ||
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

  Point3D transformed = matrix * Point3D(point2d.x, point2d.y, 0);
  return transformed.z;
}

/* The bounding rectangle for the object is the overflow rectangle translated
 * by the reference point.
 */
nsRect
nsDisplayTransform::GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap)
{
  *aSnap = false;

  if (mHasBounds) {
    return mBounds;
  }

  if (mFrame->Extend3DContext() && !mIsTransformSeparator) {
    return nsRect();
  }

  nsRect untransformedBounds = ShouldPrerender() ?
    mFrame->GetVisualOverflowRectRelativeToSelf() :
    mStoredList.GetBounds(aBuilder, aSnap);
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

  accTransform.Accumulate(GetTransform());

  if (!IsLeafOf3DContext()) {
    // Do not dive into another 3D context.
    mStoredList.DoUpdateBoundsPreserves3D(aBuilder);
  }

  /* For Preserves3D, it is bounds of only children as leaf frames.
   * For non-leaf frames, their bounds are accumulated and kept at
   * nsDisplayListBuilder.
   */
  bool snap;
  nsRect untransformedBounds = ShouldPrerender() ?
    mFrame->GetVisualOverflowRectRelativeToSelf() :
    mStoredList.GetBounds(aBuilder, &snap);
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
  if (ShouldPrerender() ||
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
nsDisplayTransform::TryMerge(nsDisplayItem *aItem)
{
  return false;
}

#else

bool
nsDisplayTransform::TryMerge(nsDisplayItem *aItem)
{
  NS_PRECONDITION(aItem, "Why did you try merging with a null item?");

  /* Make sure that we're dealing with two transforms. */
  if (aItem->GetType() != TYPE_TRANSFORM)
    return false;

  /* Check to see that both frames are part of the same content. */
  if (aItem->Frame()->GetContent() != mFrame->GetContent())
    return false;

  if (aItem->GetClip() != GetClip())
    return false;

  if (aItem->ScrollClip() != ScrollClip())
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
                                         const nsRect* aBoundsOverride,
                                         bool aPreserves3D)
{
  NS_PRECONDITION(aFrame, "Can't take the transform based on a null frame!");

  float factor = aFrame->PresContext()->AppUnitsPerDevPixel();

  uint32_t flags = INCLUDE_PERSPECTIVE|BASIS_AT_ORIGIN;
  if (aPreserves3D) {
    flags |= INCLUDE_PRESERVE3D_ANCESTORS;
  }
  return nsLayoutUtils::MatrixTransformRect
    (aUntransformedBounds,
     GetResultingTransformMatrix(aFrame, aOrigin, factor, flags, aBoundsOverride),
     factor);
}

bool nsDisplayTransform::UntransformRect(const nsRect &aTransformedBounds,
                                         const nsRect &aChildBounds,
                                         const nsIFrame* aFrame,
                                         const nsPoint &aOrigin,
                                         nsRect *aOutRect,
                                         bool aPreserves3D)
{
  NS_PRECONDITION(aFrame, "Can't take the transform based on a null frame!");

  float factor = aFrame->PresContext()->AppUnitsPerDevPixel();

  uint32_t flags = INCLUDE_PERSPECTIVE|BASIS_AT_ORIGIN;
  if (aPreserves3D) {
    flags |= INCLUDE_PRESERVE3D_ANCESTORS;
  }

  Matrix4x4 transform = GetResultingTransformMatrix(aFrame, aOrigin, factor, flags);
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

bool nsDisplayTransform::UntransformVisibleRect(nsDisplayListBuilder* aBuilder,
                                                nsRect *aOutRect)
{
  const Matrix4x4& matrix = GetTransform();
  if (matrix.IsSingular())
    return false;

  // GetTransform always operates in dev pixels.
  float factor = mFrame->PresContext()->AppUnitsPerDevPixel();
  RectDouble result(NSAppUnitsToFloatPixels(mVisibleRect.x, factor),
                    NSAppUnitsToFloatPixels(mVisibleRect.y, factor),
                    NSAppUnitsToFloatPixels(mVisibleRect.width, factor),
                    NSAppUnitsToFloatPixels(mVisibleRect.height, factor));

  bool snap;
  nsRect childBounds = mStoredList.GetBounds(aBuilder, &snap);
  RectDouble childGfxBounds(NSAppUnitsToFloatPixels(childBounds.x, factor),
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
                                           nsIFrame* aTransformFrame,
                                           nsIFrame* aPerspectiveFrame,
                                           nsDisplayList* aList)
  : nsDisplayItem(aBuilder, aPerspectiveFrame)
  , mList(aBuilder, aPerspectiveFrame, aList)
  , mTransformFrame(aTransformFrame)
  , mIndex(aBuilder->AllocatePerspectiveItemIndex())
{
  MOZ_ASSERT(mList.GetChildren()->Count() == 1);
  MOZ_ASSERT(mList.GetChildren()->GetTop()->GetType() == TYPE_TRANSFORM);
}

already_AddRefed<Layer>
nsDisplayPerspective::BuildLayer(nsDisplayListBuilder *aBuilder,
                                 LayerManager *aManager,
                                 const ContainerLayerParameters& aContainerParameters)
{
  float appUnitsPerPixel = mFrame->PresContext()->AppUnitsPerDevPixel();

  Matrix4x4 perspectiveMatrix;
  DebugOnly<bool> hasPerspective =
    nsDisplayTransform::ComputePerspectiveMatrix(mTransformFrame, appUnitsPerPixel,
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

int32_t
nsDisplayPerspective::ZIndex() const
{
  return ZIndexForFrame(mTransformFrame);
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
                         nsDisplayList* aList, mozilla::gfx::VRDeviceProxy* aHMD)
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
  RefPtr<ContainerLayer> container = aManager->GetLayerBuilder()->
    BuildContainerLayerFor(aBuilder, aManager, mFrame, this, &mList,
                           newContainerParameters, nullptr, flags);

  container->SetVRDeviceID(mHMD->GetDeviceInfo().GetDeviceID());
  container->SetInputFrameID(mHMD->GetSensorState().inputFrameID);
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
  nsRect borderArea = nsRect(ToReferenceFrame(), mFrame->GetSize());
  nsSVGIntegrationUtils::PaintFramesWithEffects(*aCtx->ThebesContext(), mFrame,
                                                mVisibleRect,
                                                borderArea,
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

  float opacity = mFrame->StyleEffects()->mOpacity;
  if (opacity == 0.0f)
    return nullptr;

  nsIFrame* firstFrame =
    nsLayoutUtils::FirstContinuationOrIBSplitSibling(mFrame);
  nsSVGEffects::EffectProperties effectProperties =
    nsSVGEffects::GetEffectProperties(firstFrame);

  bool isOK = effectProperties.HasNoFilterOrHasValidFilter();
  effectProperties.GetClipPathFrame(&isOK);

  if (!isOK) {
    return nullptr;
  }

  ContainerLayerParameters newContainerParameters = aContainerParameters;
  if (effectProperties.HasValidFilter()) {
    newContainerParameters.mDisableSubpixelAntialiasingInDescendants = true;
  }

  RefPtr<ContainerLayer> container = aManager->GetLayerBuilder()->
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

bool nsDisplaySVGEffects::TryMerge(nsDisplayItem* aItem)
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
  if (aItem->ScrollClip() != ScrollClip())
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
  if (mFrame->StyleEffects()->mOpacity != 1.0f) {
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

