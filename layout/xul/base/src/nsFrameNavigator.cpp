/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

//
// Eric Vaughan
// Netscape Communications
//
// See documentation in associated header file
//

#include "nsFrameNavigator.h"
#include "nsCOMPtr.h"
#include "nsIContent.h"

nsIAtom*
nsFrameNavigator::GetTag(nsIFrame* frame)
{
   nsCOMPtr<nsIContent> content;
   frame->GetContent(getter_AddRefs(content));
   if (content) {
     nsIAtom* atom = nsnull;
     content->GetTag(atom);
     return atom; 
   }

   return nsnull;
}

nsIFrame*
nsFrameNavigator::GetChildBeforeAfter(nsIFrame* start, PRBool before)
{
   nsIFrame* parent = nsnull;
   start->GetParent(&parent);
   PRInt32 index = IndexOf(parent,start);
   PRInt32 count = CountFrames(parent);

   if (index == -1) 
     return nsnull;

   if (before) {
     if (index == 0) {
         return nsnull;
     }

     return GetChildAt(parent, index-1);
   }


   if (index == count-1)
       return nsnull;

   return GetChildAt(parent, index+1);
}

PRInt32
nsFrameNavigator::IndexOf(nsIFrame* parent, nsIFrame* child)
{
  PRInt32 count = 0;

  nsIFrame* childFrame;
  parent->FirstChild(nsnull, &childFrame); 
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
nsFrameNavigator::CountFrames(nsIFrame* aFrame)
{
  PRInt32 count = 0;

  nsIFrame* childFrame;
  aFrame->FirstChild(nsnull, &childFrame); 
  while (nsnull != childFrame) 
  {    
    nsresult rv = childFrame->GetNextSibling(&childFrame);
    NS_ASSERTION(rv == NS_OK,"failed to get next child");
    count++;
  }

  return count;
}

nsIFrame*
nsFrameNavigator::GetChildAt(nsIFrame* parent, PRInt32 index)
{
  PRInt32 count = 0;

  nsIFrame* childFrame;
  parent->FirstChild(nsnull, &childFrame); 
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

