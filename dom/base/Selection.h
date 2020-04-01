/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_Selection_h__
#define mozilla_Selection_h__

#include "mozilla/dom/StyledRange.h"
#include "mozilla/AutoRestore.h"
#include "mozilla/EventForwards.h"
#include "mozilla/RangeBoundary.h"
#include "mozilla/SelectionChangeEventDispatcher.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/WeakPtr.h"
#include "nsDirection.h"
#include "nsISelectionController.h"
#include "nsISelectionListener.h"
#include "nsRange.h"
#include "nsTArrayForwardDeclare.h"
#include "nsThreadUtils.h"
#include "nsWrapperCache.h"

struct CachedOffsetForFrame;
class nsAutoScrollTimer;
class nsIFrame;
class nsFrameSelection;
class nsPIDOMWindowOuter;
struct SelectionDetails;
struct SelectionCustomColors;
class nsCopySupport;
class nsHTMLCopyEncoder;

namespace mozilla {
class AccessibleCaretEventHub;
class ErrorResult;
class HTMLEditor;
class PostContentIterator;
enum class TableSelectionMode : uint32_t;
struct AutoPrepareFocusRange;
namespace dom {
class DocGroup;
}  // namespace dom
}  // namespace mozilla

namespace mozilla {

namespace dom {

// Note, the ownership of mozilla::dom::Selection depends on which way the
// object is created. When nsFrameSelection has created Selection,
// addreffing/releasing the Selection object is aggregated to nsFrameSelection.
// Otherwise normal addref/release is used.  This ensures that nsFrameSelection
// is never deleted before its Selections.
class Selection final : public nsSupportsWeakReference,
                        public nsWrapperCache,
                        public SupportsWeakPtr<Selection> {
 protected:
  virtual ~Selection();

 public:
  /**
   * @param aFrameSelection can be nullptr.
   */
  explicit Selection(SelectionType aSelectionType,
                     nsFrameSelection* aFrameSelection);

  MOZ_DECLARE_WEAKREFERENCE_TYPENAME(Selection)

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(Selection)

  // match this up with EndbatchChanges. will stop ui updates while multiple
  // selection methods are called
  void StartBatchChanges();

  // match this up with StartBatchChanges
  void EndBatchChanges(int16_t aReason = nsISelectionListener::NO_REASON);

  /**
   * NotifyAutoCopy() starts to notify AutoCopyListener of selection changes.
   */
  void NotifyAutoCopy() {
    MOZ_ASSERT(mSelectionType == SelectionType::eNormal);

    mNotifyAutoCopy = true;
  }

  /**
   * MaybeNotifyAccessibleCaretEventHub() starts to notify
   * AccessibleCaretEventHub of selection change if aPresShell has it.
   */
  void MaybeNotifyAccessibleCaretEventHub(PresShell* aPresShell);

  /**
   * StopNotifyingAccessibleCaretEventHub() stops notifying
   * AccessibleCaretEventHub of selection change.
   */
  void StopNotifyingAccessibleCaretEventHub();

  /**
   * EnableSelectionChangeEvent() starts to notify
   * SelectionChangeEventDispatcher of selection change to dispatch a
   * selectionchange event at every selection change.
   */
  void EnableSelectionChangeEvent() {
    if (!mSelectionChangeEventDispatcher) {
      mSelectionChangeEventDispatcher = new SelectionChangeEventDispatcher();
    }
  }

  // Required for WebIDL bindings, see
  // https://developer.mozilla.org/en-US/docs/Mozilla/WebIDL_bindings#Adding_WebIDL_bindings_to_a_class.
  Document* GetParentObject() const;

  DocGroup* GetDocGroup() const;

  // utility methods for scrolling the selection into view
  nsPresContext* GetPresContext() const;
  PresShell* GetPresShell() const;
  nsFrameSelection* GetFrameSelection() const { return mFrameSelection; }
  // Returns a rect containing the selection region, and frame that that
  // position is relative to. For SELECTION_ANCHOR_REGION or
  // SELECTION_FOCUS_REGION the rect is a zero-width rectangle. For
  // SELECTION_WHOLE_SELECTION the rect contains both the anchor and focus
  // region rects.
  nsIFrame* GetSelectionAnchorGeometry(SelectionRegion aRegion, nsRect* aRect);
  // Returns the position of the region (SELECTION_ANCHOR_REGION or
  // SELECTION_FOCUS_REGION only), and frame that that position is relative to.
  // The 'position' is a zero-width rectangle.
  nsIFrame* GetSelectionEndPointGeometry(SelectionRegion aRegion,
                                         nsRect* aRect);

  nsresult PostScrollSelectionIntoViewEvent(SelectionRegion aRegion,
                                            int32_t aFlags,
                                            ScrollAxis aVertical,
                                            ScrollAxis aHorizontal);
  enum {
    SCROLL_SYNCHRONOUS = 1 << 1,
    SCROLL_FIRST_ANCESTOR_ONLY = 1 << 2,
    SCROLL_DO_FLUSH =
        1 << 3,  // only matters if SCROLL_SYNCHRONOUS is passed too
    SCROLL_OVERFLOW_HIDDEN = 1 << 5,
    SCROLL_FOR_CARET_MOVE = 1 << 6
  };
  // If aFlags doesn't contain SCROLL_SYNCHRONOUS, then we'll flush when
  // the scroll event fires so we make sure to scroll to the right place.
  // Otherwise, if SCROLL_DO_FLUSH is also in aFlags, then this method will
  // flush layout and you MUST hold a strong ref on 'this' for the duration
  // of this call.  This might destroy arbitrary layout objects.
  MOZ_CAN_RUN_SCRIPT nsresult
  ScrollIntoView(SelectionRegion aRegion, ScrollAxis aVertical = ScrollAxis(),
                 ScrollAxis aHorizontal = ScrollAxis(), int32_t aFlags = 0);
  static nsresult SubtractRange(StyledRange& aRange, nsRange& aSubtract,
                                nsTArray<StyledRange>* aOutput);

 private:
  static bool AreUserSelectedRangesNonEmpty(
      const nsRange& aRange, nsTArray<RefPtr<nsRange>>& aTempRangesToAdd);
  /**
   * https://w3c.github.io/selection-api/#selectstart-event.
   */
  enum class DispatchSelectstartEvent {
    No,
    Maybe,
  };

  /**
   * See `AddRangesForSelectableNodes`.
   */
  // TODO: annotate with `MOZ_CAN_RUN_SCRIPT` instead.
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT_BOUNDARY nsresult
  AddRangesForUserSelectableNodes(
      nsRange* aRange, int32_t* aOutIndex,
      const DispatchSelectstartEvent aDispatchSelectstartEvent);

  /**
   * Adds aRange to this Selection.  If mUserInitiated is true,
   * then aRange is first scanned for -moz-user-select:none nodes and split up
   * into multiple ranges to exclude those before adding the resulting ranges
   * to this Selection.
   *
   * @param aOutIndex points to the range last added, if at least one was added.
   *                  If aRange is already contained, it points to the range
   *                  containing it. -1 if mStyledRanges.mRanges was empty and
   * no range was added.
   */
  // TODO: annotate with `MOZ_CAN_RUN_SCRIPT` instead.
  [[nodiscard]] MOZ_CAN_RUN_SCRIPT_BOUNDARY nsresult
  AddRangesForSelectableNodes(
      nsRange* aRange, int32_t* aOutIndex,
      DispatchSelectstartEvent aDispatchSelectstartEvent);

 public:
  nsresult RemoveCollapsedRanges();
  nsresult Clear(nsPresContext* aPresContext);
  nsresult Collapse(nsINode* aContainer, int32_t aOffset) {
    if (!aContainer) {
      return NS_ERROR_INVALID_ARG;
    }
    return Collapse(RawRangeBoundary(aContainer, aOffset));
  }
  nsresult Collapse(const RawRangeBoundary& aPoint) {
    ErrorResult result;
    Collapse(aPoint, result);
    return result.StealNSResult();
  }

  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  nsresult Extend(nsINode* aContainer, int32_t aOffset);

  /**
   * See mStyledRanges.mRanges.
   */
  nsRange* GetRangeAt(int32_t aIndex) const;

  // Get the anchor-to-focus range if we don't care which end is
  // anchor and which end is focus.
  const nsRange* GetAnchorFocusRange() const { return mAnchorFocusRange; }

  nsDirection GetDirection() const { return mDirection; }

  void SetDirection(nsDirection aDir) { mDirection = aDir; }
  nsresult SetAnchorFocusToRange(nsRange* aRange);
  void ReplaceAnchorFocusRange(nsRange* aRange);
  void AdjustAnchorFocusForMultiRange(nsDirection aDirection);

  nsresult GetPrimaryFrameForAnchorNode(nsIFrame** aReturnFrame);
  nsresult GetPrimaryFrameForFocusNode(nsIFrame** aReturnFrame,
                                       int32_t* aOffset, bool aVisual);

  UniquePtr<SelectionDetails> LookUpSelection(
      nsIContent* aContent, int32_t aContentOffset, int32_t aContentLength,
      UniquePtr<SelectionDetails> aDetailsHead, SelectionType aSelectionType,
      bool aSlowCheck);

  NS_IMETHOD Repaint(nsPresContext* aPresContext);

  MOZ_CAN_RUN_SCRIPT
  nsresult StartAutoScrollTimer(nsIFrame* aFrame, const nsPoint& aPoint,
                                uint32_t aDelay);

  nsresult StopAutoScrollTimer();

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL methods
  nsINode* GetAnchorNode() const {
    const RangeBoundary& anchor = AnchorRef();
    return anchor.IsSet() ? anchor.Container() : nullptr;
  }
  uint32_t AnchorOffset() const {
    const RangeBoundary& anchor = AnchorRef();
    const Maybe<uint32_t> offset =
        anchor.Offset(RangeBoundary::OffsetFilter::kValidOffsets);
    return offset ? *offset : 0;
  }
  nsINode* GetFocusNode() const {
    const RangeBoundary& focus = FocusRef();
    return focus.IsSet() ? focus.Container() : nullptr;
  }
  uint32_t FocusOffset() const {
    const RangeBoundary& focus = FocusRef();
    const Maybe<uint32_t> offset =
        focus.Offset(RangeBoundary::OffsetFilter::kValidOffsets);
    return offset ? *offset : 0;
  }

  nsIContent* GetChildAtAnchorOffset() {
    const RangeBoundary& anchor = AnchorRef();
    return anchor.IsSet() ? anchor.GetChildAtOffset() : nullptr;
  }
  nsIContent* GetChildAtFocusOffset() {
    const RangeBoundary& focus = FocusRef();
    return focus.IsSet() ? focus.GetChildAtOffset() : nullptr;
  }

  const RangeBoundary& AnchorRef() const;
  const RangeBoundary& FocusRef() const;

  /*
   * IsCollapsed -- is the whole selection just one point, or unset?
   */
  bool IsCollapsed() const {
    uint32_t cnt = mStyledRanges.mRanges.Length();
    if (cnt == 0) {
      return true;
    }

    if (cnt != 1) {
      return false;
    }

    return mStyledRanges.mRanges[0].mRange->Collapsed();
  }

  // *JS() methods are mapped to Selection.*().
  // They may move focus only when the range represents normal selection.
  // These methods shouldn't be used by non-JS callers.
  void CollapseJS(nsINode* aContainer, uint32_t aOffset,
                  mozilla::ErrorResult& aRv);
  void CollapseToStartJS(mozilla::ErrorResult& aRv);
  void CollapseToEndJS(mozilla::ErrorResult& aRv);

  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  void ExtendJS(nsINode& aContainer, uint32_t aOffset,
                mozilla::ErrorResult& aRv);

  void SelectAllChildrenJS(nsINode& aNode, mozilla::ErrorResult& aRv);

  /**
   * Deletes this selection from document the nodes belong to.
   * Only if this has `SelectionType::eNormal`.
   * TODO: mark as `MOZ_CAN_RUN_SCRIPT`.
   */
  void DeleteFromDocument(mozilla::ErrorResult& aRv);

  uint32_t RangeCount() const { return mStyledRanges.mRanges.Length(); }

  void GetType(nsAString& aOutType) const;

  nsRange* GetRangeAt(uint32_t aIndex, mozilla::ErrorResult& aRv);
  MOZ_CAN_RUN_SCRIPT void AddRangeJS(nsRange& aRange,
                                     mozilla::ErrorResult& aRv);

  /**
   * Callers need to keep `aRange` alive.
   */
  MOZ_CAN_RUN_SCRIPT void RemoveRangeAndUnselectFramesAndNotifyListeners(
      nsRange& aRange, mozilla::ErrorResult& aRv);

  MOZ_CAN_RUN_SCRIPT_BOUNDARY void RemoveAllRanges(mozilla::ErrorResult& aRv);

  /**
   * Whether Stringify should flush layout or not.
   */
  enum class FlushFrames { No, Yes };
  MOZ_CAN_RUN_SCRIPT
  void Stringify(nsAString& aResult, FlushFrames = FlushFrames::Yes);

  /**
   * Indicates whether the node is part of the selection. If partlyContained
   * is true, the function returns true when some part of the node
   * is part of the selection. If partlyContained is false, the
   * function only returns true when the entire node is part of the selection.
   */
  bool ContainsNode(nsINode& aNode, bool aPartlyContained,
                    mozilla::ErrorResult& aRv);

  /**
   * Check to see if the given point is contained within the selection area. In
   * particular, this iterates through all the rects that make up the selection,
   * not just the bounding box, and checks to see if the given point is
   * contained in any one of them.
   * @param aPoint The point to check, relative to the root frame.
   */
  bool ContainsPoint(const nsPoint& aPoint);

  /**
   * Modifies the selection.  Note that the parameters are case-insensitive.
   *
   * @param alter can be one of { "move", "extend" }
   *   - "move" collapses the selection to the end of the selection and
   *      applies the movement direction/granularity to the collapsed
   *      selection.
   *   - "extend" leaves the start of the selection unchanged, and applies
   *      movement direction/granularity to the end of the selection.
   * @param direction can be one of { "forward", "backward", "left", "right" }
   * @param granularity can be one of { "character", "word",
   *                                    "line", "lineboundary" }
   *
   * @throws NS_ERROR_NOT_IMPLEMENTED if the granularity is "sentence",
   * "sentenceboundary", "paragraph", "paragraphboundary", or
   * "documentboundary".  Throws NS_ERROR_INVALID_ARG if alter, direction,
   * or granularity has an unrecognized value.
   */
  // TODO: replace with `MOZ_CAN_RUN_SCRIPT`.
  MOZ_CAN_RUN_SCRIPT_BOUNDARY void Modify(const nsAString& aAlter,
                                          const nsAString& aDirection,
                                          const nsAString& aGranularity,
                                          mozilla::ErrorResult& aRv);

  MOZ_CAN_RUN_SCRIPT
  void SetBaseAndExtentJS(nsINode& aAnchorNode, uint32_t aAnchorOffset,
                          nsINode& aFocusNode, uint32_t aFocusOffset,
                          mozilla::ErrorResult& aRv);

  bool GetInterlinePosition(mozilla::ErrorResult& aRv);
  void SetInterlinePosition(bool aValue, mozilla::ErrorResult& aRv);

  Nullable<int16_t> GetCaretBidiLevel(mozilla::ErrorResult& aRv) const;
  void SetCaretBidiLevel(const Nullable<int16_t>& aCaretBidiLevel,
                         mozilla::ErrorResult& aRv);

  void ToStringWithFormat(const nsAString& aFormatType, uint32_t aFlags,
                          int32_t aWrapColumn, nsAString& aReturn,
                          mozilla::ErrorResult& aRv);
  void AddSelectionListener(nsISelectionListener* aListener);
  void RemoveSelectionListener(nsISelectionListener* aListener);

  RawSelectionType RawType() const {
    return ToRawSelectionType(mSelectionType);
  }
  SelectionType Type() const { return mSelectionType; }

  void GetRangesForInterval(nsINode& aBeginNode, int32_t aBeginOffset,
                            nsINode& aEndNode, int32_t aEndOffset,
                            bool aAllowAdjacent,
                            nsTArray<RefPtr<nsRange>>& aReturn,
                            mozilla::ErrorResult& aRv);

  MOZ_CAN_RUN_SCRIPT void ScrollIntoView(int16_t aRegion, bool aIsSynchronous,
                                         WhereToScroll aVPercent,
                                         WhereToScroll aHPercent,
                                         mozilla::ErrorResult& aRv);

  void SetColors(const nsAString& aForeColor, const nsAString& aBackColor,
                 const nsAString& aAltForeColor, const nsAString& aAltBackColor,
                 mozilla::ErrorResult& aRv);

  void ResetColors(mozilla::ErrorResult& aRv);

  /**
   * Non-JS callers should use the following
   * collapse/collapseToStart/extend/etc methods, instead of the *JS
   * versions that bindings call.
   */

  /**
   * Collapses the selection to a single point, at the specified offset
   * in the given node. When the selection is collapsed, and the content
   * is focused and editable, the caret will blink there.
   * @param aContainer The given node where the selection will be set
   * @param offset      Where in given dom node to place the selection (the
   *                    offset into the given node)
   */
  // TODO: mark as `MOZ_CAN_RUN_SCRIPT`
  // (https://bugzilla.mozilla.org/show_bug.cgi?id=1615296).
  void Collapse(nsINode& aContainer, uint32_t aOffset, ErrorResult& aRv) {
    Collapse(RawRangeBoundary(&aContainer, aOffset), aRv);
  }

  // TODO: this should be `MOZ_CAN_RUN_SCRIPT` instead
  // (https://bugzilla.mozilla.org/show_bug.cgi?id=1615296).
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  void Collapse(const RawRangeBoundary& aPoint, ErrorResult& aRv);

  /**
   * Collapses the whole selection to a single point at the start
   * of the current selection (irrespective of direction).  If content
   * is focused and editable, the caret will blink there.
   */
  void CollapseToStart(mozilla::ErrorResult& aRv);

  /**
   * Collapses the whole selection to a single point at the end
   * of the current selection (irrespective of direction).  If content
   * is focused and editable, the caret will blink there.
   */
  void CollapseToEnd(mozilla::ErrorResult& aRv);

  /**
   * Extends the selection by moving the selection end to the specified node and
   * offset, preserving the selection begin position. The new selection end
   * result will always be from the anchorNode to the new focusNode, regardless
   * of direction.
   *
   * @param aContainer The node where the selection will be extended to
   * @param aOffset    Where in aContainer to place the offset of the new
   *                   selection end.
   */
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  void Extend(nsINode& aContainer, uint32_t aOffset, ErrorResult& aRv);

  MOZ_CAN_RUN_SCRIPT void AddRangeAndSelectFramesAndNotifyListeners(
      nsRange& aRange, mozilla::ErrorResult& aRv);

  /**
   * Adds all children of the specified node to the selection.
   * @param aNode the parent of the children to be added to the selection.
   */
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  void SelectAllChildren(nsINode& aNode, mozilla::ErrorResult& aRv);

  /**
   * SetStartAndEnd() removes all ranges and sets new range as given range.
   * Different from SetBaseAndExtent(), this won't compare the DOM points of
   * aStartRef and aEndRef for performance nor set direction to eDirPrevious.
   * Note that this may reset the limiter and move focus.  If you don't want
   * that, use SetStartAndEndInLimiter() instead.
   */
  MOZ_CAN_RUN_SCRIPT
  void SetStartAndEnd(const RawRangeBoundary& aStartRef,
                      const RawRangeBoundary& aEndRef, ErrorResult& aRv) {
    SetStartAndEndInternal(InLimiter::eNo, aStartRef, aEndRef, eDirNext, aRv);
  }
  MOZ_CAN_RUN_SCRIPT
  void SetStartAndEnd(nsINode& aStartContainer, uint32_t aStartOffset,
                      nsINode& aEndContainer, uint32_t aEndOffset,
                      ErrorResult& aRv) {
    SetStartAndEnd(RawRangeBoundary(&aStartContainer, aStartOffset),
                   RawRangeBoundary(&aEndContainer, aEndOffset), aRv);
  }

  /**
   * SetStartAndEndInLimiter() is similar to SetStartAndEnd(), but this respects
   * the selection limiter.  If all or part of given range is not in the
   * limiter, this returns error.
   */
  MOZ_CAN_RUN_SCRIPT
  void SetStartAndEndInLimiter(const RawRangeBoundary& aStartRef,
                               const RawRangeBoundary& aEndRef,
                               ErrorResult& aRv) {
    SetStartAndEndInternal(InLimiter::eYes, aStartRef, aEndRef, eDirNext, aRv);
  }
  MOZ_CAN_RUN_SCRIPT
  void SetStartAndEndInLimiter(nsINode& aStartContainer, uint32_t aStartOffset,
                               nsINode& aEndContainer, uint32_t aEndOffset,
                               ErrorResult& aRv) {
    SetStartAndEndInLimiter(RawRangeBoundary(&aStartContainer, aStartOffset),
                            RawRangeBoundary(&aEndContainer, aEndOffset), aRv);
  }

  /**
   * SetBaseAndExtent() is alternative of the JS API for internal use.
   * Different from SetStartAndEnd(), this sets anchor and focus points as
   * specified, then if anchor point is after focus node, this sets the
   * direction to eDirPrevious.
   * Note that this may reset the limiter and move focus.  If you don't want
   * that, use SetBaseAndExtentInLimier() instead.
   */
  MOZ_CAN_RUN_SCRIPT
  void SetBaseAndExtent(nsINode& aAnchorNode, uint32_t aAnchorOffset,
                        nsINode& aFocusNode, uint32_t aFocusOffset,
                        ErrorResult& aRv);
  MOZ_CAN_RUN_SCRIPT
  void SetBaseAndExtent(const RawRangeBoundary& aAnchorRef,
                        const RawRangeBoundary& aFocusRef, ErrorResult& aRv) {
    SetBaseAndExtentInternal(InLimiter::eNo, aAnchorRef, aFocusRef, aRv);
  }

  /**
   * SetBaseAndExtentInLimier() is similar to SetBaseAndExtent(), but this
   * respects the selection limiter.  If all or part of given range is not in
   * the limiter, this returns error.
   */
  MOZ_CAN_RUN_SCRIPT
  void SetBaseAndExtentInLimiter(nsINode& aAnchorNode, uint32_t aAnchorOffset,
                                 nsINode& aFocusNode, uint32_t aFocusOffset,
                                 ErrorResult& aRv) {
    SetBaseAndExtentInLimiter(RawRangeBoundary(&aAnchorNode, aAnchorOffset),
                              RawRangeBoundary(&aFocusNode, aFocusOffset), aRv);
  }
  MOZ_CAN_RUN_SCRIPT
  void SetBaseAndExtentInLimiter(const RawRangeBoundary& aAnchorRef,
                                 const RawRangeBoundary& aFocusRef,
                                 ErrorResult& aRv) {
    SetBaseAndExtentInternal(InLimiter::eYes, aAnchorRef, aFocusRef, aRv);
  }

  void AddSelectionChangeBlocker();
  void RemoveSelectionChangeBlocker();
  bool IsBlockingSelectionChangeEvents() const;

  // Whether this selection is focused in an editable element.
  bool IsEditorSelection() const;

  /**
   * Set the painting style for the range. The range must be a range in
   * the selection. The textRangeStyle will be used by text frame
   * when it is painting the selection.
   */
  nsresult SetTextRangeStyle(nsRange* aRange,
                             const TextRangeStyle& aTextRangeStyle);

  // Methods to manipulate our mFrameSelection's ancestor limiter.
  nsIContent* GetAncestorLimiter() const;
  void SetAncestorLimiter(nsIContent* aLimiter);

  /*
   * Frame Offset cache can be used just during calling
   * nsEditor::EndPlaceHolderTransaction. EndPlaceHolderTransaction will give
   * rise to reflow/refreshing view/scroll, and call times of
   * nsTextFrame::GetPointFromOffset whose return value is to be cached. see
   * bugs 35296 and 199412
   */
  void SetCanCacheFrameOffset(bool aCanCacheFrameOffset);

  // Selection::GetRangesForIntervalArray
  //
  //    Fills a nsTArray with the ranges overlapping the range specified by
  //    the given endpoints. Ranges in the selection exactly adjacent to the
  //    input range are not returned unless aAllowAdjacent is set.
  //
  //    For example, if the following ranges were in the selection
  //    (assume everything is within the same node)
  //
  //    Start Offset: 0 2 7 9
  //      End Offset: 2 5 9 10
  //
  //    and passed aBeginOffset of 2 and aEndOffset of 9, then with
  //    aAllowAdjacent set, all the ranges should be returned. If
  //    aAllowAdjacent was false, the ranges [2, 5] and [7, 9] only
  //    should be returned
  //
  //    Now that overlapping ranges are disallowed, there can be a maximum of
  //    2 adjacent ranges
  nsresult GetRangesForIntervalArray(nsINode* aBeginNode, int32_t aBeginOffset,
                                     nsINode* aEndNode, int32_t aEndOffset,
                                     bool aAllowAdjacent,
                                     nsTArray<nsRange*>* aRanges);

  /**
   * Modifies the cursor Bidi level after a change in keyboard direction
   * @param langRTL is true if the new language is right-to-left or
   *                false if the new language is left-to-right.
   */
  nsresult SelectionLanguageChange(bool aLangRTL);

 private:
  friend class ::nsAutoScrollTimer;

  MOZ_CAN_RUN_SCRIPT nsresult DoAutoScroll(nsIFrame* aFrame, nsPoint aPoint);

  bool HasSameRootOrSameComposedDoc(const nsINode& aNode);

  // XXX Please don't add additional uses of this method, it's only for
  // XXX supporting broken code (bug 1245883) in the following classes:
  friend class ::nsCopySupport;
  friend class ::nsHTMLCopyEncoder;
  MOZ_CAN_RUN_SCRIPT
  void AddRangeAndSelectFramesAndNotifyListeners(nsRange& aRange,
                                                 Document* aDocument,
                                                 ErrorResult&);

  // This is helper method for GetPrimaryFrameForFocusNode.
  // If aVisual is true, this returns caret frame.
  // If false, this returns primary frame.
  nsresult GetPrimaryOrCaretFrameForNodeOffset(nsIContent* aContent,
                                               uint32_t aOffset,
                                               nsIFrame** aReturnFrame,
                                               int32_t* aOffsetUsed,
                                               bool aVisual) const;

  // Get the cached value for nsTextFrame::GetPointFromOffset.
  nsresult GetCachedFrameOffset(nsIFrame* aFrame, int32_t inOffset,
                                nsPoint& aPoint);

  enum class InLimiter {
    // If eYes, the method may reset selection limiter and move focus if the
    // given range is out of the limiter.
    eYes,
    // If eNo, the method won't reset selection limiter.  So, if given range
    // is out of bounds, the method may return error.
    eNo,
  };
  MOZ_CAN_RUN_SCRIPT
  void SetStartAndEndInternal(InLimiter aInLimiter,
                              const RawRangeBoundary& aStartRef,
                              const RawRangeBoundary& aEndRef,
                              nsDirection aDirection, ErrorResult& aRv);
  MOZ_CAN_RUN_SCRIPT
  void SetBaseAndExtentInternal(InLimiter aInLimiter,
                                const RawRangeBoundary& aAnchorRef,
                                const RawRangeBoundary& aFocusRef,
                                ErrorResult& aRv);

 public:
  SelectionType GetType() const { return mSelectionType; }

  SelectionCustomColors* GetCustomColors() const { return mCustomColors.get(); }

  MOZ_CAN_RUN_SCRIPT nsresult NotifySelectionListeners(bool aCalledByJS);
  MOZ_CAN_RUN_SCRIPT nsresult NotifySelectionListeners();

  friend struct AutoUserInitiated;
  struct MOZ_RAII AutoUserInitiated {
    explicit AutoUserInitiated(
        Selection* aSelection MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
        : mSavedValue(aSelection->mUserInitiated) {
      MOZ_GUARD_OBJECT_NOTIFIER_INIT;
      aSelection->mUserInitiated = true;
    }
    AutoRestore<bool> mSavedValue;
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
  };

 private:
  friend struct mozilla::AutoPrepareFocusRange;
  class ScrollSelectionIntoViewEvent;
  friend class ScrollSelectionIntoViewEvent;

  class ScrollSelectionIntoViewEvent : public Runnable {
   public:
    MOZ_CAN_RUN_SCRIPT_BOUNDARY NS_DECL_NSIRUNNABLE

    ScrollSelectionIntoViewEvent(Selection* aSelection, SelectionRegion aRegion,
                                 ScrollAxis aVertical, ScrollAxis aHorizontal,
                                 int32_t aFlags)
        : Runnable("dom::Selection::ScrollSelectionIntoViewEvent"),
          mSelection(aSelection),
          mRegion(aRegion),
          mVerticalScroll(aVertical),
          mHorizontalScroll(aHorizontal),
          mFlags(aFlags) {
      NS_ASSERTION(aSelection, "null parameter");
    }
    void Revoke() { mSelection = nullptr; }

   private:
    Selection* mSelection;
    SelectionRegion mRegion;
    ScrollAxis mVerticalScroll;
    ScrollAxis mHorizontalScroll;
    int32_t mFlags;
  };

  /**
   * Set mAnchorFocusRange to mStyledRanges.mRanges[aIndex] if aIndex is a valid
   * index. Set mAnchorFocusRange to nullptr if aIndex is negative. Otherwise,
   * i.e., if aIndex is positive but out of bounds of mStyledRanges.mRanges, do
   * nothing.
   */
  void SetAnchorFocusRange(int32_t aIndex);
  void SelectFramesOf(nsIContent* aContent, bool aSelected) const;

  /**
   * https://dom.spec.whatwg.org/#concept-tree-inclusive-descendant.
   */
  nsresult SelectFramesOfInclusiveDescendantsOfContent(
      PostContentIterator& aPostOrderIter, nsIContent* aContent,
      bool aSelected) const;

  nsresult SelectFrames(nsPresContext* aPresContext, nsRange* aRange,
                        bool aSelect) const;

  /**
   * SelectFramesInAllRanges() calls SelectFrames() for all current
   * ranges.
   */
  void SelectFramesInAllRanges(nsPresContext* aPresContext);

  /**
   * @param aOutIndex points to the index of the range in mStyledRanges.mRanges.
   * If aDidAddRange is true, it is in [0, mStyledRanges.mRanges.Length()).
   */
  MOZ_CAN_RUN_SCRIPT nsresult MaybeAddTableCellRange(nsRange& aRange,
                                                     bool* aDidAddRange,
                                                     int32_t* aOutIndex);

  Document* GetDocument() const;

  void Disconnect();

  struct StyledRanges {
    StyledRange* FindRangeData(nsRange* aRange);

    nsresult RemoveRangeAndUnregisterSelection(nsRange& aRange);

    /**
     * Binary searches the given sorted array of ranges for the insertion point
     * for the given node/offset. The given comparator is used, and the index
     * where the point should appear in the array is returned.

     * If there is an item in the array equal to the input point (aPointNode,
     * aPointOffset), we will return the index of this item.
     *
     * @return the index where the point should appear in the array. In
     *         [0, `aElementArray->Length()`].
     */
    static int32_t FindInsertionPoint(
        const nsTArray<StyledRange>* aElementArray, const nsINode& aPointNode,
        int32_t aPointOffset,
        int32_t (*aComparator)(const nsINode&, int32_t, const nsRange&));

    /**
     * Works on the same principle as GetRangesForIntervalArray, however
     * instead this returns the indices into mRanges between which
     * the overlapping ranges lie.
     *
     * @param aStartIndex will be less or equal than aEndIndex.
     * @param aEndIndex can be in [-1, mRanges.Length()].
     */
    nsresult GetIndicesForInterval(const nsINode* aBeginNode,
                                   int32_t aBeginOffset,
                                   const nsINode* aEndNode, int32_t aEndOffset,
                                   bool aAllowAdjacent, int32_t& aStartIndex,
                                   int32_t& aEndIndex) const;

    bool HasEqualRangeBoundariesAt(const nsRange& aRange,
                                   int32_t aRangeIndex) const;

    /**
     * Preserves the sorting and disjunctiveness of mRanges.
     *
     * @param aOutIndex will point to the index of the added range, or if aRange
     *                  is already contained, to the one containing it. Hence
     *                  it'll always be in [0, mRanges.Length()).
     */
    MOZ_CAN_RUN_SCRIPT nsresult MaybeAddRangeAndTruncateOverlaps(
        nsRange* aRange, int32_t* aOutIndex, Selection& aSelection);

    /**
     * GetCommonEditingHost() returns common editing host of all
     * ranges if there is. If at least one of the ranges is in non-editable
     * element, returns nullptr.  See following examples for the detail:
     *
     *  <div id="a" contenteditable>
     *    an[cestor
     *    <div id="b" contenteditable="false">
     *      non-editable
     *      <div id="c" contenteditable>
     *        desc]endant
     *  in this case, this returns div#a because div#c is also in div#a.
     *
     *  <div id="a" contenteditable>
     *    an[ce]stor
     *    <div id="b" contenteditable="false">
     *      non-editable
     *      <div id="c" contenteditable>
     *        de[sc]endant
     *  in this case, this returns div#a because second range is also in div#a
     *  and common ancestor of the range (i.e., div#c) is editable.
     *
     *  <div id="a" contenteditable>
     *    an[ce]stor
     *    <div id="b" contenteditable="false">
     *      [non]-editable
     *      <div id="c" contenteditable>
     *        de[sc]endant
     *  in this case, this returns nullptr because the second range is in
     *  non-editable area.
     */
    Element* GetCommonEditingHost() const;

    void MaybeFocusCommonEditingHost(PresShell* aPresShell) const;

    // These are the ranges inside this selection. They are kept sorted in order
    // of DOM start position.
    //
    // This data structure is sorted by the range beginnings. As the ranges are
    // disjoint, it is also implicitly sorted by the range endings. This allows
    // us to perform binary searches when searching for existence of a range,
    // giving us O(log n) search time.
    //
    // Inserting a new range requires finding the overlapping interval,
    // requiring two binary searches plus up to an additional 6 DOM comparisons.
    // If this proves to be a performance concern, then an interval tree may be
    // a possible solution, allowing the calculation of the overlap interval in
    // O(log n) time, though this would require rebalancing and other overhead.
    AutoTArray<StyledRange, 1> mRanges;
  };

  StyledRanges mStyledRanges;

  RefPtr<nsRange> mAnchorFocusRange;
  RefPtr<nsFrameSelection> mFrameSelection;
  RefPtr<AccessibleCaretEventHub> mAccessibleCaretEventHub;
  RefPtr<SelectionChangeEventDispatcher> mSelectionChangeEventDispatcher;
  RefPtr<nsAutoScrollTimer> mAutoScrollTimer;
  nsTArray<nsCOMPtr<nsISelectionListener>> mSelectionListeners;
  nsRevocableEventPtr<ScrollSelectionIntoViewEvent> mScrollEvent;
  CachedOffsetForFrame* mCachedOffsetForFrame;
  nsDirection mDirection;
  const SelectionType mSelectionType;
  UniquePtr<SelectionCustomColors> mCustomColors;

  // Non-zero if we don't want any changes we make to the selection to be
  // visible to content. If non-zero, content won't be notified about changes.
  uint32_t mSelectionChangeBlockerCount;

  /**
   * True if the current selection operation was initiated by user action.
   * It determines whether we exclude -moz-user-select:none nodes or not,
   * as well as whether selectstart events will be fired.
   */
  bool mUserInitiated;

  /**
   * When the selection change is caused by a call of Selection API,
   * mCalledByJS is true.  Otherwise, false.
   */
  bool mCalledByJS;

  /**
   * true if AutoCopyListner::OnSelectionChange() should be called.
   */
  bool mNotifyAutoCopy;
};

// Stack-class to turn on/off selection batching.
class MOZ_STACK_CLASS SelectionBatcher final {
 private:
  RefPtr<Selection> mSelection;

 public:
  explicit SelectionBatcher(Selection* aSelection) {
    mSelection = aSelection;
    if (mSelection) {
      mSelection->StartBatchChanges();
    }
  }

  ~SelectionBatcher() {
    if (mSelection) {
      mSelection->EndBatchChanges();
    }
  }
};

class MOZ_RAII AutoHideSelectionChanges final {
 private:
  RefPtr<Selection> mSelection;
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
 public:
  explicit AutoHideSelectionChanges(const nsFrameSelection* aFrame);

  explicit AutoHideSelectionChanges(
      Selection* aSelection MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : mSelection(aSelection) {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    mSelection = aSelection;
    if (mSelection) {
      mSelection->AddSelectionChangeBlocker();
    }
  }

  ~AutoHideSelectionChanges() {
    if (mSelection) {
      mSelection->RemoveSelectionChangeBlocker();
    }
  }
};

}  // namespace dom

inline bool IsValidRawSelectionType(RawSelectionType aRawSelectionType) {
  return aRawSelectionType >= nsISelectionController::SELECTION_NONE &&
         aRawSelectionType <= nsISelectionController::SELECTION_URLSTRIKEOUT;
}

inline SelectionType ToSelectionType(RawSelectionType aRawSelectionType) {
  if (!IsValidRawSelectionType(aRawSelectionType)) {
    return SelectionType::eInvalid;
  }
  return static_cast<SelectionType>(aRawSelectionType);
}

inline RawSelectionType ToRawSelectionType(SelectionType aSelectionType) {
  MOZ_ASSERT(aSelectionType != SelectionType::eInvalid);
  return static_cast<RawSelectionType>(aSelectionType);
}

inline RawSelectionType ToRawSelectionType(TextRangeType aTextRangeType) {
  return ToRawSelectionType(ToSelectionType(aTextRangeType));
}

inline SelectionTypeMask ToSelectionTypeMask(SelectionType aSelectionType) {
  MOZ_ASSERT(aSelectionType != SelectionType::eInvalid);
  return aSelectionType == SelectionType::eNone
             ? 0
             : static_cast<SelectionTypeMask>(
                   1 << (static_cast<uint8_t>(aSelectionType) - 1));
}

}  // namespace mozilla

#endif  // mozilla_Selection_h__
