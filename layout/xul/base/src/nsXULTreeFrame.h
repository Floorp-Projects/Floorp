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
 * Original Author: David W. Hyatt (hyatt@netscape.com)
 *
 * Contributor(s): 
 */

#include "nsBoxFrame.h"
#include "nsITreeFrame.h"

class nsXULTreeOuterGroupFrame;
class nsIPresShell;

class nsXULTreeFrame : public nsBoxFrame, public nsITreeFrame
{
public:
  friend nsresult NS_NewXULTreeFrame(nsIPresShell* aPresShell, 
                                     nsIFrame** aNewFrame, 
                                     PRBool aIsRoot = PR_FALSE,
                                     nsIBoxLayout* aLayoutManager = nsnull,
                                     PRBool aDefaultHorizontal = PR_TRUE);

  NS_DECL_ISUPPORTS

  NS_IMETHOD DoLayout(nsBoxLayoutState& aBoxLayoutState);
  
  // nsITreeFrame
  NS_IMETHOD EnsureRowIsVisible(PRInt32 aRowIndex);
  NS_IMETHOD GetNextItem(nsIDOMElement* aStartItem, PRInt32 aDelta, nsIDOMElement** aResult);
  NS_IMETHOD GetPreviousItem(nsIDOMElement* aStartItem, PRInt32 aDelta, nsIDOMElement** aResult);
  NS_IMETHOD ScrollToIndex(PRInt32 aRowIndex);
  NS_IMETHOD GetItemAtIndex(PRInt32 aIndex, nsIDOMElement** aResult);
  NS_IMETHOD GetIndexOfItem(nsIPresContext* aPresContext, nsIDOMElement* aElement, PRInt32* aResult);
  NS_IMETHOD GetNumberOfVisibleRows(PRInt32 *aResult);
  NS_IMETHOD GetIndexOfFirstVisibleRow(PRInt32 *aResult);
  NS_IMETHOD GetRowCount(PRInt32* aResult);

protected:
  nsXULTreeFrame(nsIPresShell* aPresShell, PRBool aIsRoot = nsnull, nsIBoxLayout* aLayoutManager = nsnull, PRBool aDefaultHorizontal = PR_TRUE);
  virtual ~nsXULTreeFrame();

  void GetTreeBody(nsXULTreeOuterGroupFrame** aResult);

protected: // Data Members
  nsIPresShell* mPresShell;
}; // class nsXULTreeFrame
