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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsCOMPtr.h"
#include "nsHTMLAtoms.h"
#include "nsWidgetsCID.h"
#include "nsViewsCID.h"
#include "nsIDeviceContext.h"
#include "nsIPresShell.h"
#include "nsIViewManager.h"
#include "nsIComponentManager.h"
#include "nsIFormControl.h"
#include "nsFormFrame.h"
#include "nsNativeFormControlFrame.h"

static NS_DEFINE_IID(kIWidgetIID, NS_IWIDGET_IID);
static NS_DEFINE_IID(kViewCID, NS_VIEW_CID);
static NS_DEFINE_IID(kIViewIID, NS_IVIEW_IID);


nsNativeFormControlFrame::nsNativeFormControlFrame()
  : nsFormControlFrame()
{
  mLastMouseState	= eMouseNone;
  mWidget					= nsnull;
}

nsNativeFormControlFrame::~nsNativeFormControlFrame()
{
  NS_IF_RELEASE(mWidget);
}


NS_METHOD
nsNativeFormControlFrame::Reflow(nsIPresContext*          aPresContext,
                           nsHTMLReflowMetrics&     aDesiredSize,
                           const nsHTMLReflowState& aReflowState,
                           nsReflowStatus&          aStatus)
{
	if (mDidInit) {
		return nsFormControlFrame::Reflow(aPresContext, aDesiredSize, aReflowState, aStatus);
	}

  nsresult result = NS_OK;

  nsCOMPtr<nsIDeviceContext> dx;
  aPresContext->GetDeviceContext(getter_AddRefs(dx));
  PRBool requiresWidget = PR_TRUE;
 
    // Checkto see if the device context supports widgets at all
  if (dx) { 
    dx->SupportsNativeWidgets(requiresWidget);
  }

  nsWidgetRendering mode;
  aPresContext->GetWidgetRenderingMode(&mode);
  if ((eWidgetRendering_Gfx == mode) || (eWidgetRendering_PartialGfx == mode)) {
      // Check with the frame to see if requires a widget to render
    if (PR_TRUE == requiresWidget) {
      RequiresWidget(requiresWidget);   
    }
  }

	if (! requiresWidget) {
		return nsFormControlFrame::Reflow(aPresContext, aDesiredSize, aReflowState, aStatus);
	}

  // add ourself as an nsIFormControlFrame
  if (!mFormFrame && (eReflowReason_Initial == aReflowState.reason)) {
    nsFormFrame::AddFormControlFrame(aPresContext, *NS_STATIC_CAST(nsIFrame*, this));
  }

  GetDesiredSize(aPresContext, aReflowState, aDesiredSize, mWidgetSize);

	{
	  nsCOMPtr<nsIPresShell> presShell;
	  aPresContext->GetShell(getter_AddRefs(presShell));
	  nsCOMPtr<nsIViewManager> viewMan;
	  presShell->GetViewManager(getter_AddRefs(viewMan));
	  nsRect boundBox(0, 0, aDesiredSize.width, aDesiredSize.height); 

	  // absolutely positioned controls already have a view but not a widget
	  nsIView* view = nsnull;
	  GetView(aPresContext, &view);
	  if (nsnull == view) {
	    result = nsComponentManager::CreateInstance(kViewCID, nsnull, kIViewIID, (void **)&view);
	    if (!NS_SUCCEEDED(result)) {
	      NS_ASSERTION(0, "Could not create view for form control"); 
	      aStatus = NS_FRAME_NOT_COMPLETE;
	      return result;
	    }

	    nsIFrame* parWithView;
	    nsIView *parView;

	    GetParentWithView(aPresContext, &parWithView);
	    parWithView->GetView(aPresContext, &parView);

	    // initialize the view as hidden since we don't know the (x,y) until Paint
	    result = view->Init(viewMan, boundBox, parView, nsnull, nsViewVisibility_kHide);
	    if (NS_OK != result) {
	      NS_ASSERTION(0, "view initialization failed"); 
	      aStatus = NS_FRAME_NOT_COMPLETE;
	      return NS_OK;
	    }

	    viewMan->InsertChild(parView, view, 0);
	    SetView(aPresContext, view);
	  }

	  PRInt32 type;
	  GetType(&type);
	  const nsIID& id = GetCID();

	  if ((NS_FORM_INPUT_HIDDEN != type) && (PR_TRUE == requiresWidget)) {
	    // Do the following only if a widget is created
	    nsWidgetInitData* initData = GetWidgetInitData(aPresContext); // needs to be deleted
	    view->CreateWidget(id, initData);

	    if (nsnull != initData) {
	      delete(initData);
	    }

	    // set our widget
	    result = GetWidget(view, &mWidget);
	    if ((PR_FALSE == NS_SUCCEEDED(result)) || (nsnull == mWidget)) { // keep the ref on mWidget
	      NS_ASSERTION(0, "could not get widget");
	    }
	  }

	  PostCreateWidget(aPresContext, aDesiredSize.width, aDesiredSize.height);
	  mDidInit = PR_TRUE;

	  if ((aDesiredSize.width != boundBox.width) || (aDesiredSize.height != boundBox.height)) {
	    viewMan->ResizeView(view, aDesiredSize.width, aDesiredSize.height);
	  }
	}
  
  aDesiredSize.ascent = aDesiredSize.height;
  aDesiredSize.descent = 0;

  if (nsnull != aDesiredSize.maxElementSize) {
    //XXX aDesiredSize.AddBorderPaddingToMaxElementSize(borderPadding);
  }
    
  aStatus = NS_FRAME_COMPLETE;
  return NS_OK;
}


NS_IMETHODIMP
nsNativeFormControlFrame::AttributeChanged(nsIPresContext* aPresContext,
                                     nsIContent*     aChild,
                                     PRInt32         aNameSpaceID,
                                     nsIAtom*        aAttribute,
                                     PRInt32         aHint)
{
  if (mWidget) {
    if (nsHTMLAtoms::disabled == aAttribute) {
      mWidget->Enable(!nsFormFrame::GetDisabled(this));
    }
  }
  return NS_OK;
}


void 
nsNativeFormControlFrame::SetColors(nsIPresContext* aPresContext)
{
  if (mWidget) {
    nsCompatibility mode;
    aPresContext->GetCompatibilityMode(&mode);
    const nsStyleColor* color =
      (const nsStyleColor*)mStyleContext->GetStyleData(eStyleStruct_Color);
    if (nsnull != color) {
      if (!(NS_STYLE_BG_COLOR_TRANSPARENT & color->mBackgroundFlags)) {
        mWidget->SetBackgroundColor(color->mBackgroundColor);
#ifdef bug_1021_closed
      } else if (eCompatibility_NavQuirks == mode) {
#else
      } else {
#endif
        mWidget->SetBackgroundColor(NS_RGB(0xFF, 0xFF, 0xFF));
      }
      mWidget->SetForegroundColor(color->mColor);
    }
  }
}


// native widgets don't need to repaint
void 
nsNativeFormControlFrame::SetFocus(PRBool aOn, PRBool aRepaint)
{
  if (mWidget) {
    if (aOn) {
      mWidget->SetFocus();
    }
    else {
      //Unsetting of focus on native widget is accomplised
      //by pushing focus up to its parent
      nsIWidget *parentWidget = mWidget->GetParent();
      if (parentWidget) {
        parentWidget->SetFocus();
        NS_RELEASE(parentWidget);
      }
    }
  }
}

nsresult
nsNativeFormControlFrame::GetWidget(nsIWidget** aWidget)
{
  if (mWidget) {
    NS_ADDREF(mWidget);
    *aWidget = mWidget;
    mWidget->Enable(!nsFormFrame::GetDisabled(this));
    return NS_OK;
  } else {
    *aWidget = nsnull;
    return NS_FORM_NOTOK;
  }
}

nsresult
nsNativeFormControlFrame::GetWidget(nsIView* aView, nsIWidget** aWidget)
{
  nsIWidget*  widget;
  aView->GetWidget(widget);
  nsresult    result;

  if (nsnull == widget) {
    *aWidget = nsnull;
    result = NS_ERROR_FAILURE;

  } else {
    result =  widget->QueryInterface(kIWidgetIID, (void**)aWidget); // keep the ref
    if (NS_FAILED(result)) {
      NS_ASSERTION(0, "The widget interface is invalid");  // need to print out more details of the widget
    }
    NS_RELEASE(widget);
  }

  return result;
}


NS_METHOD nsNativeFormControlFrame::HandleEvent(nsIPresContext* aPresContext, 
                                          nsGUIEvent* aEvent,
                                          nsEventStatus* aEventStatus)
{
  NS_ENSURE_ARG_POINTER(aEventStatus);
  if (nsnull == mWidget) {
    return nsFormControlFrame::HandleEvent(aPresContext, aEvent, aEventStatus);
  }

  if (nsEventStatus_eConsumeNoDefault == *aEventStatus) {
    return NS_OK;
  }

	// make sure that the widget in the event is this input
	// XXX if there is no view, it could be an image button. Unfortunately,
	// every image button will get every event.
	nsIView* view;
	GetView(aPresContext, &view);
	if (view) {
	  if (mWidget != aEvent->widget) {
	    *aEventStatus = nsEventStatus_eIgnore;
	    return NS_OK;
	  }
	}

  // if not native then use the NS_MOUSE_LEFT_CLICK to see if pressed
  // unfortunately native widgets don't seem to handle this right. 
  // so use the old code for native stuff. -EDV

	//printf(" %d %d %d %d (%d,%d) \n", this, aEvent->widget, aEvent->widgetSupports, 
	//       aEvent->message, aEvent->point.x, aEvent->point.y);

	PRInt32 type;
	GetType(&type);
	switch (aEvent->message) {
	  case NS_MOUSE_ENTER_SYNTH:
	    mLastMouseState = eMouseEnter;
	    break;

	  case NS_MOUSE_LEFT_BUTTON_DOWN:
            mLastMouseState = eMouseDown;
	    break;

	  case NS_MOUSE_LEFT_BUTTON_UP:
	    if (eMouseDown == mLastMouseState) {
	      MouseClicked(aPresContext);
	    } 
	    mLastMouseState = eMouseEnter;
	    break;

	  case NS_MOUSE_EXIT_SYNTH:
	    mLastMouseState = eMouseNone;
	    break;

	  case NS_CONTROL_CHANGE:
	    ControlChanged(aPresContext);
	    break;

		case NS_KEY_DOWN:
			return nsFormControlFrame::HandleEvent(aPresContext, aEvent, aEventStatus);
	}

  *aEventStatus = nsEventStatus_eConsumeDoDefault;
  return NS_OK;
}

