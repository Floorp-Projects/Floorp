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
#include "nsFormControlHelper.h"
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
#include "nsIDocument.h"

//Enumeration of possible mouse states used to detect mouse clicks
/*enum nsMouseState {
  eMouseNone,
  eMouseEnter,
  eMouseExit,
  eMouseDown,
  eMouseUp
};*/

static NS_DEFINE_IID(kIFormControlIID, NS_IFORMCONTROL_IID);
static NS_DEFINE_IID(kIFormControlFrameIID, NS_IFORMCONTROLFRAME_IID);
static NS_DEFINE_IID(kViewCID, NS_VIEW_CID);
static NS_DEFINE_IID(kIViewIID, NS_IVIEW_IID);

class nsHTMLButtonControlFrame : public nsHTMLContainerFrame,
                                 public nsIFormControlFrame 
{
public:
  nsHTMLButtonControlFrame();

  NS_IMETHOD  QueryInterface(const nsIID& aIID, void** aInstancePtr);

  NS_IMETHOD Paint(nsIPresContext& aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect& aDirtyRect,
                   nsFramePaintLayer aWhichLayer);

  NS_IMETHOD Reflow(nsIPresContext&          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  NS_IMETHOD HandleEvent(nsIPresContext& aPresContext, 
                         nsGUIEvent* aEvent,
                         nsEventStatus& aEventStatus);

  NS_IMETHOD GetFrameForPoint(const nsPoint& aPoint, nsIFrame** aFrame);

  NS_IMETHOD SetInitialChildList(nsIPresContext& aPresContext,
                                 nsIAtom*        aListName,
                                 nsIFrame*       aChildList);

  NS_IMETHOD GetFrameName(nsString& aResult) const {
    return MakeFrameName("ButtonControl", aResult);
  }

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

  NS_IMETHOD GetFont(nsIPresContext* aPresContext, 
                    nsFont&         aFont);

  NS_IMETHOD GetFormContent(nsIContent*& aContent) const;
  virtual nscoord GetVerticalInsidePadding(float aPixToTwip,
                                           nscoord aInnerHeight) const;
  virtual nscoord GetHorizontalInsidePadding(nsIPresContext& aPresContext,
                                             float aPixToTwip, 
                                             nscoord aInnerWidth,
                                             nscoord aCharWidth) const;

  void GetDefaultLabel(nsString& aLabel);

       // nsIFormControlFrame
  NS_IMETHOD SetProperty(nsIAtom* aName, const nsString& aValue);
  NS_IMETHOD GetProperty(nsIAtom* aName, nsString& aValue); 

protected:
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
  PRBool mDidInit;
};

nsresult
NS_NewHTMLButtonControlFrame(nsIFrame*& aResult)
{
  aResult = new nsHTMLButtonControlFrame;
  if (nsnull == aResult) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

nsHTMLButtonControlFrame::nsHTMLButtonControlFrame()
  : nsHTMLContainerFrame()
{
  mInline = PR_TRUE;
  mLastMouseState = eMouseNone;
  mPreviousCursor = eCursor_standard;
  mGotFocus = PR_FALSE;
  mTranslatedRect = nsRect(0,0,0,0);
  mDidInit = PR_FALSE;
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
      result = formControl->GetHTMLAttribute(nsHTMLAtoms::name, value);
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
      result = formControl->GetHTMLAttribute(nsHTMLAtoms::value, value);
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
static
void ReflowTemp(nsIPresContext& aPresContext, nsHTMLButtonControlFrame* aFrame, nsRect& aRect)
{  
  // XXX You can't do this. Incremental reflow commands are dispatched from the
  // root frame downward...
#if 0
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
  NS_RELEASE(cmd);
#else
  nsIContent* content;
  aFrame->GetContent(content);
  if (nsnull != content) {
    nsIDocument*  document;

    content->GetDocument(document);
    document->ContentChanged(content, nsnull);
    NS_RELEASE(document);
    NS_RELEASE(content);
  }
#endif
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

  float p2t;
  aPresContext.GetScaledPixelsToTwips(p2t);
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
    spacing->SetBorderStyle(i,(spacing->GetBorderStyle(i) == NS_STYLE_BORDER_STYLE_INSET) ? 
                               NS_STYLE_BORDER_STYLE_OUTSET : NS_STYLE_BORDER_STYLE_INSET);
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

            
NS_IMETHODIMP
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
        switch (aEvent->message) {
        case NS_MOUSE_ENTER:
          if (mLastMouseState == eMouseDown) {
            ShiftContents(aPresContext, PR_TRUE);
          }
	        break;
        case NS_MOUSE_LEFT_BUTTON_DOWN:
          mGotFocus = PR_TRUE;
          ShiftContents(aPresContext, PR_TRUE);
	        mLastMouseState = eMouseDown;
	        break;
        case NS_MOUSE_LEFT_BUTTON_UP:
	        if (eMouseDown == mLastMouseState) {
            if (nsEventStatus_eConsumeNoDefault != aEventStatus) {
              ShiftContents(aPresContext, PR_FALSE);
              MouseClicked(&aPresContext);
            }
	          mLastMouseState = eMouseUp;
	        } 
	        break;
        case NS_MOUSE_EXIT:
          if (mLastMouseState == eMouseDown) {
            ShiftContents(aPresContext, PR_FALSE);
          }
	        break;
        }
        aEventStatus = nsEventStatus_eConsumeNoDefault;
        NS_RELEASE(viewMan);
      }
    }
  }
  return NS_OK;
}


NS_IMETHODIMP
nsHTMLButtonControlFrame::GetFrameForPoint(const nsPoint& aPoint, nsIFrame** aFrame)
{
  *aFrame = this;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLButtonControlFrame::SetInitialChildList(nsIPresContext& aPresContext,
                                              nsIAtom*        aListName,
                                              nsIFrame*       aChildList)
{
  // add ourself as an nsIFormControlFrame
  nsFormFrame::AddFormControlFrame(aPresContext, *this);

  // cache our display type
  const nsStyleDisplay* styleDisplay;
  GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&) styleDisplay);
  mInline = (NS_STYLE_DISPLAY_BLOCK != styleDisplay->mDisplay);

  PRUint8 flags = (mInline) ? NS_BLOCK_SHRINK_WRAP : 0;
  nsIFrame* areaFrame;
  NS_NewAreaFrame(areaFrame, flags);
  mFrames.SetFrames(areaFrame);

  // Resolve style and initialize the frame
  nsIStyleContext* styleContext =
    aPresContext.ResolvePseudoStyleContextFor(mContent, nsHTMLAtoms::buttonContentPseudo, mStyleContext);
  mFrames.FirstChild()->Init(aPresContext, mContent, this, styleContext);
  NS_RELEASE(styleContext);                                           

  // Set the parent for each of the child frames
  for (nsIFrame* frame = aChildList; nsnull != frame; frame->GetNextSibling(frame)) {
    frame->SetParent(mFrames.FirstChild());
  }

  // Queue up the frames for the inline frame
  return mFrames.FirstChild()->SetInitialChildList(aPresContext, nsnull, aChildList);
}

NS_IMETHODIMP
nsHTMLButtonControlFrame::Paint(nsIPresContext& aPresContext,
                                nsIRenderingContext& aRenderingContext,
                                const nsRect& aDirtyRect,
                                nsFramePaintLayer aWhichLayer)
{
  nsresult result = nsHTMLContainerFrame::Paint(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);
  if (NS_FAILED(result)) {
    return result;
  }
  if (eFramePaintLayer_Overlay == aWhichLayer) {
    if (mGotFocus) { // draw dashed line to indicate selection, XXX don't calc rect every time
      const nsStyleSpacing* spacing =
        (const nsStyleSpacing*)mStyleContext->GetStyleData(eStyleStruct_Spacing);
      nsMargin border;
      spacing->CalcBorderFor(this, border);

      float p2t;
      aPresContext.GetScaledPixelsToTwips(p2t);
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
  if (!mDidInit) {
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
    mDidInit = PR_TRUE;
  }

  // reflow the child
  nsIFrame* firstKid = mFrames.FirstChild();
  nsSize availSize(aReflowState.availableWidth, aReflowState.availableHeight);
  nsHTMLReflowState reflowState(aPresContext, firstKid, aReflowState, availSize);

  // XXX Proper handling of incremental reflow...
  if (eReflowReason_Incremental == aReflowState.reason) {
    nsIFrame* targetFrame;
    
    // See if it's targeted at us
    aReflowState.reflowCommand->GetTarget(targetFrame);
    if (this == targetFrame) {
      // XXX Handle this...
      reflowState.reason = eReflowReason_Resize;
    } else {
      nsIFrame* nextFrame;

      // Remove the next frame from the reflow path
      aReflowState.reflowCommand->GetNext(nextFrame);
      NS_ASSERTION(nextFrame == firstKid, "unexpected next frame");
    }
  }

  ReflowChild(firstKid, aPresContext, aDesiredSize, reflowState, aStatus);

  // get border and padding
  const nsStyleSpacing* spacing =
    (const nsStyleSpacing*)mStyleContext->GetStyleData(eStyleStruct_Spacing);
  nsMargin borderPadding;
  spacing->CalcBorderPaddingFor(this, borderPadding);

  // Place the child
  nsRect rect = nsRect(borderPadding.left, borderPadding.top, aDesiredSize.width, aDesiredSize.height);
  firstKid->SetRect(rect);

  // add in our border and padding to the size of the child
  aDesiredSize.width  += borderPadding.left + borderPadding.right;
  aDesiredSize.height += borderPadding.top + borderPadding.bottom;

  // adjust our max element size, if necessary
  if (aDesiredSize.maxElementSize) {
    aDesiredSize.AddBorderPaddingToMaxElementSize(borderPadding);
  }

  // if we are constrained and the child is smaller, use the constrained values
  if (aReflowState.HaveFixedContentWidth() && (aDesiredSize.width < aReflowState.computedWidth)) {
    aDesiredSize.width = aReflowState.computedWidth;
  }
  if (aReflowState.HaveFixedContentHeight() && (aDesiredSize.height < aReflowState.computedHeight)) {
    aDesiredSize.height = aReflowState.computedHeight;
  }

  aDesiredSize.ascent  = aDesiredSize.height;
  aDesiredSize.descent = 0;

  aStatus = NS_FRAME_COMPLETE;
  return NS_OK;
}

PRIntn
nsHTMLButtonControlFrame::GetSkipSides() const
{
  return 0;
}

NS_IMETHODIMP
nsHTMLButtonControlFrame::GetFont(nsIPresContext*        aPresContext, 
                             nsFont&                aFont)
{
  nsFormControlHelper::GetFont(this, aPresContext, mStyleContext, aFont);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLButtonControlFrame::GetFormContent(nsIContent*& aContent) const
{
  return GetContent(aContent);
}

nscoord 
nsHTMLButtonControlFrame::GetVerticalInsidePadding(float aPixToTwip, 
                                               nscoord aInnerHeight) const
{
   return 0;
}

nscoord 
nsHTMLButtonControlFrame::GetHorizontalInsidePadding(nsIPresContext& aPresContext,
                                               float aPixToTwip, 
                                               nscoord aInnerWidth,
                                               nscoord aCharWidth) const
{
  return 0;
}

NS_IMETHODIMP nsHTMLButtonControlFrame::SetProperty(nsIAtom* aName, const nsString& aValue)
{
  return NS_OK;
}

NS_IMETHODIMP nsHTMLButtonControlFrame::GetProperty(nsIAtom* aName, nsString& aValue)
{
  return NS_OK;
}



