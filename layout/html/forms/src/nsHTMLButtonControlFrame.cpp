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

#include "nsHTMLContainerFrame.h"
#include "nsIFormControlFrame.h"
#include "nsHTMLParts.h"
#include "nsIFormControl.h"
#include "nsFormFrame.h"

#include "nsIRenderingContext.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsIStyleContext.h"
#include "nsLeafFrame.h"
#include "nsCSSRendering.h"
#include "nsHTMLIIDs.h"
#include "nsISupports.h"
#include "nsHTMLAtoms.h"
#include "nsIImage.h"
#include "nsHTMLImage.h"
#include "nsStyleUtil.h"
#include "nsDOMEvent.h"
#include "nsStyleConsts.h"
#include "nsIHTMLAttributes.h"
#include "nsGenericHTMLElement.h"
#include "nsIWidget.h"
#include "nsRepository.h"
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsViewsCID.h"

//Enumeration of possible mouse states used to detect mouse clicks
enum nsMouseState {
  eMouseNone,
  eMouseEnter,
  eMouseDown,
  eMouseUp
};

static NS_DEFINE_IID(kIFormControlIID, NS_IFORMCONTROL_IID);
static NS_DEFINE_IID(kIFormControlFrameIID, NS_IFORMCONTROLFRAME_IID);
static NS_DEFINE_IID(kViewCID, NS_VIEW_CID);
static NS_DEFINE_IID(kIViewIID, NS_IVIEW_IID);

class nsHTMLButtonControlFrame : public nsHTMLContainerFrame,
                                 public nsIFormControlFrame 
{
public:
  nsHTMLButtonControlFrame(nsIContent* aContent, nsIFrame* aParentFrame);

  NS_IMETHOD  QueryInterface(const nsIID& aIID, void** aInstancePtr);

  NS_IMETHOD Paint(nsIPresContext& aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect& aDirtyRect);

  NS_IMETHOD Reflow(nsIPresContext&          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  NS_IMETHOD HandleEvent(nsIPresContext& aPresContext, 
                         nsGUIEvent* aEvent,
                         nsEventStatus& aEventStatus);

  NS_IMETHOD Init(nsIPresContext& aPresContext, nsIFrame* aChildList);

  virtual PRBool IsSuccessful();
  NS_IMETHOD GetType(PRInt32* aType) const;
  NS_IMETHOD GetName(nsString* aName);
  NS_IMETHOD GetValue(nsString* aName);
  virtual PRInt32 GetMaxNumValues();
  virtual PRBool  GetNamesValues(PRInt32 aMaxNumValues, PRInt32& aNumValues,
                                 nsString* aValues, nsString* aNames);
  virtual void MouseClicked(nsIPresContext* aPresContext);
  virtual void Reset() {};
  virtual void SetFormFrame(nsFormFrame* aFormFrame) { mFormFrame = aFormFrame; }

  void GetDefaultLabel(nsString& aLabel);

protected:
  virtual  ~nsHTMLButtonControlFrame();
  NS_IMETHOD_(nsrefcnt) AddRef(void);
  NS_IMETHOD_(nsrefcnt) Release(void);

  PRIntn GetSkipSides() const;
  PRBool mInline;
  nsFormFrame* mFormFrame;
  nsMouseState mLastMouseState;
  nsCursor mPreviousCursor;
};

nsresult
NS_NewHTMLButtonControlFrame(nsIContent* aContent,
                         nsIFrame*   aParent,
                         nsIFrame*&  aResult)
{
  aResult = new nsHTMLButtonControlFrame(aContent, aParent);
  if (nsnull == aResult) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

nsHTMLButtonControlFrame::nsHTMLButtonControlFrame(nsIContent* aContent,
                                           nsIFrame* aParentFrame)
  : nsHTMLContainerFrame(aContent, aParentFrame)
{
  mInline = PR_TRUE;
  mLastMouseState = eMouseNone;
  mPreviousCursor = eCursor_standard;
}

nsHTMLButtonControlFrame::~nsHTMLButtonControlFrame()
{
}

nsrefcnt nsHTMLButtonControlFrame::AddRef(void)
{
  NS_WARNING("not supported");
  return 1;
}

nsrefcnt nsHTMLButtonControlFrame::Release(void)
{
  NS_WARNING("not supported");
  return 1;
}

nsresult
nsHTMLButtonControlFrame::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  NS_PRECONDITION(0 != aInstancePtr, "null ptr");
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kIFormControlFrameIID)) {
    *aInstancePtr = (void*) ((nsIFormControlFrame*) this);
    return NS_OK;
  }
  return nsHTMLContainerFrame::QueryInterface(aIID, aInstancePtr);
}

void
nsHTMLButtonControlFrame::GetDefaultLabel(nsString& aString) 
{
  PRInt32 type;
  GetType(&type);
  if (NS_FORM_BUTTON_BUTTON == type) {
    aString = "Button";
  } 
  else if (NS_FORM_BUTTON_RESET == type) {
    aString = "Reset";
  } 
  else if (NS_FORM_BUTTON_SUBMIT == type) {
    aString = "Submit";
  } 
}


PRInt32
nsHTMLButtonControlFrame::GetMaxNumValues() 
{
  return 1;
}


PRBool
nsHTMLButtonControlFrame::GetNamesValues(PRInt32 aMaxNumValues, PRInt32& aNumValues,
                                     nsString* aValues, nsString* aNames)
{
  nsAutoString name;
  nsresult result = GetName(&name);
  if ((aMaxNumValues <= 0) || (NS_CONTENT_ATTR_HAS_VALUE != result)) {
    return PR_FALSE;
  }

  PRInt32 type;
  GetType(&type);
  nsAutoString value;
  nsresult valResult = GetValue(&value);

  if (NS_CONTENT_ATTR_HAS_VALUE == valResult) {
    aValues[0] = value;
    aNames[0]  = name;
    aNumValues = 1;
    return PR_TRUE;
  } else {
    aNumValues = 0;
    return PR_FALSE;
  }
}


NS_IMETHODIMP
nsHTMLButtonControlFrame::GetType(PRInt32* aType) const
{
  nsresult result = NS_FORM_NOTOK;
  if (mContent) {
    nsIFormControl* formControl = nsnull;
    result = mContent->QueryInterface(kIFormControlIID, (void**)&formControl);
    if ((NS_OK == result) && formControl) {
      result = formControl->GetType(aType);
      NS_RELEASE(formControl);
    }
  }
  return result;
}

NS_IMETHODIMP
nsHTMLButtonControlFrame::GetName(nsString* aResult)
{
  nsresult result = NS_FORM_NOTOK;
  if (mContent) {
    nsIHTMLContent* formControl = nsnull;
    result = mContent->QueryInterface(kIHTMLContentIID, (void**)&formControl);
    if ((NS_OK == result) && formControl) {
      nsHTMLValue value;
      result = formControl->GetAttribute(nsHTMLAtoms::name, value);
      if (NS_CONTENT_ATTR_HAS_VALUE == result) {
        if (eHTMLUnit_String == value.GetUnit()) {
          value.GetStringValue(*aResult);
        }
      }
      NS_RELEASE(formControl);
    }
  }
  return result;
}

NS_IMETHODIMP
nsHTMLButtonControlFrame::GetValue(nsString* aResult)
{
  nsresult result = NS_FORM_NOTOK;
  if (mContent) {
    nsIHTMLContent* formControl = nsnull;
    result = mContent->QueryInterface(kIHTMLContentIID, (void**)&formControl);
    if ((NS_OK == result) && formControl) {
      nsHTMLValue value;
      result = formControl->GetAttribute(nsHTMLAtoms::value, value);
      if (NS_CONTENT_ATTR_HAS_VALUE == result) {
        if (eHTMLUnit_String == value.GetUnit()) {
          value.GetStringValue(*aResult);
        }
      }
      NS_RELEASE(formControl);
    }
  }
  return result;
}

PRBool
nsHTMLButtonControlFrame::IsSuccessful()
{
  nsAutoString name;
  return (NS_CONTENT_ATTR_HAS_VALUE == GetName(&name));
}



void
nsHTMLButtonControlFrame::MouseClicked(nsIPresContext* aPresContext) 
{
  if (!mFormFrame) {
    return;
  }

  PRInt32 type;
  GetType(&type);
  if (nsnull != mFormFrame) {
    if (NS_FORM_BUTTON_RESET == type) {
      //Send DOM event
      nsEventStatus mStatus;
      nsEvent mEvent;
      mEvent.eventStructType = NS_EVENT;
      mEvent.message = NS_FORM_RESET;
      mContent->HandleDOMEvent(*aPresContext, &mEvent, nsnull, DOM_EVENT_INIT, mStatus); 

      mFormFrame->OnReset();
    } 
    else if (NS_FORM_BUTTON_SUBMIT == type) {
      //Send DOM event
      nsEventStatus mStatus;
      nsEvent mEvent;
      mEvent.eventStructType = NS_EVENT;
      mEvent.message = NS_FORM_SUBMIT;
      mContent->HandleDOMEvent(*aPresContext, &mEvent, nsnull, DOM_EVENT_INIT, mStatus); 

      mFormFrame->OnSubmit(aPresContext, this);
    }
  } 
}

NS_METHOD 
nsHTMLButtonControlFrame::HandleEvent(nsIPresContext& aPresContext, 
                                      nsGUIEvent* aEvent,
                                      nsEventStatus& aEventStatus)
{
  static int foo = 0;

  aEventStatus = nsEventStatus_eIgnore;
  nsresult result = NS_OK;

  nsIView* view;
  GetView(view);
  if (view) {
    nsIViewManager* viewMan;
    view->GetViewManager(viewMan);
    if (viewMan) {
      nsIView* grabber;
      viewMan->GetMouseEventGrabber(grabber);
      if ((grabber == view) || (nsnull == grabber)) {
        nsIWidget* window;
        PRBool ignore;

        switch (aEvent->message) {
        case NS_MOUSE_ENTER: // not implemented yet on frames
	        mLastMouseState = eMouseEnter;
	        break;
        case NS_MOUSE_LEFT_BUTTON_DOWN:
	        mLastMouseState = eMouseDown;
	        //mLastMouseState = (eMouseEnter == mLastMouseState) ? eMouseDown : eMouseNone;
	        break;
        case NS_MOUSE_MOVE:
printf ("%d mRect=(%d,%d,%d,%d), x=%d, y=%d \n", foo, mRect.x, mRect.y, mRect.width, mRect.height, aEvent->point.x, aEvent->point.y);
          if ((aEvent->point.x <= mRect.width) && (aEvent->point.y <= mRect.height)) { // mouse enter, frames don't support enter yet
            if (nsnull == grabber) { 
printf("%d enter\n", foo);
              viewMan->GrabMouseEvents(view, ignore);
              GetWindow(window);
              if (window) {
                mPreviousCursor = window->GetCursor();
                window->SetCursor(eCursor_standard); 
                //window->SetCursor(eCursor_sizeWE); //eCursor_arrow_west_plus); 
                NS_RELEASE(window);
              }
            }
          } else { // mouse exit, frames don't support exit yet 
            viewMan->GrabMouseEvents(nsnull, ignore); 
printf("%d exit\n", foo);
            GetWindow(window);
            if (window) {
              window->SetCursor(mPreviousCursor);  // eCursor_sizeWE 
              NS_RELEASE(window);
            }
          }
          break;
        case NS_MOUSE_LEFT_BUTTON_UP:
	        if (eMouseDown == mLastMouseState) {
            nsEventStatus status = nsEventStatus_eIgnore;
            nsMouseEvent event;
            event.eventStructType = NS_MOUSE_EVENT;
            event.message = NS_MOUSE_LEFT_CLICK;
            mContent->HandleDOMEvent(aPresContext, &event, nsnull, DOM_EVENT_INIT, status);
        
            if (nsEventStatus_eConsumeNoDefault != status) {
              MouseClicked(&aPresContext);
            }
	        } 
	        mLastMouseState = eMouseEnter;
	        break;
        case NS_MOUSE_EXIT: // doesn't work for frames, yet
	        break;
        }
        aEventStatus = nsEventStatus_eConsumeNoDefault;
        NS_RELEASE(viewMan);
        return NS_OK;
      }
    }
  }
  if (nsnull == mFirstChild) { // XXX see corresponding hack in nsHTMLContainerFrame::DeleteFrame
    aEventStatus = nsEventStatus_eConsumeNoDefault;
    return NS_OK;
  } else {
    return nsHTMLContainerFrame::HandleEvent(aPresContext, aEvent, aEventStatus);
  }
}


NS_IMETHODIMP
nsHTMLButtonControlFrame::Init(nsIPresContext& aPresContext, nsIFrame* aChildList)
{
  // cache our display type
  const nsStyleDisplay* styleDisplay;
  GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&) styleDisplay);
  mInline = (NS_STYLE_DISPLAY_BLOCK != styleDisplay->mDisplay);

  PRUint8 flags = (mInline) ? NS_BODY_SHRINK_WRAP : 0;
  NS_NewBodyFrame(mContent, this, mFirstChild, flags);

  // Resolve style and set the style context
  nsIStyleContext* styleContext =
    aPresContext.ResolveStyleContextFor(mContent, this);              
  mFirstChild->SetStyleContext(&aPresContext, styleContext);
  NS_RELEASE(styleContext);                                           

  // Set the geometric and content parent for each of the child frames
  for (nsIFrame* frame = aChildList; nsnull != frame; frame->GetNextSibling(frame)) {
    frame->SetGeometricParent(mFirstChild);
    frame->SetContentParent(mFirstChild);
  }

  // Queue up the frames for the inline frame
  return mFirstChild->Init(aPresContext, aChildList);
}

NS_IMETHODIMP
nsHTMLButtonControlFrame::Paint(nsIPresContext& aPresContext,
                                nsIRenderingContext& aRenderingContext,
                                const nsRect& aDirtyRect)
{
  return nsHTMLContainerFrame::Paint(aPresContext, aRenderingContext, aDirtyRect);
}

NS_IMETHODIMP 
nsHTMLButtonControlFrame::Reflow(nsIPresContext& aPresContext,
                               nsHTMLReflowMetrics& aDesiredSize,
                               const nsHTMLReflowState& aReflowState,
                               nsReflowStatus& aStatus)
{
  nsSize availSize(aReflowState.maxSize);

  // reflow the child
  nsHTMLReflowState reflowState(aPresContext, mFirstChild, aReflowState, availSize);
  ReflowChild(mFirstChild, aPresContext, aDesiredSize, reflowState, aStatus);

  // get border and padding
  const nsStyleSpacing* spacing =
    (const nsStyleSpacing*)mStyleContext->GetStyleData(eStyleStruct_Spacing);
  nsMargin borderPadding;
  spacing->CalcBorderPaddingFor(this, borderPadding);

  // Place the child
  nsRect rect = nsRect(borderPadding.left, borderPadding.top, aDesiredSize.width, aDesiredSize.height);
  mFirstChild->SetRect(rect);

  // add in our border and padding to the size of the child
  aDesiredSize.width  += borderPadding.left + borderPadding.right;
  aDesiredSize.height += borderPadding.top + borderPadding.bottom;

  // adjust our max element size, if necessary
  if (aDesiredSize.maxElementSize) {
    aDesiredSize.maxElementSize->width  += borderPadding.left + borderPadding.right;
    aDesiredSize.maxElementSize->height += borderPadding.top + borderPadding.bottom;
  }
  aDesiredSize.ascent  = aDesiredSize.height;
  aDesiredSize.descent = 0;

  // create our view, we need a view to grab the mouse away from children frames
  nsIView* view;
  GetView(view);
  if (!view) {
    nsresult result = nsRepository::CreateInstance(kViewCID, nsnull, kIViewIID,
                                                  (void **)&view);
	  nsIPresShell   *presShell = aPresContext.GetShell();     
	  nsIViewManager *viewMan   = presShell->GetViewManager();  
    NS_RELEASE(presShell);

    nsIFrame* parWithView;
	  nsIView *parView;
    GetParentWithView(parWithView);
	  parWithView->GetView(parView);
    nsRect boundBox(0, 0, aDesiredSize.width, aDesiredSize.height); 
    result = view->Init(viewMan, boundBox, parView, nsnull);
    viewMan->InsertChild(parView, view, 0);
    SetView(view);
    NS_RELEASE(viewMan);
  }

  aStatus = NS_FRAME_COMPLETE;
  return NS_OK;
}

PRIntn
nsHTMLButtonControlFrame::GetSkipSides() const
{
  return 0;
}


