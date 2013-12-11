/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsResizerFrame.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIDOMNodeList.h"
#include "nsGkAtoms.h"
#include "nsINameSpaceManager.h"
#include "nsIDOMElementCSSInlineStyle.h"
#include "nsIDOMCSSStyleDeclaration.h"

#include "nsPresContext.h"
#include "nsFrameManager.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIBaseWindow.h"
#include "nsPIDOMWindow.h"
#include "mozilla/MouseEvents.h"
#include "nsEventDispatcher.h"
#include "nsContentUtils.h"
#include "nsMenuPopupFrame.h"
#include "nsIScreenManager.h"
#include "mozilla/dom/Element.h"
#include "nsError.h"
#include <algorithm>

using namespace mozilla;

//
// NS_NewResizerFrame
//
// Creates a new Resizer frame and returns it
//
nsIFrame*
NS_NewResizerFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsResizerFrame(aPresShell, aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsResizerFrame)

nsResizerFrame::nsResizerFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
:nsTitleBarFrame(aPresShell, aContext)
{
}

NS_IMETHODIMP
nsResizerFrame::HandleEvent(nsPresContext* aPresContext,
                            WidgetGUIEvent* aEvent,
                            nsEventStatus* aEventStatus)
{
  NS_ENSURE_ARG_POINTER(aEventStatus);
  if (nsEventStatus_eConsumeNoDefault == *aEventStatus) {
    return NS_OK;
  }

  nsWeakFrame weakFrame(this);
  bool doDefault = true;

  switch (aEvent->message) {
    case NS_TOUCH_START:
    case NS_MOUSE_BUTTON_DOWN: {
      if (aEvent->eventStructType == NS_TOUCH_EVENT ||
          (aEvent->eventStructType == NS_MOUSE_EVENT &&
           aEvent->AsMouseEvent()->button == WidgetMouseEvent::eLeftButton)) {
        nsCOMPtr<nsIBaseWindow> window;
        nsIPresShell* presShell = aPresContext->GetPresShell();
        nsIContent* contentToResize =
          GetContentToResize(presShell, getter_AddRefs(window));
        if (contentToResize) {
          nsIFrame* frameToResize = contentToResize->GetPrimaryFrame();
          if (!frameToResize)
            break;

          // cache the content rectangle for the frame to resize
          // GetScreenRectInAppUnits returns the border box rectangle, so
          // adjust to get the desired content rectangle.
          nsRect rect = frameToResize->GetScreenRectInAppUnits();
          switch (frameToResize->StylePosition()->mBoxSizing) {
            case NS_STYLE_BOX_SIZING_CONTENT:
              rect.Deflate(frameToResize->GetUsedPadding());
            case NS_STYLE_BOX_SIZING_PADDING:
              rect.Deflate(frameToResize->GetUsedBorder());
            default:
              break;
          }

          mMouseDownRect = rect.ToNearestPixels(aPresContext->AppUnitsPerDevPixel());
          doDefault = false;
        }
        else {
          // If there is no window, then resizing isn't allowed.
          if (!window)
            break;

          doDefault = false;
            
          // ask the widget implementation to begin a resize drag if it can
          Direction direction = GetDirection();
          nsresult rv = aEvent->widget->BeginResizeDrag(aEvent,
                        direction.mHorizontal, direction.mVertical);
          // for native drags, don't set the fields below
          if (rv != NS_ERROR_NOT_IMPLEMENTED)
             break;
             
          // if there's no native resize support, we need to do window
          // resizing ourselves
          window->GetPositionAndSize(&mMouseDownRect.x, &mMouseDownRect.y,
                                     &mMouseDownRect.width, &mMouseDownRect.height);
        }

        // remember current mouse coordinates
        nsIntPoint refPoint;
        if (!GetEventPoint(aEvent, refPoint))
          return NS_OK;
        mMouseDownPoint = refPoint + aEvent->widget->WidgetToScreenOffset();

        // we're tracking
        mTrackingMouseMove = true;

        nsIPresShell::SetCapturingContent(GetContent(), CAPTURE_IGNOREALLOWED);
      }
    }
    break;

  case NS_TOUCH_END:
  case NS_MOUSE_BUTTON_UP: {
    if (aEvent->eventStructType == NS_TOUCH_EVENT ||
        (aEvent->eventStructType == NS_MOUSE_EVENT &&
         aEvent->AsMouseEvent()->button == WidgetMouseEvent::eLeftButton)) {
      // we're done tracking.
      mTrackingMouseMove = false;

      nsIPresShell::SetCapturingContent(nullptr, 0);

      doDefault = false;
    }
  }
  break;

  case NS_TOUCH_MOVE:
  case NS_MOUSE_MOVE: {
    if (mTrackingMouseMove)
    {
      nsCOMPtr<nsIBaseWindow> window;
      nsIPresShell* presShell = aPresContext->GetPresShell();
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
      nsIntPoint refPoint;
      if (!GetEventPoint(aEvent, refPoint))
        return NS_OK;
      nsIntPoint screenPoint(refPoint + aEvent->widget->WidgetToScreenOffset());
      nsIntPoint mouseMove(screenPoint - mMouseDownPoint);

      // Determine which direction to resize by checking the dir attribute.
      // For windows and menus, ensure that it can be resized in that direction.
      Direction direction = GetDirection();
      if (window || menuPopupFrame) {
        if (menuPopupFrame) {
          menuPopupFrame->CanAdjustEdges(
            (direction.mHorizontal == -1) ? NS_SIDE_LEFT : NS_SIDE_RIGHT,
            (direction.mVertical == -1) ? NS_SIDE_TOP : NS_SIDE_BOTTOM, mouseMove);
        }
      }
      else if (!contentToResize) {
        break; // don't do anything if there's nothing to resize
      }

      nsIntRect rect = mMouseDownRect;

      // Check if there are any size constraints on this window.
      widget::SizeConstraints sizeConstraints;
      if (window) {
        nsCOMPtr<nsIWidget> widget;
        window->GetMainWidget(getter_AddRefs(widget));
        sizeConstraints = widget->GetSizeConstraints();
      }

      AdjustDimensions(&rect.x, &rect.width, sizeConstraints.mMinSize.width,
                       sizeConstraints.mMaxSize.width, mouseMove.x, direction.mHorizontal);
      AdjustDimensions(&rect.y, &rect.height, sizeConstraints.mMinSize.height,
                       sizeConstraints.mMaxSize.height, mouseMove.y, direction.mVertical);

      // Don't allow resizing a window or a popup past the edge of the screen,
      // so adjust the rectangle to fit within the available screen area.
      if (window) {
        nsCOMPtr<nsIScreen> screen;
        nsCOMPtr<nsIScreenManager> sm(do_GetService("@mozilla.org/gfx/screenmanager;1"));
        if (sm) {
          nsIntRect frameRect = GetScreenRect();
          // ScreenForRect requires display pixels, so scale from device pix
          double scale;
          window->GetUnscaledDevicePixelsPerCSSPixel(&scale);
          sm->ScreenForRect(NSToIntRound(frameRect.x / scale),
                            NSToIntRound(frameRect.y / scale), 1, 1,
                            getter_AddRefs(screen));
          if (screen) {
            nsIntRect screenRect;
            screen->GetRect(&screenRect.x, &screenRect.y,
                            &screenRect.width, &screenRect.height);
            rect.IntersectRect(rect, screenRect);
          }
        }
      }
      else if (menuPopupFrame) {
        nsRect frameRect = menuPopupFrame->GetScreenRectInAppUnits();
        nsIFrame* rootFrame = aPresContext->PresShell()->FrameManager()->GetRootFrame();
        nsRect rootScreenRect = rootFrame->GetScreenRectInAppUnits();

        nsRect screenRect = menuPopupFrame->GetConstraintRect(frameRect, rootScreenRect);
        // round using ToInsidePixels as it's better to be a pixel too small
        // than be too large. If the popup is too large it could get flipped
        // to the opposite side of the anchor point while resizing.
        nsIntRect screenRectPixels = screenRect.ToInsidePixels(aPresContext->AppUnitsPerDevPixel());
        rect.IntersectRect(rect, screenRectPixels);
      }

      if (contentToResize) {
        // convert the rectangle into css pixels. When changing the size in a
        // direction, don't allow the new size to be less that the resizer's
        // size. This ensures that content isn't resized too small as to make
        // the resizer invisible.
        nsRect appUnitsRect = rect.ToAppUnits(aPresContext->AppUnitsPerDevPixel());
        if (appUnitsRect.width < mRect.width && mouseMove.x)
          appUnitsRect.width = mRect.width;
        if (appUnitsRect.height < mRect.height && mouseMove.y)
          appUnitsRect.height = mRect.height;
        nsIntRect cssRect = appUnitsRect.ToInsidePixels(nsPresContext::AppUnitsPerCSSPixel());

        nsIntRect oldRect;
        nsWeakFrame weakFrame(menuPopupFrame);
        if (menuPopupFrame) {
          nsCOMPtr<nsIWidget> widget = menuPopupFrame->GetWidget();
          if (widget)
            widget->GetScreenBounds(oldRect);

          // convert the new rectangle into outer window coordinates
          nsIntPoint clientOffset = widget->GetClientOffset();
          rect.x -= clientOffset.x; 
          rect.y -= clientOffset.y; 
        }

        SizeInfo sizeInfo, originalSizeInfo;
        sizeInfo.width.AppendInt(cssRect.width);
        sizeInfo.height.AppendInt(cssRect.height);
        ResizeContent(contentToResize, direction, sizeInfo, &originalSizeInfo);
        MaybePersistOriginalSize(contentToResize, originalSizeInfo);

        // Move the popup to the new location unless it is anchored, since
        // the position shouldn't change. nsMenuPopupFrame::SetPopupPosition
        // will instead ensure that the popup's position is anchored at the
        // right place.
        if (weakFrame.IsAlive() &&
            (oldRect.x != rect.x || oldRect.y != rect.y) &&
            (!menuPopupFrame->IsAnchored() ||
             menuPopupFrame->PopupLevel() != ePopupLevelParent)) {
          menuPopupFrame->MoveTo(rect.x, rect.y, true);
        }
      }
      else {
        window->SetPositionAndSize(rect.x, rect.y, rect.width, rect.height, true); // do the repaint.
      }

      doDefault = false;
    }
  }
  break;

  case NS_MOUSE_CLICK: {
    WidgetMouseEvent* mouseEvent = aEvent->AsMouseEvent();
    if (mouseEvent->IsLeftClickEvent()) {
      MouseClicked(aPresContext, mouseEvent);
    }
    break;
  }
  case NS_MOUSE_DOUBLECLICK:
    if (aEvent->AsMouseEvent()->button == WidgetMouseEvent::eLeftButton) {
      nsCOMPtr<nsIBaseWindow> window;
      nsIPresShell* presShell = aPresContext->GetPresShell();
      nsIContent* contentToResize =
        GetContentToResize(presShell, getter_AddRefs(window));
      if (contentToResize) {
        nsMenuPopupFrame* menuPopupFrame = do_QueryFrame(contentToResize->GetPrimaryFrame());
        if (menuPopupFrame)
          break; // Don't restore original sizing for menupopup frames until
                 // we handle screen constraints here. (Bug 357725)

        RestoreOriginalSize(contentToResize);
      }
    }
    break;
  }

  if (!doDefault)
    *aEventStatus = nsEventStatus_eConsumeNoDefault;

  if (doDefault && weakFrame.IsAlive())
    return nsTitleBarFrame::HandleEvent(aPresContext, aEvent, aEventStatus);

  return NS_OK;
}

nsIContent*
nsResizerFrame::GetContentToResize(nsIPresShell* aPresShell, nsIBaseWindow** aWindow)
{
  *aWindow = nullptr;

  nsAutoString elementid;
  mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::element, elementid);
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
    bool isChromeShell = false;
    nsCOMPtr<nsISupports> cont = aPresShell->GetPresContext()->GetContainer();
    nsCOMPtr<nsIDocShellTreeItem> dsti = do_QueryInterface(cont);
    if (dsti) {
      int32_t type = -1;
      isChromeShell = (NS_SUCCEEDED(dsti->GetItemType(&type)) &&
                       type == nsIDocShellTreeItem::typeChrome);
    }

    if (!isChromeShell) {
      // don't allow resizers in content shells, except for the viewport
      // scrollbar which doesn't have a parent
      nsIContent* nonNativeAnon = mContent->FindFirstNonChromeOnlyAccessContent();
      if (!nonNativeAnon || nonNativeAnon->GetParent()) {
        return nullptr;
      }
    }

    // get the document and the window - should this be cached?
    nsPIDOMWindow *domWindow = aPresShell->GetDocument()->GetWindow();
    if (domWindow) {
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

void
nsResizerFrame::AdjustDimensions(int32_t* aPos, int32_t* aSize,
                                 int32_t aMinSize, int32_t aMaxSize,
                                 int32_t aMovement, int8_t aResizerDirection)
{
  int32_t oldSize = *aSize;

  *aSize += aResizerDirection * aMovement;
  // use one as a minimum size or the element could disappear
  if (*aSize < 1)
    *aSize = 1;

  // Constrain the size within the minimum and maximum size.
  *aSize = std::max(aMinSize, std::min(aMaxSize, *aSize));

  // For left and top resizers, the window must be moved left by the same
  // amount that the window was resized.
  if (aResizerDirection == -1)
    *aPos += oldSize - *aSize;
}

/* static */ void
nsResizerFrame::ResizeContent(nsIContent* aContent, const Direction& aDirection,
                              const SizeInfo& aSizeInfo, SizeInfo* aOriginalSizeInfo)
{
  // for XUL elements, just set the width and height attributes. For
  // other elements, set style.width and style.height
  if (aContent->IsXUL()) {
    if (aOriginalSizeInfo) {
      aContent->GetAttr(kNameSpaceID_None, nsGkAtoms::width,
                        aOriginalSizeInfo->width);
      aContent->GetAttr(kNameSpaceID_None, nsGkAtoms::height,
                        aOriginalSizeInfo->height);
    }
    // only set the property if the element could have changed in that direction
    if (aDirection.mHorizontal) {
      aContent->SetAttr(kNameSpaceID_None, nsGkAtoms::width, aSizeInfo.width, true);
    }
    if (aDirection.mVertical) {
      aContent->SetAttr(kNameSpaceID_None, nsGkAtoms::height, aSizeInfo.height, true);
    }
  }
  else {
    nsCOMPtr<nsIDOMElementCSSInlineStyle> inlineStyleContent =
      do_QueryInterface(aContent);
    if (inlineStyleContent) {
      nsCOMPtr<nsIDOMCSSStyleDeclaration> decl;
      inlineStyleContent->GetStyle(getter_AddRefs(decl));

      if (aOriginalSizeInfo) {
        decl->GetPropertyValue(NS_LITERAL_STRING("width"),
                               aOriginalSizeInfo->width);
        decl->GetPropertyValue(NS_LITERAL_STRING("height"),
                               aOriginalSizeInfo->height);
      }

      // only set the property if the element could have changed in that direction
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
            !Substring(heightstr, heightstr.Length() - 2, 2).EqualsLiteral("px"))
          heightstr.AppendLiteral("px");
        decl->SetProperty(NS_LITERAL_STRING("height"), heightstr, EmptyString());
      }
    }
  }
}

/* static */ void
nsResizerFrame::SizeInfoDtorFunc(void *aObject, nsIAtom *aPropertyName,
                                 void *aPropertyValue, void *aData)
{
  nsResizerFrame::SizeInfo *propertyValue =
    static_cast<nsResizerFrame::SizeInfo*>(aPropertyValue);
  delete propertyValue;
}

/* static */ void
nsResizerFrame::MaybePersistOriginalSize(nsIContent* aContent,
                                         const SizeInfo& aSizeInfo)
{
  nsresult rv;

  aContent->GetProperty(nsGkAtoms::_moz_original_size, &rv);
  if (rv != NS_PROPTABLE_PROP_NOT_THERE)
    return;

  nsAutoPtr<SizeInfo> sizeInfo(new SizeInfo(aSizeInfo));
  rv = aContent->SetProperty(nsGkAtoms::_moz_original_size, sizeInfo.get(),
                             &SizeInfoDtorFunc);
  if (NS_SUCCEEDED(rv))
    sizeInfo.forget();
}

/* static */ void
nsResizerFrame::RestoreOriginalSize(nsIContent* aContent)
{
  nsresult rv;
  SizeInfo* sizeInfo =
    static_cast<SizeInfo*>(aContent->GetProperty(nsGkAtoms::_moz_original_size,
                           &rv));
  if (NS_FAILED(rv))
    return;

  NS_ASSERTION(sizeInfo, "We set a null sizeInfo!?");
  Direction direction = {1, 1};
  ResizeContent(aContent, direction, *sizeInfo, nullptr);
  aContent->DeleteProperty(nsGkAtoms::_moz_original_size);
}

/* returns a Direction struct containing the horizontal and vertical direction
 */
nsResizerFrame::Direction
nsResizerFrame::GetDirection()
{
  static const nsIContent::AttrValuesArray strings[] =
    {&nsGkAtoms::topleft,    &nsGkAtoms::top,    &nsGkAtoms::topright,
     &nsGkAtoms::left,                           &nsGkAtoms::right,
     &nsGkAtoms::bottomleft, &nsGkAtoms::bottom, &nsGkAtoms::bottomright,
     &nsGkAtoms::bottomstart,                    &nsGkAtoms::bottomend,
     nullptr};

  static const Direction directions[] =
    {{-1, -1}, {0, -1}, {1, -1},
     {-1,  0},          {1,  0},
     {-1,  1}, {0,  1}, {1,  1},
     {-1,  1},          {1,  1}
    };

  if (!GetContent())
    return directions[0]; // default: topleft

  int32_t index = GetContent()->FindAttrValueIn(kNameSpaceID_None,
                                                nsGkAtoms::dir,
                                                strings, eCaseMatters);
  if(index < 0)
    return directions[0]; // default: topleft
  else if (index >= 8 && StyleVisibility()->mDirection == NS_STYLE_DIRECTION_RTL) {
    // Directions 8 and higher are RTL-aware directions and should reverse the
    // horizontal component if RTL.
    Direction direction = directions[index];
    direction.mHorizontal *= -1;
    return direction;
  }
  return directions[index];
}

void
nsResizerFrame::MouseClicked(nsPresContext* aPresContext,
                             WidgetMouseEvent* aEvent)
{
  // Execute the oncommand event handler.
  nsContentUtils::DispatchXULCommand(mContent,
                                     aEvent && aEvent->mFlags.mIsTrusted);
}
