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
#include "nsIXULTreeSlice.h"

class nsXULTreeSliceFrame : public nsBoxFrame, public nsIXULTreeSlice
{
public:
  NS_DECL_ISUPPORTS

  friend nsresult NS_NewXULTreeSliceFrame(nsIPresShell* aPresShell, 
                                          nsIFrame** aNewFrame, 
                                          PRBool aIsRoot = PR_FALSE,
                                          nsIBoxLayout* aLayoutManager = nsnull,
                                          PRBool aDefaultHorizontal = PR_TRUE);

  // nsIXULTreeSlice
  NS_IMETHOD IsOutermostFrame(PRBool* aResult) { *aResult = PR_FALSE; return NS_OK; };
  NS_IMETHOD IsGroupFrame(PRBool* aResult) { *aResult = PR_FALSE; return NS_OK; };
  NS_IMETHOD IsRowFrame(PRBool* aResult) { *aResult = PR_TRUE; return NS_OK; };
  NS_IMETHOD GetOnScreenRowCount(PRInt32* aCount) { *aCount = 1; return NS_OK; }

  // nsIBox
  NS_IMETHOD GetPrefSize(nsBoxLayoutState& aState, nsSize& aSize);
  
protected:
  nsXULTreeSliceFrame(nsIPresShell* aPresShell, PRBool aIsRoot = nsnull, nsIBoxLayout* aLayoutManager = nsnull, PRBool aDefaultHorizontal = PR_TRUE);
  virtual ~nsXULTreeSliceFrame();

protected: // Data Members
  
}; // class nsXULTreeSliceFrame
