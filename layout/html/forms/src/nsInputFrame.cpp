/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsInput.h"
#include "nsInputFrame.h"
#include "nsHTMLParts.h"
#include "nsHTMLTagContent.h"
#include "nsIRenderingContext.h"
#include "nsIPresShell.h"
#include "nsIPresContext.h"
#include "nsIStyleContext.h"
#include "nsLeafFrame.h"
#include "nsCSSRendering.h"
#include "nsHTMLIIDs.h"
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsCoord.h"
#include "nsWidgetsCID.h"
#include "nsViewsCID.h"
#include "nsRepository.h"
#include "nsGUIEvent.h"

#include "nsIButton.h"  // remove this when GetCID is pure virtual
#include "nsICheckButton.h"  //remove this
#include "nsITextWidget.h"  //remove this
#include "nsISupports.h"

nsInputFrame::nsInputFrame(nsIContent* aContent,
                           PRInt32 aIndexInParent,
                           nsIFrame* aParentFrame)
  : nsLeafFrame(aContent, aIndexInParent, aParentFrame)
{
  mCacheBounds.width  = 0;
  mCacheBounds.height = 0;
  mLastMouseState = eMouseNone;
}

nsInputFrame::~nsInputFrame()
{
}

// XXX it would be cool if form element used our rendering sw, then
// they could be blended, and bordered, and so on...
NS_METHOD
nsInputFrame::Paint(nsIPresContext& aPresContext,
                    nsIRenderingContext& aRenderingContext,
                    const nsRect& aDirtyRect)
{
  static NS_DEFINE_IID(kIWidgetIID, NS_IWIDGET_IID);

  nsPoint offset;
  nsIView *parent;
  GetOffsetFromView(offset, parent);
  if (nsnull == parent) {  // a problem
	  NS_ASSERTION(0, "parent view was null\n");
  } else {
	  nsIView* view;
	  GetView(view);
	  float t2p = aPresContext.GetTwipsToPixels();
	  //nsIWidget *widget = view->GetWindow();
	  nsIWidget* widget;
	  nsresult result = GetWidget(view, &widget);
	  if (NS_OK == result) {
      nsRect  vBounds;
      view->GetBounds(vBounds);

      if ((vBounds.x != offset.x) || (vBounds.y != offset.y))
        view->SetPosition(offset.x, offset.y);

	    // initially the widget was created as hidden
	    if (nsViewVisibility_kHide == view->GetVisibility())
	      view->SetVisibility(nsViewVisibility_kShow);
	  } else {
	    NS_ASSERTION(0, "could not get widget");
	  }
	  NS_IF_RELEASE(widget);
	  NS_RELEASE(view);
    NS_RELEASE(parent);
  }
  return NS_OK;
}

PRBool 
nsInputFrame::BoundsAreSet()
{
  if ((0 != mCacheBounds.width) || (0 != mCacheBounds.height)) {
    return PR_TRUE;
  } else {
	return PR_FALSE;
  }
}


void 
nsInputFrame::GetDesiredSize(nsIPresContext* aPresContext,
                             nsReflowMetrics& aDesiredSize,
                             const nsSize& aMaxSize)
{
  if (!BoundsAreSet()) {
    PreInitializeWidget(aPresContext, mCacheBounds); // sets mCacheBounds
  }
  aDesiredSize.width  = mCacheBounds.width;
  aDesiredSize.height = mCacheBounds.height;
}

void 
nsInputFrame::PreInitializeWidget(nsIPresContext* aPresContext, 
								  nsSize& aBounds)
{
  // this should always be overridden by subclasses, but if not make it 10 x 10
  float p2t = aPresContext->GetPixelsToTwips();
  aBounds.width  = (int)(10 * p2t);
  aBounds.height = (int)(10 * p2t);
}

NS_METHOD
nsInputFrame::ResizeReflow(nsIPresContext* aPresContext,
                          nsReflowMetrics& aDesiredSize,
                          const nsSize& aMaxSize,
                          nsSize* aMaxElementSize,
                          ReflowStatus& aStatus)
{
  // XXX add in code to check for width/height being set via css
  // and if set use them instead of calling GetDesiredSize.

  nsIView* view;
  GetView(view);
  if (nsnull == view) {

    static NS_DEFINE_IID(kViewCID, NS_VIEW_CID);
    static NS_DEFINE_IID(kIViewIID, NS_IVIEW_IID);

    nsresult result = 
	  NSRepository::CreateInstance(kViewCID, nsnull, kIViewIID, (void **)&view);// need to release
	  if (NS_OK != result) {
	    NS_ASSERTION(0, "Could not create view for button"); 
      aStatus = frNotComplete;
      return NS_OK;
	  }
	  nsIPresShell   *presShell = aPresContext->GetShell();     // need to release
	  nsIViewManager *viewMan   = presShell->GetViewManager();  // need to release

	  PreInitializeWidget(aPresContext, mCacheBounds); // sets mCacheBounds
	  //float t2p = aPresContext->GetTwipsToPixels();
	  nsRect boundBox(0, 0, mCacheBounds.width, mCacheBounds.height); 

    nsIFrame* parWithView;
	  nsIView *parView;

    GetParentWithView(parWithView);
	  parWithView->GetView(parView);

	  const nsIID id = GetCID();
	  // initialize the view as hidden since we don't know the (x,y) until Paint
    result = view->Init(viewMan, boundBox, parView, &id, nsnull, 0, nsnull,
		                1.0f, nsViewVisibility_kHide);
    if (NS_OK != result) {
	    NS_ASSERTION(0, "widget initialization failed"); 
      aStatus = frNotComplete;
      return NS_OK;
	  }

	  // set the content's widget, so it can get content modified by the widget
	  nsIWidget *widget;
	  result = GetWidget(view, &widget);
	  if (NS_OK == result) {
      nsInput* content = (nsInput *)mContent; // change this cast to QueryInterface 
      content->SetWidget(widget);
	    NS_IF_RELEASE(widget);
	  } else {
	    NS_ASSERTION(0, "could not get widget");
	  }

    viewMan->InsertChild(parView, view, 0);
	  SetView(view);
	  InitializeWidget(view);
	  
    NS_IF_RELEASE(parView);
    NS_IF_RELEASE(view);
	  NS_IF_RELEASE(viewMan);  
	  NS_IF_RELEASE(presShell); 
  }
  aDesiredSize.width   = mCacheBounds.width;
  aDesiredSize.height  = mCacheBounds.height;
  aDesiredSize.ascent  = mCacheBounds.height;
  aDesiredSize.descent = 0;

  if (nsnull != aMaxElementSize) {
    aMaxElementSize->width = aDesiredSize.width;
	  aMaxElementSize->height = aDesiredSize.height;
  }
    
  aStatus = frComplete;
  return NS_OK;
}

nsresult
nsInputFrame::GetWidget(nsIView* aView, nsIWidget** aWidget)
{
  const nsIID id = GetIID();
  if (NS_OK == aView->QueryInterface(id, (void**)aWidget)) {
    return NS_OK;
  } else {
	NS_ASSERTION(0, "The widget interface is invalid");  // need to print out more details of the widget
	return NS_NOINTERFACE;
  }
}

const nsIID
nsInputFrame::GetIID()
{
  static NS_DEFINE_IID(kButtonIID, NS_IBUTTON_IID);
  return kButtonIID;
}
  
const nsIID
nsInputFrame::GetCID()
{
  static NS_DEFINE_IID(kButtonCID, NS_BUTTON_CID);
  return kButtonCID;
}

NS_METHOD nsInputFrame::HandleEvent(nsIPresContext& aPresContext, 
                                    nsGUIEvent* aEvent,
                                    nsEventStatus& aEventStatus)
{
	// make sure that the widget in the event is this
  static NS_DEFINE_IID(kSupportsIID, NS_ISUPPORTS_IID);
  nsIWidget* thisWidget;
  nsIView*   view;
  GetView(view);
  nsresult result = GetWidget(view, &thisWidget);
  nsISupports* thisWidgetSup;
  result = thisWidget->QueryInterface(kSupportsIID, (void **)&thisWidgetSup);
  nsISupports* eventWidgetSup;
  result = aEvent->widget->QueryInterface(kSupportsIID, (void **)&eventWidgetSup);

  PRBool isOurEvent = (thisWidgetSup == eventWidgetSup) ? PR_TRUE : PR_FALSE;
    
  NS_RELEASE(eventWidgetSup);
  NS_RELEASE(thisWidgetSup);
  NS_IF_RELEASE(view);
  NS_IF_RELEASE(thisWidget);

	if (!isOurEvent) {
		aEventStatus = nsEventStatus_eIgnore;
    return NS_OK;
	}

  switch (aEvent->message) {
    case NS_MOUSE_ENTER:
	    mLastMouseState = eMouseEnter;
	    break;
    case NS_MOUSE_LEFT_BUTTON_DOWN:
	    mLastMouseState = 
	      (eMouseEnter == mLastMouseState) ? eMouseDown : eMouseNone;
	    break;
    case NS_MOUSE_LEFT_BUTTON_UP:
	    if (eMouseDown == mLastMouseState) {
	      /*nsIView* view = GetView();
	      nsIWidget *widget = view->GetWindow();
		  widget->SetFocus();
		  NS_RELEASE(widget);
	      NS_RELEASE(view); */
		  MouseClicked();
		  //return PR_FALSE;
	    } 
	    mLastMouseState = eMouseEnter;
	    break;
    case NS_MOUSE_EXIT:
	    mLastMouseState = eMouseNone;
	    break;
  }
  aEventStatus = nsEventStatus_eIgnore;
  return NS_OK;
}






