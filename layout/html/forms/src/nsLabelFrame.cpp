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
#include "nsDOMEvent.h"
#include "nsIDOMHTMLCollection.h"
#include "nsStyleConsts.h"
#include "nsIHTMLAttributes.h"
#include "nsGenericHTMLElement.h"
#include "nsIWidget.h"
#include "nsIComponentManager.h"
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsViewsCID.h"
#include "nsColor.h"
#include "nsIDocument.h"
#include "nsIHTMLDocument.h"

//Enumeration of possible mouse states used to detect mouse clicks
enum nsMouseState {
  eMouseNone,
  eMouseEnter,
  eMouseExit,
  eMouseDown,
  eMouseUp
};

static NS_DEFINE_IID(kIFormControlIID, NS_IFORMCONTROL_IID);
static NS_DEFINE_IID(kIFormControlFrameIID, NS_IFORMCONTROLFRAME_IID);
static NS_DEFINE_IID(kViewCID, NS_VIEW_CID);
static NS_DEFINE_IID(kIViewIID, NS_IVIEW_IID);
static NS_DEFINE_IID(kIHTMLDocumentIID, NS_IHTMLDOCUMENT_IID);
static NS_DEFINE_IID(kIFormIID, NS_IFORM_IID);

class nsLabelFrame : public nsHTMLContainerFrame
{
public:
  nsLabelFrame();

  NS_IMETHOD Paint(nsIPresContext& aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect& aDirtyRect,
                   nsFramePaintLayer aWhichLayer);

  NS_IMETHOD SetInitialChildList(nsIPresContext& aPresContext,
                                 nsIAtom*        aListName,
                                 nsIFrame*       aChildList);

  NS_IMETHOD Reflow(nsIPresContext&          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  NS_IMETHOD HandleEvent(nsIPresContext& aPresContext, 
                         nsGUIEvent* aEvent,
                         nsEventStatus& aEventStatus);

  NS_IMETHOD GetFrameForPoint(const nsPoint& aPoint, nsIFrame** aFrame);

  NS_IMETHOD GetFor(nsString& aFor);

  NS_IMETHOD GetFrameName(nsString& aResult) const {
    return MakeFrameName("Label", aResult);
  }

protected:
  PRBool FindFirstControl(nsIFrame* aParentFrame, nsIFormControlFrame*& aResultFrame);
  PRBool FindForControl(nsIFormControlFrame*& aResultFrame);
  void GetTranslatedRect(nsRect& aRect);

  PRIntn GetSkipSides() const;
  PRBool mInline;
  nsCursor mPreviousCursor;
  nsMouseState mLastMouseState;
  PRBool mControlIsInside;
  nsIFormControlFrame* mControlFrame;
  nsRect mTranslatedRect;
  PRBool mDidInit;
};

nsresult
NS_NewLabelFrame(nsIFrame*& aResult)
{
  aResult = new nsLabelFrame;
  if (nsnull == aResult) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
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
  mDidInit         = PR_FALSE;
}

void
nsLabelFrame::GetTranslatedRect(nsRect& aRect)
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

            
NS_METHOD 
nsLabelFrame::HandleEvent(nsIPresContext& aPresContext, 
                          nsGUIEvent* aEvent,
                          nsEventStatus& aEventStatus)
{
  if (!mControlFrame) {
    return NS_OK;
  }

  aEventStatus = nsEventStatus_eIgnore;
  switch (aEvent->message) {
    case NS_MOUSE_LEFT_BUTTON_DOWN:
      mControlFrame->SetFocus(PR_TRUE);
 	    mLastMouseState = eMouseDown;
      aEventStatus = nsEventStatus_eConsumeNoDefault;
	    break;
    case NS_MOUSE_LEFT_BUTTON_UP:
	    if (eMouseDown == mLastMouseState) {
        if (nsEventStatus_eConsumeNoDefault != aEventStatus) {
          mControlFrame->MouseClicked(&aPresContext);
        }
	    } 
	    mLastMouseState = eMouseUp;
      aEventStatus = nsEventStatus_eConsumeNoDefault;
      break;
  }
  return nsFrame::HandleEvent(aPresContext, aEvent, aEventStatus);
}

NS_IMETHODIMP
nsLabelFrame::GetFrameForPoint(const nsPoint& aPoint, nsIFrame** aFrame)
{
  *aFrame = this;
  return NS_OK;
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
  static char whitespace[] = " \r\n\t";

  nsAutoString forId;
  if (NS_CONTENT_ATTR_HAS_VALUE != GetFor(forId)) {
    return PR_FALSE;
  }

  nsIDocument* iDoc = nsnull;
  nsresult result = mContent->GetDocument(iDoc);
  if ((NS_OK != result) || (nsnull == iDoc)) {
    return PR_FALSE;
  }

  nsIHTMLDocument* htmlDoc = nsnull;
  result = iDoc->QueryInterface(kIHTMLDocumentIID, (void**)&htmlDoc);
  if ((NS_OK != result) || (nsnull == htmlDoc)) {
    NS_RELEASE(iDoc);
    return PR_FALSE;
  }

  nsIPresShell *shell = iDoc->GetShellAt(0);
  if (nsnull == shell) {
    NS_RELEASE(iDoc);
    NS_RELEASE(htmlDoc);
    return PR_FALSE;
  }

  nsIDOMHTMLCollection* forms = nsnull;
  htmlDoc->GetForms(&forms);
  PRUint32 numForms;
  forms->GetLength(&numForms);

  PRBool returnValue = PR_FALSE;

  for (PRUint32 formX = 0; formX < numForms; formX++) {
    nsIDOMNode* node = nsnull;
    forms->Item(formX, &node);
    if (nsnull == node) {
      continue;
    }
    nsIForm* form = nsnull;
    result = node->QueryInterface(kIFormIID, (void**)&form);
    if ((NS_OK != result) || (nsnull == form)) {
      continue;
    }
    PRUint32 numControls;
    form->GetElementCount(&numControls);
    for (PRUint32 controlX = 0; controlX < numControls; controlX++) {
      nsIFormControl* control = nsnull;
      form->GetElementAt(controlX, &control);
      if (nsnull == control) {
        continue;
      }
      // buttons have implicit labels and we don't allow them to have explicit ones
      PRInt32 type;
      control->GetType(&type);
      if (!IsButton(type)) {
        nsIHTMLContent* htmlContent = nsnull;
        result = control->QueryInterface(kIHTMLContentIID, (void**)&htmlContent);
        if ((NS_OK == result) && (nsnull != htmlContent)) {
          nsHTMLValue value;
          nsAutoString id;
          result = htmlContent->GetHTMLAttribute(nsHTMLAtoms::id, value);
          if ((NS_CONTENT_ATTR_HAS_VALUE == result) && (eHTMLUnit_String == value.GetUnit())) {
            value.GetStringValue(id);
            id.Trim(whitespace, PR_TRUE, PR_TRUE);    
            if (id.Equals(forId)) {
              nsIFrame* frame;
              shell->GetPrimaryFrameFor(htmlContent, &frame);
              if (nsnull != frame) {
                nsIFormControlFrame* fcFrame = nsnull;
                result = frame->QueryInterface(kIFormControlFrameIID, (void**)&fcFrame);
                if ((NS_OK == result) && (nsnull != fcFrame)) {
                  aResultFrame = fcFrame;
                  NS_RELEASE(fcFrame);
                  returnValue = PR_TRUE;
                }
              }
            }
          }
          NS_RELEASE(htmlContent);
        }
      }
      NS_RELEASE(control);
    } 
  }
  NS_RELEASE(iDoc);
  NS_RELEASE(htmlDoc);
  NS_RELEASE(forms);
  NS_RELEASE(shell);

  return returnValue;
}

PRBool 
nsLabelFrame::FindFirstControl(nsIFrame* aParentFrame, nsIFormControlFrame*& aResultFrame)
{
  nsIFrame* child = nsnull;
  aParentFrame->FirstChild(nsnull, &child);
  while (nsnull != child) {
    nsIFormControlFrame* fcFrame = nsnull;
    nsresult result = child->QueryInterface(kIFormControlFrameIID, (void**)&fcFrame);
    if ((NS_OK == result) && fcFrame) {
      PRInt32 type;
      fcFrame->GetType(&type);
      // buttons have implicit labels and we don't allow them to have explicit ones
      if (!IsButton(type)) {
        aResultFrame = fcFrame;
        return PR_TRUE;
      }
      NS_RELEASE(fcFrame);
    } else if (FindFirstControl(child, aResultFrame)) {
      return PR_TRUE;
    }
    child->GetNextSibling(&child);
  }
  return PR_FALSE;
}


NS_IMETHODIMP
nsLabelFrame::SetInitialChildList(nsIPresContext& aPresContext,
                                  nsIAtom*        aListName,
                                  nsIFrame*       aChildList)
{
  // cache our display type
  const nsStyleDisplay* styleDisplay;
  GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&) styleDisplay);
  mInline = (NS_STYLE_DISPLAY_BLOCK != styleDisplay->mDisplay);

  PRUint8 flags = (mInline) ? NS_BLOCK_SHRINK_WRAP : 0;
  nsIFrame* areaFrame;
  NS_NewAreaFrame(areaFrame, flags);
  mFrames.SetFrames(areaFrame);

  // Resolve style and initialize the frame
  nsIStyleContext* styleContext;
  aPresContext.ResolvePseudoStyleContextFor(mContent, nsHTMLAtoms::labelContentPseudo,
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
nsLabelFrame::Paint(nsIPresContext& aPresContext,
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
  if (aReflowState.computedWidth == 0) {
    aReflowState.computedWidth = aReflowState.availableWidth;
  }
  if ((aReflowState.computedWidth != NS_INTRINSICSIZE) &&
      (aReflowState.computedWidth > aReflowState.availableWidth) &&
      (aReflowState.availableWidth > 0)) {
//    printf("BUG - %s has a computed width = %d, available width = %d \n", 
//    aMessage, aReflowState.computedWidth, aReflowState.availableWidth);
    aReflowState.computedWidth = aReflowState.availableWidth;
  }
  if (aReflowState.computedHeight == 0) {
    aReflowState.computedHeight = aReflowState.availableHeight;
  }
  if ((aReflowState.computedHeight != NS_INTRINSICSIZE) &&
      (aReflowState.computedHeight > aReflowState.availableHeight) &&
      (aReflowState.availableHeight > 0)) {
//    printf("BUG - %s has a computed height = %d, available height = %d \n", 
//    aMessage, aReflowState.computedHeight, aReflowState.availableHeight);
    aReflowState.computedHeight = aReflowState.availableHeight;
  }
}

NS_IMETHODIMP 
nsLabelFrame::Reflow(nsIPresContext&          aPresContext,
                     nsHTMLReflowMetrics&     aDesiredSize,
                     const nsHTMLReflowState& aReflowState,
                     nsReflowStatus&          aStatus)
{
  // XXX remove the following when the reflow state is fixed
  LabelHack((nsHTMLReflowState&)aReflowState, "BUG - label");
  if (!mDidInit) {
    // create our view, we need a view to grab the mouse 
    nsIView* view;
    GetView(&view);
    if (!view) {
      nsresult result = nsComponentManager::CreateInstance(kViewCID, nsnull, kIViewIID,
                                                    (void **)&view);
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
    }
    mDidInit = PR_TRUE;
  }

  if (nsnull == mControlFrame) {
    // check to see if a form control is referenced via the "for" attribute
    if (FindForControl(mControlFrame)) {
      mControlIsInside = PR_FALSE;
    } else {
      // find the 1st (and should be only) form control contained within if there is no "for"
      mControlIsInside = FindFirstControl(this, mControlFrame);
    }
  }

  nsSize availSize(aReflowState.computedWidth, aReflowState.computedHeight);

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
  // XXX remove when reflow state is fixed
  LabelHack(reflowState, "label's area");
  ReflowChild(firstKid, aPresContext, aDesiredSize, reflowState, aStatus);

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
  //if (aReflowState.HaveFixedContentWidth() && (aDesiredSize.width < aReflowState.computedWidth)) {
  //  aDesiredSize.width = aReflowState.computedWidth;
  //}
  //if (aReflowState.HaveFixedContentHeight() && (aDesiredSize.height < aReflowState.computedHeight)) {
  //  aDesiredSize.height = aReflowState.computedHeight;
  //}

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


