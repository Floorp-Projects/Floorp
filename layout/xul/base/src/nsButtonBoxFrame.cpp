/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "nsCOMPtr.h"
#include "nsButtonBoxFrame.h"
#include "nsIContent.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMXULButtonElement.h"
#include "nsGkAtoms.h"
#include "nsINameSpaceManager.h"
#include "nsPresContext.h"
#include "nsIPresShell.h"
#include "nsEventStateManager.h"
#include "nsIDOMElement.h"
#include "nsDisplayList.h"
#include "nsContentUtils.h"
#include "mozilla/dom/Element.h"
#include "mozilla/TextEvents.h"

using namespace mozilla;

//
// NS_NewXULButtonFrame
//
// Creates a new Button frame and returns it
//
nsIFrame*
NS_NewButtonBoxFrame (nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsButtonBoxFrame(aPresShell, aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsButtonBoxFrame)

void
nsButtonBoxFrame::BuildDisplayListForChildren(nsDisplayListBuilder*   aBuilder,
                                              const nsRect&           aDirtyRect,
                                              const nsDisplayListSet& aLists)
{
  // override, since we don't want children to get events
  if (aBuilder->IsForEventDelivery())
    return;
  nsBoxFrame::BuildDisplayListForChildren(aBuilder, aDirtyRect, aLists);
}

NS_IMETHODIMP
nsButtonBoxFrame::HandleEvent(nsPresContext* aPresContext, 
                              WidgetGUIEvent* aEvent,
                              nsEventStatus* aEventStatus)
{
  NS_ENSURE_ARG_POINTER(aEventStatus);
  if (nsEventStatus_eConsumeNoDefault == *aEventStatus) {
    return NS_OK;
  }

  switch (aEvent->message) {
    case NS_KEY_DOWN: {
      WidgetKeyboardEvent* keyEvent = aEvent->AsKeyboardEvent();
      if (!keyEvent) {
        break;
      }
      if (NS_VK_SPACE == keyEvent->keyCode) {
        nsEventStateManager* esm = aPresContext->EventStateManager();
        // :hover:active state
        esm->SetContentState(mContent, NS_EVENT_STATE_HOVER);
        esm->SetContentState(mContent, NS_EVENT_STATE_ACTIVE);
      }
      break;
    }

// On mac, Return fires the default button, not the focused one.
#ifndef XP_MACOSX
    case NS_KEY_PRESS: {
      WidgetKeyboardEvent* keyEvent = aEvent->AsKeyboardEvent();
      if (!keyEvent) {
        break;
      }
      if (NS_VK_RETURN == keyEvent->keyCode) {
        nsCOMPtr<nsIDOMXULButtonElement> buttonEl(do_QueryInterface(mContent));
        if (buttonEl) {
          MouseClicked(aPresContext, aEvent);
          *aEventStatus = nsEventStatus_eConsumeNoDefault;
        }
      }
      break;
    }
#endif

    case NS_KEY_UP: {
      WidgetKeyboardEvent* keyEvent = aEvent->AsKeyboardEvent();
      if (!keyEvent) {
        break;
      }
      if (NS_VK_SPACE == keyEvent->keyCode) {
        // only activate on keyup if we're already in the :hover:active state
        NS_ASSERTION(mContent->IsElement(), "How do we have a non-element?");
        nsEventStates buttonState = mContent->AsElement()->State();
        if (buttonState.HasAllStates(NS_EVENT_STATE_ACTIVE |
                                     NS_EVENT_STATE_HOVER)) {
          // return to normal state
          nsEventStateManager *esm = aPresContext->EventStateManager();
          esm->SetContentState(nullptr, NS_EVENT_STATE_ACTIVE);
          esm->SetContentState(nullptr, NS_EVENT_STATE_HOVER);
          MouseClicked(aPresContext, aEvent);
        }
      }
      break;
    }

    case NS_MOUSE_CLICK:
      if (aEvent->IsLeftClickEvent()) {
        MouseClicked(aPresContext, aEvent);
      }
      break;
  }

  return nsBoxFrame::HandleEvent(aPresContext, aEvent, aEventStatus);
}

void 
nsButtonBoxFrame::DoMouseClick(WidgetGUIEvent* aEvent, bool aTrustEvent)
{
  // Don't execute if we're disabled.
  if (mContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::disabled,
                            nsGkAtoms::_true, eCaseMatters))
    return;

  // Execute the oncommand event handler.
  bool isShift = false;
  bool isControl = false;
  bool isAlt = false;
  bool isMeta = false;
  if(aEvent) {
    WidgetInputEvent* inputEvent = aEvent->AsInputEvent();
    isShift = inputEvent->IsShift();
    isControl = inputEvent->IsControl();
    isAlt = inputEvent->IsAlt();
    isMeta = inputEvent->IsMeta();
  }

  // Have the content handle the event, propagating it according to normal DOM rules.
  nsCOMPtr<nsIPresShell> shell = PresContext()->GetPresShell();
  if (shell) {
    nsContentUtils::DispatchXULCommand(mContent,
                                       aEvent ?
                                         aEvent->mFlags.mIsTrusted : aTrustEvent,
                                       nullptr, shell,
                                       isControl, isAlt, isShift, isMeta);
  }
}
