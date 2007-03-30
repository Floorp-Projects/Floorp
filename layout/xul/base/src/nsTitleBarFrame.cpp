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
#include "nsTitleBarFrame.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIDOMXULDocument.h"
#include "nsIDOMNodeList.h"
#include "nsGkAtoms.h"
#include "nsIWidget.h"
#include "nsPresContext.h"
#include "nsPIDOMWindow.h"
#include "nsIViewManager.h"
#include "nsGUIEvent.h"
#include "nsEventDispatcher.h"
#include "nsDisplayList.h"

//
// NS_NewTitleBarFrame
//
// Creates a new TitleBar frame and returns it
//
nsIFrame*
NS_NewTitleBarFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsTitleBarFrame(aPresShell, aContext);
} // NS_NewTitleBarFrame

nsTitleBarFrame::nsTitleBarFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
:nsBoxFrame(aPresShell, aContext, PR_FALSE)
{
  mTrackingMouseMove = PR_FALSE;
}



NS_IMETHODIMP
nsTitleBarFrame::Init(nsIContent*      aContent,
                      nsIFrame*        aParent,
                      nsIFrame*        asPrevInFlow)
{
  nsresult rv = nsBoxFrame::Init(aContent, aParent, asPrevInFlow);

  CreateViewForFrame(PresContext(), this, GetStyleContext(), PR_TRUE);

  return rv;
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


  PRBool doDefault = PR_TRUE;

  switch (aEvent->message) {

   case NS_MOUSE_BUTTON_DOWN:  {
       if (aEvent->eventStructType == NS_MOUSE_EVENT &&
           NS_STATIC_CAST(nsMouseEvent*, aEvent)->button ==
             nsMouseEvent::eLeftButton)
       {

         // we're tracking.
         mTrackingMouseMove = PR_TRUE;

         // start capture.
         CaptureMouseEvents(aPresContext,PR_TRUE);



         // remember current mouse coordinates.
         mLastPoint = aEvent->refPoint;

         *aEventStatus = nsEventStatus_eConsumeNoDefault;
         doDefault = PR_FALSE;
       }
     }
     break;


   case NS_MOUSE_BUTTON_UP: {
       if(mTrackingMouseMove && aEvent->eventStructType == NS_MOUSE_EVENT &&
          NS_STATIC_CAST(nsMouseEvent*, aEvent)->button ==
            nsMouseEvent::eLeftButton)
       {
         // we're done tracking.
         mTrackingMouseMove = PR_FALSE;

         // end capture
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
         nsPIDOMWindow *window =
           aPresContext->PresShell()->GetDocument()->GetWindow();

         if (window) {
           nsPoint nsMoveBy = aEvent->refPoint - mLastPoint;
           window->MoveBy(nsMoveBy.x,nsMoveBy.y);
         }

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

  if ( doDefault )
    return nsBoxFrame::HandleEvent(aPresContext, aEvent, aEventStatus);
  else
    return NS_OK;
}

NS_IMETHODIMP
nsTitleBarFrame::CaptureMouseEvents(nsPresContext* aPresContext,PRBool aGrabMouseEvents)
{
  // get its view
  nsIView* view = GetView();
  PRBool result;

  if (view) {
    nsIViewManager* viewMan = view->GetViewManager();
    if (viewMan) {
      // nsIWidget* widget = view->GetWidget();
      if (aGrabMouseEvents) {
        viewMan->GrabMouseEvents(view,result);
        //mIsCapturingMouseEvents = PR_TRUE;
        //widget->CaptureMouse(PR_TRUE);
      } else {
        viewMan->GrabMouseEvents(nsnull,result);
        //mIsCapturingMouseEvents = PR_FALSE;
        //widget->CaptureMouse(PR_FALSE);
      }
    }
  }

  return NS_OK;

}



void
nsTitleBarFrame::MouseClicked(nsPresContext* aPresContext, nsGUIEvent* aEvent)
{
  // Execute the oncommand event handler.
  nsEventStatus status = nsEventStatus_eIgnore;

  nsXULCommandEvent event(aEvent ? NS_IS_TRUSTED_EVENT(aEvent) : PR_FALSE,
                          NS_XUL_COMMAND, nsnull);

  nsEventDispatcher::Dispatch(mContent, aPresContext, &event, nsnull, &status);
}
