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
#include "nsIOutlinerView.h"
#include "nsICSSPseudoComparator.h"
#include "nsIScrollbarMediator.h"
#include "nsIWidget.h"

class nsSupportsHashtable;

class nsDFAState : public nsHashKey
{
public:
  PRUint32 mStateID;

  nsDFAState(PRUint32 aID) :mStateID(aID) {};

  PRUint32 GetStateID() { return mStateID; };

  PRUint32 HashCode(void) const {
    return mStateID;
  }

  PRBool Equals(const nsHashKey *aKey) const {
    nsDFAState* key = (nsDFAState*)aKey;
    return key->mStateID == mStateID;
  }

  nsHashKey *Clone(void) const {
    return new nsDFAState(mStateID);
  }
};

class nsTransitionKey : public nsHashKey
{
public:
  PRUint32 mState;
  nsCOMPtr<nsIAtom> mInputSymbol;

  nsTransitionKey(PRUint32 aState, nsIAtom* aSymbol) :mState(aState), mInputSymbol(aSymbol) {};

  PRUint32 HashCode(void) const {
    // Make a 32-bit integer that combines the low-order 16 bits of the state and the input symbol.
    PRInt32 hb = mState << 16;
    PRInt32 lb = (((PRInt32)mInputSymbol.get()) << 16) >> 16;
    return hb+lb;
  }

  PRBool Equals(const nsHashKey *aKey) const {
    nsTransitionKey* key = (nsTransitionKey*)aKey;
    return key->mState == mState && key->mInputSymbol == mInputSymbol;
  }

  nsHashKey *Clone(void) const {
    return new nsTransitionKey(mState, mInputSymbol);
  }
};

class nsOutlinerStyleCache 
{
public:
  nsOutlinerStyleCache() :mTransitionTable(nsnull), mCache(nsnull), mNextState(0) {};
  virtual ~nsOutlinerStyleCache() { delete mTransitionTable; delete mCache; };

  nsresult GetStyleContext(nsICSSPseudoComparator* aComparator, nsIPresContext* aPresContext, 
                           nsIContent* aContent, 
                           nsIStyleContext* aContext, nsIAtom* aPseudoElement,
                           nsISupportsArray* aInputWord,
                           nsIStyleContext** aResult);

protected:
  // A transition table for a deterministic finite automaton.  The DFA
  // takes as its input a single pseudoelement and an ordered set of properties.  
  // It transitions on an input word that is the concatenation of the pseudoelement supplied
  // with the properties in the array.
  // 
  // It transitions from state to state by looking up entries in the transition table (which is
  // a mapping from (S,i)->S', where S is the current state, i is the next
  // property in the input word, and S' is the state to transition to.
  //
  // If S' is not found, it is constructed and entered into the hashtable
  // under the key (S,i).
  //
  // Once the entire word has been consumed, the final state is used
  // to reference the cache table to locate the style context.
  nsHashtable* mTransitionTable;

  // The cache of all active style contexts.  This is a hash from 
  // a final state in the DFA, Sf, to the resultant style context.
  nsSupportsHashtable* mCache;

  // An integer counter that is used when we need to make new states in the
  // DFA.
  PRUint32 mNextState;
};

// This class is our column info.  We use it to iterate our columns and to obtain
// information about each column.
class nsOutlinerColumn {
  nsOutlinerColumn* mNext;

  nsString mID;
  PRUint32 mCropStyle;
  PRUint32 mTextAlignment;
  
  PRBool mIsPrimaryCol;
  PRBool mIsCyclerCol;

  nsIFrame* mColFrame;
  nsIContent* mColElement;

public:
  nsOutlinerColumn(nsIContent* aColElement, nsIFrame* aFrame);
  virtual ~nsOutlinerColumn() { delete mNext; };

  void SetNext(nsOutlinerColumn* aNext) { mNext = aNext; };
  nsOutlinerColumn* GetNext() { return mNext; };

  nsIContent* GetElement() { return mColElement; };

  nscoord GetWidth();
  const PRUnichar* GetID() { return mID.GetUnicode(); };

  PRBool IsPrimary() { return mIsPrimaryCol; };
  PRBool IsCycler() { return mIsCyclerCol; };

  PRInt32 GetCropStyle() { return mCropStyle; };
};

// The actual frame that paints the cells and rows.
class nsOutlinerBodyFrame : public nsLeafBoxFrame, public nsIOutlinerBoxObject, public nsICSSPseudoComparator,
                            public nsIScrollbarMediator
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOUTLINERBOXOBJECT

  // nsICSSPseudoComparator
  NS_IMETHOD PseudoMatches(nsIAtom* aTag, nsCSSSelector* aSelector, PRBool* aResult);

  // nsIScrollbarMediator
  NS_IMETHOD PositionChanged(PRInt32 aOldIndex, PRInt32& aNewIndex);
  NS_IMETHOD ScrollbarButtonPressed(PRInt32 aOldIndex, PRInt32 aNewIndex);
  NS_IMETHOD VisibilityChanged(PRBool aVisible) { Invalidate(); return NS_OK; };

  // Overridden from nsIFrame to cache our pres context.
  NS_IMETHOD Init(nsIPresContext* aPresContext, nsIContent* aContent,
                  nsIFrame* aParent, nsIStyleContext* aContext, nsIFrame* aPrevInFlow);
  NS_IMETHOD Destroy(nsIPresContext* aPresContext);
  NS_IMETHOD Reflow(nsIPresContext* aPresContext,
                    nsHTMLReflowMetrics& aReflowMetrics,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus& aStatus);


  // Painting methods.
  // Paint is the generic nsIFrame paint method.  We override this method
  // to paint our contents (our rows and cells).
  NS_IMETHOD Paint(nsIPresContext*      aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect&        aDirtyRect,
                   nsFramePaintLayer    aWhichLayer);

  // This method paints a specific column background of the outliner.
  NS_IMETHOD PaintColumn(nsOutlinerColumn*    aColumn,
                         const nsRect& aCellRect,
                         nsIPresContext*      aPresContext,
                         nsIRenderingContext& aRenderingContext,
                         const nsRect&        aDirtyRect,
                         nsFramePaintLayer    aWhichLayer);

  // This method paints a single row in the outliner.
  NS_IMETHOD PaintRow(int aRowIndex, const nsRect& aRowRect,
                      nsIPresContext*      aPresContext,
                      nsIRenderingContext& aRenderingContext,
                      const nsRect&        aDirtyRect,
                      nsFramePaintLayer    aWhichLayer);

  // This method paints a specific cell in a given row of the outliner.
  NS_IMETHOD PaintCell(int aRowIndex, 
                       nsOutlinerColumn*    aColumn,
                       const nsRect& aCellRect,
                       nsIPresContext*      aPresContext,
                       nsIRenderingContext& aRenderingContext,
                       const nsRect&        aDirtyRect,
                       nsFramePaintLayer    aWhichLayer);

  // This method paints the text string inside a particular cell of the outliner.
  NS_IMETHOD PaintText(int aRowIndex, 
                       nsOutlinerColumn*    aColumn,
                       const nsRect& aTextRect,
                       nsIPresContext*      aPresContext,
                       nsIRenderingContext& aRenderingContext,
                       const nsRect&        aDirtyRect,
                       nsFramePaintLayer    aWhichLayer);

  // This method is called with a specific style context and rect to
  // paint the background rect as if it were a full-blown frame.
  NS_IMETHOD PaintBackgroundLayer(nsIStyleContext* aStyleContext, nsIPresContext* aPresContext, 
                                  nsIRenderingContext& aRenderingContext, 
                                  const nsRect& aRect, const nsRect& aDirtyRect);

  friend nsresult NS_NewOutlinerBodyFrame(nsIPresShell* aPresShell, 
                                          nsIFrame** aNewFrame);

  friend class nsOutlinerBoxObject;

protected:
  nsOutlinerBodyFrame(nsIPresShell* aPresShell);
  virtual ~nsOutlinerBodyFrame();

  // Caches our box object.
  void SetBoxObject(nsIOutlinerBoxObject* aBoxObject) { mOutlinerBoxObject = aBoxObject; };

  // Returns the height of rows in the tree.
  PRInt32 GetRowHeight();

  // Returns our indentation width.
  PRInt32 GetIndentation();

  // Returns our width/height once border and padding have been removed.
  nsRect GetInnerBox();

  // Looks up a style context in the style cache.  On a cache miss we resolve
  // the pseudo-styles passed in and place them into the cache.
  nsresult GetPseudoStyleContext(nsIPresContext* aPresContext, nsIAtom* aPseudoElement, nsIStyleContext** aResult);

  // Builds our cache of column info.
  void EnsureColumns();

  // Update the curpos of the scrollbar.
  void UpdateScrollbar();

  // Use to auto-fill some of the common properties without the view having to do it.
  // Examples include container, open, selected, and focused.
  void PrefillPropertyArray(PRInt32 aRowIndex, const PRUnichar* aColID);

  // Our internal scroll method, used by all the public scroll methods.
  nsresult ScrollInternal(PRInt32 aRow);

protected: // Data Members
  // Our cached pres context.
  nsIPresContext* mPresContext;

  // The cached box object parent.
  nsIOutlinerBoxObject* mOutlinerBoxObject;

  // The current view for this outliner widget.  We get all of our row and cell data
  // from the view.
  nsCOMPtr<nsIOutlinerView> mView;    
  
  // Whether or not we're currently focused.
  PRBool mFocused;

  // A cache of all the style contexts we have seen for rows and cells of the tree.  This is a mapping from
  // a list of atoms to a corresponding style context.  This cache stores every combination that
  // occurs in the tree, so for n distinct properties, this cache could have 2 to the n entries
  // (the power set of all row properties).
  nsOutlinerStyleCache mStyleCache;

  // Cached column information.
  nsOutlinerColumn* mColumns;

  // Our vertical scrollbar.
  nsIFrame* mScrollbar;

  // Our outliner widget.
  nsCOMPtr<nsIWidget> mOutlinerWidget;

  // The index of the first visible row and the # of rows visible onscreen.  
  // The outliner only examines onscreen rows, starting from
  // this index and going up to index+pageCount.
  PRInt32 mTopRowIndex;
  PRInt32 mPageCount;

  // Cached heights.and indent info.
  nsRect mInnerBox;
  PRInt32 mRowHeight;
  PRInt32 mIndentation;

  // A scratch array used when looking up cached style contexts.
  nsCOMPtr<nsISupportsArray> mScratchArray;

}; // class nsOutlinerBodyFrame
