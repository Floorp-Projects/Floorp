/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "APZCCallbackHelper.h"

#include "TouchActionHelper.h"
#include "gfxPlatform.h"  // For gfxPlatform::UseTiling

#include "mozilla/EventForwards.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/MouseEventBinding.h"
#include "mozilla/dom/BrowserParent.h"
#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/layers/RepaintRequest.h"
#include "mozilla/layers/WebRenderLayerManager.h"
#include "mozilla/layers/WebRenderBridgeChild.h"
#include "mozilla/DisplayPortUtils.h"
#include "mozilla/PresShell.h"
#include "mozilla/TouchEvents.h"
#include "mozilla/ToString.h"
#include "mozilla/ViewportUtils.h"
#include "nsContainerFrame.h"
#include "nsContentUtils.h"
#include "nsIContent.h"
#include "nsIDOMWindowUtils.h"
#include "mozilla/dom/Document.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIScrollableFrame.h"
#include "nsLayoutUtils.h"
#include "nsPrintfCString.h"
#include "nsRefreshDriver.h"
#include "nsString.h"
#include "nsView.h"
#include "Layers.h"

static mozilla::LazyLogModule sApzHlpLog("apz.helper");
#define APZCCH_LOG(...) MOZ_LOG(sApzHlpLog, LogLevel::Debug, (__VA_ARGS__))
static mozilla::LazyLogModule sDisplayportLog("apz.displayport");

namespace mozilla {
namespace layers {

using dom::BrowserParent;

uint64_t APZCCallbackHelper::sLastTargetAPZCNotificationInputBlock =
    uint64_t(-1);

static ScreenMargin RecenterDisplayPort(const ScreenMargin& aDisplayPort) {
  ScreenMargin margins = aDisplayPort;
  margins.right = margins.left = margins.LeftRight() / 2;
  margins.top = margins.bottom = margins.TopBottom() / 2;
  return margins;
}

static PresShell* GetPresShell(const nsIContent* aContent) {
  if (dom::Document* doc = aContent->GetComposedDoc()) {
    return doc->GetPresShell();
  }
  return nullptr;
}

static CSSPoint ScrollFrameTo(nsIScrollableFrame* aFrame,
                              const RepaintRequest& aRequest,
                              bool& aSuccessOut) {
  aSuccessOut = false;
  CSSPoint targetScrollPosition = aRequest.GetLayoutScrollOffset();

  if (!aFrame) {
    return targetScrollPosition;
  }

  CSSPoint geckoScrollPosition =
      CSSPoint::FromAppUnits(aFrame->GetScrollPosition());

  // If the repaint request was triggered due to a previous main-thread scroll
  // offset update sent to the APZ, then we don't need to do another scroll here
  // and we can just return.
  if (!aRequest.GetScrollOffsetUpdated()) {
    return geckoScrollPosition;
  }

  // If this frame is overflow:hidden, then the expectation is that it was
  // sized in a way that respects its scrollable boundaries. For the root
  // frame, this means that it cannot be scrolled in such a way that it moves
  // the layout viewport. For a non-root frame, this means that it cannot be
  // scrolled at all.
  //
  // In either case, |targetScrollPosition| should be the same as
  // |geckoScrollPosition| here.
  //
  // However, this is slightly racy. We query the overflow property of the
  // scroll frame at the time the repaint request arrives at the main thread
  // (i.e., right now), but APZ made the decision of whether or not to allow
  // scrolling based on the information it had at the time it processed the
  // scroll event. The overflow property could have changed at some time
  // between the two events and so APZ may have computed a scrollable region
  // that is larger than what is actually allowed.
  //
  // Currently, we allow the scroll position to change even though the frame is
  // overflow:hidden (that is, we take |targetScrollPosition|). If this turns
  // out to be problematic, an alternative solution would be to ignore the
  // scroll position change (that is, use |geckoScrollPosition|).
  if (aFrame->GetScrollStyles().mVertical == StyleOverflow::Hidden &&
      targetScrollPosition.y != geckoScrollPosition.y) {
    NS_WARNING(
        nsPrintfCString(
            "APZCCH: targetScrollPosition.y (%f) != geckoScrollPosition.y (%f)",
            targetScrollPosition.y, geckoScrollPosition.y)
            .get());
  }
  if (aFrame->GetScrollStyles().mHorizontal == StyleOverflow::Hidden &&
      targetScrollPosition.x != geckoScrollPosition.x) {
    NS_WARNING(
        nsPrintfCString(
            "APZCCH: targetScrollPosition.x (%f) != geckoScrollPosition.x (%f)",
            targetScrollPosition.x, geckoScrollPosition.x)
            .get());
  }

  // If the scrollable frame is currently in the middle of an async or smooth
  // scroll then we don't want to interrupt it (see bug 961280).
  // Also if the scrollable frame got a scroll request from a higher priority
  // origin since the last layers update, then we don't want to push our scroll
  // request because we'll clobber that one, which is bad.
  bool scrollInProgress = APZCCallbackHelper::IsScrollInProgress(aFrame);
  if (!scrollInProgress) {
    aFrame->ScrollToCSSPixelsForApz(targetScrollPosition);
    geckoScrollPosition = CSSPoint::FromAppUnits(aFrame->GetScrollPosition());
    aSuccessOut = true;
  }
  // Return the final scroll position after setting it so that anything that
  // relies on it can have an accurate value. Note that even if we set it above
  // re-querying it is a good idea because it may have gotten clamped or
  // rounded.
  return geckoScrollPosition;
}

/**
 * Scroll the scroll frame associated with |aContent| to the scroll position
 * requested in |aRequest|.
 *
 * Any difference between the requested and actual scroll positions is used to
 * update the callback-transform stored on the content, and return a new
 * display port.
 */
static DisplayPortMargins ScrollFrame(nsIContent* aContent,
                                      const RepaintRequest& aRequest) {
  // Scroll the window to the desired spot
  nsIScrollableFrame* sf =
      nsLayoutUtils::FindScrollableFrameFor(aRequest.GetScrollId());
  if (sf) {
    sf->ResetScrollInfoIfNeeded(aRequest.GetScrollGeneration(),
                                aRequest.GetScrollAnimationType());
    sf->SetScrollableByAPZ(!aRequest.IsScrollInfoLayer());
    if (sf->IsRootScrollFrameOfDocument()) {
      if (!APZCCallbackHelper::IsScrollInProgress(sf)) {
        APZCCH_LOG("Setting VV offset to %s\n",
                   ToString(aRequest.GetVisualScrollOffset()).c_str());
        if (sf->SetVisualViewportOffset(
                CSSPoint::ToAppUnits(aRequest.GetVisualScrollOffset()),
                /* aRepaint = */ false)) {
          // sf can't be destroyed if SetVisualViewportOffset returned true.
          sf->MarkEverScrolled();
        }
      }
    }
  }
  // sf might have been destroyed by the call to SetVisualViewportOffset, so
  // re-get it.
  sf = nsLayoutUtils::FindScrollableFrameFor(aRequest.GetScrollId());
  bool scrollUpdated = false;
  auto displayPortMargins = DisplayPortMargins::ForScrollFrame(
      sf, aRequest.GetDisplayPortMargins(),
      Some(aRequest.DisplayportPixelsPerCSSPixel()));
  CSSPoint apzScrollOffset = aRequest.GetVisualScrollOffset();
  CSSPoint actualScrollOffset = ScrollFrameTo(sf, aRequest, scrollUpdated);
  CSSPoint scrollDelta = apzScrollOffset - actualScrollOffset;

  if (scrollUpdated) {
    if (aRequest.IsScrollInfoLayer()) {
      // In cases where the APZ scroll offset is different from the content
      // scroll offset, we want to interpret the margins as relative to the APZ
      // scroll offset except when the frame is not scrollable by APZ.
      // Therefore, if the layer is a scroll info layer, we leave the margins
      // as-is and they will be interpreted as relative to the content scroll
      // offset.
      if (nsIFrame* frame = aContent->GetPrimaryFrame()) {
        frame->SchedulePaint();
      }
    } else {
      // Correct the display port due to the difference between the requested
      // and actual scroll offsets.
      displayPortMargins = DisplayPortMargins::FromAPZ(
          aRequest.GetDisplayPortMargins(), apzScrollOffset, actualScrollOffset,
          aRequest.DisplayportPixelsPerCSSPixel());
    }
  } else if (aRequest.IsRootContent() &&
             apzScrollOffset != aRequest.GetLayoutScrollOffset()) {
    // APZ uses the visual viewport's offset to calculate where to place the
    // display port, so the display port is misplaced when a pinch zoom occurs.
    //
    // We need to force a display port adjustment in the following paint to
    // account for a difference between the requested and actual scroll
    // offsets in repaints requested by
    // AsyncPanZoomController::NotifyLayersUpdated.
    displayPortMargins = DisplayPortMargins::FromAPZ(
        aRequest.GetDisplayPortMargins(), apzScrollOffset, actualScrollOffset,
        aRequest.DisplayportPixelsPerCSSPixel());
  } else {
    // For whatever reason we couldn't update the scroll offset on the scroll
    // frame, which means the data APZ used for its displayport calculation is
    // stale. Fall back to a sane default behaviour. Note that we don't
    // tile-align the recentered displayport because tile-alignment depends on
    // the scroll position, and the scroll position here is out of our control.
    // See bug 966507 comment 21 for a more detailed explanation.
    displayPortMargins = DisplayPortMargins::ForScrollFrame(
        sf, RecenterDisplayPort(aRequest.GetDisplayPortMargins()),
        Some(aRequest.DisplayportPixelsPerCSSPixel()));
  }

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
      sf && sf->CurrentScrollGeneration() != aRequest.GetScrollGeneration() &&
      nsLayoutUtils::CanScrollOriginClobberApz(sf->LastScrollOrigin());
  if (aContent && !mainThreadScrollChanged) {
    aContent->SetProperty(nsGkAtoms::apzCallbackTransform,
                          new CSSPoint(scrollDelta),
                          nsINode::DeleteProperty<CSSPoint>);
  }

  return displayPortMargins;
}

static void SetDisplayPortMargins(PresShell* aPresShell, nsIContent* aContent,
                                  const DisplayPortMargins& aDisplayPortMargins,
                                  CSSSize aDisplayPortBase) {
  if (!aContent) {
    return;
  }

  bool hadDisplayPort = DisplayPortUtils::HasDisplayPort(aContent);
  if (MOZ_LOG_TEST(sDisplayportLog, LogLevel::Debug)) {
    if (!hadDisplayPort) {
      mozilla::layers::ScrollableLayerGuid::ViewID viewID =
          mozilla::layers::ScrollableLayerGuid::NULL_SCROLL_ID;
      nsLayoutUtils::FindIDFor(aContent, &viewID);
      MOZ_LOG(
          sDisplayportLog, LogLevel::Debug,
          ("APZCCH installing displayport margins %s on scrollId=%" PRIu64 "\n",
           ToString(aDisplayPortMargins).c_str(), viewID));
    }
  }
  DisplayPortUtils::SetDisplayPortMargins(
      aContent, aPresShell, aDisplayPortMargins,
      hadDisplayPort ? DisplayPortUtils::ClearMinimalDisplayPortProperty::No
                     : DisplayPortUtils::ClearMinimalDisplayPortProperty::Yes,
      0);
  if (!hadDisplayPort) {
    DisplayPortUtils::SetZeroMarginDisplayPortOnAsyncScrollableAncestors(
        aContent->GetPrimaryFrame());
  }

  nsRect base(0, 0, aDisplayPortBase.width * AppUnitsPerCSSPixel(),
              aDisplayPortBase.height * AppUnitsPerCSSPixel());
  DisplayPortUtils::SetDisplayPortBaseIfNotSet(aContent, base);
}

static void SetPaintRequestTime(nsIContent* aContent,
                                const TimeStamp& aPaintRequestTime) {
  aContent->SetProperty(nsGkAtoms::paintRequestTime,
                        new TimeStamp(aPaintRequestTime),
                        nsINode::DeleteProperty<TimeStamp>);
}

void APZCCallbackHelper::NotifyLayerTransforms(
    const nsTArray<MatrixMessage>& aTransforms) {
  MOZ_ASSERT(NS_IsMainThread());
  for (const MatrixMessage& msg : aTransforms) {
    BrowserParent* parent =
        BrowserParent::GetBrowserParentFromLayersId(msg.GetLayersId());
    if (parent) {
      parent->SetChildToParentConversionMatrix(
          ViewAs<LayoutDeviceToLayoutDeviceMatrix4x4>(
              msg.GetMatrix(),
              PixelCastJustification::ContentProcessIsLayerInUiProcess),
          msg.GetTopLevelViewportVisibleRectInBrowserCoords());
    }
  }
}

void APZCCallbackHelper::UpdateRootFrame(const RepaintRequest& aRequest) {
  if (aRequest.GetScrollId() == ScrollableLayerGuid::NULL_SCROLL_ID) {
    return;
  }
  RefPtr<nsIContent> content =
      nsLayoutUtils::FindContentFor(aRequest.GetScrollId());
  if (!content) {
    return;
  }

  RefPtr<PresShell> presShell = GetPresShell(content);
  if (!presShell || aRequest.GetPresShellId() != presShell->GetPresShellId()) {
    return;
  }

  APZCCH_LOG("Handling request %s\n", ToString(aRequest).c_str());
  if (nsLayoutUtils::AllowZoomingForDocument(presShell->GetDocument()) &&
      aRequest.GetAsyncZoom().scale != 1.0) {
    // If zooming is disabled then we don't really want to let APZ fiddle
    // with these things. In theory setting the resolution here should be a
    // no-op, but setting the visual viewport size is bad because it can cause a
    // stale value to be returned by window.innerWidth/innerHeight (see bug
    // 1187792).

    float presShellResolution = presShell->GetResolution();

    // If the pres shell resolution has changed on the content side side
    // the time this repaint request was fired, consider this request out of
    // date and drop it; setting a zoom based on the out-of-date resolution can
    // have the effect of getting us stuck with the stale resolution.
    // One might think that if the last ResolutionChangeOrigin was apz then the
    // pres shell resolutions should match but
    // that is not the case. We can get multiple repaint requests that has the
    // same pres shell resolution (because apz didn't receive a content layers
    // update inbetween) if the first has async zoom we apply that and chance
    // the content pres shell resolution and thus when handling the second
    // repaint request the pres shell resolution won't match. So that's why we
    // also check if the last resolution change origin was apz (aka 'us').
    if (!FuzzyEqualsMultiplicative(presShellResolution,
                                   aRequest.GetPresShellResolution()) &&
        presShell->GetLastResolutionChangeOrigin() !=
            ResolutionChangeOrigin::Apz) {
      return;
    }

    // The pres shell resolution is updated by the the async zoom since the
    // last paint.
    // We want to calculate the new presshell resolution as
    // |aRequest.GetPresShellResolution() * aRequest.GetAsyncZoom()| but that
    // calculation can lead to small inaccuracies due to limited floating point
    // precision. Specifically,
    // clang-format off
    // asyncZoom = zoom / layerPixelsPerCSSPixel
    //           = zoom / (devPixelsPerCSSPixel * cumulativeResolution)
    // clang-format on
    // Since this is a root frame we generally do not allow css transforms to
    // scale it, so it is very likely that cumulativeResolution ==
    // presShellResoluion. So
    // clang-format off
    // newPresShellResoluion = presShellResoluion * asyncZoom
    // = presShellResoluion * zoom / (devPixelsPerCSSPixel * presShellResoluion)
    // = zoom / devPixelsPerCSSPixel
    // clang-format on
    // However, we want to keep the calculation general and so we do not assume
    // presShellResoluion == cumulativeResolution, but rather factor those
    // values out so they cancel and the floating point division has a very high
    // probability of being exactly 1.
    presShellResolution =
        (aRequest.GetPresShellResolution() /
         aRequest.GetCumulativeResolution().scale) *
        (aRequest.GetZoom() / aRequest.GetDevPixelsPerCSSPixel()).scale;
    presShell->SetResolutionAndScaleTo(presShellResolution,
                                       ResolutionChangeOrigin::Apz);

    // Changing the resolution will trigger a reflow which will cause the
    // main-thread scroll position to be realigned in layer pixels. This
    // (subpixel) scroll mutation can trigger a scroll update to APZ which
    // is undesirable. Instead of having that happen as part of the post-reflow
    // code, we force it to happen here with ScrollOrigin::Apz so that it
    // doesn't trigger a scroll update to APZ.
    nsIScrollableFrame* sf =
        nsLayoutUtils::FindScrollableFrameFor(aRequest.GetScrollId());
    CSSPoint currentScrollPosition =
        CSSPoint::FromAppUnits(sf->GetScrollPosition());
    sf->ScrollToCSSPixelsForApz(currentScrollPosition);
  }

  // Do this as late as possible since scrolling can flush layout. It also
  // adjusts the display port margins, so do it before we set those.
  DisplayPortMargins displayPortMargins = ScrollFrame(content, aRequest);

  SetDisplayPortMargins(presShell, content, displayPortMargins,
                        aRequest.CalculateCompositedSizeInCssPixels());
  SetPaintRequestTime(content, aRequest.GetPaintRequestTime());
}

void APZCCallbackHelper::UpdateSubFrame(const RepaintRequest& aRequest) {
  if (aRequest.GetScrollId() == ScrollableLayerGuid::NULL_SCROLL_ID) {
    return;
  }
  RefPtr<nsIContent> content =
      nsLayoutUtils::FindContentFor(aRequest.GetScrollId());
  if (!content) {
    return;
  }

  // We don't currently support zooming for subframes, so nothing extra
  // needs to be done beyond the tasks common to this and UpdateRootFrame.
  DisplayPortMargins displayPortMargins = ScrollFrame(content, aRequest);
  if (RefPtr<PresShell> presShell = GetPresShell(content)) {
    SetDisplayPortMargins(presShell, content, displayPortMargins,
                          aRequest.CalculateCompositedSizeInCssPixels());
  }
  SetPaintRequestTime(content, aRequest.GetPaintRequestTime());
}

bool APZCCallbackHelper::GetOrCreateScrollIdentifiers(
    nsIContent* aContent, uint32_t* aPresShellIdOut,
    ScrollableLayerGuid::ViewID* aViewIdOut) {
  if (!aContent) {
    return false;
  }
  *aViewIdOut = nsLayoutUtils::FindOrCreateIDFor(aContent);
  if (PresShell* presShell = GetPresShell(aContent)) {
    *aPresShellIdOut = presShell->GetPresShellId();
    return true;
  }
  return false;
}

void APZCCallbackHelper::InitializeRootDisplayport(PresShell* aPresShell) {
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
  ScrollableLayerGuid::ViewID viewId;
  if (APZCCallbackHelper::GetOrCreateScrollIdentifiers(content, &presShellId,
                                                       &viewId)) {
    MOZ_LOG(
        sDisplayportLog, LogLevel::Debug,
        ("Initializing root displayport on scrollId=%" PRIu64 "\n", viewId));
    Maybe<nsRect> baseRect =
        DisplayPortUtils::GetRootDisplayportBase(aPresShell);
    if (baseRect) {
      DisplayPortUtils::SetDisplayPortBaseIfNotSet(content, *baseRect);
    }

    // Note that we also set the base rect that goes with these margins in
    // nsRootBoxFrame::BuildDisplayList.
    DisplayPortUtils::SetDisplayPortMargins(
        content, aPresShell, DisplayPortMargins::Empty(content),
        DisplayPortUtils::ClearMinimalDisplayPortProperty::Yes, 0);
    DisplayPortUtils::SetZeroMarginDisplayPortOnAsyncScrollableAncestors(
        content->GetPrimaryFrame());
  }
}

nsPresContext* APZCCallbackHelper::GetPresContextForContent(
    nsIContent* aContent) {
  dom::Document* doc = aContent->GetComposedDoc();
  if (!doc) {
    return nullptr;
  }
  PresShell* presShell = doc->GetPresShell();
  if (!presShell) {
    return nullptr;
  }
  return presShell->GetPresContext();
}

PresShell* APZCCallbackHelper::GetRootContentDocumentPresShellForContent(
    nsIContent* aContent) {
  nsPresContext* context = GetPresContextForContent(aContent);
  if (!context) {
    return nullptr;
  }
  context = context->GetInProcessRootContentDocumentPresContext();
  if (!context) {
    return nullptr;
  }
  return context->PresShell();
}

nsEventStatus APZCCallbackHelper::DispatchWidgetEvent(WidgetGUIEvent& aEvent) {
  nsEventStatus status = nsEventStatus_eConsumeNoDefault;
  if (aEvent.mWidget) {
    aEvent.mWidget->DispatchEvent(&aEvent, status);
  }
  return status;
}

nsEventStatus APZCCallbackHelper::DispatchSynthesizedMouseEvent(
    EventMessage aMsg, uint64_t aTime, const LayoutDevicePoint& aRefPoint,
    Modifiers aModifiers, int32_t aClickCount, nsIWidget* aWidget) {
  MOZ_ASSERT(aMsg == eMouseMove || aMsg == eMouseDown || aMsg == eMouseUp ||
             aMsg == eMouseLongTap);

  WidgetMouseEvent event(true, aMsg, aWidget, WidgetMouseEvent::eReal,
                         WidgetMouseEvent::eNormal);
  event.mRefPoint = LayoutDeviceIntPoint::Truncate(aRefPoint.x, aRefPoint.y);
  event.mTime = aTime;
  event.mButton = MouseButton::ePrimary;
  event.mButtons |= MouseButtonsFlag::ePrimaryFlag;
  event.mInputSource = dom::MouseEvent_Binding::MOZ_SOURCE_TOUCH;
  if (aMsg == eMouseLongTap) {
    event.mFlags.mOnlyChromeDispatch = true;
  }
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

PreventDefaultResult APZCCallbackHelper::DispatchMouseEvent(
    PresShell* aPresShell, const nsString& aType, const CSSPoint& aPoint,
    int32_t aButton, int32_t aClickCount, int32_t aModifiers,
    unsigned short aInputSourceArg, uint32_t aPointerId) {
  NS_ENSURE_TRUE(aPresShell, PreventDefaultResult::ByContent);

  PreventDefaultResult preventDefaultResult;
  nsContentUtils::SendMouseEvent(
      aPresShell, aType, aPoint.x, aPoint.y, aButton,
      nsIDOMWindowUtils::MOUSE_BUTTONS_NOT_SPECIFIED, aClickCount, aModifiers,
      /* aIgnoreRootScrollFrame = */ false, 0, aInputSourceArg, aPointerId,
      false, &preventDefaultResult, false,
      /* aIsWidgetEventSynthesized = */ false);
  return preventDefaultResult;
}

void APZCCallbackHelper::FireSingleTapEvent(const LayoutDevicePoint& aPoint,
                                            Modifiers aModifiers,
                                            int32_t aClickCount,
                                            nsIWidget* aWidget) {
  if (aWidget->Destroyed()) {
    return;
  }
  APZCCH_LOG("Dispatching single-tap component events to %s\n",
             ToString(aPoint).c_str());
  int time = 0;
  DispatchSynthesizedMouseEvent(eMouseMove, time, aPoint, aModifiers,
                                aClickCount, aWidget);
  DispatchSynthesizedMouseEvent(eMouseDown, time, aPoint, aModifiers,
                                aClickCount, aWidget);
  DispatchSynthesizedMouseEvent(eMouseUp, time, aPoint, aModifiers, aClickCount,
                                aWidget);
}

static dom::Element* GetDisplayportElementFor(
    nsIScrollableFrame* aScrollableFrame) {
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
  MOZ_ASSERT(content->IsElement());  // roc says this must be true
  return content->AsElement();
}

static dom::Element* GetRootDocumentElementFor(nsIWidget* aWidget) {
  // This returns the root element that ChromeProcessController sets the
  // displayport on during initialization.
  if (nsView* view = nsView::GetViewFor(aWidget)) {
    if (PresShell* presShell = view->GetPresShell()) {
      MOZ_ASSERT(presShell->GetDocument());
      return presShell->GetDocument()->GetDocumentElement();
    }
  }
  return nullptr;
}

namespace {

using FrameForPointOption = nsLayoutUtils::FrameForPointOption;

// Determine the scrollable target frame for the given point and add it to
// the target list. If the frame doesn't have a displayport, set one.
// Return whether or not the frame had a displayport that has already been
// painted (in this case, the caller can send the SetTargetAPZC notification
// right away, rather than waiting for a transaction to propagate the
// displayport to APZ first).
static bool PrepareForSetTargetAPZCNotification(
    nsIWidget* aWidget, const LayersId& aLayersId, nsIFrame* aRootFrame,
    const LayoutDeviceIntPoint& aRefPoint,
    nsTArray<ScrollableLayerGuid>* aTargets) {
  ScrollableLayerGuid guid(aLayersId, 0, ScrollableLayerGuid::NULL_SCROLL_ID);
  RelativeTo relativeTo{aRootFrame, ViewportType::Visual};
  nsPoint point = nsLayoutUtils::GetEventCoordinatesRelativeTo(
      aWidget, aRefPoint, relativeTo);
  nsIFrame* target = nsLayoutUtils::GetFrameForPoint(relativeTo, point);
  nsIScrollableFrame* scrollAncestor =
      target ? nsLayoutUtils::GetAsyncScrollableAncestorFrame(target)
             : aRootFrame->PresShell()->GetRootScrollFrameAsScrollable();

  // Assuming that if there's no scrollAncestor, there's already a displayPort.
  nsCOMPtr<dom::Element> dpElement =
      scrollAncestor ? GetDisplayportElementFor(scrollAncestor)
                     : GetRootDocumentElementFor(aWidget);

  if (MOZ_LOG_TEST(sApzHlpLog, LogLevel::Debug)) {
    nsAutoString dpElementDesc;
    if (dpElement) {
      dpElement->Describe(dpElementDesc);
    }
    APZCCH_LOG("For event at %s found scrollable element %p (%s)\n",
               ToString(aRefPoint).c_str(), dpElement.get(),
               NS_LossyConvertUTF16toASCII(dpElementDesc).get());
  }

  bool guidIsValid = APZCCallbackHelper::GetOrCreateScrollIdentifiers(
      dpElement, &(guid.mPresShellId), &(guid.mScrollId));
  aTargets->AppendElement(guid);

  if (!guidIsValid) {
    return false;
  }
  if (DisplayPortUtils::HasNonMinimalNonZeroDisplayPort(dpElement)) {
    // If the element has a displayport but it hasn't been painted yet,
    // we want the caller to wait for the paint to happen, but we don't
    // need to set the displayport here since it's already been set.
    return !DisplayPortUtils::HasPaintedDisplayPort(dpElement);
  }

  if (!scrollAncestor) {
    // This can happen if the document element gets swapped out after
    // ChromeProcessController runs InitializeRootDisplayport. In this case
    // let's try to set a displayport again and bail out on this operation.
    APZCCH_LOG("Widget %p's document element %p didn't have a displayport\n",
               aWidget, dpElement.get());
    APZCCallbackHelper::InitializeRootDisplayport(aRootFrame->PresShell());
    return false;
  }

  APZCCH_LOG("%p didn't have a displayport, so setting one...\n",
             dpElement.get());
  MOZ_LOG(sDisplayportLog, LogLevel::Debug,
          ("Activating displayport on scrollId=%" PRIu64 " for SetTargetAPZC\n",
           guid.mScrollId));
  bool activated = DisplayPortUtils::CalculateAndSetDisplayPortMargins(
      scrollAncestor, DisplayPortUtils::RepaintMode::Repaint);
  if (!activated) {
    return false;
  }

  nsIFrame* frame = do_QueryFrame(scrollAncestor);
  DisplayPortUtils::SetZeroMarginDisplayPortOnAsyncScrollableAncestors(frame);

  return !DisplayPortUtils::HasPaintedDisplayPort(dpElement);
}

static void SendLayersDependentApzcTargetConfirmation(
    nsPresContext* aPresContext, uint64_t aInputBlockId,
    nsTArray<ScrollableLayerGuid>&& aTargets) {
  PresShell* ps = aPresContext->GetPresShell();
  if (!ps) {
    return;
  }

  WindowRenderer* renderer = ps->GetWindowRenderer();
  if (!renderer) {
    return;
  }

  if (WebRenderLayerManager* wrlm = renderer->AsWebRender()) {
    if (WebRenderBridgeChild* wrbc = wrlm->WrBridge()) {
      wrbc->SendSetConfirmedTargetAPZC(aInputBlockId, aTargets);
    }
    return;
  }
}

}  // namespace

DisplayportSetListener::DisplayportSetListener(
    nsIWidget* aWidget, nsPresContext* aPresContext,
    const uint64_t& aInputBlockId, nsTArray<ScrollableLayerGuid>&& aTargets)
    : ManagedPostRefreshObserver(aPresContext),
      mWidget(aWidget),
      mInputBlockId(aInputBlockId),
      mTargets(std::move(aTargets)) {
  MOZ_ASSERT(!mAction, "Setting Action twice");
  mAction = [instance = MOZ_KnownLive(this)](bool aWasCanceled) {
    instance->OnPostRefresh();
    return Unregister::Yes;
  };
}

DisplayportSetListener::~DisplayportSetListener() = default;

void DisplayportSetListener::Register() {
  APZCCH_LOG("DisplayportSetListener::Register\n");
  mPresContext->RegisterManagedPostRefreshObserver(this);
}

void DisplayportSetListener::OnPostRefresh() {
  APZCCH_LOG("Got refresh, sending target APZCs for input block %" PRIu64 "\n",
             mInputBlockId);
  SendLayersDependentApzcTargetConfirmation(mPresContext, mInputBlockId,
                                            std::move(mTargets));
}

already_AddRefed<DisplayportSetListener>
APZCCallbackHelper::SendSetTargetAPZCNotification(nsIWidget* aWidget,
                                                  dom::Document* aDocument,
                                                  const WidgetGUIEvent& aEvent,
                                                  const LayersId& aLayersId,
                                                  uint64_t aInputBlockId) {
  if (!aWidget || !aDocument) {
    return nullptr;
  }
  if (aInputBlockId == sLastTargetAPZCNotificationInputBlock) {
    // We have already confirmed the target APZC for a previous event of this
    // input block. If we activated a scroll frame for this input block,
    // sending another target APZC confirmation would be harmful, as it might
    // race the original confirmation (which needs to go through a layers
    // transaction).
    APZCCH_LOG("Not resending target APZC confirmation for input block %" PRIu64
               "\n",
               aInputBlockId);
    return nullptr;
  }
  sLastTargetAPZCNotificationInputBlock = aInputBlockId;
  if (PresShell* presShell = aDocument->GetPresShell()) {
    if (nsIFrame* rootFrame = presShell->GetRootFrame()) {
      bool waitForRefresh = false;
      nsTArray<ScrollableLayerGuid> targets;

      if (const WidgetTouchEvent* touchEvent = aEvent.AsTouchEvent()) {
        for (size_t i = 0; i < touchEvent->mTouches.Length(); i++) {
          waitForRefresh |= PrepareForSetTargetAPZCNotification(
              aWidget, aLayersId, rootFrame, touchEvent->mTouches[i]->mRefPoint,
              &targets);
        }
      } else if (const WidgetWheelEvent* wheelEvent = aEvent.AsWheelEvent()) {
        waitForRefresh = PrepareForSetTargetAPZCNotification(
            aWidget, aLayersId, rootFrame, wheelEvent->mRefPoint, &targets);
      } else if (const WidgetMouseEvent* mouseEvent = aEvent.AsMouseEvent()) {
        waitForRefresh = PrepareForSetTargetAPZCNotification(
            aWidget, aLayersId, rootFrame, mouseEvent->mRefPoint, &targets);
      }
      // TODO: Do other types of events need to be handled?

      if (!targets.IsEmpty()) {
        if (waitForRefresh) {
          APZCCH_LOG(
              "At least one target got a new displayport, need to wait for "
              "refresh\n");
          return MakeAndAddRef<DisplayportSetListener>(
              aWidget, presShell->GetPresContext(), aInputBlockId,
              std::move(targets));
        }
        APZCCH_LOG("Sending target APZCs for input block %" PRIu64 "\n",
                   aInputBlockId);
        aWidget->SetConfirmedTargetAPZC(aInputBlockId, targets);
      }
    }
  }
  return nullptr;
}

nsTArray<TouchBehaviorFlags>
APZCCallbackHelper::SendSetAllowedTouchBehaviorNotification(
    nsIWidget* aWidget, dom::Document* aDocument,
    const WidgetTouchEvent& aEvent, uint64_t aInputBlockId,
    const SetAllowedTouchBehaviorCallback& aCallback) {
  nsTArray<TouchBehaviorFlags> flags;
  if (!aWidget || !aDocument) {
    return flags;
  }
  if (PresShell* presShell = aDocument->GetPresShell()) {
    if (nsIFrame* rootFrame = presShell->GetRootFrame()) {
      for (uint32_t i = 0; i < aEvent.mTouches.Length(); i++) {
        flags.AppendElement(TouchActionHelper::GetAllowedTouchBehavior(
            aWidget, RelativeTo{rootFrame, ViewportType::Visual},
            aEvent.mTouches[i]->mRefPoint));
      }
      aCallback(aInputBlockId, flags);
    }
  }
  return flags;
}

void APZCCallbackHelper::NotifyMozMouseScrollEvent(
    const ScrollableLayerGuid::ViewID& aScrollId, const nsString& aEvent) {
  nsCOMPtr<nsIContent> targetContent = nsLayoutUtils::FindContentFor(aScrollId);
  if (!targetContent) {
    return;
  }
  RefPtr<dom::Document> ownerDoc = targetContent->OwnerDoc();
  if (!ownerDoc) {
    return;
  }

  nsContentUtils::DispatchEventOnlyToChrome(ownerDoc, targetContent, aEvent,
                                            CanBubble::eYes, Cancelable::eYes);
}

void APZCCallbackHelper::NotifyFlushComplete(PresShell* aPresShell) {
  MOZ_ASSERT(NS_IsMainThread());
  // In some cases, flushing the APZ state to the main thread doesn't actually
  // trigger a flush and repaint (this is an intentional optimization - the
  // stuff visible to the user is still correct). However, reftests update their
  // snapshot based on invalidation events that are emitted during paints,
  // so we ensure that we kick off a paint when an APZ flush is done. Note that
  // only chrome/testing code can trigger this behaviour.
  if (aPresShell && aPresShell->GetRootFrame()) {
    aPresShell->GetRootFrame()->SchedulePaint(nsIFrame::PAINT_DEFAULT, false);
  }

  nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
  MOZ_ASSERT(observerService);
  observerService->NotifyObservers(nullptr, "apz-repaints-flushed", nullptr);
}

/* static */
bool APZCCallbackHelper::IsScrollInProgress(nsIScrollableFrame* aFrame) {
  using IncludeApzAnimation = nsIScrollableFrame::IncludeApzAnimation;

  return aFrame->IsScrollAnimating(IncludeApzAnimation::No) ||
         nsLayoutUtils::CanScrollOriginClobberApz(aFrame->LastScrollOrigin());
}

/* static */
void APZCCallbackHelper::NotifyAsyncScrollbarDragInitiated(
    uint64_t aDragBlockId, const ScrollableLayerGuid::ViewID& aScrollId,
    ScrollDirection aDirection) {
  MOZ_ASSERT(NS_IsMainThread());
  if (nsIScrollableFrame* scrollFrame =
          nsLayoutUtils::FindScrollableFrameFor(aScrollId)) {
    scrollFrame->AsyncScrollbarDragInitiated(aDragBlockId, aDirection);
  }
}

/* static */
void APZCCallbackHelper::NotifyAsyncScrollbarDragRejected(
    const ScrollableLayerGuid::ViewID& aScrollId) {
  MOZ_ASSERT(NS_IsMainThread());
  if (nsIScrollableFrame* scrollFrame =
          nsLayoutUtils::FindScrollableFrameFor(aScrollId)) {
    scrollFrame->AsyncScrollbarDragRejected();
  }
}

/* static */
void APZCCallbackHelper::NotifyAsyncAutoscrollRejected(
    const ScrollableLayerGuid::ViewID& aScrollId) {
  MOZ_ASSERT(NS_IsMainThread());
  nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
  MOZ_ASSERT(observerService);

  nsAutoString data;
  data.AppendInt(aScrollId);
  observerService->NotifyObservers(nullptr, "autoscroll-rejected-by-apz",
                                   data.get());
}

/* static */
void APZCCallbackHelper::CancelAutoscroll(
    const ScrollableLayerGuid::ViewID& aScrollId) {
  MOZ_ASSERT(NS_IsMainThread());
  nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
  MOZ_ASSERT(observerService);

  nsAutoString data;
  data.AppendInt(aScrollId);
  observerService->NotifyObservers(nullptr, "apz:cancel-autoscroll",
                                   data.get());
}

/* static */
void APZCCallbackHelper::NotifyPinchGesture(
    PinchGestureInput::PinchGestureType aType,
    const LayoutDevicePoint& aFocusPoint, LayoutDeviceCoord aSpanChange,
    Modifiers aModifiers, const nsCOMPtr<nsIWidget>& aWidget) {
  APZCCH_LOG("APZCCallbackHelper dispatching pinch gesture\n");
  EventMessage msg;
  switch (aType) {
    case PinchGestureInput::PINCHGESTURE_START:
      msg = eMagnifyGestureStart;
      break;
    case PinchGestureInput::PINCHGESTURE_SCALE:
      msg = eMagnifyGestureUpdate;
      break;
    case PinchGestureInput::PINCHGESTURE_FINGERLIFTED:
    case PinchGestureInput::PINCHGESTURE_END:
      msg = eMagnifyGesture;
      break;
  }

  WidgetSimpleGestureEvent event(true, msg, aWidget.get());
  // XXX mDelta for the eMagnifyGesture event is supposed to be the
  // cumulative magnification over the entire gesture (per docs in
  // SimpleGestureEvent.webidl) but currently APZ just sends us a zero
  // aSpanChange for that event, so the mDelta is wrong. Nothing relies
  // on that currently, but we might want to fix it at some point.
  event.mDelta = aSpanChange;
  event.mModifiers = aModifiers;
  event.mRefPoint = RoundedToInt(aFocusPoint);

  DispatchWidgetEvent(event);
}

}  // namespace layers
}  // namespace mozilla
