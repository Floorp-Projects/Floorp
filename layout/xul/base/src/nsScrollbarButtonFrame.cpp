/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
#include "nsINameSpaceManager.h"
#include "nsGkAtoms.h"
#include "nsSliderFrame.h"
#include "nsScrollbarFrame.h"
#include "nsIScrollbarMediator.h"
#include "nsRepeatService.h"
#include "nsGUIEvent.h"
#include "mozilla/LookAndFeel.h"

using namespace mozilla;

//
// NS_NewToolbarFrame
//
// Creates a new Toolbar frame and returns it
//
nsIFrame*
NS_NewScrollbarButtonFrame (nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsScrollbarButtonFrame(aPresShell, aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsScrollbarButtonFrame)

NS_IMETHODIMP
nsScrollbarButtonFrame::HandleEvent(nsPresContext* aPresContext, 
                                    nsGUIEvent* aEvent,
                                    nsEventStatus* aEventStatus)
{  
  NS_ENSURE_ARG_POINTER(aEventStatus);

  // If a web page calls event.preventDefault() we still want to
  // scroll when scroll arrow is clicked. See bug 511075.
  if (!mContent->IsInNativeAnonymousSubtree() &&
      nsEventStatus_eConsumeNoDefault == *aEventStatus) {
    return NS_OK;
  }

  switch (aEvent->message) {
    case NS_MOUSE_BUTTON_DOWN:
      mCursorOnThis = true;
      // if we didn't handle the press ourselves, pass it on to the superclass
      if (HandleButtonPress(aPresContext, aEvent, aEventStatus)) {
        return NS_OK;
      }
      break;
    case NS_MOUSE_BUTTON_UP:
      HandleRelease(aPresContext, aEvent, aEventStatus);
      break;
    case NS_MOUSE_EXIT_SYNTH:
      mCursorOnThis = false;
      break;
    case NS_MOUSE_MOVE: {
      nsPoint cursor =
        nsLayoutUtils::GetEventCoordinatesRelativeTo(aEvent, this);
      nsRect frameRect(nsPoint(0, 0), GetSize());
      mCursorOnThis = frameRect.Contains(cursor);
      break;
    }
  }

  return nsButtonBoxFrame::HandleEvent(aPresContext, aEvent, aEventStatus);
}


bool
nsScrollbarButtonFrame::HandleButtonPress(nsPresContext* aPresContext, 
                                          nsGUIEvent*     aEvent,
                                          nsEventStatus*  aEventStatus)
{
  // Get the desired action for the scrollbar button.
  LookAndFeel::IntID tmpAction;
  uint16_t button = static_cast<nsMouseEvent*>(aEvent)->button;
  if (button == nsMouseEvent::eLeftButton) {
    tmpAction = LookAndFeel::eIntID_ScrollButtonLeftMouseButtonAction;
  } else if (button == nsMouseEvent::eMiddleButton) {
    tmpAction = LookAndFeel::eIntID_ScrollButtonMiddleMouseButtonAction;
  } else if (button == nsMouseEvent::eRightButton) {
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

  if (scrollbar == nullptr)
    return false;

  // get the scrollbars content node
  nsIContent* content = scrollbar->GetContent();

  static nsIContent::AttrValuesArray strings[] = { &nsGkAtoms::increment,
                                                   &nsGkAtoms::decrement,
                                                   nullptr };
  int32_t index = mContent->FindAttrValueIn(kNameSpaceID_None,
                                            nsGkAtoms::type,
                                            strings, eCaseMatters);
  int32_t direction;
  if (index == 0) 
    direction = 1;
  else if (index == 1)
    direction = -1;
  else
    return false;

  // Whether or not to repeat the click action.
  bool repeat = true;
  // Use smooth scrolling by default.
  bool smoothScroll = true;
  switch (pressedButtonAction) {
    case 0:
      mIncrement = direction * nsSliderFrame::GetIncrement(content);
      break;
    case 1:
      mIncrement = direction * nsSliderFrame::GetPageIncrement(content);
      break;
    case 2:
      if (direction == -1)
        mIncrement = -nsSliderFrame::GetCurrentPosition(content);
      else
        mIncrement = nsSliderFrame::GetMaxPosition(content) - 
                     nsSliderFrame::GetCurrentPosition(content);
      // Don't repeat or use smooth scrolling if scrolling to beginning or end
      // of a page.
      repeat = smoothScroll = false;
      break;
    case 3:
    default:
      // We were told to ignore this click, or someone assigned a non-standard
      // value to the button's action.
      return false;
  }
  // set this attribute so we can style it later
  nsWeakFrame weakFrame(this);
  mContent->SetAttr(kNameSpaceID_None, nsGkAtoms::active, NS_LITERAL_STRING("true"), true);

  nsIPresShell::SetCapturingContent(mContent, CAPTURE_IGNOREALLOWED);

  if (weakFrame.IsAlive()) {
    DoButtonAction(smoothScroll);
  }
  if (repeat)
    StartRepeat();
  return true;
}

NS_IMETHODIMP 
nsScrollbarButtonFrame::HandleRelease(nsPresContext* aPresContext, 
                                      nsGUIEvent*     aEvent,
                                      nsEventStatus*  aEventStatus)
{
  nsIPresShell::SetCapturingContent(nullptr, 0);
  // we're not active anymore
  mContent->UnsetAttr(kNameSpaceID_None, nsGkAtoms::active, true);
  StopRepeat();
  return NS_OK;
}

void nsScrollbarButtonFrame::Notify()
{
  // Since this is only going to get called if we're scrolling a page length
  // or a line increment, we will always use smooth scrolling.
  if (mCursorOnThis ||
      LookAndFeel::GetInt(
        LookAndFeel::eIntID_ScrollbarButtonAutoRepeatBehavior, 0)) {
    DoButtonAction(true);
  }
}

void
nsScrollbarButtonFrame::MouseClicked(nsPresContext* aPresContext, nsGUIEvent* aEvent) 
{
  nsButtonBoxFrame::MouseClicked(aPresContext, aEvent);
  //MouseClicked();
}

void
nsScrollbarButtonFrame::DoButtonAction(bool aSmoothScroll) 
{
  // get the scrollbar control
  nsIFrame* scrollbar;
  GetParentWithTag(nsGkAtoms::scrollbar, this, scrollbar);

  if (scrollbar == nullptr)
    return;

  // get the scrollbars content node
  nsCOMPtr<nsIContent> content = scrollbar->GetContent();

  // get the current pos
  int32_t curpos = nsSliderFrame::GetCurrentPosition(content);
  int32_t oldpos = curpos;

  // get the max pos
  int32_t maxpos = nsSliderFrame::GetMaxPosition(content);

  // increment the given amount
  if (mIncrement)
    curpos += mIncrement;

  // make sure the current position is between the current and max positions
  if (curpos < 0)
    curpos = 0;
  else if (curpos > maxpos)
    curpos = maxpos;

  nsScrollbarFrame* sb = do_QueryFrame(scrollbar);
  if (sb) {
    nsIScrollbarMediator* m = sb->GetScrollbarMediator();
    if (m) {
      m->ScrollbarButtonPressed(sb, oldpos, curpos);
      return;
    }
  }

  // set the current position of the slider.
  nsAutoString curposStr;
  curposStr.AppendInt(curpos);

  if (aSmoothScroll)
    content->SetAttr(kNameSpaceID_None, nsGkAtoms::smooth, NS_LITERAL_STRING("true"), false);
  content->SetAttr(kNameSpaceID_None, nsGkAtoms::curpos, curposStr, true);
  if (aSmoothScroll)
    content->UnsetAttr(kNameSpaceID_None, nsGkAtoms::smooth, false);
}

nsresult
nsScrollbarButtonFrame::GetChildWithTag(nsPresContext* aPresContext,
                                        nsIAtom* atom, nsIFrame* start,
                                        nsIFrame*& result)
{
  // recursively search our children
  nsIFrame* childFrame = start->GetFirstPrincipalChild();
  while (nullptr != childFrame) 
  {    
    // get the content node
    nsIContent* child = childFrame->GetContent();

    if (child) {
      // see if it is the child
       if (child->Tag() == atom)
       {
         result = childFrame;

         return NS_OK;
       }
    }

     // recursive search the child
     GetChildWithTag(aPresContext, atom, childFrame, result);
     if (result != nullptr) 
       return NS_OK;

    childFrame = childFrame->GetNextSibling();
  }

  result = nullptr;
  return NS_OK;
}

nsresult
nsScrollbarButtonFrame::GetParentWithTag(nsIAtom* toFind, nsIFrame* start,
                                         nsIFrame*& result)
{
   while (start)
   {
      start = start->GetParent();

      if (start) {
        // get the content node
        nsIContent* child = start->GetContent();

        if (child && child->Tag() == toFind) {
          result = start;
          return NS_OK;
        }
      }
   }

   result = nullptr;
   return NS_OK;
}

void
nsScrollbarButtonFrame::DestroyFrom(nsIFrame* aDestructRoot)
{
  // Ensure our repeat service isn't going... it's possible that a scrollbar can disappear out
  // from under you while you're in the process of scrolling.
  StopRepeat();
  nsButtonBoxFrame::DestroyFrom(aDestructRoot);
}
