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
#include "nsIDOMHTMLDocument.h"
#include "nsIDOMXULDocument.h"
#include "nsHTMLContainerFrame.h"
#include "nsIFormControlFrame.h"
#include "nsHTMLParts.h"
#include "nsIForm.h"
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
#include "nsStyleUtil.h"
#include "nsIDOMHTMLCollection.h"
#include "nsStyleConsts.h"
#include "nsIHTMLAttributes.h"
#include "nsIWidget.h"
#include "nsIComponentManager.h"
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsViewsCID.h"
#include "nsColor.h"
#include "nsIDocument.h"
#include "nsIHTMLDocument.h"
#include "nsIDOMHTMLAnchorElement.h"
#include "nsFormControlFrame.h"

static NS_DEFINE_IID(kViewCID, NS_VIEW_CID);

class nsLabelFrame : public nsHTMLContainerFrame
{
public:
  nsLabelFrame();
  virtual ~nsLabelFrame();

  NS_IMETHOD Destroy(nsIPresContext *aPresContext);

  NS_IMETHOD Init(nsIPresContext*  aPresContext,
                  nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIStyleContext* aContext,
                  nsIFrame*        aPrevInFlow);

  NS_IMETHOD Paint(nsIPresContext* aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect& aDirtyRect,
                   nsFramePaintLayer aWhichLayer);

  NS_IMETHOD SetInitialChildList(nsIPresContext* aPresContext,
                                 nsIAtom*        aListName,
                                 nsIFrame*       aChildList);

  NS_IMETHOD Reflow(nsIPresContext*          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  NS_IMETHOD HandleEvent(nsIPresContext* aPresContext, 
                         nsGUIEvent* aEvent,
                         nsEventStatus* aEventStatus);

  NS_IMETHOD GetFrameForPoint(nsIPresContext* aPresContext, const nsPoint& aPoint, nsFramePaintLayer aWhichLayer, nsIFrame** aFrame);

  NS_IMETHOD GetFor(nsString& aFor);

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsString& aResult) const {
    return MakeFrameName("Label", aResult);
  }
#endif

protected:
  PRBool FindFirstControl(nsIPresContext* aPresContext,
                          nsIFrame* aParentFrame,
                          nsIFormControlFrame*& aResultFrame);
  PRBool FindForControl(nsIFormControlFrame*& aResultFrame);
  void GetTranslatedRect(nsIPresContext* aPresContext, nsRect& aRect);

  PRIntn GetSkipSides() const;
  PRBool mInline;
  nsCursor mPreviousCursor;
  nsMouseState mLastMouseState;
  PRBool mControlIsInside;
  nsIFormControlFrame* mControlFrame;
  nsRect mTranslatedRect;

};

nsresult
NS_NewLabelFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame, PRUint32 aStateFlags)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsLabelFrame* it = new (aPresShell) nsLabelFrame;
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  // set the state flags (if any are provided)
  nsFrameState state;
  it->GetFrameState( &state );
  state |= aStateFlags;
  it->SetFrameState( state );

  *aNewFrame = it;
  return NS_OK;
}

nsLabelFrame::nsLabelFrame()
  : nsHTMLContainerFrame()
{
  mInline          = PR_TRUE;
  mLastMouseState  = eMouseNone;
  mPreviousCursor  = eCursor_standard;
  mControlIsInside = PR_FALSE;
  mControlFrame    = nsnull;
  mTranslatedRect  = nsRect(0,0,0,0);
}

nsLabelFrame::~nsLabelFrame()
{
}


NS_IMETHODIMP
nsLabelFrame::Destroy(nsIPresContext *aPresContext)
{
  nsFormControlFrame::RegUnRegAccessKey(aPresContext, NS_STATIC_CAST(nsIFrame*, this), PR_FALSE);
  return nsHTMLContainerFrame::Destroy(aPresContext);
}


void
nsLabelFrame::GetTranslatedRect(nsIPresContext* aPresContext, nsRect& aRect)
{
  nsIView* view;
  nsPoint viewOffset(0,0);
  GetOffsetFromView(aPresContext, viewOffset, &view);
  while (nsnull != view) {
    nsPoint tempOffset;
    view->GetPosition(&tempOffset.x, &tempOffset.y);
    viewOffset += tempOffset;
    view->GetParent(view);
  }
  aRect = nsRect(viewOffset.x, viewOffset.y, mRect.width, mRect.height);
}

            
NS_METHOD 
nsLabelFrame::HandleEvent(nsIPresContext* aPresContext, 
                          nsGUIEvent* aEvent,
                          nsEventStatus* aEventStatus)
{
#if 0
  NS_ENSURE_ARG_POINTER(aEventStatus);
  // if we don't have a control to send the event to
  // then what is the point?
  if (!mControlFrame) {
    return NS_OK;
  }

  // check to see if the label is disabled and if not, 
  // then check to see if the control in the label is disabled
  if (nsEventStatus_eConsumeNoDefault != *aEventStatus) { 
    if ( nsFormFrame::GetDisabled(this) ) {
      return NS_OK;
    } else {
      nsIFrame * frame;
      if (NS_OK == mControlFrame->QueryInterface(NS_GET_IID(nsIFrame), (void**)&frame)) {
        if ( nsFormFrame::GetDisabled(frame)) {
          return NS_OK;
        }
      } else {
        // if I can't get the nsIFrame something is really wrong
        // so leave
        return NS_OK;
      }
    }
  }

  // send the mouse click events down into the control
  *aEventStatus = nsEventStatus_eIgnore;
  switch (aEvent->message) {
    case NS_MOUSE_LEFT_BUTTON_DOWN:{
      nsIContent * content;
      mControlFrame->GetFormContent(content);
      if (nsnull != content) {
        content->SetFocus(aPresContext);
        NS_RELEASE(content);
      }
      mLastMouseState = eMouseDown;
      *aEventStatus = nsEventStatus_eConsumeNoDefault;
      } break;

    case NS_MOUSE_LEFT_BUTTON_UP:
	    if (eMouseDown == mLastMouseState) {
        if (nsEventStatus_eConsumeNoDefault != *aEventStatus) {
          mControlFrame->MouseClicked(aPresContext);
        }
	    } 
	    mLastMouseState = eMouseUp;
      *aEventStatus = nsEventStatus_eConsumeNoDefault;
      break;
  }
#endif
  return nsFrame::HandleEvent(aPresContext, aEvent, aEventStatus);
}

NS_IMETHODIMP
nsLabelFrame::GetFrameForPoint(nsIPresContext* aPresContext,
                               const nsPoint& aPoint,
                               nsFramePaintLayer aWhichLayer,
                               nsIFrame** aFrame)
{
  nsresult rv = nsHTMLContainerFrame::GetFrameForPoint(aPresContext, aPoint, aWhichLayer, aFrame);
  if (rv == NS_OK) {
    nsCOMPtr<nsIFormControlFrame> controlFrame = do_QueryInterface(*aFrame);
    if (!controlFrame) {
      // if the hit frame isn't a form control then
      // check to see if it is an anchor

      // XXX It could be something else that should get the event.  Perhaps
      // this is better handled by event bubbling?

      nsIFrame * parent;
      (*aFrame)->GetParent(&parent);
      while (parent != this && parent != nsnull) {
        nsCOMPtr<nsIContent> content;
        parent->GetContent(getter_AddRefs(content));
        nsCOMPtr<nsIDOMHTMLAnchorElement> anchorElement(do_QueryInterface(content));
        if (anchorElement) {
          nsIStyleContext *psc;
          parent->GetStyleContext(&psc);
          if (psc) {
            const nsStyleDisplay* disp = (const nsStyleDisplay*)
              psc->GetStyleData(eStyleStruct_Display);
            if (disp->IsVisible()) {
              *aFrame = parent;
              return NS_OK;
            }
          }
        }
        parent->GetParent(&parent);
      }
      const nsStyleDisplay* disp = (const nsStyleDisplay*)
        mStyleContext->GetStyleData(eStyleStruct_Display);
      if (disp->IsVisible()) {
        *aFrame = this;
        return NS_OK;
      }
    }
  }

  return rv;
}

NS_IMETHODIMP
nsLabelFrame::GetFor(nsString& aResult)
{
  nsresult result = NS_FORM_NOTOK;
  if (mContent) {
    nsIHTMLContent* htmlContent = nsnull;
    result = mContent->QueryInterface(kIHTMLContentIID, (void**)&htmlContent);
    if ((NS_OK == result) && htmlContent) {
      nsHTMLValue value;
      result = htmlContent->GetHTMLAttribute(nsHTMLAtoms::_for, value);
      if (NS_CONTENT_ATTR_HAS_VALUE == result) {
        if (eHTMLUnit_String == value.GetUnit()) {
          value.GetStringValue(aResult);
        }
      }
      NS_RELEASE(htmlContent);
    }
  }
  return result;
}

static
PRBool IsButton(PRInt32 aType)
{
  if ((NS_FORM_INPUT_BUTTON == aType) || (NS_FORM_INPUT_RESET   == aType)   ||
      (NS_FORM_INPUT_SUBMIT == aType) || (NS_FORM_BUTTON_BUTTON == aType)   ||
      (NS_FORM_BUTTON_RESET == aType) || (NS_FORM_BUTTON_SUBMIT == aType))  {
    return PR_TRUE;
  } else {
    return PR_FALSE;
  }
}


PRBool 
nsLabelFrame::FindForControl(nsIFormControlFrame*& aResultFrame)
{
  nsAutoString forId;
  if (NS_CONTENT_ATTR_HAS_VALUE != GetFor(forId)) {
    return PR_FALSE;
  }

  nsCOMPtr<nsIDocument> iDoc;
  if (NS_FAILED(mContent->GetDocument(*getter_AddRefs(iDoc))))
    return PR_FALSE;
  
  nsCOMPtr<nsIDOMHTMLDocument> htmlDoc = do_QueryInterface(iDoc);

  nsCOMPtr<nsIDOMElement> domElement;
  if (htmlDoc) {
    if (NS_FAILED(htmlDoc->GetElementById(forId, getter_AddRefs(domElement))))
      return PR_FALSE;
  }
#ifdef INCLUDE_XUL
  else {
    nsCOMPtr<nsIDOMXULDocument> xulDoc = do_QueryInterface(iDoc);
    if (xulDoc) {
      if (NS_FAILED(xulDoc->GetElementById(forId, getter_AddRefs(domElement))))
        return PR_FALSE;
    }
  }
#endif // INCLUDE_XUL

  if (!domElement)
    return PR_FALSE;

  nsCOMPtr<nsIPresShell> shell = getter_AddRefs(iDoc->GetShellAt(0));
  if (nsnull == shell)
    return PR_FALSE;

  nsCOMPtr<nsIFormControl> control = do_QueryInterface(domElement);
  nsCOMPtr<nsIContent> controlContent = do_QueryInterface(domElement);

  // Labels have to be linked to form controls
  if (!control)
    return PR_FALSE;

  // buttons have implicit labels and we don't allow them to have explicit ones
  PRBool returnValue = PR_FALSE;

  PRInt32 type;
  control->GetType(&type);
  if (!IsButton(type)) {  
    nsIFrame* frame;
    shell->GetPrimaryFrameFor(controlContent, &frame);
    if (nsnull != frame) {
      nsIFormControlFrame* fcFrame = nsnull;
      nsresult result = frame->QueryInterface(NS_GET_IID(nsIFormControlFrame), (void**)&fcFrame);
      if ((NS_OK == result) && (nsnull != fcFrame)) {
        aResultFrame = fcFrame;
        NS_RELEASE(fcFrame);
        returnValue = PR_TRUE;
      }
    }
  }

  return returnValue;
}

PRBool 
nsLabelFrame::FindFirstControl(nsIPresContext* aPresContext,
                               nsIFrame* aParentFrame,
                               nsIFormControlFrame*& aResultFrame)
{
  nsIFrame* child = nsnull;
  aParentFrame->FirstChild(aPresContext, nsnull, &child);
  while (nsnull != child) {
    nsIFormControlFrame* fcFrame = nsnull;
    nsresult result = child->QueryInterface(NS_GET_IID(nsIFormControlFrame), (void**)&fcFrame);
    if ((NS_OK == result) && fcFrame) {
      PRInt32 type;
      fcFrame->GetType(&type);
      // buttons have implicit labels and we don't allow them to have explicit ones
      if (!IsButton(type)) {
        aResultFrame = fcFrame;
        return PR_TRUE;
      }
      NS_RELEASE(fcFrame);
    } else if (FindFirstControl(aPresContext, child, aResultFrame)) {
      return PR_TRUE;
    }
    child->GetNextSibling(&child);
  }
  return PR_FALSE;
}


NS_IMETHODIMP
nsLabelFrame::SetInitialChildList(nsIPresContext* aPresContext,
                                  nsIAtom*        aListName,
                                  nsIFrame*       aChildList)
{
  // cache our display type
  const nsStyleDisplay* styleDisplay;
  GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&) styleDisplay);
  mInline = (NS_STYLE_DISPLAY_BLOCK != styleDisplay->mDisplay);

  const nsStylePosition* position;
  GetStyleData(eStyleStruct_Position, (const nsStyleStruct*&) position);

  PRUint32 flags = (mInline) ? NS_BLOCK_SHRINK_WRAP : 0;
  nsCOMPtr<nsIPresShell> shell;
  aPresContext->GetShell(getter_AddRefs(shell));

  flags |= position->IsAbsolutelyPositioned() ? NS_BLOCK_SPACE_MGR : 0;

  nsIFrame* areaFrame;
  NS_NewAreaFrame(shell, &areaFrame, flags);
  mFrames.SetFrames(areaFrame);

  // Resolve style and initialize the frame
  nsIStyleContext* styleContext;
  aPresContext->ResolvePseudoStyleContextFor(mContent, nsHTMLAtoms::labelContentPseudo,
                                            mStyleContext, PR_FALSE,
                                            &styleContext);
  mFrames.FirstChild()->Init(aPresContext, mContent, this, styleContext, nsnull);
  NS_RELEASE(styleContext);                                           

  // Set the geometric and content parent for each of the child frames
  for (nsIFrame* frame = aChildList; nsnull != frame; frame->GetNextSibling(&frame)) {
    frame->SetParent(mFrames.FirstChild());
  }

  // Queue up the frames for the body frame
  return mFrames.FirstChild()->SetInitialChildList(aPresContext, nsnull, aChildList);
}

NS_IMETHODIMP
nsLabelFrame::Paint(nsIPresContext* aPresContext,
                    nsIRenderingContext& aRenderingContext,
                    const nsRect& aDirtyRect,
                    nsFramePaintLayer aWhichLayer)
{
  return nsHTMLContainerFrame::Paint(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);
}

// XXX a hack until the reflow state does this correctly
// XXX when it gets fixed, leave in the printf statements or add an assertion
static
void LabelHack(nsHTMLReflowState& aReflowState, char* aMessage)
{
  if (aReflowState.mComputedWidth == 0) {
    aReflowState.mComputedWidth = aReflowState.availableWidth;
    //NS_ASSERTION(0, "LabelHack: Path #1");
  }
  if ((aReflowState.mComputedWidth != NS_INTRINSICSIZE) &&
      (aReflowState.mComputedWidth > aReflowState.availableWidth) &&
      (aReflowState.availableWidth > 0)) {
    printf("BUG - %s has a computed width = %d, available width = %d \n", 
           aMessage, aReflowState.mComputedWidth, aReflowState.availableWidth);
    aReflowState.mComputedWidth = aReflowState.availableWidth;
    NS_ASSERTION(0, "LabelHack: Path #2");
  }
  if (aReflowState.mComputedHeight == 0) {
    aReflowState.mComputedHeight = aReflowState.availableHeight;
    NS_ASSERTION(0, "LabelHack: Path #3");
  }
  if ((aReflowState.mComputedHeight != NS_INTRINSICSIZE) &&
      (aReflowState.mComputedHeight > aReflowState.availableHeight) &&
      (aReflowState.availableHeight > 0)) {
    printf("BUG - %s has a computed height = %d, available height = %d \n", 
           aMessage, aReflowState.mComputedHeight, aReflowState.availableHeight);
    aReflowState.mComputedHeight = aReflowState.availableHeight;
    NS_ASSERTION(0, "LabelHack: Path #4");
  }
}

NS_IMETHODIMP
nsLabelFrame::Init(nsIPresContext*  aPresContext,
                   nsIContent*      aContent,
                   nsIFrame*        aParent,
                   nsIStyleContext* aContext,
                   nsIFrame*        aPrevInFlow)
{
  // call our base class
  nsresult  rv = nsHTMLContainerFrame::Init(aPresContext, aContent, aParent,
                                            aContext, aPrevInFlow);

  // create our view, we need a view to grab the mouse 
  nsIView* view;
  GetView(aPresContext, &view);
  if (!view) {
    nsresult result = nsComponentManager::CreateInstance(kViewCID, nsnull, NS_GET_IID(nsIView),
                                                  (void **)&view);
    nsCOMPtr<nsIPresShell> presShell;
    aPresContext->GetShell(getter_AddRefs(presShell));
    nsCOMPtr<nsIViewManager> viewMan;
    presShell->GetViewManager(getter_AddRefs(viewMan));

    nsIFrame* parWithView;
    nsIView *parView;
    GetParentWithView(aPresContext, &parWithView);
    parWithView->GetView(aPresContext, &parView);
    // the view's size is not know yet, but its size will be kept in synch with our frame.
    nsRect boundBox(0, 0, 0, 0); 
    result = view->Init(viewMan, boundBox, parView);
    view->SetContentTransparency(PR_TRUE);
    viewMan->InsertChild(parView, view, 0);
    SetView(aPresContext, view);
  }

  return rv;
}

NS_IMETHODIMP 
nsLabelFrame::Reflow(nsIPresContext*          aPresContext,
                     nsHTMLReflowMetrics&     aDesiredSize,
                     const nsHTMLReflowState& aReflowState,
                     nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsLabelFrame", aReflowState.reason);

  PRBool     isStyleChange      = PR_FALSE;
  PRBool     isDirtyChildReflow = PR_FALSE;

  // Check for an incremental reflow
  if (eReflowReason_Incremental == aReflowState.reason) {
    // See if we're the target frame
    nsIFrame* targetFrame;
    aReflowState.reflowCommand->GetTarget(targetFrame);
    if (this == targetFrame) {
      // Get the reflow type
      nsIReflowCommand::ReflowType  reflowType;
      aReflowState.reflowCommand->GetType(reflowType);

      switch (reflowType) {
      case nsIReflowCommand::ReflowDirty:
        isDirtyChildReflow = PR_TRUE;
        break;

      case nsIReflowCommand::StyleChanged:
        // Remember it's a style change so we can set the reflow reason below
        isStyleChange = PR_TRUE;
        break;

      default:
        NS_ASSERTION(PR_FALSE, "unexpected reflow command type");
      }

    } else {
      nsIFrame* nextFrame;
      // Get the next frame in the reflow chain
      aReflowState.reflowCommand->GetNext(nextFrame);
      NS_ASSERTION(nextFrame == mFrames.FirstChild(), "unexpected next reflow command frame");
    }
  } else if (eReflowReason_Initial == aReflowState.reason) {
    nsFormControlFrame::RegUnRegAccessKey(aPresContext, NS_STATIC_CAST(nsIFrame*, this), PR_TRUE);
  } 

  // XXX remove the following when the reflow state is fixed
  LabelHack((nsHTMLReflowState&)aReflowState, "BUG - label");

  if (nsnull == mControlFrame) {
    // check to see if a form control is referenced via the "for" attribute
    if (FindForControl(mControlFrame)) {
      mControlIsInside = PR_FALSE;
    } else {
      // find the 1st (and should be only) form control contained within if there is no "for"
      mControlIsInside = FindFirstControl(aPresContext, this, mControlFrame);
    }
  }

  nsSize availSize(aReflowState.availableWidth, aReflowState.availableHeight);

  // get border and padding
  const nsStyleSpacing* spacing =
    (const nsStyleSpacing*)mStyleContext->GetStyleData(eStyleStruct_Spacing);
  nsMargin borderPadding;
  spacing->CalcBorderPaddingFor(this, borderPadding);

  if (NS_INTRINSICSIZE != availSize.width) {
    availSize.width -= borderPadding.left + borderPadding.right;
    availSize.width = PR_MAX(availSize.width,0);
  }
  if (NS_AUTOHEIGHT != availSize.height) {
    availSize.height -= borderPadding.top + borderPadding.bottom;
    availSize.height = PR_MAX(availSize.height,0);
  }

  // reflow the child
  nsIFrame* firstKid = mFrames.FirstChild();
  nsHTMLReflowState reflowState(aPresContext, aReflowState, firstKid, availSize);

  if (isDirtyChildReflow) {
    // Note: the only reason the frame would be dirty would be if it had
    // just been inserted or appended
    reflowState.reason = eReflowReason_Initial;
    reflowState.reflowCommand = nsnull;
  } else if (isStyleChange) {
    reflowState.reason = eReflowReason_StyleChange;
    reflowState.reflowCommand = nsnull;
  }
  // XXX remove when reflow state is fixed
  LabelHack(reflowState, "label's area");

  ReflowChild(firstKid, aPresContext, aDesiredSize, reflowState,
              borderPadding.left, borderPadding.top, 0, aStatus);

  // Place the child
  FinishReflowChild(firstKid, aPresContext, aDesiredSize,
                    borderPadding.left, borderPadding.top, 0);

  // If the child frame was just inserted, then 
  // we're responsible for making sure it repaints
  // XXX Although in this case I don't think the areaFrame can be delete
#if 1
  if (isDirtyChildReflow) {
    // Complete the reflow and position and size the child frame
    nsRect  rect(reflowState.mComputedMargin.left, reflowState.mComputedMargin.top,
                 aDesiredSize.width, aDesiredSize.height);
    // Damage the area occupied by the deleted frame
    Invalidate(aPresContext, rect, PR_FALSE);
  }
#endif
  // add in our border and padding to the size of the child
  aDesiredSize.width  += borderPadding.left + borderPadding.right;
  aDesiredSize.height += borderPadding.top + borderPadding.bottom;

  // adjust our max element size, if necessary
  if (aDesiredSize.maxElementSize) {
    aDesiredSize.AddBorderPaddingToMaxElementSize(borderPadding);
  }

  aDesiredSize.ascent  = aDesiredSize.height;
  aDesiredSize.descent = 0;

  aStatus = NS_FRAME_COMPLETE;
  return NS_OK;
}


PRIntn
nsLabelFrame::GetSkipSides() const
{
  return 0;
}


