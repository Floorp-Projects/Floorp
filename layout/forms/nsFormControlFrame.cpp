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
#include "nsFormControlFrame.h"
#include "nsHTMLParts.h"
#include "nsIHTMLContent.h"
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
#include "nsIComponentManager.h"
#include "nsGUIEvent.h"
#include "nsDOMEvent.h"
#include "nsIFontMetrics.h"
#include "nsIFormControl.h"
#include "nsIDeviceContext.h"
#include "nsHTMLAtoms.h"
#include "nsIButton.h"  // remove this when GetCID is pure virtual
#include "nsICheckButton.h"  //remove this
#include "nsITextWidget.h"  //remove this
#include "nsISupports.h"
#include "nsStyleConsts.h"
#include "nsUnitConversion.h"
#include "nsStyleUtil.h"
#include "nsFormFrame.h"
#include "nsIContent.h"
#include "nsStyleUtil.h"
#include "nsINameSpaceManager.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIDOMHTMLLabelElement.h"
#include "nsIDOMHTMLTextAreaElement.h"
#include "nsIDOMHTMLLegendElement.h"
#include "nsIDOMHTMLButtonElement.h"
#include "nsIEventStateManager.h"

#ifdef DEBUG_evaughan
#define DEBUG_rods
#endif

#ifdef DEBUG_rods
#define FCF_NOISY
#endif

const PRInt32 kSizeNotSet = -1;

static NS_DEFINE_IID(kIWidgetIID, NS_IWIDGET_IID);
static NS_DEFINE_IID(kIFormControlIID, NS_IFORMCONTROL_IID);
static NS_DEFINE_IID(kIFormControlFrameIID, NS_IFORMCONTROLFRAME_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kViewCID, NS_VIEW_CID);
static NS_DEFINE_IID(kIViewIID, NS_IVIEW_IID);
static NS_DEFINE_IID(kIDOMHTMLInputElementIID, NS_IDOMHTMLINPUTELEMENT_IID);

nsFormControlFrame::nsFormControlFrame()
  : nsLeafFrame()
{
  mDidInit        = PR_FALSE;
  mFormFrame      = nsnull;
  mSuggestedWidth = NS_FORMSIZE_NOTSET;
  mSuggestedHeight = NS_FORMSIZE_NOTSET;
  mPresContext    = nsnull;

  // Reflow Optimization
  mCacheSize.width             = kSizeNotSet;
  mCacheSize.height            = kSizeNotSet;
  mCachedMaxElementSize.width  = kSizeNotSet;
  mCachedMaxElementSize.height = kSizeNotSet;
}

nsFormControlFrame::~nsFormControlFrame()
{
  if (mFormFrame) {
    mFormFrame->RemoveFormControlFrame(*this);
    // This method only removes from radio lists if we are a radio input
    mFormFrame->RemoveRadioControlFrame(this);
    mFormFrame = nsnull;
  }
  RegUnRegAccessKey(mPresContext, NS_STATIC_CAST(nsIFrame*, this), PR_FALSE);
}

// Frames are not refcounted, no need to AddRef
NS_IMETHODIMP
nsFormControlFrame::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  NS_PRECONDITION(0 != aInstancePtr, "null ptr");
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kIFormControlFrameIID)) {
    *aInstancePtr = (void*) ((nsIFormControlFrame*) this);
    return NS_OK;
  }
  return nsLeafFrame::QueryInterface(aIID, aInstancePtr);
}

void nsFormControlFrame::SetupCachedSizes(nsSize& aCacheSize,
                                              nsSize& aCachedMaxElementSize,
                                              nsHTMLReflowMetrics& aDesiredSize)
{
  aCacheSize.width  = aDesiredSize.width;
  aCacheSize.height = aDesiredSize.height;
  if (aDesiredSize.maxElementSize != nsnull) {
    aCachedMaxElementSize.width  = aDesiredSize.maxElementSize->width;
    aCachedMaxElementSize.height = aDesiredSize.maxElementSize->height;
  }
}

//------------------------------------------------------------
void nsFormControlFrame::SkipResizeReflow(nsSize& aCacheSize,
                                          nsSize& aCachedMaxElementSize,
                                          nsSize& aCachedAvailableSize,
                                          nsHTMLReflowMetrics& aDesiredSize,
                                          const nsHTMLReflowState& aReflowState,
                                          nsReflowStatus& aStatus,
                                          PRBool& aBailOnWidth,
                                          PRBool& aBailOnHeight)
{

  if (aReflowState.reason == eReflowReason_Incremental) {
    aBailOnHeight = PR_FALSE;
    aBailOnWidth  = PR_FALSE;

  } else if (eReflowReason_Initial == aReflowState.reason) {
    aBailOnHeight = PR_FALSE;
    aBailOnWidth  = PR_FALSE;

  } else {

    nscoord width;
    if (NS_UNCONSTRAINEDSIZE == aReflowState.mComputedWidth) {
      if (aReflowState.availableWidth == NS_UNCONSTRAINEDSIZE) {
        width = NS_UNCONSTRAINEDSIZE;
        aBailOnWidth = aCacheSize.width != kSizeNotSet;
#ifdef FCF_NOISY
        if (aBailOnWidth) {
          printf("-------------- #1 Bailing on aCachedAvailableSize.width %d != kSizeNotSet\n", aCachedAvailableSize.width);
        }
#endif
      } else {
        width = aReflowState.availableWidth - aReflowState.mComputedBorderPadding.left -
                aReflowState.mComputedBorderPadding.right;
        aBailOnWidth = aCachedAvailableSize.width <= width && aCachedAvailableSize.width != kSizeNotSet;
#ifdef FCF_NOISY
        if (aBailOnWidth) {
          printf("-------------- #2 Bailing on aCachedAvailableSize.width %d <= width %d\n", aCachedAvailableSize.width, width );
        }
#endif
      }
    } else {
      width = aReflowState.mComputedWidth;
      //if (aCachedAvailableSize.width == kSizeNotSet) {
      //  //aBailOnWidth = aCachedAvailableSize.width == aCacheSize.width;
        aBailOnWidth = PR_FALSE;
      //} else {
        aBailOnWidth = width == (aCacheSize.width - aReflowState.mComputedBorderPadding.left - aReflowState.mComputedBorderPadding.right);
      //}
#ifdef FCF_NOISY
      if (aBailOnWidth) {
        printf("-------------- #3 Bailing on aCachedAvailableSize.width %d == aReflowState.mComputedWidth %d\n", aCachedAvailableSize.width, width );
      }
#endif
    }
    
    nscoord height;
    if (NS_UNCONSTRAINEDSIZE == aReflowState.mComputedHeight) {
      if (aReflowState.availableHeight == NS_UNCONSTRAINEDSIZE) {
        height = NS_UNCONSTRAINEDSIZE;
        aBailOnHeight = aCachedAvailableSize.height != kSizeNotSet;
      } else {
        height = aReflowState.availableHeight - aReflowState.mComputedBorderPadding.top -
                 aReflowState.mComputedBorderPadding.bottom;
        aBailOnHeight = aCachedAvailableSize.height <= height;
      }
    } else {
      height = aReflowState.mComputedWidth;
      aBailOnHeight = aCachedAvailableSize.height == height;
    }

    if (aBailOnWidth || aBailOnHeight) {
      aDesiredSize.width  = aCacheSize.width;
      aDesiredSize.height = aCacheSize.height;

      if (aDesiredSize.maxElementSize != nsnull) {
        aDesiredSize.maxElementSize->width  = aCachedMaxElementSize.width;
        aDesiredSize.maxElementSize->height = aCachedMaxElementSize.height;
      }
      aDesiredSize.ascent = aDesiredSize.height;
      aDesiredSize.descent = 0;
    }
  }
}

nscoord 
nsFormControlFrame::GetScrollbarWidth(float aPixToTwip)
{
   return NSIntPixelsToTwips(19, aPixToTwip);  // XXX this is windows
}

nscoord 
nsFormControlFrame::GetVerticalBorderWidth(float aPixToTwip) const
{
   return NSIntPixelsToTwips(3, aPixToTwip);
}

nscoord 
nsFormControlFrame::GetHorizontalBorderWidth(float aPixToTwip) const
{
  return GetVerticalBorderWidth(aPixToTwip);
}

nscoord 
nsFormControlFrame::GetVerticalInsidePadding(nsIPresContext* aPresContext,
                                             float aPixToTwip, 
                                             nscoord aInnerHeight) const
{
   return NSIntPixelsToTwips(3, aPixToTwip);
}

nscoord 
nsFormControlFrame::GetHorizontalInsidePadding(nsIPresContext* aPresContext,
                                               float aPixToTwip, 
                                               nscoord aInnerWidth,
                                               nscoord aCharWidth) const
{
  return GetVerticalInsidePadding(aPresContext, aPixToTwip, aInnerWidth);
}

PRInt32
nsFormControlFrame::GetMaxNumValues()
{
  return 0;
}

PRBool
nsFormControlFrame::GetNamesValues(PRInt32 aMaxNumValues, PRInt32& aNumValues, 
                                   nsString* aValues, nsString* aNames)
{
  aNumValues = 0;
  return PR_FALSE;
}

void 
nsFormControlFrame::SetClickPoint(nscoord aX, nscoord aY)
{
  mLastClickPoint.x = aX;
  mLastClickPoint.y = aY;
}

// XXX it would be cool if form element used our rendering sw, then
// they could be blended, and bordered, and so on...
NS_METHOD
nsFormControlFrame::Paint(nsIPresContext* aPresContext,
                          nsIRenderingContext& aRenderingContext,
                          const nsRect& aDirtyRect,
                          nsFramePaintLayer aWhichLayer)
{

  nsresult rv = nsLeafFrame::Paint(aPresContext, aRenderingContext, aDirtyRect,
                            aWhichLayer);

 if (NS_FRAME_PAINT_LAYER_BACKGROUND == aWhichLayer){

    nsRect rect(0, 0, mRect.width, mRect.height);
    PaintSpecialBorder(aPresContext, 
                       aRenderingContext,
                       this,
                       aDirtyRect,
                       rect);    
  }
  return rv;
}

void 
nsFormControlFrame::GetDesiredSize(nsIPresContext*          aPresContext,
                                   const nsHTMLReflowState& aReflowState,
                                   nsHTMLReflowMetrics&     aDesiredLayoutSize,
                                   nsSize&                  aDesiredWidgetSize)
{
  // get the css size and let the frame use or override it
  nsSize styleSize;
  GetStyleSize(aPresContext, aReflowState, styleSize);

  // subclasses should always override this method, but if not and no css, make it small
  aDesiredLayoutSize.width  = (styleSize.width  > CSS_NOTSET) ? styleSize.width  : 144;
  aDesiredLayoutSize.height = (styleSize.height > CSS_NOTSET) ? styleSize.height : 144;
  aDesiredLayoutSize.ascent = aDesiredLayoutSize.height;
  aDesiredLayoutSize.descent = 0;
  if (aDesiredLayoutSize.maxElementSize) {
    aDesiredLayoutSize.maxElementSize->width  = aDesiredLayoutSize.width;
    aDesiredLayoutSize.maxElementSize->height = aDesiredLayoutSize.height;
  }
  aDesiredWidgetSize.width  = aDesiredLayoutSize.width;
  aDesiredWidgetSize.height = aDesiredLayoutSize.height;
}

void 
nsFormControlFrame::GetDesiredSize(nsIPresContext* aPresContext,
                             const nsHTMLReflowState& aReflowState,
                             nsHTMLReflowMetrics& aDesiredSize)
{
  nsSize ignore;
  GetDesiredSize(aPresContext, aReflowState, aDesiredSize, ignore);
}

NS_IMETHODIMP
nsFormControlFrame::DidReflow(nsIPresContext* aPresContext,
                        nsDidReflowStatus aStatus)
{
  nsresult rv = nsLeafFrame::DidReflow(aPresContext, aStatus);


  // The view is created hidden; once we have reflowed it and it has been
  // positioned then we show it.
  if (NS_FRAME_REFLOW_FINISHED == aStatus) {
    nsIView* view = nsnull;
    GetView(aPresContext, &view);
    if (view) {
      const nsStyleDisplay* display;
      GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)display));
      nsViewVisibility newVis = display->IsVisible() ? nsViewVisibility_kShow : nsViewVisibility_kHide;
      nsViewVisibility oldVis;
      // only change if different.
      view->GetVisibility(oldVis);
      if (newVis != oldVis) 
        view->SetVisibility(newVis);
    }
  }
  
  return rv;
}

NS_IMETHODIMP
nsFormControlFrame::SetInitialChildList(nsIPresContext* aPresContext,
                                        nsIAtom*        aListName,
                                        nsIFrame*       aChildList)
{
  return NS_OK;
}

NS_METHOD
nsFormControlFrame::Reflow(nsIPresContext*          aPresContext,
                           nsHTMLReflowMetrics&     aDesiredSize,
                           const nsHTMLReflowState& aReflowState,
                           nsReflowStatus&          aStatus)
{
  if (!mDidInit) {
    mPresContext = aPresContext;
    InitializeControl(aPresContext);
    mDidInit = PR_TRUE;
  }

  // add ourself as an nsIFormControlFrame
  if (!mFormFrame && (eReflowReason_Initial == aReflowState.reason)) {
    nsFormFrame::AddFormControlFrame(aPresContext, *NS_STATIC_CAST(nsIFrame*, this));
  }

#if 0
  nsresult skiprv = SkipResizeReflow(mCacheSize, mCachedMaxElementSize, aPresContext, 
                                     aDesiredSize, aReflowState, aStatus);
  if (NS_SUCCEEDED(skiprv)) {
    return skiprv;
  }
#endif

  nsresult rv = nsLeafFrame::Reflow(aPresContext, aDesiredSize, aReflowState, aStatus);

  aStatus = NS_FRAME_COMPLETE;
  SetupCachedSizes(mCacheSize, mCachedMaxElementSize, aDesiredSize);
  return rv;
}


nsWidgetInitData* 
nsFormControlFrame::GetWidgetInitData(nsIPresContext* aPresContext)
{
  return nsnull;
}


nsresult
nsFormControlFrame::RegUnRegAccessKey(nsIPresContext* aPresContext, nsIFrame * aFrame, PRBool aDoReg)
{
#if 0
  NS_ASSERTION(aPresContext, "aPresContext is NULL in RegUnRegAccessKey!");
  NS_ASSERTION(aFrame, "aFrame is NULL in RegUnRegAccessKey!");

  nsresult rv = NS_ERROR_FAILURE;
  nsAutoString accessKey;

  if (aFrame != nsnull) {
    nsCOMPtr<nsIContent> content;
    if (NS_SUCCEEDED(aFrame->GetContent(getter_AddRefs(content)))) {
#if 1
      PRInt32 nameSpaceID;
      content->GetNameSpaceID(nameSpaceID);
      nsAutoString resultValue;
      rv = content->GetAttribute(nameSpaceID, nsHTMLAtoms::accesskey, accessKey);
#else
      nsCOMPtr<nsIDOMHTMLInputElement> inputElement(do_QueryInterface(content));
      if (inputElement) {
        rv = inputElement->GetAccessKey(accessKey);
      } else {
        nsCOMPtr<nsIDOMHTMLTextAreaElement> textarea(do_QueryInterface(content));
        if (textarea) {
          rv = textarea->GetAccessKey(accessKey);
        } else {
          nsCOMPtr<nsIDOMHTMLLabelElement> label(do_QueryInterface(content));
          if (label) {
            rv = label->GetAccessKey(accessKey);
          } else {
            nsCOMPtr<nsIDOMHTMLLegendElement> legend(do_QueryInterface(content));
            if (legend) {
              rv = legend->GetAccessKey(accessKey);
            } else {
              nsCOMPtr<nsIDOMHTMLButtonElement> btn(do_QueryInterface(content));
              if (btn) {
                rv = btn->GetAccessKey(accessKey);
              } 
            }
          }
        }
      }
#endif
    }
  }

  if (NS_CONTENT_ATTR_NOT_THERE != rv) {
    nsCOMPtr<nsIEventStateManager> stateManager;
    if (NS_SUCCEEDED(aPresContext->GetEventStateManager(getter_AddRefs(stateManager)))) {
      if (aDoReg) {
        return stateManager->RegisterAccessKey(aFrame, (PRUint32)accessKey.First());
      } else {
        return stateManager->UnregisterAccessKey(aFrame);
      }
    }
  }
#endif
  return NS_ERROR_FAILURE;
}

void 
nsFormControlFrame::InitializeControl(nsIPresContext* aPresContext)
{
  RegUnRegAccessKey(aPresContext, NS_STATIC_CAST(nsIFrame*, this), PR_TRUE);
}

void 
nsFormControlFrame::SetFocus(PRBool aOn, PRBool aRepaint)
{
}

void
nsFormControlFrame::ScrollIntoView(nsIPresContext* aPresContext)
{
  if (aPresContext) {
    nsCOMPtr<nsIPresShell> presShell;
    aPresContext->GetShell(getter_AddRefs(presShell));
    presShell->ScrollFrameIntoView(this,
                   NS_PRESSHELL_SCROLL_IF_NOT_VISIBLE,NS_PRESSHELL_SCROLL_IF_NOT_VISIBLE);

  }
}

/*
 * FIXME: this ::GetIID() method has no meaning in life and should be
 * removed.
 * Pierre Phaneuf <pp@ludusdesign.com>
 */
const nsIID&
nsFormControlFrame::GetIID()
{
  static NS_DEFINE_IID(kButtonIID, NS_IBUTTON_IID);
  return kButtonIID;
}
  
const nsIID&
nsFormControlFrame::GetCID()
{
  static NS_DEFINE_IID(kButtonCID, NS_BUTTON_CID);
  return kButtonCID;
}

NS_IMETHODIMP
nsFormControlFrame::GetMaxLength(PRInt32* aSize)
{
  *aSize = -1;
  nsresult result = NS_CONTENT_ATTR_NOT_THERE;
  nsIHTMLContent* content = nsnull;
  mContent->QueryInterface(kIHTMLContentIID, (void**) &content);
  if (nsnull != content) {
    nsHTMLValue value;
    result = content->GetHTMLAttribute(nsHTMLAtoms::maxlength, value);
    if (eHTMLUnit_Integer == value.GetUnit()) { 
      *aSize = value.GetIntValue();
    }
    NS_RELEASE(content);
  }
  return result;
}

nsresult
nsFormControlFrame::GetSizeFromContent(PRInt32* aSize) const
{
  *aSize = -1;
  nsresult result = NS_CONTENT_ATTR_NOT_THERE;
  nsIHTMLContent* content = nsnull;
  mContent->QueryInterface(kIHTMLContentIID, (void**) &content);
  if (nsnull != content) {
    nsHTMLValue value;
    result = content->GetHTMLAttribute(nsHTMLAtoms::size, value);
    if (eHTMLUnit_Integer == value.GetUnit()) { 
      *aSize = value.GetIntValue();
    }
    NS_RELEASE(content);
  }
  return result;
}

NS_IMETHODIMP
nsFormControlFrame::GetType(PRInt32* aType) const
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
nsFormControlFrame::GetName(nsString* aResult)
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
nsFormControlFrame::GetValue(nsString* aResult)
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
nsFormControlFrame::IsSuccessful(nsIFormControlFrame* aSubmitter)
{
  nsAutoString name;
  // Since JS Submit() calls are not linked to an element, aSubmitter is null.
  // Return success to allow the call to go through.
  if (aSubmitter == nsnull) return PR_TRUE;
  return (NS_CONTENT_ATTR_HAS_VALUE == GetName(&name));
}

NS_METHOD
nsFormControlFrame::HandleEvent(nsIPresContext* aPresContext, 
                                          nsGUIEvent* aEvent,
                                          nsEventStatus* aEventStatus)
{
  NS_ENSURE_ARG_POINTER(aEventStatus);
  if (nsEventStatus_eConsumeNoDefault == *aEventStatus) {
    return NS_OK;
  }

  // if not native then use the NS_MOUSE_LEFT_CLICK to see if pressed
  // unfortunately native widgets don't seem to handle this right. 
  // so use the old code for native stuff. -EDV
  switch (aEvent->message) {
     case NS_MOUSE_LEFT_CLICK:
        MouseClicked(aPresContext);
     break;

	   case NS_KEY_DOWN:
	    if (NS_KEY_EVENT == aEvent->eventStructType) {
	      nsKeyEvent* keyEvent = (nsKeyEvent*)aEvent;
	      if (NS_VK_RETURN == keyEvent->keyCode) {
	        EnterPressed(aPresContext);
	      }
	      //else if (NS_VK_SPACE == keyEvent->keyCode) {
	      //  MouseClicked(aPresContext);
	      //}
	    }
	    break;
  }

  *aEventStatus = nsEventStatus_eConsumeDoDefault;
  return NS_OK;
}

void 
nsFormControlFrame::GetStyleSize(nsIPresContext* aPresContext,
                                 const nsHTMLReflowState& aReflowState,
                                 nsSize& aSize)
{
  if (aReflowState.mComputedWidth != NS_INTRINSICSIZE) {
    aSize.width = aReflowState.mComputedWidth;
  }
  else {
    aSize.width = CSS_NOTSET;
  }
  if (aReflowState.mComputedHeight != NS_INTRINSICSIZE) {
    aSize.height = aReflowState.mComputedHeight;
  }
  else {
    aSize.height = CSS_NOTSET;
  }
}

void
nsFormControlFrame::Reset(nsIPresContext* aPresContext)
{
}

NS_IMETHODIMP
nsFormControlFrame::GetFormContent(nsIContent*& aContent) const
{
  nsIContent* content;
  nsresult    rv;

  rv = GetContent(&content);
  aContent = content;
  return rv;
}

NS_IMETHODIMP
nsFormControlFrame::GetFont(nsIPresContext* aPresContext, 
                            const nsFont*&  aFont)
{
  return nsFormControlHelper::GetFont(this, aPresContext, mStyleContext, aFont);
}

nsresult
nsFormControlFrame::GetDefaultCheckState(PRBool *aState)
{	nsresult res = NS_OK;
  nsIDOMHTMLInputElement* inputElement;
  if (NS_OK == mContent->QueryInterface(kIDOMHTMLInputElementIID, (void**)&inputElement)) {
    res = inputElement->GetDefaultChecked(aState);
    NS_RELEASE(inputElement);
  }
	return res;
}

nsresult
nsFormControlFrame::SetDefaultCheckState(PRBool aState)
{
	nsresult res = NS_OK;
  nsIDOMHTMLInputElement* inputElement;
  if (NS_OK == mContent->QueryInterface(kIDOMHTMLInputElementIID, (void**)&inputElement)) {
    res = inputElement->SetDefaultChecked(aState);
    NS_RELEASE(inputElement);
  }
	return res;
}

nsresult
nsFormControlFrame::GetCurrentCheckState(PRBool *aState)
{
	nsresult res = NS_OK;
  nsIDOMHTMLInputElement* inputElement;
  if (NS_OK == mContent->QueryInterface(kIDOMHTMLInputElementIID, (void**)&inputElement)) {
    res = inputElement->GetChecked(aState);
    NS_RELEASE(inputElement);
  }
	return res;
}

nsresult
nsFormControlFrame::SetCurrentCheckState(PRBool aState)
{
	nsresult res = NS_OK;
  nsIDOMHTMLInputElement* inputElement;
  if (NS_OK == mContent->QueryInterface(kIDOMHTMLInputElementIID, (void**)&inputElement)) {
    inputElement->SetChecked(aState); 
   NS_RELEASE(inputElement);
  }
	return res;
}

NS_IMETHODIMP
nsFormControlFrame::SetProperty(nsIPresContext* aPresContext, nsIAtom* aName, const nsString& aValue)
{
  return NS_OK;
}

NS_IMETHODIMP
nsFormControlFrame::GetProperty(nsIAtom* aName, nsString& aValue)
{
  return NS_OK;
}

nsresult
nsFormControlFrame::RequiresWidget(PRBool & aRequiresWidget)
{
  aRequiresWidget = PR_TRUE;
  return NS_OK;
}


NS_IMETHODIMP
nsFormControlFrame::SetSuggestedSize(nscoord aWidth, nscoord aHeight)
{
  mSuggestedWidth = aWidth;
  mSuggestedHeight = aHeight;
  return NS_OK;
}

//-------------------------------------------------------------------------
// static 
nsresult nsFormControlFrame::PaintSpecialBorder(nsIPresContext* aPresContext, 
                                                nsIRenderingContext& aRenderingContext,
                                                nsIFrame *aFrame,
                                                const nsRect& aDirtyRect,
                                                const nsRect& aRect)
{
  NS_ASSERTION( aPresContext, "PresContext cannot be null");
  NS_ASSERTION( aFrame, "Frame cannot be null");

  nsresult rv=NS_OK;
#if 0
  if(aPresContext && aFrame){
    // first probe for the pseudo-style context for the frame

    nsCOMPtr<nsIContent> content;
    aFrame->GetContent(getter_AddRefs(content));
    nsCOMPtr<nsIStyleContext> context;
    aFrame->GetStyleContext(getter_AddRefs(context));
    nsCOMPtr<nsIStyleContext> specialBorderStyle;

    // style for the inner such as a dotted line (Windows)
    aPresContext->ProbePseudoStyleContextFor(content, nsHTMLAtoms::mozControlSpecialBorderPseudo, context,
                                            PR_FALSE,
                                            getter_AddRefs(specialBorderStyle));
    if (specialBorderStyle){
      // paint the border

      const nsStyleSpacing* spacing = (const nsStyleSpacing*)specialBorderStyle ->GetStyleData(eStyleStruct_Spacing);
      nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, aFrame,
                                  aDirtyRect, aRect, *spacing, specialBorderStyle, 0);
    }

  } else {
    rv = NS_ERROR_NULL_POINTER;
  }
#endif
  return rv;
}
