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

#include "nscore.h"
#include "nsLeafBoxFrame.h"
#include "nsIOutlinerBoxObject.h"
#include "nsIOutlinerView.h"
#include "nsICSSPseudoComparator.h"
#include "nsIScrollbarMediator.h"
#include "nsIRenderingContext.h"
#include "nsIDragSession.h"
#include "nsIWidget.h"
#include "nsHashtable.h"

#ifdef USE_IMG2
#include "imgIDecoderObserver.h"
#endif

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
    PRInt32 lb = (NS_PTR_TO_INT32(mInputSymbol.get()) << 16) >> 16;
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

  static PRBool PR_CALLBACK DeleteDFAState(nsHashKey *aKey, void *aData, void *closure);

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
  nsObjectHashtable* mTransitionTable;

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
  nsCOMPtr<nsIAtom> mIDAtom;

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
  const PRUnichar* GetID() { return mID.get(); };
  void GetID(nsString& aID) { aID = mID; };

  void GetIDAtom(nsIAtom** aResult) { *aResult = mIDAtom; NS_IF_ADDREF(*aResult); };

  PRBool IsPrimary() { return mIsPrimaryCol; };
  PRBool IsCycler() { return mIsCyclerCol; };

  PRInt32 GetCropStyle() { return mCropStyle; };
};

#ifdef USE_IMG2
// The interface for our image listener.
// {90586540-2D50-403e-8DCE-981CAA778444}
#define NS_IOUTLINERIMAGELISTENER_IID \
{ 0x90586540, 0x2d50, 0x403e, { 0x8d, 0xce, 0x98, 0x1c, 0xaa, 0x77, 0x84, 0x44 } }

class nsIOutlinerImageListener : public nsISupports
{
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IOUTLINERIMAGELISTENER_IID; return iid; }

public:
  NS_IMETHOD AddRow(int aIndex)=0;
  NS_IMETHOD Invalidate()=0;
};

// This class handles image load observation.
class nsOutlinerImageListener : public imgIDecoderObserver, public nsIOutlinerImageListener
{
public:
  nsOutlinerImageListener(nsIOutlinerBoxObject* aOutliner, const PRUnichar* aColID);
  virtual ~nsOutlinerImageListener();

  NS_DECL_ISUPPORTS
  NS_DECL_IMGIDECODEROBSERVER
  NS_DECL_IMGICONTAINEROBSERVER

  NS_IMETHOD AddRow(int aIndex);
  NS_IMETHOD Invalidate();

private:
  int mMin;
  int mMax;
  nsString mColID;
  nsIOutlinerBoxObject* mOutliner;
};
#endif

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
                   nsFramePaintLayer    aWhichLayer,
                   PRUint32             aFlags = 0);

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

  // This method paints the twisty inside a cell in the primary column of an outliner.
  NS_IMETHOD PaintTwisty(int                  aRowIndex,
                         nsOutlinerColumn*    aColumn,
                         const nsRect&        aTwistyRect,
                         nsIPresContext*      aPresContext,
                         nsIRenderingContext& aRenderingContext,
                         const nsRect&        aDirtyRect,
                         nsFramePaintLayer    aWhichLayer,
                         nscoord&             aRemainingWidth,
                         nscoord&             aCurrX);

  // This method paints the image inside the cell of an outliner.
  NS_IMETHOD PaintImage(int                  aRowIndex,
                        nsOutlinerColumn*    aColumn,
                        const nsRect&        aImageRect,
                        nsIPresContext*      aPresContext,
                        nsIRenderingContext& aRenderingContext,
                        const nsRect&        aDirtyRect,
                        nsFramePaintLayer    aWhichLayer,
                        nscoord&             aRemainingWidth,
                        nscoord&             aCurrX);

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

  // This method is called whenever an outlinercol is added or removed and
  // the column cache needs to be rebuilt.
  void InvalidateColumnCache();
                                  
  friend nsresult NS_NewOutlinerBodyFrame(nsIPresShell* aPresShell, 
                                          nsIFrame** aNewFrame);

  friend class nsOutlinerBoxObject;

protected:
  nsOutlinerBodyFrame(nsIPresShell* aPresShell);
  virtual ~nsOutlinerBodyFrame();

  // Caches our box object.
  void SetBoxObject(nsIOutlinerBoxObject* aBoxObject) { mOutlinerBoxObject = aBoxObject; };

  // A helper used when hit testing.
  nsresult GetItemWithinCellAt(PRInt32 aX, const nsRect& aCellRect, PRInt32 aRowIndex,
                               nsOutlinerColumn* aColumn, PRUnichar** aChildElt);

#ifdef USE_IMG2
  // Fetch an image from the image cache.
  nsresult GetImage(PRInt32 aRowIndex, const PRUnichar* aColID, 
                    nsIStyleContext* aStyleContext, imgIContainer** aResult);
#endif

  // Returns the size of a given image.   This size *includes* border and
  // padding.  It does not include margins.
  nsRect GetImageSize(PRInt32 aRowIndex, const PRUnichar* aColID, nsIStyleContext* aStyleContext);

  // Returns the height of rows in the tree.
  PRInt32 GetRowHeight();

  // Returns our indentation width.
  PRInt32 GetIndentation();

  // Returns our width/height once border and padding have been removed.
  nsRect GetInnerBox();

  // Looks up a style context in the style cache.  On a cache miss we resolve
  // the pseudo-styles passed in and place them into the cache.
  nsresult GetPseudoStyleContext(nsIAtom* aPseudoElement, nsIStyleContext** aResult);

  // Builds our cache of column info.
  void EnsureColumns();

  // Update the curpos of the scrollbar.
  void UpdateScrollbar();

  // Update the visibility of the scrollbar.
  nsresult SetVisibleScrollbar(PRBool aSetVisible);

  // Use to auto-fill some of the common properties without the view having to do it.
  // Examples include container, open, selected, and focus.
  void PrefillPropertyArray(PRInt32 aRowIndex, nsOutlinerColumn* aCol);

  // Our internal scroll method, used by all the public scroll methods.
  nsresult ScrollInternal(PRInt32 aRow);
  
  // convert pixels, probably from an event, into twips in our coordinate space
  void AdjustEventCoordsToBoxCoordSpace ( PRInt32 inX, PRInt32 inY, PRInt32* outX, PRInt32* outY ) ;

protected: // Data Members
  // Our cached pres context.
  nsIPresContext* mPresContext;

  // The cached box object parent.
  nsCOMPtr<nsIOutlinerBoxObject> mOutlinerBoxObject;

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

  // A hashtable that maps from style contexts to image requests.  The style context represents
  // a resolved :-moz-outliner-cell-image (or twisty) pseudo-element.  It maps directly to an
  // imgIRequest.  This allows us to avoid even wasting time checking URLs.
  nsSupportsHashtable* mImageCache;

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

  // Cached heights and indent info.
  nsRect mInnerBox;
  PRInt32 mRowHeight;
  PRInt32 mIndentation;

  // An indicator that columns have changed and need to be rebuilt
  PRBool mColumnsDirty;

  // A scratch array used when looking up cached style contexts.
  nsCOMPtr<nsISupportsArray> mScratchArray;
  
  enum { kIllegalRow = -1 } ;
  enum { kDrawFeedback = PR_TRUE, kUndrawFeedback = PR_FALSE } ;
  enum DropOrientation { kNoOrientation, kBeforeRow = 1, kOnRow = 2, kAfterRow = 3 } ;
  
    // draw (or undraw) feedback at the given location with the given orientation
  void DrawDropFeedback ( PRInt32 inDropRow, DropOrientation inDropOrient, PRBool inDrawFeedback ) ;
  
    // calc the row and above/below/on status given where the mouse currently is hovering
  void ComputeDropPosition ( nsIDOMEvent* inEvent, PRInt32* outRow, DropOrientation* outOrient ) ;

    // calculate if we're in the region in which we want to auto-scroll the outliner
  PRBool IsInDragScrollRegion ( nsIDOMEvent* inEvent, PRBool* outScrollUp ) ;
  
  PRInt32 mDropRow;               // the row the mouse is hovering over during a drop
  DropOrientation mDropOrient;    // where we want to draw feedback (above/below/on this row) if allowed
  PRBool mDropAllowed;            // if the drop is actually allowed here or not. we draw if this is true
  PRBool mIsSortRectDrawn;        // have we already drawn the sort rectangle?
  PRBool mAlreadyUndrewDueToScroll;   // we undraw early during auto-scroll; did we do this already?
  
  nsCOMPtr<nsIDragSession> mDragSession;
  nsCOMPtr<nsIRenderingContext> mRenderingContext;

}; // class nsOutlinerBodyFrame
