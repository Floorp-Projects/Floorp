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
#include "nsIScrollableView.h"

#ifdef DEBUG_evaughan
//#define DEBUG_rods
#endif

#ifdef DEBUG_rods
//#define FCF_NOISY
#endif

#ifdef FCF_NOISY
#define REFLOW_DEBUG_MSG(_msg1) printf((_msg1))
#define REFLOW_DEBUG_MSG2(_msg1, _msg2) printf((_msg1), (_msg2))
#define REFLOW_DEBUG_MSG3(_msg1, _msg2, _msg3) printf((_msg1), (_msg2), (_msg3))
#define REFLOW_DEBUG_MSG4(_msg1, _msg2, _msg3, _msg4) printf((_msg1), (_msg2), (_msg3), (_msg4))

#define IF_REFLOW_DEBUG_MSG(_bool, _msg1) if ((_bool)) printf((_msg1))
#define IF_REFLOW_DEBUG_MSG2(_bool, _msg1, _msg2) if ((_bool)) printf((_msg1), (_msg2))
#define IF_REFLOW_DEBUG_MSG3(_bool, _msg1, _msg2, _msg3) if ((_bool)) printf((_msg1), (_msg2), (_msg3))
#define IF_REFLOW_DEBUG_MSG4(_bool, _msg1, _msg2, _msg3, _msg4) if ((_bool)) printf((_msg1), (_msg2), (_msg3), (_msg4))

#else //-------------
#define REFLOW_DEBUG_MSG(_msg) 
#define REFLOW_DEBUG_MSG2(_msg1, _msg2) 
#define REFLOW_DEBUG_MSG3(_msg1, _msg2, _msg3) 
#define REFLOW_DEBUG_MSG4(_msg1, _msg2, _msg3, _msg4) 

#define IF_REFLOW_DEBUG_MSG(_bool, _msg) 
#define IF_REFLOW_DEBUG_MSG2(_bool, _msg1, _msg2) 
#define IF_REFLOW_DEBUG_MSG3(_bool, _msg1, _msg2, _msg3) 
#define IF_REFLOW_DEBUG_MSG4(_bool, _msg1, _msg2, _msg3, _msg4) 
#endif

const PRInt32 kSizeNotSet = -1;

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kViewCID, NS_VIEW_CID);

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
}

NS_IMETHODIMP
nsFormControlFrame::Destroy(nsIPresContext *aPresContext)
{
  nsFormControlFrame::RegUnRegAccessKey(aPresContext, NS_STATIC_CAST(nsIFrame*, this), PR_FALSE);
  return nsLeafFrame::Destroy(aPresContext);
}

// Frames are not refcounted, no need to AddRef
NS_IMETHODIMP
nsFormControlFrame::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  NS_PRECONDITION(0 != aInstancePtr, "null ptr");
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(NS_GET_IID(nsIFormControlFrame))) {
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

#if 0 // Testing out changes
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

  if (aReflowState.reason == eReflowReason_Incremental ||
      aReflowState.reason == eReflowReason_Dirty) {
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
        IF_REFLOW_DEBUG_MSG2(aBailOnWidth, "-------------- #1 Bailing on aCachedAvailableSize.width %d != kSizeNotSet\n", aCachedAvailableSize.width);
      } else {
        //width = aReflowState.availableWidth - aReflowState.mComputedBorderPadding.left -
        //        aReflowState.mComputedBorderPadding.right;
        aBailOnWidth = aCacheSize.width <= aReflowState.availableWidth && aCacheSize.width != kSizeNotSet;

        if (aBailOnWidth) {
          REFLOW_DEBUG_MSG3("-------------- #2 Bailing on aCachedSize.width %d <= (AW - BP) %d\n", aCachedAvailableSize.width, width );
        } else {
          //aBailOnWidth = width <= (aCacheSize.width - aReflowState.mComputedBorderPadding.left - aReflowState.mComputedBorderPadding.right) &&
          //               aCachedAvailableSize.width == kSizeNotSet;
          //if (aBailOnWidth) {
          //  REFLOW_DEBUG_MSG3("-------------- #2.2 Bailing on width %d <= aCachedSize.width %d\n", width, (aCacheSize.width - aReflowState.mComputedBorderPadding.left - aReflowState.mComputedBorderPadding.right));
          //}        
        }
      }
    } else {
      width = aReflowState.mComputedWidth;
      aBailOnWidth = width == (aCacheSize.width - aReflowState.mComputedBorderPadding.left - aReflowState.mComputedBorderPadding.right);
      IF_REFLOW_DEBUG_MSG3(aBailOnWidth, "-------------- #3 Bailing on aCachedAvailableSize.width %d == aReflowState.mComputedWidth %d\n", aCachedAvailableSize.width, width );
    }
    
    nscoord height;
    if (NS_UNCONSTRAINEDSIZE == aReflowState.mComputedHeight) {
      if (aReflowState.availableHeight == NS_UNCONSTRAINEDSIZE) {
        height = NS_UNCONSTRAINEDSIZE;
        aBailOnHeight = aCacheSize.height != kSizeNotSet;
        if (aBailOnHeight) {
          IF_REFLOW_DEBUG_MSG2(aBailOnHeight, "-------------- #1 Bailing on aCachedAvailableSize.height %d != kSizeNotSet\n", aCachedAvailableSize.height);
        }
      } else {
        height = aReflowState.availableHeight - aReflowState.mComputedBorderPadding.left -
                aReflowState.mComputedBorderPadding.right;
        aBailOnHeight = aCachedAvailableSize.height <= height && aCachedAvailableSize.height != kSizeNotSet;
        if (aBailOnHeight) {
          REFLOW_DEBUG_MSG3("-------------- #2 Bailing on aCachedAvailableSize.height %d <= height %d\n", aCachedAvailableSize.height, height );
        } else {
          aBailOnHeight = height <= (aCacheSize.height - aReflowState.mComputedBorderPadding.left - aReflowState.mComputedBorderPadding.right) &&
                         aCachedAvailableSize.height == kSizeNotSet;
          if (aBailOnHeight) {
            REFLOW_DEBUG_MSG3("-------------- #2.2 Bailing on height %d <= aCachedSize.height %d\n", height, (aCacheSize.height - aReflowState.mComputedBorderPadding.left - aReflowState.mComputedBorderPadding.right));
          }        
        }
      }
    } else {
      height = aReflowState.mComputedHeight;
        aBailOnHeight = height == (aCacheSize.height - aReflowState.mComputedBorderPadding.left - aReflowState.mComputedBorderPadding.right);
        IF_REFLOW_DEBUG_MSG3(aBailOnHeight, "-------------- #3 Bailing on aCachedAvailableSize.height %d == aReflowState.mComputedHeight %d\n", aCachedAvailableSize.height, height );
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
#else
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

  if (aReflowState.reason == eReflowReason_Incremental ||
#ifdef IBMBIDI
      aReflowState.reason == eReflowReason_StyleChange ||
#endif
      aReflowState.reason == eReflowReason_Dirty) {
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
        } else {
          aBailOnWidth = width <= (aCacheSize.width - aReflowState.mComputedBorderPadding.left - aReflowState.mComputedBorderPadding.right) &&
                         aCachedAvailableSize.width == kSizeNotSet;
          if (aBailOnWidth) {
            printf("-------------- #2.2 Bailing on width %d <= aCachedAvailableSize.width %d\n",(aCacheSize.width - aReflowState.mComputedBorderPadding.left - aReflowState.mComputedBorderPadding.right), width );
          }        
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
        aBailOnHeight = aCacheSize.height != kSizeNotSet;
#ifdef FCF_NOISY
        if (aBailOnHeight) {
          printf("-------------- #1 Bailing on aCachedAvailableSize.height %d != kSizeNotSet\n", aCachedAvailableSize.height);
        }
#endif
      } else {
        height = aReflowState.availableHeight - aReflowState.mComputedBorderPadding.left -
                aReflowState.mComputedBorderPadding.right;
        aBailOnHeight = aCachedAvailableSize.height <= height && aCachedAvailableSize.height != kSizeNotSet;
#ifdef FCF_NOISY
        if (aBailOnHeight) {
          printf("-------------- #2 Bailing on aCachedAvailableSize.height %d <= height %d\n", aCachedAvailableSize.height, height );
        } else {
          aBailOnHeight = height <= (aCacheSize.height - aReflowState.mComputedBorderPadding.left - aReflowState.mComputedBorderPadding.right) &&
                         aCachedAvailableSize.height == kSizeNotSet;
          if (aBailOnHeight) {
            printf("-------------- #2.2 Bailing on height %d <= aCachedAvailableSize.height %d\n",(aCacheSize.height - aReflowState.mComputedBorderPadding.left - aReflowState.mComputedBorderPadding.right), height );
          }        
        }
#endif
      }
    } else {
      height = aReflowState.mComputedHeight;
      //if (aCachedAvailableSize.height == kSizeNotSet) {
      //  //aBailOnHeight = aCachedAvailableSize.height == aCacheSize.height;
        aBailOnHeight = PR_FALSE;
      //} else {
        aBailOnHeight = height == (aCacheSize.height - aReflowState.mComputedBorderPadding.left - aReflowState.mComputedBorderPadding.right);
      //}
#ifdef FCF_NOISY
      if (aBailOnHeight) {
        printf("-------------- #3 Bailing on aCachedAvailableSize.height %d == aReflowState.mComputedHeight %d\n", aCachedAvailableSize.height, height );
      }
#endif
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
#endif

nscoord 
nsFormControlFrame::GetScrollbarWidth(float aPixToTwip)
{
   return NSIntPixelsToTwips(19, aPixToTwip);  // XXX this is windows
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
nsFormControlFrame::Paint(nsIPresContext*      aPresContext,
                          nsIRenderingContext& aRenderingContext,
                          const nsRect&        aDirtyRect,
                          nsFramePaintLayer    aWhichLayer,
                          PRUint32             aFlags)
{
  PRBool isVisible;
  if (NS_SUCCEEDED(IsVisibleForPainting(aPresContext, aRenderingContext, PR_TRUE, &isVisible)) && !isVisible) {
    return NS_OK;
  }

  nsresult rv = nsLeafFrame::Paint(aPresContext, aRenderingContext, aDirtyRect,
                            aWhichLayer);

 if (NS_FRAME_PAINT_LAYER_BACKGROUND == aWhichLayer)
 {
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
      const nsStyleVisibility* vis;
      GetStyleData(eStyleStruct_Visibility, ((const nsStyleStruct *&)vis));
      nsViewVisibility newVis = vis->IsVisible() ? nsViewVisibility_kShow : nsViewVisibility_kHide;
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
  DO_GLOBAL_REFLOW_COUNT("nsFormControlFrame", aReflowState.reason);

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
      rv = content->GetAttr(nameSpaceID, nsHTMLAtoms::accesskey, accessKey);
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
        return stateManager->RegisterAccessKey(aFrame, nsnull, (PRUint32)accessKey.First());
      } else {
        return stateManager->UnregisterAccessKey(aFrame, nsnull, (PRUint32)accessKey.First());
      }
    }
  }
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
    if (presShell) {
      presShell->ScrollFrameIntoView(this,
                   NS_PRESSHELL_SCROLL_IF_NOT_VISIBLE,NS_PRESSHELL_SCROLL_IF_NOT_VISIBLE);
    }
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
  return NS_GET_IID(nsIButton);
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
    result = mContent->QueryInterface(NS_GET_IID(nsIFormControl), (void**)&formControl);
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
  PRBool disabled = PR_FALSE;
  nsFormControlHelper::GetDisabled(mContent, &disabled);

  // Since JS Submit() calls are not linked to an element, aSubmitter is null.
  // Return success to allow the call to go through.
  if (aSubmitter == nsnull) return PR_TRUE;

  return !disabled && (NS_CONTENT_ATTR_HAS_VALUE == GetName(&name));
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

  // Check for user-input:none style
  const nsStyleUserInterface* uiStyle;
  GetStyleData(eStyleStruct_UserInterface,  (const nsStyleStruct *&)uiStyle);
  if (uiStyle->mUserInput == NS_STYLE_USER_INPUT_NONE || uiStyle->mUserInput == NS_STYLE_USER_INPUT_DISABLED)
    return nsFrame::HandleEvent(aPresContext, aEvent, aEventStatus);

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
  if (NS_OK == mContent->QueryInterface(NS_GET_IID(nsIDOMHTMLInputElement), (void**)&inputElement)) {
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
  if (NS_OK == mContent->QueryInterface(NS_GET_IID(nsIDOMHTMLInputElement), (void**)&inputElement)) {
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
  if (NS_OK == mContent->QueryInterface(NS_GET_IID(nsIDOMHTMLInputElement), (void**)&inputElement)) {
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
  if (NS_OK == mContent->QueryInterface(NS_GET_IID(nsIDOMHTMLInputElement), (void**)&inputElement)) {
    inputElement->SetChecked(aState); 
   NS_RELEASE(inputElement);
  }
	return res;
}

NS_IMETHODIMP
nsFormControlFrame::SetProperty(nsIPresContext* aPresContext, nsIAtom* aName, const nsAReadableString& aValue)
{
  return NS_OK;
}

NS_IMETHODIMP
nsFormControlFrame::GetProperty(nsIAtom* aName, nsAWritableString& aValue)
{
  aValue.Truncate();
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

      const nsStyleBorder* border = (const nsStyleBorder*)specialBorderStyle ->GetStyleData(eStyleStruct_Border);
      nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, aFrame,
                                  aDirtyRect, aRect, *border, specialBorderStyle, 0);
    }

  } else {
    rv = NS_ERROR_NULL_POINTER;
  }
#endif
  return rv;
}

nsresult 
nsFormControlFrame::GetScreenHeight(nsIPresContext* aPresContext, nscoord& aHeight)
{
  aHeight = 0;
  nsIDeviceContext* context;
  aPresContext->GetDeviceContext( &context );
	if ( nsnull != context ) {
		PRInt32 height;
    PRInt32 width;
		context->GetDeviceSurfaceDimensions(width, height);
		float devUnits;
 		context->GetDevUnitsToAppUnits(devUnits);
		aHeight = NSToIntRound(float( height) / devUnits );
		NS_RELEASE( context );
		return NS_OK;
	}

  return NS_ERROR_FAILURE;
}

// Calculate a frame's position in screen coordinates
nsresult
nsFormControlFrame::GetAbsoluteFramePosition(nsIPresContext* aPresContext,
                                             nsIFrame *aFrame, 
                                             nsRect& aAbsoluteTwipsRect, 
                                             nsRect& aAbsolutePixelRect)
{
  //XXX: This code needs to take the view's offset into account when calculating
  //the absolute coordinate of the frame.
  nsresult rv = NS_OK;
 
  aFrame->GetRect(aAbsoluteTwipsRect);
  // zero these out, 
  // because the GetOffsetFromView figures them out
  aAbsoluteTwipsRect.x = 0;
  aAbsoluteTwipsRect.y = 0;

    // Get conversions between twips and pixels
  float t2p;
  float p2t;
  aPresContext->GetTwipsToPixels(&t2p);
  aPresContext->GetPixelsToTwips(&p2t);
  
   // Add in frame's offset from it it's containing view
  nsIView *containingView = nsnull;
  nsPoint offset;
  rv = aFrame->GetOffsetFromView(aPresContext, offset, &containingView);
  if (NS_SUCCEEDED(rv) && (nsnull != containingView)) {
    aAbsoluteTwipsRect.x += offset.x;
    aAbsoluteTwipsRect.y += offset.y;

    nsPoint viewOffset;
    containingView->GetPosition(&viewOffset.x, &viewOffset.y);
    nsIView * parent;
    containingView->GetParent(parent);

    // if we don't have a parent view then 
    // check to see if we have a widget and adjust our offset for the widget
    if (parent == nsnull) {
      nsIWidget * widget;
      containingView->GetWidget(widget);
      if (nsnull != widget) {
        // Add in the absolute offset of the widget.
        nsRect absBounds;
        nsRect lc;
        widget->WidgetToScreen(lc, absBounds);
        // Convert widget coordinates to twips   
        aAbsoluteTwipsRect.x += NSIntPixelsToTwips(absBounds.x, p2t);
        aAbsoluteTwipsRect.y += NSIntPixelsToTwips(absBounds.y, p2t);   
        NS_RELEASE(widget);
      }
      rv = NS_OK;
    } else {

      while (nsnull != parent) {
        nsPoint po;
        parent->GetPosition(&po.x, &po.y);
        viewOffset.x += po.x;
        viewOffset.y += po.y;
        nsIScrollableView * scrollView;
        if (NS_OK == containingView->QueryInterface(NS_GET_IID(nsIScrollableView), (void **)&scrollView)) {
          nscoord x;
          nscoord y;
          scrollView->GetScrollPosition(x, y);
          viewOffset.x -= x;
          viewOffset.y -= y;
        }
        nsIWidget * widget;
        parent->GetWidget(widget);
        if (nsnull != widget) {
          // Add in the absolute offset of the widget.
          nsRect absBounds;
          nsRect lc;
          widget->WidgetToScreen(lc, absBounds);
          // Convert widget coordinates to twips   
          aAbsoluteTwipsRect.x += NSIntPixelsToTwips(absBounds.x, p2t);
          aAbsoluteTwipsRect.y += NSIntPixelsToTwips(absBounds.y, p2t);   
          NS_RELEASE(widget);
          break;
        }
        parent->GetParent(parent);
      }
      aAbsoluteTwipsRect.x += viewOffset.x;
      aAbsoluteTwipsRect.y += viewOffset.y;
    }
  }

   // convert to pixel coordinates
  if (NS_SUCCEEDED(rv)) {
   aAbsolutePixelRect.x = NSTwipsToIntPixels(aAbsoluteTwipsRect.x, t2p);
   aAbsolutePixelRect.y = NSTwipsToIntPixels(aAbsoluteTwipsRect.y, t2p);
   aAbsolutePixelRect.width = NSTwipsToIntPixels(aAbsoluteTwipsRect.width, t2p);
   aAbsolutePixelRect.height = NSTwipsToIntPixels(aAbsoluteTwipsRect.height, t2p);
  }

  return rv;
}
