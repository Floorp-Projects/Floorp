/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Original Author: David W. Hyatt (hyatt@netscape.com)
 *    Blake Ross (blakeross@telocity.com)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
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
#include "nsIEventStateManager.h"
#include "nsXULAtoms.h"
#include "nsIDOMElement.h"

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
    case NS_KEY_DOWN:
      if (NS_KEY_EVENT == aEvent->eventStructType) {
        nsKeyEvent* keyEvent = (nsKeyEvent*)aEvent;
        if (NS_VK_SPACE == keyEvent->keyCode) {
           nsCOMPtr<nsIEventStateManager> esm;
           aPresContext->GetEventStateManager(getter_AddRefs(esm));           
           esm->SetContentState(mContent, NS_EVENT_STATE_HOVER |  
                                          NS_EVENT_STATE_ACTIVE);  // :hover:active state
        }
      }
      break;

    case NS_KEY_PRESS:
      if (NS_KEY_EVENT == aEvent->eventStructType) {
        nsKeyEvent* keyEvent = (nsKeyEvent*)aEvent;
        if (NS_VK_RETURN == keyEvent->charCode) {
           MouseClicked(aPresContext, aEvent);
        }
      }
      break;

    case NS_KEY_UP:
      if (NS_KEY_EVENT == aEvent->eventStructType) {
        nsKeyEvent* keyEvent = (nsKeyEvent*)aEvent;
        if (NS_VK_SPACE == keyEvent->keyCode) {
           nsCOMPtr<nsIEventStateManager> esm;
           aPresContext->GetEventStateManager(getter_AddRefs(esm));
           esm->SetContentState(nsnull, NS_EVENT_STATE_HOVER |
                                        NS_EVENT_STATE_ACTIVE);    // return to normal state
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
  mContent->GetAttr(kNameSpaceID_None, nsHTMLAtoms::disabled, disabled);
  if (disabled.EqualsWithConversion("true"))
    return;

  nsresult rv = NS_OK;

  // Execute the oncommand event handler.
  nsEventStatus status = nsEventStatus_eIgnore;
  nsMouseEvent event;
  event.eventStructType = NS_EVENT;
  event.message = NS_XUL_COMMAND;
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
    // See if we have a command elt.  If so, we execute on the command instead
    // of on our content element.
    nsAutoString command;
    mContent->GetAttr(kNameSpaceID_None, nsXULAtoms::command, command);
    if (!command.IsEmpty()) {
      nsCOMPtr<nsIDocument> doc;
      mContent->GetDocument(*getter_AddRefs(doc));
      nsCOMPtr<nsIDOMDocument> domDoc(do_QueryInterface(doc));
      nsCOMPtr<nsIDOMElement> commandElt;
      domDoc->GetElementById(command, getter_AddRefs(commandElt));
      nsCOMPtr<nsIContent> commandContent(do_QueryInterface(commandElt));
      if (commandContent)
        shell->HandleDOMEventWithTarget(commandContent, &event, &status);
    }
    else
      shell->HandleDOMEventWithTarget(mContent, &event, &status);
  }
}
