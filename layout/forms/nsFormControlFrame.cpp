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
#include "nsGkAtoms.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIEventStateManager.h"
#include "nsIScrollableView.h"
#include "nsILookAndFeel.h"

//#define FCF_NOISY

const PRInt32 kSizeNotSet = -1;

nsFormControlFrame::nsFormControlFrame(nsStyleContext* aContext) :
  nsLeafFrame(aContext),
  mDidInit(PR_FALSE)
{
}

nsFormControlFrame::~nsFormControlFrame()
{
}

void
nsFormControlFrame::Destroy()
{
  // XXXldb Do we really need to do this?  Shouldn't only those frames
  // that use it do it?
  nsFormControlFrame::RegUnRegAccessKey(NS_STATIC_CAST(nsIFrame*, this), PR_FALSE);
  nsLeafFrame::Destroy();
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

nscoord
nsFormControlFrame::GetIntrinsicWidth()
{
  // Intrinsic width is 144 twips.  Why?  I have no idea; that's what
  // it was before I touched this code, and the original checkin
  // comment is not so helpful.
  return 144;
}

nscoord
nsFormControlFrame::GetIntrinsicHeight()
{
  // Intrinsic height is 144 twips.  Why?  I have no idea; that's what
  // it was before I touched this code, and the original checkin
  // comment is not so helpful.
  return 144;
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
nsFormControlFrame::SetInitialChildList(nsIAtom*        aListName,
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
  DO_GLOBAL_REFLOW_COUNT("nsFormControlFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);

  if (mState & NS_FRAME_FIRST_REFLOW) {
    RegUnRegAccessKey(NS_STATIC_CAST(nsIFrame*, this), PR_TRUE);
  }

  return nsLeafFrame::Reflow(aPresContext, aDesiredSize, aReflowState,
                             aStatus);
}

nsresult
nsFormControlFrame::RegUnRegAccessKey(nsIFrame * aFrame, PRBool aDoReg)
{
  NS_ENSURE_ARG_POINTER(aFrame);
  
  nsPresContext* presContext = aFrame->GetPresContext();
  
  NS_ASSERTION(presContext, "aPresContext is NULL in RegUnRegAccessKey!");

  nsAutoString accessKey;

  nsIContent* content = aFrame->GetContent();
  content->GetAttr(kNameSpaceID_None, nsGkAtoms::accesskey, accessKey);
  if (!accessKey.IsEmpty()) {
    nsIEventStateManager *stateManager = presContext->EventStateManager();
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

  aHeight = aPresContext->AppUnitsToDevPixels(screen.height);
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
        aAbsoluteTwipsRect.x += aPresContext->DevPixelsToAppUnits(absBounds.x);
        aAbsoluteTwipsRect.y += aPresContext->DevPixelsToAppUnits(absBounds.y);   
        break;
      }

      view = view->GetParent();
    } while (view);
  }
  
  // convert to pixel coordinates
  if (NS_SUCCEEDED(rv)) {
    aAbsolutePixelRect.x = aPresContext->AppUnitsToDevPixels(aAbsoluteTwipsRect.x);
    aAbsolutePixelRect.y = aPresContext->AppUnitsToDevPixels(aAbsoluteTwipsRect.y);
    aAbsolutePixelRect.width = aPresContext->AppUnitsToDevPixels(aAbsoluteTwipsRect.width);
    aAbsolutePixelRect.height = aPresContext->AppUnitsToDevPixels(aAbsoluteTwipsRect.height);
  }

  return rv;
}
