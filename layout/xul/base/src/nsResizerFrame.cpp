/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Original Author: Eric J. Burley (ericb@neoplanet.com)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsResizerFrame.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIDOMXULDocument.h"
#include "nsIDOMNodeList.h"
#include "nsGkAtoms.h"
#include "nsINameSpaceManager.h"
#include "nsIDOMElementCSSInlineStyle.h"
#include "nsIDOMCSSStyleDeclaration.h"

#include "nsPresContext.h"
#include "nsFrameManager.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIBaseWindow.h"
#include "nsPIDOMWindow.h"
#include "nsGUIEvent.h"
#include "nsEventDispatcher.h"
#include "nsContentUtils.h"
#include "nsMenuPopupFrame.h"
#include "nsIScreenManager.h"
#include "mozilla/dom/Element.h"
#include "nsContentErrors.h"


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
                            nsGUIEvent* aEvent,
                            nsEventStatus* aEventStatus)
{
  NS_ENSURE_ARG_POINTER(aEventStatus);
  if (nsEventStatus_eConsumeNoDefault == *aEventStatus) {
    return NS_OK;
  }

  nsWeakFrame weakFrame(this);
  bool doDefault = true;

  switch (aEvent->message) {
    case NS_MOUSE_BUTTON_DOWN: {
      if (aEvent->eventStructType == NS_MOUSE_EVENT &&
        static_cast<nsMouseEvent*>(aEvent)->button == nsMouseEvent::eLeftButton)
      {
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
          switch (frameToResize->GetStylePosition()->mBoxSizing) {
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

        // we're tracking
        mTrackingMouseMove = true;

        // remember current mouse coordinates
        mMouseDownPoint = aEvent->refPoint + aEvent->widget->WidgetToScreenOffset();

        nsIPresShell::SetCapturingContent(GetContent(), CAPTURE_IGNOREALLOWED);
      }
    }
    break;

  case NS_MOUSE_BUTTON_UP: {

    if (mTrackingMouseMove && aEvent->eventStructType == NS_MOUSE_EVENT &&
        static_cast<nsMouseEvent*>(aEvent)->button == nsMouseEvent::eLeftButton)
    {
      // we're done tracking.
      mTrackingMouseMove = false;

      nsIPresShell::SetCapturingContent(nsnull, 0);

      doDefault = false;
    }
  }
  break;

  case NS_MOUSE_MOVE: {
    if (mTrackingMouseMove)
    {
      nsCOMPtr<nsIBaseWindow> window;
      nsIPresShell* presShell = aPresContext->GetPresShell();
      nsCOMPtr<nsIContent> contentToResize =
        GetContentToResize(presShell, getter_AddRefs(window));

      // check if the returned content really is a menupopup
      nsMenuPopupFrame* menuPopupFrame = nsnull;
      if (contentToResize) {
        nsIFrame* frameToResize = contentToResize->GetPrimaryFrame();
        if (frameToResize && frameToResize->GetType() == nsGkAtoms::menuPopupFrame) {
          menuPopupFrame = static_cast<nsMenuPopupFrame *>(frameToResize);
        }
      }

      // both MouseMove and direction are negative when pointing to the
      // top and left, and positive when pointing to the bottom and right

      // retrieve the offset of the mousemove event relative to the mousedown.
      // The difference is how much the resize needs to be
      nsIntPoint screenPoint(aEvent->refPoint + aEvent->widget->WidgetToScreenOffset());
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
      AdjustDimensions(&rect.x, &rect.width, mouseMove.x, direction.mHorizontal);
      AdjustDimensions(&rect.y, &rect.height, mouseMove.y, direction.mVertical);

      // Don't allow resizing a window or a popup past the edge of the screen,
      // so adjust the rectangle to fit within the available screen area.
      if (window) {
        nsCOMPtr<nsIScreen> screen;
        nsCOMPtr<nsIScreenManager> sm(do_GetService("@mozilla.org/gfx/screenmanager;1"));
        if (sm) {
          nsIntRect frameRect = GetScreenRect();
          sm->ScreenForRect(frameRect.x, frameRect.y, 1, 1, getter_AddRefs(screen));
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

  case NS_MOUSE_CLICK:
    if (NS_IS_MOUSE_LEFT_CLICK(aEvent))
    {
      MouseClicked(aPresContext, aEvent);
    }
    break;

  case NS_MOUSE_DOUBLECLICK:
    if (aEvent->eventStructType == NS_MOUSE_EVENT &&
        static_cast<nsMouseEvent*>(aEvent)->button == nsMouseEvent::eLeftButton)
    {
      nsCOMPtr<nsIBaseWindow> window;
      nsIPresShell* presShell = aPresContext->GetPresShell();
      nsIContent* contentToResize =
        GetContentToResize(presShell, getter_AddRefs(window));
      if (contentToResize) {
        nsIFrame* frameToResize = contentToResize->GetPrimaryFrame();
        if (frameToResize && frameToResize->GetType() == nsGkAtoms::menuPopupFrame)
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
  *aWindow = nsnull;

  nsAutoString elementid;
  mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::element, elementid);
  if (elementid.IsEmpty()) {
    // If the resizer is in a popup, resize the popup's widget, otherwise
    // resize the widget associated with the window.
    nsIFrame* popup = GetParent();
    while (popup) {
      if (popup->GetType() == nsGkAtoms::menuPopupFrame) {
        return popup->GetContent();
      }
      popup = popup->GetParent();
    }

    // don't allow resizing windows in content shells
    bool isChromeShell = false;
    nsCOMPtr<nsISupports> cont = aPresShell->GetPresContext()->GetContainer();
    nsCOMPtr<nsIDocShellTreeItem> dsti = do_QueryInterface(cont);
    if (dsti) {
      PRInt32 type = -1;
      isChromeShell = (NS_SUCCEEDED(dsti->GetItemType(&type)) &&
                       type == nsIDocShellTreeItem::typeChrome);
    }

    if (!isChromeShell) {
      // don't allow resizers in content shells, except for the viewport
      // scrollbar which doesn't have a parent
      nsIContent* nonNativeAnon = mContent->FindFirstNonNativeAnonymous();
      if (!nonNativeAnon || nonNativeAnon->GetParent()) {
        return nsnull;
      }
    }

    // get the document and the window - should this be cached?
    nsPIDOMWindow *domWindow = aPresShell->GetDocument()->GetWindow();
    if (domWindow) {
      nsCOMPtr<nsIDocShellTreeItem> docShellAsItem =
        do_QueryInterface(domWindow->GetDocShell());
      if (docShellAsItem) {
        nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
        docShellAsItem->GetTreeOwner(getter_AddRefs(treeOwner));
        if (treeOwner) {
          CallQueryInterface(treeOwner, aWindow);
        }
      }
    }

    return nsnull;
  }

  if (elementid.EqualsLiteral("_parent")) {
    // return the parent, but skip over native anonymous content
    nsIContent* parent = mContent->GetParent();
    return parent ? parent->FindFirstNonNativeAnonymous() : nsnull;
  }

  return aPresShell->GetDocument()->GetElementById(elementid);
}

/* adjust the window position and size according to the mouse movement and
 * the resizer direction
 */
void
nsResizerFrame::AdjustDimensions(PRInt32* aPos, PRInt32* aSize,
                                 PRInt32 aMovement, PRInt8 aResizerDirection)
{
  switch(aResizerDirection)
  {
    case -1:
      // only move the window when the direction is top and/or left
      *aPos+= aMovement;
      // falling through: the window is resized in both cases
    case 1:
      *aSize+= aResizerDirection*aMovement;
      // use one as a minimum size or the element could disappear
      if (*aSize < 1)
        *aSize = 1;
  }
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
  ResizeContent(aContent, direction, *sizeInfo, nsnull);
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
     nsnull};

  static const Direction directions[] =
    {{-1, -1}, {0, -1}, {1, -1},
     {-1,  0},          {1,  0},
     {-1,  1}, {0,  1}, {1,  1},
     {-1,  1},          {1,  1}
    };

  if (!GetContent())
    return directions[0]; // default: topleft

  PRInt32 index = GetContent()->FindAttrValueIn(kNameSpaceID_None,
                                                nsGkAtoms::dir,
                                                strings, eCaseMatters);
  if(index < 0)
    return directions[0]; // default: topleft
  else if (index >= 8 && GetStyleVisibility()->mDirection == NS_STYLE_DIRECTION_RTL) {
    // Directions 8 and higher are RTL-aware directions and should reverse the
    // horizontal component if RTL.
    Direction direction = directions[index];
    direction.mHorizontal *= -1;
    return direction;
  }
  return directions[index];
}

void
nsResizerFrame::MouseClicked(nsPresContext* aPresContext, nsGUIEvent *aEvent)
{
  // Execute the oncommand event handler.
  nsContentUtils::DispatchXULCommand(mContent,
                                     aEvent ?
                                       NS_IS_TRUSTED_EVENT(aEvent) : false);
}
