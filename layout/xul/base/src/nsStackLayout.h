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
 *   David Hyatt (hyatt@netscape.com)
 */

/**

  Eric D Vaughan
  A frame that can have multiple children. Only one child may be displayed at one time. So the
  can be flipped though like a deck of cards.
 
**/

#ifndef nsStackLayout_h___
#define nsStackLayout_h___

#include "nsBoxLayout.h"
#include "nsCOMPtr.h"

class nsStackLayout : public nsBoxLayout
{
public:

  friend nsresult NS_NewStackLayout(nsIPresShell* aPresShell, nsCOMPtr<nsIBoxLayout>& aNewLayout);
  static void Shutdown();

  nsStackLayout();

  NS_IMETHOD Layout(nsIBox* aBox, nsBoxLayoutState& aState);

  NS_IMETHOD GetPrefSize(nsIBox* aBox, nsBoxLayoutState& aBoxLayoutState, nsSize& aSize);
  NS_IMETHOD GetMinSize(nsIBox* aBox, nsBoxLayoutState& aBoxLayoutState, nsSize& aSize);
  NS_IMETHOD GetMaxSize(nsIBox* aBox, nsBoxLayoutState& aBoxLayoutState, nsSize& aSize);
  NS_IMETHOD GetAscent(nsIBox* aBox, nsBoxLayoutState& aBoxLayoutState, nscoord& aAscent);

protected:
  PRBool AddOffset(nsBoxLayoutState& aState, nsIBox* aChild, nsSize& aSize);

private:
  static nsIBoxLayout* gInstance;

}; // class nsStackLayout



#endif

