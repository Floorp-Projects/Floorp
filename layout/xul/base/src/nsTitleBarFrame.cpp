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
#include "nsHTMLAtoms.h"
#include "nsIWidget.h"
#include "nsPresContext.h"
#include "nsIDOMWindowInternal.h"
#include "nsIScriptGlobalObject.h"
#include "nsIViewManager.h"
#include "nsGUIEvent.h"

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



NS_IMETHODIMP  nsTitleBarFrame::Init(nsPresContext*  aPresContext,
                nsIContent*      aContent,
                nsIFrame*        aParent,
                nsStyleContext*  aContext,
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

NS_IMETHODIMP nsTitleBarFrame::GetFrameForPoint(nsPresContext* aPresContext,
                                    const nsPoint& aPoint, 
                                    nsFramePaintLayer aWhichLayer,
                                    nsIFrame**     aFrame)
{
  // override, since we don't want children to get events
  return nsFrame::GetFrameForPoint(aPresContext, aPoint, aWhichLayer, aFrame);
}





	
NS_IMETHODIMP
nsTitleBarFrame::HandleEvent(nsPresContext* aPresContext, 
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
			   nsCOMPtr<nsIDocument> document;
			   aPresContext->PresShell()->GetDocument(getter_AddRefs(document));
			   nsCOMPtr<nsIDOMWindowInternal> window(do_QueryInterface(document->GetScriptGlobalObject()));



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
nsTitleBarFrame::MouseClicked (nsPresContext* aPresContext) 
{
  // Execute the oncommand event handler.
  nsEventStatus status = nsEventStatus_eIgnore;
  nsMouseEvent event(NS_XUL_COMMAND);
  mContent->HandleDOMEvent(aPresContext, &event, nsnull, NS_EVENT_FLAG_INIT, &status);
}
