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

#include "nsFormControlFrame.h"
#include "nsHTMLAtoms.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIEventStateManager.h"
#include "nsIScrollableView.h"
#include "nsILookAndFeel.h"

//#define FCF_NOISY

const PRInt32 kSizeNotSet = -1;

nsFormControlFrame::nsFormControlFrame()
{
  mDidInit        = PR_FALSE;
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

void 
nsFormControlFrame::GetDesiredSize(nsPresContext* aPresContext,
                             const nsHTMLReflowState& aReflowState,
                             nsHTMLReflowMetrics& aDesiredSize)
{
  // get the css size and let the frame use or override it
  nsSize styleSize;
  GetStyleSize(aPresContext, aReflowState, styleSize);

  // subclasses should always override this method, but if not and no css, make it small
  aDesiredSize.width  = (styleSize.width  > CSS_NOTSET) ? styleSize.width  : 144;
  aDesiredSize.height = (styleSize.height > CSS_NOTSET) ? styleSize.height : 144;
  aDesiredSize.ascent = aDesiredSize.height;
  aDesiredSize.descent = 0;
  if (aDesiredSize.mComputeMEW) {
    aDesiredSize.SetMEWToActualWidth(aReflowState.mStylePosition->mWidth.GetUnit());
  }
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
    RegUnRegAccessKey(aPresContext, NS_STATIC_CAST(nsIFrame*, this), PR_TRUE);
    mDidInit = PR_TRUE;
  }

  nsresult rv = nsLeafFrame::Reflow(aPresContext, aDesiredSize, aReflowState, aStatus);

  aStatus = NS_FRAME_COMPLETE;
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);
  aDesiredSize.mOverflowArea = nsRect(0, 0, aDesiredSize.width, aDesiredSize.height);
  FinishAndStoreOverflow(&aDesiredSize);
  return rv;
}

nsresult
nsFormControlFrame::RegUnRegAccessKey(nsPresContext* aPresContext, nsIFrame * aFrame, PRBool aDoReg)
{
  NS_ASSERTION(aPresContext, "aPresContext is NULL in RegUnRegAccessKey!");
  NS_ENSURE_ARG_POINTER(aFrame);

  nsAutoString accessKey;

  nsIContent* content = aFrame->GetContent();
  content->GetAttr(kNameSpaceID_None, nsHTMLAtoms::accesskey, accessKey);
  if (!accessKey.IsEmpty()) {
    nsIEventStateManager *stateManager = aPresContext->EventStateManager();
    if (aDoReg) {
      return stateManager->RegisterAccessKey(content, (PRUint32)accessKey.First());
    } else {
      return stateManager->UnregisterAccessKey(content, (PRUint32)accessKey.First());
    }
  }
  return NS_ERROR_FAILURE;
}

void 
nsFormControlFrame::SetFocus(PRBool aOn, PRBool aRepaint)
{
}

NS_METHOD
nsFormControlFrame::HandleEvent(nsPresContext* aPresContext, 
                                          nsGUIEvent* aEvent,
                                          nsEventStatus* aEventStatus)
{
  // Check for user-input:none style
  const nsStyleUserInterface* uiStyle = GetStyleUserInterface();
  if (uiStyle->mUserInput == NS_STYLE_USER_INPUT_NONE ||
      uiStyle->mUserInput == NS_STYLE_USER_INPUT_DISABLED)
    return nsFrame::HandleEvent(aPresContext, aEvent, aEventStatus);

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

void
nsFormControlFrame::GetCurrentCheckState(PRBool *aState)
{
  nsCOMPtr<nsIDOMHTMLInputElement> inputElement = do_QueryInterface(mContent);
  if (inputElement) {
    inputElement->GetChecked(aState);
  }
}

nsresult
nsFormControlFrame::SetFormProperty(nsIAtom* aName, const nsAString& aValue)
{
  return NS_OK;
}

nsresult
nsFormControlFrame::GetFormProperty(nsIAtom* aName, nsAString& aValue) const
{
  aValue.Truncate();
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
  rv = aFrame->GetOffsetFromView(frameOffset, &view);

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
