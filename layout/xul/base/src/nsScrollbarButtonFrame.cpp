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
  PRUint16 button = static_cast<nsMouseEvent*>(aEvent)->button;
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
  PRInt32 pressedButtonAction;
  if (NS_FAILED(LookAndFeel::GetInt(tmpAction, &pressedButtonAction))) {
    return false;
  }

  // get the scrollbar control
  nsIFrame* scrollbar;
  GetParentWithTag(nsGkAtoms::scrollbar, this, scrollbar);

  if (scrollbar == nsnull)
    return false;

  // get the scrollbars content node
  nsIContent* content = scrollbar->GetContent();

  static nsIContent::AttrValuesArray strings[] = { &nsGkAtoms::increment,
                                                   &nsGkAtoms::decrement,
                                                   nsnull };
  PRInt32 index = mContent->FindAttrValueIn(kNameSpaceID_None,
                                            nsGkAtoms::type,
                                            strings, eCaseMatters);
  PRInt32 direction;
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
  nsIPresShell::SetCapturingContent(nsnull, 0);
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

  if (scrollbar == nsnull)
    return;

  // get the scrollbars content node
  nsCOMPtr<nsIContent> content = scrollbar->GetContent();

  // get the current pos
  PRInt32 curpos = nsSliderFrame::GetCurrentPosition(content);
  PRInt32 oldpos = curpos;

  // get the max pos
  PRInt32 maxpos = nsSliderFrame::GetMaxPosition(content);

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
  while (nsnull != childFrame) 
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
     if (result != nsnull) 
       return NS_OK;

    childFrame = childFrame->GetNextSibling();
  }

  result = nsnull;
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

   result = nsnull;
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
