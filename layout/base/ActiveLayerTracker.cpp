/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ActiveLayerTracker.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/dom/KeyframeEffectReadOnly.h"
#include "mozilla/gfx/Matrix.h"
#include "mozilla/EffectSet.h"
#include "mozilla/PodOperations.h"
#include "gfx2DGlue.h"
#include "nsExpirationTracker.h"
#include "nsContainerFrame.h"
#include "nsIContent.h"
#include "nsRefreshDriver.h"
#include "nsPIDOMWindow.h"
#include "nsIDocument.h"
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
    ACTIVITY_MARGIN_LEFT,
    ACTIVITY_MARGIN_TOP,
    ACTIVITY_MARGIN_RIGHT,
    ACTIVITY_MARGIN_BOTTOM,
    ACTIVITY_BACKGROUND_POSITION,

    ACTIVITY_SCALE,

    // keep as last item
    ACTIVITY_COUNT
  };

  explicit LayerActivity(nsIFrame* aFrame)
    : mFrame(aFrame)
    , mContent(nullptr)
    , mContentActive(false)
  {
    PodArrayZero(mRestyleCounts);
  }
  ~LayerActivity();
  nsExpirationState* GetExpirationState() { return &mState; }
  uint8_t& RestyleCountForProperty(nsCSSPropertyID aProperty)
  {
    return mRestyleCounts[GetActivityIndexForProperty(aProperty)];
  }

  static ActivityIndex GetActivityIndexForProperty(nsCSSPropertyID aProperty)
  {
    switch (aProperty) {
    case eCSSProperty_opacity: return ACTIVITY_OPACITY;
    case eCSSProperty_transform: return ACTIVITY_TRANSFORM;
    case eCSSProperty_left: return ACTIVITY_LEFT;
    case eCSSProperty_top: return ACTIVITY_TOP;
    case eCSSProperty_right: return ACTIVITY_RIGHT;
    case eCSSProperty_bottom: return ACTIVITY_BOTTOM;
    case eCSSProperty_margin_left: return ACTIVITY_MARGIN_LEFT;
    case eCSSProperty_margin_top: return ACTIVITY_MARGIN_TOP;
    case eCSSProperty_margin_right: return ACTIVITY_MARGIN_RIGHT;
    case eCSSProperty_margin_bottom: return ACTIVITY_MARGIN_BOTTOM;
    case eCSSProperty_background_position: return ACTIVITY_BACKGROUND_POSITION;
    case eCSSProperty_background_position_x: return ACTIVITY_BACKGROUND_POSITION;
    case eCSSProperty_background_position_y: return ACTIVITY_BACKGROUND_POSITION;
    default: MOZ_ASSERT(false); return ACTIVITY_OPACITY;
    }
  }

  // While tracked, exactly one of mFrame or mContent is non-null, depending
  // on whether this property is stored on a frame or on a content node.
  // When this property is expired by the layer activity tracker, both mFrame
  // and mContent are nulled-out and the property is deleted.
  nsIFrame* mFrame;
  nsIContent* mContent;

  nsExpirationState mState;

  // Previous scale due to the CSS transform property.
  Maybe<gfxSize> mPreviousTransformScale;

  // The scroll frame during for which we most recently received a call to
  // NotifyAnimatedFromScrollHandler.
  nsWeakFrame mAnimatingScrollHandlerFrame;
  // The set of activities that were triggered during
  // mAnimatingScrollHandlerFrame's scroll event handler.
  EnumSet<ActivityIndex> mScrollHandlerInducedActivity;

  // Number of restyle operations detected
  uint8_t mRestyleCounts[ACTIVITY_COUNT];
  bool mContentActive;
};

class LayerActivityTracker final : public nsExpirationTracker<LayerActivity,4> {
public:
  // 75-100ms is a good timeout period. We use 4 generations of 25ms each.
  enum { GENERATION_MS = 100 };
  LayerActivityTracker()
    : nsExpirationTracker<LayerActivity,4>(GENERATION_MS,
                                           "LayerActivityTracker")
    , mDestroying(false)
  {}
  ~LayerActivityTracker() {
    mDestroying = true;
    AgeAllGenerations();
  }

  virtual void NotifyExpired(LayerActivity* aObject);

public:
  nsWeakFrame mCurrentScrollHandlerFrame;

private:
  bool mDestroying;
};

static LayerActivityTracker* gLayerActivityTracker = nullptr;

LayerActivity::~LayerActivity()
{
  if (mFrame || mContent) {
    NS_ASSERTION(gLayerActivityTracker, "Should still have a tracker");
    gLayerActivityTracker->RemoveObject(this);
  }
}

// Frames with this property have NS_FRAME_HAS_LAYER_ACTIVITY_PROPERTY set
NS_DECLARE_FRAME_PROPERTY_DELETABLE(LayerActivityProperty, LayerActivity)

void
LayerActivityTracker::NotifyExpired(LayerActivity* aObject)
{
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
             "A LayerActivity object should always have a reference to either its frame or its content");

  if (f) {
    // The pres context might have been detached during the delay -
    // that's fine, just skip the paint.
    if (f->PresContext()->GetContainerWeak()) {
      f->SchedulePaint();
    }
    f->RemoveStateBits(NS_FRAME_HAS_LAYER_ACTIVITY_PROPERTY);
    f->Properties().Delete(LayerActivityProperty());
  } else {
    c->DeleteProperty(nsGkAtoms::LayerActivity);
  }
}

static LayerActivity*
GetLayerActivity(nsIFrame* aFrame)
{
  if (!aFrame->HasAnyStateBits(NS_FRAME_HAS_LAYER_ACTIVITY_PROPERTY)) {
    return nullptr;
  }
  FrameProperties properties = aFrame->Properties();
  return properties.Get(LayerActivityProperty());
}

static LayerActivity*
GetLayerActivityForUpdate(nsIFrame* aFrame)
{
  FrameProperties properties = aFrame->Properties();
  LayerActivity* layerActivity = properties.Get(LayerActivityProperty());
  if (layerActivity) {
    gLayerActivityTracker->MarkUsed(layerActivity);
  } else {
    if (!gLayerActivityTracker) {
      gLayerActivityTracker = new LayerActivityTracker();
    }
    layerActivity = new LayerActivity(aFrame);
    gLayerActivityTracker->AddObject(layerActivity);
    aFrame->AddStateBits(NS_FRAME_HAS_LAYER_ACTIVITY_PROPERTY);
    properties.Set(LayerActivityProperty(), layerActivity);
  }
  return layerActivity;
}

static void
IncrementMutationCount(uint8_t* aCount)
{
  *aCount = uint8_t(std::min(0xFF, *aCount + 1));
}

/* static */ void
ActiveLayerTracker::TransferActivityToContent(nsIFrame* aFrame, nsIContent* aContent)
{
  if (!aFrame->HasAnyStateBits(NS_FRAME_HAS_LAYER_ACTIVITY_PROPERTY)) {
    return;
  }
  FrameProperties properties = aFrame->Properties();
  LayerActivity* layerActivity = properties.Remove(LayerActivityProperty());
  aFrame->RemoveStateBits(NS_FRAME_HAS_LAYER_ACTIVITY_PROPERTY);
  if (!layerActivity) {
    return;
  }
  layerActivity->mFrame = nullptr;
  layerActivity->mContent = aContent;
  aContent->SetProperty(nsGkAtoms::LayerActivity, layerActivity,
                        nsINode::DeleteProperty<LayerActivity>, true);
}

/* static */ void
ActiveLayerTracker::TransferActivityToFrame(nsIContent* aContent, nsIFrame* aFrame)
{
  LayerActivity* layerActivity = static_cast<LayerActivity*>(
    aContent->UnsetProperty(nsGkAtoms::LayerActivity));
  if (!layerActivity) {
    return;
  }
  layerActivity->mContent = nullptr;
  layerActivity->mFrame = aFrame;
  aFrame->AddStateBits(NS_FRAME_HAS_LAYER_ACTIVITY_PROPERTY);
  aFrame->Properties().Set(LayerActivityProperty(), layerActivity);
}

static void
IncrementScaleRestyleCountIfNeeded(nsIFrame* aFrame, LayerActivity* aActivity)
{
  const nsStyleDisplay* display = aFrame->StyleDisplay();
  if (!display->mSpecifiedTransform) {
    // The transform was removed.
    aActivity->mPreviousTransformScale = Nothing();
    IncrementMutationCount(&aActivity->mRestyleCounts[LayerActivity::ACTIVITY_SCALE]);
    return;
  }

  // Compute the new scale due to the CSS transform property.
  nsPresContext* presContext = aFrame->PresContext();
  RuleNodeCacheConditions dummy;
  bool dummyBool;
  nsStyleTransformMatrix::TransformReferenceBox refBox(aFrame);
  Matrix4x4 transform =
    nsStyleTransformMatrix::ReadTransforms(display->mSpecifiedTransform->mHead,
                                           aFrame->StyleContext(),
                                           presContext,
                                           dummy, refBox,
                                           presContext->AppUnitsPerCSSPixel(),
                                           &dummyBool);
  Matrix transform2D;
  if (!transform.Is2D(&transform2D)) {
    // We don't attempt to handle 3D transforms; just assume the scale changed.
    aActivity->mPreviousTransformScale = Nothing();
    IncrementMutationCount(&aActivity->mRestyleCounts[LayerActivity::ACTIVITY_SCALE]);
    return;
  }

  gfxSize scale = ThebesMatrix(transform2D).ScaleFactors(true);
  if (aActivity->mPreviousTransformScale == Some(scale)) {
    return;  // Nothing changed.
  }

  aActivity->mPreviousTransformScale = Some(scale);
  IncrementMutationCount(&aActivity->mRestyleCounts[LayerActivity::ACTIVITY_SCALE]);
}

/* static */ void
ActiveLayerTracker::NotifyRestyle(nsIFrame* aFrame, nsCSSPropertyID aProperty)
{
  LayerActivity* layerActivity = GetLayerActivityForUpdate(aFrame);
  uint8_t& mutationCount = layerActivity->RestyleCountForProperty(aProperty);
  IncrementMutationCount(&mutationCount);

  if (aProperty == eCSSProperty_transform) {
    IncrementScaleRestyleCountIfNeeded(aFrame, layerActivity);
  }
}

/* static */ void
ActiveLayerTracker::NotifyOffsetRestyle(nsIFrame* aFrame)
{
  LayerActivity* layerActivity = GetLayerActivityForUpdate(aFrame);
  IncrementMutationCount(&layerActivity->mRestyleCounts[LayerActivity::ACTIVITY_LEFT]);
  IncrementMutationCount(&layerActivity->mRestyleCounts[LayerActivity::ACTIVITY_TOP]);
  IncrementMutationCount(&layerActivity->mRestyleCounts[LayerActivity::ACTIVITY_RIGHT]);
  IncrementMutationCount(&layerActivity->mRestyleCounts[LayerActivity::ACTIVITY_BOTTOM]);
}

/* static */ void
ActiveLayerTracker::NotifyAnimated(nsIFrame* aFrame,
                                   nsCSSPropertyID aProperty,
                                   const nsAString& aNewValue,
                                   nsDOMCSSDeclaration* aDOMCSSDecl)
{
  LayerActivity* layerActivity = GetLayerActivityForUpdate(aFrame);
  uint8_t& mutationCount = layerActivity->RestyleCountForProperty(aProperty);
  if (mutationCount != 0xFF) {
    nsAutoString oldValue;
    aDOMCSSDecl->GetPropertyValue(aProperty, oldValue);
    if (aNewValue != oldValue) {
      // We know this is animated, so just hack the mutation count.
      mutationCount = 0xFF;
    }
  }
}

/* static */ void
ActiveLayerTracker::NotifyAnimatedFromScrollHandler(nsIFrame* aFrame, nsCSSPropertyID aProperty,
                                                    nsIFrame* aScrollFrame)
{
  if (aFrame->PresContext() != aScrollFrame->PresContext()) {
    // Don't allow cross-document dependencies.
    return;
  }
  LayerActivity* layerActivity = GetLayerActivityForUpdate(aFrame);
  LayerActivity::ActivityIndex activityIndex = LayerActivity::GetActivityIndexForProperty(aProperty);

  if (layerActivity->mAnimatingScrollHandlerFrame.GetFrame() != aScrollFrame) {
    // Discard any activity of a different scroll frame. We only track the
    // most recent scroll handler induced activity.
    layerActivity->mScrollHandlerInducedActivity.clear();
    layerActivity->mAnimatingScrollHandlerFrame = aScrollFrame;
  }

  layerActivity->mScrollHandlerInducedActivity += activityIndex;
}

static bool
IsPresContextInScriptAnimationCallback(nsPresContext* aPresContext)
{
  if (aPresContext->RefreshDriver()->IsInRefresh()) {
    return true;
  }
  // Treat timeouts/setintervals as scripted animation callbacks for our
  // purposes.
  nsPIDOMWindowInner* win = aPresContext->Document()->GetInnerWindow();
  return win && win->IsRunningTimeout();
}

/* static */ void
ActiveLayerTracker::NotifyInlineStyleRuleModified(nsIFrame* aFrame,
                                                  nsCSSPropertyID aProperty,
                                                  const nsAString& aNewValue,
                                                  nsDOMCSSDeclaration* aDOMCSSDecl)
{
  if (IsPresContextInScriptAnimationCallback(aFrame->PresContext())) {
    NotifyAnimated(aFrame, aProperty, aNewValue, aDOMCSSDecl);
  }
  if (gLayerActivityTracker &&
      gLayerActivityTracker->mCurrentScrollHandlerFrame.IsAlive()) {
    NotifyAnimatedFromScrollHandler(aFrame, aProperty,
      gLayerActivityTracker->mCurrentScrollHandlerFrame.GetFrame());
  }
}

/* static */ bool
ActiveLayerTracker::IsStyleMaybeAnimated(nsIFrame* aFrame, nsCSSPropertyID aProperty)
{
  return IsStyleAnimated(nullptr, aFrame, aProperty);
}

/* static */ bool
ActiveLayerTracker::IsBackgroundPositionAnimated(nsDisplayListBuilder* aBuilder,
                                                 nsIFrame* aFrame)
{
  return IsStyleAnimated(aBuilder, aFrame, eCSSProperty_background_position_x) ||
         IsStyleAnimated(aBuilder, aFrame, eCSSProperty_background_position_y);
}

static bool
CheckScrollInducedActivity(LayerActivity* aLayerActivity,
                           LayerActivity::ActivityIndex aActivityIndex,
                           nsDisplayListBuilder* aBuilder)
{
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

/* static */ bool
ActiveLayerTracker::IsStyleAnimated(nsDisplayListBuilder* aBuilder,
                                    nsIFrame* aFrame, nsCSSPropertyID aProperty)
{
  // TODO: Add some abuse restrictions
  if ((aFrame->StyleDisplay()->mWillChangeBitField & NS_STYLE_WILL_CHANGE_TRANSFORM) &&
      aProperty == eCSSProperty_transform &&
      (!aBuilder || aBuilder->IsInWillChangeBudget(aFrame, aFrame->GetSize()))) {
    return true;
  }
  if ((aFrame->StyleDisplay()->mWillChangeBitField & NS_STYLE_WILL_CHANGE_OPACITY) &&
      aProperty == eCSSProperty_opacity &&
      (!aBuilder || aBuilder->IsInWillChangeBudget(aFrame, aFrame->GetSize()))) {
    return true;
  }

  LayerActivity* layerActivity = GetLayerActivity(aFrame);
  if (layerActivity) {
    LayerActivity::ActivityIndex activityIndex = LayerActivity::GetActivityIndexForProperty(aProperty);
    if (layerActivity->mRestyleCounts[activityIndex] >= 2) {
      return true;
    }
    if (CheckScrollInducedActivity(layerActivity, activityIndex, aBuilder)) {
      return true;
    }
  }
  if (aProperty == eCSSProperty_transform && aFrame->Combines3DTransformWithAncestors()) {
    return IsStyleAnimated(aBuilder, aFrame->GetParent(), aProperty);
  }
  return nsLayoutUtils::HasEffectiveAnimation(aFrame, aProperty);
}

/* static */ bool
ActiveLayerTracker::IsOffsetOrMarginStyleAnimated(nsIFrame* aFrame)
{
  LayerActivity* layerActivity = GetLayerActivity(aFrame);
  if (layerActivity) {
    if (layerActivity->mRestyleCounts[LayerActivity::ACTIVITY_LEFT] >= 2 ||
        layerActivity->mRestyleCounts[LayerActivity::ACTIVITY_TOP] >= 2 ||
        layerActivity->mRestyleCounts[LayerActivity::ACTIVITY_RIGHT] >= 2 ||
        layerActivity->mRestyleCounts[LayerActivity::ACTIVITY_BOTTOM] >= 2 ||
        layerActivity->mRestyleCounts[LayerActivity::ACTIVITY_MARGIN_LEFT] >= 2 ||
        layerActivity->mRestyleCounts[LayerActivity::ACTIVITY_MARGIN_TOP] >= 2 ||
        layerActivity->mRestyleCounts[LayerActivity::ACTIVITY_MARGIN_RIGHT] >= 2 ||
        layerActivity->mRestyleCounts[LayerActivity::ACTIVITY_MARGIN_BOTTOM] >= 2) {
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

// A helper function for IsScaleSubjectToAnimation which returns true if the
// given EffectSet contains a current effect that animates scale.
static bool
ContainsAnimatedScale(EffectSet& aEffects, nsIFrame* aFrame)
{
  for (dom::KeyframeEffectReadOnly* effect : aEffects) {
    if (!effect->IsCurrent()) {
      continue;
    }

    for (const AnimationProperty& prop : effect->Properties()) {
      if (prop.mProperty != eCSSProperty_transform) {
        continue;
      }
      for (AnimationPropertySegment segment : prop.mSegments) {
        gfxSize from = segment.mFromValue.GetScaleValue(aFrame);
        if (from != gfxSize(1.0f, 1.0f)) {
          return true;
        }
        gfxSize to = segment.mToValue.GetScaleValue(aFrame);
        if (to != gfxSize(1.0f, 1.0f)) {
          return true;
        }
      }
    }
  }

  return false;
}

/* static */ bool
ActiveLayerTracker::IsScaleSubjectToAnimation(nsIFrame* aFrame)
{
  // Check whether JavaScript is animating this frame's scale.
  LayerActivity* layerActivity = GetLayerActivity(aFrame);
  if (layerActivity && layerActivity->mRestyleCounts[LayerActivity::ACTIVITY_SCALE] >= 2) {
    return true;
  }

  // Check if any animations, transitions, etc. associated with this frame may
  // animate its scale.
  EffectSet* effects = EffectSet::GetEffectSet(aFrame);
  if (effects && ContainsAnimatedScale(*effects, aFrame)) {
    return true;
  }

  return false;
}

/* static */ void
ActiveLayerTracker::NotifyContentChange(nsIFrame* aFrame)
{
  LayerActivity* layerActivity = GetLayerActivityForUpdate(aFrame);
  layerActivity->mContentActive = true;
}

/* static */ bool
ActiveLayerTracker::IsContentActive(nsIFrame* aFrame)
{
  LayerActivity* layerActivity = GetLayerActivity(aFrame);
  return layerActivity && layerActivity->mContentActive;
}

/* static */ void
ActiveLayerTracker::SetCurrentScrollHandlerFrame(nsIFrame* aFrame)
{
  if (!gLayerActivityTracker) {
    gLayerActivityTracker = new LayerActivityTracker();
  }
  gLayerActivityTracker->mCurrentScrollHandlerFrame = aFrame;
}

/* static */ void
ActiveLayerTracker::Shutdown()
{
  delete gLayerActivityTracker;
  gLayerActivityTracker = nullptr;
}

} // namespace mozilla
