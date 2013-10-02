/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsFormControlFrame.h"
#include "nsGkAtoms.h"
#include "nsLayoutUtils.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsEventStateManager.h"
#include "mozilla/LookAndFeel.h"
#include "nsDeviceContext.h"

using namespace mozilla;

//#define FCF_NOISY

nsFormControlFrame::nsFormControlFrame(nsStyleContext* aContext) :
  nsLeafFrame(aContext)
{
}

nsFormControlFrame::~nsFormControlFrame()
{
}

nsIAtom*
nsFormControlFrame::GetType() const
{
  return nsGkAtoms::formControlFrame; 
}

void
nsFormControlFrame::DestroyFrom(nsIFrame* aDestructRoot)
{
  // Unregister the access key registered in reflow
  nsFormControlFrame::RegUnRegAccessKey(static_cast<nsIFrame*>(this), false);
  nsLeafFrame::DestroyFrom(aDestructRoot);
}

NS_QUERYFRAME_HEAD(nsFormControlFrame)
  NS_QUERYFRAME_ENTRY(nsIFormControlFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsLeafFrame)

NS_IMPL_FRAMEARENA_HELPERS(nsFormControlFrame)

nscoord
nsFormControlFrame::GetIntrinsicWidth()
{
  // Provide a reasonable default for sites that use an "auto" height.
  // Note that if you change this, you should change the values in forms.css
  // as well.  This is the 13px default width minus the 2px default border.
  return nsPresContext::CSSPixelsToAppUnits(13 - 2 * 2);
}

nscoord
nsFormControlFrame::GetIntrinsicHeight()
{
  // Provide a reasonable default for sites that use an "auto" height.
  // Note that if you change this, you should change the values in forms.css
  // as well. This is the 13px default width minus the 2px default border.
  return nsPresContext::CSSPixelsToAppUnits(13 - 2 * 2);
}

nscoord
nsFormControlFrame::GetBaseline() const
{
  NS_ASSERTION(!NS_SUBTREE_DIRTY(this),
               "frame must not be dirty");
  // Treat radio buttons and checkboxes as having an intrinsic baseline
  // at the bottom of the control (use the bottom content edge rather
  // than the bottom margin edge).
  return mRect.height - GetUsedBorderAndPadding().bottom;
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
    RegUnRegAccessKey(static_cast<nsIFrame*>(this), true);
  }

  nsresult rv = nsLeafFrame::Reflow(aPresContext, aDesiredSize, aReflowState,
                                    aStatus);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (nsLayoutUtils::FontSizeInflationEnabled(aPresContext)) {
    float inflation = nsLayoutUtils::FontSizeInflationFor(this);
    aDesiredSize.width *= inflation;
    aDesiredSize.height *= inflation;
    aDesiredSize.UnionOverflowAreasWithDesiredBounds();
    FinishAndStoreOverflow(&aDesiredSize);
  }
  return NS_OK;
}

nsresult
nsFormControlFrame::RegUnRegAccessKey(nsIFrame * aFrame, bool aDoReg)
{
  NS_ENSURE_ARG_POINTER(aFrame);
  
  nsPresContext* presContext = aFrame->PresContext();
  
  NS_ASSERTION(presContext, "aPresContext is NULL in RegUnRegAccessKey!");

  nsAutoString accessKey;

  nsIContent* content = aFrame->GetContent();
  content->GetAttr(kNameSpaceID_None, nsGkAtoms::accesskey, accessKey);
  if (!accessKey.IsEmpty()) {
    nsEventStateManager *stateManager = presContext->EventStateManager();
    if (aDoReg) {
      stateManager->RegisterAccessKey(content, (uint32_t)accessKey.First());
    } else {
      stateManager->UnregisterAccessKey(content, (uint32_t)accessKey.First());
    }
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

void 
nsFormControlFrame::SetFocus(bool aOn, bool aRepaint)
{
}

NS_METHOD
nsFormControlFrame::HandleEvent(nsPresContext* aPresContext, 
                                WidgetGUIEvent* aEvent,
                                nsEventStatus* aEventStatus)
{
  // Check for user-input:none style
  const nsStyleUserInterface* uiStyle = StyleUserInterface();
  if (uiStyle->mUserInput == NS_STYLE_USER_INPUT_NONE ||
      uiStyle->mUserInput == NS_STYLE_USER_INPUT_DISABLED)
    return nsFrame::HandleEvent(aPresContext, aEvent, aEventStatus);

  return NS_OK;
}

void
nsFormControlFrame::GetCurrentCheckState(bool *aState)
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

// static
nsRect
nsFormControlFrame::GetUsableScreenRect(nsPresContext* aPresContext)
{
  nsRect screen;

  nsDeviceContext *context = aPresContext->DeviceContext();
  int32_t dropdownCanOverlapOSBar =
    LookAndFeel::GetInt(LookAndFeel::eIntID_MenusCanOverlapOSBar, 0);
  if ( dropdownCanOverlapOSBar )
    context->GetRect(screen);
  else
    context->GetClientRect(screen);

  return screen;
}
