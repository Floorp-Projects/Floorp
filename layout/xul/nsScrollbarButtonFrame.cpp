/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//
// Eric Vaughan
// Netscape Communications
//
// See documentation in associated header file
//

#include "nsScrollbarButtonFrame.h"
#include "nsPresContext.h"
#include "nsIContent.h"
#include "nsCOMPtr.h"
#include "nsNameSpaceManager.h"
#include "nsGkAtoms.h"
#include "nsSliderFrame.h"
#include "nsScrollbarFrame.h"
#include "nsIScrollbarMediator.h"
#include "nsRepeatService.h"
#include "mozilla/LookAndFeel.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/PresShell.h"
#include "mozilla/Telemetry.h"

using namespace mozilla;

//
// NS_NewToolbarFrame
//
// Creates a new Toolbar frame and returns it
//
nsIFrame* NS_NewScrollbarButtonFrame(PresShell* aPresShell,
                                     ComputedStyle* aStyle) {
  return new (aPresShell)
      nsScrollbarButtonFrame(aStyle, aPresShell->GetPresContext());
}

NS_IMPL_FRAMEARENA_HELPERS(nsScrollbarButtonFrame)

nsresult nsScrollbarButtonFrame::HandleEvent(nsPresContext* aPresContext,
                                             WidgetGUIEvent* aEvent,
                                             nsEventStatus* aEventStatus) {
  NS_ENSURE_ARG_POINTER(aEventStatus);

  // If a web page calls event.preventDefault() we still want to
  // scroll when scroll arrow is clicked. See bug 511075.
  if (!mContent->IsInNativeAnonymousSubtree() &&
      nsEventStatus_eConsumeNoDefault == *aEventStatus) {
    return NS_OK;
  }

  switch (aEvent->mMessage) {
    case eMouseDown:
      mCursorOnThis = true;
      // if we didn't handle the press ourselves, pass it on to the superclass
      if (HandleButtonPress(aPresContext, aEvent, aEventStatus)) {
        return NS_OK;
      }
      break;
    case eMouseUp:
      HandleRelease(aPresContext, aEvent, aEventStatus);
      break;
    case eMouseOut:
      mCursorOnThis = false;
      break;
    case eMouseMove: {
      nsPoint cursor =
          nsLayoutUtils::GetEventCoordinatesRelativeTo(aEvent, this);
      nsRect frameRect(nsPoint(0, 0), GetSize());
      mCursorOnThis = frameRect.Contains(cursor);
      break;
    }
    default:
      break;
  }

  return nsButtonBoxFrame::HandleEvent(aPresContext, aEvent, aEventStatus);
}

bool nsScrollbarButtonFrame::HandleButtonPress(nsPresContext* aPresContext,
                                               WidgetGUIEvent* aEvent,
                                               nsEventStatus* aEventStatus) {
  // Get the desired action for the scrollbar button.
  LookAndFeel::IntID tmpAction;
  uint16_t button = aEvent->AsMouseEvent()->mButton;
  if (button == MouseButton::eLeft) {
    tmpAction = LookAndFeel::eIntID_ScrollButtonLeftMouseButtonAction;
  } else if (button == MouseButton::eMiddle) {
    tmpAction = LookAndFeel::eIntID_ScrollButtonMiddleMouseButtonAction;
  } else if (button == MouseButton::eRight) {
    tmpAction = LookAndFeel::eIntID_ScrollButtonRightMouseButtonAction;
  } else {
    return false;
  }

  // Get the button action metric from the pres. shell.
  int32_t pressedButtonAction;
  if (NS_FAILED(LookAndFeel::GetInt(tmpAction, &pressedButtonAction))) {
    return false;
  }

  // get the scrollbar control
  nsIFrame* scrollbar;
  GetParentWithTag(nsGkAtoms::scrollbar, this, scrollbar);

  if (scrollbar == nullptr) return false;

  static Element::AttrValuesArray strings[] = {nsGkAtoms::increment,
                                               nsGkAtoms::decrement, nullptr};
  int32_t index = mContent->AsElement()->FindAttrValueIn(
      kNameSpaceID_None, nsGkAtoms::type, strings, eCaseMatters);
  int32_t direction;
  if (index == 0)
    direction = 1;
  else if (index == 1)
    direction = -1;
  else
    return false;

  bool repeat = pressedButtonAction != 2;
  // set this attribute so we can style it later
  AutoWeakFrame weakFrame(this);
  mContent->AsElement()->SetAttr(kNameSpaceID_None, nsGkAtoms::active,
                                 NS_LITERAL_STRING("true"), true);

  PresShell::SetCapturingContent(mContent, CaptureFlags::IgnoreAllowedState);

  if (!weakFrame.IsAlive()) {
    return false;
  }

  nsScrollbarFrame* sb = do_QueryFrame(scrollbar);
  if (sb) {
    nsIScrollbarMediator* m = sb->GetScrollbarMediator();
    switch (pressedButtonAction) {
      case 0:
        sb->SetIncrementToLine(direction);
        if (m) {
          m->ScrollByLine(sb, direction, nsIScrollbarMediator::ENABLE_SNAP);
        }
        break;
      case 1:
        sb->SetIncrementToPage(direction);
        if (m) {
          m->ScrollByPage(sb, direction, nsIScrollbarMediator::ENABLE_SNAP);
        }
        break;
      case 2:
        sb->SetIncrementToWhole(direction);
        if (m) {
          m->ScrollByWhole(sb, direction, nsIScrollbarMediator::ENABLE_SNAP);
        }
        break;
      case 3:
      default:
        // We were told to ignore this click, or someone assigned a non-standard
        // value to the button's action.
        return false;
    }
    if (!weakFrame.IsAlive()) {
      return false;
    }

    if (!m) {
      sb->MoveToNewPosition();
      if (!weakFrame.IsAlive()) {
        return false;
      }
    }
  }
  if (repeat) {
    StartRepeat();
  }
  return true;
}

NS_IMETHODIMP
nsScrollbarButtonFrame::HandleRelease(nsPresContext* aPresContext,
                                      WidgetGUIEvent* aEvent,
                                      nsEventStatus* aEventStatus) {
  PresShell::ReleaseCapturingContent();
  // we're not active anymore
  mContent->AsElement()->UnsetAttr(kNameSpaceID_None, nsGkAtoms::active, true);
  StopRepeat();
  nsIFrame* scrollbar;
  GetParentWithTag(nsGkAtoms::scrollbar, this, scrollbar);
  nsScrollbarFrame* sb = do_QueryFrame(scrollbar);
  if (sb) {
    nsIScrollbarMediator* m = sb->GetScrollbarMediator();
    if (m) {
      m->ScrollbarReleased(sb);
    }
  }
  return NS_OK;
}

void nsScrollbarButtonFrame::Notify() {
  if (mCursorOnThis ||
      LookAndFeel::GetInt(LookAndFeel::eIntID_ScrollbarButtonAutoRepeatBehavior,
                          0)) {
    // get the scrollbar control
    nsIFrame* scrollbar;
    GetParentWithTag(nsGkAtoms::scrollbar, this, scrollbar);
    nsScrollbarFrame* sb = do_QueryFrame(scrollbar);
    if (sb) {
      nsIScrollbarMediator* m = sb->GetScrollbarMediator();
      if (m) {
        m->RepeatButtonScroll(sb);
      } else {
        sb->MoveToNewPosition();
      }
    }
  }
}

nsresult nsScrollbarButtonFrame::GetChildWithTag(nsAtom* atom, nsIFrame* start,
                                                 nsIFrame*& result) {
  // recursively search our children
  for (nsIFrame* childFrame : start->PrincipalChildList()) {
    // get the content node
    nsIContent* child = childFrame->GetContent();

    if (child) {
      // see if it is the child
      if (child->IsXULElement(atom)) {
        result = childFrame;

        return NS_OK;
      }
    }

    // recursive search the child
    GetChildWithTag(atom, childFrame, result);
    if (result != nullptr) return NS_OK;
  }

  result = nullptr;
  return NS_OK;
}

nsresult nsScrollbarButtonFrame::GetParentWithTag(nsAtom* toFind,
                                                  nsIFrame* start,
                                                  nsIFrame*& result) {
  while (start) {
    start = start->GetParent();

    if (start) {
      // get the content node
      nsIContent* child = start->GetContent();

      if (child && child->IsXULElement(toFind)) {
        result = start;
        return NS_OK;
      }
    }
  }

  result = nullptr;
  return NS_OK;
}

void nsScrollbarButtonFrame::DestroyFrom(nsIFrame* aDestructRoot,
                                         PostDestroyData& aPostDestroyData) {
  // Ensure our repeat service isn't going... it's possible that a scrollbar can
  // disappear out from under you while you're in the process of scrolling.
  StopRepeat();
  nsButtonBoxFrame::DestroyFrom(aDestructRoot, aPostDestroyData);
}
