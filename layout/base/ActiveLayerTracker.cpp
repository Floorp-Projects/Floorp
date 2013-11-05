/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ActiveLayerTracker.h"

#include "nsExpirationTracker.h"
#include "nsIFrame.h"
#include "nsIContent.h"
#include "nsRefreshDriver.h"
#include "nsPIDOMWindow.h"
#include "nsIDocument.h"

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
  LayerActivity(nsIFrame* aFrame)
    : mFrame(aFrame)
    , mOpacityRestyleCount(0)
    , mTransformRestyleCount(0)
    , mContentActive(false)
  {}
  ~LayerActivity();
  nsExpirationState* GetExpirationState() { return &mState; }
  uint8_t& RestyleCountForProperty(nsCSSProperty aProperty)
  {
    switch (aProperty) {
    case eCSSProperty_opacity: return mOpacityRestyleCount;
    case eCSSProperty_transform: return mTransformRestyleCount;
    default: MOZ_ASSERT(false); return mOpacityRestyleCount;
    }
  }

  nsIFrame* mFrame;
  nsExpirationState mState;
  // Number of restyle operations detected
  uint8_t mOpacityRestyleCount;
  uint8_t mTransformRestyleCount;
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
  if (mFrame) {
    NS_ASSERTION(gLayerActivityTracker, "Should still have a tracker");
    gLayerActivityTracker->RemoveObject(this);
  }
}

static void DestroyLayerActivity(void* aPropertyValue)
{
  delete static_cast<LayerActivity*>(aPropertyValue);
}

NS_DECLARE_FRAME_PROPERTY(LayerActivityProperty, DestroyLayerActivity)

void
LayerActivityTracker::NotifyExpired(LayerActivity* aObject)
{
  RemoveObject(aObject);

  nsIFrame* f = aObject->mFrame;
  aObject->mFrame = nullptr;

  f->SchedulePaint();
  f->Properties().Delete(LayerActivityProperty());
}

static LayerActivity*
GetLayerActivity(nsIFrame* aFrame)
{
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
    properties.Set(LayerActivityProperty(), layerActivity);
  }
  return layerActivity;
}

/* static */ void
ActiveLayerTracker::NotifyRestyle(nsIFrame* aFrame, nsCSSProperty aProperty)
{
  LayerActivity* layerActivity = GetLayerActivityForUpdate(aFrame);
  uint8_t& mutationCount = layerActivity->RestyleCountForProperty(aProperty);
  mutationCount = uint8_t(std::min(0xFF, mutationCount + 1));
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
ActiveLayerTracker::IsStyleAnimated(nsIFrame* aFrame, nsCSSProperty aProperty)
{
  LayerActivity* layerActivity = GetLayerActivity(aFrame);
  if (layerActivity) {
    if (layerActivity->RestyleCountForProperty(aProperty) >= 2) {
      return true;
    }
  }
  if (aProperty == eCSSProperty_transform && aFrame->Preserves3D()) {
    return IsStyleAnimated(aFrame->GetParent(), aProperty);
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
