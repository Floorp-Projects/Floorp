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

#include "nsHTMLButtonControlFrame.h"

#include "nsCOMPtr.h"
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
#include "nsStyleUtil.h"
#include "nsDOMEvent.h"
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
#include "nsButtonFrameRenderer.h"

static NS_DEFINE_IID(kIFormControlIID, NS_IFORMCONTROL_IID);
static NS_DEFINE_IID(kIFormControlFrameIID, NS_IFORMCONTROLFRAME_IID);
static NS_DEFINE_IID(kViewCID, NS_VIEW_CID);
static NS_DEFINE_IID(kIViewIID, NS_IVIEW_IID);

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
  mPreviousCursor = eCursor_standard;
  mTranslatedRect = nsRect(0,0,0,0);
  mDidInit = PR_FALSE;
  mRenderer.SetNameSpace(kNameSpaceID_None);
}

NS_IMETHODIMP
nsHTMLButtonControlFrame::Init(nsIPresContext&  aPresContext,
              nsIContent*      aContent,
              nsIFrame*        aParent,
              nsIStyleContext* aContext,
              nsIFrame*        aPrevInFlow)
{
  nsresult  rv = nsHTMLContainerFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);
  mRenderer.SetFrame(this,aPresContext);
  return rv;
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
      mContent->HandleDOMEvent(*aPresContext, &mEvent, nsnull, NS_EVENT_FLAG_INIT, mStatus); 

      mFormFrame->OnReset();
    } else if (NS_FORM_BUTTON_SUBMIT == type) {
      //Send DOM event
      nsEventStatus mStatus;
      nsEvent mEvent;
      mEvent.eventStructType = NS_EVENT;
      mEvent.message = NS_FORM_SUBMIT;
      mContent->HandleDOMEvent(*aPresContext, &mEvent, nsnull, NS_EVENT_FLAG_INIT, mStatus); 

      mFormFrame->OnSubmit(aPresContext, this);
    }
  } 
}

void 
nsHTMLButtonControlFrame::SetFocus(PRBool aOn, PRBool aRepaint)
{
  mRenderer.SetFocus(aOn, aRepaint);
}

void
nsHTMLButtonControlFrame::GetTranslatedRect(nsRect& aRect)
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
nsHTMLButtonControlFrame::HandleEvent(nsIPresContext& aPresContext, 
                                      nsGUIEvent*     aEvent,
                                      nsEventStatus&  aEventStatus)
{
  nsWidgetRendering mode;
  aPresContext.GetWidgetRenderingMode(&mode);

  // if disabled do nothing
  if (mRenderer.isDisabled()) {
    return NS_OK;
  }

  nsresult result = mRenderer.HandleEvent(aPresContext, aEvent, aEventStatus);
  if (NS_OK != result)
     return result;
    
  aEventStatus = nsEventStatus_eIgnore;
 
  switch (aEvent->message) {

    case NS_MOUSE_ENTER:
	    break;
 
    case NS_MOUSE_LEFT_BUTTON_DOWN:
      mRenderer.SetFocus(PR_TRUE, PR_TRUE);         
	    break;

    case NS_MOUSE_LEFT_BUTTON_UP:
      if (mRenderer.isHover()) 
			  MouseClicked(&aPresContext);
	    break;

    case NS_KEY_DOWN:
      if (NS_KEY_EVENT == aEvent->eventStructType) {
        nsKeyEvent* keyEvent = (nsKeyEvent*)aEvent;
        if (NS_VK_SPACE  == keyEvent->keyCode) {
          MouseClicked(&aPresContext);
        }
      }
      break;

    case NS_MOUSE_EXIT:
	    break;
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
  nsIStyleContext* styleContext;
  aPresContext.ResolvePseudoStyleContextFor(mContent, nsHTMLAtoms::buttonContentPseudo,
                                            mStyleContext, PR_FALSE,
                                            &styleContext);
  mFrames.FirstChild()->Init(aPresContext, mContent, this, styleContext, nsnull);
  NS_RELEASE(styleContext);                                           

  // Set the parent for each of the child frames
  for (nsIFrame* frame = aChildList; nsnull != frame; frame->GetNextSibling(&frame)) {
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

  nsRect rect(0, 0, mRect.width, mRect.height);
  mRenderer.PaintButton(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer, rect);
  PaintChildren(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);

  return NS_OK;
}

// XXX a hack until the reflow state does this correctly
// XXX when it gets fixed, leave in the printf statements or add an assertion
static
void ButtonHack(nsHTMLReflowState& aReflowState, char* aMessage)
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
nsHTMLButtonControlFrame::Reflow(nsIPresContext& aPresContext,
                               nsHTMLReflowMetrics& aDesiredSize,
                               const nsHTMLReflowState& aReflowState,
                               nsReflowStatus& aStatus)
{
  // XXX remove the following when the reflow state is fixed
  ButtonHack((nsHTMLReflowState&)aReflowState, "html4 button");

  // commenting this out for now. We need a view to do mouse grabbing but
  // it doesn't really seem to work correctly. When you press the only event
  // you can get after that is a release. You need mouse enter and exit.
  // the view also breaks the outline code. For some reason you can not reset 
  // the clip rect to draw outside you bounds if you have a view. And you need to
  // because the outline must be drawn outside of our bounds according to CSS. -EDV
#if 0
  if (!mDidInit) {
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
    mDidInit = PR_TRUE;
  }
#endif

  // reflow the child
  nsIFrame* firstKid = mFrames.FirstChild();
  nsSize availSize(aReflowState.computedWidth, aReflowState.computedHeight);

  nsMargin borderPadding = mRenderer.GetFullButtonBorderAndPadding();

  if (NS_INTRINSICSIZE != availSize.width) {
    availSize.width -= borderPadding.left + borderPadding.right;
    availSize.width = PR_MAX(availSize.width,0);
  }
  if (NS_AUTOHEIGHT != availSize.height) {
    availSize.height -= borderPadding.top + borderPadding.bottom;
    availSize.height = PR_MAX(availSize.height,0);
  }

  nsHTMLReflowState reflowState(aPresContext, aReflowState, firstKid, availSize);
  // XXX remove the following when the reflow state is fixed
  ButtonHack(reflowState, "html4 button's area");

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
    }
  }

  ReflowChild(firstKid, aPresContext, aDesiredSize, reflowState, aStatus);

  // Place the child
  nsRect rect = nsRect(borderPadding.left, borderPadding.top, aDesiredSize.width, aDesiredSize.height);
  firstKid->SetRect(rect);

  // add in our border and padding to the size of the child
  aDesiredSize.width  += borderPadding.left + borderPadding.right;
  aDesiredSize.height += borderPadding.top + borderPadding.bottom;

  //adjust our max element size, if necessary
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
  nsIContent* content;
  nsresult    rv;

  rv = GetContent(&content);
  aContent = content;
  return rv;
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

nsresult nsHTMLButtonControlFrame::RequiresWidget(PRBool& aRequiresWidget)
{
  aRequiresWidget = PR_FALSE;
  return NS_OK;
}


NS_IMETHODIMP nsHTMLButtonControlFrame::SetProperty(nsIAtom* aName, const nsString& aValue)
{
  return NS_OK;
}

NS_IMETHODIMP nsHTMLButtonControlFrame::GetProperty(nsIAtom* aName, nsString& aValue)
{
  return NS_OK;
}

//
// ReResolveStyleContext
//
// When the style context changes, make sure that all of our styles are still up to date.
//
NS_IMETHODIMP
nsHTMLButtonControlFrame::ReResolveStyleContext ( nsIPresContext* aPresContext, nsIStyleContext* aParentContext,
                                                  PRInt32 aParentChange, nsStyleChangeList* aChangeList, 
                                                  PRInt32* aLocalChange)
{
  // this re-resolves |mStyleContext|, so it may change
  nsresult rv = nsHTMLContainerFrame::ReResolveStyleContext(aPresContext, aParentContext,
                                                            aParentChange, aChangeList, aLocalChange); 
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (NS_COMFALSE != rv) {
    mRenderer.ReResolveStyles(*aPresContext);
  }
  
  return rv;
  
} // ReResolveStyleContext




