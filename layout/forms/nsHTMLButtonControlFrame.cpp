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
#include "nsColor.h"

//Enumeration of possible mouse states used to detect mouse clicks
enum nsMouseState {
  eMouseNone,
  eMouseEnter,
  eMouseDown,
  eMouseExit
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

  NS_IMETHOD SetInitialChildList(nsIPresContext& aPresContext,
                                 nsIAtom*        aListName,
                                 nsIFrame*       aChildList);

  virtual PRBool IsSuccessful(nsIFormControlFrame* aSubmitter);
  NS_IMETHOD GetType(PRInt32* aType) const;
  NS_IMETHOD GetName(nsString* aName);
  NS_IMETHOD GetValue(nsString* aName);
  virtual PRInt32 GetMaxNumValues();
  virtual PRBool  GetNamesValues(PRInt32 aMaxNumValues, PRInt32& aNumValues,
                                 nsString* aValues, nsString* aNames);
  virtual void MouseClicked(nsIPresContext* aPresContext);
  virtual void Reset() {};
  virtual void SetFormFrame(nsFormFrame* aFormFrame) { mFormFrame = aFormFrame; }

  void SetFocus(PRBool aOn, PRBool aRepaint);

  void GetDefaultLabel(nsString& aLabel);

  NS_IMETHOD  GetCursorAndContentAt(nsIPresContext& aPresContext,
                          const nsPoint&  aPoint,
                          nsIFrame**      aFrame,
                          nsIContent**    aContent,
                          PRInt32&        aCursor);
protected:
  virtual  ~nsHTMLButtonControlFrame();
  NS_IMETHOD_(nsrefcnt) AddRef(void);
  NS_IMETHOD_(nsrefcnt) Release(void);
  void AddToPadding(nsIPresContext& aPresContext, nsStyleUnit aStyleUnit,
                    nscoord aValue, nscoord aLength, nsStyleCoord& aStyleCoord);
  void ShiftContents(nsIPresContext& aPresContext, PRBool aDown);
  void GetTranslatedRect(nsRect& aRect);

  PRIntn GetSkipSides() const;
  PRBool mInline;
  nsFormFrame* mFormFrame;
  nsMouseState mLastMouseState;
  nsCursor mPreviousCursor;
  PRBool mGotFocus;
  nsRect mTranslatedRect;
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
  mGotFocus = PR_FALSE;
  mTranslatedRect = nsRect(0,0,0,0);
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
nsHTMLButtonControlFrame::IsSuccessful(nsIFormControlFrame* aSubmitter)
{
  if (this == (aSubmitter)) {
    nsAutoString name;
    return (NS_CONTENT_ATTR_HAS_VALUE == GetName(&name));
  } else {
    return PR_FALSE;
  }
}

void
nsHTMLButtonControlFrame::MouseClicked(nsIPresContext* aPresContext) 
{
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
    } else if (NS_FORM_BUTTON_SUBMIT == type) {
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

// XXX temporary hack code until new style rules are added
void ReflowTemp(nsIPresContext& aPresContext, nsHTMLButtonControlFrame* aFrame, nsRect& aRect)
{  
  nsHTMLReflowMetrics metrics(nsnull);
  nsSize size;
  aFrame->GetSize(size);
  nsIPresShell        *shell;
  nsIRenderingContext *acx;
  shell = aPresContext.GetShell();
  shell->CreateRenderingContext(aFrame, acx);
  NS_RELEASE(shell);
  nsIReflowCommand* cmd;
  nsresult result = NS_NewHTMLReflowCommand(&cmd, aFrame, nsIReflowCommand::ContentChanged);
  nsHTMLReflowState state(aPresContext, aFrame, *cmd, size, acx);
  //nsHTMLReflowState state(aPresContext, aFrame, eReflowReason_Initial,
  //                        size, acx);
  state.reason = eReflowReason_Incremental;
  nsReflowStatus status;
  nsDidReflowStatus didStatus;
  aFrame->WillReflow(aPresContext);
  aFrame->Reflow(aPresContext, metrics, state, status);
  aFrame->DidReflow(aPresContext, didStatus);
  NS_IF_RELEASE(acx);
  aFrame->Invalidate(aRect, PR_TRUE);
}

void 
nsHTMLButtonControlFrame::SetFocus(PRBool aOn, PRBool aRepaint)
{
  mGotFocus = aOn;
  if (aRepaint) {
    nsRect rect(0, 0, mRect.width, mRect.height);
    Invalidate(rect, PR_TRUE);
  }
}

// XXX temporary hack code until new style rules are added
void
nsHTMLButtonControlFrame::AddToPadding(nsIPresContext& aPresContext,
                                       nsStyleUnit aStyleUnit,
                                       nscoord aValue, 
                                       nscoord aLength,
                                       nsStyleCoord& aStyleCoord)
{
  if (eStyleUnit_Coord == aStyleUnit) {
    nscoord coord;
    coord = aStyleCoord.GetCoordValue();
    coord += aValue;
    aStyleCoord.SetCoordValue(coord);
  } else if (eStyleUnit_Percent == aStyleUnit) {
    float increment = ((float)aValue) / ((float)aLength);
    float percent = aStyleCoord.GetPercentValue();
    percent += increment;
    aStyleCoord.SetPercentValue(percent);
  }
}

// XXX temporary hack code until new style rules are added
void 
nsHTMLButtonControlFrame::ShiftContents(nsIPresContext& aPresContext, PRBool aDown)
{
  nsStyleSpacing* spacing =
    (nsStyleSpacing*)mStyleContext->GetStyleData(eStyleStruct_Spacing); // cast deliberate

  float p2t = aPresContext.GetPixelsToTwips();
  nscoord shift = (aDown) ? NSIntPixelsToTwips(1, p2t) : -NSIntPixelsToTwips(1, p2t);
  nsStyleCoord styleCoord;

  // alter the padding so the content shifts down and to the right one pixel
  AddToPadding(aPresContext, spacing->mPadding.GetLeftUnit(), shift, mRect.width,
               spacing->mPadding.GetLeft(styleCoord));
  spacing->mPadding.SetLeft(styleCoord);
  AddToPadding(aPresContext, spacing->mPadding.GetTopUnit(), shift, mRect.height,
               spacing->mPadding.GetTop(styleCoord));
  spacing->mPadding.SetTop(styleCoord);
  AddToPadding(aPresContext, spacing->mPadding.GetRightUnit(), -shift, mRect.width,
               spacing->mPadding.GetRight(styleCoord));
  spacing->mPadding.SetRight(styleCoord);
  AddToPadding(aPresContext, spacing->mPadding.GetBottomUnit(), -shift, mRect.height,
               spacing->mPadding.GetBottom(styleCoord));
  spacing->mPadding.SetBottom(styleCoord);

  // XXX change the border, border-left, border-right, etc are not working. Instead change inset to outset, vice versa
  for (PRInt32 i = 0; i < 4; i++) {
    spacing->mBorderStyle[i] = (spacing->mBorderStyle[i] == NS_STYLE_BORDER_STYLE_INSET) ? NS_STYLE_BORDER_STYLE_OUTSET : NS_STYLE_BORDER_STYLE_INSET;
  }

  mStyleContext->RecalcAutomaticData(&aPresContext);

  nsRect rect(0, 0, mRect.width, mRect.height);
  ReflowTemp(aPresContext, this, rect);
}

void
nsHTMLButtonControlFrame::GetTranslatedRect(nsRect& aRect)
{
  nsIView* view;
  nsPoint viewOffset(0,0);
  GetOffsetFromView(viewOffset, view);
  while (nsnull != view) {
    nsPoint tempOffset;
    view->GetPosition(&tempOffset.x, &tempOffset.y);
    viewOffset += tempOffset;
    view->GetParent(view);
  }
  aRect = nsRect(viewOffset.x, viewOffset.y, mRect.width, mRect.height);
}

            
NS_METHOD 
nsHTMLButtonControlFrame::HandleEvent(nsIPresContext& aPresContext, 
                                      nsGUIEvent* aEvent,
                                      nsEventStatus& aEventStatus)
{
  if (nsFormFrame::GetDisabled(this)) { // XXX cache disabled
    return NS_OK;
  }

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
        case NS_MOUSE_ENTER: // not implemented yet on frames, 1st mouse move simulates it
	        mLastMouseState = eMouseEnter;
	        break;
        case NS_MOUSE_LEFT_BUTTON_DOWN:
          mGotFocus = PR_TRUE;
          ShiftContents(aPresContext, PR_TRUE);
	        mLastMouseState = eMouseDown;
	        break;
        case NS_MOUSE_MOVE:
          //printf ("%d mRect=(%d,%d,%d,%d), x=%d, y=%d \n", foo++, mRect.x, mRect.y, mRect.width, mRect.height, aEvent->point.x, aEvent->point.y);
          
          if (nsnull == grabber) { // simulated mouse enter
            GetTranslatedRect(mTranslatedRect);
            //printf("%d enter\n", foo++);
            viewMan->GrabMouseEvents(view, ignore);
            GetWindow(window);
            if (window) {
              mPreviousCursor = window->GetCursor(); 
              window->SetCursor(eCursor_select); // set it to something else to work around bug 
              window->SetCursor(eCursor_standard); 
              NS_RELEASE(window);
            }
            mLastMouseState = eMouseEnter;
          // simulated mouse exit
          } else if (!mTranslatedRect.Contains(aEvent->point)) {
            //printf("%d exit\n", foo++);
            if (mLastMouseState == eMouseDown) { // 
              ShiftContents(aPresContext, PR_FALSE);
            }
            viewMan->GrabMouseEvents(nsnull, ignore); 
            GetWindow(window);
            if (window) {
              window->SetCursor(mPreviousCursor);  
              NS_RELEASE(window);
            }
            mLastMouseState = eMouseExit;
          }
          break;
        case NS_MOUSE_LEFT_BUTTON_UP:
	        if (eMouseDown == mLastMouseState) {
            ShiftContents(aPresContext, PR_FALSE);
            nsEventStatus status = nsEventStatus_eIgnore;
            nsMouseEvent event;
            event.eventStructType = NS_MOUSE_EVENT;
            event.message = NS_MOUSE_LEFT_CLICK;
            mContent->HandleDOMEvent(aPresContext, &event, nsnull, DOM_EVENT_INIT, status);
        
            if (nsEventStatus_eConsumeNoDefault != status) {
              MouseClicked(&aPresContext);
            }
	          mLastMouseState = eMouseEnter;
	        } 
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
nsHTMLButtonControlFrame::SetInitialChildList(nsIPresContext& aPresContext,
                                              nsIAtom*        aListName,
                                              nsIFrame*       aChildList)
{
  // add ourself as an nsIFormControlFrame
  nsFormFrame::AddFormControlFrame(aPresContext, *this);

  // create our view, we need a view to grab the mouse 
  nsIView* view;
  GetView(view);
  if (!view) {
    nsresult result = nsRepository::CreateInstance(kViewCID, nsnull, kIViewIID, (void **)&view);
	  nsIPresShell   *presShell = aPresContext.GetShell();     
	  nsIViewManager *viewMan   = presShell->GetViewManager();  
    NS_RELEASE(presShell);

    nsIFrame* parWithView;
	  nsIView *parView;
    GetParentWithView(parWithView);
	  parWithView->GetView(parView);
    // the view's size is not know yet, but its size will be kept in synch with our frame.
    nsRect boundBox(0, 0, 500, 500); 
    result = view->Init(viewMan, boundBox, parView, nsnull);
    viewMan->InsertChild(parView, view, 0);
    SetView(view);

    const nsStyleColor* color = (const nsStyleColor*) mStyleContext->GetStyleData(eStyleStruct_Color);
    // set the opacity
    viewMan->SetViewOpacity(view, color->mOpacity);

    NS_RELEASE(viewMan);
  }

  // cache our display type
  const nsStyleDisplay* styleDisplay;
  GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&) styleDisplay);
  mInline = (NS_STYLE_DISPLAY_BLOCK != styleDisplay->mDisplay);

  PRUint8 flags = (mInline) ? NS_BODY_SHRINK_WRAP : 0;
  NS_NewBodyFrame(mContent, this, mFirstChild, flags);

  // Resolve style and set the style context
  nsIStyleContext* styleContext =
    aPresContext.ResolvePseudoStyleContextFor(mContent, nsHTMLAtoms::buttonContentPseudo, mStyleContext);
  mFirstChild->SetStyleContext(&aPresContext, styleContext);
  NS_RELEASE(styleContext);                                           

  // Set the geometric and content parent for each of the child frames
  for (nsIFrame* frame = aChildList; nsnull != frame; frame->GetNextSibling(frame)) {
    frame->SetGeometricParent(mFirstChild);
    frame->SetContentParent(mFirstChild);
  }

  // Queue up the frames for the inline frame
  return mFirstChild->SetInitialChildList(aPresContext, nsnull, aChildList);
}

NS_IMETHODIMP
nsHTMLButtonControlFrame::Paint(nsIPresContext& aPresContext,
                                nsIRenderingContext& aRenderingContext,
                                const nsRect& aDirtyRect)
{
  nsresult result = nsHTMLContainerFrame::Paint(aPresContext, aRenderingContext, aDirtyRect);
  if (NS_OK == result) {
    if (mGotFocus) { // draw dashed line to indicate selection, XXX don't calc rect every time
      const nsStyleSpacing* spacing =
        (const nsStyleSpacing*)mStyleContext->GetStyleData(eStyleStruct_Spacing);
      nsMargin border;
      spacing->CalcBorderFor(this, border);

      float p2t = aPresContext.GetPixelsToTwips();
      nscoord onePixel = NSIntPixelsToTwips(1, p2t);

      nsRect outside(0, 0, mRect.width, mRect.height);
      outside.Deflate(border);
      outside.Deflate(onePixel, onePixel);

      nsRect inside(outside);
      inside.Deflate(onePixel, onePixel);

      PRUint8 borderStyles[4];
      nscolor borderColors[4];
      nscolor black = NS_RGB(0,0,0);
      for (PRInt32 i = 0; i < 4; i++) {
        borderStyles[i] = NS_STYLE_BORDER_STYLE_DOTTED;
        borderColors[i] = black;
      }
      nsCSSRendering::DrawDashedSides(0, aRenderingContext, borderStyles, borderColors, outside,
                                      inside, PR_FALSE, nsnull);
    }
  }
  return result;
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

  // if we are constrained and the child is smaller, use the constrained values
  if (aReflowState.HaveFixedContentWidth() && (aDesiredSize.width < aReflowState.minWidth)) {
    aDesiredSize.width = aReflowState.minWidth;
  }
  if (aReflowState.HaveFixedContentHeight() && (aDesiredSize.height < aReflowState.minHeight)) {
    aDesiredSize.height = aReflowState.minHeight;
  }

  aDesiredSize.ascent  = aDesiredSize.height;
  aDesiredSize.descent = 0;

  aStatus = NS_FRAME_COMPLETE;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLButtonControlFrame::GetCursorAndContentAt(nsIPresContext& aPresContext,
                                                const nsPoint&  aPoint,
                                                nsIFrame**      aFrame,
                                                nsIContent**    aContent,
                                                PRInt32&        aCursor)
{
  nsresult result = nsHTMLContainerFrame::GetCursorAndContentAt(aPresContext, aPoint, aFrame, aContent, aCursor);
  aCursor = eCursor_standard;
  return result;
}

PRIntn
nsHTMLButtonControlFrame::GetSkipSides() const
{
  return 0;
}


