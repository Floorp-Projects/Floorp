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

/**
 
  Author:
  Eric D Vaughan

**/

#ifndef nsGridLayout_h___
#define nsGridLayout_h___

#include "nsStackLayout.h"
#include "nsIMonument.h"
#include "nsCOMPtr.h"
#include "nsIBox.h"

class nsTempleLayout;
class nsMonumentLayout;
class nsBoxSizeList;

class nsGridLayout : public nsStackLayout, 
                     public nsIMonument
{
public:

  friend nsresult NS_NewGridLayout(nsIPresShell* aPresShell, nsCOMPtr<nsIBoxLayout>& aNewLayout);

  NS_DECL_ISUPPORTS_INHERITED

  NS_IMETHOD CastToTemple(nsTempleLayout** aTemple);
  NS_IMETHOD CastToObelisk(nsObeliskLayout** aObelisk);
  NS_IMETHOD GetOtherMonuments(nsIBox* aBox, nsBoxSizeList** aList);
  NS_IMETHOD GetOtherMonumentsAt(nsIBox* aBox, PRInt32 aIndexOfObelisk, nsBoxSizeList** aList, nsMonumentLayout* aRequestor = nsnull);
  NS_IMETHOD GetOtherTemple(nsIBox* aBox, nsTempleLayout** aTemple, nsIBox** aTempleBox, nsMonumentLayout* aRequestor = nsnull);
  NS_IMETHOD GetMonumentsAt(nsIBox* aBox, PRInt32 aMonumentIndex, nsBoxSizeList** aList);
  NS_IMETHOD BuildBoxSizeList(nsIBox* aBox, nsBoxLayoutState& aState, nsBoxSize*& aFirst, nsBoxSize*& aLast);
  NS_IMETHOD GetParentMonument(nsIBox* aBox, nsCOMPtr<nsIBox>& aParentBox, nsCOMPtr<nsIMonument>& aParentMonument);
  NS_IMETHOD GetMonumentList(nsIBox* aBox, nsBoxLayoutState& aState, nsBoxSizeList** aList);

protected:
  nsGridLayout(nsIPresShell* aShell);
}; // class nsGridLayout

#endif

