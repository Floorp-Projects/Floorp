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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#include "nsCOMPtr.h"
#include "nsXULCheckboxFrame.h"
#include "nsIDOMXULCheckboxElement.h"
#include "nsIContent.h"

//
// NS_NewXULCheckboxFrame
//
// Creates a new checkbox frame and returns it in |aNewFrame|
//
nsresult
NS_NewXULCheckboxFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame )
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsXULCheckboxFrame* it = new (aPresShell) nsXULCheckboxFrame;
  if (nsnull == it)
    return NS_ERROR_OUT_OF_MEMORY;

  // it->SetFlags(aFlags);
  *aNewFrame = it;
  return NS_OK;
  
} // NS_NewXULCheckboxFrame

void nsXULCheckboxFrame::ToggleCheckState()
{
  nsCOMPtr<nsIDOMXULCheckboxElement> element = do_QueryInterface(mContent);
  if(element) {
    PRBool disabled;
    element->GetDisabled(&disabled);
    if(!disabled) {
      PRBool checked;
      element->GetChecked(&checked);
      element->SetChecked(!checked);
    }
  }
}

NS_IMETHODIMP nsXULCheckboxFrame::HandleEvent(
  nsIPresContext* aPresContext, 
  nsGUIEvent* aEvent,
  nsEventStatus* aEventStatus)
{
    switch (aEvent->message) {
      case NS_MOUSE_LEFT_BUTTON_UP:
        ToggleCheckState();
        break;
      case NS_KEY_PRESS:
        if (NS_KEY_EVENT == aEvent->eventStructType) {
          nsKeyEvent* keyEvent = (nsKeyEvent*)aEvent;
          if (NS_VK_SPACE == keyEvent->keyCode) 
            ToggleCheckState();
        }
        break;
  }

  return nsTitledButtonFrame::HandleEvent(aPresContext, aEvent, aEventStatus);
}

PRIntn nsXULCheckboxFrame::GetDefaultAlignment()
{
  return NS_SIDE_LEFT;
}

