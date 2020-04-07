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
    ACTIVITY_LEFT,
    ACTIVITY_TOP,
    ACTIVITY_RIGHT,
    ACTIVITY_BOTTOM,
    ACTIVITY_BACKGROUND_POSITION,

    ACTIVITY_SCALE,
    ACTIVITY_TRIGGERED_REPAINT,

    // keep as last item
    ACTIVITY_COUNT
  };

  explicit LayerActivity(nsIFrame* aFrame)
      : mFrame(aFrame), mContent(nullptr), mContentActive(false) {
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
        // TODO: Bug 1559232: Add offset-position.
        return ACTIVITY_TRANSFORM;
      case eCSSProperty_left:
        return ACTIVITY_LEFT;
      case eCSSProperty_top:
        return ACTIVITY_TOP;
      case eCSSProperty_right:
        return ACTIVITY_RIGHT;
      case eCSSProperty_bottom:
        return ACTIVITY_BOTTOM;
      case eCSSProperty_background_position:
        return ACTIVITY_BACKGROUND_POSITION;
      case eCSSProperty_background_position_x:
        return ACTIVITY_BACKGROUND_POSITION;
      case eCSSProperty_background_position_y:
        return ACTIVITY_BACKGROUND_POSITION;
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
  Maybe<Size> mPreviousTransformScale;

  // The scroll frame during for which we most recently received a call to
  // NotifyAnimatedFromScrollHandler.
  WeakFrame mAnimatingScrollHandlerFrame;
  // The set of activities that were triggered during
  // mAnimatingScrollHandlerFrame's scroll event handler.
  EnumSet<ActivityIndex> mScrollHandlerInducedActivity;

  // Number of restyle operations detected
  uint8_t mRestyleCounts[ACTIVITY_COUNT];
  bool mContentActive;
};

class LayerActivityTracker final
    : public nsExpirationTracker<LayerActivity, 4> {
 public:
  // 75-100ms is a good timeout period. We use 4 generations of 25ms each.
  enum { GENERATION_MS = 100 };

  explicit LayerActivityTracker(nsIEventTarget* aEventTarget)
      : nsExpirationTracker<LayerActivity, 4>(
            GENERATION_MS, "LayerActivityTracker", aEventTarget),
        mDestroying(false) {}
  ~LayerActivityTracker() override {
    mDestroying = true;
    AgeAllGenerations();
  }

  void NotifyExpired(LayerActivity* aObject) override;

 public:
  WeakFrame mCurrentScrollHandlerFrame;

 private:
  bool mDestroying;
};

static LayerActivityTracker* gLayerActivityTracker = nullptr;

LayerActivity::~LayerActivity() {
  if (mFrame || mContent) {
    NS_ASSERTION(gLayerActivityTracker, "Should still have a tracker");
    gLayerActivityTracker->RemoveObject(this);
  }
}

// Frames with this property have NS_FRAME_HAS_LAYER_ACTIVITY_PROPERTY set
NS_DECLARE_FRAME_PROPERTY_DELETABLE(LayerActivityProperty, LayerActivity)

void LayerActivityTracker::NotifyExpired(LayerActivity* aObject) {
  if (!mDestroying && aObject->mAnimatingScrollHandlerFrame.IsAlive()) {
    // Reset the restyle counts, but let the layer activity survive.
    PodArrayZero(aObject->mRestyleCounts);
    MarkUsed(aObject);
    return;
  }

  RemoveObject(aObject);

  nsIFrame* f = aObject->mFrame;
  nsIContent* c = aObject->mContent;
  aObject->mFrame = nullptr;
  aObject->mContent = nullptr;

  MOZ_ASSERT((f == nullptr) != (c == nullptr),
             "A LayerActivity object should always have a reference to either "
             "its frame or its content");

  if (f) {
    // The pres context might have been detached during the delay -
    // that's fine, just skip the paint.
    if (f->PresContext()->GetContainerWeak() && !gfxVars::UseWebRender()) {
      f->SchedulePaint(nsIFrame::PAINT_DEFAULT, false);
    }
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
  const nsStyleDisplay* display = aFrame->StyleDisplay();
  if (!display->HasTransformProperty() && !display->HasIndividualTransform() &&
      display->mOffsetPath.IsNone()) {
    // The transform was removed.
    aActivity->mPreviousTransformScale = Nothing();
    IncrementMutationCount(
        &aActivity->mRestyleCounts[LayerActivity::ACTIVITY_SCALE]);
    return;
  }

  // Compute the new scale due to the CSS transform property.
  // Note: Motion path doesn't contribute to scale factor. (It only has 2d
  // translate and 2d rotate, so we use Nothing() for it.)
  nsStyleTransformMatrix::TransformReferenceBox refBox(aFrame);
  Matrix4x4 transform = nsStyleTransformMatrix::ReadTransforms(
      display->mTranslate, display->mRotate, display->mScale, Nothing(),
      display->mTransform, refBox, AppUnitsPerCSSPixel());
  Matrix transform2D;
  if (!transform.Is2D(&transform2D)) {
    // We don't attempt to handle 3D transforms; just assume the scale changed.
    aActivity->mPreviousTransformScale = Nothing();
    IncrementMutationCount(
        &aActivity->mRestyleCounts[LayerActivity::ACTIVITY_SCALE]);
    return;
  }

  Size scale = transform2D.ScaleFactors(true);
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

/* static */
void ActiveLayerTracker::NotifyOffsetRestyle(nsIFrame* aFrame) {
  LayerActivity* layerActivity = GetLayerActivityForUpdate(aFrame);
  IncrementMutationCount(
      &layerActivity->mRestyleCounts[LayerActivity::ACTIVITY_LEFT]);
  IncrementMutationCount(
      &layerActivity->mRestyleCounts[LayerActivity::ACTIVITY_TOP]);
  IncrementMutationCount(
      &layerActivity->mRestyleCounts[LayerActivity::ACTIVITY_RIGHT]);
  IncrementMutationCount(
      &layerActivity->mRestyleCounts[LayerActivity::ACTIVITY_BOTTOM]);
}

/* static */
void ActiveLayerTracker::NotifyAnimated(nsIFrame* aFrame,
                                        nsCSSPropertyID aProperty,
                                        const nsACString& aNewValue,
                                        nsDOMCSSDeclaration* aDOMCSSDecl) {
  LayerActivity* layerActivity = GetLayerActivityForUpdate(aFrame);
  uint8_t& mutationCount = layerActivity->RestyleCountForProperty(aProperty);
  if (mutationCount != 0xFF) {
    nsAutoString oldValue;
    aDOMCSSDecl->GetPropertyValue(aProperty, oldValue);
    if (NS_ConvertUTF16toUTF8(oldValue) != aNewValue) {
      // We know this is animated, so just hack the mutation count.
      mutationCount = 0xFF;
    }
  }
}

/* static */
void ActiveLayerTracker::NotifyAnimatedFromScrollHandler(
    nsIFrame* aFrame, nsCSSPropertyID aProperty, nsIFrame* aScrollFrame) {
  if (aFrame->PresContext() != aScrollFrame->PresContext()) {
    // Don't allow cross-document dependencies.
    return;
  }
  LayerActivity* layerActivity = GetLayerActivityForUpdate(aFrame);
  LayerActivity::ActivityIndex activityIndex =
      LayerActivity::GetActivityIndexForProperty(aProperty);

  if (layerActivity->mAnimatingScrollHandlerFrame.GetFrame() != aScrollFrame) {
    // Discard any activity of a different scroll frame. We only track the
    // most recent scroll handler induced activity.
    layerActivity->mScrollHandlerInducedActivity.clear();
    layerActivity->mAnimatingScrollHandlerFrame = aScrollFrame;
  }

  layerActivity->mScrollHandlerInducedActivity += activityIndex;
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
    nsIFrame* aFrame, nsCSSPropertyID aProperty, const nsACString& aNewValue,
    nsDOMCSSDeclaration* aDOMCSSDecl) {
  if (IsPresContextInScriptAnimationCallback(aFrame->PresContext())) {
    NotifyAnimated(aFrame, aProperty, aNewValue, aDOMCSSDecl);
  }
  if (gLayerActivityTracker &&
      gLayerActivityTracker->mCurrentScrollHandlerFrame.IsAlive()) {
    NotifyAnimatedFromScrollHandler(
        aFrame, aProperty,
        gLayerActivityTracker->mCurrentScrollHandlerFrame.GetFrame());
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

static bool CheckScrollInducedActivity(
    LayerActivity* aLayerActivity, LayerActivity::ActivityIndex aActivityIndex,
    nsDisplayListBuilder* aBuilder) {
  if (!aLayerActivity->mScrollHandlerInducedActivity.contains(aActivityIndex) ||
      !aLayerActivity->mAnimatingScrollHandlerFrame.IsAlive()) {
    return false;
  }

  nsIScrollableFrame* scrollFrame =
      do_QueryFrame(aLayerActivity->mAnimatingScrollHandlerFrame.GetFrame());
  if (scrollFrame && (!aBuilder || scrollFrame->IsScrollingActive(aBuilder))) {
    return true;
  }

  // The scroll frame has been destroyed or has become inactive. Clear it from
  // the layer activity so that it can expire.
  aLayerActivity->mAnimatingScrollHandlerFrame = nullptr;
  aLayerActivity->mScrollHandlerInducedActivity.clear();
  return false;
}

/* static */
bool ActiveLayerTracker::IsBackgroundPositionAnimated(
    nsDisplayListBuilder* aBuilder, nsIFrame* aFrame) {
  LayerActivity* layerActivity = GetLayerActivity(aFrame);
  if (layerActivity) {
    LayerActivity::ActivityIndex activityIndex =
        LayerActivity::ActivityIndex::ACTIVITY_BACKGROUND_POSITION;
    if (layerActivity->mRestyleCounts[activityIndex] >= 2) {
      // If the frame needs to be repainted frequently, we probably don't get
      // much from treating the property as animated, *unless* this frame's
      // 'scale' (which includes the bounds changes of a rotation) is changing.
      // Marking a scaling transform as animating allows us to avoid resizing
      // the texture, even if we have to repaint the contents of that texture.
      if (layerActivity
              ->mRestyleCounts[LayerActivity::ACTIVITY_TRIGGERED_REPAINT] < 2) {
        return true;
      }
    }
    if (CheckScrollInducedActivity(layerActivity, activityIndex, aBuilder)) {
      return true;
    }
  }
  return nsLayoutUtils::HasEffectiveAnimation(
      aFrame, nsCSSPropertyIDSet({eCSSProperty_background_position_x,
                                  eCSSProperty_background_position_y}));
}

static bool IsMotionPathAnimated(nsDisplayListBuilder* aBuilder,
                                 nsIFrame* aFrame) {
  return ActiveLayerTracker::IsStyleAnimated(
             aBuilder, aFrame, nsCSSPropertyIDSet{eCSSProperty_offset_path}) ||
         (!aFrame->StyleDisplay()->mOffsetPath.IsNone() &&
          ActiveLayerTracker::IsStyleAnimated(
              aBuilder, aFrame,
              nsCSSPropertyIDSet{eCSSProperty_offset_distance,
                                 eCSSProperty_offset_rotate,
                                 eCSSProperty_offset_anchor}));
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
    return true;
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
    if (CheckScrollInducedActivity(layerActivity, activityIndex, aBuilder)) {
      return true;
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
bool ActiveLayerTracker::IsOffsetStyleAnimated(nsIFrame* aFrame) {
  LayerActivity* layerActivity = GetLayerActivity(aFrame);
  if (layerActivity) {
    if (layerActivity->mRestyleCounts[LayerActivity::ACTIVITY_LEFT] >= 2 ||
        layerActivity->mRestyleCounts[LayerActivity::ACTIVITY_TOP] >= 2 ||
        layerActivity->mRestyleCounts[LayerActivity::ACTIVITY_RIGHT] >= 2 ||
        layerActivity->mRestyleCounts[LayerActivity::ACTIVITY_BOTTOM] >= 2) {
      return true;
    }
  }
  // We should also check for running CSS animations of these properties once
  // bug 1009693 is fixed. Until that happens, layerization isn't useful for
  // animations of these properties because we'll invalidate the layer contents
  // on every change anyway.
  // See bug 1151346 for a patch that adds a check for CSS animations.
  return false;
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
void ActiveLayerTracker::NotifyContentChange(nsIFrame* aFrame) {
  LayerActivity* layerActivity = GetLayerActivityForUpdate(aFrame);
  layerActivity->mContentActive = true;
}

/* static */
bool ActiveLayerTracker::IsContentActive(nsIFrame* aFrame) {
  LayerActivity* layerActivity = GetLayerActivity(aFrame);
  return layerActivity && layerActivity->mContentActive;
}

/* static */
void ActiveLayerTracker::SetCurrentScrollHandlerFrame(nsIFrame* aFrame) {
  if (!gLayerActivityTracker) {
    gLayerActivityTracker =
        new LayerActivityTracker(GetMainThreadSerialEventTarget());
  }
  gLayerActivityTracker->mCurrentScrollHandlerFrame = aFrame;
}

/* static */
void ActiveLayerTracker::Shutdown() {
  delete gLayerActivityTracker;
  gLayerActivityTracker = nullptr;
}

}  // namespace mozilla
