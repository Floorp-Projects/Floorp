/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ActiveLayerTracker.h"

#include "nsExpirationTracker.h"
#include "nsContainerFrame.h"
#include "nsIContent.h"
#include "nsRefreshDriver.h"
#include "nsPIDOMWindow.h"
#include "nsIDocument.h"
#include "nsAnimationManager.h"
#include "nsTransitionManager.h"
#include "nsDisplayList.h"

namespace mozilla {

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
  explicit LayerActivity(nsIFrame* aFrame)
    : mFrame(aFrame)
    , mContent(nullptr)
    , mOpacityRestyleCount(0)
    , mTransformRestyleCount(0)
    , mLeftRestyleCount(0)
    , mTopRestyleCount(0)
    , mRightRestyleCount(0)
    , mBottomRestyleCount(0)
    , mMarginLeftRestyleCount(0)
    , mMarginTopRestyleCount(0)
    , mMarginRightRestyleCount(0)
    , mMarginBottomRestyleCount(0)
    , mContentActive(false)
  {}
  ~LayerActivity();
  nsExpirationState* GetExpirationState() { return &mState; }
  uint8_t& RestyleCountForProperty(nsCSSProperty aProperty)
  {
    switch (aProperty) {
    case eCSSProperty_opacity: return mOpacityRestyleCount;
    case eCSSProperty_transform: return mTransformRestyleCount;
    case eCSSProperty_left: return mLeftRestyleCount;
    case eCSSProperty_top: return mTopRestyleCount;
    case eCSSProperty_right: return mRightRestyleCount;
    case eCSSProperty_bottom: return mBottomRestyleCount;
    case eCSSProperty_margin_left: return mMarginLeftRestyleCount;
    case eCSSProperty_margin_top: return mMarginTopRestyleCount;
    case eCSSProperty_margin_right: return mMarginRightRestyleCount;
    case eCSSProperty_margin_bottom: return mMarginBottomRestyleCount;
    default: MOZ_ASSERT(false); return mOpacityRestyleCount;
    }
  }

  // While tracked, exactly one of mFrame or mContent is non-null, depending
  // on whether this property is stored on a frame or on a content node.
  // When this property is expired by the layer activity tracker, both mFrame
  // and mContent are nulled-out and the property is deleted.
  nsIFrame* mFrame;
  nsIContent* mContent;

  nsExpirationState mState;
  // Number of restyle operations detected
  uint8_t mOpacityRestyleCount;
  uint8_t mTransformRestyleCount;
  uint8_t mLeftRestyleCount;
  uint8_t mTopRestyleCount;
  uint8_t mRightRestyleCount;
  uint8_t mBottomRestyleCount;
  uint8_t mMarginLeftRestyleCount;
  uint8_t mMarginTopRestyleCount;
  uint8_t mMarginRightRestyleCount;
  uint8_t mMarginBottomRestyleCount;
  bool mContentActive;
};

class LayerActivityTracker MOZ_FINAL : public nsExpirationTracker<LayerActivity,4> {
public:
  // 75-100ms is a good timeout period. We use 4 generations of 25ms each.
  enum { GENERATION_MS = 100 };
  LayerActivityTracker()
    : nsExpirationTracker<LayerActivity,4>(GENERATION_MS) {}
  ~LayerActivityTracker() {
    AgeAllGenerations();
  }

  virtual void NotifyExpired(LayerActivity* aObject);
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
NS_DECLARE_FRAME_PROPERTY(LayerActivityProperty, DeleteValue<LayerActivity>)

void
LayerActivityTracker::NotifyExpired(LayerActivity* aObject)
{
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
  return static_cast<LayerActivity*>(properties.Get(LayerActivityProperty()));
}

static LayerActivity*
GetLayerActivityForUpdate(nsIFrame* aFrame)
{
  FrameProperties properties = aFrame->Properties();
  LayerActivity* layerActivity =
    static_cast<LayerActivity*>(properties.Get(LayerActivityProperty()));
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
  LayerActivity* layerActivity =
    static_cast<LayerActivity*>(properties.Remove(LayerActivityProperty()));
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

/* static */ void
ActiveLayerTracker::NotifyRestyle(nsIFrame* aFrame, nsCSSProperty aProperty)
{
  LayerActivity* layerActivity = GetLayerActivityForUpdate(aFrame);
  uint8_t& mutationCount = layerActivity->RestyleCountForProperty(aProperty);
  IncrementMutationCount(&mutationCount);
}

/* static */ void
ActiveLayerTracker::NotifyOffsetRestyle(nsIFrame* aFrame)
{
  LayerActivity* layerActivity = GetLayerActivityForUpdate(aFrame);
  IncrementMutationCount(&layerActivity->mLeftRestyleCount);
  IncrementMutationCount(&layerActivity->mTopRestyleCount);
  IncrementMutationCount(&layerActivity->mRightRestyleCount);
  IncrementMutationCount(&layerActivity->mBottomRestyleCount);
}

/* static */ void
ActiveLayerTracker::NotifyAnimated(nsIFrame* aFrame, nsCSSProperty aProperty)
{
  LayerActivity* layerActivity = GetLayerActivityForUpdate(aFrame);
  uint8_t& mutationCount = layerActivity->RestyleCountForProperty(aProperty);
  // We know this is animated, so just hack the mutation count.
  mutationCount = 0xFF;
}

static bool
IsPresContextInScriptAnimationCallback(nsPresContext* aPresContext)
{
  if (aPresContext->RefreshDriver()->IsInRefresh()) {
    return true;
  }
  // Treat timeouts/setintervals as scripted animation callbacks for our
  // purposes.
  nsPIDOMWindow* win = aPresContext->Document()->GetInnerWindow();
  return win && win->IsRunningTimeout();
}

/* static */ void
ActiveLayerTracker::NotifyInlineStyleRuleModified(nsIFrame* aFrame,
                                                  nsCSSProperty aProperty)
{
  if (!IsPresContextInScriptAnimationCallback(aFrame->PresContext())) {
    return;
  }
  NotifyAnimated(aFrame, aProperty);
}

/* static */ bool
ActiveLayerTracker::IsStyleMaybeAnimated(nsIFrame* aFrame, nsCSSProperty aProperty)
{
  return IsStyleAnimated(nullptr, aFrame, aProperty);
}

/* static */ bool
ActiveLayerTracker::IsStyleAnimated(nsDisplayListBuilder* aBuilder,
                                    nsIFrame* aFrame, nsCSSProperty aProperty)
{
  // TODO: Add some abuse restrictions
  if ((aFrame->StyleDisplay()->mWillChangeBitField & NS_STYLE_WILL_CHANGE_TRANSFORM) &&
      aProperty == eCSSProperty_transform &&
      (!aBuilder || aBuilder->IsInWillChangeBudget(aFrame))) {
    return true;
  }
  if ((aFrame->StyleDisplay()->mWillChangeBitField & NS_STYLE_WILL_CHANGE_OPACITY) &&
      aProperty == eCSSProperty_opacity &&
      (!aBuilder || aBuilder->IsInWillChangeBudget(aFrame))) {
    return true;
  }

  LayerActivity* layerActivity = GetLayerActivity(aFrame);
  if (layerActivity) {
    if (layerActivity->RestyleCountForProperty(aProperty) >= 2) {
      return true;
    }
  }
  if (aProperty == eCSSProperty_transform && aFrame->Preserves3D()) {
    return IsStyleAnimated(aBuilder, aFrame->GetParent(), aProperty);
  }
  nsIContent* content = aFrame->GetContent();
  if (content) {
    return nsLayoutUtils::HasCurrentAnimationsForProperty(content, aProperty);
  }

  return false;
}

/* static */ bool
ActiveLayerTracker::IsOffsetOrMarginStyleAnimated(nsIFrame* aFrame)
{
  LayerActivity* layerActivity = GetLayerActivity(aFrame);
  if (layerActivity) {
    if (layerActivity->mLeftRestyleCount >= 2 ||
        layerActivity->mTopRestyleCount >= 2 ||
        layerActivity->mRightRestyleCount >= 2 ||
        layerActivity->mBottomRestyleCount >= 2 ||
        layerActivity->mMarginLeftRestyleCount >= 2 ||
        layerActivity->mMarginTopRestyleCount >= 2 ||
        layerActivity->mMarginRightRestyleCount >= 2 ||
        layerActivity->mMarginBottomRestyleCount >= 2) {
      return true;
    }
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
ActiveLayerTracker::Shutdown()
{
  delete gLayerActivityTracker;
  gLayerActivityTracker = nullptr;
}

}
