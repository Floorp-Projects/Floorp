/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * Original Author: David W. Hyatt (hyatt@netscape.com)
 *
 * Contributor(s):
 *  Dean Tessman <dean_tessman@hotmail.com>
 *  Brian Ryner <bryner@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nscore.h"
#include "nsLeafBoxFrame.h"
#include "nsITreeBoxObject.h"
#include "nsITreeView.h"
#include "nsICSSPseudoComparator.h"
#include "nsIScrollbarMediator.h"
#include "nsIRenderingContext.h"
#include "nsIDragSession.h"
#include "nsIWidget.h"
#include "nsHashtable.h"
#include "nsITimer.h"
#include "nsIReflowCallback.h"

#include "imgIDecoderObserver.h"

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

class nsTreeStyleCache 
{
public:
  nsTreeStyleCache() :mTransitionTable(nsnull), mCache(nsnull), mNextState(0) {};
  virtual ~nsTreeStyleCache() { Clear(); };

  void Clear() { delete mTransitionTable; mTransitionTable = nsnull; delete mCache; mCache = nsnull; mNextState = 0; };

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
class nsTreeColumn {
public:
  nsTreeColumn(nsIContent* aColElement, nsIFrame* aFrame);
  virtual ~nsTreeColumn() { delete mNext; };

  void SetNext(nsTreeColumn* aNext) { mNext = aNext; };
  nsTreeColumn* GetNext() { return mNext; };

  nsIContent* GetElement() { return mColElement; };

  nscoord GetWidth();
  const nsAFlatString& GetID() { return mID; };

  void GetIDAtom(nsIAtom** aResult) { *aResult = mIDAtom; NS_IF_ADDREF(*aResult); };

  PRBool IsPrimary() { return mIsPrimaryCol; };
  PRBool IsCycler() { return mIsCyclerCol; };

  enum Type {
    eText = 0,
    eCheckbox = 1,
    eProgressMeter = 2
  };
  Type GetType() { return mType; };

  PRInt32 GetCropStyle() { return mCropStyle; };
  PRInt32 GetTextAlignment() { return mTextAlignment; };

  PRInt32 GetColIndex() { return mColIndex; };

private:
  nsTreeColumn* mNext;

  nsString mID;
  nsCOMPtr<nsIAtom> mIDAtom;

  PRUint32 mCropStyle;
  PRUint32 mTextAlignment;
  
  PRPackedBool mIsPrimaryCol;
  PRPackedBool mIsCyclerCol;
  Type mType;

  nsIFrame* mColFrame;
  nsIContent* mColElement;

  PRInt32 mColIndex;
};

// The interface for our image listener.
// {90586540-2D50-403e-8DCE-981CAA778444}
#define NS_ITREEIMAGELISTENER_IID \
{ 0x90586540, 0x2d50, 0x403e, { 0x8d, 0xce, 0x98, 0x1c, 0xaa, 0x77, 0x84, 0x44 } }

class nsITreeImageListener : public nsISupports
{
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ITREEIMAGELISTENER_IID)

public:
  NS_IMETHOD AddRow(int aIndex)=0;
  NS_IMETHOD Invalidate()=0;
};

// This class handles image load observation.
class nsTreeImageListener : public imgIDecoderObserver, public nsITreeImageListener
{
public:
  nsTreeImageListener(nsITreeBoxObject* aTree, const PRUnichar* aColID);
  virtual ~nsTreeImageListener();

  NS_DECL_ISUPPORTS
  NS_DECL_IMGIDECODEROBSERVER
  NS_DECL_IMGICONTAINEROBSERVER

  NS_IMETHOD AddRow(int aIndex);
  NS_IMETHOD Invalidate();

private:
  int mMin;
  int mMax;
  nsString mColID;
  nsITreeBoxObject* mTree;
};

// The actual frame that paints the cells and rows.
class nsTreeBodyFrame : public nsLeafBoxFrame, public nsITreeBoxObject, public nsICSSPseudoComparator,
                        public nsIScrollbarMediator,
                        public nsIReflowCallback
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSITREEBOXOBJECT

  // nsIBox
  NS_IMETHOD GetPrefSize(nsBoxLayoutState& aBoxLayoutState, nsSize& aSize);
  NS_IMETHOD SetBounds(nsBoxLayoutState& aBoxLayoutState, const nsRect& aRect);

  // nsIReflowCallback
  NS_IMETHOD ReflowFinished(nsIPresShell* aPresShell, PRBool* aFlushFlag);

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

  // Painting methods.
  // Paint is the generic nsIFrame paint method.  We override this method
  // to paint our contents (our rows and cells).
  NS_IMETHOD Paint(nsIPresContext*      aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect&        aDirtyRect,
                   nsFramePaintLayer    aWhichLayer,
                   PRUint32             aFlags = 0);

  // This method paints a specific column background of the tree.
  nsresult PaintColumn(nsTreeColumn*        aColumn,
                       const nsRect&        aColumnRect,
                       nsIPresContext*      aPresContext,
                       nsIRenderingContext& aRenderingContext,
                       const nsRect&        aDirtyRect,
                       nsFramePaintLayer    aWhichLayer);

  // This method paints a single row in the tree.
  nsresult PaintRow(PRInt32              aRowIndex,
                    const nsRect&        aRowRect,
                    nsIPresContext*      aPresContext,
                    nsIRenderingContext& aRenderingContext,
                    const nsRect&        aDirtyRect,
                    nsFramePaintLayer    aWhichLayer);

  // This method paints a specific cell in a given row of the tree.
  nsresult PaintCell(PRInt32              aRowIndex, 
                     nsTreeColumn*        aColumn,
                     const nsRect&        aCellRect,
                     nsIPresContext*      aPresContext,
                     nsIRenderingContext& aRenderingContext,
                     const nsRect&        aDirtyRect,
                     nsFramePaintLayer    aWhichLayer);

  // This method paints the twisty inside a cell in the primary column of an tree.
  nsresult PaintTwisty(PRInt32              aRowIndex,
                       nsTreeColumn*        aColumn,
                       const nsRect&        aTwistyRect,
                       nsIPresContext*      aPresContext,
                       nsIRenderingContext& aRenderingContext,
                       const nsRect&        aDirtyRect,
                       nsFramePaintLayer    aWhichLayer,
                       nscoord&             aRemainingWidth,
                       nscoord&             aCurrX);

  // This method paints the image inside the cell of an tree.
  nsresult PaintImage(PRInt32              aRowIndex,
                      nsTreeColumn*        aColumn,
                      const nsRect&        aImageRect,
                      nsIPresContext*      aPresContext,
                      nsIRenderingContext& aRenderingContext,
                      const nsRect&        aDirtyRect,
                      nsFramePaintLayer    aWhichLayer,
                      nscoord&             aRemainingWidth,
                      nscoord&             aCurrX);

  // This method paints the text string inside a particular cell of the tree.
  nsresult PaintText(PRInt32              aRowIndex, 
                     nsTreeColumn*        aColumn,
                     const nsRect&        aTextRect,
                     nsIPresContext*      aPresContext,
                     nsIRenderingContext& aRenderingContext,
                     const nsRect&        aDirtyRect,
                     nsFramePaintLayer    aWhichLayer);

  // This method paints the checkbox inside a particular cell of the tree.
  nsresult PaintCheckbox(PRInt32              aRowIndex, 
                         nsTreeColumn*        aColumn,
                         const nsRect&        aCheckboxRect,
                         nsIPresContext*      aPresContext,
                         nsIRenderingContext& aRenderingContext,
                         const nsRect&        aDirtyRect,
                         nsFramePaintLayer    aWhichLayer);

  // This method paints the progress meter inside a particular cell of the tree.
  nsresult PaintProgressMeter(PRInt32              aRowIndex, 
                              nsTreeColumn*        aColumn,
                              const nsRect&        aProgressMeterRect,
                              nsIPresContext*      aPresContext,
                              nsIRenderingContext& aRenderingContext,
                              const nsRect&        aDirtyRect,
                              nsFramePaintLayer    aWhichLayer);

  // This method paints a drop feedback of the tree.
  nsresult PaintDropFeedback(nsIPresContext*      aPresContext,
                             nsIRenderingContext& aRenderingContext,
                             const nsRect&        aDirtyRect,
                             nsFramePaintLayer    aWhichLayer);

  // This method is called with a specific style context and rect to
  // paint the background rect as if it were a full-blown frame.
  nsresult PaintBackgroundLayer(nsIStyleContext*     aStyleContext,
                                nsIPresContext*      aPresContext, 
                                nsIRenderingContext& aRenderingContext, 
                                const nsRect&        aRect,
                                const nsRect&        aDirtyRect);

  // This method is called whenever an treecol is added or removed and
  // the column cache needs to be rebuilt.
  void InvalidateColumnCache();

  friend nsresult NS_NewTreeBodyFrame(nsIPresShell* aPresShell, 
                                          nsIFrame** aNewFrame);

  friend class nsTreeBoxObject;

protected:
  nsTreeBodyFrame(nsIPresShell* aPresShell);
  virtual ~nsTreeBodyFrame();

  // Caches our box object.
  void SetBoxObject(nsITreeBoxObject* aBoxObject) { mTreeBoxObject = aBoxObject; };

  // A helper used when hit testing.
  nsresult GetItemWithinCellAt(PRInt32 aX, const nsRect& aCellRect, PRInt32 aRowIndex,
                               nsTreeColumn* aColumn, PRUnichar** aChildElt);

  // Fetch an image from the image cache.
  nsresult GetImage(PRInt32 aRowIndex, const PRUnichar* aColID, PRBool aUseContext,
                    nsIStyleContext* aStyleContext, PRBool& aAllowImageRegions, imgIContainer** aResult);

  // Returns the size of a given image.   This size *includes* border and
  // padding.  It does not include margins.
  nsRect GetImageSize(PRInt32 aRowIndex, const PRUnichar* aColID, PRBool aUseContext, nsIStyleContext* aStyleContext);

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

  // Makes |mScrollbar| non-null if at all possible, and returns it.
  nsIFrame* EnsureScrollbar();

  // Update the curpos of the scrollbar.
  void UpdateScrollbar();

  // Check vertical overflow.
  nsresult CheckVerticalOverflow();

  // Use to auto-fill some of the common properties without the view having to do it.
  // Examples include container, open, selected, and focus.
  void PrefillPropertyArray(PRInt32 aRowIndex, nsTreeColumn* aCol);

  // Our internal scroll method, used by all the public scroll methods.
  nsresult ScrollInternal(PRInt32 aRow);
  
  // Convert pixels, probably from an event, into twips in our coordinate space.
  void AdjustEventCoordsToBoxCoordSpace (PRInt32 aX, PRInt32 aY, PRInt32* aResultX, PRInt32* aResultY);

  // Convert a border style into line style.
  nsLineStyle ConvertBorderStyleToLineStyle(PRUint8 aBorderStyle);

  // Cache the box object
  void EnsureBoxObject();

  void EnsureView();

  // Get the base element, <tree> or <select>
  nsresult GetBaseElement(nsIContent** aElement);

  void GetCellWidth(PRInt32 aRow, const nsAString& aColID,
                    nsIRenderingContext* aRenderingContext,
                    nscoord& aDesiredSize, nscoord& aCurrentSize);
  nscoord CalcMaxRowWidth(nsBoxLayoutState& aState);

  PRBool CanAutoScroll(PRInt32 aRowIndex);

  // Calc the row and above/below/on status given where the mouse currently is hovering.
  // Also calc if we're in the region in which we want to auto-scroll the tree.
  // A positive value of |aScrollLines| means scroll down, a negative value
  // means scroll up, a zero value means that we aren't in drag scroll region.
  void ComputeDropPosition(nsIDOMEvent* aEvent, PRInt32* aRow, PRInt16* aOrient,
                           PRInt16* aScrollLines);

  // Mark ourselves dirty if we're a select widget
  void MarkDirtyIfSelect();

  static void OpenCallback(nsITimer *aTimer, void *aClosure);

  static void ScrollCallback(nsITimer *aTimer, void *aClosure);

protected: // Data Members
  // Our cached pres context.
  nsIPresContext* mPresContext;

  // The cached box object parent.
  nsCOMPtr<nsITreeBoxObject> mTreeBoxObject;

  // The current view for this tree widget.  We get all of our row and cell data
  // from the view.
  nsCOMPtr<nsITreeView> mView;    
  
  // A cache of all the style contexts we have seen for rows and cells of the tree.  This is a mapping from
  // a list of atoms to a corresponding style context.  This cache stores every combination that
  // occurs in the tree, so for n distinct properties, this cache could have 2 to the n entries
  // (the power set of all row properties).
  nsTreeStyleCache mStyleCache;

  // A hashtable that maps from URLs to image requests.  The URL is provided
  // by the view or by the style context. The style context represents
  // a resolved :-moz-tree-cell-image (or twisty) pseudo-element.
  // It maps directly to an imgIRequest.
  nsSupportsHashtable* mImageCache;

  // Cached column information.
  nsTreeColumn* mColumns;

  // Our vertical scrollbar.
  nsIFrame* mScrollbar;

  // Our tree widget.
  nsCOMPtr<nsIWidget> mTreeWidget;

  // The index of the first visible row and the # of rows visible onscreen.  
  // The tree only examines onscreen rows, starting from
  // this index and going up to index+pageCount.
  PRInt32 mTopRowIndex;
  PRInt32 mPageCount;

  // Cached heights and indent info.
  nsRect mInnerBox;
  PRInt32 mRowHeight;
  PRInt32 mIndentation;
  nscoord mStringWidth;

  // A scratch array used when looking up cached style contexts.
  nsCOMPtr<nsISupportsArray> mScratchArray;

  // Whether or not we're currently focused.
  PRPackedBool mFocused;

  // An indicator that columns have changed and need to be rebuilt
  PRPackedBool mColumnsDirty;

  // If the drop is actually allowed here or not.
  PRPackedBool mDropAllowed;

  // Do we have a fixed number of onscreen rows?
  PRPackedBool mHasFixedRowCount;

  PRPackedBool mVerticalOverflow;

  // A guard that prevents us from recursive painting.
  PRPackedBool mImageGuard;

  PRPackedBool mReflowCallbackPosted;

  // The row the mouse is hovering over during a drop.
  PRInt32 mDropRow;

  // Where we want to draw feedback (above/on this row/below) if allowed.
  PRInt16 mDropOrient;

  // Number of lines to be scrolled.
  PRInt16 mScrollLines;

  nsCOMPtr<nsIDragSession> mDragSession;

  // Timer for opening spring-loaded folders or scrolling the tree.
  nsCOMPtr<nsITimer> mTimer;

}; // class nsTreeBodyFrame
