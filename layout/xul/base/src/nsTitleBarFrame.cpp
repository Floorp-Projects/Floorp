/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Original Author: Eric J. Burley (ericb@neoplanet.com)
 *
 * Contributor(s): 
 * based heavily on David Hyatt's work on nsButtonBoxFrame
 */

#include "nsCOMPtr.h"
#include "nsTitleBarFrame.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIDOMXULDocument.h"
#include "nsIDOMNodeList.h"
#include "nsHTMLAtoms.h"
#include "nsINameSpaceManager.h"

#include "nsIWidget.h"
#include "nsIPresContext.h"
#include "nsIDOMWindowInternal.h"
#include "nsIScriptGlobalObject.h"
#include "nsIViewManager.h"

//
// NS_NewTitleBarFrame
//
// Creates a new TitleBar frame and returns it in |aNewFrame|
//
nsresult
NS_NewTitleBarFrame( nsIPresShell* aPresShell, nsIFrame** aNewFrame )
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsTitleBarFrame* it = new (aPresShell) nsTitleBarFrame(aPresShell);
  if (nsnull == it)
    return NS_ERROR_OUT_OF_MEMORY;

  // it->SetFlags(aFlags);
  *aNewFrame = it;
  return NS_OK;
  
} // NS_NewTitleBarFrame

nsTitleBarFrame::nsTitleBarFrame(nsIPresShell* aPresShell)
:nsBoxFrame(aPresShell, PR_FALSE) 
{
	mTrackingMouseMove = PR_FALSE;
}



NS_IMETHODIMP  nsTitleBarFrame::Init(nsIPresContext*  aPresContext,
                nsIContent*      aContent,
                nsIFrame*        aParent,
                nsIStyleContext* aContext,
                nsIFrame*        asPrevInFlow)
{
	nsresult rv = nsBoxFrame::Init(aPresContext, aContent, aParent, aContext, asPrevInFlow);

	CreateViewForFrame(aPresContext,this,aContext,PR_TRUE);

	return rv;
}




NS_IMETHODIMP
nsTitleBarFrame::GetMouseThrough(PRBool& aMouseThrough)
{
  aMouseThrough = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP nsTitleBarFrame::GetFrameForPoint(nsIPresContext* aPresContext,
                                    const nsPoint& aPoint, 
                                    nsFramePaintLayer aWhichLayer,
                                    nsIFrame**     aFrame)
{
  // override, since we don't want children to get events
  return nsFrame::GetFrameForPoint(aPresContext, aPoint, aWhichLayer, aFrame);
}





	
NS_IMETHODIMP
nsTitleBarFrame::HandleEvent(nsIPresContext* aPresContext, 
                                      nsGUIEvent* aEvent,
                                      nsEventStatus* aEventStatus)
{


  PRBool doDefault = PR_TRUE;

  switch (aEvent->message) {

	 case NS_MOUSE_LEFT_BUTTON_DOWN:	{
			 
			 // we're tracking.
			 mTrackingMouseMove = PR_TRUE;
			 
			 // start capture.		
			 CaptureMouseEvents(aPresContext,PR_TRUE);


			 
			 // remember current mouse coordinates.
			 mLastPoint = aEvent->refPoint;

			 *aEventStatus = nsEventStatus_eConsumeNoDefault;
			 doDefault = PR_FALSE;
		 }
		 break;
		 

	 case NS_MOUSE_LEFT_BUTTON_UP: {

			 if(mTrackingMouseMove)
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
			   // get the document and the global script object - should this be cached?
			   nsCOMPtr<nsIPresShell> presShell;
			   aPresContext->GetShell(getter_AddRefs(presShell));
			   nsCOMPtr<nsIDocument> document;
			   presShell->GetDocument(getter_AddRefs(document));
			   nsCOMPtr<nsIScriptGlobalObject> scriptGlobalObject;
			   document->GetScriptGlobalObject(getter_AddRefs(scriptGlobalObject));
			   nsCOMPtr<nsIDOMWindowInternal> window(do_QueryInterface(scriptGlobalObject));



				 nsPoint nsMoveBy;
				 nsMoveBy = aEvent->refPoint - mLastPoint;
				 
				 
				 window->MoveBy(nsMoveBy.x,nsMoveBy.y);
				 
				 
				 
				 *aEventStatus = nsEventStatus_eConsumeNoDefault;				
				 
				 doDefault = PR_FALSE;
			 }
		 }
		 break;



    case NS_MOUSE_LEFT_CLICK:
      MouseClicked(aPresContext);
      break;
  }
  
  if ( doDefault )  
	  return nsBoxFrame::HandleEvent(aPresContext, aEvent, aEventStatus);
  else
	  return NS_OK;
}

NS_IMETHODIMP 
nsTitleBarFrame::CaptureMouseEvents(nsIPresContext* aPresContext,PRBool aGrabMouseEvents)
{
	// get its view
  nsIView* view = nsnull;
  GetView(aPresContext, &view);
  nsCOMPtr<nsIViewManager> viewMan;
  PRBool result;

  nsCOMPtr<nsIWidget> widget;

  if (view) {
    view->GetViewManager(*getter_AddRefs(viewMan));
    if (viewMan) {
      view->GetWidget(*getter_AddRefs(widget));
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
nsTitleBarFrame::MouseClicked (nsIPresContext* aPresContext) 
{
  // Execute the oncommand event handler.
  nsEventStatus status = nsEventStatus_eIgnore;
  nsMouseEvent event;
  event.eventStructType = NS_EVENT;
  event.message = NS_MENU_ACTION;
  event.isShift = PR_FALSE;
  event.isControl = PR_FALSE;
  event.isAlt = PR_FALSE;
  event.isMeta = PR_FALSE;
  event.clickCount = 0;
  event.widget = nsnull;
  mContent->HandleDOMEvent(aPresContext, &event, nsnull, NS_EVENT_FLAG_INIT, &status);
}
