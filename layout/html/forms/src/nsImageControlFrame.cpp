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
#include "nsCOMPtr.h"
#include "nsImageFrame.h"
#include "nsFormControlHelper.h"
#include "nsIFormControlFrame.h"
#include "nsIFormControl.h"
#include "nsHTMLParts.h"
#include "nsIRenderingContext.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsIStyleContext.h"
#include "nsLeafFrame.h"
#include "nsCSSRendering.h"
#include "nsHTMLIIDs.h"
#include "nsIViewManager.h"
#include "nsISupports.h"
#include "nsHTMLAtoms.h"
#include "nsIView.h"
#include "nsViewsCID.h"
#include "nsIDeviceContext.h"
#include "nsIFontMetrics.h"
#include "nsIImage.h"
#include "nsStyleUtil.h"
#include "nsDOMEvent.h"
#include "nsStyleConsts.h"
#include "nsIHTMLAttributes.h"
#include "nsGenericHTMLElement.h"
#include "nsFormFrame.h"

//Enumeration of possible mouse states used to detect mouse clicks
/*enum nsMouseState {
  eMouseNone,
  eMouseEnter,
  eMouseExit,
  eMouseDown,
  eMouseUp
};
*/
static NS_DEFINE_IID(kIFormControlFrameIID, NS_IFORMCONTROLFRAME_IID);
static NS_DEFINE_IID(kViewCID, NS_VIEW_CID);
static NS_DEFINE_IID(kIViewIID, NS_IVIEW_IID);

#define nsImageControlFrameSuper nsImageFrame
class nsImageControlFrame : public nsImageControlFrameSuper,
                            public nsIFormControlFrame 
{
public:
  nsImageControlFrame();

  NS_IMETHOD  QueryInterface(const nsIID& aIID, void** aInstancePtr);

  NS_IMETHOD SetInitialChildList(nsIPresContext& aPresContext,
                                 nsIAtom*        aListName,
                                 nsIFrame*       aChildList);

  NS_IMETHOD HandleEvent(nsIPresContext& aPresContext, 
                         nsGUIEvent* aEvent,
                         nsEventStatus& aEventStatus);

  NS_IMETHOD GetFrameName(nsString& aResult) const {
    return MakeFrameName("ImageControl", aResult);
  }

  virtual void MouseClicked(nsIPresContext* aPresContext);

  virtual void SetFormFrame(nsFormFrame* aFormFrame) { mFormFrame = aFormFrame; }

  PRBool IsSuccessful(nsIFormControlFrame* aSubmitter);

  virtual PRInt32 GetMaxNumValues();

  virtual PRBool GetNamesValues(PRInt32 aMaxNumValues, PRInt32& aNumValues,
                                nsString* aValues, nsString* aNames);

  NS_IMETHOD GetType(PRInt32* aType) const;

  NS_IMETHOD GetName(nsString* aName);

  virtual void Reset() {};

  void SetFocus(PRBool aOn, PRBool aRepaint);
  NS_IMETHOD GetFont(nsIPresContext* aPresContext, 
                    nsFont&         aFont);

  NS_IMETHOD GetFormContent(nsIContent*& aContent) const;
  virtual nscoord GetVerticalInsidePadding(float aPixToTwip,
                                           nscoord aInnerHeight) const;
  virtual nscoord GetHorizontalInsidePadding(nsIPresContext& aPresContext,
                                             float aPixToTwip, 
                                             nscoord aInnerWidth,
                                             nscoord aCharWidth) const;

  virtual nsresult RequiresWidget(PRBool &aRequiresWidget);


        // nsIFormControlFrame
  NS_IMETHOD SetProperty(nsIAtom* aName, const nsString& aValue);
  NS_IMETHOD GetProperty(nsIAtom* aName, nsString& aValue); 

protected:
  void GetTranslatedRect(nsRect& aRect); // XXX this implementation is a copy of nsHTMLButtonControlFrame
  NS_IMETHOD_(nsrefcnt) AddRef(void);
  NS_IMETHOD_(nsrefcnt) Release(void);

  nsFormFrame* mFormFrame;
  nsMouseState mLastMouseState;
  nsPoint mLastClickPoint; 
  nsCursor mPreviousCursor;
  nsRect mTranslatedRect;
  PRBool mGotFocus;
};


nsImageControlFrame::nsImageControlFrame()
{
  mLastMouseState = eMouseNone;
  mLastClickPoint = nsPoint(0,0);
  mPreviousCursor = eCursor_standard;
  mTranslatedRect = nsRect(0,0,0,0);
  mGotFocus = PR_FALSE;
}

nsresult
NS_NewImageControlFrame(nsIFrame*& aResult)
{
  aResult = new nsImageControlFrame;
  if (nsnull == aResult) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

nsresult
nsImageControlFrame::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  NS_PRECONDITION(0 != aInstancePtr, "null ptr");
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kIFormControlFrameIID)) {
    *aInstancePtr = (void*) ((nsIFormControlFrame*) this);
    return NS_OK;
  }
  return nsImageControlFrameSuper::QueryInterface(aIID, aInstancePtr);
}

nsrefcnt nsImageControlFrame::AddRef(void)
{
  NS_WARNING("not supported");
  return 1;
}

nsrefcnt nsImageControlFrame::Release(void)
{
  NS_WARNING("not supported");
  return 1;
}

NS_IMETHODIMP
nsImageControlFrame::SetInitialChildList(nsIPresContext& aPresContext,
                                         nsIAtom*        aListName,
                                         nsIFrame*       aChildList)
{
  // add ourself as an nsIFormControlFrame
  nsFormFrame::AddFormControlFrame(aPresContext, *this);
  if (nsnull == mFormFrame) {
    return NS_OK;
  }

  // create our view, we need a view to grab the mouse 
  nsIView* view;
  GetView(&view);
  if (!view) {
    nsresult result = nsComponentManager::CreateInstance(kViewCID, nsnull, kIViewIID, (void **)&view);
	  nsCOMPtr<nsIPresShell> presShell;
    aPresContext.GetShell(getter_AddRefs(presShell));
	  nsCOMPtr<nsIViewManager> viewMan;
    presShell->GetViewManager(getter_AddRefs(viewMan));

    nsIFrame* parWithView;
	  nsIView *parView;
    GetParentWithView(&parWithView);
	  parWithView->GetView(&parView);
    // the view's size is not know yet, but its size will be kept in synch with our frame.
    nsRect boundBox(0, 0, 500, 500); 
    result = view->Init(viewMan, boundBox, parView, nsnull);
    viewMan->InsertChild(parView, view, 0);
    SetView(view);

    const nsStyleColor* color = (const nsStyleColor*) mStyleContext->GetStyleData(eStyleStruct_Color);
    // set the opacity
    viewMan->SetViewOpacity(view, color->mOpacity);
  }
  return NS_OK;
}

NS_METHOD 
nsImageControlFrame::HandleEvent(nsIPresContext& aPresContext, 
                                 nsGUIEvent* aEvent,
                                 nsEventStatus& aEventStatus)
{
  if (nsFormFrame::GetDisabled(this)) { // XXX cache disabled
    return NS_OK;
  }

  aEventStatus = nsEventStatus_eIgnore;

  switch (aEvent->message) {
    case NS_MOUSE_LEFT_BUTTON_DOWN:
    {
      // Store click point for GetNamesValues
      float t2p;
      aPresContext.GetTwipsToPixels(&t2p);
      mLastClickPoint.x = NSTwipsToIntPixels(aEvent->point.x, t2p);
      mLastClickPoint.y = NSTwipsToIntPixels(aEvent->point.y, t2p);

      mLastMouseState = eMouseDown;
      mGotFocus = PR_TRUE;
      aEventStatus = nsEventStatus_eConsumeNoDefault;
      return NS_OK;
      break;
    }
    case NS_MOUSE_LEFT_BUTTON_UP: 
    {
      if (eMouseDown == mLastMouseState) {
        if (nsEventStatus_eConsumeNoDefault != aEventStatus) {
          MouseClicked(&aPresContext);
        }
      } 
      mLastMouseState = eMouseUp;
      aEventStatus = nsEventStatus_eConsumeNoDefault;
      return NS_OK;
      break;
    }
  }
  return nsImageControlFrameSuper::HandleEvent(aPresContext, aEvent,
                                               aEventStatus);
}

void 
nsImageControlFrame::SetFocus(PRBool aOn, PRBool aRepaint)
{
  mGotFocus = aOn;
  if (aRepaint) {
    nsRect rect(0, 0, mRect.width, mRect.height);
    Invalidate(rect, PR_TRUE);
  }
}

void
nsImageControlFrame::GetTranslatedRect(nsRect& aRect)
{
  nsIView* view;
  nsPoint viewOffset(0,0);
  GetOffsetFromView(viewOffset, &view);
  while (nsnull != view) {
    nsPoint tempOffset;
    view->GetPosition(&tempOffset.x, &tempOffset.y);
    viewOffset += tempOffset;
    view->GetParent(view);
  }
  aRect = nsRect(viewOffset.x, viewOffset.y, mRect.width, mRect.height);
}

NS_IMETHODIMP
nsImageControlFrame::GetType(PRInt32* aType) const
{
  *aType = NS_FORM_INPUT_IMAGE;
  return NS_OK;
}

NS_IMETHODIMP
nsImageControlFrame::GetName(nsString* aResult)
{
  if (nsnull == aResult) {
    return NS_OK;
  } else {
    return nsFormFrame::GetName(this, *aResult);
  }
}

PRBool
nsImageControlFrame::IsSuccessful(nsIFormControlFrame* aSubmitter)
{
  return (this == (aSubmitter));
}

PRInt32
nsImageControlFrame::GetMaxNumValues() 
{
  return 2;
}


PRBool
nsImageControlFrame::GetNamesValues(PRInt32 aMaxNumValues, PRInt32& aNumValues,
                                     nsString* aValues, nsString* aNames)
{
  if (aMaxNumValues <= 0) {
    return PR_FALSE;
  }

  char buf[20];
  aNumValues = 2;

  aValues[0].SetLength(0);
  sprintf(&buf[0], "%d", mLastClickPoint.x);
  aValues[0].Append(&buf[0]);

  aValues[1].SetLength(0);
  sprintf(&buf[0], "%d", mLastClickPoint.y);
  aValues[1].Append(&buf[0]);

  nsAutoString name;
  nsresult result = GetName(&name);
  if (NS_CONTENT_ATTR_HAS_VALUE == result) {
    aNames[0] = name;
    aNames[0].Append(".x");
    aNames[1] = name;
    aNames[1].Append(".y");
  } else {
    // If the Image Element has no name, simply return x and y
    // to Nav and IE compatability.
    aNames[0] = "x";
    aNames[1] = "y";
  }

  return PR_TRUE;
}


void
nsImageControlFrame::MouseClicked(nsIPresContext* aPresContext) 
{
  PRInt32 type;
  GetType(&type);

  if ((nsnull != mFormFrame) && !nsFormFrame::GetDisabled(this)) {
    nsIContent *formContent = nsnull;
    mFormFrame->GetContent(&formContent);

    nsEventStatus status;
    nsEvent event;
    event.eventStructType = NS_EVENT;
    event.message = NS_FORM_SUBMIT;
    if (nsnull != formContent) {
      formContent->HandleDOMEvent(*aPresContext, &event, nsnull, NS_EVENT_FLAG_INIT, status); 
      NS_RELEASE(formContent);
    }
    if (nsEventStatus_eConsumeNoDefault != status) {
      mFormFrame->OnSubmit(aPresContext, this);
    }
  } 
}

NS_IMETHODIMP
nsImageControlFrame::GetFont(nsIPresContext*        aPresContext, 
                             nsFont&                aFont)
{
  nsFormControlHelper::GetFont(this, aPresContext, mStyleContext, aFont);
  return NS_OK;
}

NS_IMETHODIMP
nsImageControlFrame::GetFormContent(nsIContent*& aContent) const
{
  nsIContent* content;
  nsresult    rv;

  rv = GetContent(&content);
  aContent = content;
  return rv;
}

nscoord 
nsImageControlFrame::GetVerticalInsidePadding(float aPixToTwip, 
                                               nscoord aInnerHeight) const
{
   return 0;
}

nscoord 
nsImageControlFrame::GetHorizontalInsidePadding(nsIPresContext& aPresContext,
                                               float aPixToTwip, 
                                               nscoord aInnerWidth,
                                               nscoord aCharWidth) const
{
  return 0;
}

nsresult nsImageControlFrame::RequiresWidget(PRBool& aRequiresWidget)
{
  aRequiresWidget = PR_FALSE;
  return NS_OK;
}


NS_IMETHODIMP nsImageControlFrame::SetProperty(nsIAtom* aName, const nsString& aValue)
{
  return NS_OK;
}

NS_IMETHODIMP nsImageControlFrame::GetProperty(nsIAtom* aName, nsString& aValue)
{
  return NS_OK;
}
