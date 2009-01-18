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
#include "nsResizerFrame.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIDOMXULDocument.h"
#include "nsIDOMNodeList.h"
#include "nsGkAtoms.h"
#include "nsINameSpaceManager.h"

#include "nsIWidget.h"
#include "nsPresContext.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIBaseWindow.h"
#include "nsPIDOMWindow.h"
#include "nsGUIEvent.h"
#include "nsEventDispatcher.h"

//
// NS_NewResizerFrame
//
// Creates a new Resizer frame and returns it
//
nsIFrame*
NS_NewResizerFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsResizerFrame(aPresShell, aContext);
} // NS_NewResizerFrame

nsResizerFrame::nsResizerFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
:nsTitleBarFrame(aPresShell, aContext)
{
}

NS_IMETHODIMP
nsResizerFrame::HandleEvent(nsPresContext* aPresContext,
                            nsGUIEvent* aEvent,
                            nsEventStatus* aEventStatus)
{
  nsWeakFrame weakFrame(this);
  PRBool doDefault = PR_TRUE;

  // what direction should we go in? 
  Direction direction = GetDirection();

  switch (aEvent->message) {

   case NS_MOUSE_BUTTON_DOWN: {
       if (aEvent->eventStructType == NS_MOUSE_EVENT &&
           static_cast<nsMouseEvent*>(aEvent)->button ==
             nsMouseEvent::eLeftButton)
       {

         nsresult rv = NS_OK;

         // ask the widget implementation to begin a resize drag if it can
         rv = aEvent->widget->BeginResizeDrag(aEvent, 
             direction.mHorizontal, direction.mVertical);

         if (rv == NS_ERROR_NOT_IMPLEMENTED) {
           // there's no native resize support, 
           // we need to window resizing ourselves

           // we're tracking.
           mTrackingMouseMove = PR_TRUE;

           // start capture.
           aEvent->widget->CaptureMouse(PR_TRUE);
           CaptureMouseEvents(aPresContext,PR_TRUE);

           // remember current mouse coordinates.
           mLastPoint = aEvent->refPoint;
           aEvent->widget->GetScreenBounds(mWidgetRect);
         }

         *aEventStatus = nsEventStatus_eConsumeNoDefault;
         doDefault = PR_FALSE;
       }
     }
     break;


   case NS_MOUSE_BUTTON_UP: {

       if(mTrackingMouseMove && aEvent->eventStructType == NS_MOUSE_EVENT &&
          static_cast<nsMouseEvent*>(aEvent)->button ==
            nsMouseEvent::eLeftButton)
       {
         // we're done tracking.
         mTrackingMouseMove = PR_FALSE;

         // end capture
         aEvent->widget->CaptureMouse(PR_FALSE);
         CaptureMouseEvents(aPresContext,PR_FALSE);

         *aEventStatus = nsEventStatus_eConsumeNoDefault;
         doDefault = PR_FALSE;
       }
     }
     break;

   case NS_MOUSE_MOVE: {
       if(mTrackingMouseMove)
       {
         // get the document and the window - should this be cached?
         nsPIDOMWindow *domWindow =
           aPresContext->PresShell()->GetDocument()->GetWindow();
         NS_ENSURE_TRUE(domWindow, NS_ERROR_FAILURE);

         nsCOMPtr<nsIDocShellTreeItem> docShellAsItem =
           do_QueryInterface(domWindow->GetDocShell());
         NS_ENSURE_TRUE(docShellAsItem, NS_ERROR_FAILURE);

         nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
         docShellAsItem->GetTreeOwner(getter_AddRefs(treeOwner));

         nsCOMPtr<nsIBaseWindow> window(do_QueryInterface(treeOwner));

         if (!window) {
           return NS_OK;
         }

         PRInt32 x,y,cx,cy;
         window->GetPositionAndSize(&x,&y,&cx,&cy);
         nsIntPoint oldWindowTopLeft(x, y);

         // both MouseMove and direction are negative when pointing to the
         // top and left, and positive when pointing to the bottom and right
         nsIntPoint mouseMove(aEvent->refPoint - mLastPoint);
         
         AdjustDimensions(&x, &cx, mouseMove.x, direction.mHorizontal);
         AdjustDimensions(&y, &cy, mouseMove.y, direction.mVertical);

         // remember the last mouse point, relative to the *new* window
         mLastPoint = aEvent->refPoint + (oldWindowTopLeft - nsIntPoint(x, y));

         window->SetPositionAndSize(x,y,cx,cy,PR_TRUE); // do the repaint.

         *aEventStatus = nsEventStatus_eConsumeNoDefault;

         doDefault = PR_FALSE;
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

  if (doDefault && weakFrame.IsAlive())
    return nsTitleBarFrame::HandleEvent(aPresContext, aEvent, aEventStatus);
  else
    return NS_OK;
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
    case -1: // only move the window when the direction is top and/or left
      *aPos+= aMovement;
    case 1: // falling through: the window is resized in both cases
      *aSize+= aResizerDirection*aMovement;
  }
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
                                                 &nsGkAtoms::bottomend,
     nsnull};

  static const Direction directions[] =
    {{-1, -1}, {0, -1}, {1, -1},
     {-1,  0},          {1,  0},
     {-1,  1}, {0,  1}, {1,  1},
                        {1,  1}
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
  nsEventStatus status = nsEventStatus_eIgnore;

  nsXULCommandEvent event(aEvent ? NS_IS_TRUSTED_EVENT(aEvent) : PR_FALSE,
                          NS_XUL_COMMAND, nsnull);

  nsEventDispatcher::Dispatch(mContent, aPresContext, &event, nsnull, &status);
}
