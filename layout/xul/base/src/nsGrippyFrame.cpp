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

//
// Eric Vaughan
// Netscape Communications
//
// See documentation in associated header file
//

#include "nsGrippyFrame.h"
#include "nsScrollbarButtonFrame.h"
#include "nsIStyleContext.h"
#include "nsIPresContext.h"
#include "nsIContent.h"
#include "nsCOMPtr.h"
#include "nsHTMLIIDs.h"
#include "nsUnitConversion.h"
#include "nsINameSpaceManager.h"
#include "nsHTMLAtoms.h"
#include "nsXULAtoms.h"
#include "nsIReflowCommand.h"
//#include "nsSliderFrame.h"
#include "nsIDOMElement.h"
#include "nsIDOMDocument.h"
#include "nsIDocument.h"
#include "nsHTMLParts.h"
#include "nsIPresShell.h"
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsHTMLContainerFrame.h"


//
// NS_NewToolbarFrame
//
// Creates a new Toolbar frame and returns it in |aNewFrame|
//
nsresult
NS_NewGrippyFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame )
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsGrippyFrame* it = new (aPresShell) nsGrippyFrame (aPresShell);
  if (nsnull == it)
    return NS_ERROR_OUT_OF_MEMORY;

  *aNewFrame = it;
  return NS_OK;
  
} // NS_NewGrippyFrame

nsGrippyFrame::nsGrippyFrame(nsIPresShell* aShell):nsButtonBoxFrame(aShell),mCollapsed(PR_FALSE)
{
}

void
nsGrippyFrame::MouseClicked (nsIPresContext* aPresContext, nsGUIEvent* aEvent) 
{
    nsButtonBoxFrame::MouseClicked(aPresContext, aEvent);

    nsIFrame* splitter;
    nsScrollbarButtonFrame::GetParentWithTag(nsXULAtoms::splitter, this, splitter);
    if (splitter == nsnull)
       return;

    // get the splitters content node
    nsCOMPtr<nsIContent> content;
    splitter->GetContent(getter_AddRefs(content));
 
	nsString a; a.AssignWithConversion("collapsed");
	nsString value;
    if (NS_CONTENT_ATTR_HAS_VALUE == content->GetAttr(kNameSpaceID_None, nsXULAtoms::state, value))
    {
     if (value.EqualsWithConversion("collapsed"))
       a.AssignWithConversion("open");
    }

    content->SetAttr(kNameSpaceID_None, nsXULAtoms::state, a, PR_TRUE);

}

/*
void
nsGrippyFrame::MouseClicked(nsIPresContext* aPresContext) 
{

  nsString style;

  if (mCollapsed) {
    style = mCollapsedChildStyle;
  } else {
    // when clicked see if we are in a splitter. 
    nsIFrame* splitter;
    nsScrollbarButtonFrame::GetParentWithTag(nsXULAtoms::splitter, this, splitter);

    if (splitter == nsnull)
       return;

    // get the splitters content node
    nsCOMPtr<nsIContent> content;
    splitter->GetContent(getter_AddRefs(content));

    // get the collapse attribute. If the attribute is not set collapse
    // the child before otherwise collapse the child after
    PRBool before = PR_TRUE;
    nsString value;
    if (NS_CONTENT_ATTR_HAS_VALUE == content->GetAttr(kNameSpaceID_None, nsXULAtoms::collapse, value))
    {
     if (value=="after")
       before = PR_FALSE;
    }

    // find the child just in the box just before the splitter. If we are not currently collapsed then
    // then get the childs style attribute and store it. Then set the child style attribute to be display none.
    // if we are already collapsed then set the child's style back to our stored value.
    nsIFrame* child = GetChildBeforeAfter(splitter,before);
    if (child == nsnull)
      return;

    child->GetContent(getter_AddRefs(mCollapsedChild));

    style = "visibility: collapse";
    mCollapsedChildStyle = "";
    mCollapsedChild->GetAttr(kNameSpaceID_None, nsHTMLAtoms::style, mCollapsedChildStyle);
  }

  mCollapsedChild->SetAttr(kNameSpaceID_None, nsHTMLAtoms::style, style, PR_TRUE);

  mCollapsed = !mCollapsed;

}
*/


nsIFrame*
nsGrippyFrame::GetChildBeforeAfter(nsIPresContext* aPresContext, nsIFrame* start, PRBool before)
{
   nsIFrame* parent = nsnull;
   start->GetParent(&parent);
   PRInt32 index = IndexOf(aPresContext, parent,start);
   PRInt32 count = CountFrames(aPresContext, parent);

   if (index == -1) 
     return nsnull;

   if (before) {
     if (index == 0) {
         return nsnull;
     }

     return GetChildAt(aPresContext, parent, index-1);
   }


   if (index == count-1)
       return nsnull;

   return GetChildAt(aPresContext, parent, index+1);

}

PRInt32
nsGrippyFrame::IndexOf(nsIPresContext* aPresContext, nsIFrame* parent, nsIFrame* child)
{
  PRInt32 count = 0;

  nsIFrame* childFrame;
  parent->FirstChild(aPresContext, nsnull, &childFrame); 
  while (nsnull != childFrame) 
  {    
    if (childFrame == child)
       return count;

    nsresult rv = childFrame->GetNextSibling(&childFrame);
    NS_ASSERTION(rv == NS_OK,"failed to get next child");
    count++;
  }

  return -1;
}

PRInt32
nsGrippyFrame::CountFrames(nsIPresContext* aPresContext, nsIFrame* aFrame)
{
  PRInt32 count = 0;

  nsIFrame* childFrame;
  aFrame->FirstChild(aPresContext, nsnull, &childFrame); 
  while (nsnull != childFrame) 
  {    
    nsresult rv = childFrame->GetNextSibling(&childFrame);
    NS_ASSERTION(rv == NS_OK,"failed to get next child");
    count++;
  }

  return count;
}

nsIFrame*
nsGrippyFrame::GetChildAt(nsIPresContext* aPresContext, nsIFrame* parent, PRInt32 index)
{
  PRInt32 count = 0;

  nsIFrame* childFrame;
  parent->FirstChild(aPresContext, nsnull, &childFrame); 
  while (nsnull != childFrame) 
  {    
    if (count == index)
       return childFrame;

    nsresult rv = childFrame->GetNextSibling(&childFrame);
    NS_ASSERTION(rv == NS_OK,"failed to get next child");
    count++;
  }

  return nsnull;
}

#ifdef DEBUG
NS_IMETHODIMP
nsGrippyFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("Grippy"), aResult);
}
#endif
