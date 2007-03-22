/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Dave Hyatt <hyatt@mozilla.org> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsBoxFrame.h"

class nsITreeBoxObject;

nsIFrame* NS_NewTreeColFrame(nsIPresShell* aPresShell, 
                             nsStyleContext* aContext,
                             PRBool aIsRoot = PR_FALSE,
                             nsIBoxLayout* aLayoutManager = nsnull);

class nsTreeColFrame : public nsBoxFrame
{
public:
  NS_DECL_ISUPPORTS

  nsTreeColFrame(nsIPresShell* aPresShell,
                 nsStyleContext* aContext,
                 PRBool aIsRoot = nsnull,
                 nsIBoxLayout* aLayoutManager = nsnull):
    nsBoxFrame(aPresShell, aContext, aIsRoot, aLayoutManager) {}

  NS_IMETHOD Init(nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIFrame*        aPrevInFlow);

  virtual void Destroy();

  NS_IMETHOD BuildDisplayListForChildren(nsDisplayListBuilder*   aBuilder,
                                         const nsRect&           aDirtyRect,
                                         const nsDisplayListSet& aLists);

  NS_IMETHOD AttributeChanged(PRInt32 aNameSpaceID,
                              nsIAtom* aAttribute,
                              PRInt32 aModType);

  virtual void SetBounds(nsBoxLayoutState& aBoxLayoutState, const nsRect& aRect,
                         PRBool aRemoveOverflowArea = PR_FALSE);

  friend nsIFrame* NS_NewTreeColFrame(nsIPresShell* aPresShell, 
                                      PRBool aIsRoot,
                                      nsIBoxLayout* aLayoutManager);

protected:
  virtual ~nsTreeColFrame();

  /**
   * @return the tree box object of the tree this column belongs to, or nsnull.
   */
  nsITreeBoxObject* GetTreeBoxObject();

  /**
   * Helper method that gets the nsITreeColumns object this column belongs to
   * and calls InvalidateColumns() on it.
   */
  void InvalidateColumns();
};
