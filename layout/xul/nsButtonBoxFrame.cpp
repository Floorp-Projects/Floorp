/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "nsCOMPtr.h"
#include "nsButtonBoxFrame.h"
#include "nsIContent.h"
#include "nsIDOMXULButtonElement.h"
#include "nsGkAtoms.h"
#include "nsNameSpaceManager.h"
#include "nsPresContext.h"
#include "nsDisplayList.h"
#include "nsContentUtils.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/MouseEventBinding.h"
#include "mozilla/EventStateManager.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/PresShell.h"
#include "mozilla/TextEvents.h"

using namespace mozilla;
using namespace mozilla::dom;

NS_IMPL_ISUPPORTS(nsButtonBoxFrame::nsButtonBoxListener, nsIDOMEventListener)

nsresult nsButtonBoxFrame::nsButtonBoxListener::HandleEvent(
    dom::Event* aEvent) {
  if (!mButtonBoxFrame) {
    return NS_OK;
  }

  nsAutoString eventType;
  aEvent->GetType(eventType);

  if (eventType.EqualsLiteral("blur")) {
    mButtonBoxFrame->Blurred();
    return NS_OK;
  }

  MOZ_ASSERT_UNREACHABLE("Unexpected eventType");
  return NS_OK;
}

//
// NS_NewXULButtonFrame
//
// Creates a new Button frame and returns it
//
nsIFrame* NS_NewButtonBoxFrame(PresShell* aPresShell, ComputedStyle* aStyle) {
  return new (aPresShell)
      nsButtonBoxFrame(aStyle, aPresShell->GetPresContext());
}

NS_IMPL_FRAMEARENA_HELPERS(nsButtonBoxFrame)

nsButtonBoxFrame::nsButtonBoxFrame(ComputedStyle* aStyle,
                                   nsPresContext* aPresContext, ClassID aID)
    : nsBoxFrame(aStyle, aPresContext, aID, false),
      mButtonBoxListener(nullptr),
      mIsHandlingKeyEvent(false) {}

void nsButtonBoxFrame::Init(nsIContent* aContent, nsContainerFrame* aParent,
                            nsIFrame* aPrevInFlow) {
  nsBoxFrame::Init(aContent, aParent, aPrevInFlow);

  mButtonBoxListener = new nsButtonBoxListener(this);

  mContent->AddSystemEventListener(u"blur"_ns, mButtonBoxListener, false);
}

void nsButtonBoxFrame::DestroyFrom(nsIFrame* aDestructRoot,
                                   PostDestroyData& aPostDestroyData) {
  mContent->RemoveSystemEventListener(u"blur"_ns, mButtonBoxListener, false);

  mButtonBoxListener->mButtonBoxFrame = nullptr;
  mButtonBoxListener = nullptr;

  nsBoxFrame::DestroyFrom(aDestructRoot, aPostDestroyData);
}

void nsButtonBoxFrame::BuildDisplayListForChildren(
    nsDisplayListBuilder* aBuilder, const nsDisplayListSet& aLists) {
  // override, since we don't want children to get events
  if (aBuilder->IsForEventDelivery()) return;
  nsBoxFrame::BuildDisplayListForChildren(aBuilder, aLists);
}

nsresult nsButtonBoxFrame::HandleEvent(nsPresContext* aPresContext,
                                       WidgetGUIEvent* aEvent,
                                       nsEventStatus* aEventStatus) {
  NS_ENSURE_ARG_POINTER(aEventStatus);
  if (nsEventStatus_eConsumeNoDefault == *aEventStatus) {
    return NS_OK;
  }

  switch (aEvent->mMessage) {
    case eKeyDown: {
      WidgetKeyboardEvent* keyEvent = aEvent->AsKeyboardEvent();
      if (!keyEvent) {
        break;
      }
      if (NS_VK_SPACE == keyEvent->mKeyCode) {
        EventStateManager* esm = aPresContext->EventStateManager();
        // :hover:active state
        esm->SetContentState(mContent, ElementState::HOVER);
        esm->SetContentState(mContent, ElementState::ACTIVE);
        mIsHandlingKeyEvent = true;
      }
      break;
    }

// On mac, Return fires the default button, not the focused one.
#ifndef XP_MACOSX
    case eKeyPress: {
      WidgetKeyboardEvent* keyEvent = aEvent->AsKeyboardEvent();
      if (!keyEvent) {
        break;
      }
      if (NS_VK_RETURN == keyEvent->mKeyCode) {
        RefPtr<nsIDOMXULButtonElement> button =
            mContent->AsElement()->AsXULButton();
        if (button) {
          MouseClicked(aEvent);
          *aEventStatus = nsEventStatus_eConsumeNoDefault;
        }
      }
      break;
    }
#endif

    case eKeyUp: {
      WidgetKeyboardEvent* keyEvent = aEvent->AsKeyboardEvent();
      if (!keyEvent) {
        break;
      }
      if (NS_VK_SPACE == keyEvent->mKeyCode) {
        mIsHandlingKeyEvent = false;
        // only activate on keyup if we're already in the :hover:active state
        NS_ASSERTION(mContent->IsElement(), "How do we have a non-element?");
        ElementState buttonState = mContent->AsElement()->State();
        if (buttonState.HasAllStates(ElementState::ACTIVE |
                                     ElementState::HOVER)) {
          // return to normal state
          EventStateManager* esm = aPresContext->EventStateManager();
          esm->SetContentState(nullptr, ElementState::ACTIVE);
          esm->SetContentState(nullptr, ElementState::HOVER);
          MouseClicked(aEvent);
        }
      }
      break;
    }

    case eMouseClick: {
      WidgetMouseEvent* mouseEvent = aEvent->AsMouseEvent();
      if (mouseEvent->IsLeftClickEvent()) {
        MouseClicked(mouseEvent);
      }
      break;
    }

    default:
      break;
  }

  return nsBoxFrame::HandleEvent(aPresContext, aEvent, aEventStatus);
}

void nsButtonBoxFrame::Blurred() {
  NS_ASSERTION(mContent->IsElement(), "How do we have a non-element?");
  ElementState buttonState = mContent->AsElement()->State();
  if (mIsHandlingKeyEvent &&
      buttonState.HasAllStates(ElementState::ACTIVE | ElementState::HOVER)) {
    // return to normal state
    EventStateManager* esm = PresContext()->EventStateManager();
    esm->SetContentState(nullptr, ElementState::ACTIVE);
    esm->SetContentState(nullptr, ElementState::HOVER);
  }
  mIsHandlingKeyEvent = false;
}

void nsButtonBoxFrame::MouseClicked(WidgetGUIEvent* aEvent) {
  // Don't execute if we're disabled.
  if (mContent->AsElement()->AttrValueIs(kNameSpaceID_None, nsGkAtoms::disabled,
                                         nsGkAtoms::_true, eCaseMatters))
    return;

  // Have the content handle the event, propagating it according to normal DOM
  // rules.
  RefPtr<mozilla::PresShell> presShell = PresContext()->GetPresShell();
  if (!presShell) {
    return;
  }

  // Execute the oncommand event handler.
  nsCOMPtr<nsIContent> content = mContent;
  WidgetInputEvent* inputEvent = aEvent->AsInputEvent();
  WidgetMouseEventBase* mouseEvent = aEvent->AsMouseEventBase();
  nsContentUtils::DispatchXULCommand(
      content, aEvent->IsTrusted(), nullptr, presShell, inputEvent->IsControl(),
      inputEvent->IsAlt(), inputEvent->IsShift(), inputEvent->IsMeta(),
      mouseEvent ? mouseEvent->mInputSource
                 : MouseEvent_Binding::MOZ_SOURCE_UNKNOWN,
      mouseEvent ? mouseEvent->mButton : 0);
}
