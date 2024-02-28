/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ActiveLayerTracker.h"

#include "mozilla/AnimationUtils.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/gfx/Matrix.h"
#include "mozilla/EffectSet.h"
#include "mozilla/MotionPathUtils.h"
#include "mozilla/PodOperations.h"
#include "mozilla/StaticPtr.h"
#include "gfx2DGlue.h"
#include "nsExpirationTracker.h"
#include "nsContainerFrame.h"
#include "nsIContent.h"
#include "nsRefreshDriver.h"
#include "nsPIDOMWindow.h"
#include "mozilla/dom/Document.h"
#include "nsAnimationManager.h"
#include "nsStyleTransformMatrix.h"
#include "nsTransitionManager.h"
#include "nsDisplayList.h"
#include "nsDOMCSSDeclaration.h"
#include "nsLayoutUtils.h"

namespace mozilla {

using namespace gfx;

/**
 * This tracks the state of a frame that may need active layers due to
 * ongoing content changes or style changes that indicate animation.
 *
 * When no changes of *any* kind are detected after 75-100ms we remove this
 * object. Because we only track all kinds of activity with a single
 * nsExpirationTracker, it's possible a frame might remain active somewhat
 * spuriously if different kinds of changes kept happening, but that almost
 * certainly doesn't matter.
 */
class LayerActivity {
 public:
  enum ActivityIndex {
    ACTIVITY_OPACITY,
    ACTIVITY_TRANSFORM,

    ACTIVITY_SCALE,
    ACTIVITY_TRIGGERED_REPAINT,

    // keep as last item
    ACTIVITY_COUNT
  };

  explicit LayerActivity(nsIFrame* aFrame) : mFrame(aFrame), mContent(nullptr) {
    PodArrayZero(mRestyleCounts);
  }
  ~LayerActivity();
  nsExpirationState* GetExpirationState() { return &mState; }
  uint8_t& RestyleCountForProperty(nsCSSPropertyID aProperty) {
    return mRestyleCounts[GetActivityIndexForProperty(aProperty)];
  }

  static ActivityIndex GetActivityIndexForProperty(nsCSSPropertyID aProperty) {
    switch (aProperty) {
      case eCSSProperty_opacity:
        return ACTIVITY_OPACITY;
      case eCSSProperty_transform:
      case eCSSProperty_translate:
      case eCSSProperty_rotate:
      case eCSSProperty_scale:
      case eCSSProperty_offset_path:
      case eCSSProperty_offset_distance:
      case eCSSProperty_offset_rotate:
      case eCSSProperty_offset_anchor:
      case eCSSProperty_offset_position:
        return ACTIVITY_TRANSFORM;
      default:
        MOZ_ASSERT(false);
        return ACTIVITY_OPACITY;
    }
  }

  static ActivityIndex GetActivityIndexForPropertySet(
      const nsCSSPropertyIDSet& aPropertySet) {
    if (aPropertySet.IsSubsetOf(
            nsCSSPropertyIDSet::TransformLikeProperties())) {
      return ACTIVITY_TRANSFORM;
    }
    MOZ_ASSERT(
        aPropertySet.IsSubsetOf(nsCSSPropertyIDSet::OpacityProperties()));
    return ACTIVITY_OPACITY;
  }

  // While tracked, exactly one of mFrame or mContent is non-null, depending
  // on whether this property is stored on a frame or on a content node.
  // When this property is expired by the layer activity tracker, both mFrame
  // and mContent are nulled-out and the property is deleted.
  nsIFrame* mFrame;
  nsIContent* mContent;

  nsExpirationState mState;

  // Previous scale due to the CSS transform property.
  Maybe<MatrixScales> mPreviousTransformScale;

  // Number of restyle operations detected
  uint8_t mRestyleCounts[ACTIVITY_COUNT];
};

class LayerActivityTracker final
    : public nsExpirationTracker<LayerActivity, 4> {
 public:
  // 75-100ms is a good timeout period. We use 4 generations of 25ms each.
  enum { GENERATION_MS = 100 };

  explicit LayerActivityTracker(nsIEventTarget* aEventTarget)
      : nsExpirationTracker<LayerActivity, 4>(
            GENERATION_MS, "LayerActivityTracker", aEventTarget) {}
  ~LayerActivityTracker() override { AgeAllGenerations(); }

  void NotifyExpired(LayerActivity* aObject) override;
};

static StaticAutoPtr<LayerActivityTracker> gLayerActivityTracker;

LayerActivity::~LayerActivity() {
  if (mFrame || mContent) {
    NS_ASSERTION(gLayerActivityTracker, "Should still have a tracker");
    gLayerActivityTracker->RemoveObject(this);
  }
}

// Frames with this property have NS_FRAME_HAS_LAYER_ACTIVITY_PROPERTY set
NS_DECLARE_FRAME_PROPERTY_DELETABLE(LayerActivityProperty, LayerActivity)

void LayerActivityTracker::NotifyExpired(LayerActivity* aObject) {
  RemoveObject(aObject);

  nsIFrame* f = aObject->mFrame;
  nsIContent* c = aObject->mContent;
  aObject->mFrame = nullptr;
  aObject->mContent = nullptr;

  MOZ_ASSERT((f == nullptr) != (c == nullptr),
             "A LayerActivity object should always have a reference to either "
             "its frame or its content");

  if (f) {
    f->RemoveStateBits(NS_FRAME_HAS_LAYER_ACTIVITY_PROPERTY);
    f->RemoveProperty(LayerActivityProperty());
  } else {
    c->RemoveProperty(nsGkAtoms::LayerActivity);
  }
}

static LayerActivity* GetLayerActivity(nsIFrame* aFrame) {
  if (!aFrame->HasAnyStateBits(NS_FRAME_HAS_LAYER_ACTIVITY_PROPERTY)) {
    return nullptr;
  }
  return aFrame->GetProperty(LayerActivityProperty());
}

static LayerActivity* GetLayerActivityForUpdate(nsIFrame* aFrame) {
  LayerActivity* layerActivity = GetLayerActivity(aFrame);
  if (layerActivity) {
    gLayerActivityTracker->MarkUsed(layerActivity);
  } else {
    if (!gLayerActivityTracker) {
      gLayerActivityTracker =
          new LayerActivityTracker(GetMainThreadSerialEventTarget());
    }
    layerActivity = new LayerActivity(aFrame);
    gLayerActivityTracker->AddObject(layerActivity);
    aFrame->AddStateBits(NS_FRAME_HAS_LAYER_ACTIVITY_PROPERTY);
    aFrame->SetProperty(LayerActivityProperty(), layerActivity);
  }
  return layerActivity;
}

static void IncrementMutationCount(uint8_t* aCount) {
  *aCount = uint8_t(std::min(0xFF, *aCount + 1));
}

/* static */
void ActiveLayerTracker::TransferActivityToContent(nsIFrame* aFrame,
                                                   nsIContent* aContent) {
  if (!aFrame->HasAnyStateBits(NS_FRAME_HAS_LAYER_ACTIVITY_PROPERTY)) {
    return;
  }
  LayerActivity* layerActivity = aFrame->TakeProperty(LayerActivityProperty());
  aFrame->RemoveStateBits(NS_FRAME_HAS_LAYER_ACTIVITY_PROPERTY);
  if (!layerActivity) {
    return;
  }
  layerActivity->mFrame = nullptr;
  layerActivity->mContent = aContent;
  aContent->SetProperty(nsGkAtoms::LayerActivity, layerActivity,
                        nsINode::DeleteProperty<LayerActivity>, true);
}

/* static */
void ActiveLayerTracker::TransferActivityToFrame(nsIContent* aContent,
                                                 nsIFrame* aFrame) {
  auto* layerActivity = static_cast<LayerActivity*>(
      aContent->TakeProperty(nsGkAtoms::LayerActivity));
  if (!layerActivity) {
    return;
  }
  layerActivity->mContent = nullptr;
  layerActivity->mFrame = aFrame;
  aFrame->AddStateBits(NS_FRAME_HAS_LAYER_ACTIVITY_PROPERTY);
  aFrame->SetProperty(LayerActivityProperty(), layerActivity);
}

static void IncrementScaleRestyleCountIfNeeded(nsIFrame* aFrame,
                                               LayerActivity* aActivity) {
  // This function is basically a simplified copy of
  // nsDisplayTransform::GetResultingTransformMatrixInternal.

  Matrix svgTransform, parentsChildrenOnlyTransform;
  const bool hasSVGTransforms =
      aFrame->HasAnyStateBits(NS_FRAME_MAY_BE_TRANSFORMED) &&
      aFrame->IsSVGTransformed(&svgTransform, &parentsChildrenOnlyTransform);

  const nsStyleDisplay* display = aFrame->StyleDisplay();
  if (!aFrame->HasAnyStateBits(NS_FRAME_MAY_BE_TRANSFORMED) ||
      (!display->HasTransformProperty() && !display->HasIndividualTransform() &&
       display->mOffsetPath.IsNone() && !hasSVGTransforms)) {
    if (aActivity->mPreviousTransformScale.isSome()) {
      // The transform was removed.
      aActivity->mPreviousTransformScale = Nothing();
      IncrementMutationCount(
          &aActivity->mRestyleCounts[LayerActivity::ACTIVITY_SCALE]);
    }

    return;
  }

  Matrix4x4 transform;
  if (aFrame->IsCSSTransformed()) {
    // Compute the new scale due to the CSS transform property.
    // Note: Motion path doesn't contribute to scale factor. (It only has 2d
    // translate and 2d rotate, so we use Nothing() for it.)
    nsStyleTransformMatrix::TransformReferenceBox refBox(aFrame);
    transform = nsStyleTransformMatrix::ReadTransforms(
        display->mTranslate, display->mRotate, display->mScale, nullptr,
        display->mTransform, refBox, AppUnitsPerCSSPixel());
  } else if (hasSVGTransforms) {
    transform = Matrix4x4::From2D(svgTransform);
  }

  const bool parentHasChildrenOnlyTransform =
      hasSVGTransforms && !parentsChildrenOnlyTransform.IsIdentity();
  if (parentHasChildrenOnlyTransform) {
    transform *= Matrix4x4::From2D(parentsChildrenOnlyTransform);
  }

  Matrix transform2D;
  if (!transform.Is2D(&transform2D)) {
    // We don't attempt to handle 3D transforms; just assume the scale changed.
    aActivity->mPreviousTransformScale = Nothing();
    IncrementMutationCount(
        &aActivity->mRestyleCounts[LayerActivity::ACTIVITY_SCALE]);
    return;
  }

  MatrixScales scale = transform2D.ScaleFactors();
  if (aActivity->mPreviousTransformScale == Some(scale)) {
    return;  // Nothing changed.
  }

  aActivity->mPreviousTransformScale = Some(scale);
  IncrementMutationCount(
      &aActivity->mRestyleCounts[LayerActivity::ACTIVITY_SCALE]);
}

/* static */
void ActiveLayerTracker::NotifyRestyle(nsIFrame* aFrame,
                                       nsCSSPropertyID aProperty) {
  LayerActivity* layerActivity = GetLayerActivityForUpdate(aFrame);
  uint8_t& mutationCount = layerActivity->RestyleCountForProperty(aProperty);
  IncrementMutationCount(&mutationCount);

  if (nsCSSPropertyIDSet::TransformLikeProperties().HasProperty(aProperty)) {
    IncrementScaleRestyleCountIfNeeded(aFrame, layerActivity);
  }
}

static bool IsPresContextInScriptAnimationCallback(
    nsPresContext* aPresContext) {
  if (aPresContext->RefreshDriver()->IsInRefresh()) {
    return true;
  }
  // Treat timeouts/setintervals as scripted animation callbacks for our
  // purposes.
  nsPIDOMWindowInner* win = aPresContext->Document()->GetInnerWindow();
  return win && win->IsRunningTimeout();
}

/* static */
void ActiveLayerTracker::NotifyInlineStyleRuleModified(
    nsIFrame* aFrame, nsCSSPropertyID aProperty) {
  if (IsPresContextInScriptAnimationCallback(aFrame->PresContext())) {
    LayerActivity* layerActivity = GetLayerActivityForUpdate(aFrame);
    // We know this is animated, so just hack the mutation count.
    layerActivity->RestyleCountForProperty(aProperty) = 0xff;
  }
}

/* static */
void ActiveLayerTracker::NotifyNeedsRepaint(nsIFrame* aFrame) {
  LayerActivity* layerActivity = GetLayerActivityForUpdate(aFrame);
  if (IsPresContextInScriptAnimationCallback(aFrame->PresContext())) {
    // This is mirroring NotifyInlineStyleRuleModified's NotifyAnimated logic.
    // Just max out the restyle count if we're in an animation callback.
    layerActivity->mRestyleCounts[LayerActivity::ACTIVITY_TRIGGERED_REPAINT] =
        0xFF;
  } else {
    IncrementMutationCount(
        &layerActivity
             ->mRestyleCounts[LayerActivity::ACTIVITY_TRIGGERED_REPAINT]);
  }
}

static bool IsMotionPathAnimated(nsDisplayListBuilder* aBuilder,
                                 nsIFrame* aFrame) {
  return ActiveLayerTracker::IsStyleAnimated(
             aBuilder, aFrame, nsCSSPropertyIDSet{eCSSProperty_offset_path}) ||
         (!aFrame->StyleDisplay()->mOffsetPath.IsNone() &&
          ActiveLayerTracker::IsStyleAnimated(
              aBuilder, aFrame,
              nsCSSPropertyIDSet{
                  eCSSProperty_offset_distance, eCSSProperty_offset_rotate,
                  eCSSProperty_offset_anchor, eCSSProperty_offset_position}));
}

/* static */
bool ActiveLayerTracker::IsTransformAnimated(nsDisplayListBuilder* aBuilder,
                                             nsIFrame* aFrame) {
  return IsStyleAnimated(aBuilder, aFrame,
                         nsCSSPropertyIDSet::CSSTransformProperties()) ||
         IsMotionPathAnimated(aBuilder, aFrame);
}

/* static */
bool ActiveLayerTracker::IsTransformMaybeAnimated(nsIFrame* aFrame) {
  return IsStyleAnimated(nullptr, aFrame,
                         nsCSSPropertyIDSet::CSSTransformProperties()) ||
         IsMotionPathAnimated(nullptr, aFrame);
}

/* static */
bool ActiveLayerTracker::IsStyleAnimated(
    nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
    const nsCSSPropertyIDSet& aPropertySet) {
  MOZ_ASSERT(
      aPropertySet.IsSubsetOf(nsCSSPropertyIDSet::TransformLikeProperties()) ||
          aPropertySet.IsSubsetOf(nsCSSPropertyIDSet::OpacityProperties()),
      "Only subset of opacity or transform-like properties set calls this");

  // For display:table content, transforms are applied to the table wrapper
  // (primary frame) but their will-change style will be specified on the style
  // frame and, unlike other transform properties, not inherited.
  // As a result, for transform properties only we need to be careful to look up
  // the will-change style on the _style_ frame.
  const nsIFrame* styleFrame = nsLayoutUtils::GetStyleFrame(aFrame);
  const nsCSSPropertyIDSet transformSet =
      nsCSSPropertyIDSet::TransformLikeProperties();
  if ((styleFrame && (styleFrame->StyleDisplay()->mWillChange.bits &
                      StyleWillChangeBits::TRANSFORM)) &&
      aPropertySet.Intersects(transformSet) &&
      (!aBuilder ||
       aBuilder->IsInWillChangeBudget(aFrame, aFrame->GetSize()))) {
    return true;
  }
  if ((aFrame->StyleDisplay()->mWillChange.bits &
       StyleWillChangeBits::OPACITY) &&
      aPropertySet.Intersects(nsCSSPropertyIDSet::OpacityProperties()) &&
      (!aBuilder ||
       aBuilder->IsInWillChangeBudget(aFrame, aFrame->GetSize()))) {
    return !StaticPrefs::gfx_will_change_ignore_opacity();
  }

  LayerActivity* layerActivity = GetLayerActivity(aFrame);
  if (layerActivity) {
    LayerActivity::ActivityIndex activityIndex =
        LayerActivity::GetActivityIndexForPropertySet(aPropertySet);
    if (layerActivity->mRestyleCounts[activityIndex] >= 2) {
      // If the frame needs to be repainted frequently, we probably don't get
      // much from treating the property as animated, *unless* this frame's
      // 'scale' (which includes the bounds changes of a rotation) is changing.
      // Marking a scaling transform as animating allows us to avoid resizing
      // the texture, even if we have to repaint the contents of that texture.
      if (layerActivity
                  ->mRestyleCounts[LayerActivity::ACTIVITY_TRIGGERED_REPAINT] <
              2 ||
          (aPropertySet.Intersects(transformSet) &&
           IsScaleSubjectToAnimation(aFrame))) {
        return true;
      }
    }
  }

  if (nsLayoutUtils::HasEffectiveAnimation(aFrame, aPropertySet)) {
    return true;
  }

  if (!aPropertySet.Intersects(transformSet) ||
      !aFrame->Combines3DTransformWithAncestors()) {
    return false;
  }

  // For preserve-3d, we check if there is any transform animation on its parent
  // frames in the 3d rendering context. If there is one, this function will
  // return true.
  return IsStyleAnimated(aBuilder, aFrame->GetParent(), aPropertySet);
}

/* static */
bool ActiveLayerTracker::IsScaleSubjectToAnimation(nsIFrame* aFrame) {
  // Check whether JavaScript is animating this frame's scale.
  LayerActivity* layerActivity = GetLayerActivity(aFrame);
  if (layerActivity &&
      layerActivity->mRestyleCounts[LayerActivity::ACTIVITY_SCALE] >= 2) {
    return true;
  }

  return AnimationUtils::FrameHasAnimatedScale(aFrame);
}

/* static */
void ActiveLayerTracker::Shutdown() { gLayerActivityTracker = nullptr; }

}  // namespace mozilla
