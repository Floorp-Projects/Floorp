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
#include "nsIOutlinerColFrame.h"
#include "nsIOutlinerBoxObject.h"

class nsSupportsHashtable;

// The actual frame that paints the cells and rows.
class nsOutlinerColFrame : public nsBoxFrame, public nsIOutlinerColFrame
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOUTLINERCOLFRAME

  friend nsresult NS_NewOutlinerColFrame(nsIPresShell* aPresShell, 
                                         nsIFrame** aNewFrame, 
                                         PRBool aIsRoot = PR_FALSE,
                                         nsIBoxLayout* aLayoutManager = nsnull);

  NS_IMETHODIMP Init(nsIPresContext*  aPresContext,
                     nsIContent*      aContent,
                     nsIFrame*        aParent,
                     nsIStyleContext* aContext,
                     nsIFrame*        aPrevInFlow);

  NS_IMETHOD GetFrameForPoint(nsIPresContext* aPresContext,
                              const nsPoint& aPoint, // Overridden to capture events
                              nsFramePaintLayer aWhichLayer,
                              nsIFrame**     aFrame);

  NS_IMETHOD AttributeChanged(nsIPresContext* aPresContext,
                              nsIContent* aChild,
                              PRInt32 aNameSpaceID,
                              nsIAtom* aAttribute,
                              PRInt32 aModType, 
                              PRInt32 aHint);


protected:
  nsOutlinerColFrame(nsIPresShell* aPresShell, PRBool aIsRoot = nsnull, nsIBoxLayout* aLayoutManager = nsnull);
  virtual ~nsOutlinerColFrame();

protected:
  // Members.
  
  void EnsureOutliner();
  void InvalidateColumnCache(nsIPresContext* aPresContext);
  
  nsCOMPtr<nsIOutlinerBoxObject> mOutliner;
}; // class nsOutlinerColFrame
