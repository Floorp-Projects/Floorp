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
 * Original Author: David W. Hyatt (hyatt@netscape.com)
 *
 * Contributor(s): 
 */
#include "nsCOMPtr.h"
#include "nsButtonBoxFrame.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIDOMXULDocument.h"
#include "nsIDOMNodeList.h"
#include "nsHTMLAtoms.h"
#include "nsINameSpaceManager.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsGUIEvent.h"

//
// NS_NewXULButtonFrame
//
// Creates a new Button frame and returns it in |aNewFrame|
//
nsresult
NS_NewButtonBoxFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame )
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsButtonBoxFrame* it = new (aPresShell) nsButtonBoxFrame(aPresShell);
  if (nsnull == it)
    return NS_ERROR_OUT_OF_MEMORY;

  // it->SetFlags(aFlags);
  *aNewFrame = it;
  return NS_OK;
  
} // NS_NewXULButtonFrame

nsButtonBoxFrame::nsButtonBoxFrame(nsIPresShell* aPresShell)
:nsBoxFrame(aPresShell, PR_FALSE)
{
}

NS_IMETHODIMP
nsButtonBoxFrame::GetMouseThrough(PRBool& aMouseThrough)
{
  aMouseThrough = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP nsButtonBoxFrame::GetFrameForPoint(nsIPresContext* aPresContext,
                                    const nsPoint& aPoint, 
                                    nsFramePaintLayer aWhichLayer,
                                    nsIFrame**     aFrame)
{
  // override, since we don't want children to get events
  return nsFrame::GetFrameForPoint(aPresContext, aPoint, aWhichLayer, aFrame);
}

NS_IMETHODIMP
nsButtonBoxFrame::HandleEvent(nsIPresContext* aPresContext, 
                                      nsGUIEvent* aEvent,
                                      nsEventStatus* aEventStatus)
{
  switch (aEvent->message) {
    case NS_KEY_PRESS:
      if (NS_KEY_EVENT == aEvent->eventStructType) {
        nsKeyEvent* keyEvent = (nsKeyEvent*)aEvent;
        if (NS_VK_SPACE == keyEvent->keyCode || NS_VK_RETURN == keyEvent->keyCode) {
          MouseClicked(aPresContext, aEvent);
        }
      }
      break;

    case NS_MOUSE_LEFT_CLICK:
      MouseClicked(aPresContext, aEvent);
      break;
  }

  return nsBoxFrame::HandleEvent(aPresContext, aEvent, aEventStatus);
}

void 
nsButtonBoxFrame::MouseClicked (nsIPresContext* aPresContext, nsGUIEvent* aEvent) 
{
  // Don't execute if we're disabled.
  nsAutoString disabled;
  mContent->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::disabled, disabled);
  if (disabled.EqualsWithConversion("true"))
    return;

  nsresult rv = NS_OK;

  // Execute the oncommand event handler.
  nsEventStatus status = nsEventStatus_eIgnore;
  nsMouseEvent event;
  event.eventStructType = NS_EVENT;
  event.message = NS_MENU_ACTION;
  if(aEvent) {
    event.isShift = ((nsInputEvent*)(aEvent))->isShift;
    event.isControl = ((nsInputEvent*)(aEvent))->isControl;
    event.isAlt = ((nsInputEvent*)(aEvent))->isAlt;
    event.isMeta = ((nsInputEvent*)(aEvent))->isMeta;
  } else {
    event.isShift = PR_FALSE;
    event.isControl = PR_FALSE;
    event.isAlt = PR_FALSE;
    event.isMeta = PR_FALSE;
  }
  event.clickCount = 0;
  event.widget = nsnull;

  // Have the content handle the event, propagating it according to normal DOM rules.
  nsCOMPtr<nsIPresShell> shell;
  rv = aPresContext->GetShell(getter_AddRefs(shell));
  if (NS_SUCCEEDED(rv) && shell) {
    shell->HandleDOMEventWithTarget(mContent, &event, &status); 
  }
}
