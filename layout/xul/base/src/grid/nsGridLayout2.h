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
 *   Eric D Vaughan (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef nsGridLayout2_h___
#define nsGridLayout2_h___

#include "nsStackLayout.h"
#include "nsIGridPart.h"
#include "nsCOMPtr.h"
#include "nsIFrame.h"
#include "nsGrid.h"

class nsGridRowGroupLayout;
class nsGridRowLayout;
class nsGridRow;
class nsBoxLayoutState;
class nsGridCell;

/**
 * The nsBoxLayout implementation for a grid.
 */
class nsGridLayout2 : public nsStackLayout, 
                      public nsIGridPart
{
public:

  friend nsresult NS_NewGridLayout2(nsIPresShell* aPresShell, nsBoxLayout** aNewLayout);

  NS_DECL_ISUPPORTS_INHERITED

  NS_IMETHOD Layout(nsIBox* aBox, nsBoxLayoutState& aBoxLayoutState);
  virtual void IntrinsicWidthsDirty(nsIBox* aBox, nsBoxLayoutState& aBoxLayoutState);

  virtual nsGridRowGroupLayout* CastToRowGroupLayout() { return nsnull; }
  virtual nsGridLayout2* CastToGridLayout() { return this; }
  virtual nsGrid* GetGrid(nsIBox* aBox, PRInt32* aIndex, nsGridRowLayout* aRequestor=nsnull);
  virtual nsIGridPart* GetParentGridPart(nsIBox* aBox, nsIBox** aParentBox) {
    NS_NOTREACHED("Should not be called"); return nsnull;
  }
  virtual nsSize GetMinSize(nsIBox* aBox, nsBoxLayoutState& aBoxLayoutState);
  virtual nsSize GetMaxSize(nsIBox* aBox, nsBoxLayoutState& aBoxLayoutState);
  virtual nsSize GetPrefSize(nsIBox* aBox, nsBoxLayoutState& aBoxLayoutState);
  virtual void CountRowsColumns(nsIBox* aBox, PRInt32& aRowCount, PRInt32& aComputedColumnCount) { aRowCount++; }
  virtual void DirtyRows(nsIBox* aBox, nsBoxLayoutState& aState) { }
  virtual PRInt32 BuildRows(nsIBox* aBox, nsGridRow* aRows);
  virtual nsMargin GetTotalMargin(nsIBox* aBox, bool aIsHorizontal);
  virtual Type GetType() { return eGrid; }
  virtual void ChildrenInserted(nsIBox* aBox, nsBoxLayoutState& aState,
                                nsIBox* aPrevBox,
                                const nsFrameList::Slice& aNewChildren);
  virtual void ChildrenAppended(nsIBox* aBox, nsBoxLayoutState& aState,
                                const nsFrameList::Slice& aNewChildren);
  virtual void ChildrenRemoved(nsIBox* aBox, nsBoxLayoutState& aState,
                             nsIBox* aChildList);
  virtual void ChildrenSet(nsIBox* aBox, nsBoxLayoutState& aState,
                         nsIBox* aChildList);

  virtual nsIGridPart* AsGridPart() { return this; }

  static void AddOffset(nsBoxLayoutState& aState, nsIBox* aChild, nsSize& aSize);

protected:

  nsGridLayout2(nsIPresShell* aShell);
  nsGrid mGrid;

private:
  void AddWidth(nsSize& aSize, nscoord aSize2, bool aIsHorizontal);


}; // class nsGridLayout2


#endif

