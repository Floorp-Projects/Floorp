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

#include "nsLeafBoxFrame.h"
#include "nsIOutlinerBoxObject.h"
#include "nsIOutlinerStore.h"
#include "nsIOutlinerRangeList.h"

class nsSupportsHashtable;

class nsOutlinerBodyFrame : public nsLeafBoxFrame, public nsIOutlinerBoxObject
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOUTLINERBOXOBJECT

  // Painting methods.
  // Paint is the generic nsIFrame paint method.  We override this method
  // to paint our contents (our rows and cells).
  NS_IMETHOD Paint(nsIPresContext*      aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect&        aDirtyRect,
                   nsFramePaintLayer    aWhichLayer);

  // This method paints a single row in the outliner.
  NS_IMETHOD PaintRow(int aRowIndex, 
                      nsIPresContext*      aPresContext,
                      nsIRenderingContext& aRenderingContext,
                      const nsRect&        aDirtyRect,
                      nsFramePaintLayer    aWhichLayer);

  // This method paints a specific cell in a given row of the outliner.
  NS_IMETHOD PaintCell(int aRowIndex, 
                       const PRUnichar* aColID, 
                       nsIPresContext*      aPresContext,
                       nsIRenderingContext& aRenderingContext,
                       const nsRect&        aDirtyRect,
                       nsFramePaintLayer    aWhichLayer);

  friend nsresult NS_NewOutlinerBodyFrame(nsIPresShell* aPresShell, 
                                          nsIFrame** aNewFrame);

protected:
  nsOutlinerBodyFrame(nsIPresShell* aPresShell);
  virtual ~nsOutlinerBodyFrame();

protected: // Data Members
  // The current store for this outliner widget.  We get all of our row and cell data
  // from the store.
  nsCOMPtr<nsIOutlinerStore> mStore;    
  
  // A cache of all the style contexts we have seen for rows of the tree.  This is a mapping from
  // a list of atoms to a corresponding style context.  This cache stores every combination that
  // occurs in the tree, so for n distinct row properties, this cache could have 2 to the n entries
  // (the power set of all row properties).
  nsSupportsHashtable* mRowStyleCache;

  // A cache of all the style contexts we have seen for cells of the tree.  This is a mapping from
  // a list of atoms to a corresponding style context.  This cache stores every combination that
  // occurs in the tree, so for n distinct cell properties, this cache could have 2 to the n entries
  // (the power set of all cell properties).
  nsSupportsHashtable* mCellStyleCache;

  // The index of the first visible row and the # of rows visible onscreen.  
  // The outliner only examines onscreen rows, starting from
  // this index and going up to index+pageCount.
  PRInt32 mTopRowIndex;
  PRInt32 mPageCount;

  // Our current selection.
  nsCOMPtr<nsIOutlinerRangeList> mSelectedRows;

}; // class nsOutlinerBodyFrame
