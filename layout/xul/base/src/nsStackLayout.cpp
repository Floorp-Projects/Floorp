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

#include "nsStackLayout.h"
#include "nsIStyleContext.h"
#include "nsCOMPtr.h"
#include "nsBoxLayoutState.h"
#include "nsBox.h"

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
nsStackLayout::GetPrefSize(nsIBox* aBox, nsBoxLayoutState& aBoxLayoutState, nsSize& aSize)
{
   aSize.width = 0;
   aSize.height = 0;

   // run through all the children and get there min, max, and preferred sizes
   // return us the size of the box

   nsIBox* child = nsnull;
   aBox->GetChildBox(&child);
   
   while (child) 
   {  
      // ignore collapsed children
     // PRBool isCollapsed = PR_FALSE;
     // child->IsCollapsed(aBoxLayoutState, isCollapsed);

     // if (!isCollapsed)
     // {
        nsSize pref(0,0);
        child->GetPrefSize(aBoxLayoutState, pref);
        AddMargin(child, pref);
        AddLargestSize(aSize, pref);
     // }

      child->GetNextBox(&child);
   }

   // now add our border and padding and insets
   AddBorderAndPadding(aBox, aSize);
   AddInset(aBox, aSize);

  return NS_OK;
}

NS_IMETHODIMP
nsStackLayout::GetMinSize(nsIBox* aBox, nsBoxLayoutState& aBoxLayoutState, nsSize& aSize)
{
   aSize.width = 0;
   aSize.height = 0;

   // run through all the children and get there min, max, and preferred sizes
   // return us the size of the box

   nsIBox* child = nsnull;
   aBox->GetChildBox(&child);
   
   while (child) 
   {  
       // ignore collapsed children
      //PRBool isCollapsed = PR_FALSE;
      //aBox->IsCollapsed(aBoxLayoutState, isCollapsed);

      //if (!isCollapsed)
      //{
        nsSize min(0,0);
        child->GetMinSize(aBoxLayoutState, min);        
        AddMargin(child, min);
        AddLargestSize(aSize, min);
      //}

      child->GetNextBox(&child);
   }

// now add our border and padding and insets
   AddBorderAndPadding(aBox, aSize);
   AddInset(aBox,aSize);

  return NS_OK;
}

NS_IMETHODIMP
nsStackLayout::GetMaxSize(nsIBox* aBox, nsBoxLayoutState& aBoxLayoutState, nsSize& aSize)
{

   aSize.width = NS_INTRINSICSIZE;
   aSize.height = NS_INTRINSICSIZE;

   // run through all the children and get there min, max, and preferred sizes
   // return us the size of the box


   nsIBox* child = nsnull;
   aBox->GetChildBox(&child);
   
   while (child) 
   {  
      // ignore collapsed children
      //PRBool isCollapsed = PR_FALSE;
      //aBox->IsCollapsed(aBoxLayoutState, isCollapsed);

      //if (!isCollapsed)
      //{
        // if completely redefined don't even ask our child for its size.
        nsSize max(NS_INTRINSICSIZE, NS_INTRINSICSIZE);
        child->GetMaxSize(aBoxLayoutState, max);

        AddMargin(child, max);
        AddSmallestSize(aSize, max);
      //}

      child->GetNextBox(&child);
      
   }

   // now add our border and padding and insets
   AddBorderAndPadding(aBox, aSize);
   AddInset(aBox, aSize);

  return NS_OK;
}


NS_IMETHODIMP
nsStackLayout::GetAscent(nsIBox* aBox, nsBoxLayoutState& aBoxLayoutState, nscoord& aAscent)
{
   aAscent = 0;

   // run through all the children and get there min, max, and preferred sizes
   // return us the size of the box
   
   nsIBox* child = nsnull;
   aBox->GetChildBox(&child);
   
   while (child) 
   {  
      // ignore collapsed children
      //PRBool isCollapsed = PR_FALSE;
      //aBox->IsCollapsed(aBoxLayoutState, isCollapsed);

      //if (!isCollapsed)
      //{
        // if completely redefined don't even ask our child for its size.
        nscoord ascent = 0;
        child->GetAscent(aBoxLayoutState, ascent);
        nsMargin margin;
        child->GetMargin(margin);
        ascent += margin.top + margin.bottom;

        if (ascent > aAscent)
            aAscent = ascent;
      //}

      child->GetNextBox(&child);

   }

  return NS_OK;
}

NS_IMETHODIMP
nsStackLayout::Layout(nsIBox* aBox, nsBoxLayoutState& aState)
{
   nsRect clientRect;
   aBox->GetClientRect(clientRect);
   
   nsIBox* child = nsnull;
   PRBool grow;

   PRInt32 passes = 0;

   do {
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

        // only layout dirty children
        PRBool isDirty = PR_FALSE;
        PRBool hasDirtyChildren = PR_FALSE;

        child->IsDirty(isDirty);
        child->HasDirtyChildren(hasDirtyChildren);

        if (sizeChanged || isDirty || hasDirtyChildren) {
 
          child->SetBounds(aState, childRect);
          child->Layout(aState);
          child->GetBounds(childRect);
          childRect.Inflate(margin);

          // did the child push back on us and get bigger?
          if (childRect.width > clientRect.width) {
             clientRect.width = childRect.width;
             grow = PR_TRUE;
          }

          if (childRect.height > clientRect.height) {
             clientRect.height = childRect.height;
             grow = PR_TRUE;
          }
        }

        child->GetNextBox(&child);
     }
     NS_BOX_ASSERTION(aBox, passes < 10,"Infinite loop! Someone won't stop growing!!");
     //if (passes > 3)
     //   printf("Growing!!!\n");

     passes++;
   } while(grow);

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


