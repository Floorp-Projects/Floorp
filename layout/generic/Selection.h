/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_Selection_h__
#define mozilla_Selection_h__

#include "nsIWeakReference.h"

#include "mozilla/AutoRestore.h"
#include "mozilla/TextRange.h"
#include "nsISelection.h"
#include "nsISelectionController.h"
#include "nsISelectionPrivate.h"
#include "nsRange.h"
#include "nsThreadUtils.h"
#include "nsWrapperCache.h"

struct CachedOffsetForFrame;
class nsAutoScrollTimer;
class nsIContentIterator;
class nsIFrame;
class nsFrameSelection;
struct SelectionDetails;

namespace mozilla {
class ErrorResult;
struct AutoPrepareFocusRange;
} // namespace mozilla

struct RangeData
{
  explicit RangeData(nsRange* aRange)
    : mRange(aRange)
  {}

  nsRefPtr<nsRange> mRange;
  mozilla::TextRangeStyle mTextRangeStyle;
};

// Note, the ownership of mozilla::dom::Selection depends on which way the
// object is created. When nsFrameSelection has created Selection,
// addreffing/releasing the Selection object is aggregated to nsFrameSelection.
// Otherwise normal addref/release is used.  This ensures that nsFrameSelection
// is never deleted before its Selections.
namespace mozilla {
namespace dom {

class Selection final : public nsISelectionPrivate,
                        public nsWrapperCache,
                        public nsSupportsWeakReference
{
protected:
  virtual ~Selection();

public:
  Selection();
  explicit Selection(nsFrameSelection *aList);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_AMBIGUOUS(Selection, nsISelectionPrivate)
  NS_DECL_NSISELECTION
  NS_DECL_NSISELECTIONPRIVATE

  nsIDocument* GetParentObject() const;

  // utility methods for scrolling the selection into view
  nsPresContext* GetPresContext() const;
  nsIPresShell* GetPresShell() const;
  nsFrameSelection* GetFrameSelection() const { return mFrameSelection; }
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
                                        int32_t aFlags,
                                        nsIPresShell::ScrollAxis aVertical,
                                        nsIPresShell::ScrollAxis aHorizontal);
  enum {
    SCROLL_SYNCHRONOUS = 1<<1,
    SCROLL_FIRST_ANCESTOR_ONLY = 1<<2,
    SCROLL_DO_FLUSH = 1<<3,
    SCROLL_OVERFLOW_HIDDEN = 1<<5
  };
  // aDoFlush only matters if aIsSynchronous is true.  If not, we'll just flush
  // when the scroll event fires so we make sure to scroll to the right place.
  nsresult      ScrollIntoView(SelectionRegion aRegion,
                               nsIPresShell::ScrollAxis aVertical =
                                 nsIPresShell::ScrollAxis(),
                               nsIPresShell::ScrollAxis aHorizontal =
                                 nsIPresShell::ScrollAxis(),
                               int32_t aFlags = 0);
  nsresult      SubtractRange(RangeData* aRange, nsRange* aSubtract,
                              nsTArray<RangeData>* aOutput);
  /**
   * AddItem adds aRange to this Selection.  If mApplyUserSelectStyle is true,
   * then aRange is first scanned for -moz-user-select:none nodes and split up
   * into multiple ranges to exclude those before adding the resulting ranges
   * to this Selection.
   */
  nsresult      AddItem(nsRange* aRange, int32_t* aOutIndex, bool aNoStartSelect = false);
  nsresult      RemoveItem(nsRange* aRange);
  nsresult      RemoveCollapsedRanges();
  nsresult      Clear(nsPresContext* aPresContext);
  nsresult      Collapse(nsINode* aParentNode, int32_t aOffset);
  nsresult      Extend(nsINode* aParentNode, int32_t aOffset);
  nsRange*      GetRangeAt(int32_t aIndex);

  // Get the anchor-to-focus range if we don't care which end is
  // anchor and which end is focus.
  const nsRange* GetAnchorFocusRange() const {
    return mAnchorFocusRange;
  }

  nsDirection  GetDirection(){return mDirection;}
  void         SetDirection(nsDirection aDir){mDirection = aDir;}
  nsresult     SetAnchorFocusToRange(nsRange *aRange);
  void         ReplaceAnchorFocusRange(nsRange *aRange);
  void         AdjustAnchorFocusForMultiRange(nsDirection aDirection);

  //  NS_IMETHOD   GetPrimaryFrameForRangeEndpoint(nsIDOMNode *aNode, int32_t aOffset, bool aIsEndNode, nsIFrame **aResultFrame);
  NS_IMETHOD   GetPrimaryFrameForAnchorNode(nsIFrame **aResultFrame);
  NS_IMETHOD   GetPrimaryFrameForFocusNode(nsIFrame **aResultFrame, int32_t *aOffset, bool aVisual);
  NS_IMETHOD   LookUpSelection(nsIContent *aContent, int32_t aContentOffset, int32_t aContentLength,
                             SelectionDetails **aReturnDetails, SelectionType aType, bool aSlowCheck);
  NS_IMETHOD   Repaint(nsPresContext* aPresContext);

  // Note: StartAutoScrollTimer might destroy arbitrary frames etc.
  nsresult     StartAutoScrollTimer(nsIFrame *aFrame,
                                    nsPoint& aPoint,
                                    uint32_t aDelay);

  nsresult     StopAutoScrollTimer();

  JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL methods
  nsINode*     GetAnchorNode();
  uint32_t     AnchorOffset();
  nsINode*     GetFocusNode();
  uint32_t     FocusOffset();

  bool IsCollapsed();
  void Collapse(nsINode& aNode, uint32_t aOffset, mozilla::ErrorResult& aRv);
  void CollapseToStart(mozilla::ErrorResult& aRv);
  void CollapseToEnd(mozilla::ErrorResult& aRv);

  void Extend(nsINode& aNode, uint32_t aOffset, mozilla::ErrorResult& aRv);

  void SelectAllChildren(nsINode& aNode, mozilla::ErrorResult& aRv);
  void DeleteFromDocument(mozilla::ErrorResult& aRv);

  uint32_t RangeCount() const
  {
    return mRanges.Length();
  }
  nsRange* GetRangeAt(uint32_t aIndex, mozilla::ErrorResult& aRv);
  void AddRange(nsRange& aRange, mozilla::ErrorResult& aRv);
  void RemoveRange(nsRange& aRange, mozilla::ErrorResult& aRv);
  void RemoveAllRanges(mozilla::ErrorResult& aRv);

  void Stringify(nsAString& aResult);

  bool ContainsNode(nsINode& aNode, bool aPartlyContained, mozilla::ErrorResult& aRv);

  void Modify(const nsAString& aAlter, const nsAString& aDirection,
              const nsAString& aGranularity, mozilla::ErrorResult& aRv);

  bool GetInterlinePosition(mozilla::ErrorResult& aRv);
  void SetInterlinePosition(bool aValue, mozilla::ErrorResult& aRv);

  Nullable<int16_t> GetCaretBidiLevel(mozilla::ErrorResult& aRv) const;
  void SetCaretBidiLevel(const Nullable<int16_t>& aCaretBidiLevel, mozilla::ErrorResult& aRv);

  void ToStringWithFormat(const nsAString& aFormatType,
                          uint32_t aFlags,
                          int32_t aWrapColumn,
                          nsAString& aReturn,
                          mozilla::ErrorResult& aRv);
  void AddSelectionListener(nsISelectionListener* aListener,
                            mozilla::ErrorResult& aRv);
  void RemoveSelectionListener(nsISelectionListener* aListener,
                               mozilla::ErrorResult& aRv);

  int16_t Type() const { return mType; }

  void GetRangesForInterval(nsINode& aBeginNode, int32_t aBeginOffset,
                            nsINode& aEndNode, int32_t aEndOffset,
                            bool aAllowAdjacent,
                            nsTArray<nsRefPtr<nsRange>>& aReturn,
                            mozilla::ErrorResult& aRv);

  void ScrollIntoView(int16_t aRegion, bool aIsSynchronous,
                      int16_t aVPercent, int16_t aHPercent,
                      mozilla::ErrorResult& aRv);

  void AddSelectionChangeBlocker();
  void RemoveSelectionChangeBlocker();
  bool IsBlockingSelectionChangeEvents() const;
private:
  friend class ::nsAutoScrollTimer;

  // Note: DoAutoScroll might destroy arbitrary frames etc.
  nsresult DoAutoScroll(nsIFrame *aFrame, nsPoint& aPoint);

public:
  SelectionType GetType(){return mType;}
  void          SetType(SelectionType aType){mType = aType;}

  nsresult     NotifySelectionListeners();

  friend struct AutoApplyUserSelectStyle;
  struct MOZ_RAII AutoApplyUserSelectStyle
  {
    explicit AutoApplyUserSelectStyle(Selection* aSelection
                             MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : mSavedValue(aSelection->mApplyUserSelectStyle)
    {
      MOZ_GUARD_OBJECT_NOTIFIER_INIT;
      aSelection->mApplyUserSelectStyle = true;
    }
    AutoRestore<bool> mSavedValue;
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
  };

private:
  friend struct mozilla::AutoPrepareFocusRange;
  class ScrollSelectionIntoViewEvent;
  friend class ScrollSelectionIntoViewEvent;

  class ScrollSelectionIntoViewEvent : public nsRunnable {
  public:
    NS_DECL_NSIRUNNABLE
    ScrollSelectionIntoViewEvent(Selection* aSelection,
                                 SelectionRegion aRegion,
                                 nsIPresShell::ScrollAxis aVertical,
                                 nsIPresShell::ScrollAxis aHorizontal,
                                 int32_t aFlags)
      : mSelection(aSelection),
        mRegion(aRegion),
        mVerticalScroll(aVertical),
        mHorizontalScroll(aHorizontal),
        mFlags(aFlags) {
      NS_ASSERTION(aSelection, "null parameter");
    }
    void Revoke() { mSelection = nullptr; }
  private:
    Selection *mSelection;
    SelectionRegion mRegion;
    nsIPresShell::ScrollAxis mVerticalScroll;
    nsIPresShell::ScrollAxis mHorizontalScroll;
    int32_t mFlags;
  };

  void setAnchorFocusRange(int32_t aIndex); // pass in index into mRanges;
                                            // negative value clears
                                            // mAnchorFocusRange
  nsresult     SelectAllFramesForContent(nsIContentIterator *aInnerIter,
                               nsIContent *aContent,
                               bool aSelected);
  nsresult     selectFrames(nsPresContext* aPresContext, nsRange *aRange, bool aSelect);
  nsresult     getTableCellLocationFromRange(nsRange *aRange, int32_t *aSelectionType, int32_t *aRow, int32_t *aCol);
  nsresult     addTableCellRange(nsRange *aRange, bool *aDidAddRange, int32_t *aOutIndex);

  nsresult FindInsertionPoint(
      nsTArray<RangeData>* aElementArray,
      nsINode* aPointNode, int32_t aPointOffset,
      nsresult (*aComparator)(nsINode*,int32_t,nsRange*,int32_t*),
      int32_t* aPoint);
  bool EqualsRangeAtPoint(nsINode* aBeginNode, int32_t aBeginOffset,
                            nsINode* aEndNode, int32_t aEndOffset,
                            int32_t aRangeIndex);
  nsresult GetIndicesForInterval(nsINode* aBeginNode, int32_t aBeginOffset,
                                 nsINode* aEndNode, int32_t aEndOffset,
                                 bool aAllowAdjacent,
                                 int32_t* aStartIndex, int32_t* aEndIndex);
  RangeData* FindRangeData(nsIDOMRange* aRange);

  void UserSelectRangesToAdd(nsRange* aItem, nsTArray<nsRefPtr<nsRange> >& rangesToAdd);

  /**
   * Helper method for AddItem.
   */
  nsresult AddItemInternal(nsRange* aRange, int32_t* aOutIndex);

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
  nsRefPtr<nsAutoScrollTimer> mAutoScrollTimer;
  nsCOMArray<nsISelectionListener> mSelectionListeners;
  nsRevocableEventPtr<ScrollSelectionIntoViewEvent> mScrollEvent;
  CachedOffsetForFrame *mCachedOffsetForFrame;
  nsDirection mDirection;
  SelectionType mType;
  /**
   * True if the current selection operation was initiated by user action.
   * It determines whether we exclude -moz-user-select:none nodes or not,
   * as well as whether selectstart events will be fired.
   */
  bool mApplyUserSelectStyle;

  // Non-zero if we don't want any changes we make to the selection to be
  // visible to content. If non-zero, content won't be notified about changes.
  uint32_t mSelectionChangeBlockerCount;
};

// Stack-class to turn on/off selection batching.
class MOZ_STACK_CLASS SelectionBatcher final
{
private:
  nsRefPtr<Selection> mSelection;
public:
  explicit SelectionBatcher(Selection* aSelection)
  {
    mSelection = aSelection;
    if (mSelection) {
      mSelection->StartBatchChanges();
    }
  }

  ~SelectionBatcher()
  {
    if (mSelection) {
      mSelection->EndBatchChanges();
    }
  }
};

class MOZ_STACK_CLASS AutoHideSelectionChanges final
{
private:
  nsRefPtr<Selection> mSelection;
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
public:
  explicit AutoHideSelectionChanges(const nsFrameSelection* aFrame);

  explicit AutoHideSelectionChanges(Selection* aSelection
                                    MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
    : mSelection(aSelection)
  {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    mSelection = aSelection;
    if (mSelection) {
      mSelection->AddSelectionChangeBlocker();
    }
  }

  ~AutoHideSelectionChanges()
  {
    if (mSelection) {
      mSelection->RemoveSelectionChangeBlocker();
    }
  }
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_Selection_h__
