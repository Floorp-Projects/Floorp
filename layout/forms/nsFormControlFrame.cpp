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
#include "nsEventStateManager.h"
#include "mozilla/LookAndFeel.h"

using namespace mozilla;

//#define FCF_NOISY

const PRInt32 kSizeNotSet = -1;

nsFormControlFrame::nsFormControlFrame(nsStyleContext* aContext) :
  nsLeafFrame(aContext)
{
}

nsFormControlFrame::~nsFormControlFrame()
{
}

void
nsFormControlFrame::DestroyFrom(nsIFrame* aDestructRoot)
{
  // Unregister the access key registered in reflow
  nsFormControlFrame::RegUnRegAccessKey(static_cast<nsIFrame*>(this), PR_FALSE);
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
    RegUnRegAccessKey(static_cast<nsIFrame*>(this), PR_TRUE);
  }

  return nsLeafFrame::Reflow(aPresContext, aDesiredSize, aReflowState,
                             aStatus);
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
      stateManager->RegisterAccessKey(content, (PRUint32)accessKey.First());
    } else {
      stateManager->UnregisterAccessKey(content, (PRUint32)accessKey.First());
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

nsresult
nsFormControlFrame::GetFormProperty(nsIAtom* aName, nsAString& aValue) const
{
  aValue.Truncate();
  return NS_OK;
}

// static
nsRect
nsFormControlFrame::GetUsableScreenRect(nsPresContext* aPresContext)
{
  nsRect screen;

  nsDeviceContext *context = aPresContext->DeviceContext();
  PRInt32 dropdownCanOverlapOSBar =
    LookAndFeel::GetInt(LookAndFeel::eIntID_MenusCanOverlapOSBar, 0);
  if ( dropdownCanOverlapOSBar )
    context->GetRect(screen);
  else
    context->GetClientRect(screen);

  return screen;
}
