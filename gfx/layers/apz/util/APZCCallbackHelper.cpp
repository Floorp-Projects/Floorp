/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "APZCCallbackHelper.h"

#include "TouchActionHelper.h"
#include "gfxPlatform.h" // For gfxPlatform::UseTiling
#include "gfxPrefs.h"
#include "LayersLogging.h"  // For Stringify
#include "mozilla/dom/Element.h"
#include "mozilla/dom/TabParent.h"
#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/layers/LayerTransactionChild.h"
#include "mozilla/layers/ShadowLayers.h"
#include "mozilla/layers/WebRenderLayerManager.h"
#include "mozilla/layers/WebRenderBridgeChild.h"
#include "mozilla/TouchEvents.h"
#include "nsContentUtils.h"
#include "nsContainerFrame.h"
#include "nsIScrollableFrame.h"
#include "nsLayoutUtils.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIDOMWindow.h"
#include "nsIDOMWindowUtils.h"
#include "nsRefreshDriver.h"
#include "nsString.h"
#include "nsView.h"
#include "Layers.h"

// #define APZCCH_LOGGING 1
#ifdef APZCCH_LOGGING
#define APZCCH_LOG(...) printf_stderr("APZCCH: " __VA_ARGS__)
#else
#define APZCCH_LOG(...)
#endif

namespace mozilla {
namespace layers {

using dom::TabParent;

uint64_t APZCCallbackHelper::sLastTargetAPZCNotificationInputBlock = uint64_t(-1);

void
APZCCallbackHelper::AdjustDisplayPortForScrollDelta(
    mozilla::layers::FrameMetrics& aFrameMetrics,
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
ScrollFrameTo(nsIScrollableFrame* aFrame, const FrameMetrics& aMetrics, bool& aSuccessOut)
{
  aSuccessOut = false;
  CSSPoint targetScrollPosition = aMetrics.GetScrollOffset();

  if (!aFrame) {
    return targetScrollPosition;
  }

  CSSPoint geckoScrollPosition = CSSPoint::FromAppUnits(aFrame->GetScrollPosition());

  // If the repaint request was triggered due to a previous main-thread scroll
  // offset update sent to the APZ, then we don't need to do another scroll here
  // and we can just return.
  if (!aMetrics.GetScrollOffsetUpdated()) {
    return geckoScrollPosition;
  }

  // If the frame is overflow:hidden on a particular axis, we don't want to allow
  // user-driven scroll on that axis. Simply set the scroll position on that axis
  // to whatever it already is. Note that this will leave the APZ's async scroll
  // position out of sync with the gecko scroll position, but APZ can deal with that
  // (by design). Note also that when we run into this case, even if both axes
  // have overflow:hidden, we want to set aSuccessOut to true, so that the displayport
  // follows the async scroll position rather than the gecko scroll position.
  if (aFrame->GetScrollbarStyles().mVertical == NS_STYLE_OVERFLOW_HIDDEN) {
    targetScrollPosition.y = geckoScrollPosition.y;
  }
  if (aFrame->GetScrollbarStyles().mHorizontal == NS_STYLE_OVERFLOW_HIDDEN) {
    targetScrollPosition.x = geckoScrollPosition.x;
  }

  // If the scrollable frame is currently in the middle of an async or smooth
  // scroll then we don't want to interrupt it (see bug 961280).
  // Also if the scrollable frame got a scroll request from a higher priority origin
  // since the last layers update, then we don't want to push our scroll request
  // because we'll clobber that one, which is bad.
  bool scrollInProgress = APZCCallbackHelper::IsScrollInProgress(aFrame);
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
  if (sf) {
    sf->ResetScrollInfoIfGeneration(aMetrics.GetScrollGeneration());
    sf->SetScrollableByAPZ(!aMetrics.IsScrollInfoLayer());
  }
  bool scrollUpdated = false;
  CSSPoint apzScrollOffset = aMetrics.GetScrollOffset();
  CSSPoint actualScrollOffset = ScrollFrameTo(sf, aMetrics, scrollUpdated);

  if (scrollUpdated) {
    if (aMetrics.IsScrollInfoLayer()) {
      // In cases where the APZ scroll offset is different from the content scroll
      // offset, we want to interpret the margins as relative to the APZ scroll
      // offset except when the frame is not scrollable by APZ. Therefore, if the
      // layer is a scroll info layer, we leave the margins as-is and they will
      // be interpreted as relative to the content scroll offset.
      if (nsIFrame* frame = aContent->GetPrimaryFrame()) {
        frame->SchedulePaint();
      }
    } else {
      // Correct the display port due to the difference between mScrollOffset and the
      // actual scroll offset.
      APZCCallbackHelper::AdjustDisplayPortForScrollDelta(aMetrics, actualScrollOffset);
    }
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
  // Note that if the main-thread had a change in its scroll position, we don't
  // want to record that difference here, because it can be large and throw off
  // input events by a large amount. It is also going to be transient, because
  // any main-thread scroll position change will be synced to APZ and we will
  // get another repaint request when APZ confirms. In the interval while this
  // is happening we can just leave the callback transform as it was.
  bool mainThreadScrollChanged =
    sf && sf->CurrentScrollGeneration() != aMetrics.GetScrollGeneration() && nsLayoutUtils::CanScrollOriginClobberApz(sf->LastScrollOrigin());
  if (aContent && !mainThreadScrollChanged) {
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

  bool hadDisplayPort = nsLayoutUtils::HasDisplayPort(aContent);
  ScreenMargin margins = aMetrics.GetDisplayPortMargins();
  nsLayoutUtils::SetDisplayPortMargins(aContent, aPresShell, margins, 0);
  if (!hadDisplayPort) {
    nsLayoutUtils::SetZeroMarginDisplayPortOnAsyncScrollableAncestors(
        aContent->GetPrimaryFrame(), nsLayoutUtils::RepaintMode::Repaint);
  }

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

static void
SetPaintRequestTime(nsIContent* aContent, const TimeStamp& aPaintRequestTime)
{
  aContent->SetProperty(nsGkAtoms::paintRequestTime,
                        new TimeStamp(aPaintRequestTime),
                        nsINode::DeleteProperty<TimeStamp>);
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

  if (gfxPrefs::APZAllowZooming()) {
    // If zooming is disabled then we don't really want to let APZ fiddle
    // with these things. In theory setting the resolution here should be a
    // no-op, but setting the SPCSPS is bad because it can cause a stale value
    // to be returned by window.innerWidth/innerHeight (see bug 1187792).

    float presShellResolution = shell->GetResolution();

    // If the pres shell resolution has changed on the content side side
    // the time this repaint request was fired, consider this request out of date
    // and drop it; setting a zoom based on the out-of-date resolution can have
    // the effect of getting us stuck with the stale resolution.
    if (!FuzzyEqualsMultiplicative(presShellResolution, aMetrics.GetPresShellResolution())) {
      return;
    }

    // The pres shell resolution is updated by the the async zoom since the
    // last paint.
    presShellResolution = aMetrics.GetPresShellResolution()
                        * aMetrics.GetAsyncZoom().scale;
    shell->SetResolutionAndScaleTo(presShellResolution);
  }

  // Do this as late as possible since scrolling can flush layout. It also
  // adjusts the display port margins, so do it before we set those.
  ScrollFrame(content, aMetrics);

  SetDisplayPortMargins(shell, content, aMetrics);
  SetPaintRequestTime(content, aMetrics.GetPaintRequestTime());
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
  SetPaintRequestTime(content, aMetrics.GetPaintRequestTime());
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

void
APZCCallbackHelper::InitializeRootDisplayport(nsIPresShell* aPresShell)
{
  // Create a view-id and set a zero-margin displayport for the root element
  // of the root document in the chrome process. This ensures that the scroll
  // frame for this element gets an APZC, which in turn ensures that all content
  // in the chrome processes is covered by an APZC.
  // The displayport is zero-margin because this element is generally not
  // actually scrollable (if it is, APZC will set proper margins when it's
  // scrolled).
  if (!aPresShell) {
    return;
  }

  MOZ_ASSERT(aPresShell->GetDocument());
  nsIContent* content = aPresShell->GetDocument()->GetDocumentElement();
  if (!content) {
    return;
  }

  uint32_t presShellId;
  FrameMetrics::ViewID viewId;
  if (APZCCallbackHelper::GetOrCreateScrollIdentifiers(content, &presShellId, &viewId)) {
    nsPresContext* pc = aPresShell->GetPresContext();
    // This code is only correct for root content or toplevel documents.
    MOZ_ASSERT(!pc || pc->IsRootContentDocument() || !pc->GetParentPresContext());
    nsIFrame* frame = aPresShell->GetRootScrollFrame();
    if (!frame) {
      frame = aPresShell->GetRootFrame();
    }
    nsRect baseRect;
    if (frame) {
      baseRect =
        nsRect(nsPoint(0, 0), nsLayoutUtils::CalculateCompositionSizeForFrame(frame));
    } else if (pc) {
      baseRect = nsRect(nsPoint(0, 0), pc->GetVisibleArea().Size());
    }
    nsLayoutUtils::SetDisplayPortBaseIfNotSet(content, baseRect);
    // Note that we also set the base rect that goes with these margins in
    // nsRootBoxFrame::BuildDisplayList.
    nsLayoutUtils::SetDisplayPortMargins(content, aPresShell, ScreenMargin(), 0,
        nsLayoutUtils::RepaintMode::DoNotRepaint);
    nsLayoutUtils::SetZeroMarginDisplayPortOnAsyncScrollableAncestors(
        content->GetPrimaryFrame(), nsLayoutUtils::RepaintMode::DoNotRepaint);
  }
}

nsPresContext*
APZCCallbackHelper::GetPresContextForContent(nsIContent* aContent)
{
  nsIDocument* doc = aContent->GetComposedDoc();
  if (!doc) {
      return nullptr;
  }
  nsIPresShell* shell = doc->GetShell();
  if (!shell) {
      return nullptr;
  }
  return shell->GetPresContext();
}

nsIPresShell*
APZCCallbackHelper::GetRootContentDocumentPresShellForContent(nsIContent* aContent)
{
    nsPresContext* context = GetPresContextForContent(aContent);
    if (!context) {
        return nullptr;
    }
    context = context->GetToplevelContentDocumentPresContext();
    if (!context) {
        return nullptr;
    }
    return context->PresShell();
}

static nsIPresShell*
GetRootDocumentPresShell(nsIContent* aContent)
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
    context = context->GetRootPresContext();
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
    if (nsIPresShell* shell = GetRootDocumentPresShell(content)) {
        input = input / shell->GetResolution();
    }

    // This represents any resolution on the Root Content Document (RCD)
    // that's not on the Root Document (RD). That is, on platforms where
    // RCD == RD, it's 1, and on platforms where RCD != RD, it's the RCD
    // resolution. 'input' has this resolution applied, but the scroll
    // delta retrieved below do not, so we need to apply them to the
    // delta before adding the delta to 'input'. (Technically, deltas
    // from scroll frames outside the RCD would already have this
    // resolution applied, but we don't have such scroll frames in
    // practice.)
    float nonRootResolution = 1.0f;
    if (nsIPresShell* shell = GetRootContentDocumentPresShellForContent(content)) {
      nonRootResolution = shell->GetCumulativeNonRootScaleResolution();
    }
    // Now apply the callback-transform. This is only approximately correct,
    // see the comment on GetCumulativeApzCallbackTransform for details.
    CSSPoint transform = nsLayoutUtils::GetCumulativeApzCallbackTransform(content->GetPrimaryFrame());
    return input + transform * nonRootResolution;
}

LayoutDeviceIntPoint
APZCCallbackHelper::ApplyCallbackTransform(const LayoutDeviceIntPoint& aPoint,
                                           const ScrollableLayerGuid& aGuid,
                                           const CSSToLayoutDeviceScale& aScale)
{
    LayoutDevicePoint point = LayoutDevicePoint(aPoint.x, aPoint.y);
    point = ApplyCallbackTransform(point / aScale, aGuid) * aScale;
    return LayoutDeviceIntPoint::Round(point);
}

void
APZCCallbackHelper::ApplyCallbackTransform(WidgetEvent& aEvent,
                                           const ScrollableLayerGuid& aGuid,
                                           const CSSToLayoutDeviceScale& aScale)
{
  if (aEvent.AsTouchEvent()) {
    WidgetTouchEvent& event = *(aEvent.AsTouchEvent());
    for (size_t i = 0; i < event.mTouches.Length(); i++) {
      event.mTouches[i]->mRefPoint = ApplyCallbackTransform(
          event.mTouches[i]->mRefPoint, aGuid, aScale);
    }
  } else {
    aEvent.mRefPoint = ApplyCallbackTransform(aEvent.mRefPoint, aGuid, aScale);
  }
}

nsEventStatus
APZCCallbackHelper::DispatchWidgetEvent(WidgetGUIEvent& aEvent)
{
  nsEventStatus status = nsEventStatus_eConsumeNoDefault;
  if (aEvent.mWidget) {
    aEvent.mWidget->DispatchEvent(&aEvent, status);
  }
  return status;
}

nsEventStatus
APZCCallbackHelper::DispatchSynthesizedMouseEvent(EventMessage aMsg,
                                                  uint64_t aTime,
                                                  const LayoutDevicePoint& aRefPoint,
                                                  Modifiers aModifiers,
                                                  int32_t aClickCount,
                                                  nsIWidget* aWidget)
{
  MOZ_ASSERT(aMsg == eMouseMove || aMsg == eMouseDown ||
             aMsg == eMouseUp || aMsg == eMouseLongTap);

  WidgetMouseEvent event(true, aMsg, aWidget,
                         WidgetMouseEvent::eReal, WidgetMouseEvent::eNormal);
  event.mRefPoint = LayoutDeviceIntPoint::Truncate(aRefPoint.x, aRefPoint.y);
  event.mTime = aTime;
  event.button = WidgetMouseEvent::eLeftButton;
  event.inputSource = nsIDOMMouseEvent::MOZ_SOURCE_TOUCH;
  if (aMsg == eMouseLongTap) {
    event.mFlags.mOnlyChromeDispatch = true;
  }
  event.mIgnoreRootScrollFrame = true;
  if (aMsg != eMouseMove) {
    event.mClickCount = aClickCount;
  }
  event.mModifiers = aModifiers;
  // Real touch events will generate corresponding pointer events. We set
  // convertToPointer to false to prevent the synthesized mouse events generate
  // pointer events again.
  event.convertToPointer = false;
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
                                       unsigned short aInputSourceArg,
                                       uint32_t aPointerId)
{
  NS_ENSURE_TRUE(aPresShell, true);

  bool defaultPrevented = false;
  nsContentUtils::SendMouseEvent(aPresShell, aType, aPoint.x, aPoint.y,
      aButton, nsIDOMWindowUtils::MOUSE_BUTTONS_NOT_SPECIFIED, aClickCount,
      aModifiers, aIgnoreRootScrollFrame, 0, aInputSourceArg, aPointerId, false,
      &defaultPrevented, false, /* aIsWidgetEventSynthesized = */ false);
  return defaultPrevented;
}


void
APZCCallbackHelper::FireSingleTapEvent(const LayoutDevicePoint& aPoint,
                                       Modifiers aModifiers,
                                       int32_t aClickCount,
                                       nsIWidget* aWidget)
{
  if (aWidget->Destroyed()) {
    return;
  }
  APZCCH_LOG("Dispatching single-tap component events to %s\n",
    Stringify(aPoint).c_str());
  int time = 0;
  DispatchSynthesizedMouseEvent(eMouseMove, time, aPoint, aModifiers, aClickCount, aWidget);
  DispatchSynthesizedMouseEvent(eMouseDown, time, aPoint, aModifiers, aClickCount, aWidget);
  DispatchSynthesizedMouseEvent(eMouseUp, time, aPoint, aModifiers, aClickCount, aWidget);
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

static nsIFrame*
UpdateRootFrameForTouchTargetDocument(nsIFrame* aRootFrame)
{
#if defined(MOZ_WIDGET_ANDROID)
  // Re-target so that the hit test is performed relative to the frame for the
  // Root Content Document instead of the Root Document which are different in
  // Android. See bug 1229752 comment 16 for an explanation of why this is necessary.
  if (nsIDocument* doc = aRootFrame->PresContext()->PresShell()->GetPrimaryContentDocument()) {
    if (nsIPresShell* shell = doc->GetShell()) {
      if (nsIFrame* frame = shell->GetRootFrame()) {
        return frame;
      }
    }
  }
#endif
  return aRootFrame;
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
  uint32_t flags = 0;
#ifdef MOZ_WIDGET_ANDROID
  // On Android, we need IGNORE_ROOT_SCROLL_FRAME for correct hit testing
  // when zoomed out. On desktop, don't use it because it interferes with
  // hit testing for some purposes such as scrollbar dragging.
  flags = nsLayoutUtils::IGNORE_ROOT_SCROLL_FRAME;
#endif
  nsIFrame* target =
    nsLayoutUtils::GetFrameForPoint(aRootFrame, point, flags);
  nsIScrollableFrame* scrollAncestor = target
    ? nsLayoutUtils::GetAsyncScrollableAncestorFrame(target)
    : aRootFrame->PresContext()->PresShell()->GetRootScrollFrameAsScrollable();

  // Assuming that if there's no scrollAncestor, there's already a displayPort.
  nsCOMPtr<dom::Element> dpElement = scrollAncestor
    ? GetDisplayportElementFor(scrollAncestor)
    : GetRootDocumentElementFor(aWidget);

#ifdef APZCCH_LOGGING
  nsAutoString dpElementDesc;
  if (dpElement) {
    dpElement->Describe(dpElementDesc);
  }
  APZCCH_LOG("For event at %s found scrollable element %p (%s)\n",
      Stringify(aRefPoint).c_str(), dpElement.get(),
      NS_LossyConvertUTF16toASCII(dpElementDesc).get());
#endif

  bool guidIsValid = APZCCallbackHelper::GetOrCreateScrollIdentifiers(
    dpElement, &(guid.mPresShellId), &(guid.mScrollId));
  aTargets->AppendElement(guid);

  if (!guidIsValid || nsLayoutUtils::HasDisplayPort(dpElement)) {
    return false;
  }

  if (!scrollAncestor) {
    MOZ_ASSERT(false);  // If you hit this, please file a bug with STR.

    // Attempt some sort of graceful handling based on a theory as to why we
    // reach this point...
    // If we get here, the document element is non-null, valid, but doesn't have
    // a displayport. It's possible that the init code in ChromeProcessController
    // failed for some reason, or the document element got swapped out at some
    // later time. In this case let's try to set a displayport on the document
    // element again and bail out on this operation.
    APZCCH_LOG("Widget %p's document element %p didn't have a displayport\n",
        aWidget, dpElement.get());
    APZCCallbackHelper::InitializeRootDisplayport(aRootFrame->PresContext()->PresShell());
    return false;
  }

  APZCCH_LOG("%p didn't have a displayport, so setting one...\n", dpElement.get());
  bool activated = nsLayoutUtils::CalculateAndSetDisplayPortMargins(
      scrollAncestor, nsLayoutUtils::RepaintMode::Repaint);
  if (!activated) {
    return false;
  }

  nsIFrame* frame = do_QueryFrame(scrollAncestor);
  nsLayoutUtils::SetZeroMarginDisplayPortOnAsyncScrollableAncestors(frame,
    nsLayoutUtils::RepaintMode::Repaint);

  return true;
}

static void
SendLayersDependentApzcTargetConfirmation(nsIPresShell* aShell, uint64_t aInputBlockId,
                                          const nsTArray<ScrollableLayerGuid>& aTargets)
{
  LayerManager* lm = aShell->GetLayerManager();
  if (!lm) {
    return;
  }

  if (WebRenderLayerManager* wrlm = lm->AsWebRenderLayerManager()) {
    if (WebRenderBridgeChild* wrbc = wrlm->WrBridge()) {
      wrbc->SendSetConfirmedTargetAPZC(aInputBlockId, aTargets);
    }
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
    SendLayersDependentApzcTargetConfirmation(mPresShell, mInputBlockId, Move(mTargets));

    if (!mPresShell->RemovePostRefreshObserver(this)) {
      MOZ_ASSERT_UNREACHABLE("Unable to unregister post-refresh observer! Leaking it instead of leaving garbage registered");
      // Graceful handling, just in case...
      mPresShell = nullptr;
      return;
    }

    delete this;
  }

private:
  RefPtr<nsIPresShell> mPresShell;
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
      new DisplayportSetListener(aShell, aInputBlockId, Move(aTargets)));
  }
  if (!waitForRefresh) {
    APZCCH_LOG("Sending target APZCs for input block %" PRIu64 "\n", aInputBlockId);
    aWidget->SetConfirmedTargetAPZC(aInputBlockId, aTargets);
  } else {
    APZCCH_LOG("Successfully registered post-refresh observer\n");
  }
}

bool
APZCCallbackHelper::SendSetTargetAPZCNotification(nsIWidget* aWidget,
                                                  nsIDocument* aDocument,
                                                  const WidgetGUIEvent& aEvent,
                                                  const ScrollableLayerGuid& aGuid,
                                                  uint64_t aInputBlockId)
{
  if (!aWidget || !aDocument) {
    return false;
  }
  if (aInputBlockId == sLastTargetAPZCNotificationInputBlock) {
    // We have already confirmed the target APZC for a previous event of this
    // input block. If we activated a scroll frame for this input block,
    // sending another target APZC confirmation would be harmful, as it might
    // race the original confirmation (which needs to go through a layers
    // transaction).
    APZCCH_LOG("Not resending target APZC confirmation for input block %" PRIu64 "\n", aInputBlockId);
    return false;
  }
  sLastTargetAPZCNotificationInputBlock = aInputBlockId;
  if (nsIPresShell* shell = aDocument->GetShell()) {
    if (nsIFrame* rootFrame = shell->GetRootFrame()) {
      rootFrame = UpdateRootFrameForTouchTargetDocument(rootFrame);

      bool waitForRefresh = false;
      nsTArray<ScrollableLayerGuid> targets;

      if (const WidgetTouchEvent* touchEvent = aEvent.AsTouchEvent()) {
        for (size_t i = 0; i < touchEvent->mTouches.Length(); i++) {
          waitForRefresh |= PrepareForSetTargetAPZCNotification(aWidget, aGuid,
              rootFrame, touchEvent->mTouches[i]->mRefPoint, &targets);
        }
      } else if (const WidgetWheelEvent* wheelEvent = aEvent.AsWheelEvent()) {
        waitForRefresh = PrepareForSetTargetAPZCNotification(aWidget, aGuid,
            rootFrame, wheelEvent->mRefPoint, &targets);
      } else if (const WidgetMouseEvent* mouseEvent = aEvent.AsMouseEvent()) {
        waitForRefresh = PrepareForSetTargetAPZCNotification(aWidget, aGuid,
            rootFrame, mouseEvent->mRefPoint, &targets);
      }
      // TODO: Do other types of events need to be handled?

      if (!targets.IsEmpty()) {
        SendSetTargetAPZCNotificationHelper(
          aWidget,
          shell,
          aInputBlockId,
          Move(targets),
          waitForRefresh);
      }

      return waitForRefresh;
    }
  }
  return false;
}

void
APZCCallbackHelper::SendSetAllowedTouchBehaviorNotification(
        nsIWidget* aWidget,
        nsIDocument* aDocument,
        const WidgetTouchEvent& aEvent,
        uint64_t aInputBlockId,
        const SetAllowedTouchBehaviorCallback& aCallback)
{
  if (nsIPresShell* shell = aDocument->GetShell()) {
    if (nsIFrame* rootFrame = shell->GetRootFrame()) {
      rootFrame = UpdateRootFrameForTouchTargetDocument(rootFrame);

      nsTArray<TouchBehaviorFlags> flags;
      for (uint32_t i = 0; i < aEvent.mTouches.Length(); i++) {
        flags.AppendElement(
          TouchActionHelper::GetAllowedTouchBehavior(aWidget,
                rootFrame, aEvent.mTouches[i]->mRefPoint));
      }
      aCallback(aInputBlockId, Move(flags));
    }
  }
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
APZCCallbackHelper::NotifyFlushComplete(nsIPresShell* aShell)
{
  MOZ_ASSERT(NS_IsMainThread());
  // In some cases, flushing the APZ state to the main thread doesn't actually
  // trigger a flush and repaint (this is an intentional optimization - the stuff
  // visible to the user is still correct). However, reftests update their
  // snapshot based on invalidation events that are emitted during paints,
  // so we ensure that we kick off a paint when an APZ flush is done. Note that
  // only chrome/testing code can trigger this behaviour.
  if (aShell && aShell->GetRootFrame()) {
    aShell->GetRootFrame()->SchedulePaint();
  }

  nsCOMPtr<nsIObserverService> observerService = mozilla::services::GetObserverService();
  MOZ_ASSERT(observerService);
  observerService->NotifyObservers(nullptr, "apz-repaints-flushed", nullptr);
}

static int32_t sActiveSuppressDisplayport = 0;
static bool sDisplayPortSuppressionRespected = true;

void
APZCCallbackHelper::SuppressDisplayport(const bool& aEnabled,
                                        const nsCOMPtr<nsIPresShell>& aShell)
{
  if (aEnabled) {
    sActiveSuppressDisplayport++;
  } else {
    bool isSuppressed = IsDisplayportSuppressed();
    sActiveSuppressDisplayport--;
    if (isSuppressed && !IsDisplayportSuppressed() &&
        aShell && aShell->GetRootFrame()) {
      // We unsuppressed the displayport, trigger a paint
      aShell->GetRootFrame()->SchedulePaint();
    }
  }

  MOZ_ASSERT(sActiveSuppressDisplayport >= 0);
}

void
APZCCallbackHelper::RespectDisplayPortSuppression(bool aEnabled,
                                                  const nsCOMPtr<nsIPresShell>& aShell)
{
  bool isSuppressed = IsDisplayportSuppressed();
  sDisplayPortSuppressionRespected = aEnabled;
  if (isSuppressed && !IsDisplayportSuppressed() &&
      aShell && aShell->GetRootFrame()) {
    // We unsuppressed the displayport, trigger a paint
    aShell->GetRootFrame()->SchedulePaint();
  }
}

bool
APZCCallbackHelper::IsDisplayportSuppressed()
{
  return sDisplayPortSuppressionRespected
      && sActiveSuppressDisplayport > 0;
}

/* static */ bool
APZCCallbackHelper::IsScrollInProgress(nsIScrollableFrame* aFrame)
{
  return aFrame->IsProcessingAsyncScroll()
         || nsLayoutUtils::CanScrollOriginClobberApz(aFrame->LastScrollOrigin())
         || aFrame->LastSmoothScrollOrigin();
}

/* static */ void
APZCCallbackHelper::NotifyAsyncScrollbarDragRejected(const FrameMetrics::ViewID& aScrollId)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (nsIScrollableFrame* scrollFrame = nsLayoutUtils::FindScrollableFrameFor(aScrollId)) {
    scrollFrame->AsyncScrollbarDragRejected();
  }
}

/* static */ void
APZCCallbackHelper::NotifyPinchGesture(PinchGestureInput::PinchGestureType aType,
                                       LayoutDeviceCoord aSpanChange,
                                       Modifiers aModifiers,
                                       nsIWidget* aWidget)
{
  EventMessage msg;
  switch (aType) {
    case PinchGestureInput::PINCHGESTURE_START:
      msg = eMagnifyGestureStart;
      break;
    case PinchGestureInput::PINCHGESTURE_SCALE:
      msg = eMagnifyGestureUpdate;
      break;
    case PinchGestureInput::PINCHGESTURE_END:
      msg = eMagnifyGesture;
      break;
    case PinchGestureInput::PINCHGESTURE_SENTINEL:
    default:
      MOZ_ASSERT_UNREACHABLE("Invalid gesture type");
      return;
  }

  WidgetSimpleGestureEvent event(true, msg, aWidget);
  event.mDelta = aSpanChange;
  event.mModifiers = aModifiers;
  DispatchWidgetEvent(event);
}

} // namespace layers
} // namespace mozilla

