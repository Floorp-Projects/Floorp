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
// David Hyatt & Eric Vaughan
// Netscape Communications
//
// See documentation in associated header file
//

#include "nsProgressMeterFrame.h"
#include "nsCSSRendering.h"
#include "nsIContent.h"
#include "nsIPresContext.h"
#include "nsHTMLAtoms.h"
#include "nsXULAtoms.h"
#include "nsINameSpaceManager.h"
#include "nsCOMPtr.h"
//
// NS_NewToolbarFrame
//
// Creates a new Toolbar frame and returns it in |aNewFrame|
//
nsresult
NS_NewProgressMeterFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame )
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsProgressMeterFrame* it = new (aPresShell) nsProgressMeterFrame(aPresShell);
  if (nsnull == it)
    return NS_ERROR_OUT_OF_MEMORY;

  *aNewFrame = it;
  return NS_OK;
  
} // NS_NewProgressMeterFrame

//
// nsProgressMeterFrame cntr
//
// Init, if necessary
//
nsProgressMeterFrame :: nsProgressMeterFrame (nsIPresShell* aPresShell)
:nsBoxFrame(aPresShell)
{
}

//
// nsProgressMeterFrame dstr
//
// Cleanup, if necessary
//
nsProgressMeterFrame :: ~nsProgressMeterFrame ( )
{
}

NS_IMETHODIMP
nsProgressMeterFrame::SetInitialChildList(nsIPresContext* aPresContext,
                                     nsIAtom*        aListName,
                                     nsIFrame*       aChildList)
{ 
  // Set up our initial flexes.
  nsresult rv = nsBoxFrame::SetInitialChildList(aPresContext, aListName, aChildList);
  AttributeChanged(aPresContext, mContent, kNameSpaceID_None, nsHTMLAtoms::value, 0);
  return rv;
}

NS_IMETHODIMP
nsProgressMeterFrame::AttributeChanged(nsIPresContext* aPresContext,
                               nsIContent* aChild,
                               PRInt32 aNameSpaceID,
                               nsIAtom* aAttribute,
                               PRInt32 aHint)
{
  nsresult rv = nsBoxFrame::AttributeChanged(aPresContext, aChild,
                                             aNameSpaceID, aAttribute, aHint);
  if (NS_OK != rv) {
    return rv;
  }

  // did the progress change?
  if (nsHTMLAtoms::value == aAttribute) {
    PRInt32 childCount;
    mContent->ChildCount(childCount);

    if (childCount >= 2) {
      nsCOMPtr<nsIContent> progressBar;
      nsCOMPtr<nsIContent> progressRemainder;

      mContent->ChildAt(0, *getter_AddRefs(progressBar));
      mContent->ChildAt(1, *getter_AddRefs(progressRemainder));

      nsAutoString value;
      mContent->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::value, value);

      PRInt32 error;
      PRInt32 flex = value.ToInteger(&error);
      if (flex < 0) flex = 0;
      if (flex > 100) flex = 100;

      PRInt32 remainder = 100 - flex;

      nsAutoString leftFlex, rightFlex;
      leftFlex.AppendInt(flex);
      rightFlex.AppendInt(remainder);
      progressBar->SetAttribute(kNameSpaceID_None, nsXULAtoms::flex, leftFlex, PR_TRUE);
      progressRemainder->SetAttribute(kNameSpaceID_None, nsXULAtoms::flex, rightFlex, PR_TRUE);
    }
  }
  return NS_OK;
}

#ifdef NS_DEBUG
NS_IMETHODIMP
nsProgressMeterFrame::GetFrameName(nsString& aResult) const
{
  return MakeFrameName("ProgressMeter", aResult);
}
#endif
