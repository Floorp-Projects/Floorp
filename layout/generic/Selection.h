/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_Selection_h__
#define mozilla_Selection_h__

#include "nsIWeakReference.h"

#include "nsISelection.h"
#include "nsISelectionController.h"
#include "nsISelectionPrivate.h"
#include "nsRange.h"

struct CachedOffsetForFrame;
class nsAutoScrollTimer;
class nsIContentIterator;
class nsIFrame;
struct SelectionDetails;
class nsSelectionIterator;

struct RangeData
{
  RangeData(nsRange* aRange)
    : mRange(aRange)
  {}

  nsRefPtr<nsRange> mRange;
  nsTextRangeStyle mTextRangeStyle;
};

// Note, the ownership of mozilla::Selection depends on which way the object is
// created. When nsFrameSelection has created Selection, addreffing/releasing
// the Selection object is aggregated to nsFrameSelection. Otherwise normal
// addref/release is used.  This ensures that nsFrameSelection is never deleted
// before its Selections.
namespace mozilla {

class Selection : public nsISelectionPrivate,
                  public nsSupportsWeakReference
{
public:
  Selection();
  Selection(nsFrameSelection *aList);
  virtual ~Selection();
  
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(Selection, nsISelectionPrivate)
  NS_DECL_NSISELECTION
  NS_DECL_NSISELECTIONPRIVATE

  // utility methods for scrolling the selection into view
  nsresult      GetPresContext(nsPresContext **aPresContext);
  nsresult      GetPresShell(nsIPresShell **aPresShell);
  // Returns a rect containing the selection region, and frame that that
  // position is relative to. For SELECTION_ANCHOR_REGION or
  // SELECTION_FOCUS_REGION the rect is a zero-width rectangle. For
  // SELECTION_WHOLE_SELECTION the rect contains both the anchor and focus
  // region rects.
  nsIFrame*     GetSelectionAnchorGeometry(SelectionRegion aRegion, nsRect *aRect);
  // Returns the position of the region (SELECTION_ANCHOR_REGION or
  // SELECTION_FOCUS_REGION only), and frame that that position is relative to.
  // The 'position' is a zero-width rectangle.
  nsIFrame*     GetSelectionEndPointGeometry(SelectionRegion aRegion, nsRect *aRect);

  nsresult      PostScrollSelectionIntoViewEvent(
                                        SelectionRegion aRegion,
                                        bool aFirstAncestorOnly,
                                        nsIPresShell::ScrollAxis aVertical,
                                        nsIPresShell::ScrollAxis aHorizontal);
  enum {
    SCROLL_SYNCHRONOUS = 1<<1,
    SCROLL_FIRST_ANCESTOR_ONLY = 1<<2,
    SCROLL_DO_FLUSH = 1<<3
  };
  // aDoFlush only matters if aIsSynchronous is true.  If not, we'll just flush
  // when the scroll event fires so we make sure to scroll to the right place.
  nsresult      ScrollIntoView(SelectionRegion aRegion,
                               nsIPresShell::ScrollAxis aVertical =
                                 nsIPresShell::ScrollAxis(),
                               nsIPresShell::ScrollAxis aHorizontal =
                                 nsIPresShell::ScrollAxis(),
                               PRInt32 aFlags = 0);
  nsresult      SubtractRange(RangeData* aRange, nsRange* aSubtract,
                              nsTArray<RangeData>* aOutput);
  nsresult      AddItem(nsRange *aRange, PRInt32* aOutIndex = nsnull);
  nsresult      RemoveItem(nsRange *aRange);
  nsresult      RemoveCollapsedRanges();
  nsresult      Clear(nsPresContext* aPresContext);
  nsresult      Collapse(nsINode* aParentNode, PRInt32 aOffset);
  nsresult      Extend(nsINode* aParentNode, PRInt32 aOffset);
  nsRange*      GetRangeAt(PRInt32 aIndex);
  PRInt32 GetRangeCount() { return mRanges.Length(); }

  // methods for convenience. Note, these don't addref
  nsINode*     GetAnchorNode();
  PRInt32      GetAnchorOffset();

  nsINode*     GetFocusNode();
  PRInt32      GetFocusOffset();

  bool IsCollapsed();

  // Get the anchor-to-focus range if we don't care which end is
  // anchor and which end is focus.
  const nsRange* GetAnchorFocusRange() const {
    return mAnchorFocusRange;
  }

  nsDirection  GetDirection(){return mDirection;}
  void         SetDirection(nsDirection aDir){mDirection = aDir;}
  nsresult     SetAnchorFocusToRange(nsRange *aRange);
  void         ReplaceAnchorFocusRange(nsRange *aRange);

  //  NS_IMETHOD   GetPrimaryFrameForRangeEndpoint(nsIDOMNode *aNode, PRInt32 aOffset, bool aIsEndNode, nsIFrame **aResultFrame);
  NS_IMETHOD   GetPrimaryFrameForAnchorNode(nsIFrame **aResultFrame);
  NS_IMETHOD   GetPrimaryFrameForFocusNode(nsIFrame **aResultFrame, PRInt32 *aOffset, bool aVisual);
  NS_IMETHOD   LookUpSelection(nsIContent *aContent, PRInt32 aContentOffset, PRInt32 aContentLength,
                             SelectionDetails **aReturnDetails, SelectionType aType, bool aSlowCheck);
  NS_IMETHOD   Repaint(nsPresContext* aPresContext);

  // Note: StartAutoScrollTimer might destroy arbitrary frames etc.
  nsresult     StartAutoScrollTimer(nsIFrame *aFrame,
                                    nsPoint& aPoint,
                                    PRUint32 aDelay);

  nsresult     StopAutoScrollTimer();

private:
  friend class ::nsAutoScrollTimer;

  // Note: DoAutoScroll might destroy arbitrary frames etc.
  nsresult DoAutoScroll(nsIFrame *aFrame, nsPoint& aPoint);

public:
  SelectionType GetType(){return mType;}
  void          SetType(SelectionType aType){mType = aType;}

  nsresult     NotifySelectionListeners();

private:
  friend class ::nsSelectionIterator;

  class ScrollSelectionIntoViewEvent;
  friend class ScrollSelectionIntoViewEvent;

  class ScrollSelectionIntoViewEvent : public nsRunnable {
  public:
    NS_DECL_NSIRUNNABLE
    ScrollSelectionIntoViewEvent(Selection* aSelection,
                                 SelectionRegion aRegion,
                                 nsIPresShell::ScrollAxis aVertical,
                                 nsIPresShell::ScrollAxis aHorizontal,
                                 bool aFirstAncestorOnly)
      : mSelection(aSelection),
        mRegion(aRegion),
        mVerticalScroll(aVertical),
        mHorizontalScroll(aHorizontal),
        mFirstAncestorOnly(aFirstAncestorOnly) {
      NS_ASSERTION(aSelection, "null parameter");
    }
    void Revoke() { mSelection = nsnull; }
  private:
    Selection *mSelection;
    SelectionRegion mRegion;
    nsIPresShell::ScrollAxis mVerticalScroll;
    nsIPresShell::ScrollAxis mHorizontalScroll;
    bool mFirstAncestorOnly;
  };

  void setAnchorFocusRange(PRInt32 aIndex); // pass in index into mRanges;
                                            // negative value clears
                                            // mAnchorFocusRange
  nsresult     SelectAllFramesForContent(nsIContentIterator *aInnerIter,
                               nsIContent *aContent,
                               bool aSelected);
  nsresult     selectFrames(nsPresContext* aPresContext, nsRange *aRange, bool aSelect);
  nsresult     getTableCellLocationFromRange(nsRange *aRange, PRInt32 *aSelectionType, PRInt32 *aRow, PRInt32 *aCol);
  nsresult     addTableCellRange(nsRange *aRange, bool *aDidAddRange, PRInt32 *aOutIndex);

  nsresult FindInsertionPoint(
      nsTArray<RangeData>* aElementArray,
      nsINode* aPointNode, PRInt32 aPointOffset,
      nsresult (*aComparator)(nsINode*,PRInt32,nsRange*,PRInt32*),
      PRInt32* aPoint);
  bool EqualsRangeAtPoint(nsINode* aBeginNode, PRInt32 aBeginOffset,
                            nsINode* aEndNode, PRInt32 aEndOffset,
                            PRInt32 aRangeIndex);
  void GetIndicesForInterval(nsINode* aBeginNode, PRInt32 aBeginOffset,
                             nsINode* aEndNode, PRInt32 aEndOffset,
                             bool aAllowAdjacent,
                             PRInt32 *aStartIndex, PRInt32 *aEndIndex);
  RangeData* FindRangeData(nsIDOMRange* aRange);

  // These are the ranges inside this selection. They are kept sorted in order
  // of DOM start position.
  //
  // This data structure is sorted by the range beginnings. As the ranges are
  // disjoint, it is also implicitly sorted by the range endings. This allows
  // us to perform binary searches when searching for existence of a range,
  // giving us O(log n) search time.
  //
  // Inserting a new range requires finding the overlapping interval, requiring
  // two binary searches plus up to an additional 6 DOM comparisons. If this
  // proves to be a performance concern, then an interval tree may be a
  // possible solution, allowing the calculation of the overlap interval in
  // O(log n) time, though this would require rebalancing and other overhead.
  nsTArray<RangeData> mRanges;

  nsRefPtr<nsRange> mAnchorFocusRange;
  nsRefPtr<nsFrameSelection> mFrameSelection;
  nsWeakPtr mPresShellWeak;
  nsRefPtr<nsAutoScrollTimer> mAutoScrollTimer;
  nsCOMArray<nsISelectionListener> mSelectionListeners;
  nsRevocableEventPtr<ScrollSelectionIntoViewEvent> mScrollEvent;
  CachedOffsetForFrame *mCachedOffsetForFrame;
  nsDirection mDirection;
  SelectionType mType;
};

} // namespace mozilla

#endif // mozilla_Selection_h__
