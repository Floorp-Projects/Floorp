/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Implementation of mozilla::dom::Selection
 */

#include "mozilla/dom/Selection.h"

#include "mozilla/Attributes.h"
#include "mozilla/AutoRestore.h"
#include "mozilla/EventStates.h"
#include "mozilla/HTMLEditor.h"
#include "mozilla/RangeBoundary.h"

#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsFrameSelection.h"
#include "nsISelectionListener.h"
#include "nsContentCID.h"
#include "nsDeviceContext.h"
#include "nsIContent.h"
#include "nsRange.h"
#include "nsITableCellLayout.h"
#include "nsTArray.h"
#include "nsTableWrapperFrame.h"
#include "nsTableCellFrame.h"
#include "nsIScrollableFrame.h"
#include "nsCCUncollectableMarker.h"
#include "nsIContentIterator.h"
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
#include "nsIPresShell.h"
#include "nsCaret.h"

#include "nsITimer.h"
#include "nsIDocument.h"
#include "nsINamed.h"

#include "nsISelectionController.h" //for the enums
#include "nsAutoCopyListener.h"
#include "SelectionChangeListener.h"
#include "nsCopySupport.h"
#include "nsIClipboard.h"
#include "nsIFrameInlines.h"
#include "nsRefreshDriver.h"
#include "nsIBidiKeyboard.h"

#include "nsError.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/ShadowRoot.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/SelectionBinding.h"
#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/Telemetry.h"
#include "nsViewManager.h"

#include "nsFocusManager.h"
#include "nsPIDOMWindow.h"

using namespace mozilla;
using namespace mozilla::dom;

//#define DEBUG_TABLE 1

static bool IsValidSelectionPoint(nsFrameSelection *aFrameSel, nsINode *aNode);

#ifdef PRINT_RANGE
static void printRange(nsRange *aDomRange);
#define DEBUG_OUT_RANGE(x)  printRange(x)
#else
#define DEBUG_OUT_RANGE(x)
#endif // PRINT_RANGE

/******************************************************************************
 * Utility methods defined in nsISelectionController.idl
 ******************************************************************************/

namespace mozilla {

const char*
ToChar(SelectionType aSelectionType)
{
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

} // namespace mozilla

//#define DEBUG_SELECTION // uncomment for printf describing every collapse and extend.
//#define DEBUG_NAVIGATION


//#define DEBUG_TABLE_SELECTION 1

struct CachedOffsetForFrame {
  CachedOffsetForFrame()
  : mCachedFrameOffset(0, 0) // nsPoint ctor
  , mLastCaretFrame(nullptr)
  , mLastContentOffset(0)
  , mCanCacheFrameOffset(false)
  {}

  nsPoint      mCachedFrameOffset;      // cached frame offset
  nsIFrame*    mLastCaretFrame;         // store the frame the caret was last drawn in.
  int32_t      mLastContentOffset;      // store last content offset
  bool mCanCacheFrameOffset;    // cached frame offset is valid?
};

class nsAutoScrollTimer final : public nsITimerCallback
                              , public nsINamed
{
public:

  NS_DECL_ISUPPORTS

  nsAutoScrollTimer()
  : mFrameSelection(0), mSelection(0), mPresContext(0), mPoint(0,0), mDelay(30)
  {
  }

  // aPoint is relative to aPresContext's root frame
  nsresult Start(nsPresContext *aPresContext, nsPoint &aPoint)
  {
    mPoint = aPoint;

    // Store the presentation context. The timer will be
    // stopped by the selection if the prescontext is destroyed.
    mPresContext = aPresContext;

    mContent = nsIPresShell::GetCapturingContent();

    if (!mTimer) {
      mTimer = NS_NewTimer(
        mPresContext->Document()->EventTargetFor(TaskCategory::Other));

      if (!mTimer) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
    }

    return mTimer->InitWithCallback(this, mDelay, nsITimer::TYPE_ONE_SHOT);
  }

  nsresult Stop()
  {
    if (mTimer) {
      mTimer->Cancel();
      mTimer = nullptr;
    }

    mContent = nullptr;
    return NS_OK;
  }

  nsresult Init(nsFrameSelection* aFrameSelection, Selection* aSelection)
  {
    mFrameSelection = aFrameSelection;
    mSelection = aSelection;
    return NS_OK;
  }

  nsresult SetDelay(uint32_t aDelay)
  {
    mDelay = aDelay;
    return NS_OK;
  }

  NS_IMETHOD Notify(nsITimer *timer) override
  {
    if (mSelection && mPresContext)
    {
      AutoWeakFrame frame =
        mContent ? mPresContext->GetPrimaryFrameFor(mContent) : nullptr;
      if (!frame) {
        return NS_OK;
      }
      mContent = nullptr;

      nsPoint pt = mPoint -
        frame->GetOffsetTo(mPresContext->PresShell()->GetRootFrame());
      RefPtr<nsFrameSelection> frameSelection = mFrameSelection;
      frameSelection->HandleDrag(frame, pt);
      if (!frame.IsAlive()) {
        return NS_OK;
      }

      NS_ASSERTION(frame->PresContext() == mPresContext, "document mismatch?");
      mSelection->DoAutoScroll(frame, pt);
    }
    return NS_OK;
  }

  NS_IMETHOD GetName(nsACString& aName) override
  {
    aName.AssignLiteral("nsAutoScrollTimer");
    return NS_OK;
  }

protected:
  virtual ~nsAutoScrollTimer()
  {
    if (mTimer) {
      mTimer->Cancel();
    }
  }

private:
  nsFrameSelection *mFrameSelection;
  Selection* mSelection;
  nsPresContext *mPresContext;
  // relative to mPresContext's root frame
  nsPoint mPoint;
  nsCOMPtr<nsITimer> mTimer;
  nsCOMPtr<nsIContent> mContent;
  uint32_t mDelay;
};

NS_IMPL_ISUPPORTS(nsAutoScrollTimer, nsITimerCallback, nsINamed)

/*
The limiter is used specifically for the text areas and textfields
In that case it is the DIV tag that is anonymously created for the text
areas/fields.  Text nodes and BR nodes fall beneath it.  In the case of a
BR node the limiter will be the parent and the offset will point before or
after the BR node.  In the case of the text node the parent content is
the text node itself and the offset will be the exact character position.
The offset is not important to check for validity.  Simply look at the
passed in content.  If it equals the limiter then the selection point is valid.
If its parent it the limiter then the point is also valid.  In the case of
NO limiter all points are valid since you are in a topmost iframe. (browser
or composer)
*/
bool
IsValidSelectionPoint(nsFrameSelection *aFrameSel, nsINode *aNode)
{
  if (!aFrameSel || !aNode)
    return false;

  nsIContent *limiter = aFrameSel->GetLimiter();
  if (limiter && limiter != aNode && limiter != aNode->GetParent()) {
    //if newfocus == the limiter. that's ok. but if not there and not parent bad
    return false; //not in the right content. tLimiter said so
  }

  limiter = aFrameSel->GetAncestorLimiter();
  return !limiter || nsContentUtils::ContentIsDescendantOf(aNode, limiter);
}

namespace mozilla {
struct MOZ_RAII AutoPrepareFocusRange
{
  AutoPrepareFocusRange(Selection* aSelection,
                        bool aContinueSelection,
                        bool aMultipleSelection
                        MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
  {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;

    if (aSelection->mRanges.Length() <= 1) {
      return;
    }

    if (aSelection->mFrameSelection->IsUserSelectionReason()) {
      mUserSelect.emplace(aSelection);
    }
    bool userSelection = aSelection->mUserInitiated;

    nsTArray<RangeData>& ranges = aSelection->mRanges;
    if (!userSelection ||
        (!aContinueSelection && aMultipleSelection)) {
      // Scripted command or the user is starting a new explicit multi-range
      // selection.
      for (RangeData& entry : ranges) {
        entry.mRange->SetIsGenerated(false);
      }
      return;
    }

    int16_t reason = aSelection->mFrameSelection->mSelectionChangeReason;
    bool isAnchorRelativeOp = (reason & (nsISelectionListener::DRAG_REASON |
                                         nsISelectionListener::MOUSEDOWN_REASON |
                                         nsISelectionListener::MOUSEUP_REASON |
                                         nsISelectionListener::COLLAPSETOSTART_REASON));
    if (!isAnchorRelativeOp) {
      return;
    }

    // This operation is against the anchor but our current mAnchorFocusRange
    // represents the focus in a multi-range selection.  The anchor from a user
    // perspective is the most distant generated range on the opposite side.
    // Find that range and make it the mAnchorFocusRange.
    const size_t len = ranges.Length();
    size_t newAnchorFocusIndex = size_t(-1);
    if (aSelection->GetDirection() == eDirNext) {
      for (size_t i = 0; i < len; ++i) {
        if (ranges[i].mRange->IsGenerated()) {
          newAnchorFocusIndex = i;
          break;
        }
      }
    } else {
      size_t i = len;
      while (i--) {
        if (ranges[i].mRange->IsGenerated()) {
          newAnchorFocusIndex = i;
          break;
        }
      }
    }

    if (newAnchorFocusIndex == size_t(-1)) {
      // There are no generated ranges - that's fine.
      return;
    }

    // Setup the new mAnchorFocusRange and mark the old one as generated.
    if (aSelection->mAnchorFocusRange) {
      aSelection->mAnchorFocusRange->SetIsGenerated(true);
    }
    nsRange* range = ranges[newAnchorFocusIndex].mRange;
    range->SetIsGenerated(false);
    aSelection->mAnchorFocusRange = range;

    // Remove all generated ranges (including the old mAnchorFocusRange).
    RefPtr<nsPresContext> presContext = aSelection->GetPresContext();
    size_t i = len;
    while (i--) {
      range = aSelection->mRanges[i].mRange;
      if (range->IsGenerated()) {
        range->SetSelection(nullptr);
        aSelection->SelectFrames(presContext, range, false);
        aSelection->mRanges.RemoveElementAt(i);
      }
    }
    if (aSelection->mFrameSelection) {
      aSelection->mFrameSelection->InvalidateDesiredPos();
    }
  }

  Maybe<Selection::AutoUserInitiated> mUserSelect;
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

} // namespace mozilla


#ifdef PRINT_RANGE
void printRange(nsRange *aDomRange)
{
  if (!aDomRange)
  {
    printf("NULL Range\n");
  }
  nsINode* startNode = aDomRange->GetStartContainer();
  nsINode* endNode = aDomRange->GetEndContainer();
  int32_t startOffset = aDomRange->StartOffset();
  int32_t endOffset = aDomRange->EndOffset();

  printf("range: 0x%lx\t start: 0x%lx %ld, \t end: 0x%lx,%ld\n",
         (unsigned long)aDomRange,
         (unsigned long)startNode, (long)startOffset,
         (unsigned long)endNode, (long)endOffset);

}
#endif /* PRINT_RANGE */

void
Selection::Stringify(nsAString& aResult)
{
  // We need FlushType::Frames here to make sure frames have been created for
  // the selected content.  Use mFrameSelection->GetShell() which returns
  // null if the Selection has been disconnected (the shell is Destroyed).
  nsCOMPtr<nsIPresShell> shell =
    mFrameSelection ? mFrameSelection->GetShell() : nullptr;
  if (!shell) {
    aResult.Truncate();
    return;
  }
  shell->FlushPendingNotifications(FlushType::Frames);

  IgnoredErrorResult rv;
  ToStringWithFormat(NS_LITERAL_STRING("text/plain"),
                     nsIDocumentEncoder::SkipInvisibleContent,
                     0, aResult, rv);
  if (rv.Failed()) {
    aResult.Truncate();
  }
}

void
Selection::ToStringWithFormat(const nsAString& aFormatType, uint32_t aFlags,
                              int32_t aWrapCol, nsAString& aReturn,
                              ErrorResult& aRv)
{
  nsresult rv = NS_OK;
  NS_ConvertUTF8toUTF16 formatType( NS_DOC_ENCODER_CONTRACTID_BASE );
  formatType.Append(aFormatType);
  nsCOMPtr<nsIDocumentEncoder> encoder =
           do_CreateInstance(NS_ConvertUTF16toUTF8(formatType).get(), &rv);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return;
  }

  nsIPresShell* shell = GetPresShell();
  if (!shell) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  nsIDocument *doc = shell->GetDocument();

  // Flags should always include OutputSelectionOnly if we're coming from here:
  aFlags |= nsIDocumentEncoder::OutputSelectionOnly;
  nsAutoString readstring;
  readstring.Assign(aFormatType);
  rv = encoder->Init(doc, readstring, aFlags);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return;
  }

  encoder->SetSelection(this);
  if (aWrapCol != 0)
    encoder->SetWrapColumn(aWrapCol);

  rv = encoder->EncodeToString(aReturn);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
  }
}

void
Selection::SetInterlinePosition(bool aHintRight, ErrorResult& aRv)
{
  if (!mFrameSelection) {
    aRv.Throw(NS_ERROR_NOT_INITIALIZED); // Can't do selection
    return;
  }
  mFrameSelection->SetHint(aHintRight ? CARET_ASSOCIATE_AFTER : CARET_ASSOCIATE_BEFORE);
}

bool
Selection::GetInterlinePosition(ErrorResult& aRv)
{
  if (!mFrameSelection) {
    aRv.Throw(NS_ERROR_NOT_INITIALIZED); // Can't do selection
    return false;
  }
  return mFrameSelection->GetHint() == CARET_ASSOCIATE_AFTER;
}

Nullable<int16_t>
Selection::GetCaretBidiLevel(mozilla::ErrorResult& aRv) const
{
  if (!mFrameSelection) {
    aRv.Throw(NS_ERROR_NOT_INITIALIZED);
    return Nullable<int16_t>();
  }
  nsBidiLevel caretBidiLevel = mFrameSelection->GetCaretBidiLevel();
  return (caretBidiLevel & BIDI_LEVEL_UNDEFINED) ?
    Nullable<int16_t>() : Nullable<int16_t>(caretBidiLevel);
}

void
Selection::SetCaretBidiLevel(const Nullable<int16_t>& aCaretBidiLevel, mozilla::ErrorResult& aRv)
{
  if (!mFrameSelection) {
    aRv.Throw(NS_ERROR_NOT_INITIALIZED);
    return;
  }
  if (aCaretBidiLevel.IsNull()) {
    mFrameSelection->UndefineCaretBidiLevel();
  } else {
    mFrameSelection->SetCaretBidiLevel(aCaretBidiLevel.Value());
  }
}

nsresult
Selection::GetTableCellLocationFromRange(nsRange* aRange,
                                         TableSelection* aSelectionType,
                                         int32_t* aRow, int32_t* aCol)
{
  if (!aRange || !aSelectionType || !aRow || !aCol)
    return NS_ERROR_NULL_POINTER;

  *aSelectionType = TableSelection::None;
  *aRow = 0;
  *aCol = 0;

  // Must have access to frame selection to get cell info
  if (!mFrameSelection) return NS_OK;

  nsresult result = GetTableSelectionType(aRange, aSelectionType);
  if (NS_FAILED(result)) return result;

  // Don't fail if range does not point to a single table cell,
  //  let aSelectionType tell user if we don't have a cell
  if (*aSelectionType  != TableSelection::Cell) {
    return NS_OK;
  }

  // Get the child content (the cell) pointed to by starting node of range
  // We do minimal checking since GetTableSelectionType assures
  //   us that this really is a table cell
  nsCOMPtr<nsIContent> child = aRange->GetChildAtStartOffset();
  if (!child)
    return NS_ERROR_FAILURE;

  // GetCellLayout depends on current frame, we need flush frame to get
  // nsITableCellLayout
  nsCOMPtr<nsIPresShell> presShell = mFrameSelection->GetShell();
  if (presShell) {
    presShell->FlushPendingNotifications(FlushType::Frames);

    // Since calling FlushPendingNotifications, so check whether disconnected.
    if (!mFrameSelection || !mFrameSelection->GetShell()) {
      return NS_ERROR_FAILURE;
    }
  }

  //Note: This is a non-ref-counted pointer to the frame
  nsITableCellLayout *cellLayout = mFrameSelection->GetCellLayout(child);
  if (!cellLayout)
    return NS_ERROR_FAILURE;

  return cellLayout->GetCellIndexes(*aRow, *aCol);
}

nsresult
Selection::AddTableCellRange(nsRange* aRange, bool* aDidAddRange,
                             int32_t* aOutIndex)
{
  if (!aDidAddRange || !aOutIndex)
    return NS_ERROR_NULL_POINTER;

  *aDidAddRange = false;
  *aOutIndex = -1;

  if (!mFrameSelection)
    return NS_OK;

  if (!aRange)
    return NS_ERROR_NULL_POINTER;

  nsresult result;

  // Get if we are adding a cell selection and the row, col of cell if we are
  int32_t newRow, newCol;
  TableSelection tableMode;
  result = GetTableCellLocationFromRange(aRange, &tableMode, &newRow, &newCol);
  if (NS_FAILED(result)) return result;

  // If not adding a cell range, we are done here
  if (tableMode != TableSelection::Cell)
  {
    mFrameSelection->mSelectingTableCellMode = tableMode;
    // Don't fail if range isn't a selected cell, aDidAddRange tells caller if we didn't proceed
    return NS_OK;
  }

  // Set frame selection mode only if not already set to a table mode
  //  so we don't lose the select row and column flags (not detected by getTableCellLocation)
  if (mFrameSelection->mSelectingTableCellMode == TableSelection::None)
    mFrameSelection->mSelectingTableCellMode = tableMode;

  *aDidAddRange = true;
  return AddItem(aRange, aOutIndex);
}

//TODO: Figure out TableSelection::Column and TableSelection::AllCells
nsresult
Selection::GetTableSelectionType(nsRange* aRange,
                                 TableSelection* aTableSelectionType)
{
  if (!aRange || !aTableSelectionType)
    return NS_ERROR_NULL_POINTER;

  *aTableSelectionType = TableSelection::None;

  // Must have access to frame selection to get cell info
  if(!mFrameSelection) return NS_OK;

  nsINode* startNode = aRange->GetStartContainer();
  if (!startNode) return NS_ERROR_FAILURE;

  nsINode* endNode = aRange->GetEndContainer();
  if (!endNode) return NS_ERROR_FAILURE;

  // Not a single selected node
  if (startNode != endNode) return NS_OK;

  nsIContent* child = aRange->GetChildAtStartOffset();

  // Not a single selected node
  if (!child || child->GetNextSibling() != aRange->GetChildAtEndOffset()) {
    return NS_OK;
  }

  nsIContent* startContent = static_cast<nsIContent*>(startNode);
  if (!(startNode->IsElement() && startContent->IsHTMLElement())) {
    // Implies a check for being an element; if we ever make this work
    // for non-HTML, need to keep checking for elements.
    return NS_OK;
  }

  if (startContent->IsHTMLElement(nsGkAtoms::tr))
  {
    *aTableSelectionType = TableSelection::Cell;
  }
  else //check to see if we are selecting a table or row (column and all cells not done yet)
  {
    if (child->IsHTMLElement(nsGkAtoms::table))
      *aTableSelectionType = TableSelection::Table;
    else if (child->IsHTMLElement(nsGkAtoms::tr))
      *aTableSelectionType = TableSelection::Row;
  }

  return NS_OK;
}

Selection::Selection()
  : mCachedOffsetForFrame(nullptr)
  , mDirection(eDirNext)
  , mSelectionType(SelectionType::eNormal)
  , mCustomColors(nullptr)
  , mUserInitiated(false)
  , mCalledByJS(false)
  , mSelectionChangeBlockerCount(0)
{
}

Selection::Selection(nsFrameSelection* aList)
  : mFrameSelection(aList)
  , mCachedOffsetForFrame(nullptr)
  , mDirection(eDirNext)
  , mSelectionType(SelectionType::eNormal)
  , mCustomColors(nullptr)
  , mUserInitiated(false)
  , mCalledByJS(false)
  , mSelectionChangeBlockerCount(0)
{
}

Selection::~Selection()
{
  SetAnchorFocusRange(-1);

  uint32_t count = mRanges.Length();
  for (uint32_t i = 0; i < count; ++i) {
    mRanges[i].mRange->SetSelection(nullptr);
  }

  if (mAutoScrollTimer) {
    mAutoScrollTimer->Stop();
    mAutoScrollTimer = nullptr;
  }

  mScrollEvent.Revoke();

  if (mCachedOffsetForFrame) {
    delete mCachedOffsetForFrame;
    mCachedOffsetForFrame = nullptr;
  }
}

nsIDocument*
Selection::GetParentObject() const
{
  nsIPresShell* shell = GetPresShell();
  if (shell) {
    return shell->GetDocument();
  }
  return nullptr;
}

DocGroup*
Selection::GetDocGroup() const
{
  nsIPresShell* shell = GetPresShell();
  if (!shell) {
    return nullptr;
  }

  nsIDocument* doc = shell->GetDocument();
  return doc ? doc->GetDocGroup() : nullptr;
}

NS_IMPL_CYCLE_COLLECTION_CLASS(Selection)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(Selection)
  // Unlink the selection listeners *before* we do RemoveAllRanges since
  // we don't want to notify the listeners during JS GC (they could be
  // in JS!).
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mSelectionListeners)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mCachedRange)
  tmp->RemoveAllRanges(IgnoreErrors());
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mFrameSelection)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(Selection)
  {
    uint32_t i, count = tmp->mRanges.Length();
    for (i = 0; i < count; ++i) {
      NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mRanges[i].mRange)
    }
  }
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mAnchorFocusRange)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCachedRange)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mFrameSelection)
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
NS_IMPL_MAIN_THREAD_ONLY_CYCLE_COLLECTING_RELEASE(Selection)

const RangeBoundary&
Selection::AnchorRef()
{
  if (!mAnchorFocusRange) {
    static RangeBoundary sEmpty;
    return sEmpty;
  }

  if (GetDirection() == eDirNext) {
    return mAnchorFocusRange->StartRef();
  }

  return mAnchorFocusRange->EndRef();
}

const RangeBoundary&
Selection::FocusRef()
{
  if (!mAnchorFocusRange) {
    static RangeBoundary sEmpty;
    return sEmpty;
  }

  if (GetDirection() == eDirNext){
    return mAnchorFocusRange->EndRef();
  }

  return mAnchorFocusRange->StartRef();
}

void
Selection::SetAnchorFocusRange(int32_t indx)
{
  if (indx >= (int32_t)mRanges.Length())
    return;
  if (indx < 0) //release all
  {
    mAnchorFocusRange = nullptr;
  }
  else{
    mAnchorFocusRange = mRanges[indx].mRange;
  }
}

static nsresult
CompareToRangeStart(nsINode* aCompareNode, int32_t aCompareOffset,
                    nsRange* aRange, int32_t* aCmp)
{
  nsINode* start = aRange->GetStartContainer();
  NS_ENSURE_STATE(aCompareNode && start);
  // If the nodes that we're comparing are not in the same document or in the
  // same subtree, assume that aCompareNode will fall at the end of the ranges.
  if (aCompareNode->GetComposedDoc() != start->GetComposedDoc() ||
      !start->GetComposedDoc() ||
      aCompareNode->SubtreeRoot() != start->SubtreeRoot()) {
    *aCmp = 1;
  } else {
    *aCmp = nsContentUtils::ComparePoints(aCompareNode, aCompareOffset,
                                          start, aRange->StartOffset());
  }
  return NS_OK;
}

static nsresult
CompareToRangeEnd(nsINode* aCompareNode, int32_t aCompareOffset,
                  nsRange* aRange, int32_t* aCmp)
{
  nsINode* end = aRange->GetEndContainer();
  NS_ENSURE_STATE(aCompareNode && end);
  // If the nodes that we're comparing are not in the same document or in the
  // same subtree, assume that aCompareNode will fall at the end of the ranges.
  if (aCompareNode->GetComposedDoc() != end->GetComposedDoc() ||
      !end->GetComposedDoc() ||
      aCompareNode->SubtreeRoot() != end->SubtreeRoot()) {
    *aCmp = 1;
  } else {
    *aCmp = nsContentUtils::ComparePoints(aCompareNode, aCompareOffset,
                                          end, aRange->EndOffset());
  }
  return NS_OK;
}

// Selection::FindInsertionPoint
//
//    Binary searches the given sorted array of ranges for the insertion point
//    for the given node/offset. The given comparator is used, and the index
//    where the point should appear in the array is placed in *aInsertionPoint.
//
//    If there is an item in the array equal to the input point, we will return
//    the index of this item.

nsresult
Selection::FindInsertionPoint(
    nsTArray<RangeData>* aElementArray,
    nsINode* aPointNode, int32_t aPointOffset,
    nsresult (*aComparator)(nsINode*,int32_t,nsRange*,int32_t*),
    int32_t* aPoint)
{
  *aPoint = 0;
  int32_t beginSearch = 0;
  int32_t endSearch = aElementArray->Length(); // one beyond what to check

  if (endSearch) {
    int32_t center = endSearch - 1; // Check last index, then binary search
    do {
      nsRange* range = (*aElementArray)[center].mRange;

      int32_t cmp;
      nsresult rv = aComparator(aPointNode, aPointOffset, range, &cmp);
      NS_ENSURE_SUCCESS(rv, rv);

      if (cmp < 0) {        // point < cur
        endSearch = center;
      } else if (cmp > 0) { // point > cur
        beginSearch = center + 1;
      } else {              // found match, done
        beginSearch = center;
        break;
      }
      center = (endSearch - beginSearch) / 2 + beginSearch;
    } while (endSearch - beginSearch > 0);
  }

  *aPoint = beginSearch;
  return NS_OK;
}

// Selection::SubtractRange
//
//    A helper function that subtracts aSubtract from aRange, and adds
//    1 or 2 RangeData objects representing the remaining non-overlapping
//    difference to aOutput. It is assumed that the caller has checked that
//    aRange and aSubtract do indeed overlap

nsresult
Selection::SubtractRange(RangeData* aRange, nsRange* aSubtract,
                         nsTArray<RangeData>* aOutput)
{
  nsRange* range = aRange->mRange;

  // First we want to compare to the range start
  int32_t cmp;
  nsresult rv = CompareToRangeStart(range->GetStartContainer(),
                                    range->StartOffset(),
                                    aSubtract, &cmp);
  NS_ENSURE_SUCCESS(rv, rv);

  // Also, make a comparison to the range end
  int32_t cmp2;
  rv = CompareToRangeEnd(range->GetEndContainer(),
                         range->EndOffset(),
                         aSubtract, &cmp2);
  NS_ENSURE_SUCCESS(rv, rv);

  // If the existing range left overlaps the new range (aSubtract) then
  // cmp < 0, and cmp2 < 0
  // If it right overlaps the new range then cmp > 0 and cmp2 > 0
  // If it fully contains the new range, then cmp < 0 and cmp2 > 0

  if (cmp2 > 0) {
    // We need to add a new RangeData to the output, running from
    // the end of aSubtract to the end of range
    RefPtr<nsRange> postOverlap = new nsRange(aSubtract->GetEndContainer());
    rv = postOverlap->SetStartAndEnd(
                        aSubtract->GetEndContainer(), aSubtract->EndOffset(),
                        range->GetEndContainer(), range->EndOffset());
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    if (!postOverlap->Collapsed()) {
      if (!aOutput->InsertElementAt(0, RangeData(postOverlap)))
        return NS_ERROR_OUT_OF_MEMORY;
      (*aOutput)[0].mTextRangeStyle = aRange->mTextRangeStyle;
    }
  }

  if (cmp < 0) {
    // We need to add a new RangeData to the output, running from
    // the start of the range to the start of aSubtract
    RefPtr<nsRange> preOverlap = new nsRange(range->GetStartContainer());
    rv = preOverlap->SetStartAndEnd(range->GetStartContainer(),
                                    range->StartOffset(),
                                    aSubtract->GetStartContainer(),
                                    aSubtract->StartOffset());
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    if (!preOverlap->Collapsed()) {
      if (!aOutput->InsertElementAt(0, RangeData(preOverlap)))
        return NS_ERROR_OUT_OF_MEMORY;
      (*aOutput)[0].mTextRangeStyle = aRange->mTextRangeStyle;
    }
  }

  return NS_OK;
}

void
Selection::UserSelectRangesToAdd(nsRange* aItem, nsTArray<RefPtr<nsRange>>& aRangesToAdd)
{
  aItem->ExcludeNonSelectableNodes(&aRangesToAdd);
  if (aRangesToAdd.IsEmpty()) {
    ErrorResult err;
    nsINode* node = aItem->GetStartContainer(err);
    if (node && node->IsContent() && node->AsContent()->GetEditingHost()) {
      // A contenteditable node with user-select:none, for example.
      // Allow it to have a collapsed selection (for the caret).
      aItem->Collapse(GetDirection() == eDirPrevious);
      aRangesToAdd.AppendElement(aItem);
    }
  }
}

nsresult
Selection::AddItem(nsRange* aItem, int32_t* aOutIndex, bool aNoStartSelect)
{
  if (!aItem)
    return NS_ERROR_NULL_POINTER;
  if (!aItem->IsPositioned())
    return NS_ERROR_UNEXPECTED;

  NS_ASSERTION(aOutIndex, "aOutIndex can't be null");

  if (mUserInitiated) {
    AutoTArray<RefPtr<nsRange>, 4> rangesToAdd;
    *aOutIndex = int32_t(mRanges.Length()) - 1;

    nsIDocument* doc = GetParentObject();
    bool selectEventsEnabled =
      nsFrameSelection::sSelectionEventsEnabled ||
      (doc && nsContentUtils::IsSystemPrincipal(doc->NodePrincipal()));

    if (!aNoStartSelect &&
        mSelectionType == SelectionType::eNormal &&
        selectEventsEnabled && IsCollapsed() &&
        !IsBlockingSelectionChangeEvents()) {
      // First, we generate the ranges to add with a scratch range, which is a
      // clone of the original range passed in. We do this seperately, because the
      // selectstart event could have caused the world to change, and required
      // ranges to be re-generated
      RefPtr<nsRange> scratchRange = aItem->CloneRange();
      UserSelectRangesToAdd(scratchRange, rangesToAdd);
      bool newRangesNonEmpty = rangesToAdd.Length() > 1 ||
        (rangesToAdd.Length() == 1 && !rangesToAdd[0]->Collapsed());

      MOZ_ASSERT(!newRangesNonEmpty || nsContentUtils::IsSafeToRunScript());
      if (newRangesNonEmpty && nsContentUtils::IsSafeToRunScript()) {
        // We consider a selection to be starting if we are currently collapsed,
        // and the selection is becoming uncollapsed, and this is caused by a user
        // initiated event.
        bool defaultAction = true;

        // The spec currently doesn't say that we should dispatch this event
        // on text controls, so for now we only support doing that under a
        // pref, disabled by default.
        // See https://github.com/w3c/selection-api/issues/53.
        bool dispatchEvent = true;
        nsCOMPtr<nsINode> target = aItem->GetStartContainer();
        if (nsFrameSelection::sSelectionEventsOnTextControlsEnabled) {
          // Get the first element which isn't in a native anonymous subtree
          while (target && target->IsInNativeAnonymousSubtree()) {
            target = target->GetParent();
          }
        } else {
          if (target->IsInNativeAnonymousSubtree()) {
            // This is a selection under a text control, so don't dispatch the
            // event.
            dispatchEvent = false;
          }
        }

        if (dispatchEvent) {
          nsContentUtils::DispatchTrustedEvent(GetParentObject(), target,
                                               NS_LITERAL_STRING("selectstart"),
                                               true, true, &defaultAction);

          if (!defaultAction) {
            return NS_OK;
          }

          // As we just dispatched an event to the DOM, something could have
          // changed under our feet. Re-generate the rangesToAdd array, and ensure
          // that the range we are about to add is still valid.
          if (!aItem->IsPositioned()) {
            return NS_ERROR_UNEXPECTED;
          }
        }
      }

      // The scratch ranges we generated may be invalid now, throw them out
      rangesToAdd.ClearAndRetainStorage();
    }

    // Generate the ranges to add
    UserSelectRangesToAdd(aItem, rangesToAdd);
    size_t newAnchorFocusIndex =
      GetDirection() == eDirPrevious ? 0 : rangesToAdd.Length() - 1;
    for (size_t i = 0; i < rangesToAdd.Length(); ++i) {
      int32_t index;
      nsresult rv = AddItemInternal(rangesToAdd[i], &index);
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
  return AddItemInternal(aItem, aOutIndex);
}

nsresult
Selection::AddItemInternal(nsRange* aItem, int32_t* aOutIndex)
{
  MOZ_ASSERT(aItem);
  MOZ_ASSERT(aItem->IsPositioned());
  MOZ_ASSERT(aOutIndex);

  *aOutIndex = -1;

  // a common case is that we have no ranges yet
  if (mRanges.Length() == 0) {
    if (!mRanges.AppendElement(RangeData(aItem)))
      return NS_ERROR_OUT_OF_MEMORY;
    aItem->SetSelection(this);

    *aOutIndex = 0;
    return NS_OK;
  }

  int32_t startIndex, endIndex;
  nsresult rv = GetIndicesForInterval(aItem->GetStartContainer(),
                                      aItem->StartOffset(),
                                      aItem->GetEndContainer(),
                                      aItem->EndOffset(), false,
                                      &startIndex, &endIndex);
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

  // If the range is already contained in mRanges, silently succeed
  bool sameRange = EqualsRangeAtPoint(aItem->GetStartContainer(),
                                      aItem->StartOffset(),
                                      aItem->GetEndContainer(),
                                      aItem->EndOffset(), startIndex);
  if (sameRange) {
    *aOutIndex = startIndex;
    return NS_OK;
  }

  if (startIndex == endIndex) {
    // The new range doesn't overlap any existing ranges
    if (!mRanges.InsertElementAt(startIndex, RangeData(aItem)))
      return NS_ERROR_OUT_OF_MEMORY;
    aItem->SetSelection(this);
    *aOutIndex = startIndex;
    return NS_OK;
  }

  // We now know that at least 1 existing range overlaps with the range that
  // we are trying to add. In fact, the only ranges of interest are those at
  // the two end points, startIndex and endIndex - 1 (which may point to the
  // same range) as these may partially overlap the new range. Any ranges
  // between these indices are fully overlapped by the new range, and so can be
  // removed
  nsTArray<RangeData> overlaps;
  if (!overlaps.InsertElementAt(0, mRanges[startIndex]))
    return NS_ERROR_OUT_OF_MEMORY;

  if (endIndex - 1 != startIndex) {
    if (!overlaps.InsertElementAt(1, mRanges[endIndex - 1]))
      return NS_ERROR_OUT_OF_MEMORY;
  }

  // Remove all the overlapping ranges
  for (int32_t i = startIndex; i < endIndex; ++i) {
    mRanges[i].mRange->SetSelection(nullptr);
  }
  mRanges.RemoveElementsAt(startIndex, endIndex - startIndex);

  nsTArray<RangeData> temp;
  for (int32_t i = overlaps.Length() - 1; i >= 0; i--) {
    nsresult rv = SubtractRange(&overlaps[i], aItem, &temp);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Insert the new element into our "leftovers" array
  int32_t insertionPoint;
  rv = FindInsertionPoint(&temp, aItem->GetStartContainer(),
                          aItem->StartOffset(), CompareToRangeStart,
                          &insertionPoint);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!temp.InsertElementAt(insertionPoint, RangeData(aItem)))
    return NS_ERROR_OUT_OF_MEMORY;

  // Merge the leftovers back in to mRanges
  if (!mRanges.InsertElementsAt(startIndex, temp))
    return NS_ERROR_OUT_OF_MEMORY;

  for (uint32_t i = 0; i < temp.Length(); ++i) {
    temp[i].mRange->SetSelection(this);
  }

  *aOutIndex = startIndex + insertionPoint;
  return NS_OK;
}

nsresult
Selection::RemoveItem(nsRange* aItem)
{
  if (!aItem)
    return NS_ERROR_NULL_POINTER;

  // Find the range's index & remove it. We could use FindInsertionPoint to
  // get O(log n) time, but that requires many expensive DOM comparisons.
  // For even several thousand items, this is probably faster because the
  // comparisons are so fast.
  int32_t idx = -1;
  uint32_t i;
  for (i = 0; i < mRanges.Length(); i ++) {
    if (mRanges[i].mRange == aItem) {
      idx = (int32_t)i;
      break;
    }
  }
  if (idx < 0)
    return NS_ERROR_DOM_NOT_FOUND_ERR;

  mRanges.RemoveElementAt(idx);
  aItem->SetSelection(nullptr);
  return NS_OK;
}

nsresult
Selection::RemoveCollapsedRanges()
{
  uint32_t i = 0;
  while (i < mRanges.Length()) {
    if (mRanges[i].mRange->Collapsed()) {
      nsresult rv = RemoveItem(mRanges[i].mRange);
      NS_ENSURE_SUCCESS(rv, rv);
    } else {
      ++i;
    }
  }
  return NS_OK;
}

nsresult
Selection::Clear(nsPresContext* aPresContext)
{
  SetAnchorFocusRange(-1);

  for (uint32_t i = 0; i < mRanges.Length(); ++i) {
    mRanges[i].mRange->SetSelection(nullptr);
    SelectFrames(aPresContext, mRanges[i].mRange, false);
  }
  mRanges.Clear();

  // Reset direction so for more dependable table selection range handling
  SetDirection(eDirNext);

  // If this was an ATTENTION selection, change it back to normal now
  if (mFrameSelection &&
      mFrameSelection->GetDisplaySelection() ==
      nsISelectionController::SELECTION_ATTENTION) {
    mFrameSelection->SetDisplaySelection(nsISelectionController::SELECTION_ON);
  }

  return NS_OK;
}

// RangeMatches*Point
//
//    Compares the range beginning or ending point, and returns true if it
//    exactly matches the given DOM point.

static inline bool
RangeMatchesBeginPoint(nsRange* aRange, nsINode* aNode, int32_t aOffset)
{
  return aRange->GetStartContainer() == aNode &&
         static_cast<int32_t>(aRange->StartOffset()) == aOffset;
}

static inline bool
RangeMatchesEndPoint(nsRange* aRange, nsINode* aNode, int32_t aOffset)
{
  return aRange->GetEndContainer() == aNode &&
         static_cast<int32_t>(aRange->EndOffset()) == aOffset;
}

// Selection::EqualsRangeAtPoint
//
//    Utility method for checking equivalence of two ranges.

bool
Selection::EqualsRangeAtPoint(
    nsINode* aBeginNode, int32_t aBeginOffset,
    nsINode* aEndNode, int32_t aEndOffset,
    int32_t aRangeIndex)
{
  if (aRangeIndex >=0 && aRangeIndex < (int32_t) mRanges.Length()) {
    nsRange* range = mRanges[aRangeIndex].mRange;
    if (RangeMatchesBeginPoint(range, aBeginNode, aBeginOffset) &&
        RangeMatchesEndPoint(range, aEndNode, aEndOffset))
      return true;
  }
  return false;
}

void
Selection::GetRangesForInterval(nsINode& aBeginNode, int32_t aBeginOffset,
                                nsINode& aEndNode, int32_t aEndOffset,
                                bool aAllowAdjacent,
                                nsTArray<RefPtr<nsRange>>& aReturn,
                                mozilla::ErrorResult& aRv)
{
  nsTArray<nsRange*> results;
  nsresult rv = GetRangesForIntervalArray(&aBeginNode, aBeginOffset,
                                          &aEndNode, aEndOffset,
                                          aAllowAdjacent, &results);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return;
  }

  aReturn.SetLength(results.Length());
  for (uint32_t i = 0; i < results.Length(); ++i) {
    aReturn[i] = results[i]; // AddRefs
  }
}

nsresult
Selection::GetRangesForIntervalArray(nsINode* aBeginNode, int32_t aBeginOffset,
                                     nsINode* aEndNode, int32_t aEndOffset,
                                     bool aAllowAdjacent,
                                     nsTArray<nsRange*>* aRanges)
{
  aRanges->Clear();
  int32_t startIndex, endIndex;
  nsresult res = GetIndicesForInterval(aBeginNode, aBeginOffset,
                                       aEndNode, aEndOffset, aAllowAdjacent,
                                       &startIndex, &endIndex);
  NS_ENSURE_SUCCESS(res, res);

  if (startIndex == -1 || endIndex == -1)
    return NS_OK;

  for (int32_t i = startIndex; i < endIndex; i++) {
    if (!aRanges->AppendElement(mRanges[i].mRange))
      return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

// Selection::GetIndicesForInterval
//
//    Works on the same principle as GetRangesForIntervalArray above, however
//    instead this returns the indices into mRanges between which the
//    overlapping ranges lie.

nsresult
Selection::GetIndicesForInterval(nsINode* aBeginNode, int32_t aBeginOffset,
                                 nsINode* aEndNode, int32_t aEndOffset,
                                 bool aAllowAdjacent,
                                 int32_t* aStartIndex, int32_t* aEndIndex)
{
  int32_t startIndex;
  int32_t endIndex;

  if (!aStartIndex)
    aStartIndex = &startIndex;
  if (!aEndIndex)
    aEndIndex = &endIndex;

  *aStartIndex = -1;
  *aEndIndex = -1;

  if (mRanges.Length() == 0)
    return NS_OK;

  bool intervalIsCollapsed = aBeginNode == aEndNode &&
    aBeginOffset == aEndOffset;

  // Ranges that end before the given interval and begin after the given
  // interval can be discarded
  int32_t endsBeforeIndex;
  if (NS_FAILED(FindInsertionPoint(&mRanges, aEndNode, aEndOffset,
                                   &CompareToRangeStart,
                                   &endsBeforeIndex))) {
    return NS_OK;
  }

  if (endsBeforeIndex == 0) {
    nsRange* endRange = mRanges[endsBeforeIndex].mRange;

    // If the interval is strictly before the range at index 0, we can optimize
    // by returning now - all ranges start after the given interval
    if (!RangeMatchesBeginPoint(endRange, aEndNode, aEndOffset))
      return NS_OK;

    // We now know that the start point of mRanges[0].mRange equals the end of
    // the interval. Thus, when aAllowadjacent is true, the caller is always
    // interested in this range. However, when excluding adjacencies, we must
    // remember to include the range when both it and the given interval are
    // collapsed to the same point
    if (!aAllowAdjacent && !(endRange->Collapsed() && intervalIsCollapsed))
      return NS_OK;
  }
  *aEndIndex = endsBeforeIndex;

  int32_t beginsAfterIndex;
  if (NS_FAILED(FindInsertionPoint(&mRanges, aBeginNode, aBeginOffset,
                                   &CompareToRangeEnd,
                                   &beginsAfterIndex))) {
    return NS_OK;
  }
  if (beginsAfterIndex == (int32_t) mRanges.Length())
    return NS_OK; // optimization: all ranges are strictly before us

  if (aAllowAdjacent) {
    // At this point, one of the following holds:
    //   endsBeforeIndex == mRanges.Length(),
    //   endsBeforeIndex points to a range whose start point does not equal the
    //     given interval's start point
    //   endsBeforeIndex points to a range whose start point equals the given
    //     interval's start point
    // In the final case, there can be two such ranges, a collapsed range, and
    // an adjacent range (they will appear in mRanges in that order). For this
    // final case, we need to increment endsBeforeIndex, until one of the
    // first two possibilites hold
    while (endsBeforeIndex < (int32_t) mRanges.Length()) {
      nsRange* endRange = mRanges[endsBeforeIndex].mRange;
      if (!RangeMatchesBeginPoint(endRange, aEndNode, aEndOffset))
        break;
      endsBeforeIndex++;
    }

    // Likewise, one of the following holds:
    //   beginsAfterIndex == 0,
    //   beginsAfterIndex points to a range whose end point does not equal
    //     the given interval's end point
    //   beginsOnOrAfter points to a range whose end point equals the given
    //     interval's end point
    // In the final case, there can be two such ranges, an adjacent range, and
    // a collapsed range (they will appear in mRanges in that order). For this
    // final case, we only need to take action if both those ranges exist, and
    // we are pointing to the collapsed range - we need to point to the
    // adjacent range
    nsRange* beginRange = mRanges[beginsAfterIndex].mRange;
    if (beginsAfterIndex > 0 && beginRange->Collapsed() &&
        RangeMatchesEndPoint(beginRange, aBeginNode, aBeginOffset)) {
      beginRange = mRanges[beginsAfterIndex - 1].mRange;
      if (RangeMatchesEndPoint(beginRange, aBeginNode, aBeginOffset))
        beginsAfterIndex--;
    }
  } else {
    // See above for the possibilities at this point. The only case where we
    // need to take action is when the range at beginsAfterIndex ends on
    // the given interval's start point, but that range isn't collapsed (a
    // collapsed range should be included in the returned results).
    nsRange* beginRange = mRanges[beginsAfterIndex].mRange;
    if (RangeMatchesEndPoint(beginRange, aBeginNode, aBeginOffset) &&
        !beginRange->Collapsed())
      beginsAfterIndex++;

    // Again, see above for the meaning of endsBeforeIndex at this point.
    // In particular, endsBeforeIndex may point to a collaped range which
    // represents the point at the end of the interval - this range should be
    // included
    if (endsBeforeIndex < (int32_t) mRanges.Length()) {
      nsRange* endRange = mRanges[endsBeforeIndex].mRange;
      if (RangeMatchesBeginPoint(endRange, aEndNode, aEndOffset) &&
          endRange->Collapsed())
        endsBeforeIndex++;
     }
  }

  NS_ASSERTION(beginsAfterIndex <= endsBeforeIndex,
               "Is mRanges not ordered?");
  NS_ENSURE_STATE(beginsAfterIndex <= endsBeforeIndex);

  *aStartIndex = beginsAfterIndex;
  *aEndIndex = endsBeforeIndex;
  return NS_OK;
}

nsresult
Selection::GetPrimaryFrameForAnchorNode(nsIFrame** aReturnFrame)
{
  if (!aReturnFrame)
    return NS_ERROR_NULL_POINTER;

  int32_t frameOffset = 0;
  *aReturnFrame = 0;
  nsCOMPtr<nsIContent> content = do_QueryInterface(GetAnchorNode());
  if (content && mFrameSelection)
  {
    *aReturnFrame = mFrameSelection->
      GetFrameForNodeOffset(content, AnchorOffset(),
                            mFrameSelection->GetHint(), &frameOffset);
    if (*aReturnFrame)
      return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

nsresult
Selection::GetPrimaryFrameForFocusNode(nsIFrame** aReturnFrame,
                                       int32_t* aOffsetUsed,
                                       bool aVisual)
{
  if (!aReturnFrame) {
    return NS_ERROR_NULL_POINTER;
  }

  *aReturnFrame = nullptr;
  nsINode* focusNode = GetFocusNode();
  if (!focusNode || !focusNode->IsContent() || !mFrameSelection) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIContent> content = focusNode->AsContent();
  int32_t frameOffset = 0;
  if (!aOffsetUsed) {
    aOffsetUsed = &frameOffset;
  }

  nsresult rv =
    GetPrimaryOrCaretFrameForNodeOffset(content, FocusOffset(), aReturnFrame,
                                        aOffsetUsed, aVisual);
  if (NS_SUCCEEDED(rv)) {
    return rv;
  }

  // If content is whitespace only, we promote focus node to parent because
  // whitespace only node might have no frame.

  if (!content->TextIsOnlyWhitespace()) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIContent> parent = content->GetParent();
  if (NS_WARN_IF(!parent)) {
    return NS_ERROR_FAILURE;
  }
  int32_t offset = parent->ComputeIndexOf(content);

  return GetPrimaryOrCaretFrameForNodeOffset(parent, offset, aReturnFrame,
                                             aOffsetUsed, aVisual);
}

nsresult
Selection::GetPrimaryOrCaretFrameForNodeOffset(nsIContent* aContent,
                                               uint32_t aOffset,
                                               nsIFrame** aReturnFrame,
                                               int32_t* aOffsetUsed,
                                               bool aVisual) const
{
  MOZ_ASSERT(aReturnFrame);
  MOZ_ASSERT(aOffsetUsed);

  *aReturnFrame = nullptr;

  if (!mFrameSelection) {
    return NS_ERROR_FAILURE;
  }

  CaretAssociationHint hint = mFrameSelection->GetHint();

  if (aVisual) {
    nsBidiLevel caretBidiLevel = mFrameSelection->GetCaretBidiLevel();

    return nsCaret::GetCaretFrameForNodeOffset(mFrameSelection,
                                               aContent, aOffset, hint,
                                               caretBidiLevel, aReturnFrame,
                                               aOffsetUsed);
  }

  *aReturnFrame =
    mFrameSelection->GetFrameForNodeOffset(aContent, aOffset,
                                           hint, aOffsetUsed);
  if (!*aReturnFrame) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

void
Selection::SelectFramesForContent(nsIContent* aContent,
                                  bool aSelected)
{
  nsIFrame* frame = aContent->GetPrimaryFrame();
  if (!frame) {
    return;
  }
  // The frame could be an SVG text frame, in which case we don't treat it
  // as a text frame.
  if (frame->IsTextFrame()) {
    nsTextFrame* textFrame = static_cast<nsTextFrame*>(frame);
    textFrame->SetSelectedRange(0, aContent->GetText()->GetLength(),
                                aSelected, mSelectionType);
  } else {
    frame->InvalidateFrameSubtree();  // frame continuations?
  }
}

//select all content children of aContent
nsresult
Selection::SelectAllFramesForContent(nsIContentIterator* aInnerIter,
                                     nsIContent* aContent,
                                     bool aSelected)
{
  // If aContent doesn't have children, we should avoid to use the content
  // iterator for performance reason.
  if (!aContent->HasChildren()) {
    SelectFramesForContent(aContent, aSelected);
    return NS_OK;
  }

  if (NS_WARN_IF(NS_FAILED(aInnerIter->Init(aContent)))) {
    return NS_ERROR_FAILURE;
  }

  for (; !aInnerIter->IsDone(); aInnerIter->Next()) {
    nsINode* node = aInnerIter->GetCurrentNode();
    MOZ_ASSERT(node);
    nsIContent* innercontent = node->IsContent() ? node->AsContent() : nullptr;
    SelectFramesForContent(innercontent, aSelected);
  }

  return NS_OK;
}

/**
 * The idea of this helper method is to select or deselect "top to bottom",
 * traversing through the frames
 */
nsresult
Selection::SelectFrames(nsPresContext* aPresContext, nsRange* aRange,
                        bool aSelect)
{
  if (!mFrameSelection || !aPresContext || !aPresContext->GetPresShell()) {
    // nothing to do
    return NS_OK;
  }
  MOZ_ASSERT(aRange && aRange->IsPositioned());

  if (mFrameSelection->GetTableCellSelection()) {
    nsINode* node = aRange->GetCommonAncestor();
    nsIFrame* frame = node->IsContent() ? node->AsContent()->GetPrimaryFrame()
                                : aPresContext->PresShell()->GetRootFrame();
    if (frame) {
      frame->InvalidateFrameSubtree();
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
        textFrame->SetSelectedRange(startOffset, endOffset, aSelect,
                                    mSelectionType);
      } else {
        frame->InvalidateFrameSubtree();
      }
    }
  }

  // If the range is in a node and the node is a leaf node, we don't need to
  // walk the subtree.
  if (aRange->Collapsed() ||
      (startNode == endNode && !startNode->HasChildren())) {
    if (!isFirstContentTextNode) {
      SelectFramesForContent(startContent, aSelect);
    }
    return NS_OK;
  }

  nsCOMPtr<nsIContentIterator> iter = NS_NewContentSubtreeIterator();
  iter->Init(aRange);
  if (isFirstContentTextNode && !iter->IsDone() &&
      iter->GetCurrentNode() == startNode) {
    iter->Next(); // first content has already been handled.
  }
  nsCOMPtr<nsIContentIterator> inneriter = NS_NewContentIterator();
  for (; !iter->IsDone(); iter->Next()) {
    nsINode* node = iter->GetCurrentNode();
    MOZ_ASSERT(node);
    nsIContent* content = node->IsContent() ? node->AsContent() : nullptr;
    SelectAllFramesForContent(inneriter, content, aSelect);
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
        textFrame->SetSelectedRange(0, aRange->EndOffset(), aSelect,
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

UniquePtr<SelectionDetails>
Selection::LookUpSelection(nsIContent* aContent, int32_t aContentOffset,
                           int32_t aContentLength,
                           UniquePtr<SelectionDetails> aDetailsHead,
                           SelectionType aSelectionType,
                           bool aSlowCheck)
{
  if (!aContent) {
    return aDetailsHead;
  }

  // it is common to have no ranges, to optimize that
  if (mRanges.Length() == 0) {
    return aDetailsHead;
  }

  nsTArray<nsRange*> overlappingRanges;
  nsresult rv = GetRangesForIntervalArray(aContent, aContentOffset,
                                          aContent, aContentOffset + aContentLength,
                                          false,
                                          &overlappingRanges);
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
      if (startOffset < (aContentOffset + aContentLength)  &&
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
    if (start < 0)
      continue; // the ranges do not overlap the input range

    auto newHead = MakeUnique<SelectionDetails>();

    newHead->mNext = std::move(detailsHead);
    newHead->mStart = start;
    newHead->mEnd = end;
    newHead->mSelectionType = aSelectionType;
    RangeData *rd = FindRangeData(range);
    if (rd) {
      newHead->mTextRangeStyle = rd->mTextRangeStyle;
    }
    detailsHead = std::move(newHead);
  }
  return detailsHead;
}

NS_IMETHODIMP
Selection::Repaint(nsPresContext* aPresContext)
{
  int32_t arrCount = (int32_t)mRanges.Length();

  if (arrCount < 1)
    return NS_OK;

  int32_t i;

  for (i = 0; i < arrCount; i++)
  {
    nsresult rv = SelectFrames(aPresContext, mRanges[i].mRange, true);

    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  return NS_OK;
}

void
Selection::SetCanCacheFrameOffset(bool aCanCacheFrameOffset)
{
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

nsresult
Selection::GetCachedFrameOffset(nsIFrame* aFrame, int32_t inOffset,
                                nsPoint& aPoint)
{
  if (!mCachedOffsetForFrame) {
    mCachedOffsetForFrame = new CachedOffsetForFrame;
  }

  nsresult rv = NS_OK;
  if (mCachedOffsetForFrame->mCanCacheFrameOffset &&
      mCachedOffsetForFrame->mLastCaretFrame &&
      (aFrame == mCachedOffsetForFrame->mLastCaretFrame) &&
      (inOffset == mCachedOffsetForFrame->mLastContentOffset))
  {
     // get cached frame offset
     aPoint = mCachedOffsetForFrame->mCachedFrameOffset;
  }
  else
  {
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

nsIContent*
Selection::GetAncestorLimiter() const
{
  if (mFrameSelection) {
    return mFrameSelection->GetAncestorLimiter();
  }
  return nullptr;
}

void
Selection::SetAncestorLimiter(nsIContent* aLimiter)
{
  if (mFrameSelection) {
    RefPtr<nsFrameSelection> frameSelection = mFrameSelection;
    frameSelection->SetAncestorLimiter(aLimiter);
  }
}

RangeData*
Selection::FindRangeData(nsRange* aRange)
{
  NS_ENSURE_TRUE(aRange, nullptr);
  for (uint32_t i = 0; i < mRanges.Length(); i++) {
    if (mRanges[i].mRange == aRange)
      return &mRanges[i];
  }
  return nullptr;
}

nsresult
Selection::SetTextRangeStyle(nsRange* aRange,
                             const TextRangeStyle& aTextRangeStyle)
{
  NS_ENSURE_ARG_POINTER(aRange);
  RangeData *rd = FindRangeData(aRange);
  if (rd) {
    rd->mTextRangeStyle = aTextRangeStyle;
  }
  return NS_OK;
}

nsresult
Selection::StartAutoScrollTimer(nsIFrame* aFrame, const nsPoint& aPoint,
                                uint32_t aDelay)
{
  MOZ_ASSERT(aFrame, "Need a frame");

  nsresult result;
  if (!mFrameSelection) {
    return NS_OK;//nothing to do
  }

  if (!mAutoScrollTimer) {
    mAutoScrollTimer = new nsAutoScrollTimer();

    result = mAutoScrollTimer->Init(mFrameSelection, this);

    if (NS_FAILED(result)) {
      return result;
    }
  }

  result = mAutoScrollTimer->SetDelay(aDelay);

  if (NS_FAILED(result)) {
    return result;
  }

  return DoAutoScroll(aFrame, aPoint);
}

nsresult
Selection::StopAutoScrollTimer()
{
  if (mAutoScrollTimer) {
    return mAutoScrollTimer->Stop();
  }
  return NS_OK;
}

nsresult
Selection::DoAutoScroll(nsIFrame* aFrame, nsPoint aPoint)
{
  MOZ_ASSERT(aFrame, "Need a frame");

  if (mAutoScrollTimer) {
    (void)mAutoScrollTimer->Stop();
  }

  nsPresContext* presContext = aFrame->PresContext();
  nsCOMPtr<nsIPresShell> shell = presContext->PresShell();
  nsRootPresContext* rootPC = presContext->GetRootPresContext();
  if (!rootPC)
    return NS_OK;
  nsIFrame* rootmostFrame = rootPC->PresShell()->GetRootFrame();
  AutoWeakFrame weakRootFrame(rootmostFrame);
  AutoWeakFrame weakFrame(aFrame);
  // Get the point relative to the root most frame because the scroll we are
  // about to do will change the coordinates of aFrame.
  nsPoint globalPoint = aPoint + aFrame->GetOffsetToCrossDoc(rootmostFrame);

  bool done = false;
  bool didScroll;
  while (true) {
    didScroll = shell->ScrollFrameRectIntoView(
                  aFrame, nsRect(aPoint, nsSize(0, 0)),
                  nsIPresShell::ScrollAxis(), nsIPresShell::ScrollAxis(),
                  0);
    if (!weakFrame || !weakRootFrame) {
      return NS_OK;
    }
    if (!didScroll && !done) {
      // If aPoint is at the screen edge then try to scroll anyway, once.
      RefPtr<nsDeviceContext> dx = shell->GetViewManager()->GetDeviceContext();
      nsRect screen;
      dx->GetRect(screen);
      nsPoint screenPoint = globalPoint +
                            rootmostFrame->GetScreenRectInAppUnits().TopLeft();
      nscoord onePx = nsPresContext::AppUnitsPerCSSPixel();
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
  if (didScroll && mAutoScrollTimer) {
    nsPoint presContextPoint = globalPoint -
      shell->GetRootFrame()->GetOffsetToCrossDoc(rootmostFrame);
    mAutoScrollTimer->Start(presContext, presContextPoint);
  }

  return NS_OK;
}


void
Selection::RemoveAllRanges(ErrorResult& aRv)
{
  if (!mFrameSelection)
    return; // nothing to do
  RefPtr<nsPresContext>  presContext = GetPresContext();
  nsresult  result = Clear(presContext);
  if (NS_FAILED(result)) {
    aRv.Throw(result);
    return;
  }

  // Turn off signal for table selection
  RefPtr<nsFrameSelection> frameSelection = mFrameSelection;
  frameSelection->ClearTableCellSelection();

  // Be aware, this instance may be destroyed after this call.
  // XXX Why doesn't this call Selection::NotifySelectionListener() directly?
  result = frameSelection->NotifySelectionListeners(GetType());

  // Also need to notify the frames!
  // PresShell::CharacterDataChanged should do that on DocumentChanged
  if (NS_FAILED(result)) {
    aRv.Throw(result);
  }
}

nsresult
Selection::RemoveAllRangesTemporarily()
{
  if (!mCachedRange) {
    // Look for a range which isn't referred by other than this instance.
    // If there is, it'll be released by calling Clear().  So, we can reuse it
    // when we need to create a range.
    for (auto& rangeData : mRanges) {
      auto& range = rangeData.mRange;
      if (range->GetRefCount() == 1 ||
          (range->GetRefCount() == 2 && range == mAnchorFocusRange)) {
        mCachedRange = range;
        break;
      }
    }
  }

  // Then, remove all ranges.
  ErrorResult result;
  RemoveAllRanges(result);
  if (result.Failed()) {
    mCachedRange = nullptr;
  }
  return result.StealNSResult();
}

void
Selection::AddRangeJS(nsRange& aRange, ErrorResult& aRv)
{
  AutoRestore<bool> calledFromJSRestorer(mCalledByJS);
  mCalledByJS = true;
  AddRange(aRange, aRv);
}

void
Selection::AddRange(nsRange& aRange, ErrorResult& aRv)
{
  return AddRangeInternal(aRange, GetParentObject(), aRv);
}

void
Selection::AddRangeInternal(nsRange& aRange, nsIDocument* aDocument,
                            ErrorResult& aRv)
{
  nsINode* rangeRoot = aRange.GetRoot();
  if (aDocument != rangeRoot && (!rangeRoot ||
                                 aDocument != rangeRoot->GetComposedDoc())) {
    // http://w3c.github.io/selection-api/#dom-selection-addrange
    // "...  if the root of the range's boundary points are the document
    // associated with context object. Otherwise, this method must do nothing."
    return;
  }

  // If a range is being added, we don't need cached range because Collapse()
  // won't use it.
  mCachedRange = nullptr;

  // AddTableCellRange might flush frame.
  RefPtr<Selection> kungFuDeathGrip(this);

  // This inserts a table cell range in proper document order
  // and returns NS_OK if range doesn't contain just one table cell
  bool didAddRange;
  int32_t rangeIndex;
  nsresult result = AddTableCellRange(&aRange, &didAddRange, &rangeIndex);
  if (NS_FAILED(result)) {
    aRv.Throw(result);
    return;
  }

  if (!didAddRange) {
    result = AddItem(&aRange, &rangeIndex);
    if (NS_FAILED(result)) {
      aRv.Throw(result);
      return;
    }
  }

  if (rangeIndex < 0) {
    return;
  }

  SetAnchorFocusRange(rangeIndex);

  // Make sure the caret appears on the next line, if at a newline
  if (mSelectionType == SelectionType::eNormal) {
    SetInterlinePosition(true, IgnoreErrors());
  }

  RefPtr<nsPresContext>  presContext = GetPresContext();
  SelectFrames(presContext, &aRange, true);

  if (!mFrameSelection)
    return;//nothing to do

  // Be aware, this instance may be destroyed after this call.
  // XXX Why doesn't this call Selection::NotifySelectionListener() directly?
  RefPtr<nsFrameSelection> frameSelection = mFrameSelection;
  result = frameSelection->NotifySelectionListeners(GetType());
  if (NS_FAILED(result)) {
    aRv.Throw(result);
  }
}

// Selection::RemoveRange
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

void
Selection::RemoveRange(nsRange& aRange, ErrorResult& aRv)
{
  nsresult rv = RemoveItem(&aRange);
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
  RefPtr<nsPresContext>  presContext = GetPresContext();
  SelectFrames(presContext, &aRange, false);

  // add back the selected bit for each range touching our nodes
  nsTArray<nsRange*> affectedRanges;
  rv = GetRangesForIntervalArray(beginNode, beginOffset,
                                 endNode, endOffset,
                                 true, &affectedRanges);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return;
  }
  for (uint32_t i = 0; i < affectedRanges.Length(); i++) {
    SelectFrames(presContext, affectedRanges[i], true);
  }

  int32_t cnt = mRanges.Length();
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

  if (!mFrameSelection)
    return;//nothing to do

  // Be aware, this instance may be destroyed after this call.
  // XXX Why doesn't this call Selection::NotifySelectionListener() directly?
  RefPtr<nsFrameSelection> frameSelection = mFrameSelection;
  rv = frameSelection->NotifySelectionListeners(GetType());
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
  }
}



/*
 * Collapse sets the whole selection to be one point.
 */
void
Selection::CollapseJS(nsINode* aContainer, uint32_t aOffset, ErrorResult& aRv)
{
  AutoRestore<bool> calledFromJSRestorer(mCalledByJS);
  mCalledByJS = true;
  if (!aContainer) {
    RemoveAllRanges(aRv);
    return;
  }
  Collapse(RawRangeBoundary(aContainer, aOffset), aRv);
}

void
Selection::Collapse(const RawRangeBoundary& aPoint, ErrorResult& aRv)
{
  if (!mFrameSelection) {
    aRv.Throw(NS_ERROR_NOT_INITIALIZED); // Can't do selection
    return;
  }

  if (!aPoint.IsSet()) {
    aRv.Throw(NS_ERROR_INVALID_ARG);
    return;
  }

  if (aPoint.Container()->NodeType() == nsINode::DOCUMENT_TYPE_NODE) {
    aRv.Throw(NS_ERROR_DOM_INVALID_NODE_TYPE_ERR);
    return;
  }

  // RawRangeBoundary::IsSetAndValid() checks if the point actually refers
  // a child of the container when IsSet() is true.  If its offset hasn't been
  // computed yet, this just checks it with its mRef.  So, we can avoid
  // computing offset here.
  if (!aPoint.IsSetAndValid()) {
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return;
  }

  if (!HasSameRoot(*aPoint.Container())) {
    // Return with no error
    return;
  }

  RefPtr<nsFrameSelection> frameSelection = mFrameSelection;
  frameSelection->InvalidateDesiredPos();
  if (!IsValidSelectionPoint(frameSelection, aPoint.Container())) {
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

  // Cache current range is if there is because it may be reusable.
  RefPtr<nsRange> oldRange = !mRanges.IsEmpty() ? mRanges[0].mRange : nullptr;

  // Delete all of the current ranges
  Clear(presContext);

  // Turn off signal for table selection
  frameSelection->ClearTableCellSelection();

  // Hack to display the caret on the right line (bug 1237236).
  if (frameSelection->GetHint() != CARET_ASSOCIATE_AFTER &&
      aPoint.Container()->IsContent()) {
    int32_t frameOffset;
    nsTextFrame* f =
      do_QueryFrame(nsCaret::GetFrameAndOffset(this, aPoint.Container(),
                                               aPoint.Offset(), &frameOffset));
    if (f && f->IsAtEndOfLine() && f->HasSignificantTerminalNewline()) {
      // RawRangeBounary::Offset() causes computing offset if it's not been
      // done yet.  However, it's called only when the container is a text
      // node.  In such case, offset has always been set since it cannot have
      // any children.  So, this doesn't cause computing offset with expensive
      // method, nsINode::ComputeIndexOf().
      if ((aPoint.Container()->AsContent() == f->GetContent() &&
           f->GetContentEnd() == static_cast<int32_t>(aPoint.Offset())) ||
          (aPoint.Container() == f->GetContent()->GetParentNode() &&
           f->GetContent() == aPoint.GetPreviousSiblingOfChildAtOffset())) {
        frameSelection->SetHint(CARET_ASSOCIATE_AFTER);
      }
    }
  }

  RefPtr<nsRange> range;
  // If the old range isn't referred by anybody other than this method,
  // we should reuse it for reducing the recreation cost.
  if (oldRange && oldRange->GetRefCount() == 1) {
    range = std::move(oldRange);
  } else if (mCachedRange) {
    range = std::move(mCachedRange);
  } else {
    range = new nsRange(aPoint.Container());
  }
  result = range->CollapseTo(aPoint);
  if (NS_FAILED(result)) {
    aRv.Throw(result);
    return;
  }

#ifdef DEBUG_SELECTION
  nsCOMPtr<nsIContent> content = do_QueryInterface(aPoint.Container());
  nsCOMPtr<nsIDocument> doc = do_QueryInterface(aPoint.Container());
  printf ("Sel. Collapse to %p %s %d\n", container.get(),
          content ? nsAtomCString(content->NodeInfo()->NameAtom()).get()
                  : (doc ? "DOCUMENT" : "???"),
          aPoint.Offset());
#endif

  int32_t rangeIndex = -1;
  result = AddItem(range, &rangeIndex);
  if (NS_FAILED(result)) {
    aRv.Throw(result);
    return;
  }
  SetAnchorFocusRange(0);
  SelectFrames(presContext, range, true);

  // Be aware, this instance may be destroyed after this call.
  // XXX Why doesn't this call Selection::NotifySelectionListener() directly?
  result = frameSelection->NotifySelectionListeners(GetType());
  if (NS_FAILED(result)) {
    aRv.Throw(result);
  }
}

/*
 * Sets the whole selection to be one point
 * at the start of the current selection
 */
void
Selection::CollapseToStartJS(ErrorResult& aRv)
{
  AutoRestore<bool> calledFromJSRestorer(mCalledByJS);
  mCalledByJS = true;
  CollapseToStart(aRv);
}

void
Selection::CollapseToStart(ErrorResult& aRv)
{
  if (RangeCount() == 0) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  // Get the first range
  nsRange* firstRange = mRanges[0].mRange;
  if (!firstRange) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  if (mFrameSelection) {
    int16_t reason = mFrameSelection->PopReason() | nsISelectionListener::COLLAPSETOSTART_REASON;
    mFrameSelection->PostReason(reason);
  }
  nsINode* container = firstRange->GetStartContainer();
  if (!container) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }
  Collapse(*container, firstRange->StartOffset(), aRv);
}

/*
 * Sets the whole selection to be one point
 * at the end of the current selection
 */
void
Selection::CollapseToEndJS(ErrorResult& aRv)
{
  AutoRestore<bool> calledFromJSRestorer(mCalledByJS);
  mCalledByJS = true;
  CollapseToEnd(aRv);
}

void
Selection::CollapseToEnd(ErrorResult& aRv)
{
  uint32_t cnt = RangeCount();
  if (cnt == 0) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  // Get the last range
  nsRange* lastRange = mRanges[cnt - 1].mRange;
  if (!lastRange) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  if (mFrameSelection) {
    int16_t reason = mFrameSelection->PopReason() | nsISelectionListener::COLLAPSETOEND_REASON;
    mFrameSelection->PostReason(reason);
  }
  nsINode* container = lastRange->GetEndContainer();
  if (!container) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }
  Collapse(*container, lastRange->EndOffset(), aRv);
}

void
Selection::GetType(nsAString& aOutType) const
{
  if (!RangeCount()) {
    aOutType.AssignLiteral("None");
  } else if (IsCollapsed()) {
    aOutType.AssignLiteral("Caret");
  } else {
    aOutType.AssignLiteral("Range");
  }
}

nsRange*
Selection::GetRangeAt(uint32_t aIndex, ErrorResult& aRv)
{
  nsRange* range = GetRangeAt(aIndex);
  if (!range) {
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return nullptr;
  }

  return range;
}

nsRange*
Selection::GetRangeAt(int32_t aIndex) const
{
  RangeData empty(nullptr);
  return mRanges.SafeElementAt(aIndex, empty).mRange;
}

/*
utility function
*/
nsresult
Selection::SetAnchorFocusToRange(nsRange* aRange)
{
  NS_ENSURE_STATE(mAnchorFocusRange);

  bool collapsed = IsCollapsed();

  nsresult res = RemoveItem(mAnchorFocusRange);
  if (NS_FAILED(res))
    return res;

  int32_t aOutIndex = -1;
  res = AddItem(aRange, &aOutIndex, !collapsed);
  if (NS_FAILED(res))
    return res;
  SetAnchorFocusRange(aOutIndex);

  return NS_OK;
}

void
Selection::ReplaceAnchorFocusRange(nsRange* aRange)
{
  NS_ENSURE_TRUE_VOID(mAnchorFocusRange);
  RefPtr<nsPresContext> presContext = GetPresContext();
  if (presContext) {
    SelectFrames(presContext, mAnchorFocusRange, false);
    SetAnchorFocusToRange(aRange);
    SelectFrames(presContext, mAnchorFocusRange, true);
  }
}

void
Selection::AdjustAnchorFocusForMultiRange(nsDirection aDirection)
{
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
  } else { // aDir == eDirNext
    firstRange->SetIsGenerated(true);
    lastRange->SetIsGenerated(false);
    SetAnchorFocusRange(RangeCount() - 1);
  }
}

/*
Notes which might come in handy for extend:

We can tell the direction of the selection by asking for the anchors selection
if the begin is less than the end then we know the selection is to the "right".
else it is a backwards selection.
a = anchor
1 = old cursor
2 = new cursor

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


/*
 * Extend extends the selection away from the anchor.
 * We don't need to know the direction, because we always change the focus.
 */
void
Selection::ExtendJS(nsINode& aContainer, uint32_t aOffset, ErrorResult& aRv)
{
  AutoRestore<bool> calledFromJSRestorer(mCalledByJS);
  mCalledByJS = true;
  Extend(aContainer, aOffset, aRv);
}

nsresult
Selection::Extend(nsINode* aContainer, int32_t aOffset)
{
  if (!aContainer) {
    return NS_ERROR_INVALID_ARG;
  }

  ErrorResult result;
  Extend(*aContainer, static_cast<uint32_t>(aOffset), result);
  return result.StealNSResult();
}

void
Selection::Extend(nsINode& aContainer, uint32_t aOffset, ErrorResult& aRv)
{
  // First, find the range containing the old focus point:
  if (!mAnchorFocusRange) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  if (!mFrameSelection) {
    aRv.Throw(NS_ERROR_NOT_INITIALIZED); // Can't do selection
    return;
  }

  if (!HasSameRoot(aContainer)) {
    // Return with no error
    return;
  }

  nsresult res;
  if (!IsValidSelectionPoint(mFrameSelection, &aContainer)) {
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

  //compare anchor to old cursor.

  // We pass |disconnected| to the following ComparePoints calls in order
  // to avoid assertions. ComparePoints returns 1 in the disconnected case
  // and we can end up in various cases below, but it is assumed that in
  // any of the cases we end up, the nsRange implementation will collapse
  // the range to the new point because we can not make a valid range with
  // a disconnected point. This means that whatever range is currently
  // selected will be cleared.
  bool disconnected = false;
  bool shouldClearRange = false;
  int32_t result1 = nsContentUtils::ComparePoints(anchorNode, anchorOffset,
                                                  focusNode, focusOffset,
                                                  &disconnected);
  //compare old cursor to new cursor
  shouldClearRange |= disconnected;
  int32_t result2 = nsContentUtils::ComparePoints(focusNode, focusOffset,
                                                  &aContainer, aOffset,
                                                  &disconnected);
  //compare anchor to new cursor
  shouldClearRange |= disconnected;
  int32_t result3 = nsContentUtils::ComparePoints(anchorNode, anchorOffset,
                                                  &aContainer, aOffset,
                                                  &disconnected);

  // If the points are disconnected, the range will be collapsed below,
  // resulting in a range that selects nothing.
  if (shouldClearRange) {
    // Repaint the current range with the selection removed.
    SelectFrames(presContext, range, false);
  }

  RefPtr<nsRange> difRange = new nsRange(&aContainer);
  if ((result1 == 0 && result3 < 0) || (result1 <= 0 && result2 < 0)){//a1,2  a,1,2
    //select from 1 to 2 unless they are collapsed
    range->SetEnd(aContainer, aOffset, aRv);
    if (aRv.Failed()) {
      return;
    }
    SetDirection(eDirNext);
    res = difRange->SetStartAndEnd(focusNode, focusOffset,
                                   range->GetEndContainer(),
                                   range->EndOffset());
    if (NS_FAILED(res)) {
      aRv.Throw(res);
      return;
    }
    SelectFrames(presContext, difRange , true);
    res = SetAnchorFocusToRange(range);
    if (NS_FAILED(res)) {
      aRv.Throw(res);
      return;
    }
  }
  else if (result1 == 0 && result3 > 0){//2, a1
    //select from 2 to 1a
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
  }
  else if (result3 <= 0 && result2 >= 0) {//a,2,1 or a2,1 or a,21 or a21
    //deselect from 2 to 1
    res = difRange->SetStartAndEnd(&aContainer, aOffset,
                                   focusNode, focusOffset);
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
    SelectFrames(presContext, difRange, false); // deselect now
    difRange->SetEnd(range->GetEndContainer(), range->EndOffset());
    SelectFrames(presContext, difRange, true); // must reselect last node
                                               // maybe more
  }
  else if (result1 >= 0 && result3 <= 0) {//1,a,2 or 1a,2 or 1,a2 or 1a2
    if (GetDirection() == eDirPrevious){
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
    if (focusNode != anchorNode || focusOffset != anchorOffset) {//if collapsed diff dont do anything
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
      //deselect from 1 to a
      SelectFrames(presContext, difRange , false);
    }
    else
    {
      res = SetAnchorFocusToRange(range);
      if (NS_FAILED(res)) {
        aRv.Throw(res);
        return;
      }
    }
    //select from a to 2
    SelectFrames(presContext, range , true);
  }
  else if (result2 <= 0 && result3 >= 0) {//1,2,a or 12,a or 1,2a or 12a
    //deselect from 1 to 2
    res = difRange->SetStartAndEnd(focusNode, focusOffset,
                                   &aContainer, aOffset);
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
    SelectFrames(presContext, difRange , false);
    difRange->SetStart(range->GetStartContainer(), range->StartOffset());
    SelectFrames(presContext, difRange, true); // must reselect last node
  }
  else if (result3 >= 0 && result1 <= 0) {//2,a,1 or 2a,1 or 2,a1 or 2a1
    if (GetDirection() == eDirNext){
      range->SetEnd(startNode, startOffset);
    }
    SetDirection(eDirPrevious);
    range->SetStart(aContainer, aOffset, aRv);
    if (aRv.Failed()) {
      return;
    }
    //deselect from a to 1
    if (focusNode != anchorNode || focusOffset!= anchorOffset) {//if collapsed diff dont do anything
      res = difRange->SetStartAndEnd(anchorNode, anchorOffset,
                                     focusNode, focusOffset);
      nsresult tmp = SetAnchorFocusToRange(range);
      if (NS_FAILED(tmp)) {
        res = tmp;
      }
      if (NS_FAILED(res)) {
        aRv.Throw(res);
        return;
      }
      SelectFrames(presContext, difRange, false);
    }
    else
    {
      res = SetAnchorFocusToRange(range);
      if (NS_FAILED(res)) {
        aRv.Throw(res);
        return;
      }
    }
    //select from 2 to a
    SelectFrames(presContext, range , true);
  }
  else if (result2 >= 0 && result1 >= 0) {//2,1,a or 21,a or 2,1a or 21a
    //select from 2 to 1
    range->SetStart(aContainer, aOffset, aRv);
    if (aRv.Failed()) {
      return;
    }
    SetDirection(eDirPrevious);
    res = difRange->SetStartAndEnd(
                      range->GetStartContainer(), range->StartOffset(),
                      focusNode, focusOffset);
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

  if (mRanges.Length() > 1) {
    for (size_t i = 0; i < mRanges.Length(); ++i) {
      nsRange* range = mRanges[i].mRange;
      MOZ_ASSERT(range->IsInSelection());
      SelectFrames(presContext, range, range->IsInSelection());
    }
  }

  DEBUG_OUT_RANGE(range);
#ifdef DEBUG_SELECTION
  if (GetDirection() != oldDirection) {
    printf("    direction changed to %s\n",
           GetDirection() == eDirNext? "eDirNext":"eDirPrevious");
  }
  nsCOMPtr<nsIContent> content = do_QueryInterface(&aContainer);
  printf ("Sel. Extend to %p %s %d\n", content.get(),
          nsAtomCString(content->NodeInfo()->NameAtom()).get(), aOffset);
#endif

  // Be aware, this instance may be destroyed after this call.
  // XXX Why doesn't this call Selection::NotifySelectionListener() directly?
  RefPtr<nsFrameSelection> frameSelection = mFrameSelection;
  res = frameSelection->NotifySelectionListeners(GetType());
  if (NS_FAILED(res)) {
    aRv.Throw(res);
  }
}

void
Selection::SelectAllChildrenJS(nsINode& aNode, ErrorResult& aRv)
{
  AutoRestore<bool> calledFromJSRestorer(mCalledByJS);
  mCalledByJS = true;
  SelectAllChildren(aNode, aRv);
}

void
Selection::SelectAllChildren(nsINode& aNode, ErrorResult& aRv)
{
  if (aNode.NodeType() == nsINode::DOCUMENT_TYPE_NODE) {
    aRv.Throw(NS_ERROR_DOM_INVALID_NODE_TYPE_ERR);
    return;
  }

  if (!HasSameRoot(aNode)) {
    // Return with no error
    return;
  }

  if (mFrameSelection) {
    mFrameSelection->PostReason(nsISelectionListener::SELECTALL_REASON);
  }
  SelectionBatcher batch(this);

  Collapse(aNode, 0, aRv);
  if (aRv.Failed()) {
    return;
  }

  Extend(aNode, aNode.GetChildCount(), aRv);
}

bool
Selection::ContainsNode(nsINode& aNode, bool aAllowPartial, ErrorResult& aRv)
{
  nsresult rv;
  if (mRanges.Length() == 0) {
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
  rv = GetRangesForIntervalArray(&aNode, 0, &aNode, nodeLength,
                                 false, &overlappingRanges);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return false;
  }
  if (overlappingRanges.Length() == 0)
    return false; // no ranges overlap

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
    if (NS_SUCCEEDED(nsRange::CompareNodeToRange(&aNode, overlappingRanges[i],
                                                 &nodeStartsBeforeRange,
                                                 &nodeEndsAfterRange))) {
      if (!nodeStartsBeforeRange && !nodeEndsAfterRange) {
        return true;
      }
    }
  }
  return false;
}

class PointInRectChecker : public nsLayoutUtils::RectCallback {
public:
  explicit PointInRectChecker(const nsPoint& aPoint)
    : mPoint(aPoint)
    , mMatchFound(false)
  {
  }

  void AddRect(const nsRect& aRect) override
  {
    mMatchFound = mMatchFound || aRect.Contains(mPoint);
  }

  bool MatchFound()
  {
    return mMatchFound;
  }

private:
  nsPoint mPoint;
  bool mMatchFound;
};

bool
Selection::ContainsPoint(const nsPoint& aPoint)
{
  if (IsCollapsed()) {
    return false;
  }
  PointInRectChecker checker(aPoint);
  for (uint32_t i = 0; i < RangeCount(); i++) {
    nsRange* range = GetRangeAt(i);
    nsRange::CollectClientRectsAndText(&checker, nullptr, range,
                                       range->GetStartContainer(),
                                       range->StartOffset(),
                                       range->GetEndContainer(),
                                       range->EndOffset(),
                                       true, false);
    if (checker.MatchFound()) {
      return true;
    }
  }
  return false;
}

nsPresContext*
Selection::GetPresContext() const
{
  nsIPresShell *shell = GetPresShell();
  if (!shell) {
    return nullptr;
  }

  return shell->GetPresContext();
}

nsIPresShell*
Selection::GetPresShell() const
{
  if (!mFrameSelection)
    return nullptr;//nothing to do

  return mFrameSelection->GetShell();
}

nsIDocument*
Selection::GetDocument() const
{
  nsIPresShell* presShell = GetPresShell();
  return presShell ? presShell->GetDocument() : nullptr;
}

nsPIDOMWindowOuter*
Selection::GetWindow() const
{
  nsIDocument* document = GetDocument();
  return document ? document->GetWindow() : nullptr;
}

HTMLEditor*
Selection::GetHTMLEditor() const
{
  nsPresContext* presContext = GetPresContext();
  if (!presContext) {
    return nullptr;
  }
  return nsContentUtils::GetHTMLEditor(presContext);
}

nsIFrame *
Selection::GetSelectionAnchorGeometry(SelectionRegion aRegion, nsRect* aRect)
{
  if (!mFrameSelection)
    return nullptr;  // nothing to do

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
  if (!anchorFrame)
    return nullptr;

  nsRect focusRect;
  nsIFrame* focusFrame = GetSelectionEndPointGeometry(
    nsISelectionController::SELECTION_FOCUS_REGION, &focusRect);
  if (!focusFrame)
    return nullptr;

  NS_ASSERTION(anchorFrame->PresContext() == focusFrame->PresContext(),
    "points of selection in different documents?");
  // make focusRect relative to anchorFrame
  focusRect += focusFrame->GetOffsetTo(anchorFrame);

  aRect->UnionRectEdges(anchorRect, focusRect);
  return anchorFrame;
}

nsIFrame *
Selection::GetSelectionEndPointGeometry(SelectionRegion aRegion, nsRect* aRect)
{
  if (!mFrameSelection)
    return nullptr;  // nothing to do

  NS_ENSURE_TRUE(aRect, nullptr);

  aRect->SetRect(0, 0, 0, 0);

  nsINode    *node       = nullptr;
  uint32_t    nodeOffset = 0;
  nsIFrame   *frame      = nullptr;

  switch (aRegion) {
    case nsISelectionController::SELECTION_ANCHOR_REGION:
      node       = GetAnchorNode();
      nodeOffset = AnchorOffset();
      break;
    case nsISelectionController::SELECTION_FOCUS_REGION:
      node       = GetFocusNode();
      nodeOffset = FocusOffset();
      break;
    default:
      return nullptr;
  }

  if (!node)
    return nullptr;

  nsCOMPtr<nsIContent> content = do_QueryInterface(node);
  NS_ENSURE_TRUE(content.get(), nullptr);
  int32_t frameOffset = 0;
  frame = mFrameSelection->GetFrameForNodeOffset(content, nodeOffset,
                                                 mFrameSelection->GetHint(),
                                                 &frameOffset);
  if (!frame)
    return nullptr;

  // Figure out what node type we have, then get the
  // appropriate rect for it's nodeOffset.
  bool isText = node->IsText();

  nsPoint pt(0, 0);
  if (isText) {
    nsIFrame* childFrame = nullptr;
    frameOffset = 0;
    nsresult rv =
      frame->GetChildFrameContainingOffset(nodeOffset,
                                           mFrameSelection->GetHint(),
                                           &frameOffset, &childFrame);
    if (NS_FAILED(rv))
      return nullptr;
    if (!childFrame)
      return nullptr;

    frame = childFrame;

    // Get the x coordinate of the offset into the text frame.
    rv = GetCachedFrameOffset(frame, nodeOffset, pt);
    if (NS_FAILED(rv))
      return nullptr;
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
Selection::ScrollSelectionIntoViewEvent::Run()
{
  if (!mSelection)
    return NS_OK;  // event revoked

  int32_t flags = Selection::SCROLL_DO_FLUSH |
                  Selection::SCROLL_SYNCHRONOUS;

  Selection* sel = mSelection; // workaround to satisfy static analysis
  RefPtr<Selection> kungFuDeathGrip(sel);
  mSelection->mScrollEvent.Forget();
  mSelection->ScrollIntoView(mRegion, mVerticalScroll,
                             mHorizontalScroll, mFlags | flags);
  return NS_OK;
}

nsresult
Selection::PostScrollSelectionIntoViewEvent(
                                         SelectionRegion aRegion,
                                         int32_t aFlags,
                                         nsIPresShell::ScrollAxis aVertical,
                                         nsIPresShell::ScrollAxis aHorizontal)
{
  // If we've already posted an event, revoke it and place a new one at the
  // end of the queue to make sure that any new pending reflow events are
  // processed before we scroll. This will insure that we scroll to the
  // correct place on screen.
  mScrollEvent.Revoke();
  nsPresContext* presContext = GetPresContext();
  NS_ENSURE_STATE(presContext);
  nsRefreshDriver* refreshDriver = presContext->RefreshDriver();
  NS_ENSURE_STATE(refreshDriver);

  mScrollEvent =
    new ScrollSelectionIntoViewEvent(this, aRegion, aVertical, aHorizontal,
                                     aFlags);
  refreshDriver->AddEarlyRunner(mScrollEvent.get());
  return NS_OK;
}

void
Selection::ScrollIntoView(int16_t aRegion, bool aIsSynchronous,
                          int16_t aVPercent, int16_t aHPercent,
                          ErrorResult& aRv)
{
  int32_t flags = aIsSynchronous ? Selection::SCROLL_SYNCHRONOUS : 0;
  nsresult rv = ScrollIntoView(aRegion,
                               nsIPresShell::ScrollAxis(aVPercent),
                               nsIPresShell::ScrollAxis(aHPercent),
                               flags);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
  }
}

nsresult
Selection::ScrollIntoView(SelectionRegion aRegion,
                          nsIPresShell::ScrollAxis aVertical,
                          nsIPresShell::ScrollAxis aHorizontal,
                          int32_t aFlags)
{
  if (!mFrameSelection)
    return NS_OK;//nothing to do

  nsIPresShell* presShell = mFrameSelection->GetShell();
  if (!presShell)
    return NS_OK;

  if (mFrameSelection->GetBatching())
    return NS_OK;

  if (!(aFlags & Selection::SCROLL_SYNCHRONOUS))
    return PostScrollSelectionIntoViewEvent(aRegion, aFlags,
      aVertical, aHorizontal);

  // From this point on, the presShell may get destroyed by the calls below, so
  // hold on to it using a strong reference to ensure the safety of the
  // accesses to frame pointers in the callees.
  nsCOMPtr<nsIPresShell> kungFuDeathGrip(presShell);

  // Now that text frame character offsets are always valid (though not
  // necessarily correct), the worst that will happen if we don't flush here
  // is that some callers might scroll to the wrong place.  Those should
  // either manually flush if they're in a safe position for it or use the
  // async version of this method.
  if (aFlags & Selection::SCROLL_DO_FLUSH) {
    presShell->FlushPendingNotifications(FlushType::Layout);

    // Reget the presshell, since it might have been Destroy'ed.
    presShell = mFrameSelection ? mFrameSelection->GetShell() : nullptr;
    if (!presShell)
      return NS_OK;
  }

  //
  // Scroll the selection region into view.
  //

  nsRect rect;
  nsIFrame* frame = GetSelectionAnchorGeometry(aRegion, &rect);
  if (!frame)
    return NS_ERROR_FAILURE;

  // Scroll vertically to get the caret into view, but only if the container
  // is perceived to be scrollable in that direction (i.e. there is a visible
  // vertical scrollbar or the scroll range is at least one device pixel)
  aVertical.mOnlyIfPerceivedScrollableDirection = true;

  uint32_t flags = 0;
  if (aFlags & Selection::SCROLL_FIRST_ANCESTOR_ONLY) {
    flags |= nsIPresShell::SCROLL_FIRST_ANCESTOR_ONLY;
  }
  if (aFlags & Selection::SCROLL_OVERFLOW_HIDDEN) {
    flags |= nsIPresShell::SCROLL_OVERFLOW_HIDDEN;
  }

  presShell->ScrollFrameRectIntoView(frame, rect, aVertical, aHorizontal,
    flags);
  return NS_OK;
}

void
Selection::AddSelectionListener(nsISelectionListener* aNewListener)
{
  MOZ_ASSERT(aNewListener);
  mSelectionListeners.AppendElement(aNewListener); // AddRefs
}

void
Selection::RemoveSelectionListener(nsISelectionListener* aListenerToRemove)
{
  mSelectionListeners.RemoveElement(aListenerToRemove); // Releases
}

Element*
Selection::GetCommonEditingHostForAllRanges()
{
  Element* editingHost = nullptr;
  for (RangeData& rangeData : mRanges) {
    nsRange* range = rangeData.mRange;
    MOZ_ASSERT(range);
    nsINode* commonAncestorNode = range->GetCommonAncestor();
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
    if (nsContentUtils::ContentIsDescendantOf(foundEditingHost, editingHost)) {
      continue;
    }
    if (nsContentUtils::ContentIsDescendantOf(editingHost, foundEditingHost)) {
      editingHost = foundEditingHost;
      continue;
    }
    // editingHost and foundEditingHost are not a descendant of the other.
    // So, there is no common editing host.
    return nullptr;
  }
  return editingHost;
}

nsresult
Selection::NotifySelectionListeners(bool aCalledByJS)
{
  AutoRestore<bool> calledFromJSRestorer(mCalledByJS);
  mCalledByJS = aCalledByJS;
  return NotifySelectionListeners();
}

nsresult
Selection::NotifySelectionListeners()
{
  if (!mFrameSelection)
    return NS_OK;//nothing to do

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
    nsPIDOMWindowOuter* window = GetWindow();
    nsIDocument* document = GetDocument();
    // If the document is in design mode or doesn't have contenteditable
    // element, we don't need to move focus.
    if (window && document && !document->HasFlag(NODE_IS_EDITABLE) &&
        GetHTMLEditor()) {
      RefPtr<Element> newEditingHost = GetCommonEditingHostForAllRanges();
      nsFocusManager* fm = nsFocusManager::GetFocusManager();
      nsCOMPtr<nsPIDOMWindowOuter> focusedWindow;
      nsIContent* focusedContent =
        nsFocusManager::GetFocusedDescendant(window,
                                             nsFocusManager::eOnlyCurrentWindow,
                                             getter_AddRefs(focusedWindow));
      nsCOMPtr<Element> focusedElement = do_QueryInterface(focusedContent);
      // When all selected ranges are in an editing host, it should take focus.
      // But otherwise, we shouldn't move focus since Chromium doesn't move
      // focus but only selection range is updated.
      if (newEditingHost && newEditingHost != focusedElement) {
        MOZ_ASSERT(!newEditingHost->IsInNativeAnonymousSubtree());
        // Note that don't steal focus from focused window if the window doesn't
        // have focus and if the window isn't focused window, shouldn't be
        // scrolled to the new focused element.
        uint32_t flags = nsIFocusManager::FLAG_NOSWITCHFRAME;
        if (focusedWindow != fm->GetFocusedWindow()) {
          flags |= nsIFocusManager::FLAG_NOSCROLL;
        }
        fm->SetFocus(newEditingHost, flags);
      }
    }
  }

  RefPtr<nsFrameSelection> frameSelection = mFrameSelection;
  if (frameSelection->GetBatching()) {
    frameSelection->SetDirty();
    return NS_OK;
  }
  if (mSelectionListeners.IsEmpty()) {
    // If there are no selection listeners, we're done!
    return NS_OK;
  }
  AutoTArray<nsCOMPtr<nsISelectionListener>, 8>
    selectionListeners(mSelectionListeners);

  nsCOMPtr<nsIDocument> doc;
  nsIPresShell* ps = GetPresShell();
  if (ps) {
    doc = ps->GetDocument();
  }

  short reason = frameSelection->PopReason();
  for (auto& listener : selectionListeners) {
    listener->NotifySelectionChanged(doc, this, reason);
  }
  return NS_OK;
}

void
Selection::StartBatchChanges()
{
  if (mFrameSelection) {
    RefPtr<nsFrameSelection> frameSelection = mFrameSelection;
    frameSelection->StartBatchChanges();
  }
}

void
Selection::EndBatchChanges(int16_t aReason)
{
  if (mFrameSelection) {
    RefPtr<nsFrameSelection> frameSelection = mFrameSelection;
    frameSelection->EndBatchChanges(aReason);
  }
}

void
Selection::AddSelectionChangeBlocker()
{
  mSelectionChangeBlockerCount++;
}

void
Selection::RemoveSelectionChangeBlocker()
{
  MOZ_ASSERT(mSelectionChangeBlockerCount > 0,
             "mSelectionChangeBlockerCount has an invalid value - "
             "maybe you have a mismatched RemoveSelectionChangeBlocker?");
  mSelectionChangeBlockerCount--;
}

bool
Selection::IsBlockingSelectionChangeEvents() const
{
  return mSelectionChangeBlockerCount > 0;
}

void
Selection::DeleteFromDocument(ErrorResult& aRv)
{
  if (!mFrameSelection)
    return;//nothing to do
  RefPtr<nsFrameSelection> frameSelection = mFrameSelection;
  nsresult rv = frameSelection->DeleteFromDocument();
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
  }
}

void
Selection::Modify(const nsAString& aAlter, const nsAString& aDirection,
                  const nsAString& aGranularity, ErrorResult& aRv)
{
  // Silently exit if there's no selection or no focus node.
  if (!mFrameSelection || !GetAnchorFocusRange() || !GetFocusNode()) {
    return;
  }

  if (!aAlter.LowerCaseEqualsLiteral("move") &&
      !aAlter.LowerCaseEqualsLiteral("extend")) {
    aRv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
    return;
  }

  if (!aDirection.LowerCaseEqualsLiteral("forward") &&
      !aDirection.LowerCaseEqualsLiteral("backward") &&
      !aDirection.LowerCaseEqualsLiteral("left") &&
      !aDirection.LowerCaseEqualsLiteral("right")) {
    aRv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
    return;
  }

  // Line moves are always visual.
  bool visual  = aDirection.LowerCaseEqualsLiteral("left") ||
                   aDirection.LowerCaseEqualsLiteral("right") ||
                   aGranularity.LowerCaseEqualsLiteral("line");

  bool forward = aDirection.LowerCaseEqualsLiteral("forward") ||
                   aDirection.LowerCaseEqualsLiteral("right");

  bool extend  = aAlter.LowerCaseEqualsLiteral("extend");

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
    aRv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
    return;
  }

  // If the anchor doesn't equal the focus and we try to move without first
  // collapsing the selection, MoveCaret will collapse the selection and quit.
  // To avoid this, we need to collapse the selection first.
  nsresult rv = NS_OK;
  if (!extend) {
    nsINode* focusNode = GetFocusNode();
    // We should have checked earlier that there was a focus node.
    if (!focusNode) {
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return;
    }
    uint32_t focusOffset = FocusOffset();
    Collapse(focusNode, focusOffset);
  }

  // If the paragraph direction of the focused frame is right-to-left,
  // we may have to swap the direction of movement.
  nsIFrame *frame;
  int32_t offset;
  rv = GetPrimaryFrameForFocusNode(&frame, &offset, visual);
  if (NS_SUCCEEDED(rv) && frame) {
    nsBidiDirection paraDir = nsBidiPresUtils::ParagraphDirection(frame);

    if (paraDir == NSBIDI_RTL && visual) {
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
  rv = frameSelection->MoveCaret(forward ? eDirNext : eDirPrevious,
                                 extend, amount,
                                 visual ? nsFrameSelection::eVisual
                                        : nsFrameSelection::eLogical);

  if (aGranularity.LowerCaseEqualsLiteral("line") && NS_FAILED(rv)) {
    nsCOMPtr<nsISelectionController> shell =
      do_QueryInterface(frameSelection->GetShell());
    if (!shell)
      return;
    shell->CompleteMove(forward, extend);
  }
}

void
Selection::SetBaseAndExtentJS(nsINode& aAnchorNode,
                              uint32_t aAnchorOffset,
                              nsINode& aFocusNode,
                              uint32_t aFocusOffset,
                              ErrorResult& aRv)
{
  AutoRestore<bool> calledFromJSRestorer(mCalledByJS);
  mCalledByJS = true;
  SetBaseAndExtent(aAnchorNode, aAnchorOffset,
                   aFocusNode, aFocusOffset, aRv);
}

void
Selection::SetBaseAndExtent(nsINode& aAnchorNode, uint32_t aAnchorOffset,
                            nsINode& aFocusNode, uint32_t aFocusOffset,
                            ErrorResult& aRv)
{
  if (!mFrameSelection) {
    return;
  }

  if (!HasSameRoot(aAnchorNode) ||
      !HasSameRoot(aFocusNode)) {
    // Return with no error
    return;
  }

  SelectionBatcher batch(this);

  int32_t relativePosition =
    nsContentUtils::ComparePoints(&aAnchorNode, aAnchorOffset,
                                  &aFocusNode, aFocusOffset);
  nsINode* start = &aAnchorNode;
  nsINode* end = &aFocusNode;
  uint32_t startOffset = aAnchorOffset;
  uint32_t endOffset = aFocusOffset;
  if (relativePosition > 0) {
    start = &aFocusNode;
    end = &aAnchorNode;
    startOffset = aFocusOffset;
    endOffset = aAnchorOffset;
  }

  // If there is cached range, we should reuse it for saving the allocation
  // const (and some other cost in nsRange::DoSetRange().
  RefPtr<nsRange> newRange = std::move(mCachedRange);

  nsresult rv = NS_OK;
  if (newRange) {
    rv = newRange->SetStartAndEnd(start, startOffset, end, endOffset);
  } else {
    rv = nsRange::CreateRange(start, startOffset, end, endOffset,
                              getter_AddRefs(newRange));
  }

  // nsRange::SetStartAndEnd() and nsRange::CreateRange() returns
  // IndexSizeError if any offset is out of bounds.
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return;
  }

  RemoveAllRanges(aRv);
  if (aRv.Failed()) {
    return;
  }

  AddRange(*newRange, aRv);
  if (aRv.Failed()) {
    return;
  }

  SetDirection(relativePosition > 0 ? eDirPrevious : eDirNext);
}

/** SelectionLanguageChange modifies the cursor Bidi level after a change in keyboard direction
 *  @param aLangRTL is true if the new language is right-to-left or false if the new language is left-to-right
 */
nsresult
Selection::SelectionLanguageChange(bool aLangRTL)
{
  if (!mFrameSelection)
    return NS_ERROR_NOT_INITIALIZED; // Can't do selection

  RefPtr<nsFrameSelection> frameSelection = mFrameSelection;

  // if the direction of the language hasn't changed, nothing to do
  nsBidiLevel kbdBidiLevel = aLangRTL ? NSBIDI_RTL : NSBIDI_LTR;
  if (kbdBidiLevel == frameSelection->mKbdBidiLevel) {
    return NS_OK;
  }

  frameSelection->mKbdBidiLevel = kbdBidiLevel;

  nsresult result;
  nsIFrame *focusFrame = 0;

  result = GetPrimaryFrameForFocusNode(&focusFrame, nullptr, false);
  if (NS_FAILED(result)) {
    return result;
  }
  if (!focusFrame) {
    return NS_ERROR_FAILURE;
  }

  int32_t frameStart, frameEnd;
  focusFrame->GetOffsets(frameStart, frameEnd);
  RefPtr<nsPresContext> context = GetPresContext();
  nsBidiLevel levelBefore, levelAfter;
  if (!context) {
    return NS_ERROR_FAILURE;
  }

  nsBidiLevel level = focusFrame->GetEmbeddingLevel();
  int32_t focusOffset = static_cast<int32_t>(FocusOffset());
  if ((focusOffset != frameStart) && (focusOffset != frameEnd))
    // the cursor is not at a frame boundary, so the level of both the characters (logically) before and after the cursor
    //  is equal to the frame level
    levelBefore = levelAfter = level;
  else {
    // the cursor is at a frame boundary, so use GetPrevNextBidiLevels to find the level of the characters
    //  before and after the cursor
    nsCOMPtr<nsIContent> focusContent = do_QueryInterface(GetFocusNode());
    nsPrevNextBidiLevels levels = frameSelection->
      GetPrevNextBidiLevels(focusContent, focusOffset, false);

    levelBefore = levels.mLevelBefore;
    levelAfter = levels.mLevelAfter;
  }

  if (IS_SAME_DIRECTION(levelBefore, levelAfter)) {
    // if cursor is between two characters with the same orientation, changing the keyboard language
    //  must toggle the cursor level between the level of the character with the lowest level
    //  (if the new language corresponds to the orientation of that character) and this level plus 1
    //  (if the new language corresponds to the opposite orientation)
    if ((level != levelBefore) && (level != levelAfter))
      level = std::min(levelBefore, levelAfter);
    if (IS_SAME_DIRECTION(level, kbdBidiLevel))
      frameSelection->SetCaretBidiLevel(level);
    else
      frameSelection->SetCaretBidiLevel(level + 1);
  }
  else {
    // if cursor is between characters with opposite orientations, changing the keyboard language must change
    //  the cursor level to that of the adjacent character with the orientation corresponding to the new language.
    if (IS_SAME_DIRECTION(levelBefore, kbdBidiLevel))
      frameSelection->SetCaretBidiLevel(levelBefore);
    else
      frameSelection->SetCaretBidiLevel(levelAfter);
  }

  // The caret might have moved, so invalidate the desired position
  // for future usages of up-arrow or down-arrow
  frameSelection->InvalidateDesiredPos();

  return NS_OK;
}

void
Selection::SetColors(const nsAString& aForegroundColor,
                     const nsAString& aBackgroundColor,
                     const nsAString& aAltForegroundColor,
                     const nsAString& aAltBackgroundColor,
                     ErrorResult& aRv)
{
  if (mSelectionType != SelectionType::eFind) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  mCustomColors.reset(new SelectionCustomColors);

  NS_NAMED_LITERAL_STRING(currentColorStr, "currentColor");
  NS_NAMED_LITERAL_STRING(transparentStr, "transparent");

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

void
Selection::ResetColors(ErrorResult& aRv)
{
  mCustomColors = nullptr;
}

JSObject*
Selection::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return mozilla::dom::SelectionBinding::Wrap(aCx, this, aGivenProto);
}

// AutoHideSelectionChanges
AutoHideSelectionChanges::AutoHideSelectionChanges(const nsFrameSelection* aFrame)
  : AutoHideSelectionChanges(
      aFrame ? aFrame->GetSelection(SelectionType::eNormal) : nullptr)
{}

bool
Selection::HasSameRoot(nsINode& aNode)
{
  nsINode* root = aNode.SubtreeRoot();
  nsIDocument* doc = GetParentObject();
  return doc == root || (root && doc == root->GetComposedDoc());
}
