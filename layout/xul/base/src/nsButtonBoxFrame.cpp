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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Original Author: David W. Hyatt (hyatt@netscape.com)
 *   Blake Ross (blakeross@telocity.com)
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
#include "nsButtonBoxFrame.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMXULButtonElement.h"
#include "nsGkAtoms.h"
#include "nsINameSpaceManager.h"
#include "nsPresContext.h"
#include "nsIPresShell.h"
#include "nsGUIEvent.h"
#include "nsIEventStateManager.h"
#include "nsIDOMElement.h"
#include "nsDisplayList.h"

//
// NS_NewXULButtonFrame
//
// Creates a new Button frame and returns it
//
nsIFrame*
NS_NewButtonBoxFrame (nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsButtonBoxFrame(aPresShell, aContext);
} // NS_NewXULButtonFrame

NS_IMETHODIMP
nsButtonBoxFrame::BuildDisplayListForChildren(nsDisplayListBuilder*   aBuilder,
                                              const nsRect&           aDirtyRect,
                                              const nsDisplayListSet& aLists)
{
  // override, since we don't want children to get events
  if (aBuilder->IsForEventDelivery())
    return NS_OK;
  return nsBoxFrame::BuildDisplayListForChildren(aBuilder, aDirtyRect, aLists);
}

NS_IMETHODIMP
nsButtonBoxFrame::HandleEvent(nsPresContext* aPresContext, 
                              nsGUIEvent* aEvent,
                              nsEventStatus* aEventStatus)
{
  switch (aEvent->message) {
    case NS_KEY_DOWN:
      if (NS_KEY_EVENT == aEvent->eventStructType) {
        nsKeyEvent* keyEvent = (nsKeyEvent*)aEvent;
        if (NS_VK_SPACE == keyEvent->keyCode) {
          nsIEventStateManager *esm = aPresContext->EventStateManager();
          // :hover:active state
          esm->SetContentState(mContent,
                               NS_EVENT_STATE_HOVER |  NS_EVENT_STATE_ACTIVE);
        }
      }
      break;

// On mac, Return fires the defualt button, not the focused one.
#ifndef XP_MACOSX
    case NS_KEY_PRESS:
      if (NS_KEY_EVENT == aEvent->eventStructType) {
        nsKeyEvent* keyEvent = (nsKeyEvent*)aEvent;
        if (NS_VK_RETURN == keyEvent->keyCode) {
          nsCOMPtr<nsIDOMXULButtonElement> buttonEl(do_QueryInterface(mContent));
          if (buttonEl) {
            MouseClicked(aPresContext, aEvent);
            *aEventStatus = nsEventStatus_eConsumeNoDefault;
          }
        }
      }
      break;
#endif

    case NS_KEY_UP:
      if (NS_KEY_EVENT == aEvent->eventStructType) {
        nsKeyEvent* keyEvent = (nsKeyEvent*)aEvent;
        if (NS_VK_SPACE == keyEvent->keyCode) {
          // only activate on keyup if we're already in the :hover:active state
          PRInt32 buttonState;
          const PRInt32 activeHover = NS_EVENT_STATE_ACTIVE | NS_EVENT_STATE_HOVER;
          nsIEventStateManager *esm = aPresContext->EventStateManager();
          esm->GetContentState(mContent, buttonState);
          if ((buttonState & activeHover) == activeHover) {
            esm->SetContentState(nsnull, activeHover);    // return to normal state
            MouseClicked(aPresContext, aEvent);
          }
        }
      }
      break;

    case NS_MOUSE_CLICK:
      if (NS_IS_MOUSE_LEFT_CLICK(aEvent)) {
        MouseClicked(aPresContext, aEvent);
      }
      break;
  }

  return nsBoxFrame::HandleEvent(aPresContext, aEvent, aEventStatus);
}

void 
nsButtonBoxFrame::DoMouseClick(nsGUIEvent* aEvent, PRBool aTrustEvent) 
{
  // Don't execute if we're disabled.
  if (mContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::disabled,
                            nsGkAtoms::_true, eCaseMatters))
    return;

  // Execute the oncommand event handler.
  nsEventStatus status = nsEventStatus_eIgnore;
  nsXULCommandEvent event(aEvent ? NS_IS_TRUSTED_EVENT(aEvent) : aTrustEvent,
                          NS_XUL_COMMAND, nsnull);
  if(aEvent) {
    event.isShift = ((nsInputEvent*)(aEvent))->isShift;
    event.isControl = ((nsInputEvent*)(aEvent))->isControl;
    event.isAlt = ((nsInputEvent*)(aEvent))->isAlt;
    event.isMeta = ((nsInputEvent*)(aEvent))->isMeta;
  }

  // Have the content handle the event, propagating it according to normal DOM rules.
  nsIPresShell *shell = GetPresContext()->GetPresShell();
  if (shell) {
    shell->HandleDOMEventWithTarget(mContent, &event, &status);
    // shell may no longer be alive, don't use it here unless you keep a ref
  }
}
