/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsResizerFrame.h"
#include "nsIContent.h"
#include "mozilla/PresShell.h"
#include "mozilla/dom/Document.h"
#include "nsGkAtoms.h"
#include "nsNameSpaceManager.h"

#include "nsPresContext.h"
#include "nsFrameManager.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIBaseWindow.h"
#include "nsPIDOMWindow.h"
#include "mozilla/MouseEvents.h"
#include "nsContentUtils.h"
#include "nsMenuPopupFrame.h"
#include "nsIScreenManager.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/MouseEventBinding.h"
#include "nsError.h"
#include "nsICSSDeclaration.h"
#include "nsStyledElement.h"
#include <algorithm>

using namespace mozilla;

//
// NS_NewResizerFrame
//
// Creates a new Resizer frame and returns it
//
nsIFrame* NS_NewResizerFrame(PresShell* aPresShell, ComputedStyle* aStyle) {
  return new (aPresShell) nsResizerFrame(aStyle, aPresShell->GetPresContext());
}

NS_IMPL_FRAMEARENA_HELPERS(nsResizerFrame)

nsResizerFrame::nsResizerFrame(ComputedStyle* aStyle,
                               nsPresContext* aPresContext)
    : nsTitleBarFrame(aStyle, aPresContext, kClassID) {}

nsresult nsResizerFrame::HandleEvent(nsPresContext* aPresContext,
                                     WidgetGUIEvent* aEvent,
                                     nsEventStatus* aEventStatus) {
  NS_ENSURE_ARG_POINTER(aEventStatus);
  if (nsEventStatus_eConsumeNoDefault == *aEventStatus) {
    return NS_OK;
  }

  AutoWeakFrame weakFrame(this);
  bool doDefault = true;

  switch (aEvent->mMessage) {
    case eTouchStart:
    case eMouseDown: {
      if (aEvent->mClass == eTouchEventClass ||
          (aEvent->mClass == eMouseEventClass &&
           aEvent->AsMouseEvent()->mButton == MouseButton::eLeft)) {
        nsCOMPtr<nsIBaseWindow> window;
        mozilla::PresShell* presShell = aPresContext->GetPresShell();
        nsIContent* contentToResize =
            GetContentToResize(presShell, getter_AddRefs(window));
        if (contentToResize) {
          nsIFrame* frameToResize = contentToResize->GetPrimaryFrame();
          if (!frameToResize) break;

          // cache the content rectangle for the frame to resize
          // GetScreenRectInAppUnits returns the border box rectangle, so
          // adjust to get the desired content rectangle.
          nsRect rect = frameToResize->GetScreenRectInAppUnits();
          if (frameToResize->StylePosition()->mBoxSizing ==
              StyleBoxSizing::Content) {
            rect.Deflate(frameToResize->GetUsedBorderAndPadding());
          }

          mMouseDownRect = LayoutDeviceIntRect::FromAppUnitsToNearest(
              rect, aPresContext->AppUnitsPerDevPixel());
          doDefault = false;
        } else {
          // If there is no window, then resizing isn't allowed.
          if (!window) break;

          doDefault = false;

          // ask the widget implementation to begin a resize drag if it can
          Direction direction = GetDirection();
          nsresult rv = aEvent->mWidget->BeginResizeDrag(
              aEvent, direction.mHorizontal, direction.mVertical);
          // for native drags, don't set the fields below
          if (rv != NS_ERROR_NOT_IMPLEMENTED) break;

          // if there's no native resize support, we need to do window
          // resizing ourselves
          window->GetPositionAndSize(&mMouseDownRect.x, &mMouseDownRect.y,
                                     &mMouseDownRect.width,
                                     &mMouseDownRect.height);
        }

        // remember current mouse coordinates
        LayoutDeviceIntPoint refPoint;
        if (!GetEventPoint(aEvent, refPoint)) return NS_OK;
        mMouseDownPoint = refPoint + aEvent->mWidget->WidgetToScreenOffset();

        // we're tracking
        mTrackingMouseMove = true;

        PresShell::SetCapturingContent(GetContent(),
                                       CaptureFlags::IgnoreAllowedState);
      }
    } break;

    case eTouchEnd:
    case eMouseUp: {
      if (aEvent->mClass == eTouchEventClass ||
          (aEvent->mClass == eMouseEventClass &&
           aEvent->AsMouseEvent()->mButton == MouseButton::eLeft)) {
        // we're done tracking.
        mTrackingMouseMove = false;

        PresShell::ReleaseCapturingContent();

        doDefault = false;
      }
    } break;

    case eTouchMove:
    case eMouseMove: {
      if (mTrackingMouseMove) {
        nsCOMPtr<nsIBaseWindow> window;
        mozilla::PresShell* presShell = aPresContext->GetPresShell();
        nsCOMPtr<nsIContent> contentToResize =
            GetContentToResize(presShell, getter_AddRefs(window));

        // check if the returned content really is a menupopup
        nsMenuPopupFrame* menuPopupFrame = nullptr;
        if (contentToResize) {
          menuPopupFrame = do_QueryFrame(contentToResize->GetPrimaryFrame());
        }

        // both MouseMove and direction are negative when pointing to the
        // top and left, and positive when pointing to the bottom and right

        // retrieve the offset of the mousemove event relative to the mousedown.
        // The difference is how much the resize needs to be
        LayoutDeviceIntPoint refPoint;
        if (!GetEventPoint(aEvent, refPoint)) return NS_OK;
        LayoutDeviceIntPoint screenPoint =
            refPoint + aEvent->mWidget->WidgetToScreenOffset();
        LayoutDeviceIntPoint mouseMove(screenPoint - mMouseDownPoint);

        // Determine which direction to resize by checking the dir attribute.
        // For windows and menus, ensure that it can be resized in that
        // direction.
        Direction direction = GetDirection();
        if (window || menuPopupFrame) {
          if (menuPopupFrame) {
            menuPopupFrame->CanAdjustEdges(
                (direction.mHorizontal == -1) ? eSideLeft : eSideRight,
                (direction.mVertical == -1) ? eSideTop : eSideBottom,
                mouseMove);
          }
        } else if (!contentToResize) {
          break;  // don't do anything if there's nothing to resize
        }

        LayoutDeviceIntRect rect = mMouseDownRect;

        // Check if there are any size constraints on this window.
        widget::SizeConstraints sizeConstraints;
        if (window) {
          nsCOMPtr<nsIWidget> widget;
          window->GetMainWidget(getter_AddRefs(widget));
          sizeConstraints = widget->GetSizeConstraints();
        }

        AdjustDimensions(&rect.x, &rect.width, sizeConstraints.mMinSize.width,
                         sizeConstraints.mMaxSize.width, mouseMove.x,
                         direction.mHorizontal);
        AdjustDimensions(&rect.y, &rect.height, sizeConstraints.mMinSize.height,
                         sizeConstraints.mMaxSize.height, mouseMove.y,
                         direction.mVertical);

        // Don't allow resizing a window or a popup past the edge of the screen,
        // so adjust the rectangle to fit within the available screen area.
        if (window) {
          nsCOMPtr<nsIScreen> screen;
          nsCOMPtr<nsIScreenManager> sm(
              do_GetService("@mozilla.org/gfx/screenmanager;1"));
          if (sm) {
            CSSIntRect frameRect = GetScreenRect();
            // ScreenForRect requires display pixels, so scale from device pix
            double scale;
            window->GetUnscaledDevicePixelsPerCSSPixel(&scale);
            sm->ScreenForRect(NSToIntRound(frameRect.x / scale),
                              NSToIntRound(frameRect.y / scale), 1, 1,
                              getter_AddRefs(screen));
            if (screen) {
              LayoutDeviceIntRect screenRect;
              screen->GetRect(&screenRect.x, &screenRect.y, &screenRect.width,
                              &screenRect.height);
              rect.IntersectRect(rect, screenRect);
            }
          }
        } else if (menuPopupFrame) {
          nsRect frameRect = menuPopupFrame->GetScreenRectInAppUnits();
          nsIFrame* rootFrame = aPresContext->PresShell()->GetRootFrame();
          nsRect rootScreenRect = rootFrame->GetScreenRectInAppUnits();

          nsPopupLevel popupLevel = menuPopupFrame->PopupLevel();
          int32_t appPerDev = aPresContext->AppUnitsPerDevPixel();
          LayoutDeviceIntRect screenRect = menuPopupFrame->GetConstraintRect(
              LayoutDeviceIntRect::FromAppUnitsToNearest(frameRect, appPerDev),
              // round using ...ToInside as it's better to be a pixel too small
              // than be too large. If the popup is too large it could get
              // flipped to the opposite side of the anchor point while
              // resizing.
              LayoutDeviceIntRect::FromAppUnitsToInside(rootScreenRect,
                                                        appPerDev),
              popupLevel);
          rect.IntersectRect(rect, screenRect);
        }

        if (contentToResize) {
          // convert the rectangle into css pixels. When changing the size in a
          // direction, don't allow the new size to be less that the resizer's
          // size. This ensures that content isn't resized too small as to make
          // the resizer invisible.
          nsRect appUnitsRect = ToAppUnits(rect.ToUnknownRect(),
                                           aPresContext->AppUnitsPerDevPixel());
          if (appUnitsRect.width < mRect.width && mouseMove.x)
            appUnitsRect.width = mRect.width;
          if (appUnitsRect.height < mRect.height && mouseMove.y)
            appUnitsRect.height = mRect.height;
          nsIntRect cssRect =
              appUnitsRect.ToInsidePixels(AppUnitsPerCSSPixel());

          LayoutDeviceIntRect oldRect;
          AutoWeakFrame weakFrame(menuPopupFrame);
          if (menuPopupFrame) {
            nsCOMPtr<nsIWidget> widget = menuPopupFrame->GetWidget();
            if (widget) oldRect = widget->GetScreenBounds();

            // convert the new rectangle into outer window coordinates
            LayoutDeviceIntPoint clientOffset = widget->GetClientOffset();
            rect.x -= clientOffset.x;
            rect.y -= clientOffset.y;
          }

          SizeInfo sizeInfo, originalSizeInfo;
          sizeInfo.width.AppendInt(cssRect.width);
          sizeInfo.height.AppendInt(cssRect.height);
          ResizeContent(contentToResize, direction, sizeInfo,
                        &originalSizeInfo);
          MaybePersistOriginalSize(contentToResize, originalSizeInfo);

          // Move the popup to the new location unless it is anchored, since
          // the position shouldn't change. nsMenuPopupFrame::SetPopupPosition
          // will instead ensure that the popup's position is anchored at the
          // right place.
          if (weakFrame.IsAlive() &&
              (oldRect.x != rect.x || oldRect.y != rect.y) &&
              (!menuPopupFrame->IsAnchored() ||
               menuPopupFrame->PopupLevel() != ePopupLevelParent)) {
            CSSPoint cssPos =
                rect.TopLeft() / aPresContext->CSSToDevPixelScale();
            menuPopupFrame->MoveTo(RoundedToInt(cssPos), true);
          }
        } else {
          window->SetPositionAndSize(
              rect.x, rect.y, rect.width, rect.height,
              nsIBaseWindow::eRepaint);  // do the repaint.
        }

        doDefault = false;
      }
    } break;

    case eMouseClick: {
      WidgetMouseEvent* mouseEvent = aEvent->AsMouseEvent();
      if (mouseEvent->IsLeftClickEvent()) {
        MouseClicked(mouseEvent);
      }
      break;
    }
    case eMouseDoubleClick:
      if (aEvent->AsMouseEvent()->mButton == MouseButton::eLeft) {
        nsCOMPtr<nsIBaseWindow> window;
        mozilla::PresShell* presShell = aPresContext->GetPresShell();
        nsIContent* contentToResize =
            GetContentToResize(presShell, getter_AddRefs(window));
        if (contentToResize) {
          nsMenuPopupFrame* menuPopupFrame =
              do_QueryFrame(contentToResize->GetPrimaryFrame());
          if (menuPopupFrame)
            break;  // Don't restore original sizing for menupopup frames until
                    // we handle screen constraints here. (Bug 357725)

          RestoreOriginalSize(contentToResize);
        }
      }
      break;

    default:
      break;
  }

  if (!doDefault) *aEventStatus = nsEventStatus_eConsumeNoDefault;

  if (doDefault && weakFrame.IsAlive())
    return nsTitleBarFrame::HandleEvent(aPresContext, aEvent, aEventStatus);

  return NS_OK;
}

nsIContent* nsResizerFrame::GetContentToResize(mozilla::PresShell* aPresShell,
                                               nsIBaseWindow** aWindow) {
  *aWindow = nullptr;

  nsAutoString elementid;
  mContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::element,
                                 elementid);
  if (elementid.IsEmpty()) {
    // If the resizer is in a popup, resize the popup's widget, otherwise
    // resize the widget associated with the window.
    nsIFrame* popup = GetParent();
    while (popup) {
      nsMenuPopupFrame* popupFrame = do_QueryFrame(popup);
      if (popupFrame) {
        return popupFrame->GetContent();
      }
      popup = popup->GetParent();
    }

    // don't allow resizing windows in content shells
    nsCOMPtr<nsIDocShellTreeItem> dsti =
        aPresShell->GetPresContext()->GetDocShell();
    if (!dsti || dsti->ItemType() != nsIDocShellTreeItem::typeChrome) {
      // don't allow resizers in content shells, except for the viewport
      // scrollbar which doesn't have a parent
      nsIContent* nonNativeAnon =
          mContent->FindFirstNonChromeOnlyAccessContent();
      if (!nonNativeAnon || nonNativeAnon->GetParent()) {
        return nullptr;
      }
    }

    // get the document and the window - should this be cached?
    if (nsPIDOMWindowOuter* domWindow =
            aPresShell->GetDocument()->GetWindow()) {
      nsCOMPtr<nsIDocShell> docShell = domWindow->GetDocShell();
      if (docShell) {
        nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
        docShell->GetTreeOwner(getter_AddRefs(treeOwner));
        if (treeOwner) {
          CallQueryInterface(treeOwner, aWindow);
        }
      }
    }

    return nullptr;
  }

  if (elementid.EqualsLiteral("_parent")) {
    // return the parent, but skip over native anonymous content
    nsIContent* parent = mContent->GetParent();
    return parent ? parent->FindFirstNonChromeOnlyAccessContent() : nullptr;
  }

  return aPresShell->GetDocument()->GetElementById(elementid);
}

void nsResizerFrame::AdjustDimensions(int32_t* aPos, int32_t* aSize,
                                      int32_t aMinSize, int32_t aMaxSize,
                                      int32_t aMovement,
                                      int8_t aResizerDirection) {
  int32_t oldSize = *aSize;

  *aSize += aResizerDirection * aMovement;
  // use one as a minimum size or the element could disappear
  if (*aSize < 1) *aSize = 1;

  // Constrain the size within the minimum and maximum size.
  *aSize = std::max(aMinSize, std::min(aMaxSize, *aSize));

  // For left and top resizers, the window must be moved left by the same
  // amount that the window was resized.
  if (aResizerDirection == -1) *aPos += oldSize - *aSize;
}

/* static */
void nsResizerFrame::ResizeContent(nsIContent* aContent,
                                   const Direction& aDirection,
                                   const SizeInfo& aSizeInfo,
                                   SizeInfo* aOriginalSizeInfo) {
  // for XUL elements, just set the width and height attributes. For
  // other elements, set style.width and style.height
  if (aContent->IsXULElement()) {
    if (aOriginalSizeInfo) {
      aContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::width,
                                     aOriginalSizeInfo->width);
      aContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::height,
                                     aOriginalSizeInfo->height);
    }
    // only set the property if the element could have changed in that direction
    if (aDirection.mHorizontal) {
      aContent->AsElement()->SetAttr(kNameSpaceID_None, nsGkAtoms::width,
                                     aSizeInfo.width, true);
    }
    if (aDirection.mVertical) {
      aContent->AsElement()->SetAttr(kNameSpaceID_None, nsGkAtoms::height,
                                     aSizeInfo.height, true);
    }
  } else {
    nsCOMPtr<nsStyledElement> inlineStyleContent = do_QueryInterface(aContent);
    if (inlineStyleContent) {
      nsICSSDeclaration* decl = inlineStyleContent->Style();

      if (aOriginalSizeInfo) {
        decl->GetPropertyValue(NS_LITERAL_STRING("width"),
                               aOriginalSizeInfo->width);
        decl->GetPropertyValue(NS_LITERAL_STRING("height"),
                               aOriginalSizeInfo->height);
      }

      // only set the property if the element could have changed in that
      // direction
      if (aDirection.mHorizontal) {
        nsAutoString widthstr(aSizeInfo.width);
        if (!widthstr.IsEmpty() &&
            !Substring(widthstr, widthstr.Length() - 2, 2).EqualsLiteral("px"))
          widthstr.AppendLiteral("px");
        decl->SetProperty(NS_LITERAL_STRING("width"), widthstr, EmptyString());
      }
      if (aDirection.mVertical) {
        nsAutoString heightstr(aSizeInfo.height);
        if (!heightstr.IsEmpty() &&
            !Substring(heightstr, heightstr.Length() - 2, 2)
                 .EqualsLiteral("px"))
          heightstr.AppendLiteral("px");
        decl->SetProperty(NS_LITERAL_STRING("height"), heightstr,
                          EmptyString());
      }
    }
  }
}

/* static */
void nsResizerFrame::MaybePersistOriginalSize(nsIContent* aContent,
                                              const SizeInfo& aSizeInfo) {
  nsresult rv;

  aContent->GetProperty(nsGkAtoms::_moz_original_size, &rv);
  if (rv != NS_PROPTABLE_PROP_NOT_THERE) return;

  nsAutoPtr<SizeInfo> sizeInfo(new SizeInfo(aSizeInfo));
  rv = aContent->SetProperty(nsGkAtoms::_moz_original_size, sizeInfo.get(),
                             nsINode::DeleteProperty<nsResizerFrame::SizeInfo>);
  if (NS_SUCCEEDED(rv)) sizeInfo.forget();
}

/* static */
void nsResizerFrame::RestoreOriginalSize(nsIContent* aContent) {
  nsresult rv;
  SizeInfo* sizeInfo = static_cast<SizeInfo*>(
      aContent->GetProperty(nsGkAtoms::_moz_original_size, &rv));
  if (NS_FAILED(rv)) return;

  NS_ASSERTION(sizeInfo, "We set a null sizeInfo!?");
  Direction direction = {1, 1};
  ResizeContent(aContent, direction, *sizeInfo, nullptr);
  aContent->DeleteProperty(nsGkAtoms::_moz_original_size);
}

/* returns a Direction struct containing the horizontal and vertical direction
 */
nsResizerFrame::Direction nsResizerFrame::GetDirection() {
  static const Element::AttrValuesArray strings[] = {
      // clang-format off
     nsGkAtoms::topleft,    nsGkAtoms::top,    nsGkAtoms::topright,
     nsGkAtoms::left,                          nsGkAtoms::right,
     nsGkAtoms::bottomleft, nsGkAtoms::bottom, nsGkAtoms::bottomright,
     nsGkAtoms::bottomstart,                   nsGkAtoms::bottomend,
     nullptr
      // clang-format on
  };

  static const Direction directions[] = {
      // clang-format off
     {-1, -1}, {0, -1}, {1, -1},
     {-1,  0},          {1,  0},
     {-1,  1}, {0,  1}, {1,  1},
     {-1,  1},          {1,  1}
      // clang-format on
  };

  if (!GetContent()) {
    return directions[0];  // default: topleft
  }

  int32_t index = mContent->AsElement()->FindAttrValueIn(
      kNameSpaceID_None, nsGkAtoms::dir, strings, eCaseMatters);
  if (index < 0) {
    return directions[0];  // default: topleft
  }

  if (index >= 8) {
    // Directions 8 and higher are RTL-aware directions and should reverse the
    // horizontal component if RTL.
    WritingMode wm = GetWritingMode();
    if (!(wm.IsVertical() ? wm.IsVerticalLR() : wm.IsBidiLTR())) {
      Direction direction = directions[index];
      direction.mHorizontal *= -1;
      return direction;
    }
  }

  return directions[index];
}

void nsResizerFrame::MouseClicked(WidgetMouseEvent* aEvent) {
  // Execute the oncommand event handler.
  nsCOMPtr<nsIContent> content = mContent;
  nsContentUtils::DispatchXULCommand(
      content, false, nullptr, nullptr, aEvent->IsControl(), aEvent->IsAlt(),
      aEvent->IsShift(), aEvent->IsMeta(), aEvent->mInputSource);
}
