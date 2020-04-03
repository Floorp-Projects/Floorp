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
#include "mozilla/dom/BrowserChild.h"
#include "mozilla/dom/HTMLCanvasElement.h"
#include "mozilla/dom/KeyframeEffect.h"
#include "mozilla/dom/Selection.h"
#include "mozilla/dom/ServiceWorkerRegistrar.h"
#include "mozilla/dom/ServiceWorkerRegistration.h"
#include "mozilla/dom/SVGElement.h"
#include "mozilla/dom/TouchEvent.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/layers/PLayerTransaction.h"
#include "mozilla/PresShell.h"
#include "mozilla/ShapeUtils.h"
#include "mozilla/StaticPrefs_apz.h"
#include "mozilla/StaticPrefs_gfx.h"
#include "mozilla/StaticPrefs_layers.h"
#include "mozilla/StaticPrefs_layout.h"
#include "nsCSSRendering.h"
#include "nsCSSRenderingGradients.h"
#include "nsRegion.h"
#include "nsStyleStructInlines.h"
#include "nsStyleTransformMatrix.h"
#include "nsTransitionManager.h"
#include "gfxMatrix.h"
#include "nsSVGIntegrationUtils.h"
#include "nsSVGUtils.h"
#include "nsLayoutUtils.h"
#include "nsIScrollableFrame.h"
#include "nsIFrameInlines.h"
#include "nsStyleConsts.h"
#include "BorderConsts.h"
#include "LayerTreeInvalidation.h"
#include "mozilla/MathAlgorithms.h"

#include "imgIContainer.h"
#include "BasicLayers.h"
#include "nsBoxFrame.h"
#include "nsImageFrame.h"
#include "nsSubDocumentFrame.h"
#include "SVGObserverUtils.h"
#include "nsSVGClipPathFrame.h"
#include "GeckoProfiler.h"
#include "nsViewManager.h"
#include "ImageLayers.h"
#include "ImageContainer.h"
#include "nsCanvasFrame.h"
#include "nsSubDocumentFrame.h"
#include "StickyScrollContainer.h"
#include "mozilla/AnimationPerformanceWarning.h"
#include "mozilla/AnimationUtils.h"
#include "mozilla/AutoRestore.h"
#include "mozilla/EffectCompositor.h"
#include "mozilla/EffectSet.h"
#include "mozilla/EventStates.h"
#include "mozilla/HashTable.h"
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
#include "nsPrintfCString.h"
#include "UnitTransforms.h"
#include "LayerAnimationInfo.h"
#include "LayersLogging.h"
#include "FrameLayerBuilder.h"
#include "mozilla/EventStateManager.h"
#include "nsCaret.h"
#include "nsDOMTokenList.h"
#include "nsCSSProps.h"
#include "nsSVGMaskFrame.h"
#include "nsTableCellFrame.h"
#include "nsTableColFrame.h"
#include "nsTextFrame.h"
#include "nsSliderFrame.h"
#include "nsFocusManager.h"
#include "ClientLayerManager.h"
#include "mozilla/layers/AnimationHelper.h"
#include "mozilla/layers/RenderRootStateManager.h"
#include "mozilla/layers/StackingContextHelper.h"
#include "mozilla/layers/TreeTraversal.h"
#include "mozilla/layers/WebRenderBridgeChild.h"
#include "mozilla/layers/WebRenderLayerManager.h"
#include "mozilla/layers/WebRenderMessages.h"
#include "mozilla/layers/WebRenderScrollData.h"

using namespace mozilla;
using namespace mozilla::layers;
using namespace mozilla::dom;
using namespace mozilla::layout;
using namespace mozilla::gfx;

typedef ScrollableLayerGuid::ViewID ViewID;
typedef nsStyleTransformMatrix::TransformReferenceBox TransformReferenceBox;

#ifdef DEBUG
static bool SpammyLayoutWarningsEnabled() {
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
  nsIFrame::DisplayItemArray* items =
      aItem->Frame()->GetProperty(nsIFrame::DisplayItems());
  if (!items) {
    return;
  }
  for (nsDisplayItemBase* i : *items) {
    if (i != aItem && !i->HasDeletedFrame() && i->Frame() == aItem->Frame() &&
        i->GetPerFrameKey() == aItem->GetPerFrameKey()) {
      if (i->IsPreProcessedItem()) {
        continue;
      }
      MOZ_DIAGNOSTIC_ASSERT(false, "Duplicate display item!");
    }
  }
}
#endif

bool ShouldBuildItemForEventsOrPlugins(const DisplayItemType aType) {
  return aType == DisplayItemType::TYPE_COMPOSITOR_HITTEST_INFO ||
         aType == DisplayItemType::TYPE_PLUGIN ||
         (GetDisplayItemFlagsForType(aType) & TYPE_IS_CONTAINER);
}

void UpdateDisplayItemData(nsPaintedDisplayItem* aItem) {
  for (mozilla::DisplayItemData* did : aItem->Frame()->DisplayItemData()) {
    if (did->GetDisplayItemKey() == aItem->GetPerFrameKey() &&
        did->GetLayer()->AsPaintedLayer()) {
      if (!did->HasMergedFrames()) {
        aItem->SetDisplayItemData(did, did->GetLayer()->Manager());
      }

      return;
    }
  }
}

/* static */
bool ActiveScrolledRoot::IsAncestor(const ActiveScrolledRoot* aAncestor,
                                    const ActiveScrolledRoot* aDescendant) {
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

/* static */
nsCString ActiveScrolledRoot::ToString(
    const ActiveScrolledRoot* aActiveScrolledRoot) {
  nsAutoCString str;
  for (auto* asr = aActiveScrolledRoot; asr; asr = asr->mParent) {
    str.AppendPrintf("<0x%p>", asr->mScrollableFrame);
    if (asr->mParent) {
      str.AppendLiteral(", ");
    }
  }
  return std::move(str);
}

static StyleTransformOperation ResolveTranslate(
    TransformReferenceBox& aRefBox, const LengthPercentage& aX,
    const LengthPercentage& aY = LengthPercentage::Zero(),
    const Length& aZ = Length{0}) {
  float x = nsStyleTransformMatrix::ProcessTranslatePart(
      aX, &aRefBox, &TransformReferenceBox::Width);
  float y = nsStyleTransformMatrix::ProcessTranslatePart(
      aY, &aRefBox, &TransformReferenceBox::Height);
  return StyleTransformOperation::Translate3D(
      LengthPercentage::FromPixels(x), LengthPercentage::FromPixels(y), aZ);
}

static StyleTranslate ResolveTranslate(const StyleTranslate& aValue,
                                       TransformReferenceBox& aRefBox) {
  if (aValue.IsTranslate()) {
    const auto& t = aValue.AsTranslate();
    float x = nsStyleTransformMatrix::ProcessTranslatePart(
        t._0, &aRefBox, &TransformReferenceBox::Width);
    float y = nsStyleTransformMatrix::ProcessTranslatePart(
        t._1, &aRefBox, &TransformReferenceBox::Height);
    return StyleTranslate::Translate(LengthPercentage::FromPixels(x),
                                     LengthPercentage::FromPixels(y), t._2);
  }

  MOZ_ASSERT(aValue.IsNone());
  return StyleTranslate::None();
}

static StyleTransform ResolveTransformOperations(
    const StyleTransform& aTransform, TransformReferenceBox& aRefBox) {
  auto convertMatrix = [](const Matrix4x4& aM) {
    return StyleTransformOperation::Matrix3D(StyleGenericMatrix3D<StyleNumber>{
        aM._11, aM._12, aM._13, aM._14, aM._21, aM._22, aM._23, aM._24, aM._31,
        aM._32, aM._33, aM._34, aM._41, aM._42, aM._43, aM._44});
  };

  Vector<StyleTransformOperation> result;
  MOZ_RELEASE_ASSERT(
      result.initCapacity(aTransform.Operations().Length()),
      "Allocating vector of transform operations should be successful.");

  for (const StyleTransformOperation& op : aTransform.Operations()) {
    switch (op.tag) {
      case StyleTransformOperation::Tag::TranslateX:
        result.infallibleAppend(ResolveTranslate(aRefBox, op.AsTranslateX()));
        break;
      case StyleTransformOperation::Tag::TranslateY:
        result.infallibleAppend(ResolveTranslate(
            aRefBox, LengthPercentage::Zero(), op.AsTranslateY()));
        break;
      case StyleTransformOperation::Tag::TranslateZ:
        result.infallibleAppend(
            ResolveTranslate(aRefBox, LengthPercentage::Zero(),
                             LengthPercentage::Zero(), op.AsTranslateZ()));
        break;
      case StyleTransformOperation::Tag::Translate: {
        const auto& translate = op.AsTranslate();
        result.infallibleAppend(
            ResolveTranslate(aRefBox, translate._0, translate._1));
        break;
      }
      case StyleTransformOperation::Tag::Translate3D: {
        const auto& translate = op.AsTranslate3D();
        result.infallibleAppend(ResolveTranslate(aRefBox, translate._0,
                                                 translate._1, translate._2));
        break;
      }
      case StyleTransformOperation::Tag::InterpolateMatrix: {
        Matrix4x4 matrix;
        nsStyleTransformMatrix::ProcessInterpolateMatrix(matrix, op, aRefBox);
        result.infallibleAppend(convertMatrix(matrix));
        break;
      }
      case StyleTransformOperation::Tag::AccumulateMatrix: {
        Matrix4x4 matrix;
        nsStyleTransformMatrix::ProcessAccumulateMatrix(matrix, op, aRefBox);
        result.infallibleAppend(convertMatrix(matrix));
        break;
      }
      case StyleTransformOperation::Tag::RotateX:
      case StyleTransformOperation::Tag::RotateY:
      case StyleTransformOperation::Tag::RotateZ:
      case StyleTransformOperation::Tag::Rotate:
      case StyleTransformOperation::Tag::Rotate3D:
      case StyleTransformOperation::Tag::ScaleX:
      case StyleTransformOperation::Tag::ScaleY:
      case StyleTransformOperation::Tag::ScaleZ:
      case StyleTransformOperation::Tag::Scale:
      case StyleTransformOperation::Tag::Scale3D:
      case StyleTransformOperation::Tag::SkewX:
      case StyleTransformOperation::Tag::SkewY:
      case StyleTransformOperation::Tag::Skew:
      case StyleTransformOperation::Tag::Matrix:
      case StyleTransformOperation::Tag::Matrix3D:
      case StyleTransformOperation::Tag::Perspective:
        result.infallibleAppend(op);
        break;
      default:
        MOZ_ASSERT_UNREACHABLE("Function not handled yet!");
    }
  }

  auto transform = StyleTransform{
      StyleOwnedSlice<StyleTransformOperation>(std::move(result))};
  MOZ_ASSERT(!transform.HasPercent());
  MOZ_ASSERT(transform.Operations().Length() ==
             aTransform.Operations().Length());
  return transform;
}

static TimingFunction ToTimingFunction(
    const Maybe<ComputedTimingFunction>& aCTF) {
  if (aCTF.isNothing()) {
    return TimingFunction(null_t());
  }

  if (aCTF->HasSpline()) {
    const SMILKeySpline* spline = aCTF->GetFunction();
    return TimingFunction(CubicBezierFunction(spline->X1(), spline->Y1(),
                                              spline->X2(), spline->Y2()));
  }

  return TimingFunction(StepFunction(
      aCTF->GetSteps().mSteps, static_cast<uint8_t>(aCTF->GetSteps().mPos)));
}

// FIXME: Bug 1489392: We don't have to normalize the path here if we accept
// the spec issue which would like to normalize svg paths at computed time.
static StyleOffsetPath NormalizeOffsetPath(const StyleOffsetPath& aOffsetPath) {
  if (aOffsetPath.IsPath()) {
    return StyleOffsetPath::Path(
        MotionPathUtils::NormalizeSVGPathData(aOffsetPath.AsPath()));
  }
  return StyleOffsetPath(aOffsetPath);
}

static void SetAnimatable(nsCSSPropertyID aProperty,
                          const AnimationValue& aAnimationValue,
                          nsIFrame* aFrame, TransformReferenceBox& aRefBox,
                          layers::Animatable& aAnimatable) {
  MOZ_ASSERT(aFrame);

  if (aAnimationValue.IsNull()) {
    aAnimatable = null_t();
    return;
  }

  switch (aProperty) {
    case eCSSProperty_background_color: {
      // We don't support color animation on the compositor yet so that we can
      // resolve currentColor at this moment.
      nscolor foreground =
          aFrame->Style()->GetVisitedDependentColor(&nsStyleText::mColor);
      aAnimatable = aAnimationValue.GetColor(foreground);
      break;
    }
    case eCSSProperty_opacity:
      aAnimatable = aAnimationValue.GetOpacity();
      break;
    case eCSSProperty_rotate:
      aAnimatable = aAnimationValue.GetRotateProperty();
      break;
    case eCSSProperty_scale:
      aAnimatable = aAnimationValue.GetScaleProperty();
      break;
    case eCSSProperty_translate:
      aAnimatable =
          ResolveTranslate(aAnimationValue.GetTranslateProperty(), aRefBox);
      break;
    case eCSSProperty_transform:
      aAnimatable = ResolveTransformOperations(
          aAnimationValue.GetTransformProperty(), aRefBox);
      break;
    case eCSSProperty_offset_path:
      aAnimatable =
          NormalizeOffsetPath(aAnimationValue.GetOffsetPathProperty());
      break;
    case eCSSProperty_offset_distance:
      aAnimatable = aAnimationValue.GetOffsetDistanceProperty();
      break;
    case eCSSProperty_offset_rotate:
      aAnimatable = aAnimationValue.GetOffsetRotateProperty();
      break;
    case eCSSProperty_offset_anchor:
      aAnimatable = aAnimationValue.GetOffsetAnchorProperty();
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Unsupported property");
  }
}

enum class Send {
  NextTransaction,
  Immediate,
};
static void AddAnimationForProperty(nsIFrame* aFrame,
                                    const AnimationProperty& aProperty,
                                    dom::Animation* aAnimation,
                                    const Maybe<TransformData>& aTransformData,
                                    Send aSendFlag,
                                    AnimationInfo& aAnimationInfo) {
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
      (aSendFlag == Send::NextTransaction)
          ? aAnimationInfo.AddAnimationForNextTransaction()
          : aAnimationInfo.AddAnimation();

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
  CSSTransition* cssTransition = aAnimation->AsCSSTransition();
  if (cssTransition) {
    cssTransition->UpdateStartValueFromReplacedTransition();
  }

  animation->originTime() =
      !aAnimation->GetTimeline()
          ? TimeStamp()
          : aAnimation->GetTimeline()->ToTimeStamp(TimeDuration());

  Nullable<TimeDuration> startTime = aAnimation->GetCurrentOrPendingStartTime();
  if (startTime.IsNull()) {
    animation->startTime() = Nothing();
  } else {
    animation->startTime() = Some(startTime.Value());
  }

  animation->holdTime() = aAnimation->GetCurrentTimeAsDuration().Value();

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
  animation->transformData() = aTransformData;
  animation->easingFunction() = ToTimingFunction(timing.TimingFunction());
  animation->iterationComposite() = static_cast<uint8_t>(
      aAnimation->GetEffect()->AsKeyframeEffect()->IterationComposite());
  animation->isNotPlaying() = !aAnimation->IsPlaying();
  animation->isNotAnimating() = false;

  TransformReferenceBox refBox(aFrame);

  // If the animation is additive or accumulates, we need to pass its base value
  // to the compositor.

  AnimationValue baseStyle =
      aAnimation->GetEffect()->AsKeyframeEffect()->BaseStyle(
          aProperty.mProperty);
  if (!baseStyle.IsNull()) {
    SetAnimatable(aProperty.mProperty, baseStyle, aFrame, refBox,
                  animation->baseStyle());
  } else {
    animation->baseStyle() = null_t();
  }

  for (uint32_t segIdx = 0; segIdx < aProperty.mSegments.Length(); segIdx++) {
    const AnimationPropertySegment& segment = aProperty.mSegments[segIdx];

    AnimationSegment* animSegment = animation->segments().AppendElement();
    SetAnimatable(aProperty.mProperty, segment.mFromValue, aFrame, refBox,
                  animSegment->startState());
    SetAnimatable(aProperty.mProperty, segment.mToValue, aFrame, refBox,
                  animSegment->endState());

    animSegment->startPortion() = segment.mFromKey;
    animSegment->endPortion() = segment.mToKey;
    animSegment->startComposite() =
        static_cast<uint8_t>(segment.mFromComposite);
    animSegment->endComposite() = static_cast<uint8_t>(segment.mToComposite);
    animSegment->sampleFn() = ToTimingFunction(segment.mTimingFunction);
  }
}

// Let's use an example to explain this function:
//
// We have 4 playing animations (without any !important rule or transition):
// Animation A: [ transform, rotate ].
// Animation B: [ rotate, scale ].
// Animation C: [ transform, margin-left ].
// Animation D: [ opacity, margin-left ].
//
// Normally, GetAnimationsForCompositor(|transform-like properties|) returns:
// [ Animation A, Animation B, Animation C ], which is the first argument of
// this function.
//
// In this function, we want to re-organize the list as (Note: don't care
// the order of properties):
// [
//   { rotate:    [ Animation A, Animation B ] },
//   { scale:     [ Animation B ] },
//   { transform: [ Animation A, Animation C ] },
// ]
//
// Therefore, AddAnimationsForProperty() will append each animation property
// into AnimationInfo,  as a final list of layers::Animation:
// [
//   { rotate: Animation A },
//   { rotate: Animation B },
//   { scale: Animation B },
//   { transform: Animation A },
//   { transform: Animation C },
// ]
//
// And then, for each transaction, we send this list to the compositor thread.
static HashMap<nsCSSPropertyID, nsTArray<RefPtr<dom::Animation>>>
GroupAnimationsByProperty(const nsTArray<RefPtr<dom::Animation>>& aAnimations,
                          const nsCSSPropertyIDSet& aPropertySet) {
  HashMap<nsCSSPropertyID, nsTArray<RefPtr<dom::Animation>>> groupedAnims;
  for (const RefPtr<dom::Animation>& anim : aAnimations) {
    const KeyframeEffect* effect = anim->GetEffect()->AsKeyframeEffect();
    MOZ_ASSERT(effect);
    for (const AnimationProperty& property : effect->Properties()) {
      if (!aPropertySet.HasProperty(property.mProperty)) {
        continue;
      }

      auto animsForPropertyPtr = groupedAnims.lookupForAdd(property.mProperty);
      if (!animsForPropertyPtr) {
        DebugOnly<bool> rv =
            groupedAnims.add(animsForPropertyPtr, property.mProperty,
                             nsTArray<RefPtr<dom::Animation>>());
        MOZ_ASSERT(rv, "Should have enough memory");
      }
      animsForPropertyPtr->value().AppendElement(anim);
    }
  }
  return groupedAnims;
}

static bool AddAnimationsForProperty(
    nsIFrame* aFrame, const EffectSet* aEffects,
    const nsTArray<RefPtr<dom::Animation>>& aCompositorAnimations,
    const Maybe<TransformData>& aTransformData, nsCSSPropertyID aProperty,
    Send aSendFlag, AnimationInfo& aAnimationInfo) {
  bool addedAny = false;
  // Add from first to last (since last overrides)
  for (dom::Animation* anim : aCompositorAnimations) {
    if (!anim->IsRelevant()) {
      continue;
    }

    dom::KeyframeEffect* keyframeEffect =
        anim->GetEffect() ? anim->GetEffect()->AsKeyframeEffect() : nullptr;
    MOZ_ASSERT(keyframeEffect,
               "A playing animation should have a keyframe effect");
    const AnimationProperty* property =
        keyframeEffect->GetEffectiveAnimationOfProperty(aProperty, *aEffects);
    if (!property) {
      continue;
    }

    // Note that if the property is overridden by !important rules,
    // GetEffectiveAnimationOfProperty returns null instead.
    // This is what we want, since if we have animations overridden by
    // !important rules, we don't want to send them to the compositor.
    MOZ_ASSERT(
        anim->CascadeLevel() != EffectCompositor::CascadeLevel::Animations ||
            !aEffects->PropertiesWithImportantRules().HasProperty(aProperty),
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

    AddAnimationForProperty(aFrame, *property, anim, aTransformData, aSendFlag,
                            aAnimationInfo);
    keyframeEffect->SetIsRunningOnCompositor(aProperty, true);
    addedAny = true;
  }
  return addedAny;
}

enum class AnimationDataType {
  WithMotionPath,
  WithoutMotionPath,
};
static Maybe<TransformData> CreateAnimationData(
    nsIFrame* aFrame, nsDisplayItem* aItem, DisplayItemType aType,
    layers::LayersBackend aLayersBackend, AnimationDataType aDataType) {
  if (aType != DisplayItemType::TYPE_TRANSFORM) {
    return Nothing();
  }

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
      nsDisplayTransform::GetDeltaToTransformOrigin(aFrame, refBox, scale);
  nsPoint origin;
  float scaleX = 1.0f;
  float scaleY = 1.0f;
  bool hasPerspectiveParent = false;
  if (aLayersBackend == layers::LayersBackend::LAYERS_WR) {
    // leave origin empty, because we are sending it separately on the
    // stacking context that we are pushing to WR, and WR will automatically
    // include it when picking up the animated transform values
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
    nsIFrame* referenceFrame = nsLayoutUtils::GetReferenceFrame(
        nsLayoutUtils::GetCrossDocParentFrame(aFrame));
    origin = aFrame->GetOffsetToCrossDoc(referenceFrame);
  }

  Maybe<MotionPathData> motionPathData;
  if (aDataType == AnimationDataType::WithMotionPath) {
    const StyleTransformOrigin& styleOrigin =
        aFrame->StyleDisplay()->mTransformOrigin;
    CSSPoint motionPathOrigin = nsStyleTransformMatrix::Convert2DPosition(
        styleOrigin.horizontal, styleOrigin.vertical, refBox);
    CSSPoint anchorAdjustment =
        MotionPathUtils::ComputeAnchorPointAdjustment(*aFrame);

    motionPathData = Some(layers::MotionPathData(
        motionPathOrigin, anchorAdjustment, RayReferenceData(aFrame)));
  }

  return Some(TransformData(origin, offsetToTransformOrigin, bounds,
                            devPixelsToAppUnits, scaleX, scaleY,
                            hasPerspectiveParent, motionPathData));
}

static void AddNonAnimatingTransformLikePropertiesStyles(
    const nsCSSPropertyIDSet& aNonAnimatingProperties, nsIFrame* aFrame,
    Send aSendFlag, AnimationInfo& aAnimationInfo) {
  auto appendFakeAnimation = [&aAnimationInfo, aSendFlag](
                                 nsCSSPropertyID aProperty,
                                 Animatable&& aBaseStyle) {
    layers::Animation* animation =
        (aSendFlag == Send::NextTransaction)
            ? aAnimationInfo.AddAnimationForNextTransaction()
            : aAnimationInfo.AddAnimation();
    animation->property() = aProperty;
    animation->baseStyle() = std::move(aBaseStyle);
    animation->easingFunction() = null_t();
    animation->isNotAnimating() = true;
  };

  const nsStyleDisplay* display = aFrame->StyleDisplay();
  // A simple optimization. We don't need to send offset-* properties if we
  // don't have offset-path and offset-position.
  // FIXME: Bug 1559232: Add offset-position here.
  bool hasMotion =
      !display->mOffsetPath.IsNone() ||
      !aNonAnimatingProperties.HasProperty(eCSSProperty_offset_path);

  for (nsCSSPropertyID id : aNonAnimatingProperties) {
    switch (id) {
      case eCSSProperty_transform:
        if (!display->mTransform.IsNone()) {
          TransformReferenceBox refBox(aFrame);
          appendFakeAnimation(
              id, ResolveTransformOperations(display->mTransform, refBox));
        }
        break;
      case eCSSProperty_translate:
        if (!display->mTranslate.IsNone()) {
          TransformReferenceBox refBox(aFrame);
          appendFakeAnimation(id,
                              ResolveTranslate(display->mTranslate, refBox));
        }
        break;
      case eCSSProperty_rotate:
        if (!display->mRotate.IsNone()) {
          appendFakeAnimation(id, display->mRotate);
        }
        break;
      case eCSSProperty_scale:
        if (!display->mScale.IsNone()) {
          appendFakeAnimation(id, display->mScale);
        }
        break;
      case eCSSProperty_offset_path:
        if (!display->mOffsetPath.IsNone()) {
          appendFakeAnimation(id, NormalizeOffsetPath(display->mOffsetPath));
        }
        break;
      case eCSSProperty_offset_distance:
        if (hasMotion && !display->mOffsetDistance.IsDefinitelyZero()) {
          appendFakeAnimation(id, display->mOffsetDistance);
        }
        break;
      case eCSSProperty_offset_rotate:
        if (hasMotion && (!display->mOffsetRotate.auto_ ||
                          display->mOffsetRotate.angle.ToDegrees() != 0.0)) {
          appendFakeAnimation(id, display->mOffsetRotate);
        }
        break;
      case eCSSProperty_offset_anchor:
        if (hasMotion && !display->mOffsetAnchor.IsAuto()) {
          appendFakeAnimation(id, display->mOffsetAnchor);
        }
        break;
      default:
        MOZ_ASSERT_UNREACHABLE("Unsupported transform-like properties");
    }
  }
}

static void AddAnimationsForDisplayItem(nsIFrame* aFrame,
                                        nsDisplayListBuilder* aBuilder,
                                        nsDisplayItem* aItem,
                                        DisplayItemType aType, Send aSendFlag,
                                        layers::LayersBackend aLayersBackend,
                                        AnimationInfo& aAnimationInfo) {
  if (aSendFlag == Send::NextTransaction) {
    aAnimationInfo.ClearAnimationsForNextTransaction();
  } else {
    aAnimationInfo.ClearAnimations();
  }

  // Update the animation generation on the layer. We need to do this before
  // any early returns since even if we don't add any animations to the
  // layer, we still need to mark it as up-to-date with regards to animations.
  // Otherwise, in RestyleManager we'll notice the discrepancy between the
  // animation generation numbers and update the layer indefinitely.
  EffectSet* effects = EffectSet::GetEffectSetForFrame(aFrame, aType);
  uint64_t animationGeneration =
      effects ? effects->GetAnimationGeneration() : 0;
  aAnimationInfo.SetAnimationGeneration(animationGeneration);
  if (!effects || effects->IsEmpty()) {
    return;
  }

  EffectCompositor::ClearIsRunningOnCompositor(aFrame, aType);
  const nsCSSPropertyIDSet& propertySet =
      LayerAnimationInfo::GetCSSPropertiesFor(aType);
  const nsTArray<RefPtr<dom::Animation>> matchedAnimations =
      EffectCompositor::GetAnimationsForCompositor(aFrame, propertySet);
  if (matchedAnimations.IsEmpty()) {
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
    return;
  }

  const HashMap<nsCSSPropertyID, nsTArray<RefPtr<dom::Animation>>>
      compositorAnimations =
          GroupAnimationsByProperty(matchedAnimations, propertySet);
  Maybe<TransformData> transformData =
      CreateAnimationData(aFrame, aItem, aType, aLayersBackend,
                          compositorAnimations.has(eCSSProperty_offset_path) ||
                                  !aFrame->StyleDisplay()->mOffsetPath.IsNone()
                              ? AnimationDataType::WithMotionPath
                              : AnimationDataType::WithoutMotionPath);
  // Bug 1424900: Drop this pref check after shipping individual transforms.
  // Bug 1582554: Drop this pref check after shipping motion path.
  const bool hasMultipleTransformLikeProperties =
      (StaticPrefs::layout_css_individual_transform_enabled() ||
       StaticPrefs::layout_css_motion_path_enabled()) &&
      aType == DisplayItemType::TYPE_TRANSFORM;
  nsCSSPropertyIDSet nonAnimatingProperties =
      nsCSSPropertyIDSet::TransformLikeProperties();
  for (auto iter = compositorAnimations.iter(); !iter.done(); iter.next()) {
    // Note: We can skip offset-* if there is no offset-path/offset-position
    // animations and styles. However, it should be fine and may be better to
    // send these information to the compositor because 1) they are simple data
    // structure, 2) AddAnimationsForProperty() marks these animations as
    // running on the composiror, so CanThrottle() returns true for them, and
    // we avoid running these animations on the main thread.
    bool added = AddAnimationsForProperty(aFrame, effects, iter.get().value(),
                                          transformData, iter.get().key(),
                                          aSendFlag, aAnimationInfo);
    if (added && transformData) {
      // Only copy TransformLikeMetaData in the first animation property.
      transformData.reset();
    }

    if (hasMultipleTransformLikeProperties && added) {
      nonAnimatingProperties.RemoveProperty(iter.get().key());
    }
  }

  // If some transform-like properties have animations, but others not, and
  // those non-animating transform-like properties have non-none
  // transform/translate/rotate/scale styles or non-initial value for motion
  // path properties, we also pass their styles into the compositor, so the
  // final transform matrix (on the compositor) could take them into account.
  if (hasMultipleTransformLikeProperties &&
      // For these cases we don't need to send the property style values to
      // the compositor:
      // 1. No property has running animations on the compositor. (i.e. All
      //    properties should be handled by main thread)
      // 2. All properties have running animations on the compositor.
      //    (i.e. Those running animations should override the styles.)
      !nonAnimatingProperties.Equals(
          nsCSSPropertyIDSet::TransformLikeProperties()) &&
      !nonAnimatingProperties.IsEmpty()) {
    AddNonAnimatingTransformLikePropertiesStyles(nonAnimatingProperties, aFrame,
                                                 aSendFlag, aAnimationInfo);
  }
}

static uint64_t AddAnimationsForWebRender(
    nsDisplayItem* aItem, mozilla::layers::RenderRootStateManager* aManager,
    nsDisplayListBuilder* aDisplayListBuilder, wr::RenderRoot aRenderRoot) {
  EffectSet* effects =
      EffectSet::GetEffectSetForFrame(aItem->Frame(), aItem->GetType());
  if (!effects || effects->IsEmpty()) {
    // If there is no animation on the nsIFrame, that means
    //  1) we've never created any animations on this frame or
    //  2) the frame was reconstruced or
    //  3) all animations on the frame have finished
    // in such cases we don't need do anything here.
    //
    // Even if there is a WebRenderAnimationData for the display item type on
    // this frame, it's going to be discarded since it's not marked as being
    // used.
    return 0;
  }

  RefPtr<WebRenderAnimationData> animationData =
      aManager->CommandBuilder()
          .CreateOrRecycleWebRenderUserData<WebRenderAnimationData>(
              aItem, aRenderRoot);
  AnimationInfo& animationInfo = animationData->GetAnimationInfo();
  AddAnimationsForDisplayItem(aItem->Frame(), aDisplayListBuilder, aItem,
                              aItem->GetType(), Send::Immediate,
                              layers::LayersBackend::LAYERS_WR, animationInfo);
  animationInfo.StartPendingAnimations(
      aManager->LayerManager()->GetAnimationReadyTime());

  // Note that animationsId can be 0 (uninitialized in AnimationInfo) if there
  // are no active animations.
  uint64_t animationsId = animationInfo.GetCompositorAnimationsId();
  if (!animationInfo.GetAnimations().IsEmpty()) {
    OpAddCompositorAnimations anim(
        CompositorAnimations(animationInfo.GetAnimations(), animationsId));
    aManager->WrBridge()->AddWebRenderParentCommand(anim, aRenderRoot);
    aManager->AddActiveCompositorAnimationId(animationsId);
  } else if (animationsId) {
    aManager->AddCompositorAnimationsIdForDiscard(animationsId);
    animationsId = 0;
  }

  return animationsId;
}

static bool GenerateAndPushTextMask(nsIFrame* aFrame, gfxContext* aContext,
                                    const nsRect& aFillRect,
                                    nsDisplayListBuilder* aBuilder) {
  if (aBuilder->IsForGenerateGlyphMask()) {
    return false;
  }

  SVGObserverUtils::GetAndObserveBackgroundClip(aFrame);

  // The main function of enabling background-clip:text property value.
  // When a nsDisplayBackgroundImage detects "text" bg-clip style, it will call
  // this function to
  // 1. Generate a mask by all descendant text frames
  // 2. Push the generated mask into aContext.

  gfxContext* sourceCtx = aContext;
  LayoutDeviceRect bounds = LayoutDeviceRect::FromAppUnits(
      aFillRect, aFrame->PresContext()->AppUnitsPerDevPixel());

  // Create a mask surface.
  RefPtr<DrawTarget> sourceTarget = sourceCtx->GetDrawTarget();
  RefPtr<DrawTarget> maskDT = sourceTarget->CreateClippedDrawTarget(
      bounds.ToUnknownRect(), SurfaceFormat::A8);
  if (!maskDT || !maskDT->IsValid()) {
    return false;
  }
  RefPtr<gfxContext> maskCtx =
      gfxContext::CreatePreservingTransformOrNull(maskDT);
  MOZ_ASSERT(maskCtx);
  maskCtx->Multiply(Matrix::Translation(bounds.TopLeft().ToUnknownPoint()));

  // Shade text shape into mask A8 surface.
  nsLayoutUtils::PaintFrame(
      maskCtx, aFrame, nsRect(nsPoint(0, 0), aFrame->GetSize()),
      NS_RGB(255, 255, 255), nsDisplayListBuilderMode::GenerateGlyph);

  // Push the generated mask into aContext, so that the caller can pop and
  // blend with it.

  Matrix currentMatrix = sourceCtx->CurrentMatrix();
  Matrix invCurrentMatrix = currentMatrix;
  invCurrentMatrix.Invert();

  RefPtr<SourceSurface> maskSurface = maskDT->Snapshot();
  sourceCtx->PushGroupForBlendBack(gfxContentType::COLOR_ALPHA, 1.0,
                                   maskSurface, invCurrentMatrix);

  return true;
}

/* static */
void nsDisplayListBuilder::AddAnimationsAndTransitionsToLayer(
    Layer* aLayer, nsDisplayListBuilder* aBuilder, nsDisplayItem* aItem,
    nsIFrame* aFrame, DisplayItemType aType) {
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

  Send sendFlag = !aBuilder ? Send::NextTransaction : Send::Immediate;
  AnimationInfo& animationInfo = aLayer->GetAnimationInfo();
  AddAnimationsForDisplayItem(aFrame, aBuilder, aItem, aType, sendFlag,
                              layers::LayersBackend::LAYERS_CLIENT,
                              animationInfo);
  animationInfo.TransferMutatedFlagToLayer(aLayer);
}

nsDisplayWrapList* nsDisplayListBuilder::MergeItems(
    nsTArray<nsDisplayWrapList*>& aItems) {
  // For merging, we create a temporary item by cloning the last item of the
  // mergeable items list. This ensures that the temporary item will have the
  // correct frame and bounds.
  nsDisplayWrapList* merged = nullptr;

  for (nsDisplayWrapList* item : Reversed(aItems)) {
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

void nsDisplayListBuilder::AutoCurrentActiveScrolledRootSetter::
    SetCurrentActiveScrolledRoot(
        const ActiveScrolledRoot* aActiveScrolledRoot) {
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
  const ActiveScrolledRoot* finiteBoundsASR =
      ActiveScrolledRoot::PickDescendant(mContentClipASR, aActiveScrolledRoot);

  // mCurrentContainerASR is adjusted so that it's still an ancestor of
  // finiteBoundsASR.
  mBuilder->mCurrentContainerASR = ActiveScrolledRoot::PickAncestor(
      mBuilder->mCurrentContainerASR, finiteBoundsASR);

  // If we are entering out-of-flow content inside a CSS filter, mark
  // scroll frames wrt. which the content is fixed as containing such content.
  if (mBuilder->mFilterASR && ActiveScrolledRoot::IsAncestor(
                                  aActiveScrolledRoot, mBuilder->mFilterASR)) {
    for (const ActiveScrolledRoot* asr = mBuilder->mFilterASR;
         asr && asr != aActiveScrolledRoot; asr = asr->mParent) {
      asr->mScrollableFrame->SetHasOutOfFlowContentInsideFilter();
    }
  }

  mUsed = true;
}

void nsDisplayListBuilder::AutoCurrentActiveScrolledRootSetter::
    InsertScrollFrame(nsIScrollableFrame* aScrollableFrame) {
  MOZ_ASSERT(!mUsed);
  size_t descendantsEndIndex = mBuilder->mActiveScrolledRoots.Length();
  const ActiveScrolledRoot* parentASR = mBuilder->mCurrentActiveScrolledRoot;
  const ActiveScrolledRoot* asr =
      mBuilder->AllocateActiveScrolledRoot(parentASR, aScrollableFrame);
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

/* static */
nsRect nsDisplayListBuilder::OutOfFlowDisplayData::ComputeVisibleRectForFrame(
    nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
    const nsRect& aVisibleRect, const nsRect& aDirtyRect,
    nsRect* aOutDirtyRect) {
  nsRect visible = aVisibleRect;
  nsRect dirtyRectRelativeToDirtyFrame = aDirtyRect;

  if (StaticPrefs::apz_allow_zooming() &&
      nsLayoutUtils::IsFixedPosFrameInDisplayPort(aFrame) &&
      aBuilder->IsPaintingToWindow()) {
    dirtyRectRelativeToDirtyFrame =
        nsRect(nsPoint(0, 0), aFrame->GetParent()->GetSize());

    // If there's a visual viewport size set, restrict the amount of the
    // fixed-position element we paint to the visual viewport. (In general
    // the fixed-position element can be as large as the layout viewport,
    // which at a high zoom level can cause us to paint too large of an
    // area.)
    PresShell* presShell = aFrame->PresShell();
    if (presShell->IsVisualViewportSizeSet()) {
      dirtyRectRelativeToDirtyFrame =
          nsRect(presShell->GetVisualViewportOffset(),
                 presShell->GetVisualViewportSize());
      // But if we have a displayport, expand it to the displayport, so
      // that async-scrolling the visual viewport within the layout viewport
      // will not checkerboard.
      if (nsIFrame* rootScrollFrame = presShell->GetRootScrollFrame()) {
        nsRect displayport;
        // Note that the displayport here is already in the right coordinate
        // space: it's relative to the scroll port (= layout viewport), but
        // covers the visual viewport with some margins around it, which is
        // exactly what we want.
        if (nsLayoutUtils::GetHighResolutionDisplayPort(
                rootScrollFrame->GetContent(), &displayport)) {
          dirtyRectRelativeToDirtyFrame = displayport;
        }
      }
    }
    visible = dirtyRectRelativeToDirtyFrame;
    if (StaticPrefs::apz_test_logging_enabled() &&
        presShell->GetDocument()->IsContentDocument()) {
      nsLayoutUtils::LogAdditionalTestData(
          aBuilder, "fixedPosDisplayport",
          ToString(CSSSize::FromAppUnits(visible)));
    }
  }

  *aOutDirtyRect = dirtyRectRelativeToDirtyFrame - aFrame->GetPosition();
  visible -= aFrame->GetPosition();

  nsRect overflowRect = aFrame->GetVisualOverflowRect();

  if (aFrame->IsTransformed() &&
      mozilla::EffectCompositor::HasAnimationsForCompositor(
          aFrame, DisplayItemType::TYPE_TRANSFORM)) {
    /**
     * Add a fuzz factor to the overflow rectangle so that elements only
     * just out of view are pulled into the display list, so they can be
     * prerendered if necessary.
     */
    overflowRect.Inflate(nsPresContext::CSSPixelsToAppUnits(32));
  }

  visible.IntersectRect(visible, overflowRect);
  aOutDirtyRect->IntersectRect(*aOutDirtyRect, overflowRect);

  return visible;
}

nsDisplayListBuilder::nsDisplayListBuilder(nsIFrame* aReferenceFrame,
                                           nsDisplayListBuilderMode aMode,
                                           bool aBuildCaret,
                                           bool aRetainingDisplayList)
    : mReferenceFrame(aReferenceFrame),
      mIgnoreScrollFrame(nullptr),
      mCurrentActiveScrolledRoot(nullptr),
      mCurrentContainerASR(nullptr),
      mCurrentFrame(aReferenceFrame),
      mCurrentReferenceFrame(aReferenceFrame),
      mRootAGR(AnimatedGeometryRoot::CreateAGRForFrame(
          aReferenceFrame, nullptr, true, aRetainingDisplayList)),
      mCurrentAGR(mRootAGR),
      mBuildingExtraPagesForPageNum(0),
      mUsedAGRBudget(0),
      mDirtyRect(-1, -1, -1, -1),
      mGlassDisplayItem(nullptr),
      mCaretFrame(nullptr),
      mScrollInfoItemsForHoisting(nullptr),
      mFirstClipChainToDestroy(nullptr),
      mMode(aMode),
      mTableBackgroundSet(nullptr),
      mCurrentScrollParentId(ScrollableLayerGuid::NULL_SCROLL_ID),
      mCurrentScrollbarTarget(ScrollableLayerGuid::NULL_SCROLL_ID),
      mFilterASR(nullptr),
      mContainsBlendMode(false),
      mIsBuildingScrollbar(false),
      mCurrentScrollbarWillHaveLayer(false),
      mBuildCaret(aBuildCaret),
      mRetainingDisplayList(aRetainingDisplayList),
      mPartialUpdate(false),
      mIgnoreSuppression(false),
      mIncludeAllOutOfFlows(false),
      mDescendIntoSubdocuments(true),
      mSelectedFramesOnly(false),
      mAllowMergingAndFlattening(true),
      mWillComputePluginGeometry(false),
      mInTransform(false),
      mInEventsAndPluginsOnly(false),
      mInFilter(false),
      mInPageSequence(false),
      mIsInChromePresContext(false),
      mSyncDecodeImages(false),
      mIsPaintingToWindow(false),
      mIsPaintingForWebRender(false),
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
      mPartialBuildFailed(false),
      mIsInActiveDocShell(false),
      mBuildAsyncZoomContainer(false),
      mBuildBackdropRootContainer(false),
      mContainsBackdropFilter(false),
      mHitTestArea(),
      mHitTestInfo(CompositorHitTestInvisibleToHit) {
  MOZ_COUNT_CTOR(nsDisplayListBuilder);

  mBuildCompositorHitTestInfo = mAsyncPanZoomEnabled && IsForPainting();

  ShouldRebuildDisplayListDueToPrefChange();

  static_assert(
      static_cast<uint32_t>(DisplayItemType::TYPE_MAX) < (1 << TYPE_BITS),
      "Check TYPE_MAX should not overflow");
}

static PresShell* GetFocusedPresShell() {
  nsPIDOMWindowOuter* focusedWnd =
      nsFocusManager::GetFocusManager()->GetFocusedWindow();
  if (!focusedWnd) {
    return nullptr;
  }

  nsCOMPtr<nsIDocShell> focusedDocShell = focusedWnd->GetDocShell();
  if (!focusedDocShell) {
    return nullptr;
  }

  return focusedDocShell->GetPresShell();
}

void nsDisplayListBuilder::BeginFrame() {
  nsCSSRendering::BeginFrameTreesLocked();
  mCurrentAGR = mRootAGR;
  mFrameToAnimatedGeometryRootMap.Put(mReferenceFrame, mRootAGR);

  mIsPaintingToWindow = false;
  mIgnoreSuppression = false;
  mInTransform = false;
  mInFilter = false;
  mSyncDecodeImages = false;

  for (auto& renderRootRect : mRenderRootRects) {
    renderRootRect = LayoutDeviceRect();
  }

  if (!mBuildCaret) {
    return;
  }

  RefPtr<PresShell> presShell = GetFocusedPresShell();
  if (presShell) {
    RefPtr<nsCaret> caret = presShell->GetCaret();
    mCaretFrame = caret->GetPaintGeometry(&mCaretRect);

    // The focused pres shell may not be in the document that we're
    // painting, or be in a popup. Check if the display root for
    // the caret matches the display root that we're painting, and
    // only use it if it matches.
    if (mCaretFrame &&
        nsLayoutUtils::GetDisplayRootFrame(mCaretFrame) !=
            nsLayoutUtils::GetDisplayRootFrame(mReferenceFrame)) {
      mCaretFrame = nullptr;
    }
  }
}

void nsDisplayListBuilder::EndFrame() {
  NS_ASSERTION(!mInInvalidSubtree,
               "Someone forgot to cleanup mInInvalidSubtree!");
  mFrameToAnimatedGeometryRootMap.Clear();
  mAGRBudgetSet.Clear();
  mActiveScrolledRoots.Clear();
  mEffectsUpdates.Clear();
  FreeClipChains();
  FreeTemporaryItems();
  nsCSSRendering::EndFrameTreesLocked();
  mCaretFrame = nullptr;
}

void nsDisplayListBuilder::MarkFrameForDisplay(nsIFrame* aFrame,
                                               nsIFrame* aStopAtFrame) {
  mFramesMarkedForDisplay.AppendElement(aFrame);
  for (nsIFrame* f = aFrame; f;
       f = nsLayoutUtils::GetParentOrPlaceholderForCrossDoc(f)) {
    if (f->GetStateBits() & NS_FRAME_FORCE_DISPLAY_LIST_DESCEND_INTO) {
      return;
    }
    f->AddStateBits(NS_FRAME_FORCE_DISPLAY_LIST_DESCEND_INTO);
    if (f == aStopAtFrame) {
      // we've reached a frame that we know will be painted, so we can stop.
      break;
    }
  }
}

void nsDisplayListBuilder::AddFrameMarkedForDisplayIfVisible(nsIFrame* aFrame) {
  mFramesMarkedForDisplayIfVisible.AppendElement(aFrame);
}

void nsDisplayListBuilder::MarkFrameForDisplayIfVisible(
    nsIFrame* aFrame, nsIFrame* aStopAtFrame) {
  AddFrameMarkedForDisplayIfVisible(aFrame);
  for (nsIFrame* f = aFrame; f; f = nsLayoutUtils::GetDisplayListParent(f)) {
    if (f->ForceDescendIntoIfVisible()) {
      return;
    }
    f->SetForceDescendIntoIfVisible(true);
    if (f == aStopAtFrame) {
      // we've reached a frame that we know will be painted, so we can stop.
      break;
    }
  }
}

void nsDisplayListBuilder::SetGlassDisplayItem(nsDisplayItem* aItem) {
  // Web pages or extensions could trigger the "Multiple glass backgrounds
  // found?" warning by using -moz-appearance:win-borderless-glass etc on their
  // own elements (as long as they are DocElementBoxFrames, which is rare as
  // each xul doc only gets one near the root). We only care about first one,
  // since that will be the background of the root window.

  if (IsPartialUpdate()) {
    if (aItem->Frame()->IsDocElementBoxFrame()) {
#ifdef DEBUG
      if (mHasGlassItemDuringPartial) {
        NS_WARNING("Multiple glass backgrounds found?");
      } else
#endif
          if (!mHasGlassItemDuringPartial) {
        mHasGlassItemDuringPartial = true;
        aItem->SetIsGlassItem();
      }
    }
    return;
  }

  if (aItem->Frame()->IsDocElementBoxFrame()) {
#ifdef DEBUG
    if (mGlassDisplayItem) {
      NS_WARNING("Multiple glass backgrounds found?");
    } else
#endif
        if (!mGlassDisplayItem) {
      mGlassDisplayItem = aItem;
      mGlassDisplayItem->SetIsGlassItem();
    }
  }
}

bool nsDisplayListBuilder::NeedToForceTransparentSurfaceForItem(
    nsDisplayItem* aItem) {
  return aItem == mGlassDisplayItem || aItem->ClearsBackground();
}

AnimatedGeometryRoot* nsDisplayListBuilder::WrapAGRForFrame(
    nsIFrame* aAnimatedGeometryRoot, bool aIsAsync,
    AnimatedGeometryRoot* aParent /* = nullptr */) {
  DebugOnly<bool> dummy;
  MOZ_ASSERT(IsAnimatedGeometryRoot(aAnimatedGeometryRoot, dummy) == AGR_YES);

  RefPtr<AnimatedGeometryRoot> result;
  if (!mFrameToAnimatedGeometryRootMap.Get(aAnimatedGeometryRoot, &result)) {
    MOZ_ASSERT(nsLayoutUtils::IsAncestorFrameCrossDoc(RootReferenceFrame(),
                                                      aAnimatedGeometryRoot));
    RefPtr<AnimatedGeometryRoot> parent = aParent;
    if (!parent) {
      nsIFrame* parentFrame =
          nsLayoutUtils::GetCrossDocParentFrame(aAnimatedGeometryRoot);
      if (parentFrame) {
        bool isAsync;
        nsIFrame* parentAGRFrame =
            FindAnimatedGeometryRootFrameFor(parentFrame, isAsync);
        parent = WrapAGRForFrame(parentAGRFrame, isAsync);
      }
    }
    result = AnimatedGeometryRoot::CreateAGRForFrame(
        aAnimatedGeometryRoot, parent, aIsAsync, IsRetainingDisplayList());
    mFrameToAnimatedGeometryRootMap.Put(aAnimatedGeometryRoot, result);
  }
  MOZ_ASSERT(!aParent || result->mParentAGR == aParent);
  return result;
}

AnimatedGeometryRoot* nsDisplayListBuilder::AnimatedGeometryRootForASR(
    const ActiveScrolledRoot* aASR) {
  if (!aASR) {
    return GetRootAnimatedGeometryRoot();
  }
  nsIFrame* scrolledFrame = aASR->mScrollableFrame->GetScrolledFrame();
  return FindAnimatedGeometryRootFor(scrolledFrame);
}

AnimatedGeometryRoot* nsDisplayListBuilder::FindAnimatedGeometryRootFor(
    nsIFrame* aFrame) {
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

AnimatedGeometryRoot* nsDisplayListBuilder::FindAnimatedGeometryRootFor(
    nsDisplayItem* aItem) {
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

void nsDisplayListBuilder::UpdateShouldBuildAsyncZoomContainer() {
  Document* document = mReferenceFrame->PresContext()->Document();
  mBuildAsyncZoomContainer = nsLayoutUtils::AllowZoomingForDocument(document);
}

void nsDisplayListBuilder::UpdateShouldBuildBackdropRootContainer() {
  mBuildBackdropRootContainer =
      StaticPrefs::layout_css_backdrop_filter_enabled();
}

// Certain prefs may cause display list items to be added or removed when they
// are toggled. In those cases, we need to fully rebuild the display list.
bool nsDisplayListBuilder::ShouldRebuildDisplayListDueToPrefChange() {
  bool shouldRebuild = false;

  // If we transition between wrapping the RCD-RSF contents into an async
  // zoom container vs. not, we need to rebuild the display list. This only
  // happens when the zooming or container scrolling prefs are toggled
  // (manually by the user, or during test setup).
  bool didBuildAsyncZoomContainer = mBuildAsyncZoomContainer;
  UpdateShouldBuildAsyncZoomContainer();
  if (didBuildAsyncZoomContainer != mBuildAsyncZoomContainer) {
    shouldRebuild = true;
  }

  // If backdrop-filter is enabled, backdrop root containers are added to the
  // display list. When the pref is toggled these containers may be added or
  // removed, so the display list should be rebuilt.
  bool didBuildBackdropRootContainer = mBuildBackdropRootContainer;
  UpdateShouldBuildBackdropRootContainer();
  if (didBuildBackdropRootContainer != mBuildBackdropRootContainer) {
    shouldRebuild = true;
  }

  return shouldRebuild;
}

bool nsDisplayListBuilder::MarkOutOfFlowFrameForDisplay(nsIFrame* aDirtyFrame,
                                                        nsIFrame* aFrame) {
  MOZ_ASSERT(aFrame->GetParent() == aDirtyFrame);
  nsRect dirty;
  nsRect visible = OutOfFlowDisplayData::ComputeVisibleRectForFrame(
      this, aFrame, GetVisibleRect(), GetDirtyRect(), &dirty);
  if (!(aFrame->GetStateBits() & NS_FRAME_FORCE_DISPLAY_LIST_DESCEND_INTO) &&
      visible.IsEmpty()) {
    return false;
  }

  // Only MarkFrameForDisplay if we're dirty. If this is a nested out-of-flow
  // frame, then it will also mark any outer frames to ensure that building
  // reaches the dirty feame.
  if (!dirty.IsEmpty() || aFrame->ForceDescendIntoIfVisible()) {
    MarkFrameForDisplay(aFrame, aDirtyFrame);
  }

  return true;
}

static void UnmarkFrameForDisplay(nsIFrame* aFrame, nsIFrame* aStopAtFrame) {
  for (nsIFrame* f = aFrame; f;
       f = nsLayoutUtils::GetParentOrPlaceholderForCrossDoc(f)) {
    if (!(f->GetStateBits() & NS_FRAME_FORCE_DISPLAY_LIST_DESCEND_INTO)) {
      return;
    }
    f->RemoveStateBits(NS_FRAME_FORCE_DISPLAY_LIST_DESCEND_INTO);
    if (f == aStopAtFrame) {
      // we've reached a frame that we know will be painted, so we can stop.
      break;
    }
  }
}

static void UnmarkFrameForDisplayIfVisible(nsIFrame* aFrame) {
  for (nsIFrame* f = aFrame; f; f = nsLayoutUtils::GetDisplayListParent(f)) {
    if (!f->ForceDescendIntoIfVisible()) {
      return;
    }
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

  DisplayItemClipChain* c = mFirstClipChainToDestroy;
  while (c) {
    DisplayItemClipChain* next = c->mNextClipChainToDestroy;
    c->DisplayItemClipChain::~DisplayItemClipChain();
    c = next;
  }

  MOZ_COUNT_DTOR(nsDisplayListBuilder);
}

uint32_t nsDisplayListBuilder::GetBackgroundPaintFlags() {
  uint32_t flags = 0;
  if (mSyncDecodeImages) {
    flags |= nsCSSRendering::PAINTBG_SYNC_DECODE_IMAGES;
  }
  if (mIsPaintingToWindow) {
    flags |= nsCSSRendering::PAINTBG_TO_WINDOW;
  }
  return flags;
}

uint32_t nsDisplayListBuilder::GetImageDecodeFlags() const {
  uint32_t flags = imgIContainer::FLAG_ASYNC_NOTIFY;
  if (mSyncDecodeImages) {
    flags |= imgIContainer::FLAG_SYNC_DECODE;
  } else {
    flags |= imgIContainer::FLAG_SYNC_DECODE_IF_FAST;
  }
  if (mIsPaintingToWindow) {
    flags |= imgIContainer::FLAG_HIGH_QUALITY_SCALING;
  }
  return flags;
}

void nsDisplayListBuilder::ComputeDefaultRenderRootRect(
    LayoutDeviceIntSize aClientSize) {
  LayoutDeviceIntRegion cutout;
  LayoutDeviceIntRect clientRect(LayoutDeviceIntPoint(), aClientSize);
  cutout.OrWith(clientRect);

  mRenderRootRects[mozilla::wr::RenderRoot::Default] =
      LayoutDeviceRect(cutout.GetBounds());
}

void nsDisplayListBuilder::SubtractFromVisibleRegion(nsRegion* aVisibleRegion,
                                                     const nsRegion& aRegion) {
  if (aRegion.IsEmpty()) {
    return;
  }

  nsRegion tmp;
  tmp.Sub(*aVisibleRegion, aRegion);
  // Don't let *aVisibleRegion get too complex, but don't let it fluff out
  // to its bounds either, which can be very bad (see bug 516740).
  // Do let aVisibleRegion get more complex if by doing so we reduce its
  // area by at least half.
  if (GetAccurateVisibleRegions() || tmp.GetNumRects() <= 15 ||
      tmp.Area() <= aVisibleRegion->Area() / 2) {
    *aVisibleRegion = tmp;
  }
}

nsCaret* nsDisplayListBuilder::GetCaret() {
  RefPtr<nsCaret> caret = CurrentPresShellState()->mPresShell->GetCaret();
  return caret;
}

void nsDisplayListBuilder::IncrementPresShellPaintCount(PresShell* aPresShell) {
  if (mIsPaintingToWindow) {
    mReferenceFrame->AddPaintedPresShell(aPresShell);
    aPresShell->IncrementPaintCount();
  }
}

void nsDisplayListBuilder::EnterPresShell(nsIFrame* aReferenceFrame,
                                          bool aPointerEventsNoneDoc) {
  PresShellState* state = mPresShellStates.AppendElement();
  state->mPresShell = aReferenceFrame->PresShell();
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
  state->mAutoLayoutPhase.emplace(aReferenceFrame->PresContext(),
                                  nsLayoutPhase::DisplayListBuilding);
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
    pointerEventsNone |= mPresShellStates[mPresShellStates.Length() - 2]
                             .mInsidePointerEventsNoneDoc;
  }
  state->mInsidePointerEventsNoneDoc = pointerEventsNone;

  state->mPresShellIgnoreScrollFrame =
      state->mPresShell->IgnoringViewportScrolling()
          ? state->mPresShell->GetRootScrollFrame()
          : nullptr;

  nsPresContext* pc = aReferenceFrame->PresContext();
  mIsInChromePresContext = pc->IsChrome();
  nsIDocShell* docShell = pc->GetDocShell();

  if (docShell) {
    docShell->GetWindowDraggingAllowed(&mWindowDraggingAllowed);
  }

  state->mTouchEventPrefEnabledDoc = dom::TouchEvent::PrefEnabled(docShell);

  if (!buildCaret) {
    return;
  }

  // Caret frames add visual area to their frame, but we don't update the
  // overflow area. Use flags to make sure we build display items for that frame
  // instead.
  if (mCaretFrame && mCaretFrame->PresShell() == state->mPresShell) {
    MarkFrameForDisplay(mCaretFrame, aReferenceFrame);
  }
}

// A non-blank paint is a paint that does not just contain the canvas
// background.
static bool DisplayListIsNonBlank(nsDisplayList* aList) {
  for (nsDisplayItem* i : *aList) {
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

// A contentful paint is a paint that does contains DOM content (text,
// images, non-blank canvases, SVG): "First Contentful Paint entry
// contains a DOMHighResTimeStamp reporting the time when the browser
// first rendered any text, image (including background images),
// non-white canvas or SVG. This excludes any content of iframes, but
// includes text with pending webfonts. This is the first time users
// could start consuming page content."
static bool DisplayListIsContentful(nsDisplayList* aList) {
  for (nsDisplayItem* i : *aList) {
    DisplayItemType type = i->GetType();
    nsDisplayList* children = i->GetChildren();

    switch (type) {
      case DisplayItemType::TYPE_SUBDOCUMENT:  // iframes are ignored
        break;
      // CANVASes check if they may have been modified (as a stand-in
      // actually tracking all modifications)
      default:
        if (i->IsContentful()) {
          return true;
        }
        if (children) {
          if (DisplayListIsContentful(children)) {
            return true;
          }
        }
        break;
    }
  }
  return false;
}

void nsDisplayListBuilder::LeavePresShell(nsIFrame* aReferenceFrame,
                                          nsDisplayList* aPaintedContents) {
  NS_ASSERTION(
      CurrentPresShellState()->mPresShell == aReferenceFrame->PresShell(),
      "Presshell mismatch");

  if (mIsPaintingToWindow && aPaintedContents) {
    nsPresContext* pc = aReferenceFrame->PresContext();
    if (!pc->HadNonBlankPaint()) {
      if (!CurrentPresShellState()->mIsBackgroundOnly &&
          DisplayListIsNonBlank(aPaintedContents)) {
        pc->NotifyNonBlankPaint();
      }
    }
    if (!pc->HadContentfulPaint()) {
      if (!CurrentPresShellState()->mIsBackgroundOnly &&
          DisplayListIsContentful(aPaintedContents)) {
        pc->NotifyContentfulPaint();
      }
    }
  }

  ResetMarkedFramesForDisplayList(aReferenceFrame);
  mPresShellStates.RemoveLastElement();

  if (!mPresShellStates.IsEmpty()) {
    nsPresContext* pc = CurrentPresContext();
    nsIDocShell* docShell = pc->GetDocShell();
    if (docShell) {
      docShell->GetWindowDraggingAllowed(&mWindowDraggingAllowed);
    }
    mIsInChromePresContext = pc->IsChrome();
  } else {
    mCurrentAGR = mRootAGR;

    for (uint32_t i = 0; i < mFramesMarkedForDisplayIfVisible.Length(); ++i) {
      UnmarkFrameForDisplayIfVisible(mFramesMarkedForDisplayIfVisible[i]);
    }
    mFramesMarkedForDisplayIfVisible.SetLength(0);
  }
}

void nsDisplayListBuilder::FreeClipChains() {
  // Iterate the clip chains from newest to oldest (forward
  // iteration), so that we destroy descendants first which
  // will drop the ref count on their ancestors.
  DisplayItemClipChain** indirect = &mFirstClipChainToDestroy;

  while (*indirect) {
    if (!(*indirect)->mRefCount) {
      DisplayItemClipChain* next = (*indirect)->mNextClipChainToDestroy;

      mClipDeduplicator.erase(*indirect);
      (*indirect)->DisplayItemClipChain::~DisplayItemClipChain();
      Destroy(DisplayListArenaObjectId::CLIPCHAIN, *indirect);

      *indirect = next;
    } else {
      indirect = &(*indirect)->mNextClipChainToDestroy;
    }
  }
}

void nsDisplayListBuilder::FreeTemporaryItems() {
  for (nsDisplayItem* i : mTemporaryItems) {
    // Temporary display items are not added to the frames.
    MOZ_ASSERT(i->Frame());
    i->RemoveFrame(i->Frame());
    i->Destroy(this);
  }

  mTemporaryItems.Clear();
}

void nsDisplayListBuilder::ResetMarkedFramesForDisplayList(
    nsIFrame* aReferenceFrame) {
  // Unmark and pop off the frames marked for display in this pres shell.
  uint32_t firstFrameForShell =
      CurrentPresShellState()->mFirstFrameMarkedForDisplay;
  for (uint32_t i = firstFrameForShell; i < mFramesMarkedForDisplay.Length();
       ++i) {
    UnmarkFrameForDisplay(mFramesMarkedForDisplay[i], aReferenceFrame);
  }
  mFramesMarkedForDisplay.SetLength(firstFrameForShell);

  firstFrameForShell = CurrentPresShellState()->mFirstFrameWithOOFData;
  for (uint32_t i = firstFrameForShell; i < mFramesWithOOFData.Length(); ++i) {
    mFramesWithOOFData[i]->RemoveProperty(OutOfFlowDisplayDataProperty());
  }
  mFramesWithOOFData.SetLength(firstFrameForShell);
}

void nsDisplayListBuilder::ClearFixedBackgroundDisplayData() {
  CurrentPresShellState()->mFixedBackgroundDisplayData = Nothing();
}

void nsDisplayListBuilder::MarkFramesForDisplayList(
    nsIFrame* aDirtyFrame, const nsFrameList& aFrames) {
  bool markedFrames = false;
  for (nsIFrame* e : aFrames) {
    // Skip the AccessibleCaret frame when building no caret.
    if (!IsBuildingCaret()) {
      nsIContent* content = e->GetContent();
      if (content && content->IsInNativeAnonymousSubtree() &&
          content->IsElement()) {
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
    const DisplayItemClipChain* combinedClipChain =
        mClipState.GetCurrentCombinedClipChain(this);
    const ActiveScrolledRoot* asr = mCurrentActiveScrolledRoot;
    OutOfFlowDisplayData* data = new OutOfFlowDisplayData(
        clipChain, combinedClipChain, asr, GetVisibleRect(), GetDirtyRect());
    aDirtyFrame->SetProperty(
        nsDisplayListBuilder::OutOfFlowDisplayDataProperty(), data);
    mFramesWithOOFData.AppendElement(aDirtyFrame);
  }

  if (!aDirtyFrame->GetParent()) {
    // This is the viewport frame of aDirtyFrame's presshell.
    // Store the current display data so that it can be used for fixed
    // background images.
    NS_ASSERTION(
        CurrentPresShellState()->mPresShell == aDirtyFrame->PresShell(),
        "Presshell mismatch");
    MOZ_ASSERT(!CurrentPresShellState()->mFixedBackgroundDisplayData,
               "already traversed this presshell's root frame?");

    const DisplayItemClipChain* clipChain =
        CopyWholeChain(mClipState.GetClipChainForContainingBlockDescendants());
    const DisplayItemClipChain* combinedClipChain =
        mClipState.GetCurrentCombinedClipChain(this);
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
void nsDisplayListBuilder::MarkPreserve3DFramesForDisplayList(
    nsIFrame* aDirtyFrame) {
  AutoTArray<nsIFrame::ChildList, 4> childListArray;
  aDirtyFrame->GetChildLists(&childListArray);
  nsIFrame::ChildListArrayIterator lists(childListArray);
  for (; !lists.IsDone(); lists.Next()) {
    nsFrameList::Enumerator childFrames(lists.CurrentList());
    for (; !childFrames.AtEnd(); childFrames.Next()) {
      nsIFrame* child = childFrames.get();
      if (child->Combines3DTransformWithAncestors()) {
        MarkFrameForDisplay(child, aDirtyFrame);
      }
    }
  }
}

ActiveScrolledRoot* nsDisplayListBuilder::AllocateActiveScrolledRoot(
    const ActiveScrolledRoot* aParent, nsIScrollableFrame* aScrollableFrame) {
  RefPtr<ActiveScrolledRoot> asr = ActiveScrolledRoot::CreateASRForFrame(
      aParent, aScrollableFrame, IsRetainingDisplayList());
  mActiveScrolledRoots.AppendElement(asr);
  return asr;
}

const DisplayItemClipChain* nsDisplayListBuilder::AllocateDisplayItemClipChain(
    const DisplayItemClip& aClip, const ActiveScrolledRoot* aASR,
    const DisplayItemClipChain* aParent) {
  MOZ_ASSERT(!(aParent && aParent->mOnStack));
  void* p = Allocate(sizeof(DisplayItemClipChain),
                     DisplayListArenaObjectId::CLIPCHAIN);
  DisplayItemClipChain* c = new (KnownNotNull, p)
      DisplayItemClipChain(aClip, aASR, aParent, mFirstClipChainToDestroy);
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
    Destroy(DisplayListArenaObjectId::CLIPCHAIN, c);
    return *(result.first);
  }
  mFirstClipChainToDestroy = c;
  return c;
}

struct ClipChainItem {
  DisplayItemClip clip;
  const ActiveScrolledRoot* asr;
};

const DisplayItemClipChain* nsDisplayListBuilder::CreateClipChainIntersection(
    const DisplayItemClipChain* aAncestor,
    const DisplayItemClipChain* aLeafClip1,
    const DisplayItemClipChain* aLeafClip2) {
  AutoTArray<ClipChainItem, 8> intersectedClips;

  const DisplayItemClipChain* clip1 = aLeafClip1;
  const DisplayItemClipChain* clip2 = aLeafClip2;

  const ActiveScrolledRoot* asr = ActiveScrolledRoot::PickDescendant(
      clip1 ? clip1->mASR : nullptr, clip2 ? clip2->mASR : nullptr);

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
        intersectedClips.AppendElement(ClipChainItem{intersection, asr});
        clip2 = clip2->mParent;
      } else {
        intersectedClips.AppendElement(ClipChainItem{clip1->mClip, asr});
      }
      clip1 = clip1->mParent;
    } else if (clip2 && clip2->mASR == asr) {
      intersectedClips.AppendElement(ClipChainItem{clip2->mClip, asr});
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

const DisplayItemClipChain* nsDisplayListBuilder::CopyWholeChain(
    const DisplayItemClipChain* aClipChain) {
  return CreateClipChainIntersection(nullptr, aClipChain, nullptr);
}

const DisplayItemClipChain* nsDisplayListBuilder::FuseClipChainUpTo(
    const DisplayItemClipChain* aClipChain, const ActiveScrolledRoot* aASR) {
  if (!aClipChain) {
    return nullptr;
  }

  const DisplayItemClipChain* sc = aClipChain;
  DisplayItemClip mergedClip;
  while (sc && ActiveScrolledRoot::PickDescendant(aASR, sc->mASR) == sc->mASR) {
    mergedClip.IntersectWith(sc->mClip);
    sc = sc->mParent;
  }

  if (!mergedClip.HasClip()) {
    return nullptr;
  }

  return AllocateDisplayItemClipChain(mergedClip, aASR, sc);
}

const nsIFrame* nsDisplayListBuilder::FindReferenceFrameFor(
    const nsIFrame* aFrame, nsPoint* aOffset) const {
  if (aFrame == mCurrentFrame) {
    if (aOffset) {
      *aOffset = mCurrentOffsetToReferenceFrame;
    }
    return mCurrentReferenceFrame;
  }
  for (const nsIFrame* f = aFrame; f;
       f = nsLayoutUtils::GetCrossDocParentFrame(f)) {
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
static bool IsStickyFrameActive(nsDisplayListBuilder* aBuilder,
                                nsIFrame* aFrame, nsIFrame* aParent) {
  MOZ_ASSERT(aFrame->StyleDisplay()->mPosition ==
             StylePositionProperty::Sticky);

  // Find the nearest scrollframe.
  nsIScrollableFrame* sf = nsLayoutUtils::GetNearestScrollableFrame(
      aFrame->GetParent(), nsLayoutUtils::SCROLLABLE_SAME_DOC |
                               nsLayoutUtils::SCROLLABLE_INCLUDE_HIDDEN);
  if (!sf) {
    return false;
  }

  return sf->IsScrollingActive(aBuilder);
}

nsDisplayListBuilder::AGRState nsDisplayListBuilder::IsAnimatedGeometryRoot(
    nsIFrame* aFrame, bool& aIsAsync, nsIFrame** aParent) {
  // We can return once we know that this frame is an AGR, and we're either
  // async, or sure that none of the later conditions might make us async.
  // The exception to this is when IsPaintingToWindow() == false.
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

  if (aFrame->StyleDisplay()->mPosition == StylePositionProperty::Sticky &&
      IsStickyFrameActive(this, aFrame, parent)) {
    aIsAsync = true;
    return AGR_YES;
  }

  if (aFrame->IsTransformed()) {
    aIsAsync = EffectCompositor::HasAnimationsForCompositor(
        aFrame, DisplayItemType::TYPE_TRANSFORM);
    return AGR_YES;
  }

  LayoutFrameType parentType = parent->Type();
  if (parentType == LayoutFrameType::Scroll ||
      parentType == LayoutFrameType::ListControl) {
    nsIScrollableFrame* sf = do_QueryFrame(parent);
    if (sf->GetScrolledFrame() == aFrame && sf->IsScrollingActive(this)) {
      MOZ_ASSERT(!aFrame->IsTransformed());
      aIsAsync = sf->IsMaybeAsynchronouslyScrolled();
      return AGR_YES;
    }
  }

  // Treat the slider thumb as being as an active scrolled root when it wants
  // its own layer so that it can move without repainting.
  if (parentType == LayoutFrameType::Slider) {
    auto* sf = static_cast<nsSliderFrame*>(parent)->GetScrollFrame();
    // The word "Maybe" in IsMaybeScrollingActive might be confusing but we do
    // indeed need to always consider scroll thumbs as AGRs if
    // IsMaybeScrollingActive is true because that is the same condition we use
    // in ScrollFrameHelper::AppendScrollPartsTo to layerize scroll thumbs.
    if (sf && sf->IsMaybeScrollingActive()) {
      return AGR_YES;
    }
  }

  if (nsLayoutUtils::IsPopup(aFrame)) {
    return AGR_YES;
  }

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

  // Fixed-pos frames are parented by the viewport frame, which has no parent.
  if (nsLayoutUtils::IsFixedPosFrameInDisplayPort(aFrame)) {
    return AGR_YES;
  }

  if (aParent) {
    *aParent = parent;
  }

  return AGR_NO;
}

nsIFrame* nsDisplayListBuilder::FindAnimatedGeometryRootFrameFor(
    nsIFrame* aFrame, bool& aIsAsync) {
  MOZ_ASSERT(
      nsLayoutUtils::IsAncestorFrameCrossDoc(RootReferenceFrame(), aFrame));
  nsIFrame* cursor = aFrame;
  while (cursor != RootReferenceFrame()) {
    nsIFrame* next;
    if (IsAnimatedGeometryRoot(cursor, aIsAsync, &next) == AGR_YES) {
      return cursor;
    }
    cursor = next;
  }
  // Root frame is always an async agr.
  aIsAsync = true;
  return cursor;
}

void nsDisplayListBuilder::RecomputeCurrentAnimatedGeometryRoot() {
  bool isAsync;
  if (*mCurrentAGR != mCurrentFrame &&
      IsAnimatedGeometryRoot(const_cast<nsIFrame*>(mCurrentFrame), isAsync) ==
          AGR_YES) {
    AnimatedGeometryRoot* oldAGR = mCurrentAGR;
    mCurrentAGR = WrapAGRForFrame(const_cast<nsIFrame*>(mCurrentFrame), isAsync,
                                  mCurrentAGR);

    // Iterate the AGR cache and look for any objects that reference the old AGR
    // and check to see if they need to be updated. AGRs can be in the cache
    // multiple times, so we may end up doing the work multiple times for AGRs
    // that don't change.
    for (auto iter = mFrameToAnimatedGeometryRootMap.Iter(); !iter.Done();
         iter.Next()) {
      RefPtr<AnimatedGeometryRoot> cached = iter.UserData();
      if (cached->mParentAGR == oldAGR && cached != mCurrentAGR) {
        // It's possible that this cached AGR struct that has the old AGR as a
        // parent should instead have mCurrentFrame has a parent.
        nsIFrame* parent = FindAnimatedGeometryRootFrameFor(*cached, isAsync);
        MOZ_ASSERT(parent == mCurrentFrame || parent == *oldAGR);
        if (parent == mCurrentFrame) {
          cached->mParentAGR = mCurrentAGR;
        }
      }
    }
  }
}

static nsRect ApplyAllClipNonRoundedIntersection(
    const DisplayItemClipChain* aClipChain, const nsRect& aRect) {
  nsRect result = aRect;
  while (aClipChain) {
    result = aClipChain->mClip.ApplyNonRoundedIntersection(result);
    aClipChain = aClipChain->mParent;
  }
  return result;
}

void nsDisplayListBuilder::AdjustWindowDraggingRegion(nsIFrame* aFrame) {
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
  nsIFrame* referenceFrame =
      const_cast<nsIFrame*>(FindReferenceFrameFor(aFrame));

  if (IsInTransform()) {
    // Only support 2d rectilinear transforms. Transform support is needed for
    // the horizontal flip transform that's applied to the urlbar textbox in
    // RTL mode - it should be able to exclude itself from the draggable region.
    referenceFrameToRootReferenceFrame =
        ViewAs<LayoutDeviceToLayoutDeviceMatrix4x4>(
            nsLayoutUtils::GetTransformToAncestor(referenceFrame,
                                                  mReferenceFrame)
                .GetMatrix());
    Matrix referenceFrameToRootReferenceFrame2d;
    if (!referenceFrameToRootReferenceFrame.Is2D(
            &referenceFrameToRootReferenceFrame2d) ||
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
  const DisplayItemClipChain* clip =
      ClipState().GetCurrentCombinedClipChain(this);
  borderBox = ApplyAllClipNonRoundedIntersection(clip, borderBox);
  if (borderBox.IsEmpty()) {
    return;
  }

  LayoutDeviceRect devPixelBorderBox = LayoutDevicePixel::FromAppUnits(
      borderBox, aFrame->PresContext()->AppUnitsPerDevPixel());

  LayoutDeviceRect transformedDevPixelBorderBox =
      TransformBy(referenceFrameToRootReferenceFrame, devPixelBorderBox);
  transformedDevPixelBorderBox.Round();
  LayoutDeviceIntRect transformedDevPixelBorderBoxInt;

  if (!transformedDevPixelBorderBox.ToIntRect(
          &transformedDevPixelBorderBoxInt)) {
    return;
  }

  LayoutDeviceIntRegion& region =
      styleUI->mWindowDragging == StyleWindowDragging::Drag
          ? mWindowDraggingRegion
          : mWindowNoDraggingRegion;

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

LayoutDeviceIntRegion nsDisplayListBuilder::GetWindowDraggingRegion() const {
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

void nsDisplayHitTestInfoBase::AddSizeOfExcludingThis(
    nsWindowSizes& aSizes) const {
  nsPaintedDisplayItem::AddSizeOfExcludingThis(aSizes);
  aSizes.mLayoutRetainedDisplayListSize +=
      aSizes.mState.mMallocSizeOf(mHitTestInfo.get());
}

void nsDisplayTransform::AddSizeOfExcludingThis(nsWindowSizes& aSizes) const {
  nsDisplayHitTestInfoBase::AddSizeOfExcludingThis(aSizes);
  aSizes.mLayoutRetainedDisplayListSize +=
      aSizes.mState.mMallocSizeOf(mTransformPreserves3D.get());
}

void nsDisplayListBuilder::AddSizeOfExcludingThis(nsWindowSizes& aSizes) const {
  mPool.AddSizeOfExcludingThis(aSizes, Arena::ArenaKind::DisplayList);

  size_t n = 0;
  MallocSizeOf mallocSizeOf = aSizes.mState.mMallocSizeOf;
  n += mDocumentWillChangeBudgets.ShallowSizeOfExcludingThis(mallocSizeOf);
  n += mFrameWillChangeBudgets.ShallowSizeOfExcludingThis(mallocSizeOf);
  n += mAGRBudgetSet.ShallowSizeOfExcludingThis(mallocSizeOf);
  n += mEffectsUpdates.ShallowSizeOfExcludingThis(mallocSizeOf);
  n += mWindowExcludeGlassRegion.SizeOfExcludingThis(mallocSizeOf);
  n += mRetainedWindowDraggingRegion.SizeOfExcludingThis(mallocSizeOf);
  n += mRetainedWindowNoDraggingRegion.SizeOfExcludingThis(mallocSizeOf);
  n += mRetainedWindowOpaqueRegion.SizeOfExcludingThis(mallocSizeOf);
  // XXX can't measure mClipDeduplicator since it uses std::unordered_set.

  aSizes.mLayoutRetainedDisplayListSize += n;
}

void RetainedDisplayList::AddSizeOfExcludingThis(nsWindowSizes& aSizes) const {
  for (nsDisplayItem* item : *this) {
    item->AddSizeOfExcludingThis(aSizes);
    if (RetainedDisplayList* children = item->GetChildren()) {
      children->AddSizeOfExcludingThis(aSizes);
    }
  }

  size_t n = 0;

  n += mDAG.mDirectPredecessorList.ShallowSizeOfExcludingThis(
      aSizes.mState.mMallocSizeOf);
  n += mDAG.mNodesInfo.ShallowSizeOfExcludingThis(aSizes.mState.mMallocSizeOf);
  n += mOldItems.ShallowSizeOfExcludingThis(aSizes.mState.mMallocSizeOf);

  aSizes.mLayoutRetainedDisplayListSize += n;
}

size_t nsDisplayListBuilder::WeakFrameRegion::SizeOfExcludingThis(
    MallocSizeOf aMallocSizeOf) const {
  size_t n = 0;
  n += mFrames.ShallowSizeOfExcludingThis(aMallocSizeOf);
  for (auto& frame : mFrames) {
    const UniquePtr<WeakFrame>& weakFrame = frame.mWeakFrame;
    n += aMallocSizeOf(weakFrame.get());
  }
  n += mRects.ShallowSizeOfExcludingThis(aMallocSizeOf);
  return n;
}

/**
 * Removes modified frames and rects from this WeakFrameRegion.
 */
void nsDisplayListBuilder::WeakFrameRegion::RemoveModifiedFramesAndRects() {
  MOZ_ASSERT(mFrames.Length() == mRects.Length());

  uint32_t i = 0;
  uint32_t length = mFrames.Length();

  while (i < length) {
    auto& wrapper = mFrames[i];

    if (!wrapper.mWeakFrame->IsAlive() ||
        AnyContentAncestorModified(wrapper.mWeakFrame->GetFrame())) {
      // To avoid multiple O(n) shifts in the array, move the last element of
      // the array to the current position and decrease the array length.
      mFrameSet.RemoveEntry(wrapper.mFrame);
      mFrames[i] = std::move(mFrames[length - 1]);
      mRects[i] = std::move(mRects[length - 1]);
      length--;
    } else {
      i++;
    }
  }

  mFrames.TruncateLength(length);
  mRects.TruncateLength(length);
}

void nsDisplayListBuilder::RemoveModifiedWindowRegions() {
  mRetainedWindowDraggingRegion.RemoveModifiedFramesAndRects();
  mRetainedWindowNoDraggingRegion.RemoveModifiedFramesAndRects();
  mWindowExcludeGlassRegion.RemoveModifiedFramesAndRects();
  mRetainedWindowOpaqueRegion.RemoveModifiedFramesAndRects();

  mHasGlassItemDuringPartial = false;
}

void nsDisplayListBuilder::ClearRetainedWindowRegions() {
  mRetainedWindowDraggingRegion.Clear();
  mRetainedWindowNoDraggingRegion.Clear();
  mWindowExcludeGlassRegion.Clear();
  mRetainedWindowOpaqueRegion.Clear();

  mGlassDisplayItem = nullptr;
}

const uint32_t gWillChangeAreaMultiplier = 3;
static uint32_t GetLayerizationCost(const nsSize& aSize) {
  // There's significant overhead for each layer created from Gecko
  // (IPC+Shared Objects) and from the backend (like an OpenGL texture).
  // Therefore we set a minimum cost threshold of a 64x64 area.
  const int minBudgetCost = 64 * 64;

  const uint32_t budgetCost = std::max(
      minBudgetCost, nsPresContext::AppUnitsToIntCSSPixels(aSize.width) *
                         nsPresContext::AppUnitsToIntCSSPixels(aSize.height));

  return budgetCost;
}

bool nsDisplayListBuilder::AddToWillChangeBudget(nsIFrame* aFrame,
                                                 const nsSize& aSize) {
  MOZ_ASSERT(IsForPainting());

  if (aFrame->MayHaveWillChangeBudget()) {
    // The frame is already in the will-change budget.
    return true;
  }

  const nsPresContext* presContext = aFrame->PresContext();
  const nsRect area = presContext->GetVisibleArea();
  const uint32_t budgetLimit =
      nsPresContext::AppUnitsToIntCSSPixels(area.width) *
      nsPresContext::AppUnitsToIntCSSPixels(area.height);
  const uint32_t cost = GetLayerizationCost(aSize);

  DocumentWillChangeBudget& documentBudget =
      mDocumentWillChangeBudgets.GetOrInsert(presContext);

  const bool onBudget =
      (documentBudget + cost) / gWillChangeAreaMultiplier < budgetLimit;

  if (onBudget) {
    documentBudget += cost;
    mFrameWillChangeBudgets.Put(aFrame,
                                FrameWillChangeBudget(presContext, cost));
    aFrame->SetMayHaveWillChangeBudget(true);
  }

  return onBudget;
}

bool nsDisplayListBuilder::IsInWillChangeBudget(nsIFrame* aFrame,
                                                const nsSize& aSize) {
  if (!IsForPainting()) {
    // If this nsDisplayListBuilder is not for painting, the layerization should
    // not matter. Do the simple thing and return false.
    return false;
  }

  const bool onBudget = AddToWillChangeBudget(aFrame, aSize);
  if (onBudget) {
    return true;
  }

  auto* pc = aFrame->PresContext();
  auto* doc = pc->Document();
  if (!doc->HasWarnedAbout(Document::eIgnoringWillChangeOverBudget)) {
    AutoTArray<nsString, 2> params;
    params.AppendElement()->AppendInt(gWillChangeAreaMultiplier);

    nsRect area = pc->GetVisibleArea();
    uint32_t budgetLimit = nsPresContext::AppUnitsToIntCSSPixels(area.width) *
                           nsPresContext::AppUnitsToIntCSSPixels(area.height);
    params.AppendElement()->AppendInt(budgetLimit);

    doc->WarnOnceAbout(Document::eIgnoringWillChangeOverBudget, false, params);
  }

  return false;
}

void nsDisplayListBuilder::ClearWillChangeBudgetStatus(nsIFrame* aFrame) {
  MOZ_ASSERT(IsForPainting());

  if (!aFrame->MayHaveWillChangeBudget()) {
    return;
  }

  aFrame->SetMayHaveWillChangeBudget(false);
  RemoveFromWillChangeBudgets(aFrame);
}

void nsDisplayListBuilder::RemoveFromWillChangeBudgets(const nsIFrame* aFrame) {
  if (auto entry = mFrameWillChangeBudgets.Lookup(aFrame)) {
    const FrameWillChangeBudget& frameBudget = entry.Data();

    DocumentWillChangeBudget* documentBudget =
        mDocumentWillChangeBudgets.GetValue(frameBudget.mPresContext);

    if (documentBudget) {
      *documentBudget -= frameBudget.mUsage;
    }

    entry.Remove();
  }
}

void nsDisplayListBuilder::ClearWillChangeBudgets() {
  mFrameWillChangeBudgets.Clear();
  mDocumentWillChangeBudgets.Clear();
}

#ifdef MOZ_GFX_OPTIMIZE_MOBILE
const float gAGRBudgetAreaMultiplier = 0.3;
#else
const float gAGRBudgetAreaMultiplier = 3.0;
#endif

bool nsDisplayListBuilder::AddToAGRBudget(nsIFrame* aFrame) {
  if (mAGRBudgetSet.Contains(aFrame)) {
    return true;
  }

  const nsPresContext* presContext =
      aFrame->PresContext()->GetRootPresContext();
  if (!presContext) {
    return false;
  }

  const nsRect area = presContext->GetVisibleArea();
  const uint32_t budgetLimit =
      gAGRBudgetAreaMultiplier *
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

void nsDisplayListBuilder::EnterSVGEffectsContents(
    nsIFrame* aEffectsFrame, nsDisplayList* aHoistedItemsStorage) {
  MOZ_ASSERT(aHoistedItemsStorage);
  if (mSVGEffectsFrames.IsEmpty()) {
    MOZ_ASSERT(!mScrollInfoItemsForHoisting);
    mScrollInfoItemsForHoisting = aHoistedItemsStorage;
  }
  mSVGEffectsFrames.AppendElement(aEffectsFrame);
}

void nsDisplayListBuilder::ExitSVGEffectsContents() {
  MOZ_ASSERT(!mSVGEffectsFrames.IsEmpty());
  mSVGEffectsFrames.RemoveLastElement();
  MOZ_ASSERT(mScrollInfoItemsForHoisting);
  if (mSVGEffectsFrames.IsEmpty()) {
    mScrollInfoItemsForHoisting = nullptr;
  }
}

bool nsDisplayListBuilder::ShouldBuildScrollInfoItemsForHoisting() const {
  /*
   * Note: if changing the conditions under which scroll info layers
   * are created, make a corresponding change to
   * ScrollFrameWillBuildScrollInfoLayer() in nsSliderFrame.cpp.
   */
  for (nsIFrame* frame : mSVGEffectsFrames) {
    if (nsSVGIntegrationUtils::UsesSVGEffectsNotSupportedInCompositor(frame)) {
      return true;
    }
  }
  return false;
}

void nsDisplayListBuilder::AppendNewScrollInfoItemForHoisting(
    nsDisplayScrollInfoLayer* aScrollInfoItem) {
  MOZ_ASSERT(ShouldBuildScrollInfoItemsForHoisting());
  MOZ_ASSERT(mScrollInfoItemsForHoisting);
  mScrollInfoItemsForHoisting->AppendToTop(aScrollInfoItem);
}

void nsDisplayListBuilder::BuildCompositorHitTestInfoIfNeeded(
    nsIFrame* aFrame, nsDisplayList* aList, const bool aBuildNew) {
  MOZ_ASSERT(aFrame);
  MOZ_ASSERT(aList);

  if (!BuildCompositorHitTestInfo()) {
    return;
  }

  const CompositorHitTestInfo info = aFrame->GetCompositorHitTestInfo(this);
  if (info == CompositorHitTestInvisibleToHit) {
    return;
  }

  const nsRect area = aFrame->GetCompositorHitTestArea(this);
  if (!aBuildNew && GetHitTestInfo() == info &&
      GetHitTestArea().Contains(area)) {
    return;
  }

  auto* item = MakeDisplayItem<nsDisplayCompositorHitTestInfo>(
      this, aFrame, info, 0, Some(area));
  MOZ_ASSERT(item);

  SetCompositorHitTestInfo(area, info);
  aList->AppendToTop(item);
}

void nsDisplayListSet::MoveTo(const nsDisplayListSet& aDestination) const {
  aDestination.BorderBackground()->AppendToTop(BorderBackground());
  aDestination.BlockBorderBackgrounds()->AppendToTop(BlockBorderBackgrounds());
  aDestination.Floats()->AppendToTop(Floats());
  aDestination.Content()->AppendToTop(Content());
  aDestination.PositionedDescendants()->AppendToTop(PositionedDescendants());
  aDestination.Outlines()->AppendToTop(Outlines());
}

static void MoveListTo(nsDisplayList* aList,
                       nsTArray<nsDisplayItem*>* aElements) {
  nsDisplayItem* item;
  while ((item = aList->RemoveBottom()) != nullptr) {
    aElements->AppendElement(item);
  }
}

nsRect nsDisplayList::GetClippedBounds(nsDisplayListBuilder* aBuilder) const {
  nsRect bounds;
  for (nsDisplayItem* i : *this) {
    bounds.UnionRect(bounds, i->GetClippedBounds(aBuilder));
  }
  return bounds;
}

nsRect nsDisplayList::GetClippedBoundsWithRespectToASR(
    nsDisplayListBuilder* aBuilder, const ActiveScrolledRoot* aASR,
    nsRect* aBuildingRect) const {
  nsRect bounds;
  for (nsDisplayItem* i : *this) {
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

nsRect nsDisplayList::GetBuildingRect() const {
  nsRect result;
  for (nsDisplayItem* i : *this) {
    result.UnionRect(result, i->GetBuildingRect());
  }
  return result;
}

bool nsDisplayList::ComputeVisibilityForRoot(nsDisplayListBuilder* aBuilder,
                                             nsRegion* aVisibleRegion) {
  AUTO_PROFILER_LABEL("nsDisplayList::ComputeVisibilityForRoot", GRAPHICS);

  nsRegion r;
  const ActiveScrolledRoot* rootASR = nullptr;
  r.And(*aVisibleRegion, GetClippedBoundsWithRespectToASR(aBuilder, rootASR));
  return ComputeVisibilityForSublist(aBuilder, aVisibleRegion, r.GetBounds());
}

static nsRegion TreatAsOpaque(nsDisplayItem* aItem,
                              nsDisplayListBuilder* aBuilder) {
  bool snap;
  nsRegion opaque = aItem->GetOpaqueRegion(aBuilder, &snap);
  MOZ_ASSERT(
      (aBuilder->IsForEventDelivery() && aBuilder->HitTestIsForVisibility()) ||
      !opaque.IsComplex());
  if (aBuilder->IsForPluginGeometry() &&
      aItem->GetType() != DisplayItemType::TYPE_COMPOSITOR_HITTEST_INFO) {
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

bool nsDisplayList::ComputeVisibilityForSublist(
    nsDisplayListBuilder* aBuilder, nsRegion* aVisibleRegion,
    const nsRect& aListVisibleBounds) {
#ifdef DEBUG
  nsRegion r;
  r.And(*aVisibleRegion, GetClippedBounds(aBuilder));
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

static void TriggerPendingAnimations(Document& aDoc,
                                     const TimeStamp& aReadyTime) {
  MOZ_ASSERT(!aReadyTime.IsNull(),
             "Animation ready time is not set. Perhaps we're using a layer"
             " manager that doesn't update it");
  if (PendingAnimationTracker* tracker = aDoc.GetPendingAnimationTracker()) {
    PresShell* presShell = aDoc.GetPresShell();
    // If paint-suppression is in effect then we haven't finished painting
    // this document yet so we shouldn't start animations
    if (!presShell || !presShell->IsPaintingSuppressed()) {
      tracker->TriggerPendingAnimationsOnNextTick(aReadyTime);
    }
  }
  auto recurse = [&aReadyTime](Document& aDoc) {
    TriggerPendingAnimations(aDoc, aReadyTime);
    return CallState::Continue;
  };
  aDoc.EnumerateSubDocuments(recurse);
}

LayerManager* nsDisplayListBuilder::GetWidgetLayerManager(nsView** aView) {
  if (aView) {
    *aView = RootReferenceFrame()->GetView();
  }
  if (RootReferenceFrame() !=
      nsLayoutUtils::GetDisplayRootFrame(RootReferenceFrame())) {
    return nullptr;
  }
  nsIWidget* window = RootReferenceFrame()->GetNearestWidget();
  if (window) {
    return window->GetLayerManager();
  }
  return nullptr;
}

// Find the layer which should house the root scroll metadata for a given
// layer tree. This is the async zoom container layer if there is one,
// otherwise it's the root layer.
Layer* GetLayerForRootMetadata(Layer* aRootLayer, ViewID aRootScrollId) {
  Layer* asyncZoomContainer = DepthFirstSearch<ForwardIterator>(
      aRootLayer, [aRootScrollId](Layer* aLayer) {
        if (auto id = aLayer->IsAsyncZoomContainer()) {
          return *id == aRootScrollId;
        }
        return false;
      });
  return asyncZoomContainer ? asyncZoomContainer : aRootLayer;
}

FrameLayerBuilder* nsDisplayList::BuildLayers(nsDisplayListBuilder* aBuilder,
                                              LayerManager* aLayerManager,
                                              uint32_t aFlags,
                                              bool aIsWidgetTransaction) {
  nsIFrame* frame = aBuilder->RootReferenceFrame();
  nsPresContext* presContext = frame->PresContext();
  PresShell* presShell = presContext->PresShell();

  FrameLayerBuilder* layerBuilder = new FrameLayerBuilder();
  layerBuilder->Init(aBuilder, aLayerManager);

  if (aFlags & PAINT_COMPRESSED) {
    layerBuilder->SetLayerTreeCompressionMode();
  }

  RefPtr<ContainerLayer> root;
  {
    AUTO_PROFILER_LABEL_CATEGORY_PAIR(GRAPHICS_LayerBuilding);
#ifdef MOZ_GECKO_PROFILER
    nsCOMPtr<nsIDocShell> docShell = presContext->GetDocShell();
    AUTO_PROFILER_TRACING_MARKER_DOCSHELL("Paint", "LayerBuilding", GRAPHICS,
                                          docShell);
#endif

    if (XRE_IsContentProcess() && StaticPrefs::gfx_content_always_paint()) {
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

    float resolutionUniform = 1.0f;
    float resolutionX = resolutionUniform;
    float resolutionY = resolutionUniform;

    // If we are in a remote browser, then apply scaling from ancestor browsers
    if (BrowserChild* browserChild = BrowserChild::GetFrom(presShell)) {
      if (!browserChild->IsTopLevel()) {
        resolutionX *= browserChild->GetEffectsInfo().mScaleX;
        resolutionY *= browserChild->GetEffectsInfo().mScaleY;
      }
    }

    ContainerLayerParameters containerParameters(resolutionX, resolutionY);

    {
      PaintTelemetry::AutoRecord record(PaintTelemetry::Metric::Layerization);

      root = layerBuilder->BuildContainerLayerFor(aBuilder, aLayerManager,
                                                  frame, nullptr, this,
                                                  containerParameters, nullptr);

      if (!record.GetStart().IsNull() &&
          StaticPrefs::layers_acceleration_draw_fps()) {
        if (PaintTiming* pt =
                ClientLayerManager::MaybeGetPaintTiming(aLayerManager)) {
          pt->flbMs() = (TimeStamp::Now() - record.GetStart()).ToMilliseconds();
        }
      }
    }

    if (!root) {
      return nullptr;
    }
    // Root is being scaled up by the X/Y resolution. Scale it back down.
    root->SetPostScale(1.0f / resolutionX, 1.0f / resolutionY);

    auto callback = [root](ScrollableLayerGuid::ViewID aScrollId) -> bool {
      return nsLayoutUtils::ContainsMetricsWithId(root, aScrollId);
    };
    if (Maybe<ScrollMetadata> rootMetadata = nsLayoutUtils::GetRootMetadata(
            aBuilder, root->Manager(), containerParameters, callback)) {
      GetLayerForRootMetadata(root, rootMetadata->GetMetrics().GetScrollId())
          ->SetScrollMetadata(rootMetadata.value());
    }

    // NS_WARNING is debug-only, so don't even bother checking the conditions
    // in a release build.
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
already_AddRefed<LayerManager> nsDisplayList::PaintRoot(
    nsDisplayListBuilder* aBuilder, gfxContext* aCtx, uint32_t aFlags) {
  AUTO_PROFILER_LABEL("nsDisplayList::PaintRoot", GRAPHICS);

  RefPtr<LayerManager> layerManager;
  bool widgetTransaction = false;
  bool doBeginTransaction = true;
  nsView* view = nullptr;
  if (aFlags & PAINT_USE_WIDGET_LAYERS) {
    layerManager = aBuilder->GetWidgetLayerManager(&view);
    if (layerManager) {
      layerManager->SetContainsSVG(false);

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
  PresShell* presShell = presContext->PresShell();
  Document* document = presShell->GetDocument();

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
          rootPresContext->ComputePluginGeometryUpdates(
              aBuilder->RootReferenceFrame(), aBuilder, this);
        }
        // This must be called even if PluginGeometryUpdates were not computed.
        rootPresContext->CollectPluginGeometryUpdates(layerManager);
      }

      auto* wrManager = static_cast<WebRenderLayerManager*>(layerManager.get());

      nsIDocShell* docShell = presContext->GetDocShell();
      WrFiltersHolder wrFilters;
      gfx::Matrix5x4* colorMatrix =
          nsDocShell::Cast(docShell)->GetColorMatrix();
      if (colorMatrix) {
        wrFilters.filters.AppendElement(
            wr::FilterOp::ColorMatrix(colorMatrix->components));
      }

      wrManager->EndTransactionWithoutLayer(this, aBuilder,
                                            std::move(wrFilters));
    }

    // For layers-free mode, we check the invalidation state bits in the
    // EndTransaction. So we clear the invalidation state bits after
    // EndTransaction.
    if (widgetTransaction ||
        // SVG-as-an-image docs don't paint as part of the retained layer tree,
        // but they still need the invalidation state bits cleared in order for
        // invalidation for CSS/SMIL animation to work properly.
        (document && document->IsBeingUsedAsImage())) {
      frame->ClearInvalidationStateBits();
    }

    aBuilder->SetIsCompositingCheap(prevIsCompositingCheap);
    if (document && widgetTransaction) {
      TriggerPendingAnimations(*document,
                               layerManager->GetAnimationReadyTime());
    }

    if (presContext->RefreshDriver()->HasScheduleFlush()) {
      presContext->NotifyInvalidation(layerManager->GetLastTransactionId(),
                                      frame->GetRect());
    }

    return layerManager.forget();
  }

  NotifySubDocInvalidationFunc computeInvalidFunc =
      presContext->MayHavePaintEventListenerInSubDocument()
          ? nsPresContext::NotifySubDocInvalidation
          : nullptr;

  UniquePtr<LayerProperties> props;

  bool computeInvalidRect =
      (computeInvalidFunc || (!layerManager->IsCompositingCheap() &&
                              layerManager->NeedsWidgetInvalidation())) &&
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

  bool temp =
      aBuilder->SetIsCompositingCheap(layerManager->IsCompositingCheap());
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

  bool sent = false;
  if (aFlags & PAINT_IDENTICAL_DISPLAY_LIST) {
    sent = layerManager->EndEmptyTransaction(flags);
  }

  if (!sent) {
    FrameLayerBuilder* layerBuilder =
        BuildLayers(aBuilder, layerManager, aFlags, widgetTransaction);

    if (!layerBuilder) {
      layerManager->SetUserData(&gLayerManagerLayerBuilder, nullptr);
      return nullptr;
    }

    // If this is the content process, we ship plugin geometry updates over with
    // layer updates, so calculate that now before we call EndTransaction.
    nsRootPresContext* rootPresContext = presContext->GetRootPresContext();
    if (rootPresContext && XRE_IsContentProcess()) {
      if (aBuilder->WillComputePluginGeometry()) {
        rootPresContext->ComputePluginGeometryUpdates(
            aBuilder->RootReferenceFrame(), aBuilder, this);
      }
      // The layer system caches plugin configuration information for forwarding
      // with layer updates which needs to get set during reflow. This must be
      // called even if there are no windowed plugins in the page.
      rootPresContext->CollectPluginGeometryUpdates(layerManager);
    }

    layerManager->EndTransaction(FrameLayerBuilder::DrawPaintedLayer, aBuilder,
                                 flags);
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
    TriggerPendingAnimations(*document, layerManager->GetAnimationReadyTime());
  }

  nsIntRegion invalid;
  if (props) {
    if (!props->ComputeDifferences(layerManager->GetRoot(), invalid,
                                   computeInvalidFunc)) {
      invalid = nsIntRect::MaxIntRect();
    }
  } else if (widgetTransaction) {
    LayerProperties::ClearInvalidations(layerManager->GetRoot());
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
        presContext->NotifyInvalidation(layerManager->GetLastTransactionId(),
                                        bounds);
      }
    } else if (shouldInvalidate) {
      view->GetViewManager()->InvalidateView(view);
    }
  }

  layerManager->SetUserData(&gLayerManagerLayerBuilder, nullptr);
  return layerManager.forget();
}

nsDisplayItem* nsDisplayList::RemoveBottom() {
  nsDisplayItem* item = mSentinel.mAbove;
  if (!item) {
    return nullptr;
  }
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

static bool IsFrameReceivingPointerEvents(nsIFrame* aFrame) {
  return StylePointerEvents::None !=
         aFrame->StyleUI()->GetEffectivePointerEvents(aFrame);
}

// A list of frames, and their z depth. Used for sorting
// the results of hit testing.
struct FramesWithDepth {
  explicit FramesWithDepth(float aDepth) : mDepth(aDepth) {}

  bool operator<(const FramesWithDepth& aOther) const {
    if (!FuzzyEqual(mDepth, aOther.mDepth, 0.1f)) {
      // We want to sort so that the shallowest item (highest depth value) is
      // first
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

// Sort the frames by depth and then moves all the contained frames to the
// destination
static void FlushFramesArray(nsTArray<FramesWithDepth>& aSource,
                             nsTArray<nsIFrame*>* aDest) {
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
                            nsTArray<nsIFrame*>* aOutFrames) const {
  nsDisplayItem* item;

  if (aState->mInPreserves3D) {
    // Collect leaves of the current 3D rendering context.
    for (nsDisplayItem* item : *this) {
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
  for (nsDisplayItem* item : *this) {
    aState->mItemBuffer.AppendElement(item);
  }

  AutoTArray<FramesWithDepth, 16> temp;
  for (int32_t i = aState->mItemBuffer.Length() - 1; i >= itemBufferStart;
       --i) {
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
      // so we can sort them later, otherwise we add them directly to the output
      // list.
      nsTArray<nsIFrame*>* writeFrames = aOutFrames;
      if (item->GetType() == DisplayItemType::TYPE_TRANSFORM &&
          static_cast<nsDisplayTransform*>(item)->IsLeafOf3DContext()) {
        if (outFrames.Length()) {
          nsDisplayTransform* transform =
              static_cast<nsDisplayTransform*>(item);
          nsPoint point = aRect.TopLeft();
          // A 1x1 rect means a point, otherwise use the center of the rect
          if (aRect.width != 1 || aRect.height != 1) {
            point = aRect.Center();
          }
          temp.AppendElement(
              FramesWithDepth(transform->GetHitDepthAtPoint(aBuilder, point)));
          writeFrames = &temp[temp.Length() - 1].mFrames;
        }
      } else {
        // We may have just finished a run of consecutive preserve-3d
        // transforms, so flush these into the destination array before
        // processing our frame list.
        FlushFramesArray(temp, aOutFrames);
      }

      for (uint32_t j = 0; j < outFrames.Length(); j++) {
        nsIFrame* f = outFrames.ElementAt(j);
        // Filter out some frames depending on the type of hittest
        // we are doing. For visibility tests, pass through all frames.
        // For pointer tests, only pass through frames that are styled
        // to receive pointer events.
        if (aBuilder->HitTestIsForVisibility() ||
            IsFrameReceivingPointerEvents(f)) {
          writeFrames->AppendElement(f);
        }
      }

      if (aBuilder->HitTestIsForVisibility()) {
        if (aState->mHitFullyOpaqueItem ||
            item->GetOpaqueRegion(aBuilder, &snap).Contains(aRect)) {
          aState->mHitFullyOpaqueItem = true;
          // We're exiting early, so pop the remaining items off the buffer.
          aState->mItemBuffer.TruncateLength(itemBufferStart);
          break;
        }
      }
    }
  }
  // Clear any remaining preserve-3d transforms.
  FlushFramesArray(temp, aOutFrames);
  NS_ASSERTION(aState->mItemBuffer.Length() == uint32_t(itemBufferStart),
               "How did we forget to pop some elements?");
}

static nsIContent* FindContentInDocument(nsDisplayItem* aItem, Document* aDoc) {
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

  operator nsDisplayItem*() { return item; }
};

struct ZOrderComparator {
  bool operator()(const ZSortItem& aLeft, const ZSortItem& aRight) const {
    // Note that we can't just take the difference of the two
    // z-indices here, because that might overflow a 32-bit int.
    return aLeft.zIndex < aRight.zIndex;
  }
};

void nsDisplayList::SortByZOrder() { Sort<ZSortItem>(ZOrderComparator()); }

struct ContentComparator {
  nsIContent* mCommonAncestor;

  explicit ContentComparator(nsIContent* aCommonAncestor)
      : mCommonAncestor(aCommonAncestor) {}

  bool operator()(nsDisplayItem* aLeft, nsDisplayItem* aRight) const {
    // It's possible that the nsIContent for aItem1 or aItem2 is in a
    // subdocument of commonAncestor, because display items for subdocuments
    // have been mixed into the same list. Ensure that we're looking at content
    // in commonAncestor's document.
    Document* commonAncestorDoc = mCommonAncestor->OwnerDoc();
    nsIContent* content1 = FindContentInDocument(aLeft, commonAncestorDoc);
    nsIContent* content2 = FindContentInDocument(aRight, commonAncestorDoc);
    if (!content1 || !content2) {
      NS_ERROR("Document trees are mixed up!");
      // Something weird going on
      return true;
    }
    return nsLayoutUtils::CompareTreePosition(content1, content2,
                                              mCommonAncestor) < 0;
  }
};

void nsDisplayList::SortByContentOrder(nsIContent* aCommonAncestor) {
  Sort<nsDisplayItem*>(ContentComparator(aCommonAncestor));
}

bool nsDisplayItemBase::HasModifiedFrame() const {
  return mItemFlags.contains(ItemBaseFlag::ModifiedFrame);
}

void nsDisplayItemBase::SetModifiedFrame(bool aModified) {
  if (aModified) {
    mItemFlags += ItemBaseFlag::ModifiedFrame;
  } else {
    mItemFlags -= ItemBaseFlag::ModifiedFrame;
  }
}

void nsDisplayItemBase::SetDeletedFrame() {
  mItemFlags += ItemBaseFlag::DeletedFrame;
}

bool nsDisplayItemBase::HasDeletedFrame() const {
  bool retval = mItemFlags.contains(ItemBaseFlag::DeletedFrame) ||
                (GetType() == DisplayItemType::TYPE_REMOTE &&
                 !static_cast<const nsDisplayRemote*>(this)->GetFrameLoader());
  MOZ_ASSERT(retval || mFrame);
  return retval;
}

#if !defined(DEBUG) && !defined(MOZ_DIAGNOSTIC_ASSERT_ENABLED)
static_assert(sizeof(nsDisplayItem) <= 176, "nsDisplayItem has grown");
#endif

nsDisplayItem::nsDisplayItem(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame)
    : nsDisplayItem(aBuilder, aFrame, aBuilder->CurrentActiveScrolledRoot()) {}

nsDisplayItem::nsDisplayItem(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                             const ActiveScrolledRoot* aActiveScrolledRoot)
    : nsDisplayItemBase(aBuilder, aFrame),
      mActiveScrolledRoot(aActiveScrolledRoot),
      mAnimatedGeometryRoot(nullptr) {
  MOZ_COUNT_CTOR(nsDisplayItem);

  mReferenceFrame = aBuilder->FindReferenceFrameFor(aFrame, &mToReferenceFrame);
  // This can return the wrong result if the item override
  // ShouldFixToViewport(), the item needs to set it again in its constructor.
  mAnimatedGeometryRoot = aBuilder->FindAnimatedGeometryRootFor(aFrame);
  MOZ_ASSERT(nsLayoutUtils::IsAncestorFrameCrossDoc(
                 aBuilder->RootReferenceFrame(), *mAnimatedGeometryRoot),
             "Bad");
  NS_ASSERTION(
      aBuilder->GetVisibleRect().width >= 0 || !aBuilder->IsForPainting(),
      "visible rect not set");

  nsDisplayItem::SetClipChain(
      aBuilder->ClipState().GetCurrentCombinedClipChain(aBuilder), true);

  // The visible rect is for mCurrentFrame, so we have to use
  // mCurrentOffsetToReferenceFrame
  nsRect visible = aBuilder->GetVisibleRect() +
                   aBuilder->GetCurrentFrameOffsetToReferenceFrame();
  SetBuildingRect(visible);

  const nsStyleDisplay* disp = mFrame->StyleDisplay();
  if (mFrame->BackfaceIsHidden(disp)) {
    mItemFlags += ItemFlag::BackfaceHidden;
  }
  if (mFrame->Combines3DTransformWithAncestors(disp)) {
    mItemFlags += ItemFlag::Combines3DTransformWithAncestors;
  }
}

/* static */
bool nsDisplayItem::ForceActiveLayers() {
  return StaticPrefs::layers_force_active();
}

int32_t nsDisplayItem::ZIndex() const { return mFrame->ZIndex(); }

bool nsDisplayItem::ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                      nsRegion* aVisibleRegion) {
  return !GetPaintRect().IsEmpty() &&
         !IsInvisibleInRect(aVisibleRegion->GetBounds());
}

bool nsDisplayItem::RecomputeVisibility(nsDisplayListBuilder* aBuilder,
                                        nsRegion* aVisibleRegion) {
  if (ForceNotVisible() && !GetSameCoordinateSystemChildren()) {
    // mForceNotVisible wants to ensure that this display item doesn't render
    // anything itself. If this item has contents, then we obviously want to
    // render those, so we don't need this check in that case.
    NS_ASSERTION(GetBuildingRect().IsEmpty(),
                 "invisible items without children should have empty vis rect");
    SetPaintRect(nsRect());
  } else {
    bool snap;
    nsRect bounds = GetBounds(aBuilder, &snap);

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

void nsDisplayItem::SetClipChain(const DisplayItemClipChain* aClipChain,
                                 bool aStore) {
  mClipChain = aClipChain;
  mClip = DisplayItemClipChain::ClipForASR(aClipChain, mActiveScrolledRoot);

  if (aStore) {
    mState.mClipChain = mClipChain;
    mState.mClip = mClip;
  }
}

Maybe<nsRect> nsDisplayItem::GetClipWithRespectToASR(
    nsDisplayListBuilder* aBuilder, const ActiveScrolledRoot* aASR) const {
  if (const DisplayItemClip* clip =
          DisplayItemClipChain::ClipForASR(GetClipChain(), aASR)) {
    return Some(clip->GetClipRect());
  }
#ifdef DEBUG
  MOZ_ASSERT(false, "item should have finite clip with respect to aASR");
#endif
  return Nothing();
}

void nsDisplayItem::FuseClipChainUpTo(nsDisplayListBuilder* aBuilder,
                                      const ActiveScrolledRoot* aASR) {
  mClipChain = aBuilder->FuseClipChainUpTo(mClipChain, aASR);

  if (mClipChain) {
    mClip = &mClipChain->mClip;
  } else {
    mClip = nullptr;
  }
}

bool nsDisplayItem::ShouldUseAdvancedLayer(LayerManager* aManager,
                                           PrefFunc aFunc) const {
  return CanUseAdvancedLayer(aManager) ? aFunc() : false;
}

bool nsDisplayItem::CanUseAdvancedLayer(LayerManager* aManager) const {
  return StaticPrefs::layers_advanced_basic_layer_enabled() || !aManager ||
         aManager->GetBackendType() == layers::LayersBackend::LAYERS_WR;
}

static const DisplayItemClipChain* FindCommonAncestorClipForIntersection(
    const DisplayItemClipChain* aOne, const DisplayItemClipChain* aTwo) {
  for (const ActiveScrolledRoot* asr =
           ActiveScrolledRoot::PickDescendant(aOne->mASR, aTwo->mASR);
       asr; asr = asr->mParent) {
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

void nsDisplayItem::IntersectClip(nsDisplayListBuilder* aBuilder,
                                  const DisplayItemClipChain* aOther,
                                  bool aStore) {
  if (!aOther || mClipChain == aOther) {
    return;
  }

  // aOther might be a reference to a clip on the stack. We need to make sure
  // that CreateClipChainIntersection will allocate the actual intersected
  // clip in the builder's arena, so for the mClipChain == nullptr case,
  // we supply nullptr as the common ancestor so that
  // CreateClipChainIntersection clones the whole chain.
  const DisplayItemClipChain* ancestorClip =
      mClipChain ? FindCommonAncestorClipForIntersection(mClipChain, aOther)
                 : nullptr;

  SetClipChain(
      aBuilder->CreateClipChainIntersection(ancestorClip, mClipChain, aOther),
      aStore);
}

nsRect nsDisplayItem::GetClippedBounds(nsDisplayListBuilder* aBuilder) const {
  bool snap;
  nsRect r = GetBounds(aBuilder, &snap);
  return GetClip().ApplyNonRoundedIntersection(r);
}

nsDisplayContainer::nsDisplayContainer(
    nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
    const ActiveScrolledRoot* aActiveScrolledRoot, nsDisplayList* aList)
    : nsDisplayItem(aBuilder, aFrame, aActiveScrolledRoot) {
  MOZ_COUNT_CTOR(nsDisplayContainer);
  mChildren.AppendToTop(aList);
  UpdateBounds(aBuilder);

  // Clear and store the clip chain set by nsDisplayItem constructor.
  nsDisplayItem::SetClipChain(nullptr, true);
}

bool nsDisplayContainer::CreateWebRenderCommands(
    mozilla::wr::DisplayListBuilder& aBuilder,
    mozilla::wr::IpcResourceUpdateQueue& aResources,
    const StackingContextHelper& aSc,
    mozilla::layers::RenderRootStateManager* aManager,
    nsDisplayListBuilder* aDisplayListBuilder) {
  aManager->CommandBuilder().CreateWebRenderCommandsFromDisplayList(
      GetChildren(), this, aDisplayListBuilder, aSc, aBuilder, aResources);
  return true;
}

/**
 * Like |nsDisplayList::ComputeVisibilityForSublist()|, but restricts
 * |aVisibleRegion| to given |aBounds| of the list.
 */
static bool ComputeClippedVisibilityForSubList(nsDisplayListBuilder* aBuilder,
                                               nsRegion* aVisibleRegion,
                                               nsDisplayList* aList,
                                               const nsRect& aBounds) {
  nsRegion visibleRegion;
  visibleRegion.And(*aVisibleRegion, aBounds);
  nsRegion originalVisibleRegion = visibleRegion;

  const bool anyItemVisible =
      aList->ComputeVisibilityForSublist(aBuilder, &visibleRegion, aBounds);
  nsRegion removed;
  // removed = originalVisibleRegion - visibleRegion
  removed.Sub(originalVisibleRegion, visibleRegion);
  // aVisibleRegion = aVisibleRegion - removed (modulo any simplifications
  // SubtractFromVisibleRegion does)
  aBuilder->SubtractFromVisibleRegion(aVisibleRegion, removed);

  return anyItemVisible;
}

bool nsDisplayContainer::ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                           nsRegion* aVisibleRegion) {
  return ::ComputeClippedVisibilityForSubList(aBuilder, aVisibleRegion,
                                              GetChildren(), GetPaintRect());
}

nsRect nsDisplayContainer::GetBounds(nsDisplayListBuilder* aBuilder,
                                     bool* aSnap) const {
  *aSnap = false;
  return mBounds;
}

nsRect nsDisplayContainer::GetComponentAlphaBounds(
    nsDisplayListBuilder* aBuilder) const {
  return mChildren.GetComponentAlphaBounds(aBuilder);
}

static nsRegion GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                                nsDisplayList* aList,
                                const nsRect& aListBounds) {
  if (aList->IsOpaque()) {
    // Everything within list bounds that's visible is opaque. This is an
    // optimization to avoid calculating the opaque region.
    return aListBounds;
  }

  if (aBuilder->HitTestIsForVisibility()) {
    // If we care about an accurate opaque region, iterate the display list
    // and build up a region of opaque bounds.
    return aList->GetOpaqueRegion(aBuilder);
  }

  return nsRegion();
}

nsRegion nsDisplayContainer::GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                                             bool* aSnap) const {
  return ::GetOpaqueRegion(aBuilder, GetChildren(), GetBounds(aBuilder, aSnap));
}

Maybe<nsRect> nsDisplayContainer::GetClipWithRespectToASR(
    nsDisplayListBuilder* aBuilder, const ActiveScrolledRoot* aASR) const {
  // Our children should have finite bounds with respect to |aASR|.
  if (aASR == mActiveScrolledRoot) {
    return Some(mBounds);
  }

  return Some(mChildren.GetClippedBoundsWithRespectToASR(aBuilder, aASR));
}

void nsDisplayContainer::HitTest(nsDisplayListBuilder* aBuilder,
                                 const nsRect& aRect, HitTestState* aState,
                                 nsTArray<nsIFrame*>* aOutFrames) {
  mChildren.HitTest(aBuilder, aRect, aState, aOutFrames);
}

void nsDisplayContainer::UpdateBounds(nsDisplayListBuilder* aBuilder) {
  // Container item bounds are expected to be clipped.
  mBounds =
      mChildren.GetClippedBoundsWithRespectToASR(aBuilder, mActiveScrolledRoot);
}

nsRect nsDisplaySolidColor::GetBounds(nsDisplayListBuilder* aBuilder,
                                      bool* aSnap) const {
  *aSnap = true;
  return mBounds;
}

LayerState nsDisplaySolidColor::GetLayerState(
    nsDisplayListBuilder* aBuilder, LayerManager* aManager,
    const ContainerLayerParameters& aParameters) {
  if (ForceActiveLayers()) {
    return LayerState::LAYER_ACTIVE;
  }

  return LayerState::LAYER_NONE;
}

already_AddRefed<Layer> nsDisplaySolidColor::BuildLayer(
    nsDisplayListBuilder* aBuilder, LayerManager* aManager,
    const ContainerLayerParameters& aContainerParameters) {
  RefPtr<ColorLayer> layer = static_cast<ColorLayer*>(
      aManager->GetLayerBuilder()->GetLeafLayerFor(aBuilder, this));
  if (!layer) {
    layer = aManager->CreateColorLayer();
    if (!layer) {
      return nullptr;
    }
  }
  layer->SetColor(ToDeviceColor(mColor));

  const int32_t appUnitsPerDevPixel =
      mFrame->PresContext()->AppUnitsPerDevPixel();
  layer->SetBounds(mBounds.ToNearestPixels(appUnitsPerDevPixel));
  layer->SetBaseTransform(gfx::Matrix4x4::Translation(
      aContainerParameters.mOffset.x, aContainerParameters.mOffset.y, 0));

  return layer.forget();
}

void nsDisplaySolidColor::Paint(nsDisplayListBuilder* aBuilder,
                                gfxContext* aCtx) {
  int32_t appUnitsPerDevPixel = mFrame->PresContext()->AppUnitsPerDevPixel();
  DrawTarget* drawTarget = aCtx->GetDrawTarget();
  Rect rect =
      NSRectToSnappedRect(GetPaintRect(), appUnitsPerDevPixel, *drawTarget);
  drawTarget->FillRect(rect, ColorPattern(ToDeviceColor(mColor)));
}

void nsDisplaySolidColor::WriteDebugInfo(std::stringstream& aStream) {
  aStream << " (rgba " << (int)NS_GET_R(mColor) << "," << (int)NS_GET_G(mColor)
          << "," << (int)NS_GET_B(mColor) << "," << (int)NS_GET_A(mColor)
          << ")";
}

bool nsDisplaySolidColor::CreateWebRenderCommands(
    mozilla::wr::DisplayListBuilder& aBuilder,
    mozilla::wr::IpcResourceUpdateQueue& aResources,
    const StackingContextHelper& aSc,
    mozilla::layers::RenderRootStateManager* aManager,
    nsDisplayListBuilder* aDisplayListBuilder) {
  LayoutDeviceRect bounds = LayoutDeviceRect::FromAppUnits(
      mBounds, mFrame->PresContext()->AppUnitsPerDevPixel());

  // There is one big solid color behind everything - just split it out if it
  // intersects multiple render roots
  if (aBuilder.GetRenderRoot() == wr::RenderRoot::Default) {
    for (auto renderRoot : wr::kRenderRoots) {
      if (aBuilder.HasSubBuilder(renderRoot)) {
        LayoutDeviceRect renderRootRect =
            aDisplayListBuilder->GetRenderRootRect(renderRoot);
        wr::LayoutRect intersection =
            wr::ToLayoutRect(bounds.Intersect(renderRootRect));
        aBuilder.SubBuilder(renderRoot)
            .PushRect(intersection, intersection, !BackfaceIsHidden(),
                      wr::ToColorF(ToDeviceColor(mColor)));
      }
    }
  } else {
    wr::LayoutRect r = wr::ToLayoutRect(bounds);

    aBuilder.PushRect(r, r, !BackfaceIsHidden(),
                      wr::ToColorF(ToDeviceColor(mColor)));
  }

  return true;
}

nsRect nsDisplaySolidColorRegion::GetBounds(nsDisplayListBuilder* aBuilder,
                                            bool* aSnap) const {
  *aSnap = true;
  return mRegion.GetBounds();
}

void nsDisplaySolidColorRegion::Paint(nsDisplayListBuilder* aBuilder,
                                      gfxContext* aCtx) {
  int32_t appUnitsPerDevPixel = mFrame->PresContext()->AppUnitsPerDevPixel();
  DrawTarget* drawTarget = aCtx->GetDrawTarget();
  ColorPattern color(ToDeviceColor(mColor));
  for (auto iter = mRegion.RectIter(); !iter.Done(); iter.Next()) {
    Rect rect =
        NSRectToSnappedRect(iter.Get(), appUnitsPerDevPixel, *drawTarget);
    drawTarget->FillRect(rect, color);
  }
}

void nsDisplaySolidColorRegion::WriteDebugInfo(std::stringstream& aStream) {
  aStream << " (rgba " << int(mColor.r * 255) << "," << int(mColor.g * 255)
          << "," << int(mColor.b * 255) << "," << mColor.a << ")";
}

bool nsDisplaySolidColorRegion::CreateWebRenderCommands(
    mozilla::wr::DisplayListBuilder& aBuilder,
    mozilla::wr::IpcResourceUpdateQueue& aResources,
    const StackingContextHelper& aSc,
    mozilla::layers::RenderRootStateManager* aManager,
    nsDisplayListBuilder* aDisplayListBuilder) {
  for (auto iter = mRegion.RectIter(); !iter.Done(); iter.Next()) {
    nsRect rect = iter.Get();
    LayoutDeviceRect layerRects = LayoutDeviceRect::FromAppUnits(
        rect, mFrame->PresContext()->AppUnitsPerDevPixel());
    wr::LayoutRect r = wr::ToLayoutRect(layerRects);
    aBuilder.PushRect(r, r, !BackfaceIsHidden(),
                      wr::ToColorF(ToDeviceColor(mColor)));
  }

  return true;
}

static void RegisterThemeGeometry(nsDisplayListBuilder* aBuilder,
                                  nsDisplayItem* aItem, nsIFrame* aFrame,
                                  nsITheme::ThemeGeometryType aType) {
  if (aBuilder->IsInChromeDocumentOrPopup() && !aBuilder->IsInTransform()) {
    nsIFrame* displayRoot = nsLayoutUtils::GetDisplayRootFrame(aFrame);
    nsPoint offset = aBuilder->IsInSubdocument()
                         ? aBuilder->ToReferenceFrame(aFrame)
                         : aFrame->GetOffsetTo(displayRoot);
    nsRect borderBox = nsRect(offset, aFrame->GetSize());
    aBuilder->RegisterThemeGeometry(
        aType, aItem,
        LayoutDeviceIntRect::FromUnknownRect(borderBox.ToNearestPixels(
            aFrame->PresContext()->AppUnitsPerDevPixel())));
  }
}

// Return the bounds of the viewport relative to |aFrame|'s reference frame.
// Returns Nothing() if transforming into |aFrame|'s coordinate space fails.
static Maybe<nsRect> GetViewportRectRelativeToReferenceFrame(
    nsDisplayListBuilder* aBuilder, nsIFrame* aFrame) {
  nsIFrame* rootFrame = aFrame->PresShell()->GetRootFrame();
  nsRect rootRect = rootFrame->GetRectRelativeToSelf();
  if (nsLayoutUtils::TransformRect(rootFrame, aFrame, rootRect) ==
      nsLayoutUtils::TRANSFORM_SUCCEEDED) {
    return Some(rootRect + aBuilder->ToReferenceFrame(aFrame));
  }
  return Nothing();
}

/* static */ nsDisplayBackgroundImage::InitData
nsDisplayBackgroundImage::GetInitData(nsDisplayListBuilder* aBuilder,
                                      nsIFrame* aFrame, uint16_t aLayer,
                                      const nsRect& aBackgroundRect,
                                      ComputedStyle* aBackgroundStyle) {
  nsPresContext* presContext = aFrame->PresContext();
  uint32_t flags = aBuilder->GetBackgroundPaintFlags();
  const nsStyleImageLayers::Layer& layer =
      aBackgroundStyle->StyleBackground()->mImage.mLayers[aLayer];

  bool isTransformedFixed;
  nsBackgroundLayerState state = nsCSSRendering::PrepareImageLayer(
      presContext, aFrame, flags, aBackgroundRect, aBackgroundRect, layer,
      &isTransformedFixed);

  // background-attachment:fixed is treated as background-attachment:scroll
  // if it's affected by a transform.
  // See https://www.w3.org/Bugs/Public/show_bug.cgi?id=17521.
  bool shouldTreatAsFixed =
      layer.mAttachment == StyleImageLayerAttachment::Fixed &&
      !isTransformedFixed;

  bool shouldFixToViewport = shouldTreatAsFixed && !layer.mImage.IsNone();
  bool isRasterImage = state.mImageRenderer.IsRasterImage();
  nsCOMPtr<imgIContainer> image;
  if (isRasterImage) {
    image = state.mImageRenderer.GetImage();
  }
  return InitData{aBuilder,        aBackgroundStyle, image,
                  aBackgroundRect, state.mFillArea,  state.mDestArea,
                  aLayer,          isRasterImage,    shouldFixToViewport};
}

nsDisplayBackgroundImage::nsDisplayBackgroundImage(
    nsDisplayListBuilder* aBuilder, nsIFrame* aFrame, const InitData& aInitData,
    nsIFrame* aFrameForBounds)
    : nsDisplayImageContainer(aBuilder, aFrame),
      mBackgroundStyle(aInitData.backgroundStyle),
      mImage(aInitData.image),
      mDependentFrame(nullptr),
      mBackgroundRect(aInitData.backgroundRect),
      mFillRect(aInitData.fillArea),
      mDestRect(aInitData.destArea),
      mLayer(aInitData.layer),
      mIsRasterImage(aInitData.isRasterImage),
      mShouldFixToViewport(aInitData.shouldFixToViewport),
      mImageFlags(0) {
  MOZ_COUNT_CTOR(nsDisplayBackgroundImage);
#ifdef DEBUG
  if (mBackgroundStyle && mBackgroundStyle != mFrame->Style()) {
    // If this changes, then you also need to adjust css::ImageLoader to
    // invalidate mFrame as needed.
    MOZ_ASSERT(mFrame->IsCanvasFrame() ||
               mFrame->IsFrameOfType(nsIFrame::eTablePart));
  }
#endif

  mBounds = GetBoundsInternal(aInitData.builder, aFrameForBounds);
  if (mShouldFixToViewport) {
    mAnimatedGeometryRoot =
        aInitData.builder->FindAnimatedGeometryRootFor(this);

    // Expand the item's visible rect to cover the entire bounds, limited to the
    // viewport rect. This is necessary because the background's clip can move
    // asynchronously.
    if (Maybe<nsRect> viewportRect = GetViewportRectRelativeToReferenceFrame(
            aInitData.builder, mFrame)) {
      SetBuildingRect(mBounds.Intersect(*viewportRect));
    }
  }
}

nsDisplayBackgroundImage::~nsDisplayBackgroundImage() {
  MOZ_COUNT_DTOR(nsDisplayBackgroundImage);
  if (mDependentFrame) {
    mDependentFrame->RemoveDisplayItem(this);
  }
}

static nsIFrame* GetBackgroundComputedStyleFrame(nsIFrame* aFrame) {
  nsIFrame* f;
  if (!nsCSSRendering::FindBackgroundFrame(aFrame, &f)) {
    // We don't want to bail out if moz-appearance is set on a root
    // node. If it has a parent content node, bail because it's not
    // a root, other wise keep going in order to let the theme stuff
    // draw the background. The canvas really should be drawing the
    // bg, but there's no way to hook that up via css.
    if (!aFrame->StyleDisplay()->HasAppearance()) {
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

static void SetBackgroundClipRegion(
    DisplayListClipState::AutoSaveRestore& aClipState, nsIFrame* aFrame,
    const nsStyleImageLayers::Layer& aLayer, const nsRect& aBackgroundRect,
    bool aWillPaintBorder) {
  nsCSSRendering::ImageLayerClipState clip;
  nsCSSRendering::GetImageLayerClip(
      aLayer, aFrame, *aFrame->StyleBorder(), aBackgroundRect, aBackgroundRect,
      aWillPaintBorder, aFrame->PresContext()->AppUnitsPerDevPixel(), &clip);

  if (clip.mHasAdditionalBGClipArea) {
    aClipState.ClipContentDescendants(
        clip.mAdditionalBGClipArea, clip.mBGClipArea,
        clip.mHasRoundedCorners ? clip.mRadii : nullptr);
  } else {
    aClipState.ClipContentDescendants(
        clip.mBGClipArea, clip.mHasRoundedCorners ? clip.mRadii : nullptr);
  }
}

/**
 * This is used for the find bar highlighter overlay. It's only accessible
 * through the AnonymousContent API, so it's not exposed to general web pages.
 */
static bool SpecialCutoutRegionCase(nsDisplayListBuilder* aBuilder,
                                    nsIFrame* aFrame,
                                    const nsRect& aBackgroundRect,
                                    nsDisplayList* aList, nscolor aColor) {
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
  aList->AppendNewToTop<nsDisplaySolidColorRegion>(aBuilder, aFrame, region,
                                                   aColor);

  return true;
}

/*static*/
bool nsDisplayBackgroundImage::AppendBackgroundItemsToTop(
    nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
    const nsRect& aBackgroundRect, nsDisplayList* aList,
    bool aAllowWillPaintBorderOptimization, ComputedStyle* aComputedStyle,
    const nsRect& aBackgroundOriginRect, nsIFrame* aSecondaryReferenceFrame,
    Maybe<nsDisplayListBuilder::AutoBuildingDisplayList>*
        aAutoBuildingDisplayList) {
  ComputedStyle* bgSC = aComputedStyle;
  const nsStyleBackground* bg = nullptr;
  nsRect bgRect = aBackgroundRect;
  nsRect bgOriginRect = bgRect;
  if (!aBackgroundOriginRect.IsEmpty()) {
    bgOriginRect = aBackgroundOriginRect;
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
  nscolor color = NS_RGBA(0, 0, 0, 0);
  if (!nsCSSRendering::IsCanvasFrame(aFrame) && bg) {
    bool drawBackgroundImage;
    color = nsCSSRendering::DetermineBackgroundColor(
        presContext, bgSC, aFrame, drawBackgroundImage, drawBackgroundColor);
  }

  if (SpecialCutoutRegionCase(aBuilder, aFrame, aBackgroundRect, aList,
                              color)) {
    return false;
  }

  const nsStyleBorder* borderStyle = aFrame->StyleBorder();
  const nsStyleEffects* effectsStyle = aFrame->StyleEffects();
  bool hasInsetShadow = effectsStyle->HasBoxShadowWithInset(true);
  bool willPaintBorder = aAllowWillPaintBorderOptimization && !isThemed &&
                         !hasInsetShadow && borderStyle->HasBorder();

  // An auxiliary list is necessary in case we have background blending; if that
  // is the case, background items need to be wrapped by a blend container to
  // isolate blending to the background
  nsDisplayList bgItemList;
  // Even if we don't actually have a background color to paint, we may still
  // need to create an item for hit testing.
  if ((drawBackgroundColor && color != NS_RGBA(0, 0, 0, 0)) ||
      aBuilder->IsForEventDelivery()) {
    if (aAutoBuildingDisplayList && !*aAutoBuildingDisplayList) {
      aAutoBuildingDisplayList->emplace(aBuilder, aFrame);
    }
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
      bool useWillPaintBorderOptimization =
          willPaintBorder &&
          nsLayoutUtils::HasNonZeroCorner(borderStyle->mBorderRadius);

      nsCSSRendering::ImageLayerClipState clip;
      nsCSSRendering::GetImageLayerClip(
          bg->BottomLayer(), aFrame, *aFrame->StyleBorder(), bgRect, bgRect,
          useWillPaintBorderOptimization,
          aFrame->PresContext()->AppUnitsPerDevPixel(), &clip);

      bgColorRect = bgColorRect.Intersect(clip.mBGClipArea);
      if (clip.mHasAdditionalBGClipArea) {
        bgColorRect = bgColorRect.Intersect(clip.mAdditionalBGClipArea);
      }
      if (clip.mHasRoundedCorners) {
        clipState.emplace(aBuilder);
        clipState->ClipContentDescendants(clip.mBGClipArea, clip.mRadii);
      }
    }
    nsDisplayBackgroundColor* bgItem;
    if (aSecondaryReferenceFrame) {
      bgItem = MakeDisplayItem<nsDisplayTableBackgroundColor>(
          aBuilder, aSecondaryReferenceFrame, bgColorRect, bgSC,
          drawBackgroundColor ? color : NS_RGBA(0, 0, 0, 0), aFrame);
    } else {
      bgItem = MakeDisplayItem<nsDisplayBackgroundColor>(
          aBuilder, aFrame, bgColorRect, bgSC,
          drawBackgroundColor ? color : NS_RGBA(0, 0, 0, 0));
    }
    if (bgItem) {
      bgItem->SetDependentFrame(aBuilder, dependentFrame);
      bgItemList.AppendToTop(bgItem);
    }
  }

  if (isThemed) {
    nsITheme* theme = presContext->Theme();
    if (theme->NeedToClearBackgroundBehindWidget(
            aFrame, aFrame->StyleDisplay()->mAppearance) &&
        aBuilder->IsInChromeDocumentOrPopup() && !aBuilder->IsInTransform()) {
      bgItemList.AppendNewToTop<nsDisplayClearBackground>(aBuilder, aFrame);
    }
    if (aSecondaryReferenceFrame) {
      nsDisplayTableThemedBackground* bgItem =
          MakeDisplayItem<nsDisplayTableThemedBackground>(
              aBuilder, aSecondaryReferenceFrame, bgRect, aFrame);
      if (bgItem) {
        bgItem->Init(aBuilder);
        bgItemList.AppendToTop(bgItem);
      }
    } else {
      nsDisplayThemedBackground* bgItem =
          MakeDisplayItem<nsDisplayThemedBackground>(aBuilder, aFrame, bgRect);
      if (bgItem) {
        bgItem->Init(aBuilder);
        bgItemList.AppendToTop(bgItem);
      }
    }
    aList->AppendToTop(&bgItemList);
    return true;
  }

  if (!bg) {
    aList->AppendToTop(&bgItemList);
    return false;
  }

  const ActiveScrolledRoot* asr = aBuilder->CurrentActiveScrolledRoot();

  bool needBlendContainer = false;

  // Passing bg == nullptr in this macro will result in one iteration with
  // i = 0.
  NS_FOR_VISIBLE_IMAGE_LAYERS_BACK_TO_FRONT(i, bg->mImage) {
    if (bg->mImage.mLayers[i].mImage.IsNone()) {
      continue;
    }

    if (aAutoBuildingDisplayList && !*aAutoBuildingDisplayList) {
      aAutoBuildingDisplayList->emplace(aBuilder, aFrame);
    }

    if (bg->mImage.mLayers[i].mBlendMode != StyleBlend::Normal) {
      needBlendContainer = true;
    }

    DisplayListClipState::AutoSaveRestore clipState(aBuilder);
    if (!aBuilder->IsForEventDelivery()) {
      const nsStyleImageLayers::Layer& layer = bg->mImage.mLayers[i];
      SetBackgroundClipRegion(clipState, aFrame, layer, bgRect,
                              willPaintBorder);
    }

    nsDisplayList thisItemList;
    nsDisplayBackgroundImage::InitData bgData =
        nsDisplayBackgroundImage::GetInitData(aBuilder, aFrame, i, bgOriginRect,
                                              bgSC);

    if (bgData.shouldFixToViewport) {
      auto* displayData = aBuilder->GetCurrentFixedBackgroundDisplayData();
      nsDisplayListBuilder::AutoBuildingDisplayList buildingDisplayList(
          aBuilder, aFrame, aBuilder->GetVisibleRect(),
          aBuilder->GetDirtyRect());

      nsDisplayListBuilder::AutoCurrentActiveScrolledRootSetter asrSetter(
          aBuilder);
      if (displayData) {
        asrSetter.SetCurrentActiveScrolledRoot(
            displayData->mContainingBlockActiveScrolledRoot);
        if (nsLayoutUtils::UsesAsyncScrolling(aFrame)) {
          // Override the dirty rect on the builder to be the dirty rect of
          // the viewport.
          // displayData->mDirtyRect is relative to the presshell's viewport
          // frame (the root frame), and we need it to be relative to aFrame.
          nsIFrame* rootFrame =
              aBuilder->CurrentPresShellState()->mPresShell->GetRootFrame();
          // There cannot be any transforms between aFrame and rootFrame
          // because then bgData.shouldFixToViewport would have been false.
          nsRect visibleRect =
              displayData->mVisibleRect + aFrame->GetOffsetTo(rootFrame);
          aBuilder->SetVisibleRect(visibleRect);
          nsRect dirtyRect =
              displayData->mDirtyRect + aFrame->GetOffsetTo(rootFrame);
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
          bgItem = MakeDisplayItem<nsDisplayTableBackgroundImage>(
              aBuilder, aSecondaryReferenceFrame, bgData, aFrame);
        } else {
          bgItem = MakeDisplayItem<nsDisplayBackgroundImage>(aBuilder, aFrame,
                                                             bgData);
        }
      }
      if (bgItem) {
        bgItem->SetDependentFrame(aBuilder, dependentFrame);
        if (aSecondaryReferenceFrame) {
          thisItemList.AppendToTop(
              nsDisplayTableFixedPosition::CreateForFixedBackground(
                  aBuilder, aSecondaryReferenceFrame, bgItem, i, aFrame));
        } else {
          thisItemList.AppendToTop(
              nsDisplayFixedPosition::CreateForFixedBackground(aBuilder, aFrame,
                                                               bgItem, i));
        }
      }

    } else {
      nsDisplayBackgroundImage* bgItem;
      if (aSecondaryReferenceFrame) {
        bgItem = MakeDisplayItem<nsDisplayTableBackgroundImage>(
            aBuilder, aSecondaryReferenceFrame, bgData, aFrame);
      } else {
        bgItem =
            MakeDisplayItem<nsDisplayBackgroundImage>(aBuilder, aFrame, bgData);
      }
      if (bgItem) {
        bgItem->SetDependentFrame(aBuilder, dependentFrame);
        thisItemList.AppendToTop(bgItem);
      }
    }

    if (bg->mImage.mLayers[i].mBlendMode != StyleBlend::Normal) {
      DisplayListClipState::AutoSaveRestore blendClip(aBuilder);
      // asr is scrolled. Even if we wrap a fixed background layer, that's
      // fine, because the item will have a scrolled clip that limits the
      // item with respect to asr.
      if (aSecondaryReferenceFrame) {
        thisItemList.AppendNewToTop<nsDisplayTableBlendMode>(
            aBuilder, aSecondaryReferenceFrame, &thisItemList,
            bg->mImage.mLayers[i].mBlendMode, asr, i + 1, aFrame);
      } else {
        thisItemList.AppendNewToTop<nsDisplayBlendMode>(
            aBuilder, aFrame, &thisItemList, bg->mImage.mLayers[i].mBlendMode,
            asr, i + 1);
      }
    }
    bgItemList.AppendToTop(&thisItemList);
  }

  if (needBlendContainer) {
    DisplayListClipState::AutoSaveRestore blendContainerClip(aBuilder);
    if (aSecondaryReferenceFrame) {
      bgItemList.AppendToTop(
          nsDisplayTableBlendContainer::CreateForBackgroundBlendMode(
              aBuilder, aSecondaryReferenceFrame, &bgItemList, asr, aFrame));
    } else {
      bgItemList.AppendToTop(
          nsDisplayBlendContainer::CreateForBackgroundBlendMode(
              aBuilder, aFrame, &bgItemList, asr));
    }
  }

  aList->AppendToTop(&bgItemList);
  return false;
}

// Check that the rounded border of aFrame, added to aToReferenceFrame,
// intersects aRect.  Assumes that the unrounded border has already
// been checked for intersection.
static bool RoundedBorderIntersectsRect(nsIFrame* aFrame,
                                        const nsPoint& aFrameToReferenceFrame,
                                        const nsRect& aTestRect) {
  if (!nsRect(aFrameToReferenceFrame, aFrame->GetSize()).Intersects(aTestRect))
    return false;

  nscoord radii[8];
  return !aFrame->GetBorderRadii(radii) ||
         nsLayoutUtils::RoundedRectIntersectsRect(
             nsRect(aFrameToReferenceFrame, aFrame->GetSize()), radii,
             aTestRect);
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
  nsRegion rgn = nsLayoutUtils::RoundedRectIntersectRect(aRoundedRect, aRadii,
                                                         aContainedRect);
  return rgn.Contains(aContainedRect);
}

bool nsDisplayBackgroundImage::CanOptimizeToImageLayer(
    LayerManager* aManager, nsDisplayListBuilder* aBuilder) {
  if (!mBackgroundStyle) {
    return false;
  }

  // We currently can't handle tiled backgrounds.
  if (!mDestRect.Contains(mFillRect)) {
    return false;
  }

  // For 'contain' and 'cover', we allow any pixel of the image to be sampled
  // because there isn't going to be any spriting/atlasing going on.
  const nsStyleImageLayers::Layer& layer =
      mBackgroundStyle->StyleBackground()->mImage.mLayers[mLayer];
  bool allowPartialImages = layer.mSize.IsContain() || layer.mSize.IsCover();
  if (!allowPartialImages && !mFillRect.Contains(mDestRect)) {
    return false;
  }

  return nsDisplayImageContainer::CanOptimizeToImageLayer(aManager, aBuilder);
}

nsRect nsDisplayBackgroundImage::GetDestRect() const { return mDestRect; }

already_AddRefed<imgIContainer> nsDisplayBackgroundImage::GetImage() {
  nsCOMPtr<imgIContainer> image = mImage;
  return image.forget();
}

nsDisplayBackgroundImage::ImageLayerization
nsDisplayBackgroundImage::ShouldCreateOwnLayer(nsDisplayListBuilder* aBuilder,
                                               LayerManager* aManager) {
  if (ForceActiveLayers()) {
    return WHENEVER_POSSIBLE;
  }

  nsIFrame* backgroundStyleFrame =
      nsCSSRendering::FindBackgroundStyleFrame(StyleFrame());
  if (ActiveLayerTracker::IsBackgroundPositionAnimated(aBuilder,
                                                       backgroundStyleFrame)) {
    return WHENEVER_POSSIBLE;
  }

  if (StaticPrefs::layout_animated_image_layers_enabled() && mBackgroundStyle) {
    const nsStyleImageLayers::Layer& layer =
        mBackgroundStyle->StyleBackground()->mImage.mLayers[mLayer];
    const auto* image = &layer.mImage;
    if (auto* request = image->GetImageRequest()) {
      nsCOMPtr<imgIContainer> image;
      if (NS_SUCCEEDED(request->GetImage(getter_AddRefs(image))) && image) {
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

static void CheckForBorderItem(nsDisplayItem* aItem, uint32_t& aFlags) {
  nsDisplayItem* nextItem = aItem->GetAbove();
  while (nextItem && nextItem->GetType() == DisplayItemType::TYPE_BACKGROUND) {
    nextItem = nextItem->GetAbove();
  }
  if (nextItem && nextItem->Frame() == aItem->Frame() &&
      nextItem->GetType() == DisplayItemType::TYPE_BORDER) {
    aFlags |= nsCSSRendering::PAINTBG_WILL_PAINT_BORDER;
  }
}

LayerState nsDisplayBackgroundImage::GetLayerState(
    nsDisplayListBuilder* aBuilder, LayerManager* aManager,
    const ContainerLayerParameters& aParameters) {
  mImageFlags = aBuilder->GetBackgroundPaintFlags();
  CheckForBorderItem(this, mImageFlags);

  ImageLayerization shouldLayerize = ShouldCreateOwnLayer(aBuilder, aManager);
  if (shouldLayerize == NO_LAYER_NEEDED) {
    // We can skip the call to CanOptimizeToImageLayer if we don't want a
    // layer anyway.
    return LayerState::LAYER_NONE;
  }

  if (CanOptimizeToImageLayer(aManager, aBuilder)) {
    if (shouldLayerize == WHENEVER_POSSIBLE) {
      return LayerState::LAYER_ACTIVE;
    }

    MOZ_ASSERT(shouldLayerize == ONLY_FOR_SCALING,
               "unhandled ImageLayerization value?");

    MOZ_ASSERT(mImage);
    int32_t imageWidth;
    int32_t imageHeight;
    mImage->GetWidth(&imageWidth);
    mImage->GetHeight(&imageHeight);
    NS_ASSERTION(imageWidth != 0 && imageHeight != 0, "Invalid image size!");

    int32_t appUnitsPerDevPixel = mFrame->PresContext()->AppUnitsPerDevPixel();
    LayoutDeviceRect destRect =
        LayoutDeviceRect::FromAppUnits(GetDestRect(), appUnitsPerDevPixel);

    const LayerRect destLayerRect = destRect * aParameters.Scale();

    // Calculate the scaling factor for the frame.
    const gfxSize scale = gfxSize(destLayerRect.width / imageWidth,
                                  destLayerRect.height / imageHeight);

    if ((scale.width != 1.0f || scale.height != 1.0f) &&
        (destLayerRect.width * destLayerRect.height >= 64 * 64)) {
      // Separate this image into a layer.
      // There's no point in doing this if we are not scaling at all or if the
      // target size is pretty small.
      return LayerState::LAYER_ACTIVE;
    }
  }

  return LayerState::LAYER_NONE;
}

already_AddRefed<Layer> nsDisplayBackgroundImage::BuildLayer(
    nsDisplayListBuilder* aBuilder, LayerManager* aManager,
    const ContainerLayerParameters& aParameters) {
  RefPtr<ImageLayer> layer = static_cast<ImageLayer*>(
      aManager->GetLayerBuilder()->GetLeafLayerFor(aBuilder, this));
  if (!layer) {
    layer = aManager->CreateImageLayer();
    if (!layer) {
      return nullptr;
    }
  }
  RefPtr<ImageContainer> imageContainer = GetContainer(aManager, aBuilder);
  layer->SetContainer(imageContainer);
  ConfigureLayer(layer, aParameters);
  return layer.forget();
}

bool nsDisplayBackgroundImage::CanBuildWebRenderDisplayItems(
    LayerManager* aManager, nsDisplayListBuilder* aDisplayListBuilder) {
  if (aDisplayListBuilder) {
    mImageFlags = aDisplayListBuilder->GetBackgroundPaintFlags();
  }

  return mBackgroundStyle->StyleBackground()->mImage.mLayers[mLayer].mClip !=
             StyleGeometryBox::Text &&
         nsCSSRendering::CanBuildWebRenderDisplayItemsForStyleImageLayer(
             aManager, *StyleFrame()->PresContext(), StyleFrame(),
             mBackgroundStyle->StyleBackground(), mLayer, mImageFlags);
}

bool nsDisplayBackgroundImage::CreateWebRenderCommands(
    mozilla::wr::DisplayListBuilder& aBuilder,
    mozilla::wr::IpcResourceUpdateQueue& aResources,
    const StackingContextHelper& aSc, RenderRootStateManager* aManager,
    nsDisplayListBuilder* aDisplayListBuilder) {
  if (!CanBuildWebRenderDisplayItems(aManager->LayerManager(),
                                     aDisplayListBuilder)) {
    return false;
  }

  CheckForBorderItem(this, mImageFlags);
  nsCSSRendering::PaintBGParams params =
      nsCSSRendering::PaintBGParams::ForSingleLayer(
          *StyleFrame()->PresContext(), GetPaintRect(), mBackgroundRect,
          StyleFrame(), mImageFlags, mLayer, CompositionOp::OP_OVER);
  params.bgClipRect = &mBounds;
  ImgDrawResult result =
      nsCSSRendering::BuildWebRenderDisplayItemsForStyleImageLayer(
          params, aBuilder, aResources, aSc, aManager, this);
  if (result == ImgDrawResult::NOT_SUPPORTED) {
    return false;
  }

  nsDisplayBackgroundGeometry::UpdateDrawResult(this, result);
  return true;
}

void nsDisplayBackgroundImage::HitTest(nsDisplayListBuilder* aBuilder,
                                       const nsRect& aRect,
                                       HitTestState* aState,
                                       nsTArray<nsIFrame*>* aOutFrames) {
  if (RoundedBorderIntersectsRect(mFrame, ToReferenceFrame(), aRect)) {
    aOutFrames->AppendElement(mFrame);
  }
}

bool nsDisplayBackgroundImage::ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                                 nsRegion* aVisibleRegion) {
  if (!nsDisplayImageContainer::ComputeVisibility(aBuilder, aVisibleRegion)) {
    return false;
  }

  // Return false if the background was propagated away from this
  // frame. We don't want this display item to show up and confuse
  // anything.
  return mBackgroundStyle;
}

/* static */
nsRegion nsDisplayBackgroundImage::GetInsideClipRegion(
    const nsDisplayItem* aItem, StyleGeometryBox aClip, const nsRect& aRect,
    const nsRect& aBackgroundRect) {
  nsRegion result;
  if (aRect.IsEmpty()) {
    return result;
  }

  nsIFrame* frame = aItem->Frame();

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

nsRegion nsDisplayBackgroundImage::GetOpaqueRegion(
    nsDisplayListBuilder* aBuilder, bool* aSnap) const {
  nsRegion result;
  *aSnap = false;

  if (!mBackgroundStyle) {
    return result;
  }

  *aSnap = true;

  // For StyleBoxDecorationBreak::Slice, don't try to optimize here, since
  // this could easily lead to O(N^2) behavior inside InlineBackgroundData,
  // which expects frames to be sent to it in content order, not reverse
  // content order which we'll produce here.
  // Of course, if there's only one frame in the flow, it doesn't matter.
  if (mFrame->StyleBorder()->mBoxDecorationBreak ==
          StyleBoxDecorationBreak::Clone ||
      (!mFrame->GetPrevContinuation() && !mFrame->GetNextContinuation())) {
    const nsStyleImageLayers::Layer& layer =
        mBackgroundStyle->StyleBackground()->mImage.mLayers[mLayer];
    if (layer.mImage.IsOpaque() && layer.mBlendMode == StyleBlend::Normal &&
        layer.mRepeat.mXRepeat != StyleImageLayerRepeat::Space &&
        layer.mRepeat.mYRepeat != StyleImageLayerRepeat::Space &&
        layer.mClip != StyleGeometryBox::Text) {
      result = GetInsideClipRegion(this, layer.mClip, mBounds, mBackgroundRect);
    }
  }

  return result;
}

Maybe<nscolor> nsDisplayBackgroundImage::IsUniform(
    nsDisplayListBuilder* aBuilder) const {
  if (!mBackgroundStyle) {
    return Some(NS_RGBA(0, 0, 0, 0));
  }
  return Nothing();
}

nsRect nsDisplayBackgroundImage::GetPositioningArea() const {
  if (!mBackgroundStyle) {
    return nsRect();
  }
  nsIFrame* attachedToFrame;
  bool transformedFixed;
  return nsCSSRendering::ComputeImageLayerPositioningArea(
             mFrame->PresContext(), mFrame, mBackgroundRect,
             mBackgroundStyle->StyleBackground()->mImage.mLayers[mLayer],
             &attachedToFrame, &transformedFixed) +
         ToReferenceFrame();
}

bool nsDisplayBackgroundImage::RenderingMightDependOnPositioningAreaSizeChange()
    const {
  if (!mBackgroundStyle) {
    return false;
  }

  nscoord radii[8];
  if (mFrame->GetBorderRadii(radii)) {
    // A change in the size of the positioning area might change the position
    // of the rounded corners.
    return true;
  }

  const nsStyleImageLayers::Layer& layer =
      mBackgroundStyle->StyleBackground()->mImage.mLayers[mLayer];
  if (layer.RenderingMightDependOnPositioningAreaSizeChange()) {
    return true;
  }
  return false;
}

void nsDisplayBackgroundImage::Paint(nsDisplayListBuilder* aBuilder,
                                     gfxContext* aCtx) {
  PaintInternal(aBuilder, aCtx, GetPaintRect(), &mBounds);
}

void nsDisplayBackgroundImage::PaintInternal(nsDisplayListBuilder* aBuilder,
                                             gfxContext* aCtx,
                                             const nsRect& aBounds,
                                             nsRect* aClipRect) {
  gfxContext* ctx = aCtx;
  StyleGeometryBox clip =
      mBackgroundStyle->StyleBackground()->mImage.mLayers[mLayer].mClip;

  if (clip == StyleGeometryBox::Text) {
    if (!GenerateAndPushTextMask(StyleFrame(), aCtx, mBackgroundRect,
                                 aBuilder)) {
      return;
    }
  }

  nsCSSRendering::PaintBGParams params =
      nsCSSRendering::PaintBGParams::ForSingleLayer(
          *StyleFrame()->PresContext(), aBounds, mBackgroundRect, StyleFrame(),
          mImageFlags, mLayer, CompositionOp::OP_OVER);
  params.bgClipRect = aClipRect;
  ImgDrawResult result = nsCSSRendering::PaintStyleImageLayer(params, *aCtx);

  if (clip == StyleGeometryBox::Text) {
    ctx->PopGroupAndBlend();
  }

  nsDisplayBackgroundGeometry::UpdateDrawResult(this, result);
}

void nsDisplayBackgroundImage::ComputeInvalidationRegion(
    nsDisplayListBuilder* aBuilder, const nsDisplayItemGeometry* aGeometry,
    nsRegion* aInvalidRegion) const {
  if (!mBackgroundStyle) {
    return;
  }

  auto* geometry = static_cast<const nsDisplayBackgroundGeometry*>(aGeometry);

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
    const auto& image =
        mBackgroundStyle->StyleBackground()->mImage.mLayers[mLayer].mImage;
    if (image.IsImageRequestType() &&
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

nsRect nsDisplayBackgroundImage::GetBounds(nsDisplayListBuilder* aBuilder,
                                           bool* aSnap) const {
  *aSnap = true;
  return mBounds;
}

nsRect nsDisplayBackgroundImage::GetBoundsInternal(
    nsDisplayListBuilder* aBuilder, nsIFrame* aFrameForBounds) {
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
  const nsStyleImageLayers::Layer& layer =
      mBackgroundStyle->StyleBackground()->mImage.mLayers[mLayer];
  return nsCSSRendering::GetBackgroundLayerRect(
      presContext, frame, mBackgroundRect, clipRect, layer,
      aBuilder->GetBackgroundPaintFlags());
}

nsDisplayTableBackgroundImage::nsDisplayTableBackgroundImage(
    nsDisplayListBuilder* aBuilder, nsIFrame* aFrame, const InitData& aData,
    nsIFrame* aCellFrame)
    : nsDisplayBackgroundImage(aBuilder, aFrame, aData, aCellFrame),
      mStyleFrame(aCellFrame),
      mTableType(GetTableTypeFromFrame(mStyleFrame)) {
  if (aBuilder->IsRetainingDisplayList()) {
    mStyleFrame->AddDisplayItem(this);
  }
}

nsDisplayTableBackgroundImage::~nsDisplayTableBackgroundImage() {
  if (mStyleFrame) {
    mStyleFrame->RemoveDisplayItem(this);
  }
}

bool nsDisplayTableBackgroundImage::IsInvalid(nsRect& aRect) const {
  bool result = mStyleFrame ? mStyleFrame->IsInvalid(aRect) : false;
  aRect += ToReferenceFrame();
  return result;
}

nsDisplayThemedBackground::nsDisplayThemedBackground(
    nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
    const nsRect& aBackgroundRect)
    : nsPaintedDisplayItem(aBuilder, aFrame), mBackgroundRect(aBackgroundRect) {
  MOZ_COUNT_CTOR(nsDisplayThemedBackground);
}

void nsDisplayThemedBackground::Init(nsDisplayListBuilder* aBuilder) {
  const nsStyleDisplay* disp = StyleFrame()->StyleDisplay();
  mAppearance = disp->mAppearance;
  StyleFrame()->IsThemed(disp, &mThemeTransparency);

  // Perform necessary RegisterThemeGeometry
  nsITheme* theme = StyleFrame()->PresContext()->Theme();
  nsITheme::ThemeGeometryType type =
      theme->ThemeGeometryTypeForWidget(StyleFrame(), disp->mAppearance);
  if (type != nsITheme::eThemeGeometryTypeUnknown) {
    RegisterThemeGeometry(aBuilder, this, StyleFrame(), type);
  }

  if (disp->mAppearance == StyleAppearance::MozWinBorderlessGlass ||
      disp->mAppearance == StyleAppearance::MozWinGlass) {
    aBuilder->SetGlassDisplayItem(this);
  }

  mBounds = GetBoundsInternal();
}

void nsDisplayThemedBackground::WriteDebugInfo(std::stringstream& aStream) {
  aStream << " (themed, appearance:" << (int)mAppearance << ")";
}

void nsDisplayThemedBackground::HitTest(nsDisplayListBuilder* aBuilder,
                                        const nsRect& aRect,
                                        HitTestState* aState,
                                        nsTArray<nsIFrame*>* aOutFrames) {
  // Assume that any point in our background rect is a hit.
  if (mBackgroundRect.Intersects(aRect)) {
    aOutFrames->AppendElement(mFrame);
  }
}

nsRegion nsDisplayThemedBackground::GetOpaqueRegion(
    nsDisplayListBuilder* aBuilder, bool* aSnap) const {
  nsRegion result;
  *aSnap = false;

  if (mThemeTransparency == nsITheme::eOpaque) {
    *aSnap = true;
    result = mBackgroundRect;
  }
  return result;
}

Maybe<nscolor> nsDisplayThemedBackground::IsUniform(
    nsDisplayListBuilder* aBuilder) const {
  if (mAppearance == StyleAppearance::MozWinBorderlessGlass ||
      mAppearance == StyleAppearance::MozWinGlass) {
    return Some(NS_RGBA(0, 0, 0, 0));
  }
  return Nothing();
}

nsRect nsDisplayThemedBackground::GetPositioningArea() const {
  return mBackgroundRect;
}

void nsDisplayThemedBackground::Paint(nsDisplayListBuilder* aBuilder,
                                      gfxContext* aCtx) {
  PaintInternal(aBuilder, aCtx, GetPaintRect(), nullptr);
}

void nsDisplayThemedBackground::PaintInternal(nsDisplayListBuilder* aBuilder,
                                              gfxContext* aCtx,
                                              const nsRect& aBounds,
                                              nsRect* aClipRect) {
  // XXXzw this ignores aClipRect.
  nsPresContext* presContext = StyleFrame()->PresContext();
  nsITheme* theme = presContext->Theme();
  nsRect drawing(mBackgroundRect);
  theme->GetWidgetOverflow(presContext->DeviceContext(), StyleFrame(),
                           mAppearance, &drawing);
  drawing.IntersectRect(drawing, aBounds);
  theme->DrawWidgetBackground(aCtx, StyleFrame(), mAppearance, mBackgroundRect,
                              drawing);
}

bool nsDisplayThemedBackground::CreateWebRenderCommands(
    mozilla::wr::DisplayListBuilder& aBuilder,
    mozilla::wr::IpcResourceUpdateQueue& aResources,
    const StackingContextHelper& aSc,
    mozilla::layers::RenderRootStateManager* aManager,
    nsDisplayListBuilder* aDisplayListBuilder) {
  nsITheme* theme = StyleFrame()->PresContext()->Theme();
  return theme->CreateWebRenderCommandsForWidget(aBuilder, aResources, aSc,
                                                 aManager, StyleFrame(),
                                                 mAppearance, mBackgroundRect);
}

bool nsDisplayThemedBackground::IsWindowActive() const {
  EventStates docState = mFrame->GetContent()->OwnerDoc()->GetDocumentState();
  return !docState.HasState(NS_DOCUMENT_STATE_WINDOW_INACTIVE);
}

void nsDisplayThemedBackground::ComputeInvalidationRegion(
    nsDisplayListBuilder* aBuilder, const nsDisplayItemGeometry* aGeometry,
    nsRegion* aInvalidRegion) const {
  auto* geometry =
      static_cast<const nsDisplayThemedBackgroundGeometry*>(aGeometry);

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
  nsITheme* theme = StyleFrame()->PresContext()->Theme();
  if (theme->WidgetAppearanceDependsOnWindowFocus(mAppearance) &&
      IsWindowActive() != geometry->mWindowIsActive) {
    aInvalidRegion->Or(*aInvalidRegion, bounds);
  }
}

nsRect nsDisplayThemedBackground::GetBounds(nsDisplayListBuilder* aBuilder,
                                            bool* aSnap) const {
  *aSnap = true;
  return mBounds;
}

nsRect nsDisplayThemedBackground::GetBoundsInternal() {
  nsPresContext* presContext = mFrame->PresContext();

  nsRect r = mBackgroundRect - ToReferenceFrame();
  presContext->Theme()->GetWidgetOverflow(presContext->DeviceContext(), mFrame,
                                          mFrame->StyleDisplay()->mAppearance,
                                          &r);
  return r + ToReferenceFrame();
}

void nsDisplayImageContainer::ConfigureLayer(
    ImageLayer* aLayer, const ContainerLayerParameters& aParameters) {
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
  Matrix transform = Matrix::Translation(p.x + aParameters.mOffset.x,
                                         p.y + aParameters.mOffset.y);
  transform.PreScale(destRect.width / containerSize.width,
                     destRect.height / containerSize.height);
  aLayer->SetBaseTransform(gfx::Matrix4x4::From2D(transform));
}

already_AddRefed<ImageContainer> nsDisplayImageContainer::GetContainer(
    LayerManager* aManager, nsDisplayListBuilder* aBuilder) {
  nsCOMPtr<imgIContainer> image = GetImage();
  if (!image) {
    MOZ_ASSERT_UNREACHABLE(
        "Must call CanOptimizeToImage() and get true "
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

bool nsDisplayImageContainer::CanOptimizeToImageLayer(
    LayerManager* aManager, nsDisplayListBuilder* aBuilder) {
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
  const gfxSize scale =
      gfxSize(destRect.width / imageWidth, destRect.height / imageHeight);

  if (scale.width < 0.34 || scale.height < 0.34) {
    // This would look awful as long as we can't use high-quality downscaling
    // for image layers (bug 803703), so don't turn this into an image layer.
    return false;
  }

  if (mFrame->IsImageFrame() || mFrame->IsImageControlFrame()) {
    // Image layer doesn't support draw focus ring for image map.
    nsImageFrame* f = static_cast<nsImageFrame*>(mFrame);
    if (f->HasImageMap()) {
      return false;
    }
  }

  return true;
}

void nsDisplayBackgroundColor::ApplyOpacity(nsDisplayListBuilder* aBuilder,
                                            float aOpacity,
                                            const DisplayItemClipChain* aClip) {
  NS_ASSERTION(CanApplyOpacity(), "ApplyOpacity should be allowed");
  mColor.a = mColor.a * aOpacity;
  IntersectClip(aBuilder, aClip, false);
}

bool nsDisplayBackgroundColor::CanApplyOpacity() const {
  // Don't apply opacity if the background color is animated since the color is
  // going to be changed on the compositor.
  return !EffectCompositor::HasAnimationsForCompositor(
      mFrame, DisplayItemType::TYPE_BACKGROUND_COLOR);
}

LayerState nsDisplayBackgroundColor::GetLayerState(
    nsDisplayListBuilder* aBuilder, LayerManager* aManager,
    const ContainerLayerParameters& aParameters) {
  if (ForceActiveLayers() && !HasBackgroundClipText()) {
    return LayerState::LAYER_ACTIVE;
  }

  if (EffectCompositor::HasAnimationsForCompositor(
          mFrame, DisplayItemType::TYPE_BACKGROUND_COLOR)) {
    return LayerState::LAYER_ACTIVE_FORCE;
  }

  return LayerState::LAYER_NONE;
}

already_AddRefed<Layer> nsDisplayBackgroundColor::BuildLayer(
    nsDisplayListBuilder* aBuilder, LayerManager* aManager,
    const ContainerLayerParameters& aContainerParameters) {
  if (mColor == sRGBColor()) {
    return nullptr;
  }

  RefPtr<ColorLayer> layer = static_cast<ColorLayer*>(
      aManager->GetLayerBuilder()->GetLeafLayerFor(aBuilder, this));
  if (!layer) {
    layer = aManager->CreateColorLayer();
    if (!layer) {
      return nullptr;
    }
  }
  layer->SetColor(ToDeviceColor(mColor));

  int32_t appUnitsPerDevPixel = mFrame->PresContext()->AppUnitsPerDevPixel();
  layer->SetBounds(mBackgroundRect.ToNearestPixels(appUnitsPerDevPixel));
  layer->SetBaseTransform(gfx::Matrix4x4::Translation(
      aContainerParameters.mOffset.x, aContainerParameters.mOffset.y, 0));

  // Both nsDisplayBackgroundColor and nsDisplayTableBackgroundColor use this
  // function, but only nsDisplayBackgroundColor supports compositor animations.
  if (GetType() == DisplayItemType::TYPE_BACKGROUND_COLOR) {
    nsDisplayListBuilder::AddAnimationsAndTransitionsToLayer(
        layer, aBuilder, this, mFrame, GetType());
  }
  return layer.forget();
}

bool nsDisplayBackgroundColor::CreateWebRenderCommands(
    mozilla::wr::DisplayListBuilder& aBuilder,
    mozilla::wr::IpcResourceUpdateQueue& aResources,
    const StackingContextHelper& aSc,
    mozilla::layers::RenderRootStateManager* aManager,
    nsDisplayListBuilder* aDisplayListBuilder) {
  if (mColor == sRGBColor()) {
    return true;
  }

  if (HasBackgroundClipText()) {
    return false;
  }

  uint64_t animationsId = 0;
  // We don't support background-color animations on table elements yet.
  if (GetType() == DisplayItemType::TYPE_BACKGROUND_COLOR) {
    animationsId = AddAnimationsForWebRender(
        this, aManager, aDisplayListBuilder, aBuilder.GetRenderRoot());
  }

  LayoutDeviceRect bounds = LayoutDeviceRect::FromAppUnits(
      mBackgroundRect, mFrame->PresContext()->AppUnitsPerDevPixel());
  wr::LayoutRect r = wr::ToLayoutRect(bounds);

  if (animationsId) {
    wr::WrAnimationProperty prop{
        wr::WrAnimationType::BackgroundColor,
        animationsId,
    };
    aBuilder.PushRectWithAnimation(r, r, !BackfaceIsHidden(),
                                   wr::ToColorF(ToDeviceColor(mColor)), &prop);
  } else {
    aBuilder.StartGroup(this);
    aBuilder.PushRect(r, r, !BackfaceIsHidden(),
                      wr::ToColorF(ToDeviceColor(mColor)));
    aBuilder.FinishGroup();
  }

  return true;
}

void nsDisplayBackgroundColor::PaintWithClip(nsDisplayListBuilder* aBuilder,
                                             gfxContext* aCtx,
                                             const DisplayItemClip& aClip) {
  MOZ_ASSERT(!HasBackgroundClipText());

  if (mColor == sRGBColor()) {
    return;
  }

  nsRect fillRect = mBackgroundRect;
  if (aClip.HasClip()) {
    fillRect.IntersectRect(fillRect, aClip.GetClipRect());
  }

  DrawTarget* dt = aCtx->GetDrawTarget();
  int32_t A2D = mFrame->PresContext()->AppUnitsPerDevPixel();
  Rect bounds = ToRect(nsLayoutUtils::RectToGfxRect(fillRect, A2D));
  MaybeSnapToDevicePixels(bounds, *dt);
  ColorPattern fill(ToDeviceColor(mColor));

  if (aClip.GetRoundedRectCount()) {
    MOZ_ASSERT(aClip.GetRoundedRectCount() == 1);

    AutoTArray<DisplayItemClip::RoundedRect, 1> roundedRect;
    aClip.AppendRoundedRects(&roundedRect);

    bool pushedClip = false;
    if (!fillRect.Contains(roundedRect[0].mRect)) {
      dt->PushClipRect(bounds);
      pushedClip = true;
    }

    RectCornerRadii pixelRadii;
    nsCSSRendering::ComputePixelRadii(roundedRect[0].mRadii, A2D, &pixelRadii);
    dt->FillRoundedRect(
        RoundedRect(NSRectToSnappedRect(roundedRect[0].mRect, A2D, *dt),
                    pixelRadii),
        fill);
    if (pushedClip) {
      dt->PopClip();
    }
  } else {
    dt->FillRect(bounds, fill);
  }
}

void nsDisplayBackgroundColor::Paint(nsDisplayListBuilder* aBuilder,
                                     gfxContext* aCtx) {
  if (mColor == sRGBColor()) {
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
  gfxRect bounds = nsLayoutUtils::RectToGfxRect(
      mBackgroundRect, mFrame->PresContext()->AppUnitsPerDevPixel());

  if (HasBackgroundClipText()) {
    if (!GenerateAndPushTextMask(mFrame, aCtx, mBackgroundRect, aBuilder)) {
      return;
    }

    ctx->SetColor(mColor);
    ctx->NewPath();
    ctx->SnappedRectangle(bounds);
    ctx->Fill();
    ctx->PopGroupAndBlend();
    return;
  }

  ctx->SetColor(mColor);
  ctx->NewPath();
  ctx->SnappedRectangle(bounds);
  ctx->Fill();
#endif
}

nsRegion nsDisplayBackgroundColor::GetOpaqueRegion(
    nsDisplayListBuilder* aBuilder, bool* aSnap) const {
  *aSnap = false;

  if (mColor.a != 1 ||
      // Even if the current alpha channel is 1, we treat this item as if it's
      // non-opaque if there is a background-color animation since the animation
      // might change the alpha channel.
      EffectCompositor::HasAnimationsForCompositor(
          mFrame, DisplayItemType::TYPE_BACKGROUND_COLOR)) {
    return nsRegion();
  }

  if (!mHasStyle || HasBackgroundClipText()) {
    return nsRegion();
  }

  *aSnap = true;
  return nsDisplayBackgroundImage::GetInsideClipRegion(
      this, mBottomLayerClip, mBackgroundRect, mBackgroundRect);
}

Maybe<nscolor> nsDisplayBackgroundColor::IsUniform(
    nsDisplayListBuilder* aBuilder) const {
  return Some(mColor.ToABGR());
}

void nsDisplayBackgroundColor::HitTest(nsDisplayListBuilder* aBuilder,
                                       const nsRect& aRect,
                                       HitTestState* aState,
                                       nsTArray<nsIFrame*>* aOutFrames) {
  if (!RoundedBorderIntersectsRect(mFrame, ToReferenceFrame(), aRect)) {
    // aRect doesn't intersect our border-radius curve.
    return;
  }

  aOutFrames->AppendElement(mFrame);
}

void nsDisplayBackgroundColor::WriteDebugInfo(std::stringstream& aStream) {
  aStream << " (rgba " << mColor.r << "," << mColor.g << "," << mColor.b << ","
          << mColor.a << ")";
  aStream << " backgroundRect" << mBackgroundRect;
}

already_AddRefed<Layer> nsDisplayClearBackground::BuildLayer(
    nsDisplayListBuilder* aBuilder, LayerManager* aManager,
    const ContainerLayerParameters& aParameters) {
  RefPtr<ColorLayer> layer = static_cast<ColorLayer*>(
      aManager->GetLayerBuilder()->GetLeafLayerFor(aBuilder, this));
  if (!layer) {
    layer = aManager->CreateColorLayer();
    if (!layer) {
      return nullptr;
    }
  }
  layer->SetColor(DeviceColor());
  layer->SetMixBlendMode(gfx::CompositionOp::OP_SOURCE);

  bool snap;
  nsRect bounds = GetBounds(aBuilder, &snap);
  int32_t appUnitsPerDevPixel = mFrame->PresContext()->AppUnitsPerDevPixel();
  layer->SetBounds(bounds.ToNearestPixels(appUnitsPerDevPixel));  // XXX Do we
                                                                  // need to
                                                                  // respect the
                                                                  // parent
                                                                  // layer's
                                                                  // scale here?

  return layer.forget();
}

bool nsDisplayClearBackground::CreateWebRenderCommands(
    mozilla::wr::DisplayListBuilder& aBuilder,
    mozilla::wr::IpcResourceUpdateQueue& aResources,
    const StackingContextHelper& aSc,
    mozilla::layers::RenderRootStateManager* aManager,
    nsDisplayListBuilder* aDisplayListBuilder) {
  LayoutDeviceRect bounds = LayoutDeviceRect::FromAppUnits(
      nsRect(ToReferenceFrame(), mFrame->GetSize()),
      mFrame->PresContext()->AppUnitsPerDevPixel());

  aBuilder.PushClearRect(wr::ToLayoutRect(bounds));

  return true;
}

nsRect nsDisplayOutline::GetBounds(nsDisplayListBuilder* aBuilder,
                                   bool* aSnap) const {
  *aSnap = false;
  return mFrame->GetVisualOverflowRectRelativeToSelf() + ToReferenceFrame();
}

void nsDisplayOutline::Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx) {
  // TODO join outlines together
  MOZ_ASSERT(mFrame->StyleOutline()->ShouldPaintOutline(),
             "Should have not created a nsDisplayOutline!");

  nsPoint offset = ToReferenceFrame();
  nsCSSRendering::PaintOutline(
      mFrame->PresContext(), *aCtx, mFrame, GetPaintRect(),
      nsRect(offset, mFrame->GetSize()), mFrame->Style());
}

bool nsDisplayOutline::IsThemedOutline() const {
  const auto& outlineStyle = mFrame->StyleOutline()->mOutlineStyle;
  if (!outlineStyle.IsAuto() ||
      !StaticPrefs::layout_css_outline_style_auto_enabled()) {
    return false;
  }

  nsPresContext* pc = mFrame->PresContext();
  nsITheme* theme = pc->Theme();
  return theme->ThemeSupportsWidget(pc, mFrame, StyleAppearance::FocusOutline);
}

bool nsDisplayOutline::CreateWebRenderCommands(
    mozilla::wr::DisplayListBuilder& aBuilder,
    mozilla::wr::IpcResourceUpdateQueue& aResources,
    const StackingContextHelper& aSc,
    mozilla::layers::RenderRootStateManager* aManager,
    nsDisplayListBuilder* aDisplayListBuilder) {
  if (IsThemedOutline()) {
    return false;
  }

  nsPoint offset = ToReferenceFrame();

  mozilla::Maybe<nsCSSBorderRenderer> borderRenderer =
      nsCSSRendering::CreateBorderRendererForOutline(
          mFrame->PresContext(), nullptr, mFrame, GetPaintRect(),
          nsRect(offset, mFrame->GetSize()), mFrame->Style());

  if (!borderRenderer) {
    // No border renderer means "there is no outline".
    // Paint nothing and return success.
    return true;
  }

  borderRenderer->CreateWebRenderCommands(this, aBuilder, aResources, aSc);
  return true;
}

bool nsDisplayOutline::IsInvisibleInRect(const nsRect& aRect) const {
  const nsStyleOutline* outline = mFrame->StyleOutline();
  nsRect borderBox(ToReferenceFrame(), mFrame->GetSize());
  if (borderBox.Contains(aRect) &&
      !nsLayoutUtils::HasNonZeroCorner(outline->mOutlineRadius)) {
    if (outline->mOutlineOffset._0 >= 0.0f) {
      // aRect is entirely inside the border-rect, and the outline isn't
      // rendered inside the border-rect, so the outline is not visible.
      return true;
    }
  }

  return false;
}

void nsDisplayEventReceiver::HitTest(nsDisplayListBuilder* aBuilder,
                                     const nsRect& aRect, HitTestState* aState,
                                     nsTArray<nsIFrame*>* aOutFrames) {
  if (!RoundedBorderIntersectsRect(mFrame, ToReferenceFrame(), aRect)) {
    // aRect doesn't intersect our border-radius curve.
    return;
  }

  aOutFrames->AppendElement(mFrame);
}

nsDisplayCompositorHitTestInfo::nsDisplayCompositorHitTestInfo(
    nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
    const mozilla::gfx::CompositorHitTestInfo& aHitTestFlags, uint16_t aIndex,
    const mozilla::Maybe<nsRect>& aArea)
    : nsDisplayHitTestInfoBase(aBuilder, aFrame),
      mIndex(aIndex),
      mAppUnitsPerDevPixel(mFrame->PresContext()->AppUnitsPerDevPixel()) {
  MOZ_COUNT_CTOR(nsDisplayCompositorHitTestInfo);
  // We should never even create this display item if we're not building
  // compositor hit-test info or if the computed hit info indicated the
  // frame is invisible to hit-testing
  MOZ_ASSERT(aBuilder->BuildCompositorHitTestInfo());
  MOZ_ASSERT(aHitTestFlags != CompositorHitTestInvisibleToHit);

  const nsRect& area =
      aArea.isSome() ? *aArea : aFrame->GetCompositorHitTestArea(aBuilder);

  SetHitTestInfo(area, aHitTestFlags);
  InitializeScrollTarget(aBuilder);
}

nsDisplayCompositorHitTestInfo::nsDisplayCompositorHitTestInfo(
    nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
    mozilla::UniquePtr<HitTestInfo>&& aHitTestInfo)
    : nsDisplayHitTestInfoBase(aBuilder, aFrame),
      mIndex(0),
      mAppUnitsPerDevPixel(mFrame->PresContext()->AppUnitsPerDevPixel()) {
  MOZ_COUNT_CTOR(nsDisplayCompositorHitTestInfo);
  SetHitTestInfo(std::move(aHitTestInfo));
  InitializeScrollTarget(aBuilder);
}

void nsDisplayCompositorHitTestInfo::InitializeScrollTarget(
    nsDisplayListBuilder* aBuilder) {
  if (aBuilder->GetCurrentScrollbarDirection().isSome()) {
    // In the case of scrollbar frames, we use the scrollbar's target
    // scrollframe instead of the scrollframe with which the scrollbar actually
    // moves.
    MOZ_ASSERT(HitTestFlags().contains(CompositorHitTestFlags::eScrollbar));
    mScrollTarget = mozilla::Some(aBuilder->GetCurrentScrollbarTarget());
  }
}

bool nsDisplayCompositorHitTestInfo::CreateWebRenderCommands(
    mozilla::wr::DisplayListBuilder& aBuilder,
    mozilla::wr::IpcResourceUpdateQueue& aResources,
    const StackingContextHelper& aSc,
    mozilla::layers::RenderRootStateManager* aManager,
    nsDisplayListBuilder* aDisplayListBuilder) {
  if (HitTestArea().IsEmpty()) {
    return true;
  }

  // XXX: eventually this scrollId computation and the SetHitTestInfo
  // call will get moved out into the WR display item iteration code so that
  // we don't need to do it as often, and so that we can do it for other
  // display item types as well (reducing the need for as many instances of
  // this display item).
  ScrollableLayerGuid::ViewID scrollId =
      mScrollTarget.valueOrFrom([&]() -> ScrollableLayerGuid::ViewID {
        const ActiveScrolledRoot* asr = GetActiveScrolledRoot();
        Maybe<ScrollableLayerGuid::ViewID> fixedTarget =
            aBuilder.GetContainingFixedPosScrollTarget(asr);
        if (fixedTarget) {
          return *fixedTarget;
        }
        if (asr) {
          return asr->GetViewId();
        }
        return ScrollableLayerGuid::NULL_SCROLL_ID;
      });

  Maybe<SideBits> sideBits =
      aBuilder.GetContainingFixedPosSideBits(GetActiveScrolledRoot());

  // Insert a transparent rectangle with the hit-test info
  aBuilder.SetHitTestInfo(scrollId, HitTestFlags(),
                          sideBits.valueOr(SideBits::eNone));

  const LayoutDeviceRect devRect =
      LayoutDeviceRect::FromAppUnits(HitTestArea(), mAppUnitsPerDevPixel);

  const wr::LayoutRect rect = wr::ToLayoutRect(devRect);

  aBuilder.StartGroup(this);
  aBuilder.PushHitTest(rect, rect, !BackfaceIsHidden());
  aBuilder.FinishGroup();
  aBuilder.ClearHitTestInfo();

  return true;
}

uint16_t nsDisplayCompositorHitTestInfo::CalculatePerFrameKey() const {
  return mIndex;
}

int32_t nsDisplayCompositorHitTestInfo::ZIndex() const {
  return mOverrideZIndex ? *mOverrideZIndex
                         : nsDisplayHitTestInfoBase::ZIndex();
}

void nsDisplayCompositorHitTestInfo::SetOverrideZIndex(int32_t aZIndex) {
  mOverrideZIndex = Some(aZIndex);
}

nsDisplayCaret::nsDisplayCaret(nsDisplayListBuilder* aBuilder,
                               nsIFrame* aCaretFrame)
    : nsPaintedDisplayItem(aBuilder, aCaretFrame),
      mCaret(aBuilder->GetCaret()),
      mBounds(aBuilder->GetCaretRect() + ToReferenceFrame()) {
  MOZ_COUNT_CTOR(nsDisplayCaret);
}

#ifdef NS_BUILD_REFCNT_LOGGING
nsDisplayCaret::~nsDisplayCaret() { MOZ_COUNT_DTOR(nsDisplayCaret); }
#endif

nsRect nsDisplayCaret::GetBounds(nsDisplayListBuilder* aBuilder,
                                 bool* aSnap) const {
  *aSnap = true;
  // The caret returns a rect in the coordinates of mFrame.
  return mBounds;
}

void nsDisplayCaret::Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx) {
  // Note: Because we exist, we know that the caret is visible, so we don't
  // need to check for the caret's visibility.
  mCaret->PaintCaret(*aCtx->GetDrawTarget(), mFrame, ToReferenceFrame());
}

bool nsDisplayCaret::CreateWebRenderCommands(
    mozilla::wr::DisplayListBuilder& aBuilder,
    mozilla::wr::IpcResourceUpdateQueue& aResources,
    const StackingContextHelper& aSc,
    mozilla::layers::RenderRootStateManager* aManager,
    nsDisplayListBuilder* aDisplayListBuilder) {
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

  gfx::DeviceColor color = ToDeviceColor(frame->GetCaretColorAt(contentOffset));
  LayoutDeviceRect devCaretRect = LayoutDeviceRect::FromAppUnits(
      caretRect + ToReferenceFrame(), appUnitsPerDevPixel);
  LayoutDeviceRect devHookRect = LayoutDeviceRect::FromAppUnits(
      hookRect + ToReferenceFrame(), appUnitsPerDevPixel);

  wr::LayoutRect caret = wr::ToLayoutRect(devCaretRect);
  wr::LayoutRect hook = wr::ToLayoutRect(devHookRect);

  // Note, WR will pixel snap anything that is layout aligned.
  aBuilder.PushRect(caret, caret, !BackfaceIsHidden(), wr::ToColorF(color));

  if (!devHookRect.IsEmpty()) {
    aBuilder.PushRect(hook, hook, !BackfaceIsHidden(), wr::ToColorF(color));
  }
  return true;
}

nsDisplayBorder::nsDisplayBorder(nsDisplayListBuilder* aBuilder,
                                 nsIFrame* aFrame)
    : nsPaintedDisplayItem(aBuilder, aFrame) {
  MOZ_COUNT_CTOR(nsDisplayBorder);

  mBounds = CalculateBounds<nsRect>(*mFrame->StyleBorder());
}

bool nsDisplayBorder::IsInvisibleInRect(const nsRect& aRect) const {
  nsRect paddingRect = GetPaddingRect();
  const nsStyleBorder* styleBorder;
  if (paddingRect.Contains(aRect) &&
      !(styleBorder = mFrame->StyleBorder())->IsBorderImageSizeAvailable() &&
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

nsDisplayItemGeometry* nsDisplayBorder::AllocateGeometry(
    nsDisplayListBuilder* aBuilder) {
  return new nsDisplayBorderGeometry(this, aBuilder);
}

void nsDisplayBorder::ComputeInvalidationRegion(
    nsDisplayListBuilder* aBuilder, const nsDisplayItemGeometry* aGeometry,
    nsRegion* aInvalidRegion) const {
  auto* geometry = static_cast<const nsDisplayBorderGeometry*>(aGeometry);
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

LayerState nsDisplayBorder::GetLayerState(
    nsDisplayListBuilder* aBuilder, LayerManager* aManager,
    const ContainerLayerParameters& aParameters) {
  return LayerState::LAYER_NONE;
}

bool nsDisplayBorder::CreateWebRenderCommands(
    mozilla::wr::DisplayListBuilder& aBuilder,
    mozilla::wr::IpcResourceUpdateQueue& aResources,
    const StackingContextHelper& aSc,
    mozilla::layers::RenderRootStateManager* aManager,
    nsDisplayListBuilder* aDisplayListBuilder) {
  nsRect rect = nsRect(ToReferenceFrame(), mFrame->GetSize());

  aBuilder.StartGroup(this);
  ImgDrawResult drawResult = nsCSSRendering::CreateWebRenderCommandsForBorder(
      this, mFrame, rect, aBuilder, aResources, aSc, aManager,
      aDisplayListBuilder);

  if (drawResult == ImgDrawResult::NOT_SUPPORTED) {
    aBuilder.CancelGroup();
    return false;
  }

  aBuilder.FinishGroup();

  nsDisplayBorderGeometry::UpdateDrawResult(this, drawResult);
  return true;
};

void nsDisplayBorder::Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx) {
  nsPoint offset = ToReferenceFrame();

  PaintBorderFlags flags = aBuilder->ShouldSyncDecodeImages()
                               ? PaintBorderFlags::SyncDecodeImages
                               : PaintBorderFlags();

  ImgDrawResult result = nsCSSRendering::PaintBorder(
      mFrame->PresContext(), *aCtx, mFrame, GetPaintRect(),
      nsRect(offset, mFrame->GetSize()), mFrame->Style(), flags,
      mFrame->GetSkipSides());

  nsDisplayBorderGeometry::UpdateDrawResult(this, result);
}

nsRect nsDisplayBorder::GetBounds(nsDisplayListBuilder* aBuilder,
                                  bool* aSnap) const {
  *aSnap = true;
  return mBounds;
}

// Given a region, compute a conservative approximation to it as a list
// of rectangles that aren't vertically adjacent (i.e., vertically
// adjacent or overlapping rectangles are combined).
// Right now this is only approximate, some vertically overlapping rectangles
// aren't guaranteed to be combined.
static void ComputeDisjointRectangles(const nsRegion& aRegion,
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

void nsDisplayBoxShadowOuter::Paint(nsDisplayListBuilder* aBuilder,
                                    gfxContext* aCtx) {
  nsPoint offset = ToReferenceFrame();
  nsRect borderRect = mFrame->VisualBorderRectRelativeToSelf() + offset;
  nsPresContext* presContext = mFrame->PresContext();
  AutoTArray<nsRect, 10> rects;
  ComputeDisjointRectangles(mVisibleRegion, &rects);

  AUTO_PROFILER_LABEL("nsDisplayBoxShadowOuter::Paint", GRAPHICS);

  for (uint32_t i = 0; i < rects.Length(); ++i) {
    nsCSSRendering::PaintBoxShadowOuter(presContext, *aCtx, mFrame, borderRect,
                                        rects[i], mOpacity);
  }
}

nsRect nsDisplayBoxShadowOuter::GetBounds(nsDisplayListBuilder* aBuilder,
                                          bool* aSnap) const {
  *aSnap = false;
  return mBounds;
}

nsRect nsDisplayBoxShadowOuter::GetBoundsInternal() {
  return nsLayoutUtils::GetBoxShadowRectForFrame(mFrame, mFrame->GetSize()) +
         ToReferenceFrame();
}

bool nsDisplayBoxShadowOuter::IsInvisibleInRect(const nsRect& aRect) const {
  nsPoint origin = ToReferenceFrame();
  nsRect frameRect(origin, mFrame->GetSize());
  if (!frameRect.Contains(aRect)) {
    return false;
  }

  // the visible region is entirely inside the border-rect, and box shadows
  // never render within the border-rect (unless there's a border radius).
  nscoord twipsRadii[8];
  bool hasBorderRadii = mFrame->GetBorderRadii(twipsRadii);
  if (!hasBorderRadii) {
    return true;
  }

  return RoundedRectContainsRect(frameRect, twipsRadii, aRect);
}

bool nsDisplayBoxShadowOuter::ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                                nsRegion* aVisibleRegion) {
  if (!nsPaintedDisplayItem::ComputeVisibility(aBuilder, aVisibleRegion)) {
    return false;
  }

  mVisibleRegion.And(*aVisibleRegion, GetPaintRect());
  return true;
}

bool nsDisplayBoxShadowOuter::CanBuildWebRenderDisplayItems() {
  auto shadows = mFrame->StyleEffects()->mBoxShadow.AsSpan();
  if (shadows.IsEmpty()) {
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

bool nsDisplayBoxShadowOuter::CreateWebRenderCommands(
    mozilla::wr::DisplayListBuilder& aBuilder,
    mozilla::wr::IpcResourceUpdateQueue& aResources,
    const StackingContextHelper& aSc,
    mozilla::layers::RenderRootStateManager* aManager,
    nsDisplayListBuilder* aDisplayListBuilder) {
  if (!CanBuildWebRenderDisplayItems()) {
    return false;
  }

  int32_t appUnitsPerDevPixel = mFrame->PresContext()->AppUnitsPerDevPixel();
  nsPoint offset = ToReferenceFrame();
  nsRect borderRect = mFrame->VisualBorderRectRelativeToSelf() + offset;
  AutoTArray<nsRect, 10> rects;
  bool snap;
  nsRect bounds = GetBounds(aDisplayListBuilder, &snap);
  ComputeDisjointRectangles(bounds, &rects);

  bool hasBorderRadius;
  bool nativeTheme =
      nsCSSRendering::HasBoxShadowNativeTheme(mFrame, hasBorderRadius);

  // Don't need the full size of the shadow rect like we do in
  // nsCSSRendering since WR takes care of calculations for blur
  // and spread radius.
  nsRect frameRect =
      nsCSSRendering::GetShadowRect(borderRect, nativeTheme, mFrame);

  RectCornerRadii borderRadii;
  if (hasBorderRadius) {
    hasBorderRadius = nsCSSRendering::GetBorderRadii(frameRect, borderRect,
                                                     mFrame, borderRadii);
  }

  // Everything here is in app units, change to device units.
  for (uint32_t i = 0; i < rects.Length(); ++i) {
    LayoutDeviceRect clipRect =
        LayoutDeviceRect::FromAppUnits(rects[i], appUnitsPerDevPixel);
    auto shadows = mFrame->StyleEffects()->mBoxShadow.AsSpan();
    MOZ_ASSERT(!shadows.IsEmpty());

    for (auto& shadow : Reversed(shadows)) {
      if (shadow.inset) {
        continue;
      }

      float blurRadius =
          float(shadow.base.blur.ToAppUnits()) / float(appUnitsPerDevPixel);
      gfx::sRGBColor shadowColor =
          nsCSSRendering::GetShadowColor(shadow.base, mFrame, mOpacity);

      // We don't move the shadow rect here since WR does it for us
      // Now translate everything to device pixels.
      const nsRect& shadowRect = frameRect;
      LayoutDevicePoint shadowOffset = LayoutDevicePoint::FromAppUnits(
          nsPoint(shadow.base.horizontal.ToAppUnits(),
                  shadow.base.vertical.ToAppUnits()),
          appUnitsPerDevPixel);

      LayoutDeviceRect deviceBox =
          LayoutDeviceRect::FromAppUnits(shadowRect, appUnitsPerDevPixel);
      wr::LayoutRect deviceBoxRect = wr::ToLayoutRect(deviceBox);
      wr::LayoutRect deviceClipRect = wr::ToLayoutRect(clipRect);

      LayoutDeviceSize zeroSize;
      wr::BorderRadius borderRadius =
          wr::ToBorderRadius(zeroSize, zeroSize, zeroSize, zeroSize);
      if (hasBorderRadius) {
        borderRadius = wr::ToBorderRadius(
            LayoutDeviceSize::FromUnknownSize(borderRadii.TopLeft()),
            LayoutDeviceSize::FromUnknownSize(borderRadii.TopRight()),
            LayoutDeviceSize::FromUnknownSize(borderRadii.BottomLeft()),
            LayoutDeviceSize::FromUnknownSize(borderRadii.BottomRight()));
      }

      float spreadRadius =
          float(shadow.spread.ToAppUnits()) / float(appUnitsPerDevPixel);

      aBuilder.PushBoxShadow(deviceBoxRect, deviceClipRect, !BackfaceIsHidden(),
                             deviceBoxRect, wr::ToLayoutVector2D(shadowOffset),
                             wr::ToColorF(ToDeviceColor(shadowColor)),
                             blurRadius, spreadRadius, borderRadius,
                             wr::BoxShadowClipMode::Outset);
    }
  }

  return true;
}

void nsDisplayBoxShadowOuter::ComputeInvalidationRegion(
    nsDisplayListBuilder* aBuilder, const nsDisplayItemGeometry* aGeometry,
    nsRegion* aInvalidRegion) const {
  auto* geometry =
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

void nsDisplayBoxShadowInner::Paint(nsDisplayListBuilder* aBuilder,
                                    gfxContext* aCtx) {
  nsPoint offset = ToReferenceFrame();
  nsRect borderRect = nsRect(offset, mFrame->GetSize());
  nsPresContext* presContext = mFrame->PresContext();
  AutoTArray<nsRect, 10> rects;
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

bool nsDisplayBoxShadowInner::CanCreateWebRenderCommands(
    nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
    const nsPoint& aReferenceOffset) {
  auto shadows = aFrame->StyleEffects()->mBoxShadow.AsSpan();
  if (shadows.IsEmpty()) {
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

/* static */
void nsDisplayBoxShadowInner::CreateInsetBoxShadowWebRenderCommands(
    mozilla::wr::DisplayListBuilder& aBuilder, const StackingContextHelper& aSc,
    nsRegion& aVisibleRegion, nsIFrame* aFrame, const nsRect& aBorderRect) {
  if (!nsCSSRendering::ShouldPaintBoxShadowInner(aFrame)) {
    return;
  }

  int32_t appUnitsPerDevPixel = aFrame->PresContext()->AppUnitsPerDevPixel();

  AutoTArray<nsRect, 10> rects;
  ComputeDisjointRectangles(aVisibleRegion, &rects);

  auto shadows = aFrame->StyleEffects()->mBoxShadow.AsSpan();

  for (uint32_t i = 0; i < rects.Length(); ++i) {
    LayoutDeviceRect clipRect =
        LayoutDeviceRect::FromAppUnits(rects[i], appUnitsPerDevPixel);

    for (auto& shadow : Reversed(shadows)) {
      if (!shadow.inset) {
        continue;
      }

      nsRect shadowRect =
          nsCSSRendering::GetBoxShadowInnerPaddingRect(aFrame, aBorderRect);
      RectCornerRadii innerRadii;
      nsCSSRendering::GetShadowInnerRadii(aFrame, aBorderRect, innerRadii);

      // Now translate everything to device pixels.
      LayoutDeviceRect deviceBoxRect =
          LayoutDeviceRect::FromAppUnits(shadowRect, appUnitsPerDevPixel);
      wr::LayoutRect deviceClipRect = wr::ToLayoutRect(clipRect);
      sRGBColor shadowColor =
          nsCSSRendering::GetShadowColor(shadow.base, aFrame, 1.0);

      LayoutDevicePoint shadowOffset = LayoutDevicePoint::FromAppUnits(
          nsPoint(shadow.base.horizontal.ToAppUnits(),
                  shadow.base.vertical.ToAppUnits()),
          appUnitsPerDevPixel);

      float blurRadius =
          float(shadow.base.blur.ToAppUnits()) / float(appUnitsPerDevPixel);

      wr::BorderRadius borderRadius = wr::ToBorderRadius(
          LayoutDeviceSize::FromUnknownSize(innerRadii.TopLeft()),
          LayoutDeviceSize::FromUnknownSize(innerRadii.TopRight()),
          LayoutDeviceSize::FromUnknownSize(innerRadii.BottomLeft()),
          LayoutDeviceSize::FromUnknownSize(innerRadii.BottomRight()));
      // NOTE: Any spread radius > 0 will render nothing. WR Bug.
      float spreadRadius =
          float(shadow.spread.ToAppUnits()) / float(appUnitsPerDevPixel);

      aBuilder.PushBoxShadow(
          wr::ToLayoutRect(deviceBoxRect), deviceClipRect,
          !aFrame->BackfaceIsHidden(), wr::ToLayoutRect(deviceBoxRect),
          wr::ToLayoutVector2D(shadowOffset),
          wr::ToColorF(ToDeviceColor(shadowColor)), blurRadius, spreadRadius,
          borderRadius, wr::BoxShadowClipMode::Inset);
    }
  }
}

bool nsDisplayBoxShadowInner::CreateWebRenderCommands(
    mozilla::wr::DisplayListBuilder& aBuilder,
    mozilla::wr::IpcResourceUpdateQueue& aResources,
    const StackingContextHelper& aSc,
    mozilla::layers::RenderRootStateManager* aManager,
    nsDisplayListBuilder* aDisplayListBuilder) {
  if (!CanCreateWebRenderCommands(aDisplayListBuilder, mFrame,
                                  ToReferenceFrame())) {
    return false;
  }

  bool snap;
  nsRegion visible = GetBounds(aDisplayListBuilder, &snap);
  nsPoint offset = ToReferenceFrame();
  nsRect borderRect = nsRect(offset, mFrame->GetSize());
  nsDisplayBoxShadowInner::CreateInsetBoxShadowWebRenderCommands(
      aBuilder, aSc, visible, mFrame, borderRect);

  return true;
}

bool nsDisplayBoxShadowInner::ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                                nsRegion* aVisibleRegion) {
  if (!nsPaintedDisplayItem::ComputeVisibility(aBuilder, aVisibleRegion)) {
    return false;
  }

  mVisibleRegion.And(*aVisibleRegion, GetPaintRect());
  return true;
}

nsDisplayWrapList::nsDisplayWrapList(nsDisplayListBuilder* aBuilder,
                                     nsIFrame* aFrame, nsDisplayList* aList)
    : nsDisplayWrapList(aBuilder, aFrame, aList,
                        aBuilder->CurrentActiveScrolledRoot(), false, 0) {}

nsDisplayWrapList::nsDisplayWrapList(
    nsDisplayListBuilder* aBuilder, nsIFrame* aFrame, nsDisplayList* aList,
    const ActiveScrolledRoot* aActiveScrolledRoot, bool aClearClipChain,
    uint16_t aIndex)
    : nsDisplayHitTestInfoBase(aBuilder, aFrame, aActiveScrolledRoot),
      mFrameActiveScrolledRoot(aBuilder->CurrentActiveScrolledRoot()),
      mOverrideZIndex(0),
      mIndex(aIndex),
      mHasZIndexOverride(false),
      mClearingClipChain(aClearClipChain) {
  MOZ_COUNT_CTOR(nsDisplayWrapList);

  mBaseBuildingRect = GetBuildingRect();

  mListPtr = &mList;
  mListPtr->AppendToTop(aList);
  nsDisplayWrapList::UpdateBounds(aBuilder);

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
  nsDisplayItem* i = mListPtr->GetBottom();
  if (i &&
      (!i->GetAbove() || i->GetType() == DisplayItemType::TYPE_TRANSFORM) &&
      i->Frame() == mFrame) {
    mReferenceFrame = i->ReferenceFrame();
    mToReferenceFrame = i->ToReferenceFrame();
  }

  nsRect visible = aBuilder->GetVisibleRect() +
                   aBuilder->GetCurrentFrameOffsetToReferenceFrame();

  SetBuildingRect(visible);
}

nsDisplayWrapList::nsDisplayWrapList(nsDisplayListBuilder* aBuilder,
                                     nsIFrame* aFrame, nsDisplayItem* aItem)
    : nsDisplayHitTestInfoBase(aBuilder, aFrame,
                               aBuilder->CurrentActiveScrolledRoot()),
      mOverrideZIndex(0),
      mIndex(0),
      mHasZIndexOverride(false) {
  MOZ_COUNT_CTOR(nsDisplayWrapList);

  mBaseBuildingRect = GetBuildingRect();

  mListPtr = &mList;
  mListPtr->AppendToTop(aItem);
  nsDisplayWrapList::UpdateBounds(aBuilder);

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

nsDisplayWrapList::~nsDisplayWrapList() { MOZ_COUNT_DTOR(nsDisplayWrapList); }

void nsDisplayWrapList::MergeDisplayListFromItem(
    nsDisplayListBuilder* aBuilder, const nsDisplayWrapList* aItem) {
  const nsDisplayWrapList* wrappedItem = aItem->AsDisplayWrapList();
  MOZ_ASSERT(wrappedItem);

  // Create a new nsDisplayWrapList using a copy-constructor. This is done
  // to preserve the information about bounds.
  nsDisplayWrapList* wrapper =
      MakeClone<nsDisplayWrapList>(aBuilder, wrappedItem);
  MOZ_ASSERT(wrapper);

  // Set the display list pointer of the new wrapper item to the display list
  // of the wrapped item.
  wrapper->mListPtr = wrappedItem->mListPtr;

  mListPtr->AppendToBottom(wrapper);
}

void nsDisplayWrapList::HitTest(nsDisplayListBuilder* aBuilder,
                                const nsRect& aRect, HitTestState* aState,
                                nsTArray<nsIFrame*>* aOutFrames) {
  mListPtr->HitTest(aBuilder, aRect, aState, aOutFrames);
}

nsRect nsDisplayWrapList::GetBounds(nsDisplayListBuilder* aBuilder,
                                    bool* aSnap) const {
  *aSnap = false;
  return mBounds;
}

bool nsDisplayWrapList::ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                          nsRegion* aVisibleRegion) {
  return ::ComputeClippedVisibilityForSubList(aBuilder, aVisibleRegion,
                                              GetChildren(), GetPaintRect());
}

nsRegion nsDisplayWrapList::GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                                            bool* aSnap) const {
  *aSnap = false;
  bool snap;
  return ::GetOpaqueRegion(aBuilder, GetChildren(), GetBounds(aBuilder, &snap));
}

Maybe<nscolor> nsDisplayWrapList::IsUniform(
    nsDisplayListBuilder* aBuilder) const {
  // We could try to do something but let's conservatively just return Nothing.
  return Nothing();
}

void nsDisplayWrapList::Paint(nsDisplayListBuilder* aBuilder,
                              gfxContext* aCtx) {
  NS_ERROR("nsDisplayWrapList should have been flattened away for painting");
}

/**
 * Returns true if all descendant display items can be placed in the same
 * PaintedLayer --- GetLayerState returns LayerState::LAYER_INACTIVE or
 * LayerState::LAYER_NONE, and they all have the expected animated geometry
 * root.
 */
static LayerState RequiredLayerStateForChildren(
    nsDisplayListBuilder* aBuilder, LayerManager* aManager,
    const ContainerLayerParameters& aParameters, const nsDisplayList& aList,
    AnimatedGeometryRoot* aExpectedAnimatedGeometryRootForChildren) {
  LayerState result = LayerState::LAYER_INACTIVE;
  for (nsDisplayItem* i : aList) {
    if (result == LayerState::LAYER_INACTIVE &&
        i->GetAnimatedGeometryRoot() !=
            aExpectedAnimatedGeometryRootForChildren) {
      result = LayerState::LAYER_ACTIVE;
    }

    LayerState state = i->GetLayerState(aBuilder, aManager, aParameters);
    if (state == LayerState::LAYER_ACTIVE &&
        (i->GetType() == DisplayItemType::TYPE_BLEND_MODE ||
         i->GetType() == DisplayItemType::TYPE_TABLE_BLEND_MODE)) {
      // nsDisplayBlendMode always returns LayerState::LAYER_ACTIVE to ensure
      // that the blending operation happens in the intermediate surface of its
      // parent display item (usually an nsDisplayBlendContainer). But this does
      // not mean that it needs all its ancestor display items to become active.
      // So we ignore its layer state and look at its children instead.
      state = RequiredLayerStateForChildren(
          aBuilder, aManager, aParameters,
          *i->GetSameCoordinateSystemChildren(), i->GetAnimatedGeometryRoot());
    }
    if ((state == LayerState::LAYER_ACTIVE ||
         state == LayerState::LAYER_ACTIVE_FORCE) &&
        state > result) {
      result = state;
    }
    if (state == LayerState::LAYER_ACTIVE_EMPTY && state > result) {
      result = LayerState::LAYER_ACTIVE_FORCE;
    }
    if (state == LayerState::LAYER_NONE) {
      nsDisplayList* list = i->GetSameCoordinateSystemChildren();
      if (list) {
        LayerState childState = RequiredLayerStateForChildren(
            aBuilder, aManager, aParameters, *list,
            aExpectedAnimatedGeometryRootForChildren);
        if (childState > result) {
          result = childState;
        }
      }
    }
  }
  return result;
}

nsRect nsDisplayWrapList::GetComponentAlphaBounds(
    nsDisplayListBuilder* aBuilder) const {
  return mListPtr->GetComponentAlphaBounds(aBuilder);
}

void nsDisplayWrapList::SetReferenceFrame(const nsIFrame* aFrame) {
  mReferenceFrame = aFrame;
  mToReferenceFrame = mFrame->GetOffsetToCrossDoc(mReferenceFrame);
}

bool nsDisplayWrapList::CreateWebRenderCommands(
    mozilla::wr::DisplayListBuilder& aBuilder,
    mozilla::wr::IpcResourceUpdateQueue& aResources,
    const StackingContextHelper& aSc,
    mozilla::layers::RenderRootStateManager* aManager,
    nsDisplayListBuilder* aDisplayListBuilder) {
  aManager->CommandBuilder().CreateWebRenderCommandsFromDisplayList(
      GetChildren(), this, aDisplayListBuilder, aSc, aBuilder, aResources);
  return true;
}

static nsresult WrapDisplayList(nsDisplayListBuilder* aBuilder,
                                nsIFrame* aFrame, nsDisplayList* aList,
                                nsDisplayWrapper* aWrapper) {
  if (!aList->GetTop()) {
    return NS_OK;
  }
  nsDisplayItem* item = aWrapper->WrapList(aBuilder, aFrame, aList);
  if (!item) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  // aList was emptied
  aList->AppendToTop(item);
  return NS_OK;
}

static nsresult WrapEachDisplayItem(nsDisplayListBuilder* aBuilder,
                                    nsDisplayList* aList,
                                    nsDisplayWrapper* aWrapper) {
  nsDisplayList newList;
  nsDisplayItem* item;
  while ((item = aList->RemoveBottom())) {
    item = aWrapper->WrapItem(aBuilder, item);
    if (!item) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    newList.AppendToTop(item);
  }
  // aList was emptied
  aList->AppendToTop(&newList);
  return NS_OK;
}

nsresult nsDisplayWrapper::WrapLists(nsDisplayListBuilder* aBuilder,
                                     nsIFrame* aFrame,
                                     const nsDisplayListSet& aIn,
                                     const nsDisplayListSet& aOut) {
  nsresult rv = WrapListsInPlace(aBuilder, aFrame, aIn);
  NS_ENSURE_SUCCESS(rv, rv);

  if (&aOut == &aIn) {
    return NS_OK;
  }
  aOut.BorderBackground()->AppendToTop(aIn.BorderBackground());
  aOut.BlockBorderBackgrounds()->AppendToTop(aIn.BlockBorderBackgrounds());
  aOut.Floats()->AppendToTop(aIn.Floats());
  aOut.Content()->AppendToTop(aIn.Content());
  aOut.PositionedDescendants()->AppendToTop(aIn.PositionedDescendants());
  aOut.Outlines()->AppendToTop(aIn.Outlines());
  return NS_OK;
}

nsresult nsDisplayWrapper::WrapListsInPlace(nsDisplayListBuilder* aBuilder,
                                            nsIFrame* aFrame,
                                            const nsDisplayListSet& aLists) {
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

nsDisplayOpacity::nsDisplayOpacity(
    nsDisplayListBuilder* aBuilder, nsIFrame* aFrame, nsDisplayList* aList,
    const ActiveScrolledRoot* aActiveScrolledRoot,
    bool aForEventsAndPluginsOnly, bool aNeedsActiveLayer)
    : nsDisplayWrapList(aBuilder, aFrame, aList, aActiveScrolledRoot, true),
      mOpacity(aFrame->StyleEffects()->mOpacity),
      mForEventsAndPluginsOnly(aForEventsAndPluginsOnly),
      mNeedsActiveLayer(aNeedsActiveLayer),
      mChildOpacityState(ChildOpacityState::Unknown) {
  MOZ_COUNT_CTOR(nsDisplayOpacity);
  mState.mOpacity = mOpacity;
}

void nsDisplayOpacity::HitTest(nsDisplayListBuilder* aBuilder,
                               const nsRect& aRect,
                               nsDisplayItem::HitTestState* aState,
                               nsTArray<nsIFrame*>* aOutFrames) {
  // TODO(emilio): special-casing zero is a bit arbitrary... Maybe we should
  // only consider fully opaque items? Or make this configurable somehow?
  if (aBuilder->HitTestIsForVisibility() && mOpacity == 0.0f) {
    return;
  }
  nsDisplayWrapList::HitTest(aBuilder, aRect, aState, aOutFrames);
}

nsRegion nsDisplayOpacity::GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                                           bool* aSnap) const {
  *aSnap = false;
  // The only time where mOpacity == 1.0 should be when we have will-change.
  // We could report this as opaque then but when the will-change value starts
  // animating the element would become non opaque and could cause repaints.
  return nsRegion();
}

// nsDisplayOpacity uses layers for rendering
already_AddRefed<Layer> nsDisplayOpacity::BuildLayer(
    nsDisplayListBuilder* aBuilder, LayerManager* aManager,
    const ContainerLayerParameters& aContainerParameters) {
  ContainerLayerParameters params = aContainerParameters;
  RefPtr<Layer> container = aManager->GetLayerBuilder()->BuildContainerLayerFor(
      aBuilder, aManager, mFrame, this, &mList, params, nullptr,
      FrameLayerBuilder::CONTAINER_ALLOW_PULL_BACKGROUND_COLOR);
  if (!container) {
    return nullptr;
  }

  container->SetOpacity(mOpacity);
  nsDisplayListBuilder::AddAnimationsAndTransitionsToLayer(
      container, aBuilder, this, mFrame, GetType());
  return container.forget();
}

/**
 * This doesn't take into account layer scaling --- the layer may be
 * rendered at a higher (or lower) resolution, affecting the retained layer
 * size --- but this should be good enough.
 */
static bool IsItemTooSmallForActiveLayer(nsIFrame* aFrame) {
  nsIntRect visibleDevPixels =
      aFrame->GetVisualOverflowRectRelativeToSelf().ToOutsidePixels(
          aFrame->PresContext()->AppUnitsPerDevPixel());
  return visibleDevPixels.Size() <
         nsIntSize(StaticPrefs::layout_min_active_layer_size(),
                   StaticPrefs::layout_min_active_layer_size());
}

/* static */
bool nsDisplayOpacity::NeedsActiveLayer(nsDisplayListBuilder* aBuilder,
                                        nsIFrame* aFrame,
                                        bool aEnforceMinimumSize) {
  if (EffectCompositor::HasAnimationsForCompositor(
          aFrame, DisplayItemType::TYPE_OPACITY) ||
      (ActiveLayerTracker::IsStyleAnimated(
           aBuilder, aFrame, nsCSSPropertyIDSet::OpacityProperties()) &&
       !(aEnforceMinimumSize && IsItemTooSmallForActiveLayer(aFrame)))) {
    return true;
  }
  return false;
}

void nsDisplayOpacity::ApplyOpacity(nsDisplayListBuilder* aBuilder,
                                    float aOpacity,
                                    const DisplayItemClipChain* aClip) {
  NS_ASSERTION(CanApplyOpacity(), "ApplyOpacity should be allowed");
  mOpacity = mOpacity * aOpacity;
  IntersectClip(aBuilder, aClip, false);
}

bool nsDisplayOpacity::CanApplyOpacity() const {
  return !EffectCompositor::HasAnimationsForCompositor(
      mFrame, DisplayItemType::TYPE_OPACITY);
}

// Only try folding our opacity down if we have at most |kOpacityMaxChildCount|
// children that don't overlap and can all apply the opacity to themselves.
static const size_t kOpacityMaxChildCount = 3;

// |kOpacityMaxListSize| defines an early exit condition for opacity items that
// are likely have more child items than |kOpacityMaxChildCount|.
static const size_t kOpacityMaxListSize = kOpacityMaxChildCount * 2;

/**
 * Recursively iterates through |aList| and collects at most
 * |kOpacityMaxChildCount| display item pointers to items that return true for
 * CanApplyOpacity(). The item pointers are added to |aArray|.
 *
 * LayerEventRegions and WrapList items are ignored.
 *
 * We need to do this recursively, because the child display items might contain
 * nested nsDisplayWrapLists.
 *
 * Returns false if there are more than |kOpacityMaxChildCount| items, or if an
 * item that returns false for CanApplyOpacity() is encountered.
 * Otherwise returns true.
 */
static bool CollectItemsWithOpacity(nsDisplayList* aList,
                                    nsTArray<nsPaintedDisplayItem*>& aArray) {
  if (aList->Count() > kOpacityMaxListSize) {
    // Exit early, since |aList| will likely contain more than
    // |kOpacityMaxChildCount| items.
    return false;
  }

  for (nsDisplayItem* i : *aList) {
    const DisplayItemType type = i->GetType();

    if (type == DisplayItemType::TYPE_COMPOSITOR_HITTEST_INFO) {
      continue;
    }

    // Descend only into wraplists.
    if (type == DisplayItemType::TYPE_WRAP_LIST ||
        type == DisplayItemType::TYPE_CONTAINER) {
      // The current display item has children, process them first.
      if (!CollectItemsWithOpacity(i->GetChildren(), aArray)) {
        return false;
      }

      continue;
    }

    if (aArray.Length() == kOpacityMaxChildCount) {
      return false;
    }

    auto* item = i->AsPaintedDisplayItem();
    if (!item || !item->CanApplyOpacity()) {
      return false;
    }

    aArray.AppendElement(item);
  }

  return true;
}

bool nsDisplayOpacity::ApplyOpacityToChildren(nsDisplayListBuilder* aBuilder) {
  if (mChildOpacityState == ChildOpacityState::Deferred) {
    return false;
  }

  // Iterate through the child display list and copy at most
  // |kOpacityMaxChildCount| child display item pointers to a temporary list.
  AutoTArray<nsPaintedDisplayItem*, kOpacityMaxChildCount> items;
  if (!CollectItemsWithOpacity(&mList, items)) {
    mChildOpacityState = ChildOpacityState::Deferred;
    return false;
  }

  struct {
    nsPaintedDisplayItem* item;
    nsRect bounds;
  } children[kOpacityMaxChildCount];

  bool snap;
  size_t childCount = 0;
  for (nsPaintedDisplayItem* item : items) {
    children[childCount].item = item;
    children[childCount].bounds = item->GetBounds(aBuilder, &snap);
    childCount++;
  }

  for (size_t i = 0; i < childCount; i++) {
    for (size_t j = i + 1; j < childCount; j++) {
      if (children[i].bounds.Intersects(children[j].bounds)) {
        mChildOpacityState = ChildOpacityState::Deferred;
        return false;
      }
    }
  }

  for (uint32_t i = 0; i < childCount; i++) {
    children[i].item->ApplyOpacity(aBuilder, mOpacity, mClipChain);
  }

  mChildOpacityState = ChildOpacityState::Applied;
  return true;
}

/**
 * Returns true if this nsDisplayOpacity contains only a filter or a mask item
 * that has the same frame as the opacity item. In this case the opacity item
 * can be optimized away.
 */
bool nsDisplayOpacity::IsEffectsWrapper() const {
  if (mList.Count() != 1) {
    return false;
  }

  const nsDisplayItem* item = mList.GetBottom();

  if (item->Frame() != mFrame) {
    // The effect item needs to have the same frame as the opacity item.
    return false;
  }

  const DisplayItemType type = item->GetType();
  return type == DisplayItemType::TYPE_MASK ||
         type == DisplayItemType::TYPE_FILTER;
}

bool nsDisplayOpacity::ShouldFlattenAway(nsDisplayListBuilder* aBuilder) {
  if (mFrame->GetPrevContinuation() || mFrame->GetNextContinuation() ||
      mFrame->HasAnyStateBits(NS_FRAME_PART_OF_IBSPLIT)) {
    // If we've been split, then we might need to merge, so
    // don't flatten us away.
    return false;
  }

  if (mNeedsActiveLayer || mOpacity == 0.0) {
    // If our opacity is zero then we'll discard all descendant display items
    // except for layer event regions, so there's no point in doing this
    // optimization (and if we do do it, then invalidations of those descendants
    // might trigger repainting).
    return false;
  }

  if (mList.IsEmpty()) {
    return false;
  }

  if (IsEffectsWrapper()) {
    MOZ_ASSERT(nsSVGIntegrationUtils::UsingEffectsForFrame(mFrame));
    static_cast<nsDisplayEffectsBase*>(mList.GetBottom())->SetHandleOpacity();
    mChildOpacityState = ChildOpacityState::Applied;
    return true;
  }

  // Return true if we successfully applied opacity to child items, or if
  // WebRender is not in use. In the latter case, the opacity gets flattened and
  // applied during layer building.
  return ApplyOpacityToChildren(aBuilder) || !gfxVars::UseWebRender();
}

nsDisplayItem::LayerState nsDisplayOpacity::GetLayerState(
    nsDisplayListBuilder* aBuilder, LayerManager* aManager,
    const ContainerLayerParameters& aParameters) {
  // If we only created this item so that we'd get correct nsDisplayEventRegions
  // for child frames, then force us to inactive to avoid unnecessary
  // layerization changes for content that won't ever be painted.
  if (mForEventsAndPluginsOnly) {
    MOZ_ASSERT(mOpacity == 0);
    return LayerState::LAYER_INACTIVE;
  }

  if (mNeedsActiveLayer) {
    // Returns LayerState::LAYER_ACTIVE_FORCE to avoid flatterning the layer for
    // async animations.
    return LayerState::LAYER_ACTIVE_FORCE;
  }

  return RequiredLayerStateForChildren(aBuilder, aManager, aParameters, mList,
                                       GetAnimatedGeometryRoot());
}

bool nsDisplayOpacity::ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                         nsRegion* aVisibleRegion) {
  // Our children are translucent so we should not allow them to subtract
  // area from aVisibleRegion. We do need to find out what is visible under
  // our children in the temporary compositing buffer, because if our children
  // paint our entire bounds opaquely then we don't need an alpha channel in
  // the temporary compositing buffer.
  nsRect bounds = GetClippedBounds(aBuilder);
  nsRegion visibleUnderChildren;
  visibleUnderChildren.And(*aVisibleRegion, bounds);
  return nsDisplayWrapList::ComputeVisibility(aBuilder, &visibleUnderChildren);
}

void nsDisplayOpacity::ComputeInvalidationRegion(
    nsDisplayListBuilder* aBuilder, const nsDisplayItemGeometry* aGeometry,
    nsRegion* aInvalidRegion) const {
  auto* geometry = static_cast<const nsDisplayOpacityGeometry*>(aGeometry);

  bool snap;
  if (mOpacity != geometry->mOpacity) {
    aInvalidRegion->Or(GetBounds(aBuilder, &snap), geometry->mBounds);
  }
}

void nsDisplayOpacity::WriteDebugInfo(std::stringstream& aStream) {
  aStream << " (opacity " << mOpacity << ")";
}

bool nsDisplayOpacity::CreateWebRenderCommands(
    mozilla::wr::DisplayListBuilder& aBuilder,
    mozilla::wr::IpcResourceUpdateQueue& aResources,
    const StackingContextHelper& aSc,
    mozilla::layers::RenderRootStateManager* aManager,
    nsDisplayListBuilder* aDisplayListBuilder) {
  float* opacityForSC = &mOpacity;

  uint64_t animationsId = AddAnimationsForWebRender(
      this, aManager, aDisplayListBuilder, aBuilder.GetRenderRoot());
  wr::WrAnimationProperty prop{
      wr::WrAnimationType::Opacity,
      animationsId,
  };

  wr::StackingContextParams params;
  params.animation = animationsId ? &prop : nullptr;
  params.opacity = opacityForSC;
  params.clip =
      wr::WrStackingContextClip::ClipChain(aBuilder.CurrentClipChainId());
  StackingContextHelper sc(aSc, GetActiveScrolledRoot(), mFrame, this, aBuilder,
                           params);

  aManager->CommandBuilder().CreateWebRenderCommandsFromDisplayList(
      &mList, this, aDisplayListBuilder, sc, aBuilder, aResources);
  return true;
}

nsDisplayBlendMode::nsDisplayBlendMode(
    nsDisplayListBuilder* aBuilder, nsIFrame* aFrame, nsDisplayList* aList,
    mozilla::StyleBlend aBlendMode,
    const ActiveScrolledRoot* aActiveScrolledRoot, uint16_t aIndex)
    : nsDisplayWrapList(aBuilder, aFrame, aList, aActiveScrolledRoot, true),
      mBlendMode(aBlendMode),
      mIndex(aIndex) {
  MOZ_COUNT_CTOR(nsDisplayBlendMode);
}

nsRegion nsDisplayBlendMode::GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                                             bool* aSnap) const {
  *aSnap = false;
  // We are never considered opaque
  return nsRegion();
}

LayerState nsDisplayBlendMode::GetLayerState(
    nsDisplayListBuilder* aBuilder, LayerManager* aManager,
    const ContainerLayerParameters& aParameters) {
  return LayerState::LAYER_ACTIVE;
}

bool nsDisplayBlendMode::CreateWebRenderCommands(
    mozilla::wr::DisplayListBuilder& aBuilder,
    mozilla::wr::IpcResourceUpdateQueue& aResources,
    const StackingContextHelper& aSc,
    mozilla::layers::RenderRootStateManager* aManager,
    nsDisplayListBuilder* aDisplayListBuilder) {
  wr::StackingContextParams params;
  params.mix_blend_mode =
      wr::ToMixBlendMode(nsCSSRendering::GetGFXBlendMode(mBlendMode));
  params.clip =
      wr::WrStackingContextClip::ClipChain(aBuilder.CurrentClipChainId());
  StackingContextHelper sc(aSc, GetActiveScrolledRoot(), mFrame, this, aBuilder,
                           params);

  return nsDisplayWrapList::CreateWebRenderCommands(
      aBuilder, aResources, sc, aManager, aDisplayListBuilder);
}

// nsDisplayBlendMode uses layers for rendering
already_AddRefed<Layer> nsDisplayBlendMode::BuildLayer(
    nsDisplayListBuilder* aBuilder, LayerManager* aManager,
    const ContainerLayerParameters& aContainerParameters) {
  ContainerLayerParameters newContainerParameters = aContainerParameters;
  newContainerParameters.mDisableSubpixelAntialiasingInDescendants = true;

  RefPtr<Layer> container = aManager->GetLayerBuilder()->BuildContainerLayerFor(
      aBuilder, aManager, mFrame, this, &mList, newContainerParameters,
      nullptr);
  if (!container) {
    return nullptr;
  }

  container->SetMixBlendMode(nsCSSRendering::GetGFXBlendMode(mBlendMode));

  return container.forget();
}

mozilla::gfx::CompositionOp nsDisplayBlendMode::BlendMode() {
  return nsCSSRendering::GetGFXBlendMode(mBlendMode);
}

bool nsDisplayBlendMode::ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                           nsRegion* aVisibleRegion) {
  // Our children are need their backdrop so we should not allow them to
  // subtract area from aVisibleRegion. We do need to find out what is visible
  // under our children in the temporary compositing buffer, because if our
  // children paint our entire bounds opaquely then we don't need an alpha
  // channel in the temporary compositing buffer.
  nsRect bounds = GetClippedBounds(aBuilder);
  nsRegion visibleUnderChildren;
  visibleUnderChildren.And(*aVisibleRegion, bounds);
  return nsDisplayWrapList::ComputeVisibility(aBuilder, &visibleUnderChildren);
}

bool nsDisplayBlendMode::CanMerge(const nsDisplayItem* aItem) const {
  // Items for the same content element should be merged into a single
  // compositing group.
  if (!HasDifferentFrame(aItem) || !HasSameTypeAndClip(aItem) ||
      !HasSameContent(aItem)) {
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

/* static */
nsDisplayBlendContainer* nsDisplayBlendContainer::CreateForMixBlendMode(
    nsDisplayListBuilder* aBuilder, nsIFrame* aFrame, nsDisplayList* aList,
    const ActiveScrolledRoot* aActiveScrolledRoot) {
  return MakeDisplayItem<nsDisplayBlendContainer>(aBuilder, aFrame, aList,
                                                  aActiveScrolledRoot, false);
}

/* static */
nsDisplayBlendContainer* nsDisplayBlendContainer::CreateForBackgroundBlendMode(
    nsDisplayListBuilder* aBuilder, nsIFrame* aFrame, nsDisplayList* aList,
    const ActiveScrolledRoot* aActiveScrolledRoot) {
  return MakeDisplayItem<nsDisplayBlendContainer>(aBuilder, aFrame, aList,
                                                  aActiveScrolledRoot, true);
}

nsDisplayBlendContainer::nsDisplayBlendContainer(
    nsDisplayListBuilder* aBuilder, nsIFrame* aFrame, nsDisplayList* aList,
    const ActiveScrolledRoot* aActiveScrolledRoot, bool aIsForBackground)
    : nsDisplayWrapList(aBuilder, aFrame, aList, aActiveScrolledRoot, true),
      mIsForBackground(aIsForBackground) {
  MOZ_COUNT_CTOR(nsDisplayBlendContainer);
}

// nsDisplayBlendContainer uses layers for rendering
already_AddRefed<Layer> nsDisplayBlendContainer::BuildLayer(
    nsDisplayListBuilder* aBuilder, LayerManager* aManager,
    const ContainerLayerParameters& aContainerParameters) {
  // turn off anti-aliasing in the parent stacking context because it changes
  // how the group is initialized.
  ContainerLayerParameters newContainerParameters = aContainerParameters;
  newContainerParameters.mDisableSubpixelAntialiasingInDescendants = true;

  RefPtr<Layer> container = aManager->GetLayerBuilder()->BuildContainerLayerFor(
      aBuilder, aManager, mFrame, this, &mList, newContainerParameters,
      nullptr);
  if (!container) {
    return nullptr;
  }

  container->SetForceIsolatedGroup(true);
  return container.forget();
}

LayerState nsDisplayBlendContainer::GetLayerState(
    nsDisplayListBuilder* aBuilder, LayerManager* aManager,
    const ContainerLayerParameters& aParameters) {
  return RequiredLayerStateForChildren(aBuilder, aManager, aParameters, mList,
                                       GetAnimatedGeometryRoot());
}

bool nsDisplayBlendContainer::CreateWebRenderCommands(
    mozilla::wr::DisplayListBuilder& aBuilder,
    mozilla::wr::IpcResourceUpdateQueue& aResources,
    const StackingContextHelper& aSc,
    mozilla::layers::RenderRootStateManager* aManager,
    nsDisplayListBuilder* aDisplayListBuilder) {
  wr::StackingContextParams params;
  params.clip =
      wr::WrStackingContextClip::ClipChain(aBuilder.CurrentClipChainId());
  StackingContextHelper sc(aSc, GetActiveScrolledRoot(), mFrame, this, aBuilder,
                           params);

  return nsDisplayWrapList::CreateWebRenderCommands(
      aBuilder, aResources, sc, aManager, aDisplayListBuilder);
}

/* static */
nsDisplayTableBlendContainer*
nsDisplayTableBlendContainer::CreateForBackgroundBlendMode(
    nsDisplayListBuilder* aBuilder, nsIFrame* aFrame, nsDisplayList* aList,
    const ActiveScrolledRoot* aActiveScrolledRoot, nsIFrame* aAncestorFrame) {
  return MakeDisplayItem<nsDisplayTableBlendContainer>(
      aBuilder, aFrame, aList, aActiveScrolledRoot, true, aAncestorFrame);
}

nsDisplayOwnLayer::nsDisplayOwnLayer(
    nsDisplayListBuilder* aBuilder, nsIFrame* aFrame, nsDisplayList* aList,
    const ActiveScrolledRoot* aActiveScrolledRoot,
    nsDisplayOwnLayerFlags aFlags, const ScrollbarData& aScrollbarData,
    bool aForceActive, bool aClearClipChain, uint16_t aIndex)
    : nsDisplayWrapList(aBuilder, aFrame, aList, aActiveScrolledRoot,
                        aClearClipChain),
      mFlags(aFlags),
      mScrollbarData(aScrollbarData),
      mForceActive(aForceActive),
      mWrAnimationId(0),
      mIndex(aIndex) {
  MOZ_COUNT_CTOR(nsDisplayOwnLayer);

  // For scroll thumb layers, override the AGR to be the thumb's AGR rather
  // than the AGR for mFrame (which is the slider frame).
  if (IsScrollThumbLayer()) {
    if (nsIFrame* thumbFrame = nsBox::GetChildXULBox(mFrame)) {
      mAnimatedGeometryRoot = aBuilder->FindAnimatedGeometryRootFor(thumbFrame);
    }
  }
}

LayerState nsDisplayOwnLayer::GetLayerState(
    nsDisplayListBuilder* aBuilder, LayerManager* aManager,
    const ContainerLayerParameters& aParameters) {
  if (mForceActive) {
    return mozilla::LayerState::LAYER_ACTIVE_FORCE;
  }

  return RequiredLayerStateForChildren(aBuilder, aManager, aParameters, mList,
                                       mAnimatedGeometryRoot);
}

bool nsDisplayOwnLayer::IsScrollThumbLayer() const {
  return mScrollbarData.mScrollbarLayerType ==
         layers::ScrollbarLayerType::Thumb;
}

bool nsDisplayOwnLayer::IsScrollbarContainer() const {
  return mScrollbarData.mScrollbarLayerType ==
         layers::ScrollbarLayerType::Container;
}

bool nsDisplayOwnLayer::IsRootScrollbarContainer() const {
  if (!IsScrollbarContainer()) {
    return false;
  }

  return mFrame->PresContext()->IsRootContentDocumentCrossProcess() &&
         mScrollbarData.mTargetViewId ==
             nsLayoutUtils::ScrollIdForRootScrollFrame(mFrame->PresContext());
}

bool nsDisplayOwnLayer::IsZoomingLayer() const {
  return GetType() == DisplayItemType::TYPE_ASYNC_ZOOM;
}

bool nsDisplayOwnLayer::IsFixedPositionLayer() const {
  return GetType() == DisplayItemType::TYPE_FIXED_POSITION;
}

bool nsDisplayOwnLayer::HasDynamicToolbar() const {
  if (!mFrame->PresContext()->IsRootContentDocumentCrossProcess()) {
    return false;
  }
  return mFrame->PresContext()->HasDynamicToolbar() ||
         // For tests on Android, this pref is set to simulate the dynamic
         // toolbar
         StaticPrefs::apz_fixed_margin_override_enabled();
}

// nsDisplayOpacity uses layers for rendering
already_AddRefed<Layer> nsDisplayOwnLayer::BuildLayer(
    nsDisplayListBuilder* aBuilder, LayerManager* aManager,
    const ContainerLayerParameters& aContainerParameters) {
  RefPtr<ContainerLayer> layer =
      aManager->GetLayerBuilder()->BuildContainerLayerFor(
          aBuilder, aManager, mFrame, this, &mList, aContainerParameters,
          nullptr, FrameLayerBuilder::CONTAINER_ALLOW_PULL_BACKGROUND_COLOR);

  if (IsScrollThumbLayer() || IsScrollbarContainer()) {
    layer->SetScrollbarData(mScrollbarData);
  }

  if (mFlags & nsDisplayOwnLayerFlags::GenerateSubdocInvalidations) {
    mFrame->PresContext()->SetNotifySubDocInvalidationData(layer);
  }
  return layer.forget();
}

bool nsDisplayOwnLayer::CreateWebRenderCommands(
    mozilla::wr::DisplayListBuilder& aBuilder,
    mozilla::wr::IpcResourceUpdateQueue& aResources,
    const StackingContextHelper& aSc, RenderRootStateManager* aManager,
    nsDisplayListBuilder* aDisplayListBuilder) {
  Maybe<wr::WrAnimationProperty> prop;
  bool needsProp = aManager->LayerManager()->AsyncPanZoomEnabled() &&
                   (IsScrollThumbLayer() || IsZoomingLayer() ||
                    (IsFixedPositionLayer() && HasDynamicToolbar()) ||
                    (IsRootScrollbarContainer() && HasDynamicToolbar()));

  if (needsProp) {
    // APZ is enabled and this is a scroll thumb or zooming layer, so we need
    // to create and set an animation id. That way APZ can adjust the position/
    // zoom of this content asynchronously as needed.
    RefPtr<WebRenderAPZAnimationData> animationData =
        aManager->CommandBuilder()
            .CreateOrRecycleWebRenderUserData<WebRenderAPZAnimationData>(
                this, aBuilder.GetRenderRoot());
    mWrAnimationId = animationData->GetAnimationId();

    prop.emplace();
    prop->id = mWrAnimationId;
    prop->effect_type = wr::WrAnimationType::Transform;
  }

  wr::StackingContextParams params;
  params.animation = prop.ptrOr(nullptr);
  params.clip =
      wr::WrStackingContextClip::ClipChain(aBuilder.CurrentClipChainId());
  if (IsScrollbarContainer()) {
    params.prim_flags |= wr::PrimitiveFlags::IS_SCROLLBAR_CONTAINER;
  }
  if (IsScrollThumbLayer()) {
    params.prim_flags |= wr::PrimitiveFlags::IS_SCROLLBAR_THUMB;
  }
  StackingContextHelper sc(aSc, GetActiveScrolledRoot(), mFrame, this, aBuilder,
                           params);

  nsDisplayWrapList::CreateWebRenderCommands(aBuilder, aResources, sc, aManager,
                                             aDisplayListBuilder);
  return true;
}

bool nsDisplayOwnLayer::UpdateScrollData(
    mozilla::layers::WebRenderScrollData* aData,
    mozilla::layers::WebRenderLayerScrollData* aLayerData) {
  bool isRelevantToApz =
      (IsScrollThumbLayer() || IsScrollbarContainer() || IsZoomingLayer() ||
       (IsFixedPositionLayer() && HasDynamicToolbar()));
  if (!isRelevantToApz) {
    return false;
  }

  if (!aLayerData) {
    return true;
  }

  if (IsZoomingLayer()) {
    aLayerData->SetZoomAnimationId(mWrAnimationId);
    return true;
  }

  if (IsFixedPositionLayer() && HasDynamicToolbar()) {
    aLayerData->SetFixedPositionAnimationId(mWrAnimationId);
    return true;
  }

  MOZ_ASSERT(IsScrollbarContainer() || IsScrollThumbLayer());

  aLayerData->SetScrollbarData(mScrollbarData);

  if (IsRootScrollbarContainer() && HasDynamicToolbar()) {
    aLayerData->SetScrollbarAnimationId(mWrAnimationId);
    return true;
  }

  if (IsScrollThumbLayer()) {
    aLayerData->SetScrollbarAnimationId(mWrAnimationId);
    LayoutDeviceRect bounds = LayoutDeviceIntRect::FromAppUnits(
        mBounds, mFrame->PresContext()->AppUnitsPerDevPixel());
    // We use a resolution of 1.0 because this is a WebRender codepath which
    // always uses containerless scrolling, and so resolution doesn't apply to
    // scrollbars.
    LayerIntRect layerBounds =
        RoundedOut(bounds * LayoutDeviceToLayerScale(1.0f));
    aLayerData->SetVisibleRegion(LayerIntRegion(layerBounds));
  }
  return true;
}

void nsDisplayOwnLayer::WriteDebugInfo(std::stringstream& aStream) {
  aStream << nsPrintfCString(" (flags 0x%x) (scrolltarget %" PRIu64 ")",
                             (int)mFlags, mScrollbarData.mTargetViewId)
                 .get();
}

nsDisplaySubDocument::nsDisplaySubDocument(nsDisplayListBuilder* aBuilder,
                                           nsIFrame* aFrame,
                                           nsSubDocumentFrame* aSubDocFrame,
                                           nsDisplayList* aList,
                                           nsDisplayOwnLayerFlags aFlags)
    : nsDisplayOwnLayer(aBuilder, aFrame, aList,
                        aBuilder->CurrentActiveScrolledRoot(), aFlags),
      mScrollParentId(aBuilder->GetCurrentScrollParentId()),
      mShouldFlatten(false),
      mSubDocFrame(aSubDocFrame) {
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

nsIFrame* nsDisplaySubDocument::FrameForInvalidation() const {
  return mSubDocFrame ? mSubDocFrame : mFrame;
}

void nsDisplaySubDocument::RemoveFrame(nsIFrame* aFrame) {
  if (aFrame == mSubDocFrame) {
    mSubDocFrame = nullptr;
    SetDeletedFrame();
  }
  nsDisplayOwnLayer::RemoveFrame(aFrame);
}

void nsDisplaySubDocument::Disown() {
  if (mFrame) {
    mFrame->RemoveDisplayItem(this);
    RemoveFrame(mFrame);
  }
  if (mSubDocFrame) {
    mSubDocFrame->RemoveDisplayItem(this);
    RemoveFrame(mSubDocFrame);
  }
}

UniquePtr<ScrollMetadata> nsDisplaySubDocument::ComputeScrollMetadata(
    LayerManager* aLayerManager,
    const ContainerLayerParameters& aContainerParameters) {
  if (!(mFlags & nsDisplayOwnLayerFlags::GenerateScrollableLayer)) {
    return UniquePtr<ScrollMetadata>(nullptr);
  }

  nsPresContext* presContext = mFrame->PresContext();
  nsIFrame* rootScrollFrame = presContext->PresShell()->GetRootScrollFrame();
  bool isRootContentDocument = presContext->IsRootContentDocumentCrossProcess();
  PresShell* presShell = presContext->PresShell();
  ContainerLayerParameters params(
      aContainerParameters.mXScale * presShell->GetResolution(),
      aContainerParameters.mYScale * presShell->GetResolution(), nsIntPoint(),
      aContainerParameters);

  nsRect viewport = mFrame->GetRect() - mFrame->GetPosition() +
                    mFrame->GetOffsetToCrossDoc(ReferenceFrame());

  nsIScrollableFrame* scrollableFrame = rootScrollFrame->GetScrollTargetFrame();
  if (isRootContentDocument) {
    viewport.SizeTo(scrollableFrame->GetScrollPortRect().Size());
  }

  UniquePtr<ScrollMetadata> metadata =
      MakeUnique<ScrollMetadata>(nsLayoutUtils::ComputeScrollMetadata(
          mFrame, rootScrollFrame, rootScrollFrame->GetContent(),
          ReferenceFrame(), aLayerManager, mScrollParentId, viewport, Nothing(),
          isRootContentDocument, Some(params)));
  if (scrollableFrame) {
    scrollableFrame->NotifyApzTransaction();
  }

  return metadata;
}

static bool UseDisplayPortForViewport(nsDisplayListBuilder* aBuilder,
                                      nsIFrame* aFrame) {
  return aBuilder->IsPaintingToWindow() &&
         nsLayoutUtils::ViewportHasDisplayPort(aFrame->PresContext());
}

nsRect nsDisplaySubDocument::GetBounds(nsDisplayListBuilder* aBuilder,
                                       bool* aSnap) const {
  bool usingDisplayPort = UseDisplayPortForViewport(aBuilder, mFrame);

  if ((mFlags & nsDisplayOwnLayerFlags::GenerateScrollableLayer) &&
      usingDisplayPort) {
    *aSnap = false;
    return mFrame->GetRect() + aBuilder->ToReferenceFrame(mFrame);
  }

  return nsDisplayOwnLayer::GetBounds(aBuilder, aSnap);
}

bool nsDisplaySubDocument::ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                             nsRegion* aVisibleRegion) {
  bool usingDisplayPort = UseDisplayPortForViewport(aBuilder, mFrame);

  if (!(mFlags & nsDisplayOwnLayerFlags::GenerateScrollableLayer) ||
      !usingDisplayPort) {
    return nsDisplayWrapList::ComputeVisibility(aBuilder, aVisibleRegion);
  }

  nsRect displayport;
  nsIFrame* rootScrollFrame = mFrame->PresShell()->GetRootScrollFrame();
  MOZ_ASSERT(rootScrollFrame);
  Unused << nsLayoutUtils::GetDisplayPort(rootScrollFrame->GetContent(),
                                          &displayport,
                                          DisplayportRelativeTo::ScrollFrame);

  nsRegion childVisibleRegion;
  // The visible region for the children may be much bigger than the hole we
  // are viewing the children from, so that the compositor process has enough
  // content to asynchronously pan while content is being refreshed.
  childVisibleRegion =
      displayport + mFrame->GetOffsetToCrossDoc(ReferenceFrame());

  nsRect boundedRect = childVisibleRegion.GetBounds().Intersect(
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

nsRegion nsDisplaySubDocument::GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                                               bool* aSnap) const {
  bool usingDisplayPort = UseDisplayPortForViewport(aBuilder, mFrame);

  if ((mFlags & nsDisplayOwnLayerFlags::GenerateScrollableLayer) &&
      usingDisplayPort) {
    *aSnap = false;
    return nsRegion();
  }

  return nsDisplayOwnLayer::GetOpaqueRegion(aBuilder, aSnap);
}

nsDisplayResolution::nsDisplayResolution(nsDisplayListBuilder* aBuilder,
                                         nsIFrame* aFrame,
                                         nsSubDocumentFrame* aSubDocFrame,
                                         nsDisplayList* aList,
                                         nsDisplayOwnLayerFlags aFlags)
    : nsDisplaySubDocument(aBuilder, aFrame, aSubDocFrame, aList, aFlags) {
  MOZ_COUNT_CTOR(nsDisplayResolution);
}

void nsDisplayResolution::HitTest(nsDisplayListBuilder* aBuilder,
                                  const nsRect& aRect, HitTestState* aState,
                                  nsTArray<nsIFrame*>* aOutFrames) {
  PresShell* presShell = mFrame->PresShell();
  nsRect rect = aRect.RemoveResolution(presShell->GetResolution());
  mList.HitTest(aBuilder, rect, aState, aOutFrames);
}

already_AddRefed<Layer> nsDisplayResolution::BuildLayer(
    nsDisplayListBuilder* aBuilder, LayerManager* aManager,
    const ContainerLayerParameters& aContainerParameters) {
  float rootLayerResolution = 1.0f;
  ContainerLayerParameters containerParameters(
      rootLayerResolution, rootLayerResolution, nsIntPoint(),
      aContainerParameters);

  RefPtr<Layer> layer =
      nsDisplaySubDocument::BuildLayer(aBuilder, aManager, containerParameters);

  return layer.forget();
}

nsDisplayFixedPosition::nsDisplayFixedPosition(
    nsDisplayListBuilder* aBuilder, nsIFrame* aFrame, nsDisplayList* aList,
    const ActiveScrolledRoot* aActiveScrolledRoot,
    const ActiveScrolledRoot* aContainerASR)
    : nsDisplayOwnLayer(aBuilder, aFrame, aList, aActiveScrolledRoot),
      mContainerASR(aContainerASR),
      mIndex(0),
      mIsFixedBackground(false) {
  MOZ_COUNT_CTOR(nsDisplayFixedPosition);
  Init(aBuilder);
}

nsDisplayFixedPosition::nsDisplayFixedPosition(nsDisplayListBuilder* aBuilder,
                                               nsIFrame* aFrame,
                                               nsDisplayList* aList,
                                               uint16_t aIndex)
    : nsDisplayOwnLayer(aBuilder, aFrame, aList,
                        aBuilder->CurrentActiveScrolledRoot()),
      mContainerASR(nullptr),  // XXX maybe this should be something?
      mIndex(aIndex),
      mIsFixedBackground(true) {
  MOZ_COUNT_CTOR(nsDisplayFixedPosition);
  Init(aBuilder);
}

void nsDisplayFixedPosition::Init(nsDisplayListBuilder* aBuilder) {
  mAnimatedGeometryRootForScrollMetadata = mAnimatedGeometryRoot;
  if (ShouldFixToViewport(aBuilder)) {
    mAnimatedGeometryRoot = aBuilder->FindAnimatedGeometryRootFor(this);
  }
}

/* static */
nsDisplayFixedPosition* nsDisplayFixedPosition::CreateForFixedBackground(
    nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
    nsDisplayBackgroundImage* aImage, uint16_t aIndex) {
  nsDisplayList temp;
  temp.AppendToTop(aImage);

  return MakeDisplayItem<nsDisplayFixedPosition>(aBuilder, aFrame, &temp,
                                                 aIndex + 1);
}

already_AddRefed<Layer> nsDisplayFixedPosition::BuildLayer(
    nsDisplayListBuilder* aBuilder, LayerManager* aManager,
    const ContainerLayerParameters& aContainerParameters) {
  RefPtr<Layer> layer =
      nsDisplayOwnLayer::BuildLayer(aBuilder, aManager, aContainerParameters);

  layer->SetIsFixedPosition(true);

  nsPresContext* presContext = mFrame->PresContext();
  nsIFrame* fixedFrame =
      mIsFixedBackground ? presContext->PresShell()->GetRootFrame() : mFrame;

  const nsIFrame* viewportFrame = fixedFrame->GetParent();
  // anchorRect will be in the container's coordinate system (aLayer's parent
  // layer). This is the same as the display items' reference frame.
  nsRect anchorRect;
  if (viewportFrame) {
    anchorRect.SizeTo(viewportFrame->GetSize());
    // Fixed position frames are reflowed into the scroll-port size if one has
    // been set.
    if (const ViewportFrame* viewport = do_QueryFrame(viewportFrame)) {
      anchorRect.SizeTo(
          viewport->AdjustViewportSizeForFixedPosition(anchorRect));
    }
  } else {
    // A display item directly attached to the viewport.
    // For background-attachment:fixed items, the anchor point is always the
    // top-left of the viewport currently.
    viewportFrame = fixedFrame;
  }
  // The anchorRect top-left is always the viewport top-left.
  anchorRect.MoveTo(viewportFrame->GetOffsetToCrossDoc(ReferenceFrame()));

  nsLayoutUtils::SetFixedPositionLayerData(layer, viewportFrame, anchorRect,
                                           fixedFrame, presContext,
                                           aContainerParameters);

  return layer.forget();
}

ViewID nsDisplayFixedPosition::GetScrollTargetId() {
  if (mContainerASR && !nsLayoutUtils::IsReallyFixedPos(mFrame)) {
    return mContainerASR->GetViewId();
  }
  return nsLayoutUtils::ScrollIdForRootScrollFrame(mFrame->PresContext());
}

bool nsDisplayFixedPosition::CreateWebRenderCommands(
    mozilla::wr::DisplayListBuilder& aBuilder,
    mozilla::wr::IpcResourceUpdateQueue& aResources,
    const StackingContextHelper& aSc,
    mozilla::layers::RenderRootStateManager* aManager,
    nsDisplayListBuilder* aDisplayListBuilder) {
  SideBits sides = SideBits::eNone;
  if (!mIsFixedBackground) {
    sides = nsLayoutUtils::GetSideBitsForFixedPositionContent(mFrame);
  }

  // We install this RAII scrolltarget tracker so that any
  // nsDisplayCompositorHitTestInfo items inside this fixed-pos item (and that
  // share the same ASR as this item) use the correct scroll target. That way
  // attempts to scroll on those items will scroll the root scroll frame.
  mozilla::wr::DisplayListBuilder::FixedPosScrollTargetTracker tracker(
      aBuilder, GetActiveScrolledRoot(), GetScrollTargetId(), sides);
  return nsDisplayOwnLayer::CreateWebRenderCommands(
      aBuilder, aResources, aSc, aManager, aDisplayListBuilder);
}

bool nsDisplayFixedPosition::UpdateScrollData(
    mozilla::layers::WebRenderScrollData* aData,
    mozilla::layers::WebRenderLayerScrollData* aLayerData) {
  if (aLayerData) {
    if (!mIsFixedBackground) {
      aLayerData->SetFixedPositionSides(
          nsLayoutUtils::GetSideBitsForFixedPositionContent(mFrame));
    }
    aLayerData->SetFixedPositionScrollContainerId(GetScrollTargetId());
  }
  return nsDisplayOwnLayer::UpdateScrollData(aData, aLayerData) | true;
}

void nsDisplayFixedPosition::WriteDebugInfo(std::stringstream& aStream) {
  aStream << nsPrintfCString(" (containerASR %s) (scrolltarget %" PRIu64 ")",
                             ActiveScrolledRoot::ToString(mContainerASR).get(),
                             GetScrollTargetId())
                 .get();
}

TableType GetTableTypeFromFrame(nsIFrame* aFrame) {
  if (aFrame->IsTableFrame()) {
    return TableType::Table;
  }

  if (aFrame->IsTableColFrame()) {
    return TableType::TableCol;
  }

  if (aFrame->IsTableColGroupFrame()) {
    return TableType::TableColGroup;
  }

  if (aFrame->IsTableRowFrame()) {
    return TableType::TableRow;
  }

  if (aFrame->IsTableRowGroupFrame()) {
    return TableType::TableRowGroup;
  }

  if (aFrame->IsTableCellFrame()) {
    return TableType::TableCell;
  }

  MOZ_ASSERT_UNREACHABLE("Invalid frame.");
  return TableType::Table;
}

nsDisplayTableFixedPosition::nsDisplayTableFixedPosition(
    nsDisplayListBuilder* aBuilder, nsIFrame* aFrame, nsDisplayList* aList,
    uint16_t aIndex, nsIFrame* aAncestorFrame)
    : nsDisplayFixedPosition(aBuilder, aFrame, aList, aIndex),
      mAncestorFrame(aAncestorFrame),
      mTableType(GetTableTypeFromFrame(aAncestorFrame)) {
  if (aBuilder->IsRetainingDisplayList()) {
    mAncestorFrame->AddDisplayItem(this);
  }
}

/* static */
nsDisplayTableFixedPosition*
nsDisplayTableFixedPosition::CreateForFixedBackground(
    nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
    nsDisplayBackgroundImage* aImage, uint16_t aIndex,
    nsIFrame* aAncestorFrame) {
  nsDisplayList temp;
  temp.AppendToTop(aImage);

  return MakeDisplayItem<nsDisplayTableFixedPosition>(
      aBuilder, aFrame, &temp, aIndex + 1, aAncestorFrame);
}

nsDisplayStickyPosition::nsDisplayStickyPosition(
    nsDisplayListBuilder* aBuilder, nsIFrame* aFrame, nsDisplayList* aList,
    const ActiveScrolledRoot* aActiveScrolledRoot,
    const ActiveScrolledRoot* aContainerASR)
    : nsDisplayOwnLayer(aBuilder, aFrame, aList, aActiveScrolledRoot),
      mContainerASR(aContainerASR) {
  MOZ_COUNT_CTOR(nsDisplayStickyPosition);
}

void nsDisplayStickyPosition::SetClipChain(
    const DisplayItemClipChain* aClipChain, bool aStore) {
  mClipChain = aClipChain;
  mClip = nullptr;

  MOZ_ASSERT(!mClip,
             "There should never be a clip on this item because no clip moves "
             "with it.");

  if (aStore) {
    mState.mClipChain = aClipChain;
    mState.mClip = mClip;
  }
}

already_AddRefed<Layer> nsDisplayStickyPosition::BuildLayer(
    nsDisplayListBuilder* aBuilder, LayerManager* aManager,
    const ContainerLayerParameters& aContainerParameters) {
  RefPtr<Layer> layer =
      nsDisplayOwnLayer::BuildLayer(aBuilder, aManager, aContainerParameters);

  StickyScrollContainer* stickyScrollContainer =
      StickyScrollContainer::GetStickyScrollContainerForFrame(mFrame);
  if (!stickyScrollContainer) {
    return layer.forget();
  }

  nsIFrame* scrollFrame = do_QueryFrame(stickyScrollContainer->ScrollFrame());
  nsPresContext* presContext = scrollFrame->PresContext();

  // Sticky position frames whose scroll frame is the root scroll frame are
  // reflowed into the scroll-port size if one has been set.
  nsSize scrollFrameSize = scrollFrame->GetSize();
  if (scrollFrame == presContext->PresShell()->GetRootScrollFrame() &&
      presContext->PresShell()->IsVisualViewportSizeSet()) {
    scrollFrameSize = presContext->PresShell()->GetVisualViewportSize();
  }

  nsLayoutUtils::SetFixedPositionLayerData(
      layer, scrollFrame,
      nsRect(scrollFrame->GetOffsetToCrossDoc(ReferenceFrame()),
             scrollFrameSize),
      mFrame, presContext, aContainerParameters);

  ViewID scrollId = nsLayoutUtils::FindOrCreateIDFor(
      stickyScrollContainer->ScrollFrame()->GetScrolledFrame()->GetContent());

  float factor = presContext->AppUnitsPerDevPixel();
  nsRectAbsolute outer;
  nsRectAbsolute inner;
  stickyScrollContainer->GetScrollRanges(mFrame, &outer, &inner);
  LayerRectAbsolute stickyOuter(
      NSAppUnitsToFloatPixels(outer.X(), factor) * aContainerParameters.mXScale,
      NSAppUnitsToFloatPixels(outer.Y(), factor) * aContainerParameters.mYScale,
      NSAppUnitsToFloatPixels(outer.XMost(), factor) *
          aContainerParameters.mXScale,
      NSAppUnitsToFloatPixels(outer.YMost(), factor) *
          aContainerParameters.mYScale);
  LayerRectAbsolute stickyInner(
      NSAppUnitsToFloatPixels(inner.X(), factor) * aContainerParameters.mXScale,
      NSAppUnitsToFloatPixels(inner.Y(), factor) * aContainerParameters.mYScale,
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

bool nsDisplayStickyPosition::CreateWebRenderCommands(
    mozilla::wr::DisplayListBuilder& aBuilder,
    mozilla::wr::IpcResourceUpdateQueue& aResources,
    const StackingContextHelper& aSc, RenderRootStateManager* aManager,
    nsDisplayListBuilder* aDisplayListBuilder) {
  StickyScrollContainer* stickyScrollContainer =
      StickyScrollContainer::GetStickyScrollContainerForFrame(mFrame);
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
    if (!stickyScrollContainer->ScrollFrame()
             ->IsMaybeAsynchronouslyScrolled()) {
      stickyScrollContainer = nullptr;
    }
  }

  Maybe<wr::SpaceAndClipChainHelper> saccHelper;

  if (stickyScrollContainer) {
    float auPerDevPixel = mFrame->PresContext()->AppUnitsPerDevPixel();

    bool snap;
    nsRect itemBounds = GetBounds(aDisplayListBuilder, &snap);

    Maybe<float> topMargin;
    Maybe<float> rightMargin;
    Maybe<float> bottomMargin;
    Maybe<float> leftMargin;
    wr::StickyOffsetBounds vBounds = {0.0, 0.0};
    wr::StickyOffsetBounds hBounds = {0.0, 0.0};
    nsPoint appliedOffset;

    nsRectAbsolute outer;
    nsRectAbsolute inner;
    stickyScrollContainer->GetScrollRanges(mFrame, &outer, &inner);

    nsIFrame* scrollFrame = do_QueryFrame(stickyScrollContainer->ScrollFrame());
    nsPoint offset = scrollFrame->GetOffsetToCrossDoc(ReferenceFrame());

    // Adjust the scrollPort coordinates to be relative to the reference frame,
    // so that it is in the same space as everything else.
    nsRect scrollPort =
        stickyScrollContainer->ScrollFrame()->GetScrollPortRect();
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
      topMargin = Some(NSAppUnitsToFloatPixels(
          itemBounds.y - scrollPort.y - distance, auPerDevPixel));
      // Question: What is the maximum positive ("downward") offset that WR
      // will have to apply to this item in order to prevent the item from
      // visually moving?
      // Answer: Since the item is "sticky" in the range [inner.YMost(),
      // outer.YMost()], the maximum offset will be the size of the range, which
      // is outer.YMost() - inner.YMost().
      vBounds.max =
          NSAppUnitsToFloatPixels(outer.YMost() - inner.YMost(), auPerDevPixel);
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
      bottomMargin = Some(NSAppUnitsToFloatPixels(
          scrollPort.YMost() - itemBounds.YMost() + distance, auPerDevPixel));
      // And here WR will be moving the item upwards rather than downwards so
      // again things are inverted from the previous block.
      vBounds.min =
          NSAppUnitsToFloatPixels(outer.Y() - inner.Y(), auPerDevPixel);
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
      leftMargin = Some(NSAppUnitsToFloatPixels(
          itemBounds.x - scrollPort.x - distance, auPerDevPixel));
      hBounds.max =
          NSAppUnitsToFloatPixels(outer.XMost() - inner.XMost(), auPerDevPixel);
      if (inner.XMost() < 0) {
        appliedOffset.x = std::min(0, outer.XMost()) - inner.XMost();
        MOZ_ASSERT(appliedOffset.x > 0);
      }
    }
    if (outer.X() != inner.X()) {
      nscoord distance = DistanceToRange(outer.X(), inner.X());
      rightMargin = Some(NSAppUnitsToFloatPixels(
          scrollPort.XMost() - itemBounds.XMost() + distance, auPerDevPixel));
      hBounds.min =
          NSAppUnitsToFloatPixels(outer.X() - inner.X(), auPerDevPixel);
      if (appliedOffset.x == 0 && inner.X() > 0) {
        appliedOffset.x = std::max(0, outer.X()) - inner.X();
        MOZ_ASSERT(appliedOffset.x < 0);
      }
    }

    LayoutDeviceRect bounds =
        LayoutDeviceRect::FromAppUnits(itemBounds, auPerDevPixel);
    wr::LayoutVector2D applied = {
        NSAppUnitsToFloatPixels(appliedOffset.x, auPerDevPixel),
        NSAppUnitsToFloatPixels(appliedOffset.y, auPerDevPixel)};
    wr::WrSpatialId spatialId = aBuilder.DefineStickyFrame(
        wr::ToLayoutRect(bounds), topMargin.ptrOr(nullptr),
        rightMargin.ptrOr(nullptr), bottomMargin.ptrOr(nullptr),
        leftMargin.ptrOr(nullptr), vBounds, hBounds, applied);

    saccHelper.emplace(aBuilder, spatialId);
    aManager->CommandBuilder().PushOverrideForASR(mContainerASR, spatialId);
  }

  {
    wr::StackingContextParams params;
    params.clip =
        wr::WrStackingContextClip::ClipChain(aBuilder.CurrentClipChainId());
    StackingContextHelper sc(aSc, GetActiveScrolledRoot(), mFrame, this,
                             aBuilder, params);
    nsDisplayWrapList::CreateWebRenderCommands(aBuilder, aResources, sc,
                                               aManager, aDisplayListBuilder);
  }

  if (stickyScrollContainer) {
    aManager->CommandBuilder().PopOverrideForASR(mContainerASR);
  }

  return true;
}

nsDisplayScrollInfoLayer::nsDisplayScrollInfoLayer(
    nsDisplayListBuilder* aBuilder, nsIFrame* aScrolledFrame,
    nsIFrame* aScrollFrame, const CompositorHitTestInfo& aHitInfo,
    const nsRect& aHitArea)
    : nsDisplayWrapList(aBuilder, aScrollFrame),
      mScrollFrame(aScrollFrame),
      mScrolledFrame(aScrolledFrame),
      mScrollParentId(aBuilder->GetCurrentScrollParentId()),
      mHitInfo(aHitInfo),
      mHitArea(aHitArea) {
#ifdef NS_BUILD_REFCNT_LOGGING
  MOZ_COUNT_CTOR(nsDisplayScrollInfoLayer);
#endif
}

already_AddRefed<Layer> nsDisplayScrollInfoLayer::BuildLayer(
    nsDisplayListBuilder* aBuilder, LayerManager* aManager,
    const ContainerLayerParameters& aContainerParameters) {
  // In general for APZ with event-regions we no longer have a need for
  // scrollinfo layers. However, in some cases, there might be content that
  // cannot be layerized, and so needs to scroll synchronously. To handle those
  // cases, we still want to generate scrollinfo layers.

  return aManager->GetLayerBuilder()->BuildContainerLayerFor(
      aBuilder, aManager, mFrame, this, &mList, aContainerParameters, nullptr,
      FrameLayerBuilder::CONTAINER_ALLOW_PULL_BACKGROUND_COLOR);
}

LayerState nsDisplayScrollInfoLayer::GetLayerState(
    nsDisplayListBuilder* aBuilder, LayerManager* aManager,
    const ContainerLayerParameters& aParameters) {
  return LayerState::LAYER_ACTIVE_EMPTY;
}

UniquePtr<ScrollMetadata> nsDisplayScrollInfoLayer::ComputeScrollMetadata(
    LayerManager* aLayerManager,
    const ContainerLayerParameters& aContainerParameters) {
  nsRect viewport = mScrollFrame->GetRect() - mScrollFrame->GetPosition() +
                    mScrollFrame->GetOffsetToCrossDoc(ReferenceFrame());

  ScrollMetadata metadata = nsLayoutUtils::ComputeScrollMetadata(
      mScrolledFrame, mScrollFrame, mScrollFrame->GetContent(),
      ReferenceFrame(), aLayerManager, mScrollParentId, viewport, Nothing(),
      false, Some(aContainerParameters));
  metadata.GetMetrics().SetIsScrollInfoLayer(true);
  nsIScrollableFrame* scrollableFrame = mScrollFrame->GetScrollTargetFrame();
  if (scrollableFrame) {
    scrollableFrame->NotifyApzTransaction();
  }

  return UniquePtr<ScrollMetadata>(new ScrollMetadata(metadata));
}

bool nsDisplayScrollInfoLayer::UpdateScrollData(
    mozilla::layers::WebRenderScrollData* aData,
    mozilla::layers::WebRenderLayerScrollData* aLayerData) {
  if (aLayerData) {
    UniquePtr<ScrollMetadata> metadata =
        ComputeScrollMetadata(aData->GetManager(), ContainerLayerParameters());
    MOZ_ASSERT(aData);
    MOZ_ASSERT(metadata);
    aLayerData->AppendScrollMetadata(*aData, *metadata);
  }
  return true;
}

bool nsDisplayScrollInfoLayer::CreateWebRenderCommands(
    mozilla::wr::DisplayListBuilder& aBuilder,
    mozilla::wr::IpcResourceUpdateQueue& aResources,
    const StackingContextHelper& aSc,
    mozilla::layers::RenderRootStateManager* aManager,
    nsDisplayListBuilder* aDisplayListBuilder) {
  ScrollableLayerGuid::ViewID scrollId =
      nsLayoutUtils::FindOrCreateIDFor(mScrollFrame->GetContent());

  aBuilder.SetHitTestInfo(scrollId, mHitInfo, SideBits::eNone);
  const LayoutDeviceRect devRect = LayoutDeviceRect::FromAppUnits(
      mHitArea, mScrollFrame->PresContext()->AppUnitsPerDevPixel());

  const wr::LayoutRect rect = wr::ToLayoutRect(devRect);

  aBuilder.PushHitTest(rect, rect, !BackfaceIsHidden());
  aBuilder.ClearHitTestInfo();

  return true;
}

void nsDisplayScrollInfoLayer::WriteDebugInfo(std::stringstream& aStream) {
  aStream << " (scrollframe " << mScrollFrame << " scrolledFrame "
          << mScrolledFrame << ")";
}

nsDisplayZoom::nsDisplayZoom(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                             nsSubDocumentFrame* aSubDocFrame,
                             nsDisplayList* aList, int32_t aAPD,
                             int32_t aParentAPD, nsDisplayOwnLayerFlags aFlags)
    : nsDisplaySubDocument(aBuilder, aFrame, aSubDocFrame, aList, aFlags),
      mAPD(aAPD),
      mParentAPD(aParentAPD) {
  MOZ_COUNT_CTOR(nsDisplayZoom);
}

nsRect nsDisplayZoom::GetBounds(nsDisplayListBuilder* aBuilder,
                                bool* aSnap) const {
  nsRect bounds = nsDisplaySubDocument::GetBounds(aBuilder, aSnap);
  *aSnap = false;
  return bounds.ScaleToOtherAppUnitsRoundOut(mAPD, mParentAPD);
}

void nsDisplayZoom::HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
                            HitTestState* aState,
                            nsTArray<nsIFrame*>* aOutFrames) {
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

bool nsDisplayZoom::ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                      nsRegion* aVisibleRegion) {
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
  if (!(mFlags & nsDisplayOwnLayerFlags::GenerateScrollableLayer) ||
      !usingDisplayPort) {
    retval = mList.ComputeVisibilityForSublist(aBuilder, &visibleRegion,
                                               transformedVisibleRect);
  } else {
    retval = nsDisplaySubDocument::ComputeVisibility(aBuilder, &visibleRegion);
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

nsDisplayAsyncZoom::nsDisplayAsyncZoom(
    nsDisplayListBuilder* aBuilder, nsIFrame* aFrame, nsDisplayList* aList,
    const ActiveScrolledRoot* aActiveScrolledRoot,
    mozilla::layers::FrameMetrics::ViewID aViewID)
    : nsDisplayOwnLayer(aBuilder, aFrame, aList, aActiveScrolledRoot),
      mViewID(aViewID) {
  MOZ_COUNT_CTOR(nsDisplayAsyncZoom);
}

#ifdef NS_BUILD_REFCNT_LOGGING
nsDisplayAsyncZoom::~nsDisplayAsyncZoom() {
  MOZ_COUNT_DTOR(nsDisplayAsyncZoom);
}
#endif

already_AddRefed<Layer> nsDisplayAsyncZoom::BuildLayer(
    nsDisplayListBuilder* aBuilder, LayerManager* aManager,
    const ContainerLayerParameters& aContainerParameters) {
  PresShell* presShell = mFrame->PresShell();
  ContainerLayerParameters containerParameters(
      presShell->GetResolution(), presShell->GetResolution(), nsIntPoint(),
      aContainerParameters);

  RefPtr<Layer> layer =
      nsDisplayOwnLayer::BuildLayer(aBuilder, aManager, containerParameters);

  layer->SetIsAsyncZoomContainer(Some(mViewID));

  layer->SetPostScale(1.0f / presShell->GetResolution(),
                      1.0f / presShell->GetResolution());
  layer->AsContainerLayer()->SetScaleToResolution(presShell->GetResolution());

  return layer.forget();
}

bool nsDisplayAsyncZoom::UpdateScrollData(
    mozilla::layers::WebRenderScrollData* aData,
    mozilla::layers::WebRenderLayerScrollData* aLayerData) {
  bool ret = nsDisplayOwnLayer::UpdateScrollData(aData, aLayerData);
  MOZ_ASSERT(ret);
  if (aLayerData) {
    aLayerData->SetAsyncZoomContainerId(mViewID);
  }
  return ret;
}

///////////////////////////////////////////////////
// nsDisplayTransform Implementation
//

#ifndef DEBUG
static_assert(sizeof(nsDisplayTransform) < 512, "nsDisplayTransform has grown");
#endif

nsDisplayTransform::nsDisplayTransform(nsDisplayListBuilder* aBuilder,
                                       nsIFrame* aFrame, nsDisplayList* aList,
                                       const nsRect& aChildrenBuildingRect,
                                       uint16_t aIndex)
    : nsDisplayHitTestInfoBase(aBuilder, aFrame),
      mTransform(Some(Matrix4x4())),
      mTransformGetter(nullptr),
      mAnimatedGeometryRootForChildren(mAnimatedGeometryRoot),
      mAnimatedGeometryRootForScrollMetadata(mAnimatedGeometryRoot),
      mChildrenBuildingRect(aChildrenBuildingRect),
      mIndex(aIndex),
      mIsTransformSeparator(true),
      mAllowAsyncAnimation(false) {
  MOZ_COUNT_CTOR(nsDisplayTransform);
  MOZ_ASSERT(aFrame, "Must have a frame!");
  Init(aBuilder, aList);
}

nsDisplayTransform::nsDisplayTransform(nsDisplayListBuilder* aBuilder,
                                       nsIFrame* aFrame, nsDisplayList* aList,
                                       const nsRect& aChildrenBuildingRect,
                                       uint16_t aIndex,
                                       bool aAllowAsyncAnimation)
    : nsDisplayHitTestInfoBase(aBuilder, aFrame),
      mTransformGetter(nullptr),
      mAnimatedGeometryRootForChildren(mAnimatedGeometryRoot),
      mAnimatedGeometryRootForScrollMetadata(mAnimatedGeometryRoot),
      mChildrenBuildingRect(aChildrenBuildingRect),
      mIndex(aIndex),
      mIsTransformSeparator(false),
      mAllowAsyncAnimation(aAllowAsyncAnimation) {
  MOZ_COUNT_CTOR(nsDisplayTransform);
  MOZ_ASSERT(aFrame, "Must have a frame!");
  SetReferenceFrameToAncestor(aBuilder);
  Init(aBuilder, aList);
}

nsDisplayTransform::nsDisplayTransform(
    nsDisplayListBuilder* aBuilder, nsIFrame* aFrame, nsDisplayList* aList,
    const nsRect& aChildrenBuildingRect, uint16_t aIndex,
    ComputeTransformFunction aTransformGetter)
    : nsDisplayHitTestInfoBase(aBuilder, aFrame),
      mTransformGetter(aTransformGetter),
      mAnimatedGeometryRootForChildren(mAnimatedGeometryRoot),
      mAnimatedGeometryRootForScrollMetadata(mAnimatedGeometryRoot),
      mChildrenBuildingRect(aChildrenBuildingRect),
      mIndex(aIndex),
      mIsTransformSeparator(false),
      mAllowAsyncAnimation(false) {
  MOZ_COUNT_CTOR(nsDisplayTransform);
  MOZ_ASSERT(aFrame, "Must have a frame!");
  Init(aBuilder, aList);
}

void nsDisplayTransform::SetReferenceFrameToAncestor(
    nsDisplayListBuilder* aBuilder) {
  if (mFrame == aBuilder->RootReferenceFrame()) {
    return;
  }
  nsIFrame* outerFrame = nsLayoutUtils::GetCrossDocParentFrame(mFrame);
  mReferenceFrame = aBuilder->FindReferenceFrameFor(outerFrame);
  mToReferenceFrame = mFrame->GetOffsetToCrossDoc(mReferenceFrame);
  if (nsLayoutUtils::IsFixedPosFrameInDisplayPort(mFrame)) {
    // This is an odd special case. If we are both IsFixedPosFrameInDisplayPort
    // and transformed that we are our own AGR parent.
    // We want our frame to be our AGR because FrameLayerBuilder uses our AGR to
    // determine if we are inside a fixed pos subtree. If we use the outer AGR
    // from outside the fixed pos subtree FLB can't tell that we are fixed pos.
    mAnimatedGeometryRoot = mAnimatedGeometryRootForChildren;
  } else if (mFrame->StyleDisplay()->mPosition ==
                 StylePositionProperty::Sticky &&
             IsStickyFrameActive(aBuilder, mFrame, nullptr)) {
    // Similar to the IsFixedPosFrameInDisplayPort case we are our own AGR.
    // We are inside the sticky position, so our AGR is the sticky positioned
    // frame, which is our AGR, not the parent AGR.
    mAnimatedGeometryRoot = mAnimatedGeometryRootForChildren;
  } else if (mAnimatedGeometryRoot->mParentAGR) {
    mAnimatedGeometryRootForScrollMetadata = mAnimatedGeometryRoot->mParentAGR;
    if (!MayBeAnimated(aBuilder)) {
      // If we're an animated transform then we want the same AGR as our
      // children so that FrameLayerBuilder knows that this layer moves with the
      // transform and won't compute occlusions. If we're not animated then use
      // our parent AGR so that inactive transform layers can go in the same
      // PaintedLayer as surrounding content.
      mAnimatedGeometryRoot = mAnimatedGeometryRoot->mParentAGR;
    }
  }

  SetBuildingRect(aBuilder->GetVisibleRect() + mToReferenceFrame);
}

void nsDisplayTransform::Init(nsDisplayListBuilder* aBuilder,
                              nsDisplayList* aChildren) {
  mShouldFlatten = false;
  mChildren.AppendToTop(aChildren);
  UpdateBounds(aBuilder);
}

bool nsDisplayTransform::ShouldFlattenAway(nsDisplayListBuilder* aBuilder) {
  if (gfxVars::UseWebRender() ||
      !StaticPrefs::layout_display_list_flatten_transform()) {
    return false;
  }

  MOZ_ASSERT(!mShouldFlatten);
  mShouldFlatten = GetTransform().Is2D();
  return mShouldFlatten;
}

/* Returns the delta specified by the transform-origin property.
 * This is a positive delta, meaning that it indicates the direction to move
 * to get from (0, 0) of the frame to the transform origin.  This function is
 * called off the main thread.
 */
/* static */
Point3D nsDisplayTransform::GetDeltaToTransformOrigin(
    const nsIFrame* aFrame, TransformReferenceBox& aRefBox,
    float aAppUnitsPerPixel) {
  MOZ_ASSERT(aFrame, "Can't get delta for a null frame!");
  MOZ_ASSERT(aFrame->IsTransformed() || aFrame->BackfaceIsHidden() ||
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

  const StyleTransformOrigin& transformOrigin = display->mTransformOrigin;
  CSSPoint origin = nsStyleTransformMatrix::Convert2DPosition(
      transformOrigin.horizontal, transformOrigin.vertical, aRefBox);

  if (aFrame->GetStateBits() & NS_FRAME_SVG_LAYOUT) {
    // SVG frames (unlike other frames) have a reference box that can be (and
    // typically is) offset from the TopLeft() of the frame. We need to account
    // for that here.
    origin.x += CSSPixel::FromAppUnits(aRefBox.X());
    origin.y += CSSPixel::FromAppUnits(aRefBox.Y());
  }

  float scale = mozilla::AppUnitsPerCSSPixel() / float(aAppUnitsPerPixel);
  float z = transformOrigin.depth._0;
  return Point3D(origin.x * scale, origin.y * scale, z * scale);
}

/* static */
bool nsDisplayTransform::ComputePerspectiveMatrix(const nsIFrame* aFrame,
                                                  float aAppUnitsPerPixel,
                                                  Matrix4x4& aOutMatrix) {
  MOZ_ASSERT(aFrame, "Can't get delta for a null frame!");
  MOZ_ASSERT(aFrame->IsTransformed() || aFrame->BackfaceIsHidden() ||
                 aFrame->Combines3DTransformWithAncestors(),
             "Shouldn't get a delta for an untransformed frame!");
  MOZ_ASSERT(aOutMatrix.IsIdentity(), "Must have a blank output matrix");

  if (!aFrame->IsTransformed()) {
    return false;
  }

  /* Find our containing block, which is the element that provides the
   * value for perspective we need to use
   */

  // TODO: Is it possible that the cbFrame's bounds haven't been set correctly
  // yet
  // (similar to the aBoundsOverride case for GetResultingTransformMatrix)?
  nsIFrame* cbFrame = aFrame->GetContainingBlock(nsIFrame::SKIP_SCROLLED_FRAME);
  if (!cbFrame) {
    return false;
  }

  /* Grab the values for perspective and perspective-origin (if present) */
  const nsStyleDisplay* cbDisplay = cbFrame->StyleDisplay();
  if (cbDisplay->mChildPerspective.IsNone()) {
    return false;
  }

  MOZ_ASSERT(cbDisplay->mChildPerspective.IsLength());
  // TODO(emilio): Seems quite silly to go through app units just to convert to
  // float pixels below.
  nscoord perspective = cbDisplay->mChildPerspective.length._0.ToAppUnits();
  if (perspective < std::numeric_limits<Float>::epsilon()) {
    return true;
  }

  TransformReferenceBox refBox(cbFrame);

  Point perspectiveOrigin = nsStyleTransformMatrix::Convert2DPosition(
      cbDisplay->mPerspectiveOrigin.horizontal,
      cbDisplay->mPerspectiveOrigin.vertical, refBox, aAppUnitsPerPixel);

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

nsDisplayTransform::FrameTransformProperties::FrameTransformProperties(
    const nsIFrame* aFrame, TransformReferenceBox& aRefBox,
    float aAppUnitsPerPixel)
    : mFrame(aFrame),
      mTranslate(aFrame->StyleDisplay()->mTranslate),
      mRotate(aFrame->StyleDisplay()->mRotate),
      mScale(aFrame->StyleDisplay()->mScale),
      mTransform(aFrame->StyleDisplay()->mTransform),
      mMotion(MotionPathUtils::ResolveMotionPath(aFrame, aRefBox)),
      mToTransformOrigin(
          GetDeltaToTransformOrigin(aFrame, aRefBox, aAppUnitsPerPixel)) {}

/* Wraps up the transform matrix in a change-of-basis matrix pair that
 * translates from local coordinate space to transform coordinate space, then
 * hands it back.
 */
Matrix4x4 nsDisplayTransform::GetResultingTransformMatrix(
    const FrameTransformProperties& aProperties, TransformReferenceBox& aRefBox,
    const nsPoint& aOrigin, float aAppUnitsPerPixel, uint32_t aFlags) {
  return GetResultingTransformMatrixInternal(aProperties, aRefBox, aOrigin,
                                             aAppUnitsPerPixel, aFlags);
}

Matrix4x4 nsDisplayTransform::GetResultingTransformMatrix(
    const nsIFrame* aFrame, const nsPoint& aOrigin, float aAppUnitsPerPixel,
    uint32_t aFlags) {
  // mRect before calling FinishAndStoreOverflow so we don't need the override.
  TransformReferenceBox refBox(aFrame);
  FrameTransformProperties props(aFrame, refBox, aAppUnitsPerPixel);

  return GetResultingTransformMatrixInternal(props, refBox, aOrigin,
                                             aAppUnitsPerPixel, aFlags);
}

static bool ShouldRoundTransformOrigin(const nsIFrame* aFrame) {
  // An SVG frame should not have its translation rounded.
  // Note it's possible that the SVG frame doesn't have an SVG
  // transform but only has a CSS transform.
  return !aFrame || !aFrame->HasAnyStateBits(NS_FRAME_SVG_LAYOUT) ||
         aFrame->IsSVGOuterSVGAnonChildFrame();
}

Matrix4x4 nsDisplayTransform::GetResultingTransformMatrixInternal(
    const FrameTransformProperties& aProperties, TransformReferenceBox& aRefBox,
    const nsPoint& aOrigin, float aAppUnitsPerPixel, uint32_t aFlags) {
  const nsIFrame* frame = aProperties.mFrame;
  NS_ASSERTION(frame || !(aFlags & INCLUDE_PERSPECTIVE),
               "Must have a frame to compute perspective!");

  // Get the underlying transform matrix:

  /* Get the matrix, then change its basis to factor in the origin. */
  Matrix4x4 result;
  // Call IsSVGTransformed() regardless of the value of
  // disp->mSpecifiedTransform, since we still need any
  // parentsChildrenOnlyTransform.
  Matrix svgTransform, parentsChildrenOnlyTransform;
  bool hasSVGTransforms =
      frame &&
      frame->IsSVGTransformed(&svgTransform, &parentsChildrenOnlyTransform);

  bool shouldRound = ShouldRoundTransformOrigin(frame);

  /* Transformed frames always have a transform, or are preserving 3d (and might
   * still have perspective!) */
  if (aProperties.HasTransform()) {
    result = nsStyleTransformMatrix::ReadTransforms(
        aProperties.mTranslate, aProperties.mRotate, aProperties.mScale,
        aProperties.mMotion, aProperties.mTransform, aRefBox,
        aAppUnitsPerPixel);
  } else if (hasSVGTransforms) {
    // Correct the translation components for zoom:
    float pixelsPerCSSPx = AppUnitsPerCSSPixel() / aAppUnitsPerPixel;
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
    float pixelsPerCSSPx = AppUnitsPerCSSPixel() / aAppUnitsPerPixel;
    parentsChildrenOnlyTransform._31 *= pixelsPerCSSPx;
    parentsChildrenOnlyTransform._32 *= pixelsPerCSSPx;

    Point3D frameOffset(
        NSAppUnitsToFloatPixels(-frame->GetPosition().x, aAppUnitsPerPixel),
        NSAppUnitsToFloatPixels(-frame->GetPosition().y, aAppUnitsPerPixel), 0);
    Matrix4x4 parentsChildrenOnlyTransform3D =
        Matrix4x4::From2D(parentsChildrenOnlyTransform)
            .ChangeBasis(frameOffset);

    result *= parentsChildrenOnlyTransform3D;
  }

  Matrix4x4 perspectiveMatrix;
  bool hasPerspective = aFlags & INCLUDE_PERSPECTIVE;
  if (hasPerspective) {
    if (ComputePerspectiveMatrix(frame, aAppUnitsPerPixel, perspectiveMatrix)) {
      result *= perspectiveMatrix;
    }
  }

  if ((aFlags & INCLUDE_PRESERVE3D_ANCESTORS) && frame &&
      frame->Combines3DTransformWithAncestors()) {
    // Include the transform set on our parent
    nsIFrame* parentFrame =
        frame->GetClosestFlattenedTreeAncestorPrimaryFrame();
    NS_ASSERTION(parentFrame && parentFrame->IsTransformed() &&
                     parentFrame->Extend3DContext(),
                 "Preserve3D mismatch!");
    TransformReferenceBox refBox(parentFrame);
    FrameTransformProperties props(parentFrame, refBox, aAppUnitsPerPixel);

    uint32_t flags =
        aFlags & (INCLUDE_PRESERVE3D_ANCESTORS | INCLUDE_PERSPECTIVE);

    // If this frame isn't transformed (but we exist for backface-visibility),
    // then we're not a reference frame so no offset to origin will be added.
    // Otherwise we need to manually translate into our parent's coordinate
    // space.
    if (frame->IsTransformed()) {
      nsLayoutUtils::PostTranslate(result, frame->GetPosition(),
                                   aAppUnitsPerPixel, shouldRound);
    }
    Matrix4x4 parent = GetResultingTransformMatrixInternal(
        props, refBox, nsPoint(0, 0), aAppUnitsPerPixel, flags);
    result = result * parent;
  }

  if (aFlags & OFFSET_BY_ORIGIN) {
    nsLayoutUtils::PostTranslate(result, aOrigin, aAppUnitsPerPixel,
                                 shouldRound);
  }

  return result;
}

bool nsDisplayOpacity::CanUseAsyncAnimations(nsDisplayListBuilder* aBuilder) {
  static constexpr nsCSSPropertyIDSet opacitySet =
      nsCSSPropertyIDSet::OpacityProperties();
  if (ActiveLayerTracker::IsStyleAnimated(aBuilder, mFrame, opacitySet)) {
    return true;
  }

  EffectCompositor::SetPerformanceWarning(
      mFrame, opacitySet,
      AnimationPerformanceWarning(
          AnimationPerformanceWarning::Type::OpacityFrameInactive));

  return false;
}

bool nsDisplayTransform::CanUseAsyncAnimations(nsDisplayListBuilder* aBuilder) {
  return mAllowAsyncAnimation;
}

bool nsDisplayBackgroundColor::CanUseAsyncAnimations(
    nsDisplayListBuilder* aBuilder) {
  return StaticPrefs::gfx_omta_background_color();
}

/* static */
auto nsDisplayTransform::ShouldPrerenderTransformedContent(
    nsDisplayListBuilder* aBuilder, nsIFrame* aFrame, nsRect* aDirtyRect)
    -> PrerenderInfo {
  PrerenderInfo result;
  // If we are in a preserve-3d tree, and we've disallowed async animations, we
  // return No prerender decision directly.
  if ((aFrame->Extend3DContext() ||
       aFrame->Combines3DTransformWithAncestors()) &&
      !aBuilder->GetPreserves3DAllowAsyncAnimation()) {
    return result;
  }

  // Elements whose transform has been modified recently, or which
  // have a compositor-animated transform, can be prerendered. An element
  // might have only just had its transform animated in which case
  // the ActiveLayerManager may not have been notified yet.
  static constexpr nsCSSPropertyIDSet transformSet =
      nsCSSPropertyIDSet::TransformLikeProperties();
  if (!ActiveLayerTracker::IsTransformMaybeAnimated(aFrame) &&
      !EffectCompositor::HasAnimationsForCompositor(
          aFrame, DisplayItemType::TYPE_TRANSFORM)) {
    EffectCompositor::SetPerformanceWarning(
        aFrame, transformSet,
        AnimationPerformanceWarning(
            AnimationPerformanceWarning::Type::TransformFrameInactive));

    // This case happens when we're sure that the frame is not animated and its
    // preserve-3d ancestors are not, either. So we don't need to pre-render.
    // However, this decision shouldn't affect the decisions for other frames in
    // the preserve-3d context. We need this flag to determine whether we should
    // block async animations on other frames in the current preserve-3d tree.
    result.mHasAnimations = false;
    return result;
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
       container;
       container = nsLayoutUtils::GetCrossDocParentFrame(container)) {
    const nsStyleSVGReset* svgReset = container->StyleSVGReset();
    if (svgReset->HasMask() || svgReset->HasClipPath()) {
      return result;
    }
  }

  // If the incoming dirty rect already contains the entire overflow area,
  // we are already rendering the entire content.
  nsRect overflow = aFrame->GetVisualOverflowRectRelativeToSelf();
  if (aDirtyRect->Contains(overflow)) {
    result.mDecision = PrerenderDecision::Full;
    return result;
  }

  float viewportRatioX =
      StaticPrefs::layout_animation_prerender_viewport_ratio_limit_x();
  float viewportRatioY =
      StaticPrefs::layout_animation_prerender_viewport_ratio_limit_y();
  uint32_t absoluteLimitX =
      StaticPrefs::layout_animation_prerender_absolute_limit_x();
  uint32_t absoluteLimitY =
      StaticPrefs::layout_animation_prerender_absolute_limit_y();
  nsSize refSize = aBuilder->RootReferenceFrame()->GetSize();
  // Only prerender if the transformed frame's size is <= a multiple of the
  // reference frame size (~viewport), and less than an absolute limit.
  // Both the ratio and the absolute limit are configurable.
  nsSize relativeLimit(nscoord(refSize.width * viewportRatioX),
                       nscoord(refSize.height * viewportRatioY));
  nsSize absoluteLimit(
      aFrame->PresContext()->DevPixelsToAppUnits(absoluteLimitX),
      aFrame->PresContext()->DevPixelsToAppUnits(absoluteLimitY));
  nsSize maxSize = Min(relativeLimit, absoluteLimit);

  const auto transform = nsLayoutUtils::GetTransformToAncestor(
      aFrame, nsLayoutUtils::GetDisplayRootFrame(aFrame));
  const gfxRect transformedBounds = transform.TransformAndClipBounds(
      gfxRect(overflow.x, overflow.y, overflow.width, overflow.height),
      gfxRect::MaxIntRect());
  const nsSize frameSize =
      nsSize(transformedBounds.width, transformedBounds.height);

  uint64_t maxLimitArea = uint64_t(maxSize.width) * maxSize.height;
  uint64_t frameArea = uint64_t(frameSize.width) * frameSize.height;
  if (frameArea <= maxLimitArea && frameSize <= absoluteLimit) {
    *aDirtyRect = overflow;
    result.mDecision = PrerenderDecision::Full;
    return result;
  }

  if (StaticPrefs::layout_animation_prerender_partial()) {
    *aDirtyRect = nsLayoutUtils::ComputePartialPrerenderArea(*aDirtyRect,
                                                             overflow, maxSize);
    result.mDecision = PrerenderDecision::Partial;
    return result;
  }

  if (frameArea > maxLimitArea) {
    uint64_t appUnitsPerPixel = AppUnitsPerCSSPixel();
    EffectCompositor::SetPerformanceWarning(
        aFrame, transformSet,
        AnimationPerformanceWarning(
            AnimationPerformanceWarning::Type::ContentTooLargeArea,
            {
                int(frameArea / (appUnitsPerPixel * appUnitsPerPixel)),
                int(maxLimitArea / (appUnitsPerPixel * appUnitsPerPixel)),
            }));
  } else {
    EffectCompositor::SetPerformanceWarning(
        aFrame, transformSet,
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

  return result;
}

/* If the matrix is singular, or a hidden backface is shown, the frame won't be
 * visible or hit. */
static bool IsFrameVisible(nsIFrame* aFrame, const Matrix4x4& aMatrix) {
  if (aMatrix.IsSingular()) {
    return false;
  }
  if (aFrame->BackfaceIsHidden() && aMatrix.IsBackfaceVisible()) {
    return false;
  }
  return true;
}

const Matrix4x4Flagged& nsDisplayTransform::GetTransform() const {
  if (mTransform) {
    return *mTransform;
  }

  float scale = mFrame->PresContext()->AppUnitsPerDevPixel();

  if (mTransformGetter) {
    mTransform.emplace(mTransformGetter(mFrame, scale));
    Point3D newOrigin =
        Point3D(NSAppUnitsToFloatPixels(mToReferenceFrame.x, scale),
                NSAppUnitsToFloatPixels(mToReferenceFrame.y, scale), 0.0f);
    mTransform->ChangeBasis(newOrigin.x, newOrigin.y, newOrigin.z);
  } else if (!mIsTransformSeparator) {
    DebugOnly<bool> isReference = mFrame->IsTransformed() ||
                                  mFrame->Combines3DTransformWithAncestors() ||
                                  mFrame->Extend3DContext();
    MOZ_ASSERT(isReference);
    mTransform.emplace(
        GetResultingTransformMatrix(mFrame, ToReferenceFrame(), scale,
                                    INCLUDE_PERSPECTIVE | OFFSET_BY_ORIGIN));
  } else {
    // Use identity matrix
    mTransform.emplace();
  }

  return *mTransform;
}

const Matrix4x4Flagged& nsDisplayTransform::GetInverseTransform() const {
  if (mInverseTransform) {
    return *mInverseTransform;
  }

  MOZ_ASSERT(!GetTransform().IsSingular());

  mInverseTransform.emplace(GetTransform().Inverse());

  return *mInverseTransform;
}

Matrix4x4 nsDisplayTransform::GetTransformForRendering(
    LayoutDevicePoint* aOutOrigin) const {
  if (!mFrame->HasPerspective() || mTransformGetter || mIsTransformSeparator) {
    if (!mTransformGetter && !mIsTransformSeparator && aOutOrigin) {
      // If aOutOrigin is provided, put the offset to origin into it, because
      // we need to keep it separate for webrender. The combination of
      // *aOutOrigin and the returned matrix here should always be equivalent
      // to what GetTransform() would have returned.
      float scale = mFrame->PresContext()->AppUnitsPerDevPixel();
      *aOutOrigin = LayoutDevicePoint::FromAppUnits(ToReferenceFrame(), scale);

      // The rounding behavior should also be the same as GetTransform().
      if (ShouldRoundTransformOrigin(mFrame)) {
        aOutOrigin->Round();
      }
      return GetResultingTransformMatrix(mFrame, nsPoint(0, 0), scale,
                                         INCLUDE_PERSPECTIVE);
    }
    return GetTransform().GetMatrix();
  }
  MOZ_ASSERT(!mTransformGetter);

  float scale = mFrame->PresContext()->AppUnitsPerDevPixel();
  // Don't include perspective transform, or the offset to origin, since
  // nsDisplayPerspective will handle both of those.
  return GetResultingTransformMatrix(mFrame, ToReferenceFrame(), scale, 0);
}

const Matrix4x4& nsDisplayTransform::GetAccumulatedPreserved3DTransform(
    nsDisplayListBuilder* aBuilder) {
  MOZ_ASSERT(!mFrame->Extend3DContext() || IsLeafOf3DContext());

  if (!IsLeafOf3DContext()) {
    return GetTransform().GetMatrix();
  }

  // XXX: should go back to fix mTransformGetter.
  if (!mTransformPreserves3D) {
    const nsIFrame* establisher;  // Establisher of the 3D rendering context.
    for (establisher = mFrame;
         establisher && establisher->Combines3DTransformWithAncestors();
         establisher =
             establisher->GetClosestFlattenedTreeAncestorPrimaryFrame()) {
    }
    const nsIFrame* establisherReference = aBuilder->FindReferenceFrameFor(
        nsLayoutUtils::GetCrossDocParentFrame(establisher));

    nsPoint offset = establisher->GetOffsetToCrossDoc(establisherReference);
    float scale = mFrame->PresContext()->AppUnitsPerDevPixel();
    uint32_t flags =
        INCLUDE_PRESERVE3D_ANCESTORS | INCLUDE_PERSPECTIVE | OFFSET_BY_ORIGIN;
    mTransformPreserves3D = MakeUnique<Matrix4x4>(
        GetResultingTransformMatrix(mFrame, offset, scale, flags));
  }

  return *mTransformPreserves3D;
}

bool nsDisplayTransform::CreateWebRenderCommands(
    mozilla::wr::DisplayListBuilder& aBuilder,
    mozilla::wr::IpcResourceUpdateQueue& aResources,
    const StackingContextHelper& aSc, RenderRootStateManager* aManager,
    nsDisplayListBuilder* aDisplayListBuilder) {
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

    // In ChooseScaleAndSetTransform, we round the offset from the reference
    // frame used to adjust the transform, if there is no transform, or it
    // is just a translation. We need to do the same here.
    position.Round();
  }

  // We don't send animations for transform separator display items.
  uint64_t animationsId =
      mIsTransformSeparator
          ? 0
          : AddAnimationsForWebRender(this, aManager, aDisplayListBuilder,
                                      aBuilder.GetRenderRoot());
  wr::WrAnimationProperty prop{
      wr::WrAnimationType::Transform,
      animationsId,
  };

  Maybe<nsDisplayTransform*> deferredTransformItem;
  if (!mFrame->ChildrenHavePerspective()) {
    // If it has perspective, we create a new scroll data via the
    // UpdateScrollData call because that scenario is more complex. Otherwise
    // we can just stash the transform on the StackingContextHelper and
    // apply it to any scroll data that are created inside this
    // nsDisplayTransform.
    deferredTransformItem = Some(this);
  }

  // Determine if we're possibly animated (= would need an active layer in FLB).
  bool animated = !mIsTransformSeparator &&
                  ActiveLayerTracker::IsTransformMaybeAnimated(Frame());

  wr::StackingContextParams params;
  params.mBoundTransform = &newTransformMatrix;
  params.animation = animationsId ? &prop : nullptr;
  params.mTransformPtr = transformForSC;
  params.prim_flags = !BackfaceIsHidden()
                          ? wr::PrimitiveFlags::IS_BACKFACE_VISIBLE
                          : wr::PrimitiveFlags{0};
  params.mDeferredTransformItem = deferredTransformItem;
  params.mAnimated = animated;
  // Determine if we would have to rasterize any items in local raster space
  // (i.e. disable subpixel AA). We don't always need to rasterize locally even
  // if the stacking context is possibly animated (at the cost of potentially
  // some false negatives with respect to will-change handling), so we pass in
  // this determination separately to accurately match with when FLB would
  // normally disable subpixel AA.
  params.mRasterizeLocally = animated && Frame()->HasAnimationOfTransform();
  params.SetPreserve3D(mFrame->Extend3DContext() && !mIsTransformSeparator);
  params.clip =
      wr::WrStackingContextClip::ClipChain(aBuilder.CurrentClipChainId());

  LayoutDeviceSize boundsSize = LayoutDeviceSize::FromAppUnits(
      mChildBounds.Size(), mFrame->PresContext()->AppUnitsPerDevPixel());

  StackingContextHelper sc(aSc, GetActiveScrolledRoot(), mFrame, this, aBuilder,
                           params, LayoutDeviceRect(position, boundsSize));

  aManager->CommandBuilder().CreateWebRenderCommandsFromDisplayList(
      GetChildren(), this, aDisplayListBuilder, sc, aBuilder, aResources);
  return true;
}

bool nsDisplayTransform::UpdateScrollData(
    mozilla::layers::WebRenderScrollData* aData,
    mozilla::layers::WebRenderLayerScrollData* aLayerData) {
  if (!mFrame->ChildrenHavePerspective()) {
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

bool nsDisplayTransform::ShouldSkipTransform(
    nsDisplayListBuilder* aBuilder) const {
  return (aBuilder->RootReferenceFrame() == mFrame) &&
         aBuilder->IsForGenerateGlyphMask();
}

already_AddRefed<Layer> nsDisplayTransform::BuildLayer(
    nsDisplayListBuilder* aBuilder, LayerManager* aManager,
    const ContainerLayerParameters& aContainerParameters) {
  // While generating a glyph mask, the transform vector of the root frame had
  // been applied into the target context, so stop applying it again here.
  const bool shouldSkipTransform = ShouldSkipTransform(aBuilder);

  /* For frames without transform, it would not be removed for
   * backface hidden here.  But, it would be removed by the init
   * function of nsDisplayTransform.
   */
  const Matrix4x4 newTransformMatrix =
      shouldSkipTransform ? Matrix4x4() : GetTransformForRendering();

  uint32_t flags = FrameLayerBuilder::CONTAINER_ALLOW_PULL_BACKGROUND_COLOR;
  RefPtr<ContainerLayer> container =
      aManager->GetLayerBuilder()->BuildContainerLayerFor(
          aBuilder, aManager, mFrame, this, GetChildren(), aContainerParameters,
          &newTransformMatrix, flags);

  if (!container) {
    return nullptr;
  }

  // Add the preserve-3d flag for this layer, BuildContainerLayerFor clears all
  // flags, so we never need to explicitly unset this flag.
  if (mFrame->Extend3DContext() && !mIsTransformSeparator) {
    container->SetContentFlags(container->GetContentFlags() |
                               Layer::CONTENT_EXTEND_3D_CONTEXT);
  } else {
    container->SetContentFlags(container->GetContentFlags() &
                               ~Layer::CONTENT_EXTEND_3D_CONTEXT);
  }

  if (mAllowAsyncAnimation) {
    mFrame->SetProperty(nsIFrame::RefusedAsyncAnimationProperty(), false);
  }

  // We don't send animations for transform separator display items.
  if (!mIsTransformSeparator) {
    nsDisplayListBuilder::AddAnimationsAndTransitionsToLayer(
        container, aBuilder, this, mFrame, GetType());
  }

  if (mAllowAsyncAnimation && MayBeAnimated(aBuilder)) {
    // Only allow async updates to the transform if we're an animated layer,
    // since that's what triggers us to set the correct AGR in the constructor
    // and makes sure FrameLayerBuilder won't compute occlusions for this layer.
    container->SetUserData(nsIFrame::LayerIsPrerenderedDataKey(),
                           /*the value is irrelevant*/ nullptr);
    container->SetContentFlags(container->GetContentFlags() |
                               Layer::CONTENT_MAY_CHANGE_TRANSFORM);
  } else {
    container->RemoveUserData(nsIFrame::LayerIsPrerenderedDataKey());
    container->SetContentFlags(container->GetContentFlags() &
                               ~Layer::CONTENT_MAY_CHANGE_TRANSFORM);
  }
  return container.forget();
}

bool nsDisplayTransform::MayBeAnimated(nsDisplayListBuilder* aBuilder,
                                       bool aEnforceMinimumSize) const {
  // If EffectCompositor::HasAnimationsForCompositor() is true then we can
  // completely bypass the main thread for this animation, so it is always
  // worthwhile.
  // For ActiveLayerTracker::IsTransformAnimated() cases the main thread is
  // already involved so there is less to be gained.
  // Therefore we check that the *post-transform* bounds of this item are
  // big enough to justify an active layer.
  if (EffectCompositor::HasAnimationsForCompositor(
          mFrame, DisplayItemType::TYPE_TRANSFORM) ||
      (ActiveLayerTracker::IsTransformAnimated(aBuilder, mFrame) &&
       !(aEnforceMinimumSize && IsItemTooSmallForActiveLayer(mFrame)))) {
    return true;
  }
  return false;
}

nsDisplayItem::LayerState nsDisplayTransform::GetLayerState(
    nsDisplayListBuilder* aBuilder, LayerManager* aManager,
    const ContainerLayerParameters& aParameters) {
  // If the transform is 3d, the layer takes part in preserve-3d
  // sorting, or the layer is a separator then we *always* want this
  // to be an active layer.
  // Checking HasPerspective() is needed to handle perspective value 0 when
  // the transform is 2D.
  if (!GetTransform().Is2D() || Combines3DTransformWithAncestors() ||
      mIsTransformSeparator || mFrame->HasPerspective()) {
    return LayerState::LAYER_ACTIVE_FORCE;
  }

  if (MayBeAnimated(aBuilder)) {
    // Returns LayerState::LAYER_ACTIVE_FORCE to avoid flatterning the layer for
    // async animations.
    return LayerState::LAYER_ACTIVE_FORCE;
  }

  // Expect the child display items to have this frame as their animated
  // geometry root (since it will be their reference frame). If they have a
  // different animated geometry root, we'll make this an active layer so the
  // animation can be accelerated.
  return RequiredLayerStateForChildren(aBuilder, aManager, aParameters,
                                       *GetChildren(),
                                       mAnimatedGeometryRootForChildren);
}

bool nsDisplayTransform::ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                           nsRegion* aVisibleRegion) {
  // nsDisplayTransform::GetBounds() returns an empty rect in nested 3d context.
  // Calling mStoredList.RecomputeVisibility below for such transform causes the
  // child display items to end up with empty visible rect.
  // We avoid this by bailing out always if we are dealing with a 3d context.
  if (mFrame->Extend3DContext() || Combines3DTransformWithAncestors()) {
    return true;
  }

  /* As we do this, we need to be sure to
   * untransform the visible rect, since we want everything that's painting to
   * think that it's painting in its original rectangular coordinate space.
   * If we can't untransform, take the entire overflow rect */
  nsRect untransformedVisibleRect;
  if (!UntransformPaintRect(aBuilder, &untransformedVisibleRect)) {
    untransformedVisibleRect = mFrame->GetVisualOverflowRectRelativeToSelf();
  }

  bool snap;
  const nsRect bounds = GetUntransformedBounds(aBuilder, &snap);
  nsRegion visibleRegion;
  visibleRegion.And(bounds, untransformedVisibleRect);
  GetChildren()->ComputeVisibilityForSublist(aBuilder, &visibleRegion,
                                             visibleRegion.GetBounds());

  return true;
}

nsRect nsDisplayTransform::TransformUntransformedBounds(
    nsDisplayListBuilder* aBuilder, const Matrix4x4Flagged& aMatrix) const {
  bool snap;
  const nsRect untransformedBounds = GetUntransformedBounds(aBuilder, &snap);
  // GetTransform always operates in dev pixels.
  const float factor = mFrame->PresContext()->AppUnitsPerDevPixel();
  return nsLayoutUtils::MatrixTransformRect(untransformedBounds, aMatrix,
                                            factor);
}

/**
 * Returns the bounds for this transform. The bounds are calculated during
 * display list building and merging, see |nsDisplayTransform::UpdateBounds()|.
 */
nsRect nsDisplayTransform::GetBounds(nsDisplayListBuilder* aBuilder,
                                     bool* aSnap) const {
  *aSnap = false;
  return mBounds;
}

void nsDisplayTransform::ComputeBounds(nsDisplayListBuilder* aBuilder) {
  MOZ_ASSERT(mFrame->Extend3DContext() || IsLeafOf3DContext());

  /* Some transforms can get empty bounds in 2D, but might get transformed again
   * and get non-empty bounds. A simple example of this would be a 180 degree
   * rotation getting applied twice.
   * We should not depend on transforming bounds level by level.
   *
   * This function collects the bounds of this transform and stores it in
   * nsDisplayListBuilder. If this is not a leaf of a 3D context, we recurse
   * down and include the bounds of the child transforms.
   * The bounds are transformed with the accumulated transformation matrix up to
   * the 3D context root coordinate space.
   */
  nsDisplayListBuilder::AutoAccumulateTransform accTransform(aBuilder);
  accTransform.Accumulate(GetTransform().GetMatrix());

  // Do not dive into another 3D context.
  if (!IsLeafOf3DContext()) {
    for (nsDisplayItem* i : *GetChildren()) {
      i->DoUpdateBoundsPreserves3D(aBuilder);
    }
  }

  /* The child transforms that extend 3D context further will have empty bounds,
   * so the untransformed bounds here is the bounds of all the non-preserve-3d
   * content under this transform.
   */
  const nsRect rect = TransformUntransformedBounds(
      aBuilder, accTransform.GetCurrentTransform());
  aBuilder->AccumulateRect(rect);
}

void nsDisplayTransform::DoUpdateBoundsPreserves3D(
    nsDisplayListBuilder* aBuilder) {
  MOZ_ASSERT(mFrame->Combines3DTransformWithAncestors() ||
             IsTransformSeparator());
  // Updating is not going through to child 3D context.
  ComputeBounds(aBuilder);
}

void nsDisplayTransform::UpdateBounds(nsDisplayListBuilder* aBuilder) {
  UpdateUntransformedBounds(aBuilder);

  if (IsTransformSeparator()) {
    MOZ_ASSERT(GetTransform().IsIdentity());
    mBounds = mChildBounds;
    return;
  }

  if (mFrame->Extend3DContext()) {
    if (!Combines3DTransformWithAncestors()) {
      // The transform establishes a 3D context. |UpdateBoundsFor3D()| will
      // collect the bounds from the child transforms.
      UpdateBoundsFor3D(aBuilder);
    } else {
      // With nested 3D transforms, the 2D bounds might not be useful.
      mBounds = nsRect();
    }

    return;
  }

  MOZ_ASSERT(!mFrame->Extend3DContext());

  // We would like to avoid calculating 2D bounds here for nested 3D transforms,
  // but mix-blend-mode relies on having bounds set. See bug 1556956.

  // A stand-alone transform.
  mBounds = TransformUntransformedBounds(aBuilder, GetTransform());
}

void nsDisplayTransform::UpdateBoundsFor3D(nsDisplayListBuilder* aBuilder) {
  MOZ_ASSERT(mFrame->Extend3DContext() &&
             !mFrame->Combines3DTransformWithAncestors() &&
             !IsTransformSeparator());

  // Always start updating from an establisher of a 3D rendering context.
  nsDisplayListBuilder::AutoAccumulateRect accRect(aBuilder);
  nsDisplayListBuilder::AutoAccumulateTransform accTransform(aBuilder);
  accTransform.StartRoot();
  ComputeBounds(aBuilder);
  mBounds = aBuilder->GetAccumulatedRect();
}

void nsDisplayTransform::UpdateUntransformedBounds(
    nsDisplayListBuilder* aBuilder) {
  mChildBounds = GetChildren()->GetClippedBoundsWithRespectToASR(
      aBuilder, mActiveScrolledRoot);
}

#ifdef DEBUG_HIT
#  include <time.h>
#endif

/* HitTest does some fun stuff with matrix transforms to obtain the answer. */
void nsDisplayTransform::HitTest(nsDisplayListBuilder* aBuilder,
                                 const nsRect& aRect, HitTestState* aState,
                                 nsTArray<nsIFrame*>* aOutFrames) {
  if (aState->mInPreserves3D) {
    GetChildren()->HitTest(aBuilder, aRect, aState, aOutFrames);
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
    Point4D point =
        matrix.ProjectPoint(Point(NSAppUnitsToFloatPixels(aRect.x, factor),
                                  NSAppUnitsToFloatPixels(aRect.y, factor)));
    if (!point.HasPositiveWCoord()) {
      return;
    }

    Point point2d = point.As2DPoint();

    resultingRect =
        nsRect(NSFloatPixelsToAppUnits(float(point2d.x), factor),
               NSFloatPixelsToAppUnits(float(point2d.y), factor), 1, 1);

  } else {
    Rect originalRect(NSAppUnitsToFloatPixels(aRect.x, factor),
                      NSAppUnitsToFloatPixels(aRect.y, factor),
                      NSAppUnitsToFloatPixels(aRect.width, factor),
                      NSAppUnitsToFloatPixels(aRect.height, factor));

    bool snap;
    nsRect childBounds = GetUntransformedBounds(aBuilder, &snap);
    Rect childGfxBounds(NSAppUnitsToFloatPixels(childBounds.x, factor),
                        NSAppUnitsToFloatPixels(childBounds.y, factor),
                        NSAppUnitsToFloatPixels(childBounds.width, factor),
                        NSAppUnitsToFloatPixels(childBounds.height, factor));

    Rect rect = matrix.ProjectRectBounds(originalRect, childGfxBounds);

    resultingRect =
        nsRect(NSFloatPixelsToAppUnits(float(rect.X()), factor),
               NSFloatPixelsToAppUnits(float(rect.Y()), factor),
               NSFloatPixelsToAppUnits(float(rect.Width()), factor),
               NSFloatPixelsToAppUnits(float(rect.Height()), factor));
  }

  if (resultingRect.IsEmpty()) {
    return;
  }

#ifdef DEBUG_HIT
  printf("Frame: %p\n", dynamic_cast<void*>(mFrame));
  printf("  Untransformed point: (%f, %f)\n", resultingRect.X(),
         resultingRect.Y());
  uint32_t originalFrameCount = aOutFrames.Length();
#endif

  GetChildren()->HitTest(aBuilder, resultingRect, aState, aOutFrames);

#ifdef DEBUG_HIT
  if (originalFrameCount != aOutFrames.Length())
    printf("  Hit! Time: %f, first frame: %p\n", static_cast<double>(clock()),
           dynamic_cast<void*>(aOutFrames.ElementAt(0)));
  printf("=== end of hit test ===\n");
#endif
}

float nsDisplayTransform::GetHitDepthAtPoint(nsDisplayListBuilder* aBuilder,
                                             const nsPoint& aPoint) {
  // GetTransform always operates in dev pixels.
  float factor = mFrame->PresContext()->AppUnitsPerDevPixel();
  Matrix4x4 matrix = GetAccumulatedPreserved3DTransform(aBuilder);

  NS_ASSERTION(IsFrameVisible(mFrame, matrix),
               "We can't have hit a frame that isn't visible!");

  Matrix4x4 inverse = matrix;
  inverse.Invert();
  Point4D point =
      inverse.ProjectPoint(Point(NSAppUnitsToFloatPixels(aPoint.x, factor),
                                 NSAppUnitsToFloatPixels(aPoint.y, factor)));

  Point point2d = point.As2DPoint();

  Point3D transformed = matrix.TransformPoint(Point3D(point2d.x, point2d.y, 0));
  return transformed.z;
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
nsRegion nsDisplayTransform::GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                                             bool* aSnap) const {
  *aSnap = false;

  nsRect untransformedVisible;
  if (!UntransformBuildingRect(aBuilder, &untransformedVisible)) {
    return nsRegion();
  }

  const Matrix4x4Flagged& matrix = GetTransform();
  Matrix matrix2d;
  if (!matrix.Is2D(&matrix2d) || !matrix2d.PreservesAxisAlignedRectangles()) {
    return nsRegion();
  }

  nsRegion result;

  bool tmpSnap;
  const nsRect bounds = GetUntransformedBounds(aBuilder, &tmpSnap);
  const nsRegion opaque = ::GetOpaqueRegion(aBuilder, GetChildren(), bounds);

  if (opaque.Contains(untransformedVisible)) {
    result = GetBuildingRect().Intersect(GetBounds(aBuilder, &tmpSnap));
  }
  return result;
}

nsRect nsDisplayTransform::GetComponentAlphaBounds(
    nsDisplayListBuilder* aBuilder) const {
  if (GetChildren()->GetComponentAlphaBounds(aBuilder).IsEmpty()) {
    return nsRect();
  }

  bool snap;
  return GetBounds(aBuilder, &snap);
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
 * @return The smallest rectangle containing the image of the transformed
 *         rectangle.
 */
nsRect nsDisplayTransform::TransformRect(const nsRect& aUntransformedBounds,
                                         const nsIFrame* aFrame,
                                         TransformReferenceBox& aRefBox) {
  MOZ_ASSERT(aFrame, "Can't take the transform based on a null frame!");

  float factor = aFrame->PresContext()->AppUnitsPerDevPixel();

  uint32_t flags =
      INCLUDE_PERSPECTIVE | OFFSET_BY_ORIGIN | INCLUDE_PRESERVE3D_ANCESTORS;
  FrameTransformProperties props(aFrame, aRefBox, factor);
  return nsLayoutUtils::MatrixTransformRect(
      aUntransformedBounds,
      GetResultingTransformMatrixInternal(props, aRefBox, nsPoint(0, 0), factor,
                                          flags),
      factor);
}

bool nsDisplayTransform::UntransformRect(const nsRect& aTransformedBounds,
                                         const nsRect& aChildBounds,
                                         const nsIFrame* aFrame,
                                         nsRect* aOutRect) {
  MOZ_ASSERT(aFrame, "Can't take the transform based on a null frame!");

  float factor = aFrame->PresContext()->AppUnitsPerDevPixel();

  uint32_t flags =
      INCLUDE_PERSPECTIVE | OFFSET_BY_ORIGIN | INCLUDE_PRESERVE3D_ANCESTORS;

  Matrix4x4 transform =
      GetResultingTransformMatrix(aFrame, nsPoint(0, 0), factor, flags);
  if (transform.IsSingular()) {
    return false;
  }

  RectDouble result(NSAppUnitsToFloatPixels(aTransformedBounds.x, factor),
                    NSAppUnitsToFloatPixels(aTransformedBounds.y, factor),
                    NSAppUnitsToFloatPixels(aTransformedBounds.width, factor),
                    NSAppUnitsToFloatPixels(aTransformedBounds.height, factor));

  RectDouble childGfxBounds(
      NSAppUnitsToFloatPixels(aChildBounds.x, factor),
      NSAppUnitsToFloatPixels(aChildBounds.y, factor),
      NSAppUnitsToFloatPixels(aChildBounds.width, factor),
      NSAppUnitsToFloatPixels(aChildBounds.height, factor));

  result = transform.Inverse().ProjectRectBounds(result, childGfxBounds);
  *aOutRect = nsLayoutUtils::RoundGfxRectToAppRect(ThebesRect(result), factor);
  return true;
}

bool nsDisplayTransform::UntransformRect(nsDisplayListBuilder* aBuilder,
                                         const nsRect& aRect,
                                         nsRect* aOutRect) const {
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
  nsRect childBounds = GetUntransformedBounds(aBuilder, &snap);
  RectDouble childGfxBounds(
      NSAppUnitsToFloatPixels(childBounds.x, factor),
      NSAppUnitsToFloatPixels(childBounds.y, factor),
      NSAppUnitsToFloatPixels(childBounds.width, factor),
      NSAppUnitsToFloatPixels(childBounds.height, factor));

  /* We want to untransform the matrix, so invert the transformation first! */
  result = GetInverseTransform().ProjectRectBounds(result, childGfxBounds);

  *aOutRect = nsLayoutUtils::RoundGfxRectToAppRect(ThebesRect(result), factor);

  return true;
}

void nsDisplayTransform::WriteDebugInfo(std::stringstream& aStream) {
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

  aStream << " allowAsync(" << (mAllowAsyncAnimation ? "true" : "false") << ")";
  aStream << " childrenBuildingRect" << mChildrenBuildingRect;
}

nsDisplayPerspective::nsDisplayPerspective(nsDisplayListBuilder* aBuilder,
                                           nsIFrame* aFrame,
                                           nsDisplayList* aList)
    : nsDisplayHitTestInfoBase(aBuilder, aFrame) {
  mList.AppendToTop(aList);
  MOZ_ASSERT(mList.Count() == 1);
  MOZ_ASSERT(mList.GetTop()->GetType() == DisplayItemType::TYPE_TRANSFORM);
  mAnimatedGeometryRoot = aBuilder->FindAnimatedGeometryRootFor(
      mFrame->GetContainingBlock(nsIFrame::SKIP_SCROLLED_FRAME));
}

already_AddRefed<Layer> nsDisplayPerspective::BuildLayer(
    nsDisplayListBuilder* aBuilder, LayerManager* aManager,
    const ContainerLayerParameters& aContainerParameters) {
  float appUnitsPerPixel = mFrame->PresContext()->AppUnitsPerDevPixel();

  Matrix4x4 perspectiveMatrix;
  DebugOnly<bool> hasPerspective = nsDisplayTransform::ComputePerspectiveMatrix(
      mFrame, appUnitsPerPixel, perspectiveMatrix);
  MOZ_ASSERT(hasPerspective, "Why did we create nsDisplayPerspective?");

  /*
   * ClipListToRange can remove our child after we were created.
   */
  if (!GetChildren()->GetTop()) {
    return nullptr;
  }

  /*
   * The resulting matrix is still in the coordinate space of the transformed
   * frame. Append a translation to the reference frame coordinates.
   */
  nsDisplayTransform* transform =
      static_cast<nsDisplayTransform*>(GetChildren()->GetTop());

  Point3D newOrigin =
      Point3D(NSAppUnitsToFloatPixels(transform->ToReferenceFrame().x,
                                      appUnitsPerPixel),
              NSAppUnitsToFloatPixels(transform->ToReferenceFrame().y,
                                      appUnitsPerPixel),
              0.0f);
  Point3D roundedOrigin(NS_round(newOrigin.x), NS_round(newOrigin.y), 0);

  perspectiveMatrix.PostTranslate(roundedOrigin);

  RefPtr<ContainerLayer> container =
      aManager->GetLayerBuilder()->BuildContainerLayerFor(
          aBuilder, aManager, mFrame, this, GetChildren(), aContainerParameters,
          &perspectiveMatrix, 0);

  if (!container) {
    return nullptr;
  }

  // Sort of a lie, but we want to pretend that the perspective layer extends a
  // 3d context so that it gets its transform combined with children. Might need
  // a better name that reflects this use case and isn't specific to
  // preserve-3d.
  container->SetContentFlags(container->GetContentFlags() |
                             Layer::CONTENT_EXTEND_3D_CONTEXT);
  container->SetTransformIsPerspective(true);

  return container.forget();
}

LayerState nsDisplayPerspective::GetLayerState(
    nsDisplayListBuilder* aBuilder, LayerManager* aManager,
    const ContainerLayerParameters& aParameters) {
  return LayerState::LAYER_ACTIVE_FORCE;
}

nsRegion nsDisplayPerspective::GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                                               bool* aSnap) const {
  if (!GetChildren()->GetTop()) {
    *aSnap = false;
    return nsRegion();
  }

  return GetChildren()->GetTop()->GetOpaqueRegion(aBuilder, aSnap);
}

bool nsDisplayPerspective::CreateWebRenderCommands(
    mozilla::wr::DisplayListBuilder& aBuilder,
    mozilla::wr::IpcResourceUpdateQueue& aResources,
    const StackingContextHelper& aSc, RenderRootStateManager* aManager,
    nsDisplayListBuilder* aDisplayListBuilder) {
  float appUnitsPerPixel = mFrame->PresContext()->AppUnitsPerDevPixel();
  Matrix4x4 perspectiveMatrix;
  DebugOnly<bool> hasPerspective = nsDisplayTransform::ComputePerspectiveMatrix(
      mFrame, appUnitsPerPixel, perspectiveMatrix);
  MOZ_ASSERT(hasPerspective, "Why did we create nsDisplayPerspective?");

  /*
   * ClipListToRange can remove our child after we were created.
   */
  if (!GetChildren()->GetTop()) {
    return false;
  }

  /*
   * The resulting matrix is still in the coordinate space of the transformed
   * frame. Append a translation to the reference frame coordinates.
   */
  nsDisplayTransform* transform =
      static_cast<nsDisplayTransform*>(GetChildren()->GetTop());

  Point3D newOrigin =
      Point3D(NSAppUnitsToFloatPixels(transform->ToReferenceFrame().x,
                                      appUnitsPerPixel),
              NSAppUnitsToFloatPixels(transform->ToReferenceFrame().y,
                                      appUnitsPerPixel),
              0.0f);
  Point3D roundedOrigin(NS_round(newOrigin.x), NS_round(newOrigin.y), 0);

  perspectiveMatrix.PostTranslate(roundedOrigin);

  nsIFrame* perspectiveFrame =
      mFrame->GetContainingBlock(nsIFrame::SKIP_SCROLLED_FRAME);

  // Passing true here is always correct, since perspective always combines
  // transforms with the descendants. However that'd make WR do a lot of work
  // that it doesn't really need to do if there aren't other transforms forming
  // part of the 3D context.
  //
  // WR knows how to treat perspective in that case, so the only thing we need
  // to do is to ensure we pass true when we're involved in a 3d context in any
  // other way via the transform-style property on either the transformed frame
  // or the perspective frame in order to not confuse WR's preserve-3d code in
  // very awful ways.
  bool preserve3D =
      mFrame->Extend3DContext() || perspectiveFrame->Extend3DContext();

  wr::StackingContextParams params;
  params.mTransformPtr = &perspectiveMatrix;
  params.reference_frame_kind = wr::WrReferenceFrameKind::Perspective;
  params.prim_flags = !BackfaceIsHidden()
                          ? wr::PrimitiveFlags::IS_BACKFACE_VISIBLE
                          : wr::PrimitiveFlags{0};
  params.SetPreserve3D(preserve3D);
  params.clip =
      wr::WrStackingContextClip::ClipChain(aBuilder.CurrentClipChainId());

  Maybe<uint64_t> scrollingRelativeTo;
  for (auto* asr = GetActiveScrolledRoot(); asr; asr = asr->mParent) {
    if (nsLayoutUtils::IsAncestorFrameCrossDoc(
            asr->mScrollableFrame->GetScrolledFrame(), perspectiveFrame)) {
      scrollingRelativeTo.emplace(asr->GetViewId());
      break;
    }
  }

  // We put the perspective reference frame wrapping the transformed frame,
  // even though there may be arbitrarily nested scroll frames in between.
  //
  // We need to know how many ancestor scroll-frames are we nested in, in order
  // for the async scrolling code in WebRender to calculate the right
  // transformation for the perspective contents.
  params.scrolling_relative_to = scrollingRelativeTo.ptrOr(nullptr);

  StackingContextHelper sc(aSc, GetActiveScrolledRoot(), mFrame, this, aBuilder,
                           params);

  aManager->CommandBuilder().CreateWebRenderCommandsFromDisplayList(
      GetChildren(), this, aDisplayListBuilder, sc, aBuilder, aResources);

  return true;
}

nsDisplayText::nsDisplayText(nsDisplayListBuilder* aBuilder,
                             nsTextFrame* aFrame,
                             const Maybe<bool>& aIsSelected)
    : nsPaintedDisplayItem(aBuilder, aFrame),
      mOpacity(1.0f),
      mVisIStartEdge(0),
      mVisIEndEdge(0) {
  MOZ_COUNT_CTOR(nsDisplayText);
  mIsFrameSelected = aIsSelected;
  mBounds = mFrame->GetVisualOverflowRectRelativeToSelf() + ToReferenceFrame();
  // Bug 748228
  mBounds.Inflate(mFrame->PresContext()->AppUnitsPerDevPixel());
}

bool nsDisplayText::CanApplyOpacity() const {
  if (IsSelected()) {
    return false;
  }

  nsTextFrame* f = static_cast<nsTextFrame*>(mFrame);
  const nsStyleText* textStyle = f->StyleText();
  if (textStyle->HasTextShadow()) {
    return false;
  }

  nsTextFrame::TextDecorations decorations;
  f->GetTextDecorations(f->PresContext(), nsTextFrame::eResolvedColors,
                        decorations);
  if (decorations.HasDecorationLines()) {
    return false;
  }

  return true;
}

void nsDisplayText::Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx) {
  AUTO_PROFILER_LABEL("nsDisplayText::Paint", GRAPHICS);

  DrawTargetAutoDisableSubpixelAntialiasing disable(aCtx->GetDrawTarget(),
                                                    IsSubpixelAADisabled());
  RenderToContext(aCtx, aBuilder);
}

bool nsDisplayText::CreateWebRenderCommands(
    mozilla::wr::DisplayListBuilder& aBuilder,
    mozilla::wr::IpcResourceUpdateQueue& aResources,
    const StackingContextHelper& aSc, RenderRootStateManager* aManager,
    nsDisplayListBuilder* aDisplayListBuilder) {
  auto* f = static_cast<nsTextFrame*>(mFrame);
  auto appUnitsPerDevPixel = f->PresContext()->AppUnitsPerDevPixel();

  nsRect bounds = f->WebRenderBounds() + ToReferenceFrame();
  // Bug 748228
  bounds.Inflate(appUnitsPerDevPixel);

  if (bounds.IsEmpty()) {
    return true;
  }

  gfx::Point deviceOffset =
      LayoutDevicePoint::FromAppUnits(bounds.TopLeft(), appUnitsPerDevPixel)
          .ToUnknownPoint();

  // Clipping the bounds to the PaintRect (factoring in what's covered by parent
  // frames) let's us early reject a bunch of things, but it can produce
  // incorrect results for shadows, because they can translate things back into
  // view. Also if we're selected we might have some shadows from the
  // ::selected and ::inctive-selected pseudo-selectors. So don't do this
  // optimization if we have shadows or a selection.
  if (!(IsSelected() || f->StyleText()->HasTextShadow())) {
    nsRect visible = GetPaintRect();
    visible.Inflate(3 * appUnitsPerDevPixel);
    bounds = bounds.Intersect(visible);
  }

  RefPtr<gfxContext> textDrawer = aBuilder.GetTextContext(
      aResources, aSc, aManager, this, bounds, deviceOffset);

  aBuilder.StartGroup(this);

  RenderToContext(textDrawer, aDisplayListBuilder, true);
  const bool result = textDrawer->GetTextDrawer()->Finish();

  if (result) {
    aBuilder.FinishGroup();
  } else {
    aBuilder.CancelGroup();
  }

  return result;
}

void nsDisplayText::RenderToContext(gfxContext* aCtx,
                                    nsDisplayListBuilder* aBuilder,
                                    bool aIsRecording) {
  nsTextFrame* f = static_cast<nsTextFrame*>(mFrame);

  // Add 1 pixel of dirty area around mVisibleRect to allow us to paint
  // antialiased pixels beyond the measured text extents.
  // This is temporary until we do this in the actual calculation of text
  // extents.
  auto A2D = mFrame->PresContext()->AppUnitsPerDevPixel();
  LayoutDeviceRect extraVisible =
      LayoutDeviceRect::FromAppUnits(GetPaintRect(), A2D);
  extraVisible.Inflate(1);

  gfxRect pixelVisible(extraVisible.x, extraVisible.y, extraVisible.width,
                       extraVisible.height);
  pixelVisible.Inflate(2);
  pixelVisible.RoundOut();

  bool willClip = !aBuilder->IsForGenerateGlyphMask() && !aIsRecording;
  if (willClip) {
    aCtx->NewPath();
    aCtx->Rectangle(pixelVisible);
    aCtx->Clip();
  }

  NS_ASSERTION(mVisIStartEdge >= 0, "illegal start edge");
  NS_ASSERTION(mVisIEndEdge >= 0, "illegal end edge");

  gfxContextMatrixAutoSaveRestore matrixSR;

  nsPoint framePt = ToReferenceFrame();
  if (f->Style()->IsTextCombined()) {
    float scaleFactor = nsTextFrame::GetTextCombineScaleFactor(f);
    if (scaleFactor != 1.0f) {
      if (auto* textDrawer = aCtx->GetTextDrawer()) {
        // WebRender doesn't support scaling text like this yet
        textDrawer->FoundUnsupportedFeature();
        return;
      }
      matrixSR.SetContext(aCtx);
      // Setup matrix to compress text for text-combine-upright if
      // necessary. This is done here because we want selection be
      // compressed at the same time as text.
      gfxPoint pt = nsLayoutUtils::PointToGfxPoint(framePt, A2D);
      gfxMatrix mat = aCtx->CurrentMatrixDouble()
                          .PreTranslate(pt)
                          .PreScale(scaleFactor, 1.0)
                          .PreTranslate(-pt);
      aCtx->SetMatrixDouble(mat);
    }
  }
  nsTextFrame::PaintTextParams params(aCtx);
  params.framePt = gfx::Point(framePt.x, framePt.y);
  params.dirtyRect = extraVisible;

  if (aBuilder->IsForGenerateGlyphMask()) {
    params.state = nsTextFrame::PaintTextParams::GenerateTextMask;
  } else {
    params.state = nsTextFrame::PaintTextParams::PaintText;
  }

  f->PaintText(params, mVisIStartEdge, mVisIEndEdge, ToReferenceFrame(),
               IsSelected(), mOpacity);

  if (willClip) {
    aCtx->PopClip();
  }
}

bool nsDisplayText::IsSelected() const {
  if (mIsFrameSelected.isNothing()) {
    MOZ_ASSERT((nsTextFrame*)do_QueryFrame(mFrame));
    auto* f = static_cast<nsTextFrame*>(mFrame);
    mIsFrameSelected.emplace(f->IsSelected());
  }

  return mIsFrameSelected.value();
}

class nsDisplayTextGeometry : public nsDisplayItemGenericGeometry {
 public:
  nsDisplayTextGeometry(nsDisplayText* aItem, nsDisplayListBuilder* aBuilder)
      : nsDisplayItemGenericGeometry(aItem, aBuilder),
        mOpacity(aItem->Opacity()),
        mVisIStartEdge(aItem->VisIStartEdge()),
        mVisIEndEdge(aItem->VisIEndEdge()) {
    nsTextFrame* f = static_cast<nsTextFrame*>(aItem->Frame());
    f->GetTextDecorations(f->PresContext(), nsTextFrame::eResolvedColors,
                          mDecorations);
  }

  /**
   * We store the computed text decorations here since they are
   * computed using style data from parent frames. Any changes to these
   * styles will only invalidate the parent frame and not this frame.
   */
  nsTextFrame::TextDecorations mDecorations;
  float mOpacity;
  nscoord mVisIStartEdge;
  nscoord mVisIEndEdge;
};

nsDisplayItemGeometry* nsDisplayText::AllocateGeometry(
    nsDisplayListBuilder* aBuilder) {
  return new nsDisplayTextGeometry(this, aBuilder);
}

void nsDisplayText::ComputeInvalidationRegion(
    nsDisplayListBuilder* aBuilder, const nsDisplayItemGeometry* aGeometry,
    nsRegion* aInvalidRegion) const {
  const nsDisplayTextGeometry* geometry =
      static_cast<const nsDisplayTextGeometry*>(aGeometry);
  nsTextFrame* f = static_cast<nsTextFrame*>(mFrame);

  nsTextFrame::TextDecorations decorations;
  f->GetTextDecorations(f->PresContext(), nsTextFrame::eResolvedColors,
                        decorations);

  bool snap;
  const nsRect& newRect = geometry->mBounds;
  nsRect oldRect = GetBounds(aBuilder, &snap);
  if (decorations != geometry->mDecorations ||
      mVisIStartEdge != geometry->mVisIStartEdge ||
      mVisIEndEdge != geometry->mVisIEndEdge ||
      !oldRect.IsEqualInterior(newRect) ||
      !geometry->mBorderRect.IsEqualInterior(GetBorderRect()) ||
      mOpacity != geometry->mOpacity) {
    aInvalidRegion->Or(oldRect, newRect);
  }
}

void nsDisplayText::WriteDebugInfo(std::stringstream& aStream) {
#ifdef DEBUG
  aStream << " (\"";

  nsTextFrame* f = static_cast<nsTextFrame*>(mFrame);
  nsCString buf;
  int32_t totalContentLength;
  f->ToCString(buf, &totalContentLength);

  aStream << buf.get() << "\")";
#endif
}

nsDisplayEffectsBase::nsDisplayEffectsBase(
    nsDisplayListBuilder* aBuilder, nsIFrame* aFrame, nsDisplayList* aList,
    const ActiveScrolledRoot* aActiveScrolledRoot, bool aClearClipChain)
    : nsDisplayWrapList(aBuilder, aFrame, aList, aActiveScrolledRoot,
                        aClearClipChain),
      mHandleOpacity(false) {
  MOZ_COUNT_CTOR(nsDisplayEffectsBase);
}

nsDisplayEffectsBase::nsDisplayEffectsBase(nsDisplayListBuilder* aBuilder,
                                           nsIFrame* aFrame,
                                           nsDisplayList* aList)
    : nsDisplayWrapList(aBuilder, aFrame, aList), mHandleOpacity(false) {
  MOZ_COUNT_CTOR(nsDisplayEffectsBase);
}

nsRegion nsDisplayEffectsBase::GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                                               bool* aSnap) const {
  *aSnap = false;
  return nsRegion();
}

void nsDisplayEffectsBase::HitTest(nsDisplayListBuilder* aBuilder,
                                   const nsRect& aRect, HitTestState* aState,
                                   nsTArray<nsIFrame*>* aOutFrames) {
  nsPoint rectCenter(aRect.x + aRect.width / 2, aRect.y + aRect.height / 2);
  if (nsSVGIntegrationUtils::HitTestFrameForEffects(
          mFrame, rectCenter - ToReferenceFrame())) {
    mList.HitTest(aBuilder, aRect, aState, aOutFrames);
  }
}

gfxRect nsDisplayEffectsBase::BBoxInUserSpace() const {
  return nsSVGUtils::GetBBox(mFrame);
}

gfxPoint nsDisplayEffectsBase::UserSpaceOffset() const {
  return nsSVGUtils::FrameSpaceInCSSPxToUserSpaceOffset(mFrame);
}

void nsDisplayEffectsBase::ComputeInvalidationRegion(
    nsDisplayListBuilder* aBuilder, const nsDisplayItemGeometry* aGeometry,
    nsRegion* aInvalidRegion) const {
  auto* geometry = static_cast<const nsDisplaySVGEffectGeometry*>(aGeometry);
  bool snap;
  nsRect bounds = GetBounds(aBuilder, &snap);
  if (geometry->mFrameOffsetToReferenceFrame != ToReferenceFrame() ||
      geometry->mUserSpaceOffset != UserSpaceOffset() ||
      !geometry->mBBox.IsEqualInterior(BBoxInUserSpace()) ||
      geometry->mOpacity != mFrame->StyleEffects()->mOpacity ||
      geometry->mHandleOpacity != ShouldHandleOpacity()) {
    // Filter and mask output can depend on the location of the frame's user
    // space and on the frame's BBox. We need to invalidate if either of these
    // change relative to the reference frame.
    // Invalidations from our inactive layer manager are not enough to catch
    // some of these cases because filters can produce output even if there's
    // nothing in the filter input.
    aInvalidRegion->Or(bounds, geometry->mBounds);
  }
}

bool nsDisplayEffectsBase::ValidateSVGFrame() {
  const nsIContent* content = mFrame->GetContent();
  bool hasSVGLayout = (mFrame->GetStateBits() & NS_FRAME_SVG_LAYOUT);
  if (hasSVGLayout) {
    nsSVGDisplayableFrame* svgFrame = do_QueryFrame(mFrame);
    if (!svgFrame || !mFrame->GetContent()->IsSVGElement()) {
      NS_ASSERTION(false, "why?");
      return false;
    }
    if (!static_cast<const SVGElement*>(content)->HasValidDimensions()) {
      return false;  // The SVG spec says not to draw filters for this
    }
  }

  return true;
}

typedef nsSVGIntegrationUtils::PaintFramesParams PaintFramesParams;

static void ComputeMaskGeometry(PaintFramesParams& aParams) {
  // Properties are added lazily and may have been removed by a restyle, so
  // make sure all applicable ones are set again.
  nsIFrame* firstFrame =
      nsLayoutUtils::FirstContinuationOrIBSplitSibling(aParams.frame);

  const nsStyleSVGReset* svgReset = firstFrame->StyleSVGReset();

  nsTArray<nsSVGMaskFrame*> maskFrames;
  // XXX check return value?
  SVGObserverUtils::GetAndObserveMasks(firstFrame, &maskFrames);

  if (maskFrames.Length() == 0) {
    return;
  }

  gfxContext& ctx = aParams.ctx;
  nsIFrame* frame = aParams.frame;

  nsPoint offsetToUserSpace =
      nsLayoutUtils::ComputeOffsetToUserSpace(aParams.builder, aParams.frame);

  gfxPoint devPixelOffsetToUserSpace = nsLayoutUtils::PointToGfxPoint(
      offsetToUserSpace, frame->PresContext()->AppUnitsPerDevPixel());

  gfxContextMatrixAutoSaveRestore matSR(&ctx);
  ctx.SetMatrixDouble(
      ctx.CurrentMatrixDouble().PreTranslate(devPixelOffsetToUserSpace));

  // Convert boaderArea and dirtyRect to user space.
  int32_t appUnitsPerDevPixel = frame->PresContext()->AppUnitsPerDevPixel();
  nsRect userSpaceBorderArea = aParams.borderArea - offsetToUserSpace;
  nsRect userSpaceDirtyRect = aParams.dirtyRect - offsetToUserSpace;

  // Union all mask layer rectangles in user space.
  gfxRect maskInUserSpace;
  for (size_t i = 0; i < maskFrames.Length(); i++) {
    nsSVGMaskFrame* maskFrame = maskFrames[i];
    gfxRect currentMaskSurfaceRect;

    if (maskFrame) {
      currentMaskSurfaceRect = maskFrame->GetMaskArea(aParams.frame);
    } else {
      nsCSSRendering::ImageLayerClipState clipState;
      nsCSSRendering::GetImageLayerClip(
          svgReset->mMask.mLayers[i], frame, *frame->StyleBorder(),
          userSpaceBorderArea, userSpaceDirtyRect, false, /* aWillPaintBorder */
          appUnitsPerDevPixel, &clipState);
      currentMaskSurfaceRect = clipState.mDirtyRectInDevPx;
    }

    maskInUserSpace = maskInUserSpace.Union(currentMaskSurfaceRect);
  }

  if (!maskInUserSpace.IsEmpty()) {
    aParams.maskRect = Some(ToRect(maskInUserSpace));
  } else {
    aParams.maskRect = Nothing();
  }
}

nsDisplayMasksAndClipPaths::nsDisplayMasksAndClipPaths(
    nsDisplayListBuilder* aBuilder, nsIFrame* aFrame, nsDisplayList* aList,
    const ActiveScrolledRoot* aActiveScrolledRoot)
    : nsDisplayEffectsBase(aBuilder, aFrame, aList, aActiveScrolledRoot, true) {
  MOZ_COUNT_CTOR(nsDisplayMasksAndClipPaths);

  nsPresContext* presContext = mFrame->PresContext();
  uint32_t flags =
      aBuilder->GetBackgroundPaintFlags() | nsCSSRendering::PAINTBG_MASK_IMAGE;
  const nsStyleSVGReset* svgReset = aFrame->StyleSVGReset();
  NS_FOR_VISIBLE_IMAGE_LAYERS_BACK_TO_FRONT(i, svgReset->mMask) {
    if (!svgReset->mMask.mLayers[i].mImage.IsResolved()) {
      continue;
    }
    bool isTransformedFixed;
    nsBackgroundLayerState state = nsCSSRendering::PrepareImageLayer(
        presContext, aFrame, flags, mFrame->GetRectRelativeToSelf(),
        mFrame->GetRectRelativeToSelf(), svgReset->mMask.mLayers[i],
        &isTransformedFixed);
    mDestRects.AppendElement(state.mDestArea);
  }
}

static bool CanMergeDisplayMaskFrame(nsIFrame* aFrame) {
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

bool nsDisplayMasksAndClipPaths::CanMerge(const nsDisplayItem* aItem) const {
  // Items for the same content element should be merged into a single
  // compositing group.
  if (!HasDifferentFrame(aItem) || !HasSameTypeAndClip(aItem) ||
      !HasSameContent(aItem)) {
    return false;
  }

  return CanMergeDisplayMaskFrame(mFrame) &&
         CanMergeDisplayMaskFrame(aItem->Frame());
}

bool nsDisplayMasksAndClipPaths::IsValidMask() {
  if (!ValidateSVGFrame()) {
    return false;
  }

  if (mFrame->StyleEffects()->mOpacity == 0.0f && mHandleOpacity) {
    return false;
  }

  nsIFrame* firstFrame =
      nsLayoutUtils::FirstContinuationOrIBSplitSibling(mFrame);

  if (SVGObserverUtils::GetAndObserveClipPath(firstFrame, nullptr) ==
          SVGObserverUtils::eHasRefsSomeInvalid ||
      SVGObserverUtils::GetAndObserveMasks(firstFrame, nullptr) ==
          SVGObserverUtils::eHasRefsSomeInvalid) {
    return false;
  }

  return true;
}

already_AddRefed<Layer> nsDisplayMasksAndClipPaths::BuildLayer(
    nsDisplayListBuilder* aBuilder, LayerManager* aManager,
    const ContainerLayerParameters& aContainerParameters) {
  if (!IsValidMask()) {
    return nullptr;
  }

  RefPtr<ContainerLayer> container =
      aManager->GetLayerBuilder()->BuildContainerLayerFor(
          aBuilder, aManager, mFrame, this, &mList, aContainerParameters,
          nullptr);

  return container.forget();
}

bool nsDisplayMasksAndClipPaths::PaintMask(nsDisplayListBuilder* aBuilder,
                                           gfxContext* aMaskContext,
                                           bool* aMaskPainted) {
  MOZ_ASSERT(aMaskContext->GetDrawTarget()->GetFormat() == SurfaceFormat::A8);

  imgDrawingParams imgParams(aBuilder->GetImageDecodeFlags());
  nsRect borderArea = nsRect(ToReferenceFrame(), mFrame->GetSize());
  nsSVGIntegrationUtils::PaintFramesParams params(
      *aMaskContext, mFrame, mBounds, borderArea, aBuilder, nullptr,
      mHandleOpacity, imgParams);
  ComputeMaskGeometry(params);
  bool painted = nsSVGIntegrationUtils::PaintMask(params);
  if (aMaskPainted) {
    *aMaskPainted = painted;
  }

  nsDisplayMasksAndClipPathsGeometry::UpdateDrawResult(this, imgParams.result);

  return imgParams.result == ImgDrawResult::SUCCESS ||
         imgParams.result == ImgDrawResult::SUCCESS_NOT_COMPLETE;
}

LayerState nsDisplayMasksAndClipPaths::GetLayerState(
    nsDisplayListBuilder* aBuilder, LayerManager* aManager,
    const ContainerLayerParameters& aParameters) {
  if (CanPaintOnMaskLayer(aManager)) {
    LayerState result = RequiredLayerStateForChildren(
        aBuilder, aManager, aParameters, mList, GetAnimatedGeometryRoot());
    // When we're not active, FrameLayerBuilder will call PaintAsLayer()
    // on us during painting. In that case we don't want a mask layer to
    // be created, because PaintAsLayer() takes care of applying the mask.
    // So we return LayerState::LAYER_SVG_EFFECTS instead of
    // LayerState::LAYER_INACTIVE so that FrameLayerBuilder doesn't set a mask
    // layer on our layer.
    return result == LayerState::LAYER_INACTIVE ? LayerState::LAYER_SVG_EFFECTS
                                                : result;
  }

  return LayerState::LAYER_SVG_EFFECTS;
}

bool nsDisplayMasksAndClipPaths::CanPaintOnMaskLayer(LayerManager* aManager) {
  if (!aManager->IsWidgetLayerManager()) {
    return false;
  }

  if (!nsSVGIntegrationUtils::IsMaskResourceReady(mFrame)) {
    return false;
  }

  if (StaticPrefs::layers_draw_mask_debug()) {
    return false;
  }

  // We don't currently support this item creating a mask
  // for both the clip-path, and rounded rect clipping.
  if (GetClip().GetRoundedRectCount() != 0) {
    return false;
  }

  return true;
}

bool nsDisplayMasksAndClipPaths::ComputeVisibility(
    nsDisplayListBuilder* aBuilder, nsRegion* aVisibleRegion) {
  // Our children may be made translucent or arbitrarily deformed so we should
  // not allow them to subtract area from aVisibleRegion.
  nsRegion childrenVisible(GetPaintRect());
  nsRect r = GetPaintRect().Intersect(mList.GetClippedBounds(aBuilder));
  mList.ComputeVisibilityForSublist(aBuilder, &childrenVisible, r);
  return true;
}

void nsDisplayMasksAndClipPaths::ComputeInvalidationRegion(
    nsDisplayListBuilder* aBuilder, const nsDisplayItemGeometry* aGeometry,
    nsRegion* aInvalidRegion) const {
  nsDisplayEffectsBase::ComputeInvalidationRegion(aBuilder, aGeometry,
                                                  aInvalidRegion);

  auto* geometry =
      static_cast<const nsDisplayMasksAndClipPathsGeometry*>(aGeometry);
  bool snap;
  nsRect bounds = GetBounds(aBuilder, &snap);

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
    const nsStyleSVGReset* svgReset = mFrame->StyleSVGReset();
    NS_FOR_VISIBLE_IMAGE_LAYERS_BACK_TO_FRONT(i, svgReset->mMask) {
      const auto& image = svgReset->mMask.mLayers[i].mImage;
      if (image.IsImageRequestType()) {
        aInvalidRegion->Or(*aInvalidRegion, bounds);
        break;
      }
    }
  }
}

void nsDisplayMasksAndClipPaths::PaintAsLayer(nsDisplayListBuilder* aBuilder,
                                              gfxContext* aCtx,
                                              LayerManager* aManager) {
  // Clip the drawing target by mVisibleRect, which contains the visible
  // region of the target frame and its out-of-flow and inflow descendants.
  gfxContext* context = aCtx;

  Rect bounds = NSRectToRect(GetPaintRect(),
                             mFrame->PresContext()->AppUnitsPerDevPixel());
  bounds.RoundOut();
  context->Clip(bounds);

  imgDrawingParams imgParams(aBuilder->GetImageDecodeFlags());
  nsRect borderArea = nsRect(ToReferenceFrame(), mFrame->GetSize());
  nsSVGIntegrationUtils::PaintFramesParams params(
      *aCtx, mFrame, GetPaintRect(), borderArea, aBuilder, aManager,
      mHandleOpacity, imgParams);

  ComputeMaskGeometry(params);

  nsSVGIntegrationUtils::PaintMaskAndClipPath(params);

  context->PopClip();

  nsDisplayMasksAndClipPathsGeometry::UpdateDrawResult(this, imgParams.result);
}

void nsDisplayMasksAndClipPaths::PaintWithContentsPaintCallback(
    nsDisplayListBuilder* aBuilder, gfxContext* aCtx,
    const std::function<void()>& aPaintChildren) {
  // Clip the drawing target by mVisibleRect, which contains the visible
  // region of the target frame and its out-of-flow and inflow descendants.
  gfxContext* context = aCtx;

  Rect bounds = NSRectToRect(GetPaintRect(),
                             mFrame->PresContext()->AppUnitsPerDevPixel());
  bounds.RoundOut();
  context->Clip(bounds);

  imgDrawingParams imgParams(aBuilder->GetImageDecodeFlags());
  nsRect borderArea = nsRect(ToReferenceFrame(), mFrame->GetSize());
  nsSVGIntegrationUtils::PaintFramesParams params(*aCtx, mFrame, GetPaintRect(),
                                                  borderArea, aBuilder, nullptr,
                                                  mHandleOpacity, imgParams);

  ComputeMaskGeometry(params);

  nsSVGIntegrationUtils::PaintMaskAndClipPath(params, aPaintChildren);

  context->PopClip();

  nsDisplayMasksAndClipPathsGeometry::UpdateDrawResult(this, imgParams.result);
}

static Maybe<wr::WrClipId> CreateSimpleClipRegion(
    const nsDisplayMasksAndClipPaths& aDisplayItem,
    wr::DisplayListBuilder& aBuilder) {
  nsIFrame* frame = aDisplayItem.Frame();
  auto* style = frame->StyleSVGReset();
  MOZ_ASSERT(style->HasClipPath() || style->HasMask());
  if (!nsSVGIntegrationUtils::UsingSimpleClipPathForFrame(frame)) {
    return Nothing();
  }

  const auto& clipPath = style->mClipPath;
  const auto& shape = *clipPath.AsShape()._0;

  auto appUnitsPerDevPixel = frame->PresContext()->AppUnitsPerDevPixel();
  const nsRect refBox =
      nsLayoutUtils::ComputeGeometryBox(frame, clipPath.AsShape()._1);

  AutoTArray<wr::ComplexClipRegion, 1> clipRegions;

  wr::LayoutRect rect;
  switch (shape.tag) {
    case StyleBasicShape::Tag::Inset: {
      const nsRect insetRect = ShapeUtils::ComputeInsetRect(shape, refBox) +
                               aDisplayItem.ToReferenceFrame();

      nscoord radii[8] = {0};

      if (ShapeUtils::ComputeInsetRadii(shape, insetRect, refBox, radii)) {
        clipRegions.AppendElement(
            wr::ToComplexClipRegion(insetRect, radii, appUnitsPerDevPixel));
      }

      rect = wr::ToLayoutRect(
          LayoutDeviceRect::FromAppUnits(insetRect, appUnitsPerDevPixel));
      break;
    }
    case StyleBasicShape::Tag::Ellipse:
    case StyleBasicShape::Tag::Circle: {
      nsPoint center = ShapeUtils::ComputeCircleOrEllipseCenter(shape, refBox);

      nsSize radii;
      if (shape.IsEllipse()) {
        radii = ShapeUtils::ComputeEllipseRadii(shape, center, refBox);
      } else {
        nscoord radius = ShapeUtils::ComputeCircleRadius(shape, center, refBox);
        radii = {radius, radius};
      }

      nsRect ellipseRect(aDisplayItem.ToReferenceFrame() + center -
                             nsPoint(radii.width, radii.height),
                         radii * 2);

      nscoord ellipseRadii[8];
      for (const auto corner : mozilla::AllPhysicalHalfCorners()) {
        ellipseRadii[corner] =
            HalfCornerIsX(corner) ? radii.width : radii.height;
      }

      clipRegions.AppendElement(wr::ToComplexClipRegion(
          ellipseRect, ellipseRadii, appUnitsPerDevPixel));

      rect = wr::ToLayoutRect(
          LayoutDeviceRect::FromAppUnits(ellipseRect, appUnitsPerDevPixel));
      break;
    }
    default:
      // Please don't add more exceptions, try to find a way to define the clip
      // without using a mask image.
      //
      // And if you _really really_ need to add an exception, add it to
      // nsSVGIntegrationUtils::UsingSimpleClipPathForFrame
      MOZ_ASSERT_UNREACHABLE("Unhandled shape id?");
      return Nothing();
  }
  wr::WrClipId clipId =
      aBuilder.DefineClip(Nothing(), rect, &clipRegions, nullptr);
  return Some(clipId);
}

enum class HandleOpacity {
  No,
  Yes,
};

static Maybe<std::pair<wr::WrClipId, HandleOpacity>> CreateWRClipPathAndMasks(
    nsDisplayMasksAndClipPaths* aDisplayItem, const LayoutDeviceRect& aBounds,
    wr::IpcResourceUpdateQueue& aResources, wr::DisplayListBuilder& aBuilder,
    const StackingContextHelper& aSc, layers::RenderRootStateManager* aManager,
    nsDisplayListBuilder* aDisplayListBuilder) {
  if (auto clip = CreateSimpleClipRegion(*aDisplayItem, aBuilder)) {
    return Some(std::make_pair(*clip, HandleOpacity::Yes));
  }

  Maybe<wr::ImageMask> mask = aManager->CommandBuilder().BuildWrMaskImage(
      aDisplayItem, aBuilder, aResources, aSc, aDisplayListBuilder, aBounds);
  if (!mask) {
    return Nothing();
  }

  wr::WrClipId clipId = aBuilder.DefineClip(
      Nothing(), wr::ToLayoutRect(aBounds), nullptr, mask.ptr());

  return Some(std::make_pair(clipId, HandleOpacity::No));
}

bool nsDisplayMasksAndClipPaths::CreateWebRenderCommands(
    mozilla::wr::DisplayListBuilder& aBuilder,
    mozilla::wr::IpcResourceUpdateQueue& aResources,
    const StackingContextHelper& aSc,
    mozilla::layers::RenderRootStateManager* aManager,
    nsDisplayListBuilder* aDisplayListBuilder) {
  bool snap;
  auto appUnitsPerDevPixel = mFrame->PresContext()->AppUnitsPerDevPixel();
  nsRect displayBounds = GetBounds(aDisplayListBuilder, &snap);
  LayoutDeviceRect bounds =
      LayoutDeviceRect::FromAppUnits(displayBounds, appUnitsPerDevPixel);

  Maybe<std::pair<wr::WrClipId, HandleOpacity>> clip = CreateWRClipPathAndMasks(
      this, bounds, aResources, aBuilder, aSc, aManager, aDisplayListBuilder);

  Maybe<StackingContextHelper> layer;
  const StackingContextHelper* sc = &aSc;
  if (clip) {
    // Create a new stacking context to attach the mask to, ensuring the mask is
    // applied to the aggregate, and not the individual elements.

    // The stacking context shouldn't have any offset.
    bounds.MoveTo(0, 0);

    wr::WrClipId clipId = clip->first;

    Maybe<float> opacity = clip->second == HandleOpacity::Yes
                               ? Some(mFrame->StyleEffects()->mOpacity)
                               : Nothing();

    wr::StackingContextParams params;
    params.clip = wr::WrStackingContextClip::ClipId(clipId);
    params.opacity = opacity.ptrOr(nullptr);
    layer.emplace(aSc, GetActiveScrolledRoot(), mFrame, this, aBuilder, params,
                  bounds);
    sc = layer.ptr();
  }

  nsDisplayEffectsBase::CreateWebRenderCommands(aBuilder, aResources, *sc,
                                                aManager, aDisplayListBuilder);

  return true;
}

Maybe<nsRect> nsDisplayMasksAndClipPaths::GetClipWithRespectToASR(
    nsDisplayListBuilder* aBuilder, const ActiveScrolledRoot* aASR) const {
  if (const DisplayItemClip* clip =
          DisplayItemClipChain::ClipForASR(GetClipChain(), aASR)) {
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
  MOZ_ASSERT(false, "item should have finite clip with respect to aASR");
#endif
  return Nothing();
}

#ifdef MOZ_DUMP_PAINTING
void nsDisplayMasksAndClipPaths::PrintEffects(nsACString& aTo) {
  nsIFrame* firstFrame =
      nsLayoutUtils::FirstContinuationOrIBSplitSibling(mFrame);
  bool first = true;
  aTo += " effects=(";
  if (mHandleOpacity) {
    first = false;
    aTo += nsPrintfCString("opacity(%f)", mFrame->StyleEffects()->mOpacity);
  }
  nsSVGClipPathFrame* clipPathFrame;
  // XXX Check return value?
  SVGObserverUtils::GetAndObserveClipPath(firstFrame, &clipPathFrame);
  if (clipPathFrame) {
    if (!first) {
      aTo += ", ";
    }
    aTo += nsPrintfCString(
        "clip(%s)", clipPathFrame->IsTrivial() ? "trivial" : "non-trivial");
    first = false;
  } else if (mFrame->StyleSVGReset()->HasClipPath()) {
    if (!first) {
      aTo += ", ";
    }
    aTo += "clip(basic-shape)";
    first = false;
  }

  nsTArray<nsSVGMaskFrame*> masks;
  // XXX check return value?
  SVGObserverUtils::GetAndObserveMasks(firstFrame, &masks);
  if (!masks.IsEmpty() && masks[0]) {
    if (!first) {
      aTo += ", ";
    }
    aTo += "mask";
  }
  aTo += ")";
}
#endif

already_AddRefed<Layer> nsDisplayBackdropRootContainer::BuildLayer(
    nsDisplayListBuilder* aBuilder, LayerManager* aManager,
    const ContainerLayerParameters& aContainerParameters) {
  RefPtr<Layer> container = aManager->GetLayerBuilder()->BuildContainerLayerFor(
      aBuilder, aManager, mFrame, this, &mList, aContainerParameters, nullptr);
  if (!container) {
    return nullptr;
  }

  return container.forget();
}

LayerState nsDisplayBackdropRootContainer::GetLayerState(
    nsDisplayListBuilder* aBuilder, LayerManager* aManager,
    const ContainerLayerParameters& aParameters) {
  return RequiredLayerStateForChildren(aBuilder, aManager, aParameters, mList,
                                       GetAnimatedGeometryRoot());
}

bool nsDisplayBackdropRootContainer::CreateWebRenderCommands(
    mozilla::wr::DisplayListBuilder& aBuilder,
    mozilla::wr::IpcResourceUpdateQueue& aResources,
    const StackingContextHelper& aSc,
    mozilla::layers::RenderRootStateManager* aManager,
    nsDisplayListBuilder* aDisplayListBuilder) {
  wr::StackingContextParams params;
  params.is_backdrop_root = true;
  params.clip =
      wr::WrStackingContextClip::ClipChain(aBuilder.CurrentClipChainId());
  StackingContextHelper sc(aSc, GetActiveScrolledRoot(), mFrame, this, aBuilder,
                           params);

  nsDisplayWrapList::CreateWebRenderCommands(aBuilder, aResources, sc, aManager,
                                             aDisplayListBuilder);
  return true;
}

/* static */
bool nsDisplayBackdropFilters::CanCreateWebRenderCommands(
    nsDisplayListBuilder* aBuilder, nsIFrame* aFrame) {
  return nsSVGIntegrationUtils::CanCreateWebRenderFiltersForFrame(aFrame);
}

bool nsDisplayBackdropFilters::CreateWebRenderCommands(
    mozilla::wr::DisplayListBuilder& aBuilder,
    mozilla::wr::IpcResourceUpdateQueue& aResources,
    const StackingContextHelper& aSc,
    mozilla::layers::RenderRootStateManager* aManager,
    nsDisplayListBuilder* aDisplayListBuilder) {
  WrFiltersHolder wrFilters;
  Maybe<nsRect> filterClip;
  auto filterChain = mFrame->StyleEffects()->mBackdropFilters.AsSpan();
  if (!nsSVGIntegrationUtils::CreateWebRenderCSSFilters(filterChain, mFrame,
                                                        wrFilters) &&
      !nsSVGIntegrationUtils::BuildWebRenderFilters(mFrame, filterChain,
                                                    wrFilters, filterClip)) {
    return false;
  }

  nsCSSRendering::ImageLayerClipState clip;
  nsCSSRendering::GetImageLayerClip(
      mFrame->StyleBackground()->BottomLayer(), mFrame, *mFrame->StyleBorder(),
      mBackdropRect, mBackdropRect, false,
      mFrame->PresContext()->AppUnitsPerDevPixel(), &clip);

  LayoutDeviceRect bounds = LayoutDeviceRect::FromAppUnits(
      mBackdropRect, mFrame->PresContext()->AppUnitsPerDevPixel());

  wr::ComplexClipRegion region =
      wr::ToComplexClipRegion(clip.mBGClipArea, clip.mRadii,
                              mFrame->PresContext()->AppUnitsPerDevPixel());

  aBuilder.PushBackdropFilter(wr::ToLayoutRect(bounds), region,
                              wrFilters.filters, wrFilters.filter_datas,
                              !BackfaceIsHidden());

  wr::StackingContextParams params;
  params.clip =
      wr::WrStackingContextClip::ClipChain(aBuilder.CurrentClipChainId());
  StackingContextHelper sc(aSc, GetActiveScrolledRoot(), mFrame, this, aBuilder,
                           params);

  nsDisplayWrapList::CreateWebRenderCommands(aBuilder, aResources, sc, aManager,
                                             aDisplayListBuilder);
  return true;
}

/* static */
nsDisplayFilters::nsDisplayFilters(nsDisplayListBuilder* aBuilder,
                                   nsIFrame* aFrame, nsDisplayList* aList)
    : nsDisplayEffectsBase(aBuilder, aFrame, aList),
      mEffectsBounds(aFrame->GetVisualOverflowRectRelativeToSelf()) {
  MOZ_COUNT_CTOR(nsDisplayFilters);
}

already_AddRefed<Layer> nsDisplayFilters::BuildLayer(
    nsDisplayListBuilder* aBuilder, LayerManager* aManager,
    const ContainerLayerParameters& aContainerParameters) {
  if (!ValidateSVGFrame()) {
    return nullptr;
  }

  if (mFrame->StyleEffects()->mOpacity == 0.0f && mHandleOpacity) {
    return nullptr;
  }

  nsIFrame* firstFrame =
      nsLayoutUtils::FirstContinuationOrIBSplitSibling(mFrame);

  // We may exist for a mix of CSS filter functions and/or references to SVG
  // filters.  If we have invalid references to SVG filters then we paint
  // nothing, so no need for a layer.
  if (SVGObserverUtils::GetAndObserveFilters(firstFrame, nullptr) ==
      SVGObserverUtils::eHasRefsSomeInvalid) {
    return nullptr;
  }

  ContainerLayerParameters newContainerParameters = aContainerParameters;
  newContainerParameters.mDisableSubpixelAntialiasingInDescendants = true;

  RefPtr<ContainerLayer> container =
      aManager->GetLayerBuilder()->BuildContainerLayerFor(
          aBuilder, aManager, mFrame, this, &mList, newContainerParameters,
          nullptr);
  return container.forget();
}

LayerState nsDisplayFilters::GetLayerState(
    nsDisplayListBuilder* aBuilder, LayerManager* aManager,
    const ContainerLayerParameters& aParameters) {
  return LayerState::LAYER_SVG_EFFECTS;
}

bool nsDisplayFilters::ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                         nsRegion* aVisibleRegion) {
  nsPoint offset = ToReferenceFrame();
  nsRect dirtyRect = nsSVGIntegrationUtils::GetRequiredSourceForInvalidArea(
                         mFrame, GetPaintRect() - offset) +
                     offset;

  // Our children may be made translucent or arbitrarily deformed so we should
  // not allow them to subtract area from aVisibleRegion.
  nsRegion childrenVisible(dirtyRect);
  nsRect r = dirtyRect.Intersect(
      mList.GetClippedBoundsWithRespectToASR(aBuilder, mActiveScrolledRoot));
  mList.ComputeVisibilityForSublist(aBuilder, &childrenVisible, r);
  return true;
}

void nsDisplayFilters::ComputeInvalidationRegion(
    nsDisplayListBuilder* aBuilder, const nsDisplayItemGeometry* aGeometry,
    nsRegion* aInvalidRegion) const {
  nsDisplayEffectsBase::ComputeInvalidationRegion(aBuilder, aGeometry,
                                                  aInvalidRegion);

  auto* geometry = static_cast<const nsDisplayFiltersGeometry*>(aGeometry);

  if (aBuilder->ShouldSyncDecodeImages() &&
      geometry->ShouldInvalidateToSyncDecodeImages()) {
    bool snap;
    nsRect bounds = GetBounds(aBuilder, &snap);
    aInvalidRegion->Or(*aInvalidRegion, bounds);
  }
}

void nsDisplayFilters::PaintAsLayer(nsDisplayListBuilder* aBuilder,
                                    gfxContext* aCtx, LayerManager* aManager) {
  imgDrawingParams imgParams(aBuilder->GetImageDecodeFlags());
  nsRect borderArea = nsRect(ToReferenceFrame(), mFrame->GetSize());
  nsSVGIntegrationUtils::PaintFramesParams params(
      *aCtx, mFrame, GetPaintRect(), borderArea, aBuilder, aManager,
      mHandleOpacity, imgParams);
  nsSVGIntegrationUtils::PaintFilter(params);
  nsDisplayFiltersGeometry::UpdateDrawResult(this, imgParams.result);
}

bool nsDisplayFilters::CanCreateWebRenderCommands() {
  return nsSVGIntegrationUtils::CanCreateWebRenderFiltersForFrame(mFrame);
}

bool nsDisplayFilters::CreateWebRenderCommands(
    mozilla::wr::DisplayListBuilder& aBuilder,
    mozilla::wr::IpcResourceUpdateQueue& aResources,
    const StackingContextHelper& aSc,
    mozilla::layers::RenderRootStateManager* aManager,
    nsDisplayListBuilder* aDisplayListBuilder) {
  float auPerDevPixel = mFrame->PresContext()->AppUnitsPerDevPixel();

  WrFiltersHolder wrFilters;
  Maybe<nsRect> filterClip;
  auto filterChain = mFrame->StyleEffects()->mFilters.AsSpan();
  if (!nsSVGIntegrationUtils::CreateWebRenderCSSFilters(filterChain, mFrame,
                                                        wrFilters) &&
      !nsSVGIntegrationUtils::BuildWebRenderFilters(mFrame, filterChain,
                                                    wrFilters, filterClip)) {
    return false;
  }

  wr::WrStackingContextClip clip;
  if (filterClip) {
    auto devPxRect = LayoutDeviceRect::FromAppUnits(
        filterClip.value() + ToReferenceFrame(), auPerDevPixel);
    wr::WrClipId clipId =
        aBuilder.DefineClip(Nothing(), wr::ToLayoutRect(devPxRect));
    clip = wr::WrStackingContextClip::ClipId(clipId);
  } else {
    clip = wr::WrStackingContextClip::ClipChain(aBuilder.CurrentClipChainId());
  }

  float opacity = mFrame->StyleEffects()->mOpacity;
  wr::StackingContextParams params;
  params.mFilters = std::move(wrFilters.filters);
  params.mFilterDatas = std::move(wrFilters.filter_datas);
  params.opacity = opacity != 1.0f && mHandleOpacity ? &opacity : nullptr;
  params.clip = clip;
  StackingContextHelper sc(aSc, GetActiveScrolledRoot(), mFrame, this, aBuilder,
                           params);

  nsDisplayEffectsBase::CreateWebRenderCommands(aBuilder, aResources, sc,
                                                aManager, aDisplayListBuilder);

  return true;
}

#ifdef MOZ_DUMP_PAINTING
void nsDisplayFilters::PrintEffects(nsACString& aTo) {
  nsIFrame* firstFrame =
      nsLayoutUtils::FirstContinuationOrIBSplitSibling(mFrame);
  bool first = true;
  aTo += " effects=(";
  if (mHandleOpacity) {
    first = false;
    aTo += nsPrintfCString("opacity(%f)", mFrame->StyleEffects()->mOpacity);
  }
  // We may exist for a mix of CSS filter functions and/or references to SVG
  // filters.  If we have invalid references to SVG filters then we paint
  // nothing, but otherwise we will apply one or more filters.
  if (SVGObserverUtils::GetAndObserveFilters(firstFrame, nullptr) !=
      SVGObserverUtils::eHasRefsSomeInvalid) {
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
    : nsDisplayWrapList(aBuilder, aFrame, aList) {
  MOZ_COUNT_CTOR(nsDisplaySVGWrapper);
}

LayerState nsDisplaySVGWrapper::GetLayerState(
    nsDisplayListBuilder* aBuilder, LayerManager* aManager,
    const ContainerLayerParameters& aParameters) {
  RefPtr<LayerManager> layerManager = aBuilder->GetWidgetLayerManager();
  if (layerManager &&
      layerManager->GetBackendType() == layers::LayersBackend::LAYERS_WR) {
    return LayerState::LAYER_ACTIVE_FORCE;
  }
  return LayerState::LAYER_NONE;
}

bool nsDisplaySVGWrapper::ShouldFlattenAway(nsDisplayListBuilder* aBuilder) {
  RefPtr<LayerManager> layerManager = aBuilder->GetWidgetLayerManager();
  if (layerManager &&
      layerManager->GetBackendType() == layers::LayersBackend::LAYERS_WR) {
    return false;
  }
  return true;
}

already_AddRefed<Layer> nsDisplaySVGWrapper::BuildLayer(
    nsDisplayListBuilder* aBuilder, LayerManager* aManager,
    const ContainerLayerParameters& aContainerParameters) {
  ContainerLayerParameters newContainerParameters = aContainerParameters;
  newContainerParameters.mDisableSubpixelAntialiasingInDescendants = true;

  RefPtr<ContainerLayer> container =
      aManager->GetLayerBuilder()->BuildContainerLayerFor(
          aBuilder, aManager, mFrame, this, &mList, newContainerParameters,
          nullptr);

  return container.forget();
}

bool nsDisplaySVGWrapper::CreateWebRenderCommands(
    mozilla::wr::DisplayListBuilder& aBuilder,
    mozilla::wr::IpcResourceUpdateQueue& aResources,
    const StackingContextHelper& aSc,
    mozilla::layers::RenderRootStateManager* aManager,
    nsDisplayListBuilder* aDisplayListBuilder) {
  return nsDisplayWrapList::CreateWebRenderCommands(
      aBuilder, aResources, aSc, aManager, aDisplayListBuilder);
}

nsDisplayForeignObject::nsDisplayForeignObject(nsDisplayListBuilder* aBuilder,
                                               nsIFrame* aFrame,
                                               nsDisplayList* aList)
    : nsDisplayWrapList(aBuilder, aFrame, aList) {
  MOZ_COUNT_CTOR(nsDisplayForeignObject);
}

#ifdef NS_BUILD_REFCNT_LOGGING
nsDisplayForeignObject::~nsDisplayForeignObject() {
  MOZ_COUNT_DTOR(nsDisplayForeignObject);
}
#endif

LayerState nsDisplayForeignObject::GetLayerState(
    nsDisplayListBuilder* aBuilder, LayerManager* aManager,
    const ContainerLayerParameters& aParameters) {
  RefPtr<LayerManager> layerManager = aBuilder->GetWidgetLayerManager();
  if (layerManager &&
      layerManager->GetBackendType() == layers::LayersBackend::LAYERS_WR) {
    return LayerState::LAYER_ACTIVE_FORCE;
  }
  return LayerState::LAYER_NONE;
}

bool nsDisplayForeignObject::ShouldFlattenAway(nsDisplayListBuilder* aBuilder) {
  RefPtr<LayerManager> layerManager = aBuilder->GetWidgetLayerManager();
  if (layerManager &&
      layerManager->GetBackendType() == layers::LayersBackend::LAYERS_WR) {
    return false;
  }
  return true;
}

already_AddRefed<Layer> nsDisplayForeignObject::BuildLayer(
    nsDisplayListBuilder* aBuilder, LayerManager* aManager,
    const ContainerLayerParameters& aContainerParameters) {
  ContainerLayerParameters newContainerParameters = aContainerParameters;
  newContainerParameters.mDisableSubpixelAntialiasingInDescendants = true;

  RefPtr<ContainerLayer> container =
      aManager->GetLayerBuilder()->BuildContainerLayerFor(
          aBuilder, aManager, mFrame, this, &mList, newContainerParameters,
          nullptr);

  return container.forget();
}

bool nsDisplayForeignObject::CreateWebRenderCommands(
    mozilla::wr::DisplayListBuilder& aBuilder,
    mozilla::wr::IpcResourceUpdateQueue& aResources,
    const StackingContextHelper& aSc,
    mozilla::layers::RenderRootStateManager* aManager,
    nsDisplayListBuilder* aDisplayListBuilder) {
  AutoRestore<bool> restoreDoGrouping(aManager->CommandBuilder().mDoGrouping);
  aManager->CommandBuilder().mDoGrouping = false;
  return nsDisplayWrapList::CreateWebRenderCommands(
      aBuilder, aResources, aSc, aManager, aDisplayListBuilder);
}

void nsDisplayListCollection::SerializeWithCorrectZOrder(
    nsDisplayList* aOutResultList, nsIContent* aContent) {
  // Sort PositionedDescendants() in CSS 'z-order' order.  The list is already
  // in content document order and SortByZOrder is a stable sort which
  // guarantees that boxes produced by the same element are placed together
  // in the sort. Consider a position:relative inline element that breaks
  // across lines and has absolutely positioned children; all the abs-pos
  // children should be z-ordered after all the boxes for the position:relative
  // element itself.
  PositionedDescendants()->SortByZOrder();

  // Now follow the rules of http://www.w3.org/TR/CSS21/zindex.html
  // 1,2: backgrounds and borders
  aOutResultList->AppendToTop(BorderBackground());
  // 3: negative z-index children.
  for (;;) {
    nsDisplayItem* item = PositionedDescendants()->GetBottom();
    if (item && item->ZIndex() < 0) {
      PositionedDescendants()->RemoveBottom();
      aOutResultList->AppendToTop(item);
      continue;
    }
    break;
  }
  // 4: block backgrounds
  aOutResultList->AppendToTop(BlockBorderBackgrounds());
  // 5: floats
  aOutResultList->AppendToTop(Floats());
  // 7: general content
  aOutResultList->AppendToTop(Content());
  // 7.5: outlines, in content tree order. We need to sort by content order
  // because an element with outline that breaks and has children with outline
  // might have placed child outline items between its own outline items.
  // The element's outline items need to all come before any child outline
  // items.
  if (aContent) {
    Outlines()->SortByContentOrder(aContent);
  }
  aOutResultList->AppendToTop(Outlines());
  // 8, 9: non-negative z-index children
  aOutResultList->AppendToTop(PositionedDescendants());
}

namespace mozilla {

uint32_t PaintTelemetry::sPaintLevel = 0;
uint32_t PaintTelemetry::sMetricLevel = 0;
EnumeratedArray<PaintTelemetry::Metric, PaintTelemetry::Metric::COUNT, double>
    PaintTelemetry::sMetrics;

PaintTelemetry::AutoRecordPaint::AutoRecordPaint() {
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

PaintTelemetry::AutoRecordPaint::~AutoRecordPaint() {
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
  Telemetry::Accumulate(Telemetry::CONTENT_PAINT_TIME,
                        static_cast<uint32_t>(totalMs));

  // Helpers for recording large/small paints.
  auto recordLarge = [=](const nsCString& aKey, double aDurationMs) -> void {
    MOZ_ASSERT(aDurationMs <= totalMs);
    uint32_t amount = static_cast<int32_t>((aDurationMs / totalMs) * 100.0);
    Telemetry::Accumulate(Telemetry::CONTENT_LARGE_PAINT_PHASE_WEIGHT, aKey,
                          amount);
  };
  auto recordSmall = [=](const nsCString& aKey, double aDurationMs) -> void {
    MOZ_ASSERT(aDurationMs <= totalMs);
    uint32_t amount = static_cast<int32_t>((aDurationMs / totalMs) * 100.0);
    Telemetry::Accumulate(Telemetry::CONTENT_SMALL_PAINT_PHASE_WEIGHT, aKey,
                          amount);
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

  Telemetry::Accumulate(Telemetry::PAINT_BUILD_LAYERS_TIME, flbMs);
}

PaintTelemetry::AutoRecord::AutoRecord(Metric aMetric) : mMetric(aMetric) {
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

PaintTelemetry::AutoRecord::~AutoRecord() {
  MOZ_ASSERT(sMetricLevel != 0);

  sMetricLevel--;
  if (mStart.IsNull()) {
    return;
  }

  sMetrics[mMetric] += (TimeStamp::Now() - mStart).ToMilliseconds();
}

}  // namespace mozilla

static nsIFrame* GetSelfOrPlaceholderFor(nsIFrame* aFrame) {
  if (aFrame->GetStateBits() & NS_FRAME_IS_PUSHED_FLOAT) {
    return aFrame;
  }

  if ((aFrame->GetStateBits() & NS_FRAME_OUT_OF_FLOW) &&
      !aFrame->GetPrevInFlow()) {
    return aFrame->GetPlaceholderFrame();
  }

  return aFrame;
}

static nsIFrame* GetAncestorFor(nsIFrame* aFrame) {
  nsIFrame* f = GetSelfOrPlaceholderFor(aFrame);
  MOZ_ASSERT(f);
  return nsLayoutUtils::GetCrossDocParentFrame(f);
}

nsDisplayListBuilder::AutoBuildingDisplayList::AutoBuildingDisplayList(
    nsDisplayListBuilder* aBuilder, nsIFrame* aForChild,
    const nsRect& aVisibleRect, const nsRect& aDirtyRect,
    const bool aIsTransformed)
    : mBuilder(aBuilder),
      mPrevFrame(aBuilder->mCurrentFrame),
      mPrevReferenceFrame(aBuilder->mCurrentReferenceFrame),
      mPrevHitTestArea(aBuilder->mHitTestArea),
      mPrevHitTestInfo(aBuilder->mHitTestInfo),
      mPrevOffset(aBuilder->mCurrentOffsetToReferenceFrame),
      mPrevVisibleRect(aBuilder->mVisibleRect),
      mPrevDirtyRect(aBuilder->mDirtyRect),
      mPrevAGR(aBuilder->mCurrentAGR),
      mPrevAncestorHasApzAwareEventHandler(
          aBuilder->mAncestorHasApzAwareEventHandler),
      mPrevBuildingInvisibleItems(aBuilder->mBuildingInvisibleItems),
      mPrevInInvalidSubtree(aBuilder->mInInvalidSubtree) {
  if (aIsTransformed) {
    aBuilder->mCurrentOffsetToReferenceFrame = nsPoint();
    aBuilder->mCurrentReferenceFrame = aForChild;
  } else if (aBuilder->mCurrentFrame == aForChild->GetParent()) {
    aBuilder->mCurrentOffsetToReferenceFrame += aForChild->GetPosition();
  } else {
    aBuilder->mCurrentReferenceFrame = aBuilder->FindReferenceFrameFor(
        aForChild, &aBuilder->mCurrentOffsetToReferenceFrame);
  }

  bool isAsync;
  mCurrentAGRState = aBuilder->IsAnimatedGeometryRoot(aForChild, isAsync);

  if (aBuilder->mCurrentFrame == aForChild->GetParent()) {
    if (mCurrentAGRState == AGR_YES) {
      aBuilder->mCurrentAGR =
          aBuilder->WrapAGRForFrame(aForChild, isAsync, aBuilder->mCurrentAGR);
    }
  } else if (aBuilder->mCurrentFrame != aForChild) {
    aBuilder->mCurrentAGR = aBuilder->FindAnimatedGeometryRootFor(aForChild);
  }

  MOZ_ASSERT(nsLayoutUtils::IsAncestorFrameCrossDoc(
      aBuilder->RootReferenceFrame(), *aBuilder->mCurrentAGR));

  // If aForChild is being visited from a frame other than it's ancestor frame,
  // mInInvalidSubtree will need to be recalculated the slow way.
  if (aForChild == mPrevFrame || GetAncestorFor(aForChild) == mPrevFrame) {
    aBuilder->mInInvalidSubtree =
        aBuilder->mInInvalidSubtree || aForChild->IsFrameModified();
  } else {
    aBuilder->mInInvalidSubtree = AnyContentAncestorModified(aForChild);
  }

  aBuilder->mCurrentFrame = aForChild;
  aBuilder->mVisibleRect = aVisibleRect;
  aBuilder->mDirtyRect =
      aBuilder->mInInvalidSubtree ? aVisibleRect : aDirtyRect;
}
