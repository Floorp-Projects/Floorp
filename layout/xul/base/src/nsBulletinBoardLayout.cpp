/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

//
// Eric Vaughan
// Netscape Communications
//
// See documentation in associated header file
//

#include "nsBulletinBoardLayout.h"
#include "nsIStyleContext.h"
#include "nsCOMPtr.h"
#include "nsBoxLayoutState.h"
#include "nsBox.h"
#include "nsHTMLAtoms.h"
#include "nsXULAtoms.h"
#include "nsIContent.h"
#include "nsINameSpaceManager.h"

nsIBoxLayout* nsBulletinBoardLayout::gInstance = nsnull;

nsresult
NS_NewBulletinBoardLayout( nsIPresShell* aPresShell, nsCOMPtr<nsIBoxLayout>& aNewLayout)
{
  if (!nsBulletinBoardLayout::gInstance) {
    nsBulletinBoardLayout::gInstance = new nsBulletinBoardLayout();
    NS_IF_ADDREF(nsBulletinBoardLayout::gInstance);
  }
  // we have not instance variables so just return our static one.
  aNewLayout = nsBulletinBoardLayout::gInstance;
  return NS_OK;
} 

/*static*/ void
nsBulletinBoardLayout::Shutdown()
{
  NS_IF_RELEASE(gInstance);
}

nsBulletinBoardLayout::nsBulletinBoardLayout()
{
}

NS_IMETHODIMP
nsBulletinBoardLayout::GetPrefSize(nsIBox* aBox, nsBoxLayoutState& aState, nsSize& aSize)
{
   aSize.width = 0;
   aSize.height = 0;

   // we are as wide as the widest child plus its left offset
   // we are tall as the tallest child plus its top offset
   nsIBox* child = nsnull;
   aBox->GetChildBox(&child);
   
   while (child) 
   {  
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
nsBulletinBoardLayout::GetMinSize(nsIBox* aBox, nsBoxLayoutState& aState, nsSize& aSize)
{
   aSize.width = 0;
   aSize.height = 0;

   // run through all the children and get there min, max, and preferred sizes
   // return us the size of the box

   nsIBox* child = nsnull;
   aBox->GetChildBox(&child);
   
   while (child) 
   {  
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
nsBulletinBoardLayout::GetMaxSize(nsIBox* aBox, nsBoxLayoutState& aState, nsSize& aSize)
{

   aSize.width = NS_INTRINSICSIZE;
   aSize.height = NS_INTRINSICSIZE;

   // run through all the children and get there min, max, and preferred sizes
   // return us the size of the box


   nsIBox* child = nsnull;
   aBox->GetChildBox(&child);
   
   while (child) 
   {  
       // if completely redefined don't even ask our child for its size.
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
nsBulletinBoardLayout::GetAscent(nsIBox* aBox, nsBoxLayoutState& aState, nscoord& aAscent)
{
  nsSize size(0,0);
  GetPrefSize(aBox, aState, size);
  aAscent = size.height;
  return NS_OK;
}

nsresult
nsBulletinBoardLayout::AddOffset(nsBoxLayoutState& aState, nsIBox* aChild, nsSize& aSize)
{
      nsSize offset(0,0);

      // get the left and top offsets
      const nsStylePosition* pos;
      nsIFrame* frame;
      aChild->GetFrame(&frame);
      frame->GetStyleData(eStyleStruct_Position,(const nsStyleStruct*&) pos);
      if (eStyleUnit_Coord == pos->mOffset.GetLeftUnit()) {
         nsStyleCoord left = 0;
         pos->mOffset.GetLeft(left);
         offset.width = left.GetCoordValue();
      }

      if (eStyleUnit_Coord == pos->mOffset.GetTopUnit()) {
         nsStyleCoord top = 0;
         pos->mOffset.GetTop(top);
         offset.height = top.GetCoordValue();
      }


    nsCOMPtr<nsIContent> content;
    frame->GetContent(getter_AddRefs(content));

    if (content) {
        nsIPresContext* presContext = aState.GetPresContext();

        nsAutoString value;
        PRInt32 error;

        if (NS_CONTENT_ATTR_HAS_VALUE == content->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::left, value))
        {
            float p2t;
            presContext->GetScaledPixelsToTwips(&p2t);

            value.Trim("%");

            offset.width = NSIntPixelsToTwips(value.ToInteger(&error), p2t);
        }

        if (NS_CONTENT_ATTR_HAS_VALUE == content->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::top, value))
        {
            float p2t;
            presContext->GetScaledPixelsToTwips(&p2t);

            value.Trim("%");

            offset.height = NSIntPixelsToTwips(value.ToInteger(&error), p2t);
        }
    }

    aSize += offset;

    return NS_OK;
}


NS_IMETHODIMP
nsBulletinBoardLayout::Layout(nsIBox* aBox, nsBoxLayoutState& aState)
{
   nsRect clientRect;
   aBox->GetClientRect(clientRect);

   nsRect childRect;
   
   nsIBox* child = nsnull;
   aBox->GetChildBox(&child);
   nsRect damageRect;
   PRBool damageRectSet = PR_FALSE;

   while (child) 
   {  
     // only layout dirty children
     PRBool isDirty = PR_FALSE;
     PRBool hasDirtyChildren = PR_FALSE;

     child->IsDirty(isDirty);
     child->HasDirtyChildren(hasDirtyChildren);

     if (isDirty || hasDirtyChildren) {
  
        nsRect oldRect;
        child->GetBounds(oldRect);

        // get the preferred size of the child
        nsSize pref(0,0);
        child->GetPrefSize(aState, pref);

        // add in its margin
        nsMargin margin;
        child->GetMargin(margin);

        // offset it from the inside of our left top border by the offset
        nsSize offset(0,0);
        AddOffset(aState, child, offset);

        childRect.width = pref.width;
        childRect.height = pref.height;
        childRect.x = clientRect.x + offset.width + margin.left;
        childRect.y = clientRect.y + offset.height + margin.top;

        if (childRect != oldRect)
        {
          // redraw the new and old positions if the 
          // child moved.
          // if the new and old rect intersect meaning we just moved a little
          // then just redraw the intersection. If they don't intersect meaning
          // we moved far redraw both separately.
          if (childRect.Intersects(oldRect)) {
             nsRect u;
             u.UnionRect(oldRect, childRect);
             aBox->Redraw(aState, &u);
          } else {
            aBox->Redraw(aState, &oldRect);
            aBox->Redraw(aState, &childRect);
          }
        }

        // layout child
        child->SetBounds(aState, childRect);
        child->Layout(aState);
     }

     child->GetNextBox(&child);
   }
   
   return NS_OK;
}

