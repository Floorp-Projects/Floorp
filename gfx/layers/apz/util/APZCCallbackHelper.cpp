/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "APZCCallbackHelper.h"

#include "ContentHelper.h"
#include "gfxPlatform.h" // For gfxPlatform::UseTiling
#include "mozilla/dom/Element.h"
#include "mozilla/dom/TabParent.h"
#include "mozilla/layers/LayerTransactionChild.h"
#include "mozilla/layers/ShadowLayers.h"
#include "mozilla/TouchEvents.h"
#include "nsIScrollableFrame.h"
#include "nsLayoutUtils.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIDOMWindow.h"
#include "nsRefreshDriver.h"
#include "nsString.h"
#include "nsView.h"
#include "Layers.h"

#define APZCCH_LOG(...)
// #define APZCCH_LOG(...) printf_stderr("APZCCH: " __VA_ARGS__)

namespace mozilla {
namespace layers {

using dom::TabParent;

static void
AdjustDisplayPortForScrollDelta(mozilla::layers::FrameMetrics& aFrameMetrics,
                                const CSSPoint& aActualScrollOffset)
{
    // Correct the display-port by the difference between the requested scroll
    // offset and the resulting scroll offset after setting the requested value.
    ScreenPoint shift =
        (aFrameMetrics.GetScrollOffset() - aActualScrollOffset) *
        aFrameMetrics.DisplayportPixelsPerCSSPixel();
    ScreenMargin margins = aFrameMetrics.GetDisplayPortMargins();
    margins.left -= shift.x;
    margins.right += shift.x;
    margins.top -= shift.y;
    margins.bottom += shift.y;
    aFrameMetrics.SetDisplayPortMargins(margins);
}

static void
RecenterDisplayPort(mozilla::layers::FrameMetrics& aFrameMetrics)
{
    ScreenMargin margins = aFrameMetrics.GetDisplayPortMargins();
    margins.right = margins.left = margins.LeftRight() / 2;
    margins.top = margins.bottom = margins.TopBottom() / 2;
    aFrameMetrics.SetDisplayPortMargins(margins);
}

static CSSPoint
ScrollFrameTo(nsIScrollableFrame* aFrame, const CSSPoint& aPoint, bool& aSuccessOut)
{
  aSuccessOut = false;

  if (!aFrame) {
    return aPoint;
  }

  CSSPoint targetScrollPosition = aPoint;

  // If the frame is overflow:hidden on a particular axis, we don't want to allow
  // user-driven scroll on that axis. Simply set the scroll position on that axis
  // to whatever it already is. Note that this will leave the APZ's async scroll
  // position out of sync with the gecko scroll position, but APZ can deal with that
  // (by design). Note also that when we run into this case, even if both axes
  // have overflow:hidden, we want to set aSuccessOut to true, so that the displayport
  // follows the async scroll position rather than the gecko scroll position.
  CSSPoint geckoScrollPosition = CSSPoint::FromAppUnits(aFrame->GetScrollPosition());
  if (aFrame->GetScrollbarStyles().mVertical == NS_STYLE_OVERFLOW_HIDDEN) {
    targetScrollPosition.y = geckoScrollPosition.y;
  }
  if (aFrame->GetScrollbarStyles().mHorizontal == NS_STYLE_OVERFLOW_HIDDEN) {
    targetScrollPosition.x = geckoScrollPosition.x;
  }

  // If the scrollable frame is currently in the middle of an async or smooth
  // scroll then we don't want to interrupt it (see bug 961280).
  // Also if the scrollable frame got a scroll request from something other than us
  // since the last layers update, then we don't want to push our scroll request
  // because we'll clobber that one, which is bad.
  bool scrollInProgress = aFrame->IsProcessingAsyncScroll()
      || (aFrame->LastScrollOrigin() && aFrame->LastScrollOrigin() != nsGkAtoms::apz)
      || aFrame->LastSmoothScrollOrigin();
  if (!scrollInProgress) {
    aFrame->ScrollToCSSPixelsApproximate(targetScrollPosition, nsGkAtoms::apz);
    geckoScrollPosition = CSSPoint::FromAppUnits(aFrame->GetScrollPosition());
    aSuccessOut = true;
  }
  // Return the final scroll position after setting it so that anything that relies
  // on it can have an accurate value. Note that even if we set it above re-querying it
  // is a good idea because it may have gotten clamped or rounded.
  return geckoScrollPosition;
}

/**
 * Scroll the scroll frame associated with |aContent| to the scroll position
 * requested in |aMetrics|.
 * The scroll offset in |aMetrics| is updated to reflect the actual scroll
 * position.
 * The displayport stored in |aMetrics| and the callback-transform stored on
 * the content are updated to reflect any difference between the requested
 * and actual scroll positions.
 */
static void
ScrollFrame(nsIContent* aContent,
            FrameMetrics& aMetrics)
{
  // Scroll the window to the desired spot
  nsIScrollableFrame* sf = nsLayoutUtils::FindScrollableFrameFor(aMetrics.GetScrollId());
  bool scrollUpdated = false;
  CSSPoint apzScrollOffset = aMetrics.GetScrollOffset();
  CSSPoint actualScrollOffset = ScrollFrameTo(sf, apzScrollOffset, scrollUpdated);

  if (scrollUpdated) {
    // Correct the display port due to the difference between mScrollOffset and the
    // actual scroll offset.
    AdjustDisplayPortForScrollDelta(aMetrics, actualScrollOffset);
  } else {
    // For whatever reason we couldn't update the scroll offset on the scroll frame,
    // which means the data APZ used for its displayport calculation is stale. Fall
    // back to a sane default behaviour. Note that we don't tile-align the recentered
    // displayport because tile-alignment depends on the scroll position, and the
    // scroll position here is out of our control. See bug 966507 comment 21 for a
    // more detailed explanation.
    RecenterDisplayPort(aMetrics);
  }

  aMetrics.SetScrollOffset(actualScrollOffset);

  // APZ transforms inputs assuming we applied the exact scroll offset it
  // requested (|apzScrollOffset|). Since we may not have, record the difference
  // between what APZ asked for and what we actually applied, and apply it to
  // input events to compensate.
  if (aContent) {
    CSSPoint scrollDelta = apzScrollOffset - actualScrollOffset;
    aContent->SetProperty(nsGkAtoms::apzCallbackTransform, new CSSPoint(scrollDelta),
                          nsINode::DeleteProperty<CSSPoint>);
  }
}

static void
SetDisplayPortMargins(nsIPresShell* aPresShell,
                      nsIContent* aContent,
                      const FrameMetrics& aMetrics)
{
  if (!aContent) {
    return;
  }

  ScreenMargin margins = aMetrics.GetDisplayPortMargins();
  nsLayoutUtils::SetDisplayPortMargins(aContent, aPresShell, margins, 0);
  CSSRect baseCSS = aMetrics.CalculateCompositedRectInCssPixels();
  nsRect base(0, 0,
              baseCSS.width * nsPresContext::AppUnitsPerCSSPixel(),
              baseCSS.height * nsPresContext::AppUnitsPerCSSPixel());
  nsLayoutUtils::SetDisplayPortBaseIfNotSet(aContent, base);
}

static already_AddRefed<nsIPresShell>
GetPresShell(const nsIContent* aContent)
{
  nsCOMPtr<nsIPresShell> result;
  if (nsIDocument* doc = aContent->GetComposedDoc()) {
    result = doc->GetShell();
  }
  return result.forget();
}

void
APZCCallbackHelper::UpdateRootFrame(FrameMetrics& aMetrics)
{
  if (aMetrics.GetScrollId() == FrameMetrics::NULL_SCROLL_ID) {
    return;
  }
  nsIContent* content = nsLayoutUtils::FindContentFor(aMetrics.GetScrollId());
  if (!content) {
    return;
  }

  nsCOMPtr<nsIPresShell> shell = GetPresShell(content);
  if (!shell || aMetrics.GetPresShellId() != shell->GetPresShellId()) {
    return;
  }

  MOZ_ASSERT(aMetrics.GetUseDisplayPortMargins());

  float presShellResolution = nsLayoutUtils::GetResolution(shell);

  // If the pres shell resolution has changed on the content side side
  // the time this repaint request was fired, consider this request out of date
  // and drop it; setting a zoom based on the out-of-date resolution can have
  // the effect of getting us stuck with the stale resolution.
  if (presShellResolution != aMetrics.GetPresShellResolution()) {
    return;
  }

  // Set the scroll port size, which determines the scroll range. For example if
  // a 500-pixel document is shown in a 100-pixel frame, the scroll port length would
  // be 100, and gecko would limit the maximum scroll offset to 400 (so as to prevent
  // overscroll). Note that if the content here was zoomed to 2x, the document would
  // be 1000 pixels long but the frame would still be 100 pixels, and so the maximum
  // scroll range would be 900. Therefore this calculation depends on the zoom applied
  // to the content relative to the container.
  // Note that this needs to happen before scrolling the frame (in UpdateFrameCommon),
  // otherwise the scroll position may get clamped incorrectly.
  CSSSize scrollPort = aMetrics.CalculateCompositedSizeInCssPixels();
  nsLayoutUtils::SetScrollPositionClampingScrollPortSize(shell, scrollPort);

  // The pres shell resolution is updated by the the async zoom since the
  // last paint.
  presShellResolution = aMetrics.GetPresShellResolution()
                      * aMetrics.GetAsyncZoom().scale;
  nsLayoutUtils::SetResolutionAndScaleTo(shell, presShellResolution);

  // Do this as late as possible since scrolling can flush layout. It also
  // adjusts the display port margins, so do it before we set those.
  ScrollFrame(content, aMetrics);

  SetDisplayPortMargins(shell, content, aMetrics);
}

void
APZCCallbackHelper::UpdateSubFrame(FrameMetrics& aMetrics)
{
  if (aMetrics.GetScrollId() == FrameMetrics::NULL_SCROLL_ID) {
    return;
  }
  nsIContent* content = nsLayoutUtils::FindContentFor(aMetrics.GetScrollId());
  if (!content) {
    return;
  }

  MOZ_ASSERT(aMetrics.GetUseDisplayPortMargins());

  // We don't currently support zooming for subframes, so nothing extra
  // needs to be done beyond the tasks common to this and UpdateRootFrame.
  ScrollFrame(content, aMetrics);
  if (nsCOMPtr<nsIPresShell> shell = GetPresShell(content)) {
    SetDisplayPortMargins(shell, content, aMetrics);
  }
}

bool
APZCCallbackHelper::GetOrCreateScrollIdentifiers(nsIContent* aContent,
                                                 uint32_t* aPresShellIdOut,
                                                 FrameMetrics::ViewID* aViewIdOut)
{
    if (!aContent) {
        return false;
    }
    *aViewIdOut = nsLayoutUtils::FindOrCreateIDFor(aContent);
    if (nsCOMPtr<nsIPresShell> shell = GetPresShell(aContent)) {
      *aPresShellIdOut = shell->GetPresShellId();
      return true;
    }
    return false;
}

class FlingSnapEvent : public nsRunnable
{
    typedef mozilla::layers::FrameMetrics::ViewID ViewID;

public:
    FlingSnapEvent(const ViewID& aScrollId,
                   const mozilla::CSSPoint& aDestination)
        : mScrollId(aScrollId)
        , mDestination(aDestination)
    {
    }

    NS_IMETHOD Run() {
        MOZ_ASSERT(NS_IsMainThread());

        nsIScrollableFrame* sf = nsLayoutUtils::FindScrollableFrameFor(mScrollId);
        if (sf) {
            sf->FlingSnap(mDestination);
        }

        return NS_OK;
    }

protected:
    ViewID mScrollId;
    mozilla::CSSPoint mDestination;
};

void
APZCCallbackHelper::RequestFlingSnap(const FrameMetrics::ViewID& aScrollId,
                                     const mozilla::CSSPoint& aDestination)
{
    nsCOMPtr<nsIRunnable> r1 = new FlingSnapEvent(aScrollId, aDestination);
    if (!NS_IsMainThread()) {
        NS_DispatchToMainThread(r1);
    } else {
        r1->Run();
    }
}

class AcknowledgeScrollUpdateEvent : public nsRunnable
{
    typedef mozilla::layers::FrameMetrics::ViewID ViewID;

public:
    AcknowledgeScrollUpdateEvent(const ViewID& aScrollId, const uint32_t& aScrollGeneration)
        : mScrollId(aScrollId)
        , mScrollGeneration(aScrollGeneration)
    {
    }

    NS_IMETHOD Run() {
        MOZ_ASSERT(NS_IsMainThread());

        nsIScrollableFrame* sf = nsLayoutUtils::FindScrollableFrameFor(mScrollId);
        if (sf) {
            sf->ResetScrollInfoIfGeneration(mScrollGeneration);
        }

        // Since the APZ and content are in sync, we need to clear any callback transform
        // that might have been set on the last repaint request (which might have failed
        // due to the inflight scroll update that this message is acknowledging).
        nsCOMPtr<nsIContent> content = nsLayoutUtils::FindContentFor(mScrollId);
        if (content) {
            content->SetProperty(nsGkAtoms::apzCallbackTransform, new CSSPoint(),
                                 nsINode::DeleteProperty<CSSPoint>);
        }

        return NS_OK;
    }

protected:
    ViewID mScrollId;
    uint32_t mScrollGeneration;
};

void
APZCCallbackHelper::AcknowledgeScrollUpdate(const FrameMetrics::ViewID& aScrollId,
                                            const uint32_t& aScrollGeneration)
{
    nsCOMPtr<nsIRunnable> r1 = new AcknowledgeScrollUpdateEvent(aScrollId, aScrollGeneration);
    if (!NS_IsMainThread()) {
        NS_DispatchToMainThread(r1);
    } else {
        r1->Run();
    }
}

static nsIPresShell*
GetRootContentDocumentPresShellForContent(nsIContent* aContent)
{
    nsIDocument* doc = aContent->GetComposedDoc();
    if (!doc) {
        return nullptr;
    }
    nsIPresShell* shell = doc->GetShell();
    if (!shell) {
        return nullptr;
    }
    nsPresContext* context = shell->GetPresContext();
    if (!context) {
        return nullptr;
    }
    context = context->GetToplevelContentDocumentPresContext();
    if (!context) {
        return nullptr;
    }
    return context->PresShell();
}

CSSPoint
APZCCallbackHelper::ApplyCallbackTransform(const CSSPoint& aInput,
                                           const ScrollableLayerGuid& aGuid)
{
    CSSPoint input = aInput;
    if (aGuid.mScrollId == FrameMetrics::NULL_SCROLL_ID) {
        return input;
    }
    nsCOMPtr<nsIContent> content = nsLayoutUtils::FindContentFor(aGuid.mScrollId);
    if (!content) {
        return input;
    }

    // First, scale inversely by the root content document's pres shell
    // resolution to cancel the scale-to-resolution transform that the
    // compositor adds to the layer with the pres shell resolution. The points
    // sent to Gecko by APZ don't have this transform unapplied (unlike other
    // compositor-side transforms) because APZ doesn't know about it.
    if (nsIPresShell* shell = GetRootContentDocumentPresShellForContent(content)) {
        input = input / shell->GetResolution();
    }

    // Now apply the callback-transform.
    // XXX: technically we need to walk all the way up the layer tree from the layer
    // represented by |aGuid.mScrollId| up to the root of the layer tree and apply
    // the input transforms at each level in turn. However, it is quite difficult
    // to do this given that the structure of the layer tree may be different from
    // the structure of the content tree. Also it may be impossible to do correctly
    // at this point because there are other CSS transforms and such interleaved in
    // between so applying the inputTransforms all in a row at the end may leave
    // some things transformed improperly. In practice we should rarely hit scenarios
    // where any of this matters, so I'm skipping it for now and just doing the single
    // transform for the layer that the input hit.
    void* property = content->GetProperty(nsGkAtoms::apzCallbackTransform);
    if (property) {
        CSSPoint delta = (*static_cast<CSSPoint*>(property));
        input += delta;
    }
    return input;
}

LayoutDeviceIntPoint
APZCCallbackHelper::ApplyCallbackTransform(const LayoutDeviceIntPoint& aPoint,
                                           const ScrollableLayerGuid& aGuid,
                                           const CSSToLayoutDeviceScale& aScale)
{
    LayoutDevicePoint point = LayoutDevicePoint(aPoint.x, aPoint.y);
    point = ApplyCallbackTransform(point / aScale, aGuid) * aScale;
    return gfx::RoundedToInt(point);
}

void
APZCCallbackHelper::ApplyCallbackTransform(WidgetTouchEvent& aEvent,
                                           const ScrollableLayerGuid& aGuid,
                                           const CSSToLayoutDeviceScale& aScale)
{
  for (size_t i = 0; i < aEvent.touches.Length(); i++) {
    aEvent.touches[i]->mRefPoint = ApplyCallbackTransform(
        aEvent.touches[i]->mRefPoint, aGuid, aScale);
  }
}

nsEventStatus
APZCCallbackHelper::DispatchWidgetEvent(WidgetGUIEvent& aEvent)
{
  nsEventStatus status = nsEventStatus_eConsumeNoDefault;
  if (aEvent.widget) {
    aEvent.widget->DispatchEvent(&aEvent, status);
  }
  return status;
}

nsEventStatus
APZCCallbackHelper::DispatchSynthesizedMouseEvent(uint32_t aMsg,
                                                  uint64_t aTime,
                                                  const LayoutDevicePoint& aRefPoint,
                                                  Modifiers aModifiers,
                                                  nsIWidget* aWidget)
{
  MOZ_ASSERT(aMsg == NS_MOUSE_MOVE || aMsg == NS_MOUSE_BUTTON_DOWN ||
             aMsg == NS_MOUSE_BUTTON_UP || aMsg == NS_MOUSE_MOZLONGTAP);

  WidgetMouseEvent event(true, aMsg, nullptr,
                         WidgetMouseEvent::eReal, WidgetMouseEvent::eNormal);
  event.refPoint = LayoutDeviceIntPoint(aRefPoint.x, aRefPoint.y);
  event.time = aTime;
  event.button = WidgetMouseEvent::eLeftButton;
  event.inputSource = nsIDOMMouseEvent::MOZ_SOURCE_TOUCH;
  event.ignoreRootScrollFrame = true;
  if (aMsg != NS_MOUSE_MOVE) {
    event.clickCount = 1;
  }
  event.modifiers = aModifiers;
  event.widget = aWidget;

  return DispatchWidgetEvent(event);
}

bool
APZCCallbackHelper::DispatchMouseEvent(const nsCOMPtr<nsIPresShell>& aPresShell,
                                       const nsString& aType,
                                       const CSSPoint& aPoint,
                                       int32_t aButton,
                                       int32_t aClickCount,
                                       int32_t aModifiers,
                                       bool aIgnoreRootScrollFrame,
                                       unsigned short aInputSourceArg)
{
  NS_ENSURE_TRUE(aPresShell, true);

  bool defaultPrevented = false;
  nsContentUtils::SendMouseEvent(aPresShell, aType, aPoint.x, aPoint.y,
      aButton, aClickCount, aModifiers, aIgnoreRootScrollFrame, 0,
      aInputSourceArg, false, &defaultPrevented, false);
  return defaultPrevented;
}


void
APZCCallbackHelper::FireSingleTapEvent(const LayoutDevicePoint& aPoint,
                                       Modifiers aModifiers,
                                       nsIWidget* aWidget)
{
  if (aWidget->Destroyed()) {
    return;
  }
  APZCCH_LOG("Dispatching single-tap component events to %s\n",
    Stringify(aPoint).c_str());
  int time = 0;
  DispatchSynthesizedMouseEvent(NS_MOUSE_MOVE, time, aPoint, aModifiers, aWidget);
  DispatchSynthesizedMouseEvent(NS_MOUSE_BUTTON_DOWN, time, aPoint, aModifiers, aWidget);
  DispatchSynthesizedMouseEvent(NS_MOUSE_BUTTON_UP, time, aPoint, aModifiers, aWidget);
}

static nsIScrollableFrame*
GetScrollableAncestorFrame(nsIFrame* aTarget)
{
  uint32_t flags = nsLayoutUtils::SCROLLABLE_ALWAYS_MATCH_ROOT
                 | nsLayoutUtils::SCROLLABLE_ONLY_ASYNC_SCROLLABLE;
  return nsLayoutUtils::GetNearestScrollableFrame(aTarget, flags);
}

static dom::Element*
GetDisplayportElementFor(nsIScrollableFrame* aScrollableFrame)
{
  if (!aScrollableFrame) {
    return nullptr;
  }
  nsIFrame* scrolledFrame = aScrollableFrame->GetScrolledFrame();
  if (!scrolledFrame) {
    return nullptr;
  }
  // |scrolledFrame| should at this point be the root content frame of the
  // nearest ancestor scrollable frame. The element corresponding to this
  // frame should be the one with the displayport set on it, so find that
  // element and return it.
  nsIContent* content = scrolledFrame->GetContent();
  MOZ_ASSERT(content->IsElement()); // roc says this must be true
  return content->AsElement();
}


static dom::Element*
GetRootDocumentElementFor(nsIWidget* aWidget)
{
  // This returns the root element that ChromeProcessController sets the
  // displayport on during initialization.
  if (nsView* view = nsView::GetViewFor(aWidget)) {
    if (nsIPresShell* shell = view->GetPresShell()) {
      MOZ_ASSERT(shell->GetDocument());
      return shell->GetDocument()->GetDocumentElement();
    }
  }
  return nullptr;
}

// Determine the scrollable target frame for the given point and add it to
// the target list. If the frame doesn't have a displayport, set one.
// Return whether or not a displayport was set.
static bool
PrepareForSetTargetAPZCNotification(nsIWidget* aWidget,
                                    const ScrollableLayerGuid& aGuid,
                                    nsIFrame* aRootFrame,
                                    const LayoutDeviceIntPoint& aRefPoint,
                                    nsTArray<ScrollableLayerGuid>* aTargets)
{
  ScrollableLayerGuid guid(aGuid.mLayersId, 0, FrameMetrics::NULL_SCROLL_ID);
  nsPoint point =
    nsLayoutUtils::GetEventCoordinatesRelativeTo(aWidget, aRefPoint, aRootFrame);
  nsIFrame* target =
    nsLayoutUtils::GetFrameForPoint(aRootFrame, point, nsLayoutUtils::IGNORE_ROOT_SCROLL_FRAME);
  nsIScrollableFrame* scrollAncestor = target
    ? GetScrollableAncestorFrame(target)
    : aRootFrame->PresContext()->PresShell()->GetRootScrollFrameAsScrollable();

  // Assuming that if there's no scrollAncestor, there's already a displayPort.
  nsCOMPtr<dom::Element> dpElement = scrollAncestor
    ? GetDisplayportElementFor(scrollAncestor)
    : GetRootDocumentElementFor(aWidget);

  nsAutoString dpElementDesc;
  if (dpElement) {
    dpElement->Describe(dpElementDesc);
  }
  APZCCH_LOG("For event at %s found scrollable element %p (%s)\n",
      Stringify(aRefPoint).c_str(), dpElement.get(),
      NS_LossyConvertUTF16toASCII(dpElementDesc).get());

  bool guidIsValid = APZCCallbackHelper::GetOrCreateScrollIdentifiers(
    dpElement, &(guid.mPresShellId), &(guid.mScrollId));
  aTargets->AppendElement(guid);

  if (!guidIsValid || nsLayoutUtils::GetDisplayPort(dpElement, nullptr)) {
    return false;
  }

  APZCCH_LOG("%p didn't have a displayport, so setting one...\n", dpElement.get());
  MOZ_ASSERT(scrollAncestor);
  return nsLayoutUtils::CalculateAndSetDisplayPortMargins(
      scrollAncestor, nsLayoutUtils::RepaintMode::Repaint);
}

static void
SendLayersDependentApzcTargetConfirmation(nsIPresShell* aShell, uint64_t aInputBlockId,
                                          const nsTArray<ScrollableLayerGuid>& aTargets)
{
  LayerManager* lm = aShell->GetLayerManager();
  if (!lm) {
    return;
  }

  LayerTransactionChild* shadow = lm->AsShadowForwarder()->GetShadowManager();
  if (!shadow) {
    return;
  }

  shadow->SendSetConfirmedTargetAPZC(aInputBlockId, aTargets);
}

class DisplayportSetListener : public nsAPostRefreshObserver {
public:
  DisplayportSetListener(nsIPresShell* aPresShell,
                         const uint64_t& aInputBlockId,
                         const nsTArray<ScrollableLayerGuid>& aTargets)
    : mPresShell(aPresShell)
    , mInputBlockId(aInputBlockId)
    , mTargets(aTargets)
  {
  }

  virtual ~DisplayportSetListener()
  {
  }

  void DidRefresh() override {
    if (!mPresShell) {
      MOZ_ASSERT_UNREACHABLE("Post-refresh observer fired again after failed attempt at unregistering it");
      return;
    }

    APZCCH_LOG("Got refresh, sending target APZCs for input block %" PRIu64 "\n", mInputBlockId);
    SendLayersDependentApzcTargetConfirmation(mPresShell, mInputBlockId, mTargets);

    if (!mPresShell->RemovePostRefreshObserver(this)) {
      MOZ_ASSERT_UNREACHABLE("Unable to unregister post-refresh observer! Leaking it instead of leaving garbage registered");
      // Graceful handling, just in case...
      mPresShell = nullptr;
      return;
    }

    delete this;
  }

private:
  nsRefPtr<nsIPresShell> mPresShell;
  uint64_t mInputBlockId;
  nsTArray<ScrollableLayerGuid> mTargets;
};

// Sends a SetTarget notification for APZC, given one or more previous
// calls to PrepareForAPZCSetTargetNotification().
static void
SendSetTargetAPZCNotificationHelper(nsIWidget* aWidget,
                                    nsIPresShell* aShell,
                                    const uint64_t& aInputBlockId,
                                    const nsTArray<ScrollableLayerGuid>& aTargets,
                                    bool aWaitForRefresh)
{
  bool waitForRefresh = aWaitForRefresh;
  if (waitForRefresh) {
    APZCCH_LOG("At least one target got a new displayport, need to wait for refresh\n");
    waitForRefresh = aShell->AddPostRefreshObserver(
      new DisplayportSetListener(aShell, aInputBlockId, aTargets));
  }
  if (!waitForRefresh) {
    APZCCH_LOG("Sending target APZCs for input block %" PRIu64 "\n", aInputBlockId);
    aWidget->SetConfirmedTargetAPZC(aInputBlockId, aTargets);
  } else {
    APZCCH_LOG("Successfully registered post-refresh observer\n");
  }
}

void
APZCCallbackHelper::SendSetTargetAPZCNotification(nsIWidget* aWidget,
                                                  nsIDocument* aDocument,
                                                  const WidgetGUIEvent& aEvent,
                                                  const ScrollableLayerGuid& aGuid,
                                                  uint64_t aInputBlockId)
{
  if (!aWidget || !aDocument) {
    return;
  }
  if (nsIPresShell* shell = aDocument->GetShell()) {
    if (nsIFrame* rootFrame = shell->GetRootFrame()) {
      bool waitForRefresh = false;
      nsTArray<ScrollableLayerGuid> targets;

      if (const WidgetTouchEvent* touchEvent = aEvent.AsTouchEvent()) {
        for (size_t i = 0; i < touchEvent->touches.Length(); i++) {
          waitForRefresh |= PrepareForSetTargetAPZCNotification(aWidget, aGuid,
              rootFrame, touchEvent->touches[i]->mRefPoint, &targets);
        }
      } else if (const WidgetWheelEvent* wheelEvent = aEvent.AsWheelEvent()) {
        waitForRefresh = PrepareForSetTargetAPZCNotification(aWidget, aGuid,
            rootFrame, wheelEvent->refPoint, &targets);
      }
      // TODO: Do other types of events need to be handled?

      if (!targets.IsEmpty()) {
        SendSetTargetAPZCNotificationHelper(
          aWidget,
          shell,
          aInputBlockId,
          targets,
          waitForRefresh);
      }
    }
  }
}

void
APZCCallbackHelper::SendSetAllowedTouchBehaviorNotification(
        nsIWidget* aWidget,
        const WidgetTouchEvent& aEvent,
        uint64_t aInputBlockId,
        const nsRefPtr<SetAllowedTouchBehaviorCallback>& aCallback)
{
  nsTArray<TouchBehaviorFlags> flags;
  for (uint32_t i = 0; i < aEvent.touches.Length(); i++) {
    flags.AppendElement(widget::ContentHelper::GetAllowedTouchBehavior(aWidget, aEvent.touches[i]->mRefPoint));
  }
  aCallback->Run(aInputBlockId, flags);
}

void
APZCCallbackHelper::NotifyMozMouseScrollEvent(const FrameMetrics::ViewID& aScrollId, const nsString& aEvent)
{
  nsCOMPtr<nsIContent> targetContent = nsLayoutUtils::FindContentFor(aScrollId);
  if (!targetContent) {
    return;
  }
  nsCOMPtr<nsIDocument> ownerDoc = targetContent->OwnerDoc();
  if (!ownerDoc) {
    return;
  }

  nsContentUtils::DispatchTrustedEvent(
    ownerDoc, targetContent,
    aEvent,
    true, true);
}

void
APZCCallbackHelper::NotifyFlushComplete()
{
  MOZ_ASSERT(NS_IsMainThread());
  nsCOMPtr<nsIObserverService> observerService = mozilla::services::GetObserverService();
  MOZ_ASSERT(observerService);
  observerService->NotifyObservers(nullptr, "apz-repaints-flushed", nullptr);
}

} // namespace layers
} // namespace mozilla

