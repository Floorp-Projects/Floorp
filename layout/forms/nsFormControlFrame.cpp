/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsCOMPtr.h"
#include "nsFormControlFrame.h"
#include "nsHTMLParts.h"
#include "nsIHTMLContent.h"
#include "nsIRenderingContext.h"
#include "nsIPresShell.h"
#include "nsPresContext.h"
#include "nsLeafFrame.h"
#include "nsCSSRendering.h"
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
#include "nsIContent.h"
#include "nsINameSpaceManager.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIDOMHTMLLabelElement.h"
#include "nsIDOMHTMLTextAreaElement.h"
#include "nsIDOMHTMLLegendElement.h"
#include "nsIDOMHTMLButtonElement.h"
#include "nsIEventStateManager.h"
#include "nsIScrollableView.h"
#include "nsILookAndFeel.h"

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

nsFormControlFrame::nsFormControlFrame()
  : nsLeafFrame()
{
  mDidInit        = PR_FALSE;
  mSuggestedWidth = NS_FORMSIZE_NOTSET;
  mSuggestedHeight = NS_FORMSIZE_NOTSET;
  mPresContext    = nsnull;

  // Reflow Optimization
  mCacheSize.width             = kSizeNotSet;
  mCacheSize.height            = kSizeNotSet;
  mCachedMaxElementWidth       = kSizeNotSet;
}

nsFormControlFrame::~nsFormControlFrame()
{
}

NS_IMETHODIMP
nsFormControlFrame::Destroy(nsPresContext *aPresContext)
{
  // XXXldb Do we really need to do this?  Shouldn't only those frames
  // that use it do it?
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
                                          nscoord& aCachedAscent,
                                          nscoord& aCachedMaxElementWidth,
                                          nsHTMLReflowMetrics& aDesiredSize)
{
  aCacheSize.width  = aDesiredSize.width;
  aCacheSize.height = aDesiredSize.height;
  aCachedAscent = aDesiredSize.ascent;
  if (aDesiredSize.mComputeMEW) {
    aCachedMaxElementWidth  = aDesiredSize.mMaxElementWidth;
  }
}

#if 0 // Testing out changes
//------------------------------------------------------------
void nsFormControlFrame::SkipResizeReflow(nsSize& aCacheSize,
                                          nscoord& aCachedMaxElementWidth,
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

      if (aDesiredSize.mComputeMEW) {
        aDesiredSize.mMaxElementWidth  = aCachedMaxElementWidth;
      }
      aDesiredSize.ascent = aDesiredSize.height;
      aDesiredSize.descent = 0;
    }
  }
}
#else
//------------------------------------------------------------
void nsFormControlFrame::SkipResizeReflow(nsSize& aCacheSize,
                                          nscoord& aCachedAscent,
                                          nscoord& aCachedMaxElementWidth,
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
      aDesiredSize.ascent = aCachedAscent;
      aDesiredSize.descent = aDesiredSize.height - aDesiredSize.ascent;

      if (aDesiredSize.mComputeMEW) {
        aDesiredSize.mMaxElementWidth = aCachedMaxElementWidth;
      }
    }
  }
}
#endif

nscoord 
nsFormControlFrame::GetScrollbarWidth(float aPixToTwip)
{
   return NSIntPixelsToTwips(19, aPixToTwip);  // XXX this is windows
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
nsFormControlFrame::Paint(nsPresContext*      aPresContext,
                          nsIRenderingContext& aRenderingContext,
                          const nsRect&        aDirtyRect,
                          nsFramePaintLayer    aWhichLayer,
                          PRUint32             aFlags)
{
  PRBool isVisible;
  if (NS_SUCCEEDED(IsVisibleForPainting(aPresContext, aRenderingContext, PR_TRUE, &isVisible)) && !isVisible) {
    return NS_OK;
  }

  nsresult rv = NS_OK;
  if (aWhichLayer == NS_FRAME_PAINT_LAYER_FOREGROUND) {
    rv = nsLeafFrame::Paint(aPresContext, aRenderingContext, aDirtyRect,
                            NS_FRAME_PAINT_LAYER_BACKGROUND);
    if (NS_FAILED(rv))
      return rv;
    // draw selection borders when appropriate
    rv = nsFrame::Paint(aPresContext, aRenderingContext, aDirtyRect,
                            NS_FRAME_PAINT_LAYER_BACKGROUND);
    if (NS_FAILED(rv))
      return rv;

    rv = nsLeafFrame::Paint(aPresContext, aRenderingContext, aDirtyRect,
                            NS_FRAME_PAINT_LAYER_FLOATS);
    if (NS_FAILED(rv))
      return rv;
    // draw selection borders when appropriate
    rv = nsFrame::Paint(aPresContext, aRenderingContext, aDirtyRect,
                            NS_FRAME_PAINT_LAYER_FLOATS);
    if (NS_FAILED(rv))
      return rv;

    rv = nsLeafFrame::Paint(aPresContext, aRenderingContext, aDirtyRect,
                            NS_FRAME_PAINT_LAYER_FOREGROUND);
    if (NS_FAILED(rv))
      return rv;
    // draw selection borders when appropriate
    rv = nsFrame::Paint(aPresContext, aRenderingContext, aDirtyRect,
                            NS_FRAME_PAINT_LAYER_FOREGROUND);
  }
  return rv;
}

NS_IMETHODIMP
nsFormControlFrame::GetFrameForPoint(nsPresContext* aPresContext,
                                     const nsPoint& aPoint,
                                     nsFramePaintLayer aWhichLayer,
                                     nsIFrame** aFrame)
{
  nsresult rv = NS_ERROR_FAILURE;
  if (aWhichLayer == NS_FRAME_PAINT_LAYER_FOREGROUND) {
    rv = nsLeafFrame::GetFrameForPoint(aPresContext, aPoint,
                                      NS_FRAME_PAINT_LAYER_FOREGROUND, aFrame);
    if (NS_SUCCEEDED(rv))
      return NS_OK;
    rv = nsLeafFrame::GetFrameForPoint(aPresContext, aPoint,
                                       NS_FRAME_PAINT_LAYER_FLOATS, aFrame);
    if (NS_SUCCEEDED(rv))
      return NS_OK;
    rv = nsLeafFrame::GetFrameForPoint(aPresContext, aPoint,
                                      NS_FRAME_PAINT_LAYER_BACKGROUND, aFrame);
  }
  return rv;
}

void 
nsFormControlFrame::GetDesiredSize(nsPresContext*          aPresContext,
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
  if (aDesiredLayoutSize.mComputeMEW) {
    aDesiredLayoutSize.mMaxElementWidth = aDesiredLayoutSize.width;
  }
  aDesiredWidgetSize.width  = aDesiredLayoutSize.width;
  aDesiredWidgetSize.height = aDesiredLayoutSize.height;
}

void 
nsFormControlFrame::GetDesiredSize(nsPresContext* aPresContext,
                             const nsHTMLReflowState& aReflowState,
                             nsHTMLReflowMetrics& aDesiredSize)
{
  nsSize ignore;
  GetDesiredSize(aPresContext, aReflowState, aDesiredSize, ignore);
}

NS_IMETHODIMP
nsFormControlFrame::DidReflow(nsPresContext*           aPresContext,
                              const nsHTMLReflowState*  aReflowState,
                              nsDidReflowStatus         aStatus)
{
  nsresult rv = nsLeafFrame::DidReflow(aPresContext, aReflowState, aStatus);


  // The view is created hidden; once we have reflowed it and it has been
  // positioned then we show it.
  if (NS_FRAME_REFLOW_FINISHED == aStatus) {
    nsIView* view = GetView();
    if (view) {
      nsViewVisibility newVis = GetStyleVisibility()->IsVisible()
                                  ? nsViewVisibility_kShow
                                  : nsViewVisibility_kHide;
      // only change if different.
      if (newVis != view->GetVisibility()) {
        nsIViewManager* vm = view->GetViewManager();
        if (vm) {
          vm->SetViewVisibility(view, newVis);
        }
      }
    }
  }
  
  return rv;
}

NS_IMETHODIMP
nsFormControlFrame::SetInitialChildList(nsPresContext* aPresContext,
                                        nsIAtom*        aListName,
                                        nsIFrame*       aChildList)
{
  return NS_OK;
}

NS_METHOD
nsFormControlFrame::Reflow(nsPresContext*          aPresContext,
                           nsHTMLReflowMetrics&     aDesiredSize,
                           const nsHTMLReflowState& aReflowState,
                           nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsFormControlFrame", aReflowState.reason);
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);

  if (!mDidInit) {
    mPresContext = aPresContext;
    InitializeControl(aPresContext);
    mDidInit = PR_TRUE;
  }

#if 0
  nsresult skiprv = SkipResizeReflow(mCacheSize, mCachedMaxElementWidth, aPresContext, 
                                     aDesiredSize, aReflowState, aStatus);
  if (NS_SUCCEEDED(skiprv)) {
    return skiprv;
  }
#endif

  nsresult rv = nsLeafFrame::Reflow(aPresContext, aDesiredSize, aReflowState, aStatus);

  aStatus = NS_FRAME_COMPLETE;
  SetupCachedSizes(mCacheSize, mCachedAscent, mCachedMaxElementWidth, aDesiredSize);
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);
  return rv;
}


nsWidgetInitData* 
nsFormControlFrame::GetWidgetInitData(nsPresContext* aPresContext)
{
  return nsnull;
}


nsresult
nsFormControlFrame::RegUnRegAccessKey(nsPresContext* aPresContext, nsIFrame * aFrame, PRBool aDoReg)
{
  NS_ASSERTION(aPresContext, "aPresContext is NULL in RegUnRegAccessKey!");
  NS_ASSERTION(aFrame, "aFrame is NULL in RegUnRegAccessKey!");

  nsresult rv = NS_ERROR_FAILURE;
  nsAutoString accessKey;

  if (aFrame) {
    nsIContent* content = aFrame->GetContent();
#if 1
      nsAutoString resultValue;
      rv = content->GetAttr(kNameSpaceID_None,
                            nsHTMLAtoms::accesskey, accessKey);
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

  if (!accessKey.IsEmpty()) {
    nsIEventStateManager *stateManager = aPresContext->EventStateManager();
    if (aDoReg) {
      return stateManager->RegisterAccessKey(aFrame->GetContent(), (PRUint32)accessKey.First());
    } else {
      return stateManager->UnregisterAccessKey(aFrame->GetContent(), (PRUint32)accessKey.First());
    }
  }
  return NS_ERROR_FAILURE;
}

void 
nsFormControlFrame::InitializeControl(nsPresContext* aPresContext)
{
  RegUnRegAccessKey(aPresContext, NS_STATIC_CAST(nsIFrame*, this), PR_TRUE);
}

void 
nsFormControlFrame::SetFocus(PRBool aOn, PRBool aRepaint)
{
}

void
nsFormControlFrame::ScrollIntoView(nsPresContext* aPresContext)
{
  if (aPresContext) {
    nsIPresShell *presShell = aPresContext->GetPresShell();
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

  nsCOMPtr<nsIHTMLContent> content(do_QueryInterface(mContent));

  if (content) {
    nsHTMLValue value;
    result = content->GetHTMLAttribute(nsHTMLAtoms::maxlength, value);
    if (eHTMLUnit_Integer == value.GetUnit()) { 
      *aSize = value.GetIntValue();
    }
  }
  return result;
}

nsresult
nsFormControlFrame::GetSizeFromContent(PRInt32* aSize) const
{
  *aSize = -1;
  nsresult result = NS_CONTENT_ATTR_NOT_THERE;

  nsCOMPtr<nsIHTMLContent> content(do_QueryInterface(mContent));

  if (content) {
    nsHTMLValue value;
    result = content->GetHTMLAttribute(nsHTMLAtoms::size, value);
    if (eHTMLUnit_Integer == value.GetUnit()) { 
      *aSize = value.GetIntValue();
    }
  }
  return result;
}

NS_IMETHODIMP_(PRInt32)
nsFormControlFrame::GetFormControlType() const
{
  return nsFormControlHelper::GetType(mContent);
}

NS_IMETHODIMP
nsFormControlFrame::GetName(nsAString* aResult)
{
  return nsFormControlHelper::GetName(mContent, aResult);
}


NS_IMETHODIMP
nsFormControlFrame::GetValue(nsAString* aResult)
{
  return nsFormControlHelper::GetValueAttr(mContent, aResult);
}


NS_METHOD
nsFormControlFrame::HandleEvent(nsPresContext* aPresContext, 
                                          nsGUIEvent* aEvent,
                                          nsEventStatus* aEventStatus)
{
  NS_ENSURE_ARG_POINTER(aEventStatus);
  if (nsEventStatus_eConsumeNoDefault == *aEventStatus) {
    return NS_OK;
  }

  // Check for user-input:none style
  const nsStyleUserInterface* uiStyle = GetStyleUserInterface();
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
nsFormControlFrame::GetStyleSize(nsPresContext* aPresContext,
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

NS_IMETHODIMP
nsFormControlFrame::GetFormContent(nsIContent*& aContent) const
{
  aContent = GetContent();
  NS_IF_ADDREF(aContent);
  return NS_OK;
}

nsresult
nsFormControlFrame::GetDefaultCheckState(PRBool *aState)
{
  nsresult res = NS_OK;
  nsCOMPtr<nsIDOMHTMLInputElement> inputElement = do_QueryInterface(mContent);
  if (inputElement) {
    res = inputElement->GetDefaultChecked(aState);
  }
	return res;
}

nsresult
nsFormControlFrame::SetDefaultCheckState(PRBool aState)
{
	nsresult res = NS_OK;
  nsCOMPtr<nsIDOMHTMLInputElement> inputElement = do_QueryInterface(mContent);
  if (inputElement) {
    res = inputElement->SetDefaultChecked(aState);
  }
	return res;
}

nsresult
nsFormControlFrame::GetCurrentCheckState(PRBool *aState)
{
	nsresult res = NS_OK;
  nsCOMPtr<nsIDOMHTMLInputElement> inputElement = do_QueryInterface(mContent);
  if (inputElement) {
    res = inputElement->GetChecked(aState);
  }
	return res;
}

nsresult
nsFormControlFrame::SetCurrentCheckState(PRBool aState)
{
	nsresult res = NS_OK;
  nsCOMPtr<nsIDOMHTMLInputElement> inputElement = do_QueryInterface(mContent);
  if (inputElement) {
    inputElement->SetChecked(aState); 
  }
	return res;
}

NS_IMETHODIMP
nsFormControlFrame::SetProperty(nsPresContext* aPresContext, nsIAtom* aName, const nsAString& aValue)
{
  return NS_OK;
}

NS_IMETHODIMP
nsFormControlFrame::GetProperty(nsIAtom* aName, nsAString& aValue)
{
  aValue.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
nsFormControlFrame::SetSuggestedSize(nscoord aWidth, nscoord aHeight)
{
  mSuggestedWidth = aWidth;
  mSuggestedHeight = aHeight;
  return NS_OK;
}

nsresult 
nsFormControlFrame::GetScreenHeight(nsPresContext* aPresContext,
                                    nscoord& aHeight)
{
  nsRect screen;

  nsIDeviceContext *context = aPresContext->DeviceContext();
  PRBool dropdownCanOverlapOSBar = PR_FALSE;
  nsILookAndFeel *lookAndFeel = aPresContext->LookAndFeel();
  lookAndFeel->GetMetric(nsILookAndFeel::eMetric_MenusCanOverlapOSBar,
                         dropdownCanOverlapOSBar);
  if ( dropdownCanOverlapOSBar )
    context->GetRect ( screen );
  else
    context->GetClientRect(screen);
      
  float devUnits;
  devUnits = context->DevUnitsToAppUnits();
  aHeight = NSToIntRound(float(screen.height) / devUnits );
  return NS_OK;
}

// Calculate a frame's position in screen coordinates
nsresult
nsFormControlFrame::GetAbsoluteFramePosition(nsPresContext* aPresContext,
                                             nsIFrame *aFrame, 
                                             nsRect& aAbsoluteTwipsRect, 
                                             nsRect& aAbsolutePixelRect)
{
  nsresult rv = NS_OK;
 
  aAbsoluteTwipsRect = aFrame->GetRect();
  // zero these out, 
  // because the GetOffsetFromView figures them out
  // XXXbz why do we need to do this, really?  Will we ever fail to
  // get a containing view?
  aAbsoluteTwipsRect.x = 0;
  aAbsoluteTwipsRect.y = 0;

  // Get conversions between twips and pixels
  float t2p;
  float p2t;
  t2p = aPresContext->TwipsToPixels();
  p2t = aPresContext->PixelsToTwips();

  // Start with frame's offset from it it's containing view
  nsIView *view = nsnull;
  nsPoint frameOffset;
  rv = aFrame->GetOffsetFromView(aPresContext, frameOffset, &view);

  if (NS_SUCCEEDED(rv) && view) {
    aAbsoluteTwipsRect.MoveTo(frameOffset);

    nsIWidget* widget;
    // Walk up the views, looking for a widget
    do { 
      // add in the offset of the view from its parent.
      aAbsoluteTwipsRect += view->GetPosition();

      widget = view->GetWidget();
      if (widget) {
        // account for space above and to the left of the view origin.
        // the widget is aligned with view's bounds, not its origin
        
        nsRect bounds = view->GetBounds();
        aAbsoluteTwipsRect.x -= bounds.x;
        aAbsoluteTwipsRect.y -= bounds.y;

        // Add in the absolute offset of the widget.
        nsRect absBounds;
        nsRect zeroRect;
        // XXX a twip version of this would be really nice here!
        widget->WidgetToScreen(zeroRect, absBounds);
          // Convert widget coordinates to twips   
        aAbsoluteTwipsRect.x += NSIntPixelsToTwips(absBounds.x, p2t);
        aAbsoluteTwipsRect.y += NSIntPixelsToTwips(absBounds.y, p2t);   
        break;
      }

      view = view->GetParent();
    } while (view);
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
