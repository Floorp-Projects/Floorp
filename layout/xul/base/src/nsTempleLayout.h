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

#ifndef nsTempleLayout_h___
#define nsTempleLayout_h___

#include "nsMonumentLayout.h"

class nsTempleLayout : public nsMonumentLayout
{
public:

  friend nsresult NS_NewTempleLayout(nsIPresShell* aPresShell, nsCOMPtr<nsIBoxLayout>& aNewLayout);

  NS_IMETHOD CastToTemple(nsTempleLayout** aTemple);
  NS_IMETHOD BuildBoxSizeList(nsIBox* aBox, nsBoxLayoutState& aState, nsBoxSize*& aFirst, nsBoxSize*& aLast, PRBool aIsHorizontal);
  NS_IMETHOD GetMonumentList(nsIBox* aBox, nsBoxLayoutState& aState, nsBoxSizeList** aList);
  NS_IMETHOD ChildrenInserted(nsIBox* aBox, nsBoxLayoutState& aState, nsIBox* aPrevBox, nsIBox* aChildList);
  NS_IMETHOD ChildrenAppended(nsIBox* aBox, nsBoxLayoutState& aState, nsIBox* aChildList);
  NS_IMETHOD ChildrenRemoved(nsIBox* aBox, nsBoxLayoutState& aState, nsIBox* aChildList);
  NS_IMETHOD DesecrateMonuments(nsIBox* aBox, nsBoxLayoutState& aState);
  NS_IMETHOD EnscriptionChanged(nsBoxLayoutState& aState, PRInt32 aIndex);
  NS_IMETHOD GetMinSize(nsIBox* aBox, nsBoxLayoutState& aBoxLayoutState, nsSize& aSize);

protected:

  nsTempleLayout(nsIPresShell* aShell);
  virtual ~nsTempleLayout();

private:
  nsBoxSizeList* mMonuments;
};

#endif

