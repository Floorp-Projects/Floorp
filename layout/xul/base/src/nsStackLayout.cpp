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
 *   David Hyatt (hyatt@netscape.com)
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

#include "nsStackLayout.h"
#include "nsCOMPtr.h"
#include "nsBoxLayoutState.h"
#include "nsBox.h"
#include "nsBoxFrame.h"
#include "nsHTMLAtoms.h"
#include "nsXULAtoms.h"
#include "nsIContent.h"
#include "nsINameSpaceManager.h"

nsIBoxLayout* nsStackLayout::gInstance = nsnull;

nsresult
NS_NewStackLayout( nsIPresShell* aPresShell, nsCOMPtr<nsIBoxLayout>& aNewLayout)
{
  if (!nsStackLayout::gInstance) {
    nsStackLayout::gInstance = new nsStackLayout();
    NS_IF_ADDREF(nsStackLayout::gInstance);
  }
  // we have not instance variables so just return our static one.
  aNewLayout = nsStackLayout::gInstance;
  return NS_OK;
} 

/*static*/ void
nsStackLayout::Shutdown()
{
  NS_IF_RELEASE(gInstance);
}

nsStackLayout::nsStackLayout()
{
}

NS_IMETHODIMP
nsStackLayout::GetPrefSize(nsIBox* aBox, nsBoxLayoutState& aState, nsSize& aSize)
{
  aSize.width = 0;
  aSize.height = 0;

  // we are as wide as the widest child plus its left offset
  // we are tall as the tallest child plus its top offset
  nsIBox* child = nsnull;
  aBox->GetChildBox(&child);
 
  while (child) {  
    nsSize pref(0,0);
    child->GetPrefSize(aState, pref);

    AddMargin(child, pref);
    AddOffset(aState, child, pref);
    AddLargestSize(aSize, pref);

    child->GetNextBox(&child);
  }

  // now add our border and padding and insets
  AddBorderAndPadding(aBox, aSize);
  AddInset(aBox, aSize);

  return NS_OK;
}

NS_IMETHODIMP
nsStackLayout::GetMinSize(nsIBox* aBox, nsBoxLayoutState& aState, nsSize& aSize)
{
  aSize.width = 0;
  aSize.height = 0;

  // run through all the children and get their min, max, and preferred sizes
  
  nsIBox* child = nsnull;
  aBox->GetChildBox(&child);
   
  while (child) {  
    nsSize min(0,0);
    child->GetMinSize(aState, min);        
    AddMargin(child, min);
    AddOffset(aState, child, min);
    AddLargestSize(aSize, min);

    child->GetNextBox(&child);
  }

  // now add our border and padding and insets
  AddBorderAndPadding(aBox, aSize);
  AddInset(aBox,aSize);

  return NS_OK;
}

NS_IMETHODIMP
nsStackLayout::GetMaxSize(nsIBox* aBox, nsBoxLayoutState& aState, nsSize& aSize)
{
  aSize.width = NS_INTRINSICSIZE;
  aSize.height = NS_INTRINSICSIZE;

  // run through all the children and get their min, max, and preferred sizes
 
  nsIBox* child = nsnull;
  aBox->GetChildBox(&child);
   
  while (child) {  
    nsSize max(NS_INTRINSICSIZE, NS_INTRINSICSIZE);
    child->GetMaxSize(aState, max);

    AddMargin(child, max);
    AddOffset(aState, child, max);
    AddSmallestSize(aSize, max);

    child->GetNextBox(&child);
  }

  // now add our border and padding and insets
  AddBorderAndPadding(aBox, aSize);
  AddInset(aBox, aSize);

  return NS_OK;
}


NS_IMETHODIMP
nsStackLayout::GetAscent(nsIBox* aBox, nsBoxLayoutState& aState, nscoord& aAscent)
{
  aAscent = 0;
  nsIBox* child = nsnull;
  aBox->GetChildBox(&child);
   
  while (child) {  
    nscoord ascent = 0;
    child->GetAscent(aState, ascent);
    nsMargin margin;
    child->GetMargin(margin);
    ascent += margin.top + margin.bottom;
    if (ascent > aAscent)
      aAscent = ascent;
    child->GetNextBox(&child);
  }

  return NS_OK;
}

PRBool
nsStackLayout::AddOffset(nsBoxLayoutState& aState, nsIBox* aChild, nsSize& aSize)
{
  nsSize offset(0,0);
  
  // get the left and top offsets
  nsIFrame* frame;
  aChild->GetFrame(&frame);
  
  // As an optimization, we cache the fact that we are not positioned to avoid
  // wasting time fetching attributes and checking style data.
  if (frame->GetStateBits() & NS_STATE_STACK_NOT_POSITIONED)
    return PR_FALSE;
  
  PRBool offsetSpecified = PR_FALSE;
  const nsStylePosition* pos = frame->GetStylePosition();
  if (eStyleUnit_Coord == pos->mOffset.GetLeftUnit()) {
     nsStyleCoord left = 0;
     pos->mOffset.GetLeft(left);
     offset.width = left.GetCoordValue();
     offsetSpecified = PR_TRUE;
  }

  if (eStyleUnit_Coord == pos->mOffset.GetTopUnit()) {
     nsStyleCoord top = 0;
     pos->mOffset.GetTop(top);
     offset.height = top.GetCoordValue();
     offsetSpecified = PR_TRUE;
  }

  nsIContent* content = frame->GetContent();

  if (content) {
    nsIPresContext* presContext = aState.PresContext();
    nsAutoString value;
    PRInt32 error;

    if (NS_CONTENT_ATTR_HAS_VALUE == content->GetAttr(kNameSpaceID_None, nsHTMLAtoms::left, value)) {
      float p2t;
      presContext->GetScaledPixelsToTwips(&p2t);
      value.Trim("%");
      offset.width = NSIntPixelsToTwips(value.ToInteger(&error), p2t);
      offsetSpecified = PR_TRUE;
    }

    if (NS_CONTENT_ATTR_HAS_VALUE == content->GetAttr(kNameSpaceID_None, nsHTMLAtoms::top, value)) {
      float p2t;
      presContext->GetScaledPixelsToTwips(&p2t);
      value.Trim("%");
      offset.height = NSIntPixelsToTwips(value.ToInteger(&error), p2t);
      offsetSpecified = PR_TRUE;
    }
  }

  aSize += offset;

  if (!offsetSpecified) {
    // If no offset was specified at all, then we cache this fact to avoid requerying
    // CSS or the content model.
    frame->AddStateBits(NS_STATE_STACK_NOT_POSITIONED);
  }
    
  return offsetSpecified;
}


NS_IMETHODIMP
nsStackLayout::Layout(nsIBox* aBox, nsBoxLayoutState& aState)
{
  nsRect clientRect;
  aBox->GetClientRect(clientRect);

  PRBool grow;

  do {
    nsIBox* child = nsnull;
    aBox->GetChildBox(&child);
    grow = PR_FALSE;

    while (child) 
    {  
      nsMargin margin;
      child->GetMargin(margin);
      nsRect childRect(clientRect);
      childRect.Deflate(margin);

      if (childRect.width < 0)
        childRect.width = 0;

      if (childRect.height < 0)
        childRect.height = 0;

      nsRect oldRect;
      child->GetBounds(oldRect);

      PRBool sizeChanged = (oldRect != childRect);

      // only lay out dirty children or children whose sizes have changed
      PRBool isDirty = PR_FALSE;
      PRBool hasDirtyChildren = PR_FALSE;

      child->IsDirty(isDirty);
      child->HasDirtyChildren(hasDirtyChildren);

      if (sizeChanged || isDirty || hasDirtyChildren) {
          // add in the child's margin
          nsMargin margin;
          child->GetMargin(margin);

          // obtain our offset from the top left border of the stack's content box.
          nsSize offset(0,0);
          PRBool offsetSpecified = AddOffset(aState, child, offset);

          // Correct the child's x/y position by adding in both the margins
          // and the left/top offset.
          childRect.x = clientRect.x + offset.width + margin.left;
          childRect.y = clientRect.y + offset.height + margin.top;
          
          // If we have an offset, we don't stretch the child.  Just use
          // its preferred size.
          if (offsetSpecified) {
            nsSize pref(0,0);
            child->GetPrefSize(aState, pref);
            childRect.width = pref.width;
            childRect.height = pref.height;
          }

          // Now place the child.
          child->SetBounds(aState, childRect);

          // Flow the child.
          child->Layout(aState);

          // Get the child's new rect.
          child->GetBounds(childRect);
          childRect.Inflate(margin);

          // Did the child push back on us and get bigger?
          if (offset.width + childRect.width > clientRect.width) {
            clientRect.width = childRect.width + offset.width;
            grow = PR_TRUE;
          }

          if (offset.height + childRect.height > clientRect.height) {
            clientRect.height = childRect.height + offset.height;
            grow = PR_TRUE;
          }

          if (childRect.x != oldRect.x || childRect.y != oldRect.y)
          {
            // redraw the new and old positions if the 
            // child moved.
            // if the new and old rect intersect meaning we just moved a little
            // then just redraw the intersection. If they don't intersect (meaning
            // we moved a good distance) redraw both separately.
            if (childRect.Intersects(oldRect)) {
              nsRect u;
              u.UnionRect(oldRect, childRect);
              aBox->Redraw(aState, &u);
            } else {
              aBox->Redraw(aState, &oldRect);
              aBox->Redraw(aState, &childRect);
            }
          }
       }

       child->GetNextBox(&child);
     }
   } while (grow);
   
   // if some HTML inside us got bigger we need to force ourselves to
   // get bigger
   nsRect bounds;
   aBox->GetBounds(bounds);
   nsMargin bp;
   aBox->GetBorderAndPadding(bp);
   clientRect.Inflate(bp);
   aBox->GetInset(bp);
   clientRect.Inflate(bp);

   if (clientRect.width > bounds.width || clientRect.height > bounds.height)
   {
     if (clientRect.width > bounds.width)
       bounds.width = clientRect.width;
     if (clientRect.height > bounds.height)
       bounds.height = clientRect.height;

     aBox->SetBounds(aState, bounds);
   }

   return NS_OK;
}

