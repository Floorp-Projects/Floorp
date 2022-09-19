/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "XULButtonElement.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/EventStateManager.h"
#include "mozilla/TextEvents.h"
#include "mozilla/dom/DocumentInlines.h"
#include "mozilla/dom/MouseEventBinding.h"
#include "nsPresContext.h"
#include "nsIDOMXULButtonElement.h"

namespace mozilla::dom {

nsresult XULButtonElement::PostHandleEvent(EventChainPostVisitor& aVisitor) {
  if (aVisitor.mEventStatus == nsEventStatus_eConsumeNoDefault) {
    return nsXULElement::PostHandleEvent(aVisitor);
  }

  // Menu buttons have their own event handling, don't mess with that.
  if (GetPrimaryFrame() && GetPrimaryFrame()->IsMenuFrame()) {
    return nsXULElement::PostHandleEvent(aVisitor);
  }

  auto* event = aVisitor.mEvent;
  switch (event->mMessage) {
    case eBlur: {
      Blurred();
      break;
    }
    case eKeyDown: {
      WidgetKeyboardEvent* keyEvent = event->AsKeyboardEvent();
      if (!keyEvent) {
        break;
      }
      if (NS_VK_SPACE == keyEvent->mKeyCode && aVisitor.mPresContext) {
        EventStateManager* esm = aVisitor.mPresContext->EventStateManager();
        // :hover:active state
        esm->SetContentState(this, ElementState::HOVER);
        esm->SetContentState(this, ElementState::ACTIVE);
        mIsHandlingKeyEvent = true;
      }
      break;
    }

// On mac, Return fires the default button, not the focused one.
#ifndef XP_MACOSX
    case eKeyPress: {
      WidgetKeyboardEvent* keyEvent = event->AsKeyboardEvent();
      if (!keyEvent) {
        break;
      }
      if (NS_VK_RETURN == keyEvent->mKeyCode) {
        if (RefPtr<nsIDOMXULButtonElement> button = AsXULButton()) {
          if (MouseClicked(*keyEvent)) {
            aVisitor.mEventStatus = nsEventStatus_eConsumeNoDefault;
          }
        }
      }
      break;
    }
#endif

    case eKeyUp: {
      WidgetKeyboardEvent* keyEvent = event->AsKeyboardEvent();
      if (!keyEvent) {
        break;
      }
      if (NS_VK_SPACE == keyEvent->mKeyCode) {
        mIsHandlingKeyEvent = false;
        ElementState buttonState = State();
        if (buttonState.HasAllStates(ElementState::ACTIVE |
                                     ElementState::HOVER) &&
            aVisitor.mPresContext) {
          // return to normal state
          EventStateManager* esm = aVisitor.mPresContext->EventStateManager();
          esm->SetContentState(nullptr, ElementState::ACTIVE);
          esm->SetContentState(nullptr, ElementState::HOVER);
          if (MouseClicked(*keyEvent)) {
            aVisitor.mEventStatus = nsEventStatus_eConsumeNoDefault;
          }
        }
      }
      break;
    }

    case eMouseClick: {
      WidgetMouseEvent* mouseEvent = event->AsMouseEvent();
      if (mouseEvent->IsLeftClickEvent()) {
        if (MouseClicked(*mouseEvent)) {
          aVisitor.mEventStatus = nsEventStatus_eConsumeNoDefault;
        }
      }
      break;
    }

    default:
      break;
  }

  return nsXULElement::PostHandleEvent(aVisitor);
}

void XULButtonElement::Blurred() {
  ElementState buttonState = State();
  if (mIsHandlingKeyEvent &&
      buttonState.HasAllStates(ElementState::ACTIVE | ElementState::HOVER)) {
    // Return to normal state
    if (nsPresContext* pc = OwnerDoc()->GetPresContext()) {
      EventStateManager* esm = pc->EventStateManager();
      esm->SetContentState(nullptr, ElementState::ACTIVE);
      esm->SetContentState(nullptr, ElementState::HOVER);
    }
  }
  mIsHandlingKeyEvent = false;
}

bool XULButtonElement::MouseClicked(WidgetGUIEvent& aEvent) {
  // Don't execute if we're disabled.
  if (GetXULBoolAttr(nsGkAtoms::disabled) || !IsInComposedDoc()) {
    return false;
  }

  // Have the content handle the event, propagating it according to normal DOM
  // rules.
  RefPtr<mozilla::PresShell> presShell = OwnerDoc()->GetPresShell();
  if (!presShell) {
    return false;
  }

  // Execute the oncommand event handler.
  WidgetInputEvent* inputEvent = aEvent.AsInputEvent();
  WidgetMouseEventBase* mouseEvent = aEvent.AsMouseEventBase();
  WidgetKeyboardEvent* keyEvent = aEvent.AsKeyboardEvent();
  // TODO: Set aSourceEvent?
  nsContentUtils::DispatchXULCommand(
      this, aEvent.IsTrusted(), /* aSourceEvent = */ nullptr, presShell,
      inputEvent->IsControl(), inputEvent->IsAlt(), inputEvent->IsShift(),
      inputEvent->IsMeta(),
      mouseEvent ? mouseEvent->mInputSource
                 : (keyEvent ? MouseEvent_Binding::MOZ_SOURCE_KEYBOARD
                             : MouseEvent_Binding::MOZ_SOURCE_UNKNOWN),
      mouseEvent ? mouseEvent->mButton : 0);
  return true;
}

}  // namespace mozilla::dom
