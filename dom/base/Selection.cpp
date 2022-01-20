/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Implementation of mozilla::dom::Selection
 */

#include "mozilla/dom/Selection.h"
#include "mozilla/intl/BidiEmbeddingLevel.h"

#include "mozilla/AccessibleCaretEventHub.h"
#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/Attributes.h"
#include "mozilla/AutoCopyListener.h"
#include "mozilla/AutoRestore.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/ContentIterator.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/SelectionBinding.h"
#include "mozilla/dom/ShadowRoot.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/EventStates.h"
#include "mozilla/HTMLEditor.h"
#include "mozilla/Logging.h"
#include "mozilla/PresShell.h"
#include "mozilla/RangeBoundary.h"
#include "mozilla/RangeUtils.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/Telemetry.h"

#include "nsCOMPtr.h"
#include "nsDebug.h"
#include "nsString.h"
#include "nsFrameSelection.h"
#include "nsISelectionListener.h"
#include "nsContentCID.h"
#include "nsDeviceContext.h"
#include "nsIContent.h"
#include "nsIContentInlines.h"
#include "nsRange.h"
#include "nsITableCellLayout.h"
#include "nsTArray.h"
#include "nsTableWrapperFrame.h"
#include "nsTableCellFrame.h"
#include "nsIScrollableFrame.h"
#include "nsCCUncollectableMarker.h"
#include "nsIDocumentEncoder.h"
#include "nsTextFragment.h"
#include <algorithm>
#include "nsContentUtils.h"

#include "nsGkAtoms.h"
#include "nsLayoutUtils.h"
#include "nsBidiPresUtils.h"
#include "nsTextFrame.h"

#include "nsContentUtils.h"
#include "nsThreadUtils.h"

#include "nsPresContext.h"
#include "nsCaret.h"

#include "nsITimer.h"
#include "mozilla/dom/Document.h"
#include "nsINamed.h"

#include "nsISelectionController.h"  //for the enums
#include "nsCopySupport.h"
#include "nsIFrameInlines.h"
#include "nsRefreshDriver.h"

#include "nsError.h"
#include "nsViewManager.h"

#include "nsFocusManager.h"
#include "nsPIDOMWindow.h"

using namespace mozilla;
using namespace mozilla::dom;

static LazyLogModule sSelectionLog("Selection");

//#define DEBUG_TABLE 1

#ifdef PRINT_RANGE
static void printRange(nsRange* aDomRange);
#  define DEBUG_OUT_RANGE(x) printRange(x)
#else
#  define DEBUG_OUT_RANGE(x)
#endif  // PRINT_RANGE

static constexpr nsLiteralCString kNoDocumentTypeNodeError =
    "DocumentType nodes are not supported"_ns;
static constexpr nsLiteralCString kNoRangeExistsError =
    "No selection range exists"_ns;

/******************************************************************************
 * Utility methods defined in nsISelectionController.idl
 ******************************************************************************/

namespace mozilla {

const char* ToChar(SelectionType aSelectionType) {
  switch (aSelectionType) {
    case SelectionType::eInvalid:
      return "SelectionType::eInvalid";
    case SelectionType::eNone:
      return "SelectionType::eNone";
    case SelectionType::eNormal:
      return "SelectionType::eNormal";
    case SelectionType::eSpellCheck:
      return "SelectionType::eSpellCheck";
    case SelectionType::eIMERawClause:
      return "SelectionType::eIMERawClause";
    case SelectionType::eIMESelectedRawClause:
      return "SelectionType::eIMESelectedRawClause";
    case SelectionType::eIMEConvertedClause:
      return "SelectionType::eIMEConvertedClause";
    case SelectionType::eIMESelectedClause:
      return "SelectionType::eIMESelectedClause";
    case SelectionType::eAccessibility:
      return "SelectionType::eAccessibility";
    case SelectionType::eFind:
      return "SelectionType::eFind";
    case SelectionType::eURLSecondary:
      return "SelectionType::eURLSecondary";
    case SelectionType::eURLStrikeout:
      return "SelectionType::eURLStrikeout";
    default:
      return "Invalid SelectionType";
  }
}

}  // namespace mozilla

//#define DEBUG_SELECTION // uncomment for printf describing every collapse and
// extend. #define DEBUG_NAVIGATION

//#define DEBUG_TABLE_SELECTION 1

struct CachedOffsetForFrame {
  CachedOffsetForFrame()
      : mCachedFrameOffset(0, 0)  // nsPoint ctor
        ,
        mLastCaretFrame(nullptr),
        mLastContentOffset(0),
        mCanCacheFrameOffset(false) {}

  nsPoint mCachedFrameOffset;  // cached frame offset
  nsIFrame* mLastCaretFrame;   // store the frame the caret was last drawn in.
  int32_t mLastContentOffset;  // store last content offset
  bool mCanCacheFrameOffset;   // cached frame offset is valid?
};

class AutoScroller final : public nsITimerCallback, public nsINamed {
 public:
  NS_DECL_ISUPPORTS

  explicit AutoScroller(nsFrameSelection* aFrameSelection)
      : mFrameSelection(aFrameSelection),
        mPresContext(0),
        mPoint(0, 0),
        mDelayInMs(30),
        mFurtherScrollingAllowed(FurtherScrollingAllowed::kYes) {
    MOZ_ASSERT(mFrameSelection);
  }

  MOZ_CAN_RUN_SCRIPT nsresult DoAutoScroll(nsIFrame* aFrame, nsPoint aPoint);

 private:
  // aPoint is relative to aPresContext's root frame
  nsresult ScheduleNextDoAutoScroll(nsPresContext* aPresContext,
                                    nsPoint& aPoint) {
    if (NS_WARN_IF(mFurtherScrollingAllowed == FurtherScrollingAllowed::kNo)) {
      return NS_ERROR_FAILURE;
    }

    mPoint = aPoint;

    // Store the presentation context. The timer will be
    // stopped by the selection if the prescontext is destroyed.
    mPresContext = aPresContext;

    mContent = PresShell::GetCapturingContent();

    if (!mTimer) {
      mTimer = NS_NewTimer(
          mPresContext->Document()->EventTargetFor(TaskCategory::Other));

      if (!mTimer) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
    }

    return mTimer->InitWithCallback(this, mDelayInMs, nsITimer::TYPE_ONE_SHOT);
  }

 public:
  enum class FurtherScrollingAllowed { kYes, kNo };

  void Stop(const FurtherScrollingAllowed aFurtherScrollingAllowed) {
    MOZ_ASSERT((aFurtherScrollingAllowed == FurtherScrollingAllowed::kNo) ||
               (mFurtherScrollingAllowed == FurtherScrollingAllowed::kYes));

    if (mTimer) {
      mTimer->Cancel();
      mTimer = nullptr;
    }

    mContent = nullptr;
    mFurtherScrollingAllowed = aFurtherScrollingAllowed;
  }

  void SetDelay(uint32_t aDelayInMs) { mDelayInMs = aDelayInMs; }

  MOZ_CAN_RUN_SCRIPT_BOUNDARY NS_IMETHOD Notify(nsITimer* timer) override {
    if (mPresContext) {
      AutoWeakFrame frame =
          mContent ? mPresContext->GetPrimaryFrameFor(mContent) : nullptr;
      if (!frame) {
        return NS_OK;
      }
      mContent = nullptr;

      nsPoint pt = mPoint - frame->GetOffsetTo(
                                mPresContext->PresShell()->GetRootFrame());
      RefPtr<nsFrameSelection> frameSelection = mFrameSelection;
      frameSelection->HandleDrag(frame, pt);
      if (!frame.IsAlive()) {
        return NS_OK;
      }

      NS_ASSERTION(frame->PresContext() == mPresContext, "document mismatch?");
      DoAutoScroll(frame, pt);
    }
    return NS_OK;
  }

  NS_IMETHOD GetName(nsACString& aName) override {
    aName.AssignLiteral("AutoScroller");
    return NS_OK;
  }

 protected:
  virtual ~AutoScroller() {
    if (mTimer) {
      mTimer->Cancel();
    }
  }

 private:
  nsFrameSelection* const mFrameSelection;
  nsPresContext* mPresContext;
  // relative to mPresContext's root frame
  nsPoint mPoint;
  nsCOMPtr<nsITimer> mTimer;
  nsCOMPtr<nsIContent> mContent;
  uint32_t mDelayInMs;
  FurtherScrollingAllowed mFurtherScrollingAllowed;
};

NS_IMPL_ISUPPORTS(AutoScroller, nsITimerCallback, nsINamed)

#ifdef PRINT_RANGE
void printRange(nsRange* aDomRange) {
  if (!aDomRange) {
    printf("NULL Range\n");
  }
  nsINode* startNode = aDomRange->GetStartContainer();
  nsINode* endNode = aDomRange->GetEndContainer();
  int32_t startOffset = aDomRange->StartOffset();
  int32_t endOffset = aDomRange->EndOffset();

  printf("range: 0x%lx\t start: 0x%lx %ld, \t end: 0x%lx,%ld\n",
         (unsigned long)aDomRange, (unsigned long)startNode, (long)startOffset,
         (unsigned long)endNode, (long)endOffset);
}
#endif /* PRINT_RANGE */

void Selection::Stringify(nsAString& aResult, FlushFrames aFlushFrames) {
  if (aFlushFrames == FlushFrames::Yes) {
    // We need FlushType::Frames here to make sure frames have been created for
    // the selected content.  Use mFrameSelection->GetPresShell() which returns
    // null if the Selection has been disconnected (the shell is Destroyed).
    RefPtr<PresShell> presShell =
        mFrameSelection ? mFrameSelection->GetPresShell() : nullptr;
    if (!presShell) {
      aResult.Truncate();
      return;
    }
    presShell->FlushPendingNotifications(FlushType::Frames);
  }

  IgnoredErrorResult rv;
  ToStringWithFormat(u"text/plain"_ns, nsIDocumentEncoder::SkipInvisibleContent,
                     0, aResult, rv);
  if (rv.Failed()) {
    aResult.Truncate();
  }
}

void Selection::ToStringWithFormat(const nsAString& aFormatType,
                                   uint32_t aFlags, int32_t aWrapCol,
                                   nsAString& aReturn, ErrorResult& aRv) {
  nsCOMPtr<nsIDocumentEncoder> encoder =
      do_createDocumentEncoder(NS_ConvertUTF16toUTF8(aFormatType).get());
  if (!encoder) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  PresShell* presShell = GetPresShell();
  if (!presShell) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  Document* doc = presShell->GetDocument();

  // Flags should always include OutputSelectionOnly if we're coming from here:
  aFlags |= nsIDocumentEncoder::OutputSelectionOnly;
  nsAutoString readstring;
  readstring.Assign(aFormatType);
  nsresult rv = encoder->Init(doc, readstring, aFlags);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return;
  }

  encoder->SetSelection(this);
  if (aWrapCol != 0) encoder->SetWrapColumn(aWrapCol);

  rv = encoder->EncodeToString(aReturn);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
  }
}

void Selection::SetInterlinePosition(bool aHintRight, ErrorResult& aRv) {
  MOZ_ASSERT(mSelectionType == SelectionType::eNormal);

  if (!mFrameSelection) {
    aRv.Throw(NS_ERROR_NOT_INITIALIZED);  // Can't do selection
    return;
  }

  mFrameSelection->SetHint(aHintRight ? CARET_ASSOCIATE_AFTER
                                      : CARET_ASSOCIATE_BEFORE);
}

bool Selection::GetInterlinePosition(ErrorResult& aRv) {
  MOZ_ASSERT(mSelectionType == SelectionType::eNormal);

  if (!mFrameSelection) {
    aRv.Throw(NS_ERROR_NOT_INITIALIZED);  // Can't do selection
    return false;
  }
  return mFrameSelection->GetHint() == CARET_ASSOCIATE_AFTER;
}

static bool IsEditorNode(const nsINode* aNode) {
  if (!aNode) {
    return false;
  }

  if (aNode->IsEditable()) {
    return true;
  }

  auto* element = Element::FromNode(aNode);
  return element && element->State().HasState(NS_EVENT_STATE_READWRITE);
}

bool Selection::IsEditorSelection() const {
  return IsEditorNode(GetFocusNode());
}

Nullable<int16_t> Selection::GetCaretBidiLevel(
    mozilla::ErrorResult& aRv) const {
  MOZ_ASSERT(mSelectionType == SelectionType::eNormal);

  if (!mFrameSelection) {
    aRv.Throw(NS_ERROR_NOT_INITIALIZED);
    return Nullable<int16_t>();
  }
  mozilla::intl::BidiEmbeddingLevel caretBidiLevel =
      static_cast<mozilla::intl::BidiEmbeddingLevel>(
          mFrameSelection->GetCaretBidiLevel());
  return (caretBidiLevel & BIDI_LEVEL_UNDEFINED)
             ? Nullable<int16_t>()
             : Nullable<int16_t>(caretBidiLevel);
}

void Selection::SetCaretBidiLevel(const Nullable<int16_t>& aCaretBidiLevel,
                                  mozilla::ErrorResult& aRv) {
  MOZ_ASSERT(mSelectionType == SelectionType::eNormal);

  if (!mFrameSelection) {
    aRv.Throw(NS_ERROR_NOT_INITIALIZED);
    return;
  }
  if (aCaretBidiLevel.IsNull()) {
    mFrameSelection->UndefineCaretBidiLevel();
  } else {
    mFrameSelection->SetCaretBidiLevelAndMaybeSchedulePaint(
        mozilla::intl::BidiEmbeddingLevel(aCaretBidiLevel.Value()));
  }
}

/**
 * Test whether the supplied range points to a single table element.
 * Result is one of the TableSelectionMode constants. "None" means
 * a table element isn't selected.
 */
// TODO: Figure out TableSelectionMode::Column and TableSelectionMode::AllCells
static nsresult GetTableSelectionMode(const nsRange& aRange,
                                      TableSelectionMode* aTableSelectionType) {
  if (!aTableSelectionType) {
    return NS_ERROR_NULL_POINTER;
  }

  *aTableSelectionType = TableSelectionMode::None;

  nsINode* startNode = aRange.GetStartContainer();
  if (!startNode) {
    return NS_ERROR_FAILURE;
  }

  nsINode* endNode = aRange.GetEndContainer();
  if (!endNode) {
    return NS_ERROR_FAILURE;
  }

  // Not a single selected node
  if (startNode != endNode) {
    return NS_OK;
  }

  nsIContent* child = aRange.GetChildAtStartOffset();

  // Not a single selected node
  if (!child || child->GetNextSibling() != aRange.GetChildAtEndOffset()) {
    return NS_OK;
  }

  nsIContent* startContent = static_cast<nsIContent*>(startNode);
  if (!(startNode->IsElement() && startContent->IsHTMLElement())) {
    // Implies a check for being an element; if we ever make this work
    // for non-HTML, need to keep checking for elements.
    return NS_OK;
  }

  if (startContent->IsHTMLElement(nsGkAtoms::tr)) {
    *aTableSelectionType = TableSelectionMode::Cell;
  } else  // check to see if we are selecting a table or row (column and all
          // cells not done yet)
  {
    if (child->IsHTMLElement(nsGkAtoms::table)) {
      *aTableSelectionType = TableSelectionMode::Table;
    } else if (child->IsHTMLElement(nsGkAtoms::tr)) {
      *aTableSelectionType = TableSelectionMode::Row;
    }
  }

  return NS_OK;
}

nsresult Selection::MaybeAddTableCellRange(nsRange& aRange, bool* aDidAddRange,
                                           int32_t* aOutIndex) {
  if (!aDidAddRange || !aOutIndex) {
    return NS_ERROR_NULL_POINTER;
  }

  *aDidAddRange = false;
  *aOutIndex = -1;

  if (!mFrameSelection) return NS_OK;

  // Get if we are adding a cell selection and the row, col of cell if we are
  TableSelectionMode tableMode;
  nsresult result = GetTableSelectionMode(aRange, &tableMode);
  if (NS_FAILED(result)) return result;

  // If not adding a cell range, we are done here
  if (tableMode != TableSelectionMode::Cell) {
    mFrameSelection->mTableSelection.mMode = tableMode;
    // Don't fail if range isn't a selected cell, aDidAddRange tells caller if
    // we didn't proceed
    return NS_OK;
  }

  // Set frame selection mode only if not already set to a table mode
  // so we don't lose the select row and column flags (not detected by
  // getTableCellLocation)
  if (mFrameSelection->mTableSelection.mMode == TableSelectionMode::None) {
    mFrameSelection->mTableSelection.mMode = tableMode;
  }

  result = AddRangesForSelectableNodes(&aRange, aOutIndex,
                                       DispatchSelectstartEvent::Maybe);

  *aDidAddRange = *aOutIndex != -1;
  return result;
}

Selection::Selection(SelectionType aSelectionType,
                     nsFrameSelection* aFrameSelection)
    : mFrameSelection(aFrameSelection),
      mCachedOffsetForFrame(nullptr),
      mDirection(eDirNext),
      mSelectionType(aSelectionType),
      mCustomColors(nullptr),
      mSelectionChangeBlockerCount(0),
      mUserInitiated(false),
      mCalledByJS(false),
      mNotifyAutoCopy(false) {}

Selection::~Selection() { Disconnect(); }

void Selection::Disconnect() {
  SetAnchorFocusRange(-1);

  mStyledRanges.UnregisterSelection();

  if (mAutoScroller) {
    mAutoScroller->Stop(AutoScroller::FurtherScrollingAllowed::kNo);
    mAutoScroller = nullptr;
  }

  mScrollEvent.Revoke();

  if (mCachedOffsetForFrame) {
    delete mCachedOffsetForFrame;
    mCachedOffsetForFrame = nullptr;
  }
}

Document* Selection::GetParentObject() const {
  PresShell* presShell = GetPresShell();
  return presShell ? presShell->GetDocument() : nullptr;
}

DocGroup* Selection::GetDocGroup() const {
  PresShell* presShell = GetPresShell();
  if (!presShell) {
    return nullptr;
  }
  Document* doc = presShell->GetDocument();
  return doc ? doc->GetDocGroup() : nullptr;
}

NS_IMPL_CYCLE_COLLECTION_CLASS(Selection)

MOZ_CAN_RUN_SCRIPT_BOUNDARY
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(Selection)
  // Unlink the selection listeners *before* we do RemoveAllRanges since
  // we don't want to notify the listeners during JS GC (they could be
  // in JS!).
  tmp->mNotifyAutoCopy = false;
  if (tmp->mAccessibleCaretEventHub) {
    tmp->StopNotifyingAccessibleCaretEventHub();
  }
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mSelectionChangeEventDispatcher)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mSelectionListeners)
  MOZ_KnownLive(tmp)->RemoveAllRanges(IgnoreErrors());
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mFrameSelection)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
  NS_IMPL_CYCLE_COLLECTION_UNLINK_WEAK_PTR
  NS_IMPL_CYCLE_COLLECTION_UNLINK_WEAK_REFERENCE
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(Selection)
  {
    uint32_t i, count = tmp->mStyledRanges.Length();
    for (i = 0; i < count; ++i) {
      NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mStyledRanges.mRanges[i].mRange)
    }
  }
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mAnchorFocusRange)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mFrameSelection)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSelectionChangeEventDispatcher)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSelectionListeners)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END
NS_IMPL_CYCLE_COLLECTION_TRACE_WRAPPERCACHE(Selection)

// QueryInterface implementation for Selection
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Selection)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_MAIN_THREAD_ONLY_CYCLE_COLLECTING_ADDREF(Selection)
NS_IMPL_MAIN_THREAD_ONLY_CYCLE_COLLECTING_RELEASE_WITH_LAST_RELEASE(
    Selection, Disconnect())

const RangeBoundary& Selection::AnchorRef() const {
  if (!mAnchorFocusRange) {
    static RangeBoundary sEmpty;
    return sEmpty;
  }

  if (GetDirection() == eDirNext) {
    return mAnchorFocusRange->StartRef();
  }

  return mAnchorFocusRange->EndRef();
}

const RangeBoundary& Selection::FocusRef() const {
  if (!mAnchorFocusRange) {
    static RangeBoundary sEmpty;
    return sEmpty;
  }

  if (GetDirection() == eDirNext) {
    return mAnchorFocusRange->EndRef();
  }

  return mAnchorFocusRange->StartRef();
}

void Selection::SetAnchorFocusRange(int32_t indx) {
  if (indx >= (int32_t)mStyledRanges.Length()) {
    return;
  }

  if (indx < 0)  // release all
  {
    mAnchorFocusRange = nullptr;
  } else {
    mAnchorFocusRange = mStyledRanges.mRanges[indx].mRange;
  }
}

static int32_t CompareToRangeStart(const nsINode& aCompareNode,
                                   int32_t aCompareOffset,
                                   const nsRange& aRange) {
  MOZ_ASSERT(aRange.GetStartContainer());
  nsINode* start = aRange.GetStartContainer();
  // If the nodes that we're comparing are not in the same document or in the
  // same subtree, assume that aCompareNode will fall at the end of the ranges.
  // NOTE(emilio): This is broken (bug 1590379). When fixed, shadow-including
  // tree order[1] seems the most reasonable order, but if we choose other order
  // than that code in nsPrintJob.cpp to deal with selection printing might need
  // to be fixed.
  //
  // [1]: https://dom.spec.whatwg.org/#concept-shadow-including-tree-order
  if (aCompareNode.GetComposedDoc() != start->GetComposedDoc() ||
      !start->GetComposedDoc() ||
      aCompareNode.SubtreeRoot() != start->SubtreeRoot()) {
    NS_WARNING(
        "`CompareToRangeStart` couldn't compare nodes, pretending some order.");
    return 1;
  }

  // The points are in the same subtree, hence there has to be an order.
  return *nsContentUtils::ComparePoints(&aCompareNode, aCompareOffset, start,
                                        aRange.StartOffset());
}

static int32_t CompareToRangeEnd(const nsINode& aCompareNode,
                                 int32_t aCompareOffset,
                                 const nsRange& aRange) {
  MOZ_ASSERT(aRange.IsPositioned());
  nsINode* end = aRange.GetEndContainer();
  // If the nodes that we're comparing are not in the same document or in the
  // same subtree, assume that aCompareNode will fall at the end of the ranges.
  if (aCompareNode.GetComposedDoc() != end->GetComposedDoc() ||
      !end->GetComposedDoc() ||
      aCompareNode.SubtreeRoot() != end->SubtreeRoot()) {
    NS_WARNING(
        "`CompareToRangeEnd` couldn't compare nodes, pretending some order.");
    return 1;
  }

  // The points are in the same subtree, hence there has to be an order.
  return *nsContentUtils::ComparePoints(&aCompareNode, aCompareOffset, end,
                                        aRange.EndOffset());
}

// static
int32_t Selection::StyledRanges::FindInsertionPoint(
    const nsTArray<StyledRange>* aElementArray, const nsINode& aPointNode,
    int32_t aPointOffset,
    int32_t (*aComparator)(const nsINode&, int32_t, const nsRange&)) {
  int32_t beginSearch = 0;
  int32_t endSearch = aElementArray->Length();  // one beyond what to check

  if (endSearch) {
    int32_t center = endSearch - 1;  // Check last index, then binary search
    do {
      const nsRange* range = (*aElementArray)[center].mRange;

      int32_t cmp{aComparator(aPointNode, aPointOffset, *range)};

      if (cmp < 0) {  // point < cur
        endSearch = center;
      } else if (cmp > 0) {  // point > cur
        beginSearch = center + 1;
      } else {  // found match, done
        beginSearch = center;
        break;
      }
      center = (endSearch - beginSearch) / 2 + beginSearch;
    } while (endSearch - beginSearch > 0);
  }

  return beginSearch;
}

// Selection::SubtractRange
//
//    A helper function that subtracts aSubtract from aRange, and adds
//    1 or 2 StyledRange objects representing the remaining non-overlapping
//    difference to aOutput. It is assumed that the caller has checked that
//    aRange and aSubtract do indeed overlap

// static
nsresult Selection::StyledRanges::SubtractRange(
    StyledRange& aRange, nsRange& aSubtract, nsTArray<StyledRange>* aOutput) {
  nsRange* range = aRange.mRange;

  if (NS_WARN_IF(!range->IsPositioned())) {
    return NS_ERROR_UNEXPECTED;
  }

  // First we want to compare to the range start
  int32_t cmp{CompareToRangeStart(*range->GetStartContainer(),
                                  range->StartOffset(), aSubtract)};

  // Also, make a comparison to the range end
  int32_t cmp2{CompareToRangeEnd(*range->GetEndContainer(), range->EndOffset(),
                                 aSubtract)};

  // If the existing range left overlaps the new range (aSubtract) then
  // cmp < 0, and cmp2 < 0
  // If it right overlaps the new range then cmp > 0 and cmp2 > 0
  // If it fully contains the new range, then cmp < 0 and cmp2 > 0

  if (cmp2 > 0) {
    // We need to add a new StyledRange to the output, running from
    // the end of aSubtract to the end of range
    ErrorResult error;
    RefPtr<nsRange> postOverlap =
        nsRange::Create(aSubtract.EndRef(), range->EndRef(), error);
    if (NS_WARN_IF(error.Failed())) {
      return error.StealNSResult();
    }
    MOZ_ASSERT(postOverlap);
    if (!postOverlap->Collapsed()) {
      // XXX(Bug 1631371) Check if this should use a fallible operation as it
      // pretended earlier.
      aOutput->InsertElementAt(0, StyledRange(postOverlap));
      (*aOutput)[0].mTextRangeStyle = aRange.mTextRangeStyle;
    }
  }

  if (cmp < 0) {
    // We need to add a new StyledRange to the output, running from
    // the start of the range to the start of aSubtract
    ErrorResult error;
    RefPtr<nsRange> preOverlap =
        nsRange::Create(range->StartRef(), aSubtract.StartRef(), error);
    if (NS_WARN_IF(error.Failed())) {
      return error.StealNSResult();
    }
    MOZ_ASSERT(preOverlap);
    if (!preOverlap->Collapsed()) {
      // XXX(Bug 1631371) Check if this should use a fallible operation as it
      // pretended earlier.
      aOutput->InsertElementAt(0, StyledRange(preOverlap));
      (*aOutput)[0].mTextRangeStyle = aRange.mTextRangeStyle;
    }
  }

  return NS_OK;
}

static void UserSelectRangesToAdd(nsRange* aItem,
                                  nsTArray<RefPtr<nsRange>>& aRangesToAdd) {
  // We cannot directly call IsEditorSelection() because we may be in an
  // inconsistent state during Collapse() (we're cleared already but we haven't
  // got a new focus node yet).
  if (IsEditorNode(aItem->GetStartContainer()) &&
      IsEditorNode(aItem->GetEndContainer())) {
    // Don't mess with the selection ranges for editing, editor doesn't really
    // deal well with multi-range selections.
    aRangesToAdd.AppendElement(aItem);
  } else {
    aItem->ExcludeNonSelectableNodes(&aRangesToAdd);
  }
}

static nsINode* DetermineSelectstartEventTarget(
    const bool aSelectionEventsOnTextControlsEnabled, const nsRange& aRange) {
  nsINode* target = aRange.GetStartContainer();
  if (aSelectionEventsOnTextControlsEnabled) {
    // Get the first element which isn't in a native anonymous subtree
    while (target && target->IsInNativeAnonymousSubtree()) {
      target = target->GetParent();
    }
  } else {
    if (target->IsInNativeAnonymousSubtree()) {
      // This is a selection under a text control, so don't dispatch the
      // event.
      target = nullptr;
    }
  }
  return target;
}

/**
 * @return true, iff the default action should be executed.
 */
static bool MaybeDispatchSelectstartEvent(
    const nsRange& aRange, const bool aSelectionEventsOnTextControlsEnabled,
    Document* aDocument) {
  nsCOMPtr<nsINode> selectstartEventTarget = DetermineSelectstartEventTarget(
      aSelectionEventsOnTextControlsEnabled, aRange);

  bool executeDefaultAction = true;

  if (selectstartEventTarget) {
    nsContentUtils::DispatchTrustedEvent(
        aDocument, selectstartEventTarget, u"selectstart"_ns, CanBubble::eYes,
        Cancelable::eYes, &executeDefaultAction);
  }

  return executeDefaultAction;
}

// static
bool Selection::IsUserSelectionCollapsed(
    const nsRange& aRange, nsTArray<RefPtr<nsRange>>& aTempRangesToAdd) {
  MOZ_ASSERT(aTempRangesToAdd.IsEmpty());

  RefPtr<nsRange> scratchRange = aRange.CloneRange();
  UserSelectRangesToAdd(scratchRange, aTempRangesToAdd);
  const bool userSelectionCollapsed =
      (aTempRangesToAdd.Length() == 0) ||
      ((aTempRangesToAdd.Length() == 1) && aTempRangesToAdd[0]->Collapsed());

  aTempRangesToAdd.ClearAndRetainStorage();

  return userSelectionCollapsed;
}

nsresult Selection::AddRangesForUserSelectableNodes(
    nsRange* aRange, int32_t* aOutIndex,
    const DispatchSelectstartEvent aDispatchSelectstartEvent) {
  MOZ_ASSERT(mUserInitiated);
  MOZ_ASSERT(aOutIndex);

  if (!aRange) {
    return NS_ERROR_NULL_POINTER;
  }

  if (!aRange->IsPositioned()) {
    return NS_ERROR_UNEXPECTED;
  }

  AutoTArray<RefPtr<nsRange>, 4> rangesToAdd;
  *aOutIndex = int32_t(mStyledRanges.Length()) - 1;

  Document* doc = GetDocument();

  if (aDispatchSelectstartEvent == DispatchSelectstartEvent::Maybe &&
      mSelectionType == SelectionType::eNormal && IsCollapsed() &&
      !IsBlockingSelectionChangeEvents()) {
    // We consider a selection to be starting if we are currently collapsed,
    // and the selection is becoming uncollapsed, and this is caused by a
    // user initiated event.

    // First, we generate the ranges to add with a scratch range, which is a
    // clone of the original range passed in. We do this seperately, because
    // the selectstart event could have caused the world to change, and
    // required ranges to be re-generated

    const bool userSelectionCollapsed =
        IsUserSelectionCollapsed(*aRange, rangesToAdd);
    MOZ_ASSERT(userSelectionCollapsed || nsContentUtils::IsSafeToRunScript());
    if (!userSelectionCollapsed && nsContentUtils::IsSafeToRunScript()) {
      // The spec currently doesn't say that we should dispatch this event
      // on text controls, so for now we only support doing that under a
      // pref, disabled by default.
      // See https://github.com/w3c/selection-api/issues/53.
      const bool executeDefaultAction = MaybeDispatchSelectstartEvent(
          *aRange,
          StaticPrefs::dom_select_events_textcontrols_selectstart_enabled(),
          doc);

      if (!executeDefaultAction) {
        return NS_OK;
      }

      // As we potentially dispatched an event to the DOM, something could have
      // changed under our feet. Re-generate the rangesToAdd array, and
      // ensure that the range we are about to add is still valid.
      if (!aRange->IsPositioned()) {
        return NS_ERROR_UNEXPECTED;
      }
    }
  }

  // Generate the ranges to add
  UserSelectRangesToAdd(aRange, rangesToAdd);
  size_t newAnchorFocusIndex =
      GetDirection() == eDirPrevious ? 0 : rangesToAdd.Length() - 1;
  for (size_t i = 0; i < rangesToAdd.Length(); ++i) {
    int32_t index;
    const RefPtr<Selection> selection{this};
    // `MOZ_KnownLive` needed because of broken static analysis
    // (https://bugzilla.mozilla.org/show_bug.cgi?id=1622253#c1).
    nsresult rv = mStyledRanges.MaybeAddRangeAndTruncateOverlaps(
        MOZ_KnownLive(rangesToAdd[i]), &index, *selection);
    NS_ENSURE_SUCCESS(rv, rv);
    if (i == newAnchorFocusIndex) {
      *aOutIndex = index;
      rangesToAdd[i]->SetIsGenerated(false);
    } else {
      rangesToAdd[i]->SetIsGenerated(true);
    }
  }
  return NS_OK;
}

nsresult Selection::AddRangesForSelectableNodes(
    nsRange* aRange, int32_t* aOutIndex,
    const DispatchSelectstartEvent aDispatchSelectstartEvent) {
  if (!aRange) {
    return NS_ERROR_NULL_POINTER;
  }

  if (!aRange->IsPositioned()) {
    return NS_ERROR_UNEXPECTED;
  }

  NS_ASSERTION(aOutIndex, "aOutIndex can't be null");

  MOZ_LOG(
      sSelectionLog, LogLevel::Debug,
      ("%s: selection=%p, type=%i, range=(%p, StartOffset=%u, EndOffset=%u)",
       __FUNCTION__, this, static_cast<int>(GetType()), aRange,
       aRange->StartOffset(), aRange->EndOffset()));

  if (mUserInitiated) {
    return AddRangesForUserSelectableNodes(aRange, aOutIndex,
                                           aDispatchSelectstartEvent);
  }

  const RefPtr<Selection> selection{this};
  return mStyledRanges.MaybeAddRangeAndTruncateOverlaps(aRange, aOutIndex,
                                                        *selection);
}

nsresult Selection::StyledRanges::MaybeAddRangeAndTruncateOverlaps(
    nsRange* aRange, int32_t* aOutIndex, Selection& aSelection) {
  MOZ_ASSERT(aRange);
  MOZ_ASSERT(aRange->IsPositioned());
  MOZ_ASSERT(aOutIndex);

  *aOutIndex = -1;

  // a common case is that we have no ranges yet
  if (mRanges.Length() == 0) {
    // XXX(Bug 1631371) Check if this should use a fallible operation as it
    // pretended earlier.
    mRanges.AppendElement(StyledRange(aRange));
    aRange->RegisterSelection(aSelection);

    *aOutIndex = 0;
    return NS_OK;
  }

  int32_t startIndex, endIndex;
  nsresult rv =
      GetIndicesForInterval(aRange->GetStartContainer(), aRange->StartOffset(),
                            aRange->GetEndContainer(), aRange->EndOffset(),
                            false, startIndex, endIndex);
  NS_ENSURE_SUCCESS(rv, rv);

  if (endIndex == -1) {
    // All ranges start after the given range. We can insert our range at
    // position 0, knowing there are no overlaps (handled below)
    startIndex = 0;
    endIndex = 0;
  } else if (startIndex == -1) {
    // All ranges end before the given range. We can insert our range at
    // the end of the array, knowing there are no overlaps (handled below)
    startIndex = mRanges.Length();
    endIndex = startIndex;
  }

  // If the range is already contained in mRanges, silently
  // succeed
  const bool sameRange = HasEqualRangeBoundariesAt(*aRange, startIndex);
  if (sameRange) {
    *aOutIndex = startIndex;
    return NS_OK;
  }

  if (startIndex == endIndex) {
    // The new range doesn't overlap any existing ranges
    // XXX(Bug 1631371) Check if this should use a fallible operation as it
    // pretended earlier.
    mRanges.InsertElementAt(startIndex, StyledRange(aRange));
    aRange->RegisterSelection(aSelection);
    *aOutIndex = startIndex;
    return NS_OK;
  }

  // We now know that at least 1 existing range overlaps with the range that
  // we are trying to add. In fact, the only ranges of interest are those at
  // the two end points, startIndex and endIndex - 1 (which may point to the
  // same range) as these may partially overlap the new range. Any ranges
  // between these indices are fully overlapped by the new range, and so can be
  // removed
  nsTArray<StyledRange> overlaps;
  // XXX(Bug 1631371) Check if this should use a fallible operation as it
  // pretended earlier.
  overlaps.InsertElementAt(0, mRanges[startIndex]);

  if (endIndex - 1 != startIndex) {
    // XXX(Bug 1631371) Check if this should use a fallible operation as it
    // pretended earlier.
    overlaps.InsertElementAt(1, mRanges[endIndex - 1]);
  }

  // Remove all the overlapping ranges
  for (int32_t i = startIndex; i < endIndex; ++i) {
    mRanges[i].mRange->UnregisterSelection();
  }
  mRanges.RemoveElementsAt(startIndex, endIndex - startIndex);

  nsTArray<StyledRange> temp;
  for (int32_t i = overlaps.Length() - 1; i >= 0; i--) {
    nsresult rv = SubtractRange(overlaps[i], *aRange, &temp);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Insert the new element into our "leftovers" array
  // `aRange` is positioned, so it has to have a start container.
  int32_t insertionPoint{FindInsertionPoint(&temp, *aRange->GetStartContainer(),
                                            aRange->StartOffset(),
                                            CompareToRangeStart)};

  // XXX(Bug 1631371) Check if this should use a fallible operation as it
  // pretended earlier.
  temp.InsertElementAt(insertionPoint, StyledRange(aRange));

  // Merge the leftovers back in to mRanges
  // XXX(Bug 1631371) Check if this should use a fallible operation as it
  // pretended earlier.
  mRanges.InsertElementsAt(startIndex, temp);

  for (uint32_t i = 0; i < temp.Length(); ++i) {
    MOZ_KnownLive(temp[i].mRange)->RegisterSelection(aSelection);
    // `MOZ_KnownLive` is required because of
    // https://bugzilla.mozilla.org/show_bug.cgi?id=1622253.
  }

  *aOutIndex = startIndex + insertionPoint;
  return NS_OK;
}

nsresult Selection::StyledRanges::RemoveRangeAndUnregisterSelection(
    nsRange& aRange) {
  // Find the range's index & remove it. We could use FindInsertionPoint to
  // get O(log n) time, but that requires many expensive DOM comparisons.
  // For even several thousand items, this is probably faster because the
  // comparisons are so fast.
  int32_t idx = -1;
  uint32_t i;
  for (i = 0; i < mRanges.Length(); i++) {
    if (mRanges[i].mRange == &aRange) {
      idx = (int32_t)i;
      break;
    }
  }
  if (idx < 0) return NS_ERROR_DOM_NOT_FOUND_ERR;

  mRanges.RemoveElementAt(idx);
  aRange.UnregisterSelection();
  return NS_OK;
}
nsresult Selection::RemoveCollapsedRanges() {
  return mStyledRanges.RemoveCollapsedRanges();
}

nsresult Selection::StyledRanges::RemoveCollapsedRanges() {
  uint32_t i = 0;
  while (i < mRanges.Length()) {
    if (mRanges[i].mRange->Collapsed()) {
      nsresult rv = RemoveRangeAndUnregisterSelection(*mRanges[i].mRange);
      NS_ENSURE_SUCCESS(rv, rv);
    } else {
      ++i;
    }
  }
  return NS_OK;
}

void Selection::Clear(nsPresContext* aPresContext) {
  SetAnchorFocusRange(-1);

  mStyledRanges.UnregisterSelection();
  for (uint32_t i = 0; i < mStyledRanges.Length(); ++i) {
    SelectFrames(aPresContext, mStyledRanges.mRanges[i].mRange, false);
  }
  mStyledRanges.Clear();

  // Reset direction so for more dependable table selection range handling
  SetDirection(eDirNext);

  // If this was an ATTENTION selection, change it back to normal now
  if (mFrameSelection && mFrameSelection->GetDisplaySelection() ==
                             nsISelectionController::SELECTION_ATTENTION) {
    mFrameSelection->SetDisplaySelection(nsISelectionController::SELECTION_ON);
  }
}

bool Selection::StyledRanges::HasEqualRangeBoundariesAt(
    const nsRange& aRange, int32_t aRangeIndex) const {
  if (aRangeIndex >= 0 && aRangeIndex < (int32_t)mRanges.Length()) {
    const nsRange* range = mRanges[aRangeIndex].mRange;
    return range->HasEqualBoundaries(aRange);
  }
  return false;
}

void Selection::GetRangesForInterval(nsINode& aBeginNode, int32_t aBeginOffset,
                                     nsINode& aEndNode, int32_t aEndOffset,
                                     bool aAllowAdjacent,
                                     nsTArray<RefPtr<nsRange>>& aReturn,
                                     mozilla::ErrorResult& aRv) {
  nsTArray<nsRange*> results;
  nsresult rv = GetRangesForIntervalArray(&aBeginNode, aBeginOffset, &aEndNode,
                                          aEndOffset, aAllowAdjacent, &results);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return;
  }

  aReturn.SetLength(results.Length());
  for (uint32_t i = 0; i < results.Length(); ++i) {
    aReturn[i] = results[i];  // AddRefs
  }
}

nsresult Selection::GetRangesForIntervalArray(
    nsINode* aBeginNode, int32_t aBeginOffset, nsINode* aEndNode,
    int32_t aEndOffset, bool aAllowAdjacent, nsTArray<nsRange*>* aRanges) {
  if (NS_WARN_IF(!aBeginNode)) {
    return NS_ERROR_UNEXPECTED;
  }

  if (NS_WARN_IF(!aEndNode)) {
    return NS_ERROR_UNEXPECTED;
  }

  aRanges->Clear();
  int32_t startIndex, endIndex;
  nsresult res = mStyledRanges.GetIndicesForInterval(
      aBeginNode, aBeginOffset, aEndNode, aEndOffset, aAllowAdjacent,
      startIndex, endIndex);
  NS_ENSURE_SUCCESS(res, res);

  if (startIndex == -1 || endIndex == -1) return NS_OK;

  for (int32_t i = startIndex; i < endIndex; i++) {
    // XXX(Bug 1631371) Check if this should use a fallible operation as it
    // pretended earlier.
    aRanges->AppendElement(mStyledRanges.mRanges[i].mRange);
  }

  return NS_OK;
}

nsresult Selection::StyledRanges::GetIndicesForInterval(
    const nsINode* aBeginNode, int32_t aBeginOffset, const nsINode* aEndNode,
    int32_t aEndOffset, bool aAllowAdjacent, int32_t& aStartIndex,
    int32_t& aEndIndex) const {
  if (NS_WARN_IF(!aBeginNode)) {
    return NS_ERROR_INVALID_POINTER;
  }

  if (NS_WARN_IF(!aEndNode)) {
    return NS_ERROR_INVALID_POINTER;
  }

  aStartIndex = -1;
  aEndIndex = -1;

  if (mRanges.Length() == 0) return NS_OK;

  const bool intervalIsCollapsed =
      aBeginNode == aEndNode && aBeginOffset == aEndOffset;

  // Ranges that end before the given interval and begin after the given
  // interval can be discarded
  int32_t endsBeforeIndex{FindInsertionPoint(&mRanges, *aEndNode, aEndOffset,
                                             &CompareToRangeStart)};

  if (endsBeforeIndex == 0) {
    const nsRange* endRange = mRanges[endsBeforeIndex].mRange;

    // If the interval is strictly before the range at index 0, we can optimize
    // by returning now - all ranges start after the given interval
    if (!endRange->StartRef().Equals(aEndNode, aEndOffset)) {
      return NS_OK;
    }

    // We now know that the start point of mRanges[0].mRange
    // equals the end of the interval. Thus, when aAllowadjacent is true, the
    // caller is always interested in this range. However, when excluding
    // adjacencies, we must remember to include the range when both it and the
    // given interval are collapsed to the same point
    if (!aAllowAdjacent && !(endRange->Collapsed() && intervalIsCollapsed))
      return NS_OK;
  }
  aEndIndex = endsBeforeIndex;

  int32_t beginsAfterIndex{FindInsertionPoint(
      &mRanges, *aBeginNode, aBeginOffset, &CompareToRangeEnd)};

  if (beginsAfterIndex == (int32_t)mRanges.Length())
    return NS_OK;  // optimization: all ranges are strictly before us

  if (aAllowAdjacent) {
    // At this point, one of the following holds:
    //   endsBeforeIndex == mRanges.Length(),
    //   endsBeforeIndex points to a range whose start point does not equal the
    //     given interval's start point
    //   endsBeforeIndex points to a range whose start point equals the given
    //     interval's start point
    // In the final case, there can be two such ranges, a collapsed range, and
    // an adjacent range (they will appear in mRanges in that
    // order). For this final case, we need to increment endsBeforeIndex, until
    // one of the first two possibilites hold
    while (endsBeforeIndex < (int32_t)mRanges.Length()) {
      const nsRange* endRange = mRanges[endsBeforeIndex].mRange;
      if (!endRange->StartRef().Equals(aEndNode, aEndOffset)) {
        break;
      }
      endsBeforeIndex++;
    }

    // Likewise, one of the following holds:
    //   beginsAfterIndex == 0,
    //   beginsAfterIndex points to a range whose end point does not equal
    //     the given interval's end point
    //   beginsOnOrAfter points to a range whose end point equals the given
    //     interval's end point
    // In the final case, there can be two such ranges, an adjacent range, and
    // a collapsed range (they will appear in mRanges in that
    // order). For this final case, we only need to take action if both those
    // ranges exist, and we are pointing to the collapsed range - we need to
    // point to the adjacent range
    const nsRange* beginRange = mRanges[beginsAfterIndex].mRange;
    if (beginsAfterIndex > 0 && beginRange->Collapsed() &&
        beginRange->EndRef().Equals(aBeginNode, aBeginOffset)) {
      beginRange = mRanges[beginsAfterIndex - 1].mRange;
      if (beginRange->EndRef().Equals(aBeginNode, aBeginOffset)) {
        beginsAfterIndex--;
      }
    }
  } else {
    // See above for the possibilities at this point. The only case where we
    // need to take action is when the range at beginsAfterIndex ends on
    // the given interval's start point, but that range isn't collapsed (a
    // collapsed range should be included in the returned results).
    const nsRange* beginRange = mRanges[beginsAfterIndex].mRange;
    if (beginRange->EndRef().Equals(aBeginNode, aBeginOffset) &&
        !beginRange->Collapsed()) {
      beginsAfterIndex++;
    }

    // Again, see above for the meaning of endsBeforeIndex at this point.
    // In particular, endsBeforeIndex may point to a collaped range which
    // represents the point at the end of the interval - this range should be
    // included
    if (endsBeforeIndex < (int32_t)mRanges.Length()) {
      const nsRange* endRange = mRanges[endsBeforeIndex].mRange;
      if (endRange->StartRef().Equals(aEndNode, aEndOffset) &&
          endRange->Collapsed()) {
        endsBeforeIndex++;
      }
    }
  }

  NS_ASSERTION(beginsAfterIndex <= endsBeforeIndex, "Is mRanges not ordered?");
  NS_ENSURE_STATE(beginsAfterIndex <= endsBeforeIndex);

  aStartIndex = beginsAfterIndex;
  aEndIndex = endsBeforeIndex;
  return NS_OK;
}

nsIFrame* Selection::GetPrimaryFrameForAnchorNode() const {
  MOZ_ASSERT(mSelectionType == SelectionType::eNormal);

  int32_t frameOffset = 0;
  nsCOMPtr<nsIContent> content = do_QueryInterface(GetAnchorNode());
  if (content && mFrameSelection) {
    return nsFrameSelection::GetFrameForNodeOffset(
        content, AnchorOffset(), mFrameSelection->GetHint(), &frameOffset);
  }
  return nullptr;
}

nsIFrame* Selection::GetPrimaryFrameForFocusNode(bool aVisual,
                                                 int32_t* aOffsetUsed) const {
  nsINode* focusNode = GetFocusNode();
  if (!focusNode || !focusNode->IsContent() || !mFrameSelection) {
    return nullptr;
  }

  nsCOMPtr<nsIContent> content = focusNode->AsContent();
  int32_t frameOffset = 0;
  if (!aOffsetUsed) {
    aOffsetUsed = &frameOffset;
  }

  nsIFrame* frame = GetPrimaryOrCaretFrameForNodeOffset(content, FocusOffset(),
                                                        aOffsetUsed, aVisual);
  if (frame) {
    return frame;
  }

  // If content is whitespace only, we promote focus node to parent because
  // whitespace only node might have no frame.

  if (!content->TextIsOnlyWhitespace()) {
    return nullptr;
  }

  nsCOMPtr<nsIContent> parent = content->GetParent();
  if (NS_WARN_IF(!parent)) {
    return nullptr;
  }
  int32_t offset = parent->ComputeIndexOf(content);

  return GetPrimaryOrCaretFrameForNodeOffset(parent, offset, aOffsetUsed,
                                             aVisual);
}

nsIFrame* Selection::GetPrimaryOrCaretFrameForNodeOffset(nsIContent* aContent,
                                                         uint32_t aOffset,
                                                         int32_t* aOffsetUsed,
                                                         bool aVisual) const {
  MOZ_ASSERT(aOffsetUsed);

  if (!mFrameSelection) {
    return nullptr;
  }

  CaretAssociationHint hint = mFrameSelection->GetHint();

  if (aVisual) {
    mozilla::intl::BidiEmbeddingLevel caretBidiLevel =
        mFrameSelection->GetCaretBidiLevel();

    return nsCaret::GetCaretFrameForNodeOffset(
        mFrameSelection, aContent, aOffset, hint, caretBidiLevel,
        /* aReturnUnadjustedFrame = */ nullptr, aOffsetUsed);
  }

  return nsFrameSelection::GetFrameForNodeOffset(aContent, aOffset, hint,
                                                 aOffsetUsed);
}

void Selection::SelectFramesOf(nsIContent* aContent, bool aSelected) const {
  nsIFrame* frame = aContent->GetPrimaryFrame();
  if (!frame) {
    return;
  }
  // The frame could be an SVG text frame, in which case we don't treat it
  // as a text frame.
  if (frame->IsTextFrame()) {
    nsTextFrame* textFrame = static_cast<nsTextFrame*>(frame);
    textFrame->SelectionStateChanged(0, textFrame->TextFragment()->GetLength(),
                                     aSelected, mSelectionType);
  } else {
    frame->SelectionStateChanged();
  }
}

nsresult Selection::SelectFramesOfInclusiveDescendantsOfContent(
    PostContentIterator& aPostOrderIter, nsIContent* aContent,
    bool aSelected) const {
  // If aContent doesn't have children, we should avoid to use the content
  // iterator for performance reason.
  if (!aContent->HasChildren()) {
    SelectFramesOf(aContent, aSelected);
    return NS_OK;
  }

  if (NS_WARN_IF(NS_FAILED(aPostOrderIter.Init(aContent)))) {
    return NS_ERROR_FAILURE;
  }

  for (; !aPostOrderIter.IsDone(); aPostOrderIter.Next()) {
    nsINode* node = aPostOrderIter.GetCurrentNode();
    MOZ_ASSERT(node);
    nsIContent* innercontent = node->IsContent() ? node->AsContent() : nullptr;
    SelectFramesOf(innercontent, aSelected);
  }

  return NS_OK;
}

void Selection::SelectFramesInAllRanges(nsPresContext* aPresContext) {
  for (size_t i = 0; i < mStyledRanges.Length(); ++i) {
    nsRange* range = mStyledRanges.mRanges[i].mRange;
    MOZ_ASSERT(range->IsInSelection());
    SelectFrames(aPresContext, range, range->IsInSelection());
  }
}

/**
 * The idea of this helper method is to select or deselect "top to bottom",
 * traversing through the frames
 */
nsresult Selection::SelectFrames(nsPresContext* aPresContext, nsRange* aRange,
                                 bool aSelect) const {
  if (!mFrameSelection || !aPresContext || !aPresContext->GetPresShell()) {
    // nothing to do
    return NS_OK;
  }
  MOZ_ASSERT(aRange && aRange->IsPositioned());

  if (mFrameSelection->IsInTableSelectionMode()) {
    nsINode* node = aRange->GetClosestCommonInclusiveAncestor();
    nsIFrame* frame = node->IsContent()
                          ? node->AsContent()->GetPrimaryFrame()
                          : aPresContext->PresShell()->GetRootFrame();
    if (frame) {
      if (frame->IsTextFrame()) {
        nsTextFrame* textFrame = static_cast<nsTextFrame*>(frame);

        MOZ_ASSERT(node == aRange->GetStartContainer());
        MOZ_ASSERT(node == aRange->GetEndContainer());
        textFrame->SelectionStateChanged(aRange->StartOffset(),
                                         aRange->EndOffset(), aSelect,
                                         mSelectionType);
      } else {
        frame->SelectionStateChanged();
      }
    }

    return NS_OK;
  }

  // Loop through the content iterator for each content node; for each text
  // node, call SetSelected on it:
  nsINode* startNode = aRange->GetStartContainer();
  nsIContent* startContent =
      startNode->IsContent() ? startNode->AsContent() : nullptr;
  if (!startContent) {
    // Don't warn, bug 1055722
    // XXX The range can start from a document node and such range can be
    //     added to Selection with JS.  Therefore, even in such cases,
    //     shouldn't we handle selection in the range?
    return NS_ERROR_UNEXPECTED;
  }

  // We must call first one explicitly
  bool isFirstContentTextNode = startContent->IsText();
  nsINode* endNode = aRange->GetEndContainer();
  if (isFirstContentTextNode) {
    nsIFrame* frame = startContent->GetPrimaryFrame();
    // The frame could be an SVG text frame, in which case we don't treat it
    // as a text frame.
    if (frame) {
      if (frame->IsTextFrame()) {
        nsTextFrame* textFrame = static_cast<nsTextFrame*>(frame);
        uint32_t startOffset = aRange->StartOffset();
        uint32_t endOffset;
        if (endNode == startContent) {
          endOffset = aRange->EndOffset();
        } else {
          endOffset = startContent->Length();
        }
        textFrame->SelectionStateChanged(startOffset, endOffset, aSelect,
                                         mSelectionType);
      } else {
        frame->SelectionStateChanged();
      }
    }
  }

  // If the range is in a node and the node is a leaf node, we don't need to
  // walk the subtree.
  if (aRange->Collapsed() ||
      (startNode == endNode && !startNode->HasChildren())) {
    if (!isFirstContentTextNode) {
      SelectFramesOf(startContent, aSelect);
    }
    return NS_OK;
  }

  ContentSubtreeIterator subtreeIter;
  subtreeIter.Init(aRange);
  if (isFirstContentTextNode && !subtreeIter.IsDone() &&
      subtreeIter.GetCurrentNode() == startNode) {
    subtreeIter.Next();  // first content has already been handled.
  }
  PostContentIterator postOrderIter;
  for (; !subtreeIter.IsDone(); subtreeIter.Next()) {
    nsINode* node = subtreeIter.GetCurrentNode();
    MOZ_ASSERT(node);
    nsIContent* content = node->IsContent() ? node->AsContent() : nullptr;
    SelectFramesOfInclusiveDescendantsOfContent(postOrderIter, content,
                                                aSelect);
  }

  // We must now do the last one if it is not the same as the first
  if (endNode != startNode) {
    nsIContent* endContent =
        endNode->IsContent() ? endNode->AsContent() : nullptr;
    // XXX The range can end at a document node and such range can be
    //     added to Selection with JS.  Therefore, even in such cases,
    //     shouldn't we handle selection in the range?
    if (NS_WARN_IF(!endContent)) {
      return NS_ERROR_UNEXPECTED;
    }
    if (endContent->IsText()) {
      nsIFrame* frame = endContent->GetPrimaryFrame();
      // The frame could be an SVG text frame, in which case we'll ignore it.
      if (frame && frame->IsTextFrame()) {
        nsTextFrame* textFrame = static_cast<nsTextFrame*>(frame);
        textFrame->SelectionStateChanged(0, aRange->EndOffset(), aSelect,
                                         mSelectionType);
      }
    }
  }
  return NS_OK;
}

// Selection::LookUpSelection
//
//    This function is called when a node wants to know where the selection is
//    over itself.
//
//    Usually, this is called when we already know there is a selection over
//    the node in question, and we only need to find the boundaries of it on
//    that node. This is when slowCheck is false--a strict test is not needed.
//    Other times, the caller has no idea, and wants us to test everything,
//    so we are supposed to determine whether there is a selection over the
//    node at all.
//
//    A previous version of this code used this flag to do less work when
//    inclusion was already known (slowCheck=false). However, our tree
//    structure allows us to quickly determine ranges overlapping the node,
//    so we just ignore the slowCheck flag and do the full test every time.
//
//    PERFORMANCE: a common case is that we are doing a fast check with exactly
//    one range in the selection. In this case, this function is slower than
//    brute force because of the overhead of checking the tree. We can optimize
//    this case to make it faster by doing the same thing the previous version
//    of this function did in the case of 1 range. This would also mean that
//    the aSlowCheck flag would have meaning again.

UniquePtr<SelectionDetails> Selection::LookUpSelection(
    nsIContent* aContent, int32_t aContentOffset, int32_t aContentLength,
    UniquePtr<SelectionDetails> aDetailsHead, SelectionType aSelectionType,
    bool aSlowCheck) {
  if (!aContent) {
    return aDetailsHead;
  }

  // it is common to have no ranges, to optimize that
  if (mStyledRanges.Length() == 0) {
    return aDetailsHead;
  }

  nsTArray<nsRange*> overlappingRanges;
  nsresult rv = GetRangesForIntervalArray(aContent, aContentOffset, aContent,
                                          aContentOffset + aContentLength,
                                          false, &overlappingRanges);
  if (NS_FAILED(rv)) {
    return aDetailsHead;
  }

  if (overlappingRanges.Length() == 0) {
    return aDetailsHead;
  }

  UniquePtr<SelectionDetails> detailsHead = std::move(aDetailsHead);

  for (uint32_t i = 0; i < overlappingRanges.Length(); i++) {
    nsRange* range = overlappingRanges[i];
    nsINode* startNode = range->GetStartContainer();
    nsINode* endNode = range->GetEndContainer();
    int32_t startOffset = range->StartOffset();
    int32_t endOffset = range->EndOffset();

    int32_t start = -1, end = -1;
    if (startNode == aContent && endNode == aContent) {
      if (startOffset < (aContentOffset + aContentLength) &&
          endOffset > aContentOffset) {
        // this range is totally inside the requested content range
        start = std::max(0, startOffset - aContentOffset);
        end = std::min(aContentLength, endOffset - aContentOffset);
      }
      // otherwise, range is inside the requested node, but does not intersect
      // the requested content range, so ignore it
    } else if (startNode == aContent) {
      if (startOffset < (aContentOffset + aContentLength)) {
        // the beginning of the range is inside the requested node, but the
        // end is outside, select everything from there to the end
        start = std::max(0, startOffset - aContentOffset);
        end = aContentLength;
      }
    } else if (endNode == aContent) {
      if (endOffset > aContentOffset) {
        // the end of the range is inside the requested node, but the beginning
        // is outside, select everything from the beginning to there
        start = 0;
        end = std::min(aContentLength, endOffset - aContentOffset);
      }
    } else {
      // this range does not begin or end in the requested node, but since
      // GetRangesForInterval returned this range, we know it overlaps.
      // Therefore, this node is enclosed in the range, and we select all
      // of it.
      start = 0;
      end = aContentLength;
    }
    if (start < 0) continue;  // the ranges do not overlap the input range

    auto newHead = MakeUnique<SelectionDetails>();

    newHead->mNext = std::move(detailsHead);
    newHead->mStart = start;
    newHead->mEnd = end;
    newHead->mSelectionType = aSelectionType;
    StyledRange* rd = mStyledRanges.FindRangeData(range);
    if (rd) {
      newHead->mTextRangeStyle = rd->mTextRangeStyle;
    }
    detailsHead = std::move(newHead);
  }
  return detailsHead;
}

NS_IMETHODIMP
Selection::Repaint(nsPresContext* aPresContext) {
  int32_t arrCount = (int32_t)mStyledRanges.Length();

  if (arrCount < 1) return NS_OK;

  int32_t i;

  for (i = 0; i < arrCount; i++) {
    nsresult rv =
        SelectFrames(aPresContext, mStyledRanges.mRanges[i].mRange, true);

    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  return NS_OK;
}

void Selection::SetCanCacheFrameOffset(bool aCanCacheFrameOffset) {
  if (!mCachedOffsetForFrame) {
    mCachedOffsetForFrame = new CachedOffsetForFrame;
  }

  mCachedOffsetForFrame->mCanCacheFrameOffset = aCanCacheFrameOffset;

  // clean up cached frame when turn off cache
  // fix bug 207936
  if (!aCanCacheFrameOffset) {
    mCachedOffsetForFrame->mLastCaretFrame = nullptr;
  }
}

nsresult Selection::GetCachedFrameOffset(nsIFrame* aFrame, int32_t inOffset,
                                         nsPoint& aPoint) {
  if (!mCachedOffsetForFrame) {
    mCachedOffsetForFrame = new CachedOffsetForFrame;
  }

  nsresult rv = NS_OK;
  if (mCachedOffsetForFrame->mCanCacheFrameOffset &&
      mCachedOffsetForFrame->mLastCaretFrame &&
      (aFrame == mCachedOffsetForFrame->mLastCaretFrame) &&
      (inOffset == mCachedOffsetForFrame->mLastContentOffset)) {
    // get cached frame offset
    aPoint = mCachedOffsetForFrame->mCachedFrameOffset;
  } else {
    // Recalculate frame offset and cache it. Don't cache a frame offset if
    // GetPointFromOffset fails, though.
    rv = aFrame->GetPointFromOffset(inOffset, &aPoint);
    if (NS_SUCCEEDED(rv) && mCachedOffsetForFrame->mCanCacheFrameOffset) {
      mCachedOffsetForFrame->mCachedFrameOffset = aPoint;
      mCachedOffsetForFrame->mLastCaretFrame = aFrame;
      mCachedOffsetForFrame->mLastContentOffset = inOffset;
    }
  }

  return rv;
}

nsIContent* Selection::GetAncestorLimiter() const {
  MOZ_ASSERT(mSelectionType == SelectionType::eNormal);

  if (mFrameSelection) {
    return mFrameSelection->GetAncestorLimiter();
  }
  return nullptr;
}

void Selection::SetAncestorLimiter(nsIContent* aLimiter) {
  MOZ_ASSERT(mSelectionType == SelectionType::eNormal);

  if (mFrameSelection) {
    RefPtr<nsFrameSelection> frameSelection = mFrameSelection;
    frameSelection->SetAncestorLimiter(aLimiter);
  }
}

void Selection::StyledRanges::UnregisterSelection() {
  uint32_t count = mRanges.Length();
  for (uint32_t i = 0; i < count; ++i) {
    mRanges[i].mRange->UnregisterSelection();
  }
}

void Selection::StyledRanges::Clear() { mRanges.Clear(); }

StyledRange* Selection::StyledRanges::FindRangeData(nsRange* aRange) {
  NS_ENSURE_TRUE(aRange, nullptr);
  for (uint32_t i = 0; i < mRanges.Length(); i++) {
    if (mRanges[i].mRange == aRange) {
      return &mRanges[i];
    }
  }
  return nullptr;
}

Selection::StyledRanges::Elements::size_type Selection::StyledRanges::Length()
    const {
  return mRanges.Length();
}

nsresult Selection::SetTextRangeStyle(nsRange* aRange,
                                      const TextRangeStyle& aTextRangeStyle) {
  NS_ENSURE_ARG_POINTER(aRange);
  StyledRange* rd = mStyledRanges.FindRangeData(aRange);
  if (rd) {
    rd->mTextRangeStyle = aTextRangeStyle;
  }
  return NS_OK;
}

nsresult Selection::StartAutoScrollTimer(nsIFrame* aFrame,
                                         const nsPoint& aPoint,
                                         uint32_t aDelayInMs) {
  MOZ_ASSERT(aFrame, "Need a frame");
  MOZ_ASSERT(mSelectionType == SelectionType::eNormal);

  if (!mFrameSelection) {
    return NS_OK;  // nothing to do
  }

  if (!mAutoScroller) {
    mAutoScroller = new AutoScroller(mFrameSelection);
  }

  mAutoScroller->SetDelay(aDelayInMs);

  RefPtr<AutoScroller> autoScroller{mAutoScroller};
  return autoScroller->DoAutoScroll(aFrame, aPoint);
}

nsresult Selection::StopAutoScrollTimer() {
  MOZ_ASSERT(mSelectionType == SelectionType::eNormal);

  if (mAutoScroller) {
    mAutoScroller->Stop(AutoScroller::FurtherScrollingAllowed::kYes);
  }

  return NS_OK;
}

nsresult AutoScroller::DoAutoScroll(nsIFrame* aFrame, nsPoint aPoint) {
  MOZ_ASSERT(aFrame, "Need a frame");

  Stop(FurtherScrollingAllowed::kYes);

  nsPresContext* presContext = aFrame->PresContext();
  RefPtr<PresShell> presShell = presContext->PresShell();
  nsRootPresContext* rootPC = presContext->GetRootPresContext();
  if (!rootPC) return NS_OK;
  nsIFrame* rootmostFrame = rootPC->PresShell()->GetRootFrame();
  AutoWeakFrame weakRootFrame(rootmostFrame);
  AutoWeakFrame weakFrame(aFrame);
  // Get the point relative to the root most frame because the scroll we are
  // about to do will change the coordinates of aFrame.
  nsPoint globalPoint = aPoint + aFrame->GetOffsetToCrossDoc(rootmostFrame);

  bool done = false;
  bool didScroll;
  while (true) {
    didScroll = presShell->ScrollFrameRectIntoView(
        aFrame, nsRect(aPoint, nsSize(0, 0)), nsMargin(), ScrollAxis(),
        ScrollAxis(), ScrollFlags::None);
    if (!weakFrame || !weakRootFrame) {
      return NS_OK;
    }
    if (!didScroll && !done) {
      // If aPoint is at the screen edge then try to scroll anyway, once.
      RefPtr<nsDeviceContext> dx =
          presShell->GetViewManager()->GetDeviceContext();
      nsRect screen;
      dx->GetRect(screen);
      nsPoint screenPoint =
          globalPoint + rootmostFrame->GetScreenRectInAppUnits().TopLeft();
      nscoord onePx = AppUnitsPerCSSPixel();
      nscoord scrollAmount = 10 * onePx;
      if (std::abs(screen.x - screenPoint.x) <= onePx) {
        aPoint.x -= scrollAmount;
      } else if (std::abs(screen.XMost() - screenPoint.x) <= onePx) {
        aPoint.x += scrollAmount;
      } else if (std::abs(screen.y - screenPoint.y) <= onePx) {
        aPoint.y -= scrollAmount;
      } else if (std::abs(screen.YMost() - screenPoint.y) <= onePx) {
        aPoint.y += scrollAmount;
      } else {
        break;
      }
      done = true;
      continue;
    }
    break;
  }

  // Start the AutoScroll timer if necessary.
  // `ScrollFrameRectIntoView` above may have run script and this may have
  // forbidden to continue scrolling.
  if (didScroll &&
      (mFurtherScrollingAllowed == FurtherScrollingAllowed::kYes)) {
    nsPoint presContextPoint =
        globalPoint -
        presShell->GetRootFrame()->GetOffsetToCrossDoc(rootmostFrame);
    ScheduleNextDoAutoScroll(presContext, presContextPoint);
  }

  return NS_OK;
}

void Selection::RemoveAllRanges(ErrorResult& aRv) {
  if (!mFrameSelection) {
    aRv.Throw(NS_ERROR_NOT_INITIALIZED);
    return;
  }

  RefPtr<nsPresContext> presContext = GetPresContext();
  Clear(presContext);

  // Turn off signal for table selection
  RefPtr<nsFrameSelection> frameSelection = mFrameSelection;
  frameSelection->ClearTableCellSelection();

  RefPtr<Selection> kungFuDeathGrip{this};
  // Be aware, this instance may be destroyed after this call.
  NotifySelectionListeners();
}

void Selection::AddRangeJS(nsRange& aRange, ErrorResult& aRv) {
  AutoRestore<bool> calledFromJSRestorer(mCalledByJS);
  mCalledByJS = true;
  AddRangeAndSelectFramesAndNotifyListeners(aRange, aRv);
}

void Selection::AddRangeAndSelectFramesAndNotifyListeners(nsRange& aRange,
                                                          ErrorResult& aRv) {
  RefPtr<Document> document(GetDocument());
  return AddRangeAndSelectFramesAndNotifyListeners(aRange, document, aRv);
}

void Selection::AddRangeAndSelectFramesAndNotifyListeners(nsRange& aRange,
                                                          Document* aDocument,
                                                          ErrorResult& aRv) {
  // If the given range is part of another Selection, we need to clone the
  // range first.
  RefPtr<nsRange> range;
  if (aRange.IsInSelection() && aRange.GetSelection() != this) {
    // Because of performance reason, when there is a cached range, let's use
    // it.  Otherwise, clone the range.
    range = aRange.CloneRange();
  } else {
    range = &aRange;
  }

  nsINode* rangeRoot = range->GetRoot();
  if (aDocument != rangeRoot &&
      (!rangeRoot || aDocument != rangeRoot->GetComposedDoc())) {
    // http://w3c.github.io/selection-api/#dom-selection-addrange
    // "...  if the root of the range's boundary points are the document
    // associated with context object. Otherwise, this method must do nothing."
    return;
  }

  // MaybeAddTableCellRange might flush frame and `NotifySelectionListeners`
  // below might destruct `this`.
  RefPtr<Selection> kungFuDeathGrip(this);

  // This inserts a table cell range in proper document order
  // and returns NS_OK if range doesn't contain just one table cell
  bool didAddRange;
  int32_t rangeIndex;
  nsresult result = MaybeAddTableCellRange(*range, &didAddRange, &rangeIndex);
  if (NS_FAILED(result)) {
    aRv.Throw(result);
    return;
  }

  if (!didAddRange) {
    result = AddRangesForSelectableNodes(range, &rangeIndex,
                                         DispatchSelectstartEvent::Maybe);
    if (NS_FAILED(result)) {
      aRv.Throw(result);
      return;
    }
  }

  if (rangeIndex < 0) {
    return;
  }

  MOZ_ASSERT(rangeIndex < static_cast<int32_t>(mStyledRanges.Length()));

  SetAnchorFocusRange(rangeIndex);

  // Make sure the caret appears on the next line, if at a newline
  if (mSelectionType == SelectionType::eNormal) {
    SetInterlinePosition(true, IgnoreErrors());
  }

  if (!mFrameSelection) {
    return;  // nothing to do
  }

  RefPtr<nsPresContext> presContext = GetPresContext();
  SelectFrames(presContext, range, true);

  // Be aware, this instance may be destroyed after this call.
  NotifySelectionListeners();
}

// Selection::RemoveRangeAndUnselectFramesAndNotifyListeners
//
//    Removes the given range from the selection. The tricky part is updating
//    the flags on the frames that indicate whether they have a selection or
//    not. There could be several selection ranges on the frame, and clearing
//    the bit would cause the selection to not be drawn, even when there is
//    another range on the frame (bug 346185).
//
//    We therefore find any ranges that intersect the same nodes as the range
//    being removed, and cause them to set the selected bits back on their
//    selected frames after we've cleared the bit from ours.

void Selection::RemoveRangeAndUnselectFramesAndNotifyListeners(
    nsRange& aRange, ErrorResult& aRv) {
  nsresult rv = mStyledRanges.RemoveRangeAndUnregisterSelection(aRange);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return;
  }

  nsINode* beginNode = aRange.GetStartContainer();
  nsINode* endNode = aRange.GetEndContainer();

  if (!beginNode || !endNode) {
    // Detached range; nothing else to do here.
    return;
  }

  // find out the length of the end node, so we can select all of it
  int32_t beginOffset, endOffset;
  if (endNode->IsText()) {
    // Get the length of the text. We can't just use the offset because
    // another range could be touching this text node but not intersect our
    // range.
    beginOffset = 0;
    endOffset = endNode->AsText()->TextLength();
  } else {
    // For non-text nodes, the given offsets should be sufficient.
    beginOffset = aRange.StartOffset();
    endOffset = aRange.EndOffset();
  }

  // clear the selected bit from the removed range's frames
  RefPtr<nsPresContext> presContext = GetPresContext();
  SelectFrames(presContext, &aRange, false);

  // add back the selected bit for each range touching our nodes
  nsTArray<nsRange*> affectedRanges;
  rv = GetRangesForIntervalArray(beginNode, beginOffset, endNode, endOffset,
                                 true, &affectedRanges);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return;
  }
  for (uint32_t i = 0; i < affectedRanges.Length(); i++) {
    SelectFrames(presContext, affectedRanges[i], true);
  }

  int32_t cnt = mStyledRanges.Length();
  if (&aRange == mAnchorFocusRange) {
    // Reset anchor to LAST range or clear it if there are no ranges.
    SetAnchorFocusRange(cnt - 1);

    // When the selection is user-created it makes sense to scroll the range
    // into view. The spell-check selection, however, is created and destroyed
    // in the background. We don't want to scroll in this case or the view
    // might appear to be moving randomly (bug 337871).
    if (mSelectionType != SelectionType::eSpellCheck && cnt > 0) {
      ScrollIntoView(nsISelectionController::SELECTION_FOCUS_REGION);
    }
  }

  if (!mFrameSelection) return;  // nothing to do

  RefPtr<Selection> kungFuDeathGrip{this};
  // Be aware, this instance may be destroyed after this call.
  NotifySelectionListeners();
}

/*
 * Collapse sets the whole selection to be one point.
 */
void Selection::CollapseJS(nsINode* aContainer, uint32_t aOffset,
                           ErrorResult& aRv) {
  AutoRestore<bool> calledFromJSRestorer(mCalledByJS);
  mCalledByJS = true;
  if (!aContainer) {
    RemoveAllRanges(aRv);
    return;
  }
  CollapseInternal(InLimiter::eNo, RawRangeBoundary(aContainer, aOffset), aRv);
}

void Selection::CollapseInternal(InLimiter aInLimiter,
                                 const RawRangeBoundary& aPoint,
                                 ErrorResult& aRv) {
  if (!mFrameSelection) {
    aRv.Throw(NS_ERROR_NOT_INITIALIZED);  // Can't do selection
    return;
  }

  if (!aPoint.IsSet()) {
    aRv.Throw(NS_ERROR_INVALID_ARG);
    return;
  }

  if (aPoint.Container()->NodeType() == nsINode::DOCUMENT_TYPE_NODE) {
    aRv.ThrowInvalidNodeTypeError(kNoDocumentTypeNodeError);
    return;
  }

  // RawRangeBoundary::IsSetAndValid() checks if the point actually refers
  // a child of the container when IsSet() is true.  If its offset hasn't been
  // computed yet, this just checks it with its mRef.  So, we can avoid
  // computing offset here.
  if (!aPoint.IsSetAndValid()) {
    aRv.ThrowIndexSizeError("The offset is out of range.");
    return;
  }

  if (!HasSameRootOrSameComposedDoc(*aPoint.Container())) {
    // Return with no error
    return;
  }

  RefPtr<nsFrameSelection> frameSelection = mFrameSelection;
  frameSelection->InvalidateDesiredCaretPos();
  if (aInLimiter == InLimiter::eYes &&
      !frameSelection->IsValidSelectionPoint(aPoint.Container())) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }
  nsresult result;

  RefPtr<nsPresContext> presContext = GetPresContext();
  if (!presContext ||
      presContext->Document() != aPoint.Container()->OwnerDoc()) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  // Delete all of the current ranges
  Clear(presContext);

  // Turn off signal for table selection
  frameSelection->ClearTableCellSelection();

  // Hack to display the caret on the right line (bug 1237236).
  if (frameSelection->GetHint() != CARET_ASSOCIATE_AFTER &&
      aPoint.Container()->IsContent()) {
    int32_t frameOffset;
    nsTextFrame* f = do_QueryFrame(nsCaret::GetFrameAndOffset(
        this, aPoint.Container(),
        *aPoint.Offset(RawRangeBoundary::OffsetFilter::kValidOffsets),
        &frameOffset));
    if (f && f->IsAtEndOfLine() && f->HasSignificantTerminalNewline()) {
      // RawRangeBounary::Offset() causes computing offset if it's not been
      // done yet.  However, it's called only when the container is a text
      // node.  In such case, offset has always been set since it cannot have
      // any children.  So, this doesn't cause computing offset with expensive
      // method, nsINode::ComputeIndexOf().
      if ((aPoint.Container()->AsContent() == f->GetContent() &&
           f->GetContentEnd() ==
               static_cast<int32_t>(*aPoint.Offset(
                   RawRangeBoundary::OffsetFilter::kValidOffsets))) ||
          (aPoint.Container() == f->GetContent()->GetParentNode() &&
           f->GetContent() == aPoint.GetPreviousSiblingOfChildAtOffset())) {
        frameSelection->SetHint(CARET_ASSOCIATE_AFTER);
      }
    }
  }

  RefPtr<nsRange> range = nsRange::Create(aPoint.Container());
  result = range->CollapseTo(aPoint);
  if (NS_FAILED(result)) {
    aRv.Throw(result);
    return;
  }

#ifdef DEBUG_SELECTION
  nsCOMPtr<nsIContent> content = do_QueryInterface(aPoint.Container());
  nsCOMPtr<Document> doc = do_QueryInterface(aPoint.Container());
  printf("Sel. Collapse to %p %s %d\n", container.get(),
         content ? nsAtomCString(content->NodeInfo()->NameAtom()).get()
                 : (doc ? "DOCUMENT" : "???"),
         aPoint.Offset());
#endif

  int32_t rangeIndex = -1;
  result = AddRangesForSelectableNodes(range, &rangeIndex,
                                       DispatchSelectstartEvent::Maybe);
  if (NS_FAILED(result)) {
    aRv.Throw(result);
    return;
  }
  SetAnchorFocusRange(0);
  SelectFrames(presContext, range, true);

  RefPtr<Selection> kungFuDeathGrip{this};
  // Be aware, this instance may be destroyed after this call.
  NotifySelectionListeners();
}

/*
 * Sets the whole selection to be one point
 * at the start of the current selection
 */
void Selection::CollapseToStartJS(ErrorResult& aRv) {
  AutoRestore<bool> calledFromJSRestorer(mCalledByJS);
  mCalledByJS = true;
  CollapseToStart(aRv);
}

void Selection::CollapseToStart(ErrorResult& aRv) {
  if (RangeCount() == 0) {
    aRv.ThrowInvalidStateError(kNoRangeExistsError);
    return;
  }

  // Get the first range
  const nsRange* firstRange = mStyledRanges.mRanges[0].mRange;
  if (!firstRange) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  if (mFrameSelection) {
    mFrameSelection->AddChangeReasons(
        nsISelectionListener::COLLAPSETOSTART_REASON);
  }
  nsINode* container = firstRange->GetStartContainer();
  if (!container) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }
  CollapseInternal(InLimiter::eNo,
                   RawRangeBoundary(container, firstRange->StartOffset()), aRv);
}

/*
 * Sets the whole selection to be one point
 * at the end of the current selection
 */
void Selection::CollapseToEndJS(ErrorResult& aRv) {
  AutoRestore<bool> calledFromJSRestorer(mCalledByJS);
  mCalledByJS = true;
  CollapseToEnd(aRv);
}

void Selection::CollapseToEnd(ErrorResult& aRv) {
  uint32_t cnt = RangeCount();
  if (cnt == 0) {
    aRv.ThrowInvalidStateError(kNoRangeExistsError);
    return;
  }

  // Get the last range
  const nsRange* lastRange = mStyledRanges.mRanges[cnt - 1].mRange;
  if (!lastRange) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  if (mFrameSelection) {
    mFrameSelection->AddChangeReasons(
        nsISelectionListener::COLLAPSETOEND_REASON);
  }
  nsINode* container = lastRange->GetEndContainer();
  if (!container) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }
  CollapseInternal(InLimiter::eNo,
                   RawRangeBoundary(container, lastRange->EndOffset()), aRv);
}

void Selection::GetType(nsAString& aOutType) const {
  if (!RangeCount()) {
    aOutType.AssignLiteral("None");
  } else if (IsCollapsed()) {
    aOutType.AssignLiteral("Caret");
  } else {
    aOutType.AssignLiteral("Range");
  }
}

nsRange* Selection::GetRangeAt(uint32_t aIndex, ErrorResult& aRv) {
  nsRange* range = GetRangeAt(aIndex);
  if (!range) {
    aRv.ThrowIndexSizeError(nsPrintfCString("%u is out of range", aIndex));
    return nullptr;
  }

  return range;
}

nsRange* Selection::GetRangeAt(int32_t aIndex) const {
  StyledRange empty(nullptr);
  return mStyledRanges.mRanges.SafeElementAt(aIndex, empty).mRange;
}

nsresult Selection::SetAnchorFocusToRange(nsRange* aRange) {
  NS_ENSURE_STATE(mAnchorFocusRange);

  const DispatchSelectstartEvent dispatchSelectstartEvent =
      IsCollapsed() ? DispatchSelectstartEvent::Maybe
                    : DispatchSelectstartEvent::No;

  nsresult res =
      mStyledRanges.RemoveRangeAndUnregisterSelection(*mAnchorFocusRange);
  if (NS_FAILED(res)) return res;

  int32_t aOutIndex = -1;
  res =
      AddRangesForSelectableNodes(aRange, &aOutIndex, dispatchSelectstartEvent);
  if (NS_FAILED(res)) return res;
  SetAnchorFocusRange(aOutIndex);

  return NS_OK;
}

void Selection::ReplaceAnchorFocusRange(nsRange* aRange) {
  NS_ENSURE_TRUE_VOID(mAnchorFocusRange);
  RefPtr<nsPresContext> presContext = GetPresContext();
  if (presContext) {
    SelectFrames(presContext, mAnchorFocusRange, false);
    SetAnchorFocusToRange(aRange);
    SelectFrames(presContext, mAnchorFocusRange, true);
  }
}

void Selection::AdjustAnchorFocusForMultiRange(nsDirection aDirection) {
  if (aDirection == mDirection) {
    return;
  }
  SetDirection(aDirection);

  if (RangeCount() <= 1) {
    return;
  }

  nsRange* firstRange = GetRangeAt(0);
  nsRange* lastRange = GetRangeAt(RangeCount() - 1);

  if (mDirection == eDirPrevious) {
    firstRange->SetIsGenerated(false);
    lastRange->SetIsGenerated(true);
    SetAnchorFocusRange(0);
  } else {  // aDir == eDirNext
    firstRange->SetIsGenerated(true);
    lastRange->SetIsGenerated(false);
    SetAnchorFocusRange(RangeCount() - 1);
  }
}

/*
 * Extend extends the selection away from the anchor.
 * We don't need to know the direction, because we always change the focus.
 */
void Selection::ExtendJS(nsINode& aContainer, uint32_t aOffset,
                         ErrorResult& aRv) {
  AutoRestore<bool> calledFromJSRestorer(mCalledByJS);
  mCalledByJS = true;
  Extend(aContainer, aOffset, aRv);
}

nsresult Selection::Extend(nsINode* aContainer, int32_t aOffset) {
  if (!aContainer) {
    return NS_ERROR_INVALID_ARG;
  }

  ErrorResult result;
  Extend(*aContainer, static_cast<uint32_t>(aOffset), result);
  return result.StealNSResult();
}

void Selection::Extend(nsINode& aContainer, uint32_t aOffset,
                       ErrorResult& aRv) {
  /*
    Notes which might come in handy for extend:

    We can tell the direction of the selection by asking for the anchors
    selection if the begin is less than the end then we know the selection is to
    the "right", else it is a backwards selection. Notation: a = anchor, 1 = old
    cursor, 2 = new cursor.

      if (a <= 1 && 1 <=2)    a,1,2  or (a1,2)
      if (a < 2 && 1 > 2)     a,2,1
      if (1 < a && a <2)      1,a,2
      if (a > 2 && 2 >1)      1,2,a
      if (2 < a && a <1)      2,a,1
      if (a > 1 && 1 >2)      2,1,a
    then execute
    a  1  2 select from 1 to 2
    a  2  1 deselect from 2 to 1
    1  a  2 deselect from 1 to a select from a to 2
    1  2  a deselect from 1 to 2
    2  1  a = continue selection from 2 to 1
  */

  // First, find the range containing the old focus point:
  if (!mAnchorFocusRange) {
    aRv.ThrowInvalidStateError(kNoRangeExistsError);
    return;
  }

  if (!mFrameSelection) {
    aRv.Throw(NS_ERROR_NOT_INITIALIZED);  // Can't do selection
    return;
  }

  if (!HasSameRootOrSameComposedDoc(aContainer)) {
    // Return with no error
    return;
  }

  nsresult res;
  if (!mFrameSelection->IsValidSelectionPoint(&aContainer)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  RefPtr<nsPresContext> presContext = GetPresContext();
  if (!presContext || presContext->Document() != aContainer.OwnerDoc()) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

#ifdef DEBUG_SELECTION
  nsDirection oldDirection = GetDirection();
#endif
  nsINode* anchorNode = GetAnchorNode();
  nsINode* focusNode = GetFocusNode();
  uint32_t anchorOffset = AnchorOffset();
  uint32_t focusOffset = FocusOffset();

  RefPtr<nsRange> range = mAnchorFocusRange->CloneRange();

  nsINode* startNode = range->GetStartContainer();
  nsINode* endNode = range->GetEndContainer();
  int32_t startOffset = range->StartOffset();
  int32_t endOffset = range->EndOffset();

  bool shouldClearRange = false;
  const Maybe<int32_t> anchorOldFocusOrder = nsContentUtils::ComparePoints(
      anchorNode, anchorOffset, focusNode, focusOffset);
  shouldClearRange |= !anchorOldFocusOrder;
  const Maybe<int32_t> oldFocusNewFocusOrder = nsContentUtils::ComparePoints(
      focusNode, focusOffset, &aContainer, aOffset);
  shouldClearRange |= !oldFocusNewFocusOrder;
  const Maybe<int32_t> anchorNewFocusOrder = nsContentUtils::ComparePoints(
      anchorNode, anchorOffset, &aContainer, aOffset);
  shouldClearRange |= !anchorNewFocusOrder;

  // If the points are disconnected, the range will be collapsed below,
  // resulting in a range that selects nothing.
  if (shouldClearRange) {
    // Repaint the current range with the selection removed.
    SelectFrames(presContext, range, false);

    res = range->CollapseTo(&aContainer, aOffset);
    if (NS_FAILED(res)) {
      aRv.Throw(res);
      return;
    }

    res = SetAnchorFocusToRange(range);
    if (NS_FAILED(res)) {
      aRv.Throw(res);
      return;
    }
  } else {
    RefPtr<nsRange> difRange = nsRange::Create(&aContainer);
    if ((*anchorOldFocusOrder == 0 && *anchorNewFocusOrder < 0) ||
        (*anchorOldFocusOrder <= 0 &&
         *oldFocusNewFocusOrder < 0)) {  // a1,2  a,1,2
      // select from 1 to 2 unless they are collapsed
      range->SetEnd(aContainer, aOffset, aRv);
      if (aRv.Failed()) {
        return;
      }
      SetDirection(eDirNext);
      res = difRange->SetStartAndEnd(
          focusNode, focusOffset, range->GetEndContainer(), range->EndOffset());
      if (NS_FAILED(res)) {
        aRv.Throw(res);
        return;
      }
      SelectFrames(presContext, difRange, true);
      res = SetAnchorFocusToRange(range);
      if (NS_FAILED(res)) {
        aRv.Throw(res);
        return;
      }
    } else if (*anchorOldFocusOrder == 0 &&
               *anchorNewFocusOrder > 0) {  // 2, a1
      // select from 2 to 1a
      SetDirection(eDirPrevious);
      range->SetStart(aContainer, aOffset, aRv);
      if (aRv.Failed()) {
        return;
      }
      SelectFrames(presContext, range, true);
      res = SetAnchorFocusToRange(range);
      if (NS_FAILED(res)) {
        aRv.Throw(res);
        return;
      }
    } else if (*anchorNewFocusOrder <= 0 &&
               *oldFocusNewFocusOrder >= 0) {  // a,2,1 or a2,1 or a,21 or a21
      // deselect from 2 to 1
      res = difRange->SetStartAndEnd(&aContainer, aOffset, focusNode,
                                     focusOffset);
      if (NS_FAILED(res)) {
        aRv.Throw(res);
        return;
      }

      range->SetEnd(aContainer, aOffset, aRv);
      if (aRv.Failed()) {
        return;
      }
      res = SetAnchorFocusToRange(range);
      if (NS_FAILED(res)) {
        aRv.Throw(res);
        return;
      }
      SelectFrames(presContext, difRange, false);  // deselect now
      difRange->SetEnd(range->GetEndContainer(), range->EndOffset());
      SelectFrames(presContext, difRange, true);  // must reselect last node
                                                  // maybe more
    } else if (*anchorOldFocusOrder >= 0 &&
               *anchorNewFocusOrder <= 0) {  // 1,a,2 or 1a,2 or 1,a2 or 1a2
      if (GetDirection() == eDirPrevious) {
        res = range->SetStart(endNode, endOffset);
        if (NS_FAILED(res)) {
          aRv.Throw(res);
          return;
        }
      }
      SetDirection(eDirNext);
      range->SetEnd(aContainer, aOffset, aRv);
      if (aRv.Failed()) {
        return;
      }
      if (focusNode != anchorNode ||
          focusOffset != anchorOffset) {  // if collapsed diff dont do anything
        res = difRange->SetStart(focusNode, focusOffset);
        nsresult tmp = difRange->SetEnd(anchorNode, anchorOffset);
        if (NS_FAILED(tmp)) {
          res = tmp;
        }
        if (NS_FAILED(res)) {
          aRv.Throw(res);
          return;
        }
        res = SetAnchorFocusToRange(range);
        if (NS_FAILED(res)) {
          aRv.Throw(res);
          return;
        }
        // deselect from 1 to a
        SelectFrames(presContext, difRange, false);
      } else {
        res = SetAnchorFocusToRange(range);
        if (NS_FAILED(res)) {
          aRv.Throw(res);
          return;
        }
      }
      // select from a to 2
      SelectFrames(presContext, range, true);
    } else if (*oldFocusNewFocusOrder <= 0 &&
               *anchorNewFocusOrder >= 0) {  // 1,2,a or 12,a or 1,2a or 12a
      // deselect from 1 to 2
      res = difRange->SetStartAndEnd(focusNode, focusOffset, &aContainer,
                                     aOffset);
      if (NS_FAILED(res)) {
        aRv.Throw(res);
        return;
      }
      SetDirection(eDirPrevious);
      range->SetStart(aContainer, aOffset, aRv);
      if (aRv.Failed()) {
        return;
      }

      res = SetAnchorFocusToRange(range);
      if (NS_FAILED(res)) {
        aRv.Throw(res);
        return;
      }
      SelectFrames(presContext, difRange, false);
      difRange->SetStart(range->GetStartContainer(), range->StartOffset());
      SelectFrames(presContext, difRange, true);  // must reselect last node
    } else if (*anchorNewFocusOrder >= 0 &&
               *anchorOldFocusOrder <= 0) {  // 2,a,1 or 2a,1 or 2,a1 or 2a1
      if (GetDirection() == eDirNext) {
        range->SetEnd(startNode, startOffset);
      }
      SetDirection(eDirPrevious);
      range->SetStart(aContainer, aOffset, aRv);
      if (aRv.Failed()) {
        return;
      }
      // deselect from a to 1
      if (focusNode != anchorNode ||
          focusOffset != anchorOffset) {  // if collapsed diff dont do anything
        res = difRange->SetStartAndEnd(anchorNode, anchorOffset, focusNode,
                                       focusOffset);
        nsresult tmp = SetAnchorFocusToRange(range);
        if (NS_FAILED(tmp)) {
          res = tmp;
        }
        if (NS_FAILED(res)) {
          aRv.Throw(res);
          return;
        }
        SelectFrames(presContext, difRange, false);
      } else {
        res = SetAnchorFocusToRange(range);
        if (NS_FAILED(res)) {
          aRv.Throw(res);
          return;
        }
      }
      // select from 2 to a
      SelectFrames(presContext, range, true);
    } else if (*oldFocusNewFocusOrder >= 0 &&
               *anchorOldFocusOrder >= 0) {  // 2,1,a or 21,a or 2,1a or 21a
      // select from 2 to 1
      range->SetStart(aContainer, aOffset, aRv);
      if (aRv.Failed()) {
        return;
      }
      SetDirection(eDirPrevious);
      res = difRange->SetStartAndEnd(range->GetStartContainer(),
                                     range->StartOffset(), focusNode,
                                     focusOffset);
      if (NS_FAILED(res)) {
        aRv.Throw(res);
        return;
      }

      SelectFrames(presContext, difRange, true);
      res = SetAnchorFocusToRange(range);
      if (NS_FAILED(res)) {
        aRv.Throw(res);
        return;
      }
    }
  }

  if (mStyledRanges.Length() > 1) {
    SelectFramesInAllRanges(presContext);
  }

  DEBUG_OUT_RANGE(range);
#ifdef DEBUG_SELECTION
  if (GetDirection() != oldDirection) {
    printf("    direction changed to %s\n",
           GetDirection() == eDirNext ? "eDirNext" : "eDirPrevious");
  }
  nsCOMPtr<nsIContent> content = do_QueryInterface(&aContainer);
  printf("Sel. Extend to %p %s %d\n", content.get(),
         nsAtomCString(content->NodeInfo()->NameAtom()).get(), aOffset);
#endif

  RefPtr<Selection> kungFuDeathGrip{this};
  // Be aware, this instance may be destroyed after this call.
  NotifySelectionListeners();
}

void Selection::SelectAllChildrenJS(nsINode& aNode, ErrorResult& aRv) {
  AutoRestore<bool> calledFromJSRestorer(mCalledByJS);
  mCalledByJS = true;
  SelectAllChildren(aNode, aRv);
}

void Selection::SelectAllChildren(nsINode& aNode, ErrorResult& aRv) {
  if (aNode.NodeType() == nsINode::DOCUMENT_TYPE_NODE) {
    aRv.ThrowInvalidNodeTypeError(kNoDocumentTypeNodeError);
    return;
  }

  if (!HasSameRootOrSameComposedDoc(aNode)) {
    // Return with no error
    return;
  }

  if (mFrameSelection) {
    mFrameSelection->AddChangeReasons(nsISelectionListener::SELECTALL_REASON);
  }

  // Chrome moves focus when aNode is outside of active editing host.
  // So, we don't need to respect the limiter with this method.
  SetStartAndEndInternal(InLimiter::eNo, RawRangeBoundary(&aNode, 0u),
                         RawRangeBoundary(&aNode, aNode.GetChildCount()),
                         eDirNext, aRv);
}

bool Selection::ContainsNode(nsINode& aNode, bool aAllowPartial,
                             ErrorResult& aRv) {
  nsresult rv;
  if (mStyledRanges.Length() == 0) {
    return false;
  }

  // XXXbz this duplicates the GetNodeLength code in nsRange.cpp
  uint32_t nodeLength;
  auto* nodeAsCharData = CharacterData::FromNode(aNode);
  if (nodeAsCharData) {
    nodeLength = nodeAsCharData->TextLength();
  } else {
    nodeLength = aNode.GetChildCount();
  }

  nsTArray<nsRange*> overlappingRanges;
  rv = GetRangesForIntervalArray(&aNode, 0, &aNode, nodeLength, false,
                                 &overlappingRanges);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return false;
  }
  if (overlappingRanges.Length() == 0) return false;  // no ranges overlap

  // if the caller said partial intersections are OK, we're done
  if (aAllowPartial) {
    return true;
  }

  // text nodes always count as inside
  if (nodeAsCharData) {
    return true;
  }

  // The caller wants to know if the node is entirely within the given range,
  // so we have to check all intersecting ranges.
  for (uint32_t i = 0; i < overlappingRanges.Length(); i++) {
    bool nodeStartsBeforeRange, nodeEndsAfterRange;
    if (NS_SUCCEEDED(RangeUtils::CompareNodeToRange(
            &aNode, overlappingRanges[i], &nodeStartsBeforeRange,
            &nodeEndsAfterRange))) {
      if (!nodeStartsBeforeRange && !nodeEndsAfterRange) {
        return true;
      }
    }
  }
  return false;
}

class PointInRectChecker : public mozilla::RectCallback {
 public:
  explicit PointInRectChecker(const nsPoint& aPoint)
      : mPoint(aPoint), mMatchFound(false) {}

  void AddRect(const nsRect& aRect) override {
    mMatchFound = mMatchFound || aRect.Contains(mPoint);
  }

  bool MatchFound() { return mMatchFound; }

 private:
  nsPoint mPoint;
  bool mMatchFound;
};

bool Selection::ContainsPoint(const nsPoint& aPoint) {
  if (IsCollapsed()) {
    return false;
  }
  PointInRectChecker checker(aPoint);
  for (uint32_t i = 0; i < RangeCount(); i++) {
    nsRange* range = GetRangeAt(i);
    nsRange::CollectClientRectsAndText(
        &checker, nullptr, range, range->GetStartContainer(),
        range->StartOffset(), range->GetEndContainer(), range->EndOffset(),
        true, false);
    if (checker.MatchFound()) {
      return true;
    }
  }
  return false;
}

void Selection::MaybeNotifyAccessibleCaretEventHub(PresShell* aPresShell) {
  MOZ_ASSERT(mSelectionType == SelectionType::eNormal);

  if (!mAccessibleCaretEventHub && aPresShell) {
    mAccessibleCaretEventHub = aPresShell->GetAccessibleCaretEventHub();
  }
}

void Selection::StopNotifyingAccessibleCaretEventHub() {
  MOZ_ASSERT(mSelectionType == SelectionType::eNormal);

  mAccessibleCaretEventHub = nullptr;
}

nsPresContext* Selection::GetPresContext() const {
  PresShell* presShell = GetPresShell();
  return presShell ? presShell->GetPresContext() : nullptr;
}

PresShell* Selection::GetPresShell() const {
  if (!mFrameSelection) {
    return nullptr;  // nothing to do
  }
  return mFrameSelection->GetPresShell();
}

Document* Selection::GetDocument() const {
  PresShell* presShell = GetPresShell();
  return presShell ? presShell->GetDocument() : nullptr;
}

nsIFrame* Selection::GetSelectionAnchorGeometry(SelectionRegion aRegion,
                                                nsRect* aRect) {
  if (!mFrameSelection) return nullptr;  // nothing to do

  NS_ENSURE_TRUE(aRect, nullptr);

  aRect->SetRect(0, 0, 0, 0);

  switch (aRegion) {
    case nsISelectionController::SELECTION_ANCHOR_REGION:
    case nsISelectionController::SELECTION_FOCUS_REGION:
      return GetSelectionEndPointGeometry(aRegion, aRect);
    case nsISelectionController::SELECTION_WHOLE_SELECTION:
      break;
    default:
      return nullptr;
  }

  NS_ASSERTION(aRegion == nsISelectionController::SELECTION_WHOLE_SELECTION,
               "should only be SELECTION_WHOLE_SELECTION here");

  nsRect anchorRect;
  nsIFrame* anchorFrame = GetSelectionEndPointGeometry(
      nsISelectionController::SELECTION_ANCHOR_REGION, &anchorRect);
  if (!anchorFrame) return nullptr;

  nsRect focusRect;
  nsIFrame* focusFrame = GetSelectionEndPointGeometry(
      nsISelectionController::SELECTION_FOCUS_REGION, &focusRect);
  if (!focusFrame) return nullptr;

  NS_ASSERTION(anchorFrame->PresContext() == focusFrame->PresContext(),
               "points of selection in different documents?");
  // make focusRect relative to anchorFrame
  focusRect += focusFrame->GetOffsetTo(anchorFrame);

  *aRect = anchorRect.UnionEdges(focusRect);
  return anchorFrame;
}

nsIFrame* Selection::GetSelectionEndPointGeometry(SelectionRegion aRegion,
                                                  nsRect* aRect) {
  if (!mFrameSelection) return nullptr;  // nothing to do

  NS_ENSURE_TRUE(aRect, nullptr);

  aRect->SetRect(0, 0, 0, 0);

  nsINode* node = nullptr;
  uint32_t nodeOffset = 0;
  nsIFrame* frame = nullptr;

  switch (aRegion) {
    case nsISelectionController::SELECTION_ANCHOR_REGION:
      node = GetAnchorNode();
      nodeOffset = AnchorOffset();
      break;
    case nsISelectionController::SELECTION_FOCUS_REGION:
      node = GetFocusNode();
      nodeOffset = FocusOffset();
      break;
    default:
      return nullptr;
  }

  if (!node) return nullptr;

  nsCOMPtr<nsIContent> content = do_QueryInterface(node);
  NS_ENSURE_TRUE(content.get(), nullptr);
  int32_t frameOffset = 0;
  frame = nsFrameSelection::GetFrameForNodeOffset(
      content, nodeOffset, mFrameSelection->GetHint(), &frameOffset);
  if (!frame) return nullptr;

  nsFrameSelection::AdjustFrameForLineStart(frame, frameOffset);

  // Figure out what node type we have, then get the
  // appropriate rect for it's nodeOffset.
  bool isText = node->IsText();

  nsPoint pt(0, 0);
  if (isText) {
    nsIFrame* childFrame = nullptr;
    frameOffset = 0;
    nsresult rv = frame->GetChildFrameContainingOffset(
        nodeOffset, mFrameSelection->GetHint(), &frameOffset, &childFrame);
    if (NS_FAILED(rv)) return nullptr;
    if (!childFrame) return nullptr;

    frame = childFrame;

    // Get the x coordinate of the offset into the text frame.
    rv = GetCachedFrameOffset(frame, nodeOffset, pt);
    if (NS_FAILED(rv)) return nullptr;
  }

  // Return the rect relative to the frame, with zero width.
  if (isText) {
    aRect->x = pt.x;
  } else if (mFrameSelection->GetHint() == CARET_ASSOCIATE_BEFORE) {
    // It's the frame's right edge we're interested in.
    aRect->x = frame->GetRect().Width();
  }
  aRect->SetHeight(frame->GetRect().Height());

  return frame;
}

NS_IMETHODIMP
Selection::ScrollSelectionIntoViewEvent::Run() {
  if (!mSelection) return NS_OK;  // event revoked

  int32_t flags = Selection::SCROLL_DO_FLUSH | Selection::SCROLL_SYNCHRONOUS;

  const RefPtr<Selection> selection{mSelection};
  selection->mScrollEvent.Forget();
  selection->ScrollIntoView(mRegion, mVerticalScroll, mHorizontalScroll,
                            mFlags | flags);
  return NS_OK;
}

nsresult Selection::PostScrollSelectionIntoViewEvent(SelectionRegion aRegion,
                                                     int32_t aFlags,
                                                     ScrollAxis aVertical,
                                                     ScrollAxis aHorizontal) {
  // If we've already posted an event, revoke it and place a new one at the
  // end of the queue to make sure that any new pending reflow events are
  // processed before we scroll. This will insure that we scroll to the
  // correct place on screen.
  mScrollEvent.Revoke();
  nsPresContext* presContext = GetPresContext();
  NS_ENSURE_STATE(presContext);
  nsRefreshDriver* refreshDriver = presContext->RefreshDriver();
  NS_ENSURE_STATE(refreshDriver);

  mScrollEvent = new ScrollSelectionIntoViewEvent(this, aRegion, aVertical,
                                                  aHorizontal, aFlags);
  refreshDriver->AddEarlyRunner(mScrollEvent.get());
  return NS_OK;
}

void Selection::ScrollIntoView(int16_t aRegion, bool aIsSynchronous,
                               WhereToScroll aVPercent, WhereToScroll aHPercent,
                               ErrorResult& aRv) {
  int32_t flags = aIsSynchronous ? Selection::SCROLL_SYNCHRONOUS : 0;
  nsresult rv = ScrollIntoView(aRegion, ScrollAxis(aVPercent),
                               ScrollAxis(aHPercent), flags);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
  }
}

nsresult Selection::ScrollIntoView(SelectionRegion aRegion,
                                   ScrollAxis aVertical, ScrollAxis aHorizontal,
                                   int32_t aFlags) {
  if (!mFrameSelection) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  RefPtr<PresShell> presShell = mFrameSelection->GetPresShell();
  if (!presShell || !presShell->GetDocument()) {
    return NS_OK;
  }

  if (mFrameSelection->IsBatching()) {
    return NS_OK;
  }

  if (!(aFlags & Selection::SCROLL_SYNCHRONOUS))
    return PostScrollSelectionIntoViewEvent(aRegion, aFlags, aVertical,
                                            aHorizontal);

  // From this point on, the presShell may get destroyed by the calls below, so
  // hold on to it using a strong reference to ensure the safety of the
  // accesses to frame pointers in the callees.
  RefPtr<PresShell> kungFuDeathGrip(presShell);

  // Now that text frame character offsets are always valid (though not
  // necessarily correct), the worst that will happen if we don't flush here
  // is that some callers might scroll to the wrong place.  Those should
  // either manually flush if they're in a safe position for it or use the
  // async version of this method.
  if (aFlags & Selection::SCROLL_DO_FLUSH) {
    presShell->GetDocument()->FlushPendingNotifications(FlushType::Layout);

    // Reget the presshell, since it might have been Destroy'ed.
    presShell = mFrameSelection ? mFrameSelection->GetPresShell() : nullptr;
    if (!presShell) {
      return NS_OK;
    }
  }

  //
  // Scroll the selection region into view.
  //

  nsRect rect;
  nsIFrame* frame = GetSelectionAnchorGeometry(aRegion, &rect);
  if (!frame) return NS_ERROR_FAILURE;

  // Scroll vertically to get the caret into view, but only if the container
  // is perceived to be scrollable in that direction (i.e. there is a visible
  // vertical scrollbar or the scroll range is at least one device pixel)
  aVertical.mOnlyIfPerceivedScrollableDirection = true;

  auto scrollFlags = ScrollFlags::None;
  if (aFlags & Selection::SCROLL_FIRST_ANCESTOR_ONLY) {
    scrollFlags |= ScrollFlags::ScrollFirstAncestorOnly;
  }
  if (aFlags & Selection::SCROLL_OVERFLOW_HIDDEN) {
    scrollFlags |= ScrollFlags::ScrollOverflowHidden;
  }

  presShell->ScrollFrameRectIntoView(frame, rect, nsMargin(), aVertical,
                                     aHorizontal, scrollFlags);
  return NS_OK;
}

void Selection::AddSelectionListener(nsISelectionListener* aNewListener) {
  MOZ_ASSERT(aNewListener);
  mSelectionListeners.AppendElement(aNewListener);  // AddRefs
}

void Selection::RemoveSelectionListener(
    nsISelectionListener* aListenerToRemove) {
  mSelectionListeners.RemoveElement(aListenerToRemove);  // Releases
}

Element* Selection::StyledRanges::GetCommonEditingHost() const {
  Element* editingHost = nullptr;
  for (const StyledRange& rangeData : mRanges) {
    const nsRange* range = rangeData.mRange;
    MOZ_ASSERT(range);
    nsINode* commonAncestorNode = range->GetClosestCommonInclusiveAncestor();
    if (!commonAncestorNode || !commonAncestorNode->IsContent()) {
      return nullptr;
    }
    nsIContent* commonAncestor = commonAncestorNode->AsContent();
    Element* foundEditingHost = commonAncestor->GetEditingHost();
    // Even when common ancestor is a non-editable element in a contenteditable
    // element, we don't need to move focus to the contenteditable element
    // because Chromium doesn't set focus to it.
    if (!foundEditingHost) {
      return nullptr;
    }
    if (!editingHost) {
      editingHost = foundEditingHost;
      continue;
    }
    if (editingHost == foundEditingHost) {
      continue;
    }
    if (foundEditingHost->IsInclusiveDescendantOf(editingHost)) {
      continue;
    }
    if (editingHost->IsInclusiveDescendantOf(foundEditingHost)) {
      editingHost = foundEditingHost;
      continue;
    }
    // editingHost and foundEditingHost are not a descendant of the other.
    // So, there is no common editing host.
    return nullptr;
  }
  return editingHost;
}

void Selection::StyledRanges::MaybeFocusCommonEditingHost(
    PresShell* aPresShell) const {
  if (!aPresShell) {
    return;
  }

  nsPresContext* presContext = aPresShell->GetPresContext();
  if (!presContext) {
    return;
  }

  Document* document = aPresShell->GetDocument();
  if (!document) {
    return;
  }

  nsPIDOMWindowOuter* window = document->GetWindow();
  // If the document is in design mode or doesn't have contenteditable
  // element, we don't need to move focus.
  if (window && !document->IsInDesignMode() &&
      nsContentUtils::GetHTMLEditor(presContext)) {
    RefPtr<Element> newEditingHost = GetCommonEditingHost();
    RefPtr<nsFocusManager> fm = nsFocusManager::GetFocusManager();
    nsCOMPtr<nsPIDOMWindowOuter> focusedWindow;
    nsIContent* focusedContent = nsFocusManager::GetFocusedDescendant(
        window, nsFocusManager::eOnlyCurrentWindow,
        getter_AddRefs(focusedWindow));
    nsCOMPtr<Element> focusedElement = do_QueryInterface(focusedContent);
    // When all selected ranges are in an editing host, it should take focus.
    // But otherwise, we shouldn't move focus since Chromium doesn't move
    // focus but only selection range is updated.
    if (newEditingHost && newEditingHost != focusedElement) {
      MOZ_ASSERT(!newEditingHost->IsInNativeAnonymousSubtree());
      // Note that don't steal focus from focused window if the window doesn't
      // have focus.  Additionally, although when an element gets focus, we
      // usually scroll to the element, but in this case, we shouldn't do it
      // because Chrome does not do so.
      fm->SetFocus(newEditingHost, nsIFocusManager::FLAG_NOSWITCHFRAME |
                                       nsIFocusManager::FLAG_NOSCROLL);
    }
  }
}

void Selection::NotifySelectionListeners(bool aCalledByJS) {
  AutoRestore<bool> calledFromJSRestorer(mCalledByJS);
  mCalledByJS = aCalledByJS;
  NotifySelectionListeners();
}

void Selection::NotifySelectionListeners() {
  if (!mFrameSelection) {
    return;  // nothing to do
  }

  MOZ_LOG(sSelectionLog, LogLevel::Debug,
          ("%s: selection=%p", __FUNCTION__, this));

  // Our internal code should not move focus with using this class while
  // this moves focus nor from selection listeners.
  AutoRestore<bool> calledByJSRestorer(mCalledByJS);
  mCalledByJS = false;

  // When normal selection is changed by Selection API, we need to move focus
  // if common ancestor of all ranges are in an editing host.  Note that we
  // don't need to move focus *to* the other focusable node because other
  // browsers don't do it either.
  if (mSelectionType == SelectionType::eNormal &&
      calledByJSRestorer.SavedValue()) {
    RefPtr<PresShell> presShell = GetPresShell();
    mStyledRanges.MaybeFocusCommonEditingHost(presShell);
  }

  RefPtr<nsFrameSelection> frameSelection = mFrameSelection;
  if (frameSelection->IsBatching()) {
    frameSelection->SetChangesDuringBatchingFlag();
    return;
  }
  if (mSelectionListeners.IsEmpty()) {
    // If there are no selection listeners, we're done!
    return;
  }

  nsCOMPtr<Document> doc;
  PresShell* presShell = GetPresShell();
  if (presShell) {
    doc = presShell->GetDocument();
  }

  // We've notified all selection listeners even when some of them are removed
  // (and may be destroyed) during notifying one of them.  Therefore, we should
  // copy all listeners to the local variable first.
  const CopyableAutoTArray<nsCOMPtr<nsISelectionListener>, 5>
      selectionListeners = mSelectionListeners;

  int16_t reason = frameSelection->PopChangeReasons();
  if (calledByJSRestorer.SavedValue()) {
    reason |= nsISelectionListener::JS_REASON;
  }

  if (mNotifyAutoCopy) {
    AutoCopyListener::OnSelectionChange(doc, *this, reason);
  }

  if (mAccessibleCaretEventHub) {
    RefPtr<AccessibleCaretEventHub> hub(mAccessibleCaretEventHub);
    hub->OnSelectionChange(doc, this, reason);
  }

  if (mSelectionChangeEventDispatcher) {
    RefPtr<SelectionChangeEventDispatcher> dispatcher(
        mSelectionChangeEventDispatcher);
    dispatcher->OnSelectionChange(doc, this, reason);
  }
  for (const auto& listener : selectionListeners) {
    // MOZ_KnownLive because 'selectionListeners' is guaranteed to
    // keep it alive.
    //
    // This can go away once
    // https://bugzilla.mozilla.org/show_bug.cgi?id=1620312 is fixed.
    MOZ_KnownLive(listener)->NotifySelectionChanged(doc, this, reason);
  }
}

void Selection::StartBatchChanges() {
  if (mFrameSelection) {
    RefPtr<nsFrameSelection> frameSelection = mFrameSelection;
    frameSelection->StartBatchChanges();
  }
}

void Selection::EndBatchChanges(int16_t aReason) {
  if (mFrameSelection) {
    RefPtr<nsFrameSelection> frameSelection = mFrameSelection;
    frameSelection->EndBatchChanges(aReason);
  }
}

void Selection::AddSelectionChangeBlocker() { mSelectionChangeBlockerCount++; }

void Selection::RemoveSelectionChangeBlocker() {
  MOZ_ASSERT(mSelectionChangeBlockerCount > 0,
             "mSelectionChangeBlockerCount has an invalid value - "
             "maybe you have a mismatched RemoveSelectionChangeBlocker?");
  mSelectionChangeBlockerCount--;
}

bool Selection::IsBlockingSelectionChangeEvents() const {
  return mSelectionChangeBlockerCount > 0;
}

void Selection::DeleteFromDocument(ErrorResult& aRv) {
  if (mSelectionType != SelectionType::eNormal) {
    return;  // Nothing to do.
  }

  // If we're already collapsed, then we do nothing (bug 719503).
  if (IsCollapsed()) {
    return;
  }

  for (uint32_t rangeIdx = 0; rangeIdx < RangeCount(); ++rangeIdx) {
    RefPtr<nsRange> range = GetRangeAt(rangeIdx);
    range->DeleteContents(aRv);
    if (aRv.Failed()) {
      return;
    }
  }

  // Collapse to the new location.
  // If we deleted one character, then we move back one element.
  // FIXME  We don't know how to do this past frame boundaries yet.
  if (AnchorOffset() > 0) {
    RefPtr<nsINode> anchor = GetAnchorNode();
    CollapseInLimiter(anchor, AnchorOffset());
  }
#ifdef DEBUG
  else {
    printf("Don't know how to set selection back past frame boundary\n");
  }
#endif
}

void Selection::Modify(const nsAString& aAlter, const nsAString& aDirection,
                       const nsAString& aGranularity, ErrorResult& aRv) {
  if (!mFrameSelection) {
    aRv.Throw(NS_ERROR_NOT_INITIALIZED);
    return;
  }

  if (!GetAnchorFocusRange() || !GetFocusNode()) {
    return;
  }

  if (!aAlter.LowerCaseEqualsLiteral("move") &&
      !aAlter.LowerCaseEqualsLiteral("extend")) {
    aRv.ThrowSyntaxError(
        R"(The first argument must be one of: "move" or "extend")");
    return;
  }

  if (!aDirection.LowerCaseEqualsLiteral("forward") &&
      !aDirection.LowerCaseEqualsLiteral("backward") &&
      !aDirection.LowerCaseEqualsLiteral("left") &&
      !aDirection.LowerCaseEqualsLiteral("right")) {
    aRv.ThrowSyntaxError(
        R"(The direction argument must be one of: "forward", "backward", "left", or "right")");
    return;
  }

  // Make sure the layout is up to date as we access bidi information below.
  if (RefPtr<Document> doc = GetDocument()) {
    doc->FlushPendingNotifications(FlushType::Layout);
  }

  // Line moves are always visual.
  bool visual = aDirection.LowerCaseEqualsLiteral("left") ||
                aDirection.LowerCaseEqualsLiteral("right") ||
                aGranularity.LowerCaseEqualsLiteral("line");

  bool forward = aDirection.LowerCaseEqualsLiteral("forward") ||
                 aDirection.LowerCaseEqualsLiteral("right");

  bool extend = aAlter.LowerCaseEqualsLiteral("extend");

  nsSelectionAmount amount;
  if (aGranularity.LowerCaseEqualsLiteral("character")) {
    amount = eSelectCluster;
  } else if (aGranularity.LowerCaseEqualsLiteral("word")) {
    amount = eSelectWordNoSpace;
  } else if (aGranularity.LowerCaseEqualsLiteral("line")) {
    amount = eSelectLine;
  } else if (aGranularity.LowerCaseEqualsLiteral("lineboundary")) {
    amount = forward ? eSelectEndLine : eSelectBeginLine;
  } else if (aGranularity.LowerCaseEqualsLiteral("sentence") ||
             aGranularity.LowerCaseEqualsLiteral("sentenceboundary") ||
             aGranularity.LowerCaseEqualsLiteral("paragraph") ||
             aGranularity.LowerCaseEqualsLiteral("paragraphboundary") ||
             aGranularity.LowerCaseEqualsLiteral("documentboundary")) {
    aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
    return;
  } else {
    aRv.ThrowSyntaxError(
        R"(The granularity argument must be one of: "character", "word", "line", or "lineboundary")");
    return;
  }

  // If the anchor doesn't equal the focus and we try to move without first
  // collapsing the selection, MoveCaret will collapse the selection and quit.
  // To avoid this, we need to collapse the selection first.
  nsresult rv = NS_OK;
  if (!extend) {
    RefPtr<nsINode> focusNode = GetFocusNode();
    // We should have checked earlier that there was a focus node.
    if (!focusNode) {
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return;
    }
    uint32_t focusOffset = FocusOffset();
    CollapseInLimiter(focusNode, focusOffset);
  }

  // If the paragraph direction of the focused frame is right-to-left,
  // we may have to swap the direction of movement.
  if (nsIFrame* frame = GetPrimaryFrameForFocusNode(visual)) {
    mozilla::intl::BidiDirection paraDir =
        nsBidiPresUtils::ParagraphDirection(frame);

    if (paraDir == mozilla::intl::BidiDirection::RTL && visual) {
      if (amount == eSelectBeginLine) {
        amount = eSelectEndLine;
        forward = !forward;
      } else if (amount == eSelectEndLine) {
        amount = eSelectBeginLine;
        forward = !forward;
      }
    }
  }

  // MoveCaret will return an error if it can't move in the specified
  // direction, but we just ignore this error unless it's a line move, in which
  // case we call nsISelectionController::CompleteMove to move the cursor to
  // the beginning/end of the line.
  RefPtr<nsFrameSelection> frameSelection = mFrameSelection;
  rv = frameSelection->MoveCaret(
      forward ? eDirNext : eDirPrevious, extend, amount,
      visual ? nsFrameSelection::eVisual : nsFrameSelection::eLogical);

  if (aGranularity.LowerCaseEqualsLiteral("line") && NS_FAILED(rv)) {
    RefPtr<PresShell> presShell = frameSelection->GetPresShell();
    if (!presShell) {
      return;
    }
    presShell->CompleteMove(forward, extend);
  }
}

void Selection::SetBaseAndExtentJS(nsINode& aAnchorNode, uint32_t aAnchorOffset,
                                   nsINode& aFocusNode, uint32_t aFocusOffset,
                                   ErrorResult& aRv) {
  AutoRestore<bool> calledFromJSRestorer(mCalledByJS);
  mCalledByJS = true;
  SetBaseAndExtent(aAnchorNode, aAnchorOffset, aFocusNode, aFocusOffset, aRv);
}

void Selection::SetBaseAndExtent(nsINode& aAnchorNode, uint32_t aAnchorOffset,
                                 nsINode& aFocusNode, uint32_t aFocusOffset,
                                 ErrorResult& aRv) {
  if (aAnchorOffset > aAnchorNode.Length()) {
    aRv.ThrowIndexSizeError(nsPrintfCString(
        "The anchor offset value %u is out of range", aAnchorOffset));
    return;
  }
  if (aFocusOffset > aFocusNode.Length()) {
    aRv.ThrowIndexSizeError(nsPrintfCString(
        "The focus offset value %u is out of range", aFocusOffset));
    return;
  }

  SetBaseAndExtent(RawRangeBoundary{&aAnchorNode, aAnchorOffset},
                   RawRangeBoundary{&aFocusNode, aFocusOffset}, aRv);
}

void Selection::SetBaseAndExtentInternal(InLimiter aInLimiter,
                                         const RawRangeBoundary& aAnchorRef,
                                         const RawRangeBoundary& aFocusRef,
                                         ErrorResult& aRv) {
  if (!mFrameSelection) {
    aRv.Throw(NS_ERROR_NOT_INITIALIZED);
    return;
  }

  if (NS_WARN_IF(!aAnchorRef.IsSet()) || NS_WARN_IF(!aFocusRef.IsSet())) {
    aRv.Throw(NS_ERROR_INVALID_ARG);
    return;
  }

  if (!HasSameRootOrSameComposedDoc(*aAnchorRef.Container()) ||
      !HasSameRootOrSameComposedDoc(*aFocusRef.Container())) {
    // Return with no error
    return;
  }

  // Prevent "selectionchange" event temporarily because it should be fired
  // after we set the direction.
  // XXX If they are disconnected, shouldn't we return error before allocating
  //     new nsRange instance?
  SelectionBatcher batch(this);
  const Maybe<int32_t> order =
      nsContentUtils::ComparePoints(aAnchorRef, aFocusRef);
  if (order && (*order <= 0)) {
    SetStartAndEndInternal(aInLimiter, aAnchorRef, aFocusRef, eDirNext, aRv);
    return;
  }

  // If there's no `order`, the range will be collapsed, unless another error is
  // detected before.
  SetStartAndEndInternal(aInLimiter, aFocusRef, aAnchorRef, eDirPrevious, aRv);
}

Result<Ok, nsresult> Selection::SetStartAndEndInLimiter(
    nsINode& aStartContainer, uint32_t aStartOffset, nsINode& aEndContainer,
    uint32_t aEndOffset, nsDirection aDirection, int16_t aReason) {
  if (mFrameSelection) {
    mFrameSelection->AddChangeReasons(aReason);
  }

  ErrorResult error;
  SetStartAndEndInternal(
      InLimiter::eYes, RawRangeBoundary(&aStartContainer, aStartOffset),
      RawRangeBoundary(&aEndContainer, aEndOffset), aDirection, error);
  MOZ_TRY(error.StealNSResult());
  return Ok();
}

void Selection::SetStartAndEndInternal(InLimiter aInLimiter,
                                       const RawRangeBoundary& aStartRef,
                                       const RawRangeBoundary& aEndRef,
                                       nsDirection aDirection,
                                       ErrorResult& aRv) {
  if (NS_WARN_IF(!aStartRef.IsSet()) || NS_WARN_IF(!aEndRef.IsSet())) {
    aRv.Throw(NS_ERROR_INVALID_ARG);
    return;
  }

  // Don't fire "selectionchange" event until everything done.
  SelectionBatcher batch(this);

  if (aInLimiter == InLimiter::eYes) {
    if (!mFrameSelection ||
        !mFrameSelection->IsValidSelectionPoint(aStartRef.Container())) {
      aRv.Throw(NS_ERROR_FAILURE);
      return;
    }
    if (aStartRef.Container() != aEndRef.Container() &&
        !mFrameSelection->IsValidSelectionPoint(aEndRef.Container())) {
      aRv.Throw(NS_ERROR_FAILURE);
      return;
    }
  }

  RefPtr<nsRange> newRange = nsRange::Create(aStartRef, aEndRef, aRv);
  if (aRv.Failed()) {
    return;
  }

  RemoveAllRanges(aRv);
  if (aRv.Failed()) {
    return;
  }

  AddRangeAndSelectFramesAndNotifyListeners(*newRange, aRv);
  if (aRv.Failed()) {
    return;
  }

  // Adding a range may set 2 or more ranges if there are non-selectable
  // contents only when this change is caused by a user operation.  Therefore,
  // we need to select frames with the result in such case.
  if (mUserInitiated) {
    RefPtr<nsPresContext> presContext = GetPresContext();
    if (mStyledRanges.Length() > 1 && presContext) {
      SelectFramesInAllRanges(presContext);
    }
  }

  SetDirection(aDirection);
}

/** SelectionLanguageChange modifies the cursor Bidi level after a change in
 * keyboard direction
 *  @param aLangRTL is true if the new language is right-to-left or false if the
 * new language is left-to-right
 */
nsresult Selection::SelectionLanguageChange(bool aLangRTL) {
  if (!mFrameSelection) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  RefPtr<nsFrameSelection> frameSelection = mFrameSelection;

  // if the direction of the language hasn't changed, nothing to do
  mozilla::intl::BidiEmbeddingLevel kbdBidiLevel =
      aLangRTL ? mozilla::intl::BidiEmbeddingLevel::RTL()
               : mozilla::intl::BidiEmbeddingLevel::LTR();
  if (kbdBidiLevel == frameSelection->mKbdBidiLevel) {
    return NS_OK;
  }

  frameSelection->mKbdBidiLevel = kbdBidiLevel;

  nsIFrame* focusFrame = GetPrimaryFrameForFocusNode(false);
  if (!focusFrame) {
    return NS_ERROR_FAILURE;
  }

  auto [frameStart, frameEnd] = focusFrame->GetOffsets();
  RefPtr<nsPresContext> context = GetPresContext();
  mozilla::intl::BidiEmbeddingLevel levelBefore, levelAfter;
  if (!context) {
    return NS_ERROR_FAILURE;
  }

  mozilla::intl::BidiEmbeddingLevel level = focusFrame->GetEmbeddingLevel();
  int32_t focusOffset = static_cast<int32_t>(FocusOffset());
  if ((focusOffset != frameStart) && (focusOffset != frameEnd))
    // the cursor is not at a frame boundary, so the level of both the
    // characters (logically) before and after the cursor is equal to the frame
    // level
    levelBefore = levelAfter = level;
  else {
    // the cursor is at a frame boundary, so use GetPrevNextBidiLevels to find
    // the level of the characters before and after the cursor
    nsCOMPtr<nsIContent> focusContent = do_QueryInterface(GetFocusNode());
    nsPrevNextBidiLevels levels =
        frameSelection->GetPrevNextBidiLevels(focusContent, focusOffset, false);

    levelBefore = levels.mLevelBefore;
    levelAfter = levels.mLevelAfter;
  }

  if (levelBefore.IsSameDirection(levelAfter)) {
    // if cursor is between two characters with the same orientation, changing
    // the keyboard language must toggle the cursor level between the level of
    // the character with the lowest level (if the new language corresponds to
    // the orientation of that character) and this level plus 1 (if the new
    // language corresponds to the opposite orientation)
    if ((level != levelBefore) && (level != levelAfter)) {
      level = std::min(levelBefore, levelAfter);
    }
    if (level.IsSameDirection(kbdBidiLevel)) {
      frameSelection->SetCaretBidiLevelAndMaybeSchedulePaint(level);
    } else {
      frameSelection->SetCaretBidiLevelAndMaybeSchedulePaint(
          mozilla::intl::BidiEmbeddingLevel(level + 1));
    }
  } else {
    // if cursor is between characters with opposite orientations, changing the
    // keyboard language must change the cursor level to that of the adjacent
    // character with the orientation corresponding to the new language.
    if (levelBefore.IsSameDirection(kbdBidiLevel)) {
      frameSelection->SetCaretBidiLevelAndMaybeSchedulePaint(levelBefore);
    } else {
      frameSelection->SetCaretBidiLevelAndMaybeSchedulePaint(levelAfter);
    }
  }

  // The caret might have moved, so invalidate the desired position
  // for future usages of up-arrow or down-arrow
  frameSelection->InvalidateDesiredCaretPos();

  return NS_OK;
}

void Selection::SetColors(const nsAString& aForegroundColor,
                          const nsAString& aBackgroundColor,
                          const nsAString& aAltForegroundColor,
                          const nsAString& aAltBackgroundColor,
                          ErrorResult& aRv) {
  if (mSelectionType != SelectionType::eFind) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  mCustomColors.reset(new SelectionCustomColors);

  constexpr auto currentColorStr = u"currentColor"_ns;
  constexpr auto transparentStr = u"transparent"_ns;

  if (!aForegroundColor.Equals(currentColorStr)) {
    nscolor foregroundColor;
    nsAttrValue aForegroundColorValue;
    aForegroundColorValue.ParseColor(aForegroundColor);
    if (!aForegroundColorValue.GetColorValue(foregroundColor)) {
      aRv.Throw(NS_ERROR_INVALID_ARG);
      return;
    }
    mCustomColors->mForegroundColor = Some(foregroundColor);
  } else {
    mCustomColors->mForegroundColor = Nothing();
  }

  if (!aBackgroundColor.Equals(transparentStr)) {
    nscolor backgroundColor;
    nsAttrValue aBackgroundColorValue;
    aBackgroundColorValue.ParseColor(aBackgroundColor);
    if (!aBackgroundColorValue.GetColorValue(backgroundColor)) {
      aRv.Throw(NS_ERROR_INVALID_ARG);
      return;
    }
    mCustomColors->mBackgroundColor = Some(backgroundColor);
  } else {
    mCustomColors->mBackgroundColor = Nothing();
  }

  if (!aAltForegroundColor.Equals(currentColorStr)) {
    nscolor altForegroundColor;
    nsAttrValue aAltForegroundColorValue;
    aAltForegroundColorValue.ParseColor(aAltForegroundColor);
    if (!aAltForegroundColorValue.GetColorValue(altForegroundColor)) {
      aRv.Throw(NS_ERROR_INVALID_ARG);
      return;
    }
    mCustomColors->mAltForegroundColor = Some(altForegroundColor);
  } else {
    mCustomColors->mAltForegroundColor = Nothing();
  }

  if (!aAltBackgroundColor.Equals(transparentStr)) {
    nscolor altBackgroundColor;
    nsAttrValue aAltBackgroundColorValue;
    aAltBackgroundColorValue.ParseColor(aAltBackgroundColor);
    if (!aAltBackgroundColorValue.GetColorValue(altBackgroundColor)) {
      aRv.Throw(NS_ERROR_INVALID_ARG);
      return;
    }
    mCustomColors->mAltBackgroundColor = Some(altBackgroundColor);
  } else {
    mCustomColors->mAltBackgroundColor = Nothing();
  }
}

void Selection::ResetColors(ErrorResult& aRv) { mCustomColors = nullptr; }

JSObject* Selection::WrapObject(JSContext* aCx,
                                JS::Handle<JSObject*> aGivenProto) {
  return mozilla::dom::Selection_Binding::Wrap(aCx, this, aGivenProto);
}

// AutoHideSelectionChanges
AutoHideSelectionChanges::AutoHideSelectionChanges(
    const nsFrameSelection* aFrame)
    : AutoHideSelectionChanges(
          aFrame ? aFrame->GetSelection(SelectionType::eNormal) : nullptr) {}

bool Selection::HasSameRootOrSameComposedDoc(const nsINode& aNode) {
  nsINode* root = aNode.SubtreeRoot();
  Document* doc = GetDocument();
  return doc == root || (root && doc == root->GetComposedDoc());
}
