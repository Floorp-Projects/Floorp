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
#include "nsIPresContext.h"
#include "nsIContent.h"
#include "nsCOMPtr.h"
#include "nsUnitConversion.h"
#include "nsINameSpaceManager.h"
#include "nsHTMLAtoms.h"
#include "nsXULAtoms.h"
#include "nsSliderFrame.h"
#include "nsIScrollbarFrame.h"
#include "nsIScrollbarMediator.h"
#include "nsRepeatService.h"
#include "nsGUIEvent.h"

//
// NS_NewToolbarFrame
//
// Creates a new Toolbar frame and returns it in |aNewFrame|
//
nsresult
NS_NewScrollbarButtonFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame )
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsScrollbarButtonFrame* it = new (aPresShell) nsScrollbarButtonFrame (aPresShell);
  if (nsnull == it)
    return NS_ERROR_OUT_OF_MEMORY;

  *aNewFrame = it;
  return NS_OK;
  
} // NS_NewScrollBarButtonFrame


nsScrollbarButtonFrame::nsScrollbarButtonFrame(nsIPresShell* aPresShell)
:nsButtonBoxFrame(aPresShell)
{
}



NS_IMETHODIMP 
nsScrollbarButtonFrame::QueryInterface(REFNSIID aIID, void** aInstancePtr)      
{           
  if (aIID.Equals(NS_GET_IID(nsITimerCallback))) {                                         
    *aInstancePtr = (void*)(nsITimerCallback*) this;                                        
    NS_ADDREF_THIS();                                                    
    return NS_OK;                                                        
  }   

  return nsButtonBoxFrame::QueryInterface(aIID, aInstancePtr);                                     
}

NS_IMETHODIMP
nsScrollbarButtonFrame::HandleEvent(nsIPresContext* aPresContext, 
                                      nsGUIEvent* aEvent,
                                      nsEventStatus* aEventStatus)
{  
  // XXX hack until handle release is actually called in nsframe.
  if (aEvent->message == NS_MOUSE_EXIT_SYNTH|| aEvent->message == NS_MOUSE_RIGHT_BUTTON_UP || aEvent->message == NS_MOUSE_LEFT_BUTTON_UP)
     HandleRelease(aPresContext, aEvent, aEventStatus);
  
  return nsButtonBoxFrame::HandleEvent(aPresContext, aEvent, aEventStatus);
}


NS_IMETHODIMP
nsScrollbarButtonFrame::HandlePress(nsIPresContext* aPresContext, 
                     nsGUIEvent*     aEvent,
                     nsEventStatus*  aEventStatus)
{
  MouseClicked();
  nsRepeatService::GetInstance()->Start(this);
  return NS_OK;
}

NS_IMETHODIMP 
nsScrollbarButtonFrame::HandleRelease(nsIPresContext* aPresContext, 
                                 nsGUIEvent*     aEvent,
                                 nsEventStatus*  aEventStatus)
{
  nsRepeatService::GetInstance()->Stop();
  return NS_OK;
}


NS_IMETHODIMP nsScrollbarButtonFrame::Notify(nsITimer *timer)
{
  MouseClicked();
  return NS_OK;
}

void
nsScrollbarButtonFrame::MouseClicked(nsIPresContext* aPresContext, nsGUIEvent* aEvent) 
{
  nsButtonBoxFrame::MouseClicked(aPresContext, aEvent);
  //MouseClicked();
}

void
nsScrollbarButtonFrame::MouseClicked() 
{
   // when we are clicked either increment or decrement the slider position.

   // get the scrollbar control
   nsIFrame* scrollbar;
   GetParentWithTag(nsXULAtoms::scrollbar, this, scrollbar);

   if (scrollbar == nsnull)
       return;

   // get the scrollbars content node
   nsIContent* content = scrollbar->GetContent();

   // get the current pos
   PRInt32 curpos = nsSliderFrame::GetCurrentPosition(content);
   PRInt32 oldpos = curpos;

   // get the max pos
   PRInt32 maxpos = nsSliderFrame::GetMaxPosition(content);

   // get the increment amount
   PRInt32 increment = nsSliderFrame::GetIncrement(content);
#ifdef MOZ_WIDGET_COCOA
   // Emulate the Mac IE behavior of scrolling 2 lines instead of 1
   // on a button press.  This makes scrolling appear smoother and
   // keeps us competitive with IE.
   increment *= 2;
#endif

   nsString value;
   if (NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttr(kNameSpaceID_None, nsHTMLAtoms::type, value))
   {
     // if our class is DecrementButton subtract the current pos by increment amount
     // if our class is IncrementButton increment the current pos by the decrement amount
     if (value.EqualsLiteral("decrement"))
         curpos -= increment;
     else if (value.EqualsLiteral("increment"))
         curpos += increment;

      // make sure the current positon is between the current and max positions
    if (curpos < 0)
       curpos = 0;
    else if (curpos > maxpos)
       curpos = maxpos;

    nsCOMPtr<nsIScrollbarFrame> sb(do_QueryInterface(scrollbar));
    if (sb) {
      nsCOMPtr<nsIScrollbarMediator> m;
      sb->GetScrollbarMediator(getter_AddRefs(m));
      if (m) {
        m->ScrollbarButtonPressed(sb, oldpos, curpos);
        return;
      }
    }

    // set the current position of the slider.
    nsAutoString curposStr;
    curposStr.AppendInt(curpos);

    content->SetAttr(kNameSpaceID_None, nsXULAtoms::smooth, NS_LITERAL_STRING("true"), PR_FALSE);
    content->SetAttr(kNameSpaceID_None, nsXULAtoms::curpos, curposStr, PR_TRUE);
    content->UnsetAttr(kNameSpaceID_None, nsXULAtoms::smooth, PR_FALSE);
  }
}

nsresult
nsScrollbarButtonFrame::GetChildWithTag(nsIPresContext* aPresContext,
                                        nsIAtom* atom, nsIFrame* start,
                                        nsIFrame*& result)
{
  // recursively search our children
  nsIFrame* childFrame = start->GetFirstChild(nsnull);
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

NS_IMETHODIMP
nsScrollbarButtonFrame::Destroy(nsIPresContext* aPresContext)
{
  // Ensure our repeat service isn't going... it's possible that a scrollbar can disappear out
  // from under you while you're in the process of scrolling.
  nsRepeatService::GetInstance()->Stop();
  return nsButtonBoxFrame::Destroy(aPresContext);
}
