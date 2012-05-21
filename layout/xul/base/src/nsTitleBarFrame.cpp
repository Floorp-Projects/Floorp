/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMPtr.h"
#include "nsTitleBarFrame.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIDOMXULDocument.h"
#include "nsIDOMNodeList.h"
#include "nsGkAtoms.h"
#include "nsIWidget.h"
#include "nsMenuPopupFrame.h"
#include "nsPresContext.h"
#include "nsIDocShellTreeItem.h"
#include "nsPIDOMWindow.h"
#include "nsGUIEvent.h"
#include "nsEventDispatcher.h"
#include "nsDisplayList.h"
#include "nsContentUtils.h"

//
// NS_NewTitleBarFrame
//
// Creates a new TitleBar frame and returns it
//
nsIFrame*
NS_NewTitleBarFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsTitleBarFrame(aPresShell, aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsTitleBarFrame)

nsTitleBarFrame::nsTitleBarFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
:nsBoxFrame(aPresShell, aContext, false)
{
  mTrackingMouseMove = false;
  UpdateMouseThrough();
}

NS_IMETHODIMP
nsTitleBarFrame::BuildDisplayListForChildren(nsDisplayListBuilder*   aBuilder,
                                             const nsRect&           aDirtyRect,
                                             const nsDisplayListSet& aLists)
{
  // override, since we don't want children to get events
  if (aBuilder->IsForEventDelivery()) {
    if (!mContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::allowevents,
                               nsGkAtoms::_true, eCaseMatters))
      return NS_OK;
  }
  return nsBoxFrame::BuildDisplayListForChildren(aBuilder, aDirtyRect, aLists);
}

NS_IMETHODIMP
nsTitleBarFrame::HandleEvent(nsPresContext* aPresContext,
                                      nsGUIEvent* aEvent,
                                      nsEventStatus* aEventStatus)
{
  NS_ENSURE_ARG_POINTER(aEventStatus);
  if (nsEventStatus_eConsumeNoDefault == *aEventStatus) {
    return NS_OK;
  }

  bool doDefault = true;

  switch (aEvent->message) {

   case NS_MOUSE_BUTTON_DOWN:  {
       if (aEvent->eventStructType == NS_MOUSE_EVENT &&
           static_cast<nsMouseEvent*>(aEvent)->button ==
             nsMouseEvent::eLeftButton)
       {
         // titlebar has no effect in non-chrome shells
         nsCOMPtr<nsISupports> cont = aPresContext->GetContainer();
         nsCOMPtr<nsIDocShellTreeItem> dsti = do_QueryInterface(cont);
         if (dsti) {
           PRInt32 type = -1;
           if (NS_SUCCEEDED(dsti->GetItemType(&type)) &&
               type == nsIDocShellTreeItem::typeChrome) {
             // we're tracking.
             mTrackingMouseMove = true;

             // start capture.
             nsIPresShell::SetCapturingContent(GetContent(), CAPTURE_IGNOREALLOWED);

             // remember current mouse coordinates.
             mLastPoint = aEvent->refPoint;
           }
         }

         *aEventStatus = nsEventStatus_eConsumeNoDefault;
         doDefault = false;
       }
     }
     break;


   case NS_MOUSE_BUTTON_UP: {
       if(mTrackingMouseMove && aEvent->eventStructType == NS_MOUSE_EVENT &&
          static_cast<nsMouseEvent*>(aEvent)->button ==
            nsMouseEvent::eLeftButton)
       {
         // we're done tracking.
         mTrackingMouseMove = false;

         // end capture
         nsIPresShell::SetCapturingContent(nsnull, 0);

         *aEventStatus = nsEventStatus_eConsumeNoDefault;
         doDefault = false;
       }
     }
     break;

   case NS_MOUSE_MOVE: {
       if(mTrackingMouseMove)
       {
         nsIntPoint nsMoveBy = aEvent->refPoint - mLastPoint;

         nsIFrame* parent = GetParent();
         while (parent && parent->GetType() != nsGkAtoms::menuPopupFrame)
           parent = parent->GetParent();

         // if the titlebar is in a popup, move the popup frame, otherwise
         // move the widget associated with the window
         if (parent) {
           nsMenuPopupFrame* menuPopupFrame = static_cast<nsMenuPopupFrame*>(parent);
           nsCOMPtr<nsIWidget> widget = menuPopupFrame->GetWidget();
           nsIntRect bounds;
           widget->GetScreenBounds(bounds);
           menuPopupFrame->MoveTo(bounds.x + nsMoveBy.x, bounds.y + nsMoveBy.y, false);
         }
         else {
           nsIPresShell* presShell = aPresContext->PresShell();
           nsPIDOMWindow *window = presShell->GetDocument()->GetWindow();
           if (window) {
             window->MoveBy(nsMoveBy.x, nsMoveBy.y);
           }
         }

         *aEventStatus = nsEventStatus_eConsumeNoDefault;

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
  }

  if ( doDefault )
    return nsBoxFrame::HandleEvent(aPresContext, aEvent, aEventStatus);
  else
    return NS_OK;
}

void
nsTitleBarFrame::MouseClicked(nsPresContext* aPresContext, nsGUIEvent* aEvent)
{
  // Execute the oncommand event handler.
  nsContentUtils::DispatchXULCommand(mContent,
                                     aEvent ?
                                       NS_IS_TRUSTED_EVENT(aEvent) : false);
}
