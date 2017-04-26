/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Implementation of selection: nsISelection,nsISelectionPrivate and nsFrameSelection
 */

#include "mozilla/dom/Selection.h"

#include "mozilla/Attributes.h"
#include "mozilla/AutoRestore.h"
#include "mozilla/EventStates.h"

#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsFrameSelection.h"
#include "nsISelectionListener.h"
#include "nsContentCID.h"
#include "nsDeviceContext.h"
#include "nsIContent.h"
#include "nsIDOMNode.h"
#include "nsRange.h"
#include "nsCOMArray.h"
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
#include "nsIFrameTraversal.h"
#include "nsLayoutUtils.h"
#include "nsLayoutCID.h"
#include "nsBidiPresUtils.h"
static NS_DEFINE_CID(kFrameTraversalCID, NS_FRAMETRAVERSAL_CID);
#include "nsTextFrame.h"

#include "nsIDOMText.h"

#include "nsContentUtils.h"
#include "nsThreadUtils.h"
#include "mozilla/Preferences.h"
#include "nsDOMClassInfoID.h"

#include "nsPresContext.h"
#include "nsIPresShell.h"
#include "nsCaret.h"
#include "AccessibleCaretEventHub.h"

#include "mozilla/MouseEvents.h"
#include "mozilla/TextEvents.h"

#include "nsITimer.h"
#include "nsFrameManager.h"
// notifications
#include "nsIDOMDocument.h"
#include "nsIDocument.h"

#include "nsISelectionController.h"//for the enums
#include "nsAutoCopyListener.h"
#include "SelectionChangeListener.h"
#include "nsCopySupport.h"
#include "nsIClipboard.h"
#include "nsIFrameInlines.h"

#include "nsIBidiKeyboard.h"

#include "nsError.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/ShadowRoot.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/SelectionBinding.h"
#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/Telemetry.h"
#include "mozilla/layers/ScrollInputMethods.h"
#include "nsViewManager.h"

#include "nsIEditor.h"
#include "nsIHTMLEditor.h"
#include "nsFocusManager.h"
#include "nsPIDOMWindow.h"

using namespace mozilla;
using namespace mozilla::dom;
using mozilla::layers::ScrollInputMethod;

//#define DEBUG_TABLE 1

static bool IsValidSelectionPoint(nsFrameSelection *aFrameSel, nsINode *aNode);

static nsIAtom *GetTag(nsINode *aNode);
// returns the parent
static nsINode* ParentOffset(nsINode *aNode, int32_t *aChildOffset);
static nsINode* GetCellParent(nsINode *aDomNode);

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

static bool
IsValidSelectionType(RawSelectionType aRawSelectionType)
{
  switch (static_cast<SelectionType>(aRawSelectionType)) {
    case SelectionType::eNone:
    case SelectionType::eNormal:
    case SelectionType::eSpellCheck:
    case SelectionType::eIMERawClause:
    case SelectionType::eIMESelectedRawClause:
    case SelectionType::eIMEConvertedClause:
    case SelectionType::eIMESelectedClause:
    case SelectionType::eAccessibility:
    case SelectionType::eFind:
    case SelectionType::eURLSecondary:
    case SelectionType::eURLStrikeout:
      return true;
    default:
      return false;
  }
}

SelectionType
ToSelectionType(RawSelectionType aRawSelectionType)
{
  if (!IsValidSelectionType(aRawSelectionType)) {
    return SelectionType::eInvalid;
  }
  return static_cast<SelectionType>(aRawSelectionType);
}

RawSelectionType
ToRawSelectionType(SelectionType aSelectionType)
{
  return static_cast<RawSelectionType>(aSelectionType);
}

bool operator &(SelectionType aSelectionType,
                RawSelectionType aRawSelectionTypes)
{
  return (ToRawSelectionType(aSelectionType) & aRawSelectionTypes) != 0;
}

} // namespace mozilla

/******************************************************************************
 * nsPeekOffsetStruct
 ******************************************************************************/

//#define DEBUG_SELECTION // uncomment for printf describing every collapse and extend.
//#define DEBUG_NAVIGATION


//#define DEBUG_TABLE_SELECTION 1

nsPeekOffsetStruct::nsPeekOffsetStruct(nsSelectionAmount aAmount,
                                       nsDirection aDirection,
                                       int32_t aStartOffset,
                                       nsPoint aDesiredPos,
                                       bool aJumpLines,
                                       bool aScrollViewStop,
                                       bool aIsKeyboardSelect,
                                       bool aVisual,
                                       bool aExtend,
                                       EWordMovementType aWordMovementType)
  : mAmount(aAmount)
  , mDirection(aDirection)
  , mStartOffset(aStartOffset)
  , mDesiredPos(aDesiredPos)
  , mWordMovementType(aWordMovementType)
  , mJumpLines(aJumpLines)
  , mScrollViewStop(aScrollViewStop)
  , mIsKeyboardSelect(aIsKeyboardSelect)
  , mVisual(aVisual)
  , mExtend(aExtend)
  , mResultContent()
  , mResultFrame(nullptr)
  , mContentOffset(0)
  , mAttach(CARET_ASSOCIATE_BEFORE)
{
}

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
      nsresult result;
      mTimer = do_CreateInstance("@mozilla.org/timer;1", &result);
      mTimer->SetTarget(
        mPresContext->Document()->EventTargetFor(TaskCategory::Other));

      if (NS_FAILED(result)) {
        return result;
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
        frame->GetOffsetTo(mPresContext->PresShell()->FrameManager()->GetRootFrame());
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

NS_IMPL_ISUPPORTS(nsAutoScrollTimer, nsITimerCallback)

nsresult NS_NewDomSelection(nsISelection **aDomSelection)
{
  Selection* rlist = new Selection;
  *aDomSelection = (nsISelection *)rlist;
  NS_ADDREF(rlist);
  return NS_OK;
}

static int8_t
GetIndexFromSelectionType(SelectionType aSelectionType)
{
  switch (aSelectionType) {
    case SelectionType::eNormal:
      return 0;
    case SelectionType::eSpellCheck:
      return 1;
    case SelectionType::eIMERawClause:
      return 2;
    case SelectionType::eIMESelectedRawClause:
      return 3;
    case SelectionType::eIMEConvertedClause:
      return 4;
    case SelectionType::eIMESelectedClause:
      return 5;
    case SelectionType::eAccessibility:
      return 6;
    case SelectionType::eFind:
      return 7;
    case SelectionType::eURLSecondary:
      return 8;
    case SelectionType::eURLStrikeout:
      return 9;
    default:
      return -1;
  }
  /* NOTREACHED */
}

static SelectionType
GetSelectionTypeFromIndex(int8_t aIndex)
{
  static const SelectionType kSelectionTypes[] = {
    SelectionType::eNormal,
    SelectionType::eSpellCheck,
    SelectionType::eIMERawClause,
    SelectionType::eIMESelectedRawClause,
    SelectionType::eIMEConvertedClause,
    SelectionType::eIMESelectedClause,
    SelectionType::eAccessibility,
    SelectionType::eFind,
    SelectionType::eURLSecondary,
    SelectionType::eURLStrikeout
  };
  if (NS_WARN_IF(aIndex < 0) ||
      NS_WARN_IF(static_cast<size_t>(aIndex) >= ArrayLength(kSelectionTypes))) {
    return SelectionType::eNormal;
  }
  return kSelectionTypes[aIndex];
}

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
        aSelection->selectFrames(presContext, range, false);
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

////////////BEGIN nsFrameSelection methods

nsFrameSelection::nsFrameSelection()
{
  for (size_t i = 0; i < kPresentSelectionTypeCount; i++){
    mDomSelections[i] = new Selection(this);
    mDomSelections[i]->SetType(GetSelectionTypeFromIndex(i));
  }
  mBatching = 0;
  mChangesDuringBatching = false;
  mNotifyFrames = true;
  
  mMouseDoubleDownState = false;
  
  mHint = CARET_ASSOCIATE_BEFORE;
  mCaretBidiLevel = BIDI_LEVEL_UNDEFINED;
  mKbdBidiLevel = NSBIDI_LTR;

  mDragSelectingCells = false;
  mSelectingTableCellMode = 0;
  mSelectedCellIndex = 0;

  nsAutoCopyListener *autoCopy = nullptr;
  // On macOS, cache the current selection to send to osx service menu.
#ifdef XP_MACOSX
  autoCopy = nsAutoCopyListener::GetInstance(nsIClipboard::kSelectionCache);
#endif

  // Check to see if the autocopy pref is enabled
  //   and add the autocopy listener if it is
  if (Preferences::GetBool("clipboard.autocopy")) {
    autoCopy = nsAutoCopyListener::GetInstance(nsIClipboard::kSelectionClipboard);
  }

  if (autoCopy) {
    int8_t index = GetIndexFromSelectionType(SelectionType::eNormal);
    if (mDomSelections[index]) {
      autoCopy->Listen(mDomSelections[index]);
    }
  }

  mDisplaySelection = nsISelectionController::SELECTION_OFF;
  mSelectionChangeReason = nsISelectionListener::NO_REASON;

  mDelayedMouseEventValid = false;
  // These values are not used since they are only valid when
  // mDelayedMouseEventValid is true, and setting mDelayedMouseEventValid
  //alwaysoverrides these values.
  mDelayedMouseEventIsShift = false;
  mDelayedMouseEventClickCount = 0;
}

nsFrameSelection::~nsFrameSelection()
{
}

NS_IMPL_CYCLE_COLLECTION_CLASS(nsFrameSelection)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsFrameSelection)
  for (size_t i = 0; i < kPresentSelectionTypeCount; ++i) {
    tmp->mDomSelections[i] = nullptr;
  }

  NS_IMPL_CYCLE_COLLECTION_UNLINK(mCellParent)
  tmp->mSelectingTableCellMode = 0;
  tmp->mDragSelectingCells = false;
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mStartSelectedCell)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mEndSelectedCell)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mAppendStartSelectedCell)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mUnselectCellOnMouseUp)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mMaintainRange)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mLimiter)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mAncestorLimiter)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsFrameSelection)
  if (tmp->mShell && tmp->mShell->GetDocument() &&
      nsCCUncollectableMarker::InGeneration(cb,
                                            tmp->mShell->GetDocument()->
                                              GetMarkedCCGeneration())) {
    return NS_SUCCESS_INTERRUPTED_TRAVERSE;
  }
  for (size_t i = 0; i < kPresentSelectionTypeCount; ++i) {
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDomSelections[i])
  }

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCellParent)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mStartSelectedCell)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mEndSelectedCell)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mAppendStartSelectedCell)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mUnselectCellOnMouseUp)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mMaintainRange)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mLimiter)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mAncestorLimiter)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(nsFrameSelection, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(nsFrameSelection, Release)

// Get the x (or y, in vertical writing mode) position requested
// by the Key Handling for line-up/down
nsresult
nsFrameSelection::FetchDesiredPos(nsPoint &aDesiredPos)
{
  if (!mShell) {
    NS_ERROR("fetch desired position failed");
    return NS_ERROR_FAILURE;
  }
  if (mDesiredPosSet) {
    aDesiredPos = mDesiredPos;
    return NS_OK;
  }

  RefPtr<nsCaret> caret = mShell->GetCaret();
  if (!caret) {
    return NS_ERROR_NULL_POINTER;
  }

  int8_t index = GetIndexFromSelectionType(SelectionType::eNormal);
  caret->SetSelection(mDomSelections[index]);

  nsRect coord;
  nsIFrame* caretFrame = caret->GetGeometry(&coord);
  if (!caretFrame) {
    return NS_ERROR_FAILURE;
  }
  nsPoint viewOffset(0, 0);
  nsView* view = nullptr;
  caretFrame->GetOffsetFromView(viewOffset, &view);
  if (view) {
    coord += viewOffset;
  }
  aDesiredPos = coord.TopLeft();
  return NS_OK;
}

void
nsFrameSelection::InvalidateDesiredPos() // do not listen to mDesiredPos;
                                         // you must get another.
{
  mDesiredPosSet = false;
}

void
nsFrameSelection::SetDesiredPos(nsPoint aPos)
{
  mDesiredPos = aPos;
  mDesiredPosSet = true;
}

nsresult
nsFrameSelection::ConstrainFrameAndPointToAnchorSubtree(nsIFrame  *aFrame,
                                                        nsPoint&   aPoint,
                                                        nsIFrame **aRetFrame,
                                                        nsPoint&   aRetPoint)
{
  //
  // The whole point of this method is to return a frame and point that
  // that lie within the same valid subtree as the anchor node's frame,
  // for use with the method GetContentAndOffsetsFromPoint().
  //
  // A valid subtree is defined to be one where all the content nodes in
  // the tree have a valid parent-child relationship.
  //
  // If the anchor frame and aFrame are in the same subtree, aFrame will
  // be returned in aRetFrame. If they are in different subtrees, we
  // return the frame for the root of the subtree.
  //

  if (!aFrame || !aRetFrame)
    return NS_ERROR_NULL_POINTER;

  *aRetFrame = aFrame;
  aRetPoint  = aPoint;

  //
  // Get the frame and content for the selection's anchor point!
  //

  nsresult result;
  nsCOMPtr<nsIDOMNode> anchorNode;
  int32_t anchorOffset = 0;

  int8_t index = GetIndexFromSelectionType(SelectionType::eNormal);
  if (!mDomSelections[index])
    return NS_ERROR_NULL_POINTER;

  result = mDomSelections[index]->GetAnchorNode(getter_AddRefs(anchorNode));

  if (NS_FAILED(result))
    return result;

  if (!anchorNode)
    return NS_OK;

  result = mDomSelections[index]->GetAnchorOffset(&anchorOffset);

  if (NS_FAILED(result))
    return result;

  nsCOMPtr<nsIContent> anchorContent = do_QueryInterface(anchorNode);

  if (!anchorContent)
    return NS_ERROR_FAILURE;
  
  //
  // Now find the root of the subtree containing the anchor's content.
  //

  NS_ENSURE_STATE(mShell);
  nsIContent* anchorRoot = anchorContent->GetSelectionRootContent(mShell);
  NS_ENSURE_TRUE(anchorRoot, NS_ERROR_UNEXPECTED);

  //
  // Now find the root of the subtree containing aFrame's content.
  //

  nsIContent* content = aFrame->GetContent();

  if (content)
  {
    nsIContent* contentRoot = content->GetSelectionRootContent(mShell);
    NS_ENSURE_TRUE(contentRoot, NS_ERROR_UNEXPECTED);

    if (anchorRoot == contentRoot)
    {
      // If the aFrame's content isn't the capturing content, it should be
      // a descendant.  At this time, we can return simply.
      nsIContent* capturedContent = nsIPresShell::GetCapturingContent();
      if (capturedContent != content)
      {
        return NS_OK;
      }

      // Find the frame under the mouse cursor with the root frame.
      // At this time, don't use the anchor's frame because it may not have
      // fixed positioned frames.
      nsIFrame* rootFrame = mShell->FrameManager()->GetRootFrame();
      nsPoint ptInRoot = aPoint + aFrame->GetOffsetTo(rootFrame);
      nsIFrame* cursorFrame =
        nsLayoutUtils::GetFrameForPoint(rootFrame, ptInRoot);

      // If the mouse cursor in on a frame which is descendant of same
      // selection root, we can expand the selection to the frame.
      if (cursorFrame && cursorFrame->PresContext()->PresShell() == mShell)
      {
        nsIContent* cursorContent = cursorFrame->GetContent();
        NS_ENSURE_TRUE(cursorContent, NS_ERROR_FAILURE);
        nsIContent* cursorContentRoot =
          cursorContent->GetSelectionRootContent(mShell);
        NS_ENSURE_TRUE(cursorContentRoot, NS_ERROR_UNEXPECTED);
        if (cursorContentRoot == anchorRoot)
        {
          *aRetFrame = cursorFrame;
          aRetPoint = aPoint + aFrame->GetOffsetTo(cursorFrame);
          return NS_OK;
        }
      }
      // Otherwise, e.g., the cursor isn't on any frames (e.g., the mouse
      // cursor is out of the window), we should use the frame of the anchor
      // root.
    }
  }

  //
  // When we can't find a frame which is under the mouse cursor and has a same
  // selection root as the anchor node's, we should return the selection root
  // frame.
  //

  *aRetFrame = anchorRoot->GetPrimaryFrame();

  if (!*aRetFrame)
    return NS_ERROR_FAILURE;

  //
  // Now make sure that aRetPoint is converted to the same coordinate
  // system used by aRetFrame.
  //

  aRetPoint = aPoint + aFrame->GetOffsetTo(*aRetFrame);

  return NS_OK;
}

void
nsFrameSelection::SetCaretBidiLevel(nsBidiLevel aLevel)
{
  // If the current level is undefined, we have just inserted new text.
  // In this case, we don't want to reset the keyboard language
  mCaretBidiLevel = aLevel;

  RefPtr<nsCaret> caret;
  if (mShell && (caret = mShell->GetCaret())) {
    caret->SchedulePaint();
  }

  return;
}

nsBidiLevel
nsFrameSelection::GetCaretBidiLevel() const
{
  return mCaretBidiLevel;
}

void
nsFrameSelection::UndefineCaretBidiLevel()
{
  mCaretBidiLevel |= BIDI_LEVEL_UNDEFINED;
}

#ifdef PRINT_RANGE
void printRange(nsRange *aDomRange)
{
  if (!aDomRange)
  {
    printf("NULL nsIDOMRange\n");
  }
  nsINode* startNode = aDomRange->GetStartParent();
  nsINode* endNode = aDomRange->GetEndParent();
  int32_t startOffset = aDomRange->StartOffset();
  int32_t endOffset = aDomRange->EndOffset();
  
  printf("range: 0x%lx\t start: 0x%lx %ld, \t end: 0x%lx,%ld\n",
         (unsigned long)aDomRange,
         (unsigned long)startNode, (long)startOffset,
         (unsigned long)endNode, (long)endOffset);
         
}
#endif /* PRINT_RANGE */

static
nsIAtom *GetTag(nsINode *aNode)
{
  nsCOMPtr<nsIContent> content = do_QueryInterface(aNode);
  if (!content) 
  {
    NS_NOTREACHED("bad node passed to GetTag()");
    return nullptr;
  }
  
  return content->NodeInfo()->NameAtom();
}

// Returns the parent
nsINode*
ParentOffset(nsINode *aNode, int32_t *aChildOffset)
{
  if (!aNode || !aChildOffset)
    return nullptr;

  nsIContent* parent = aNode->GetParent();
  if (parent)
  {
    *aChildOffset = parent->IndexOf(aNode);

    return parent;
  }

  return nullptr;
}

static nsINode*
GetCellParent(nsINode *aDomNode)
{
    if (!aDomNode)
      return nullptr;
    nsINode* current = aDomNode;
    // Start with current node and look for a table cell
    while (current)
    {
      nsIAtom* tag = GetTag(current);
      if (tag == nsGkAtoms::td || tag == nsGkAtoms::th)
        return current;
      current = current->GetParent();
    }
    return nullptr;
}

void
nsFrameSelection::Init(nsIPresShell *aShell, nsIContent *aLimiter)
{
  mShell = aShell;
  mDragState = false;
  mDesiredPosSet = false;
  mLimiter = aLimiter;
  mCaretMovementStyle =
    Preferences::GetInt("bidi.edit.caret_movement_style", 2);

  // This should only ever be initialized on the main thread, so we are OK here.
  static bool prefCachesInitialized = false;
  if (!prefCachesInitialized) {
    prefCachesInitialized = true;

    Preferences::AddBoolVarCache(&sSelectionEventsEnabled,
                                 "dom.select_events.enabled", false);
    Preferences::AddBoolVarCache(&sSelectionEventsOnTextControlsEnabled,
                                 "dom.select_events.textcontrols.enabled", false);
  }

  RefPtr<AccessibleCaretEventHub> eventHub = mShell->GetAccessibleCaretEventHub();
  if (eventHub) {
    int8_t index = GetIndexFromSelectionType(SelectionType::eNormal);
    if (mDomSelections[index]) {
      mDomSelections[index]->AddSelectionListener(eventHub);
    }
  }

  nsIDocument* doc = aShell->GetDocument();
  if (sSelectionEventsEnabled ||
      (doc && nsContentUtils::IsSystemPrincipal(doc->NodePrincipal()))) {
    int8_t index = GetIndexFromSelectionType(SelectionType::eNormal);
    if (mDomSelections[index]) {
      // The Selection instance will hold a strong reference to its selectionchangelistener
      // so we don't have to worry about that!
      RefPtr<SelectionChangeListener> listener = new SelectionChangeListener;
      mDomSelections[index]->AddSelectionListener(listener);
    }
  }
}

bool nsFrameSelection::sSelectionEventsEnabled = false;
bool nsFrameSelection::sSelectionEventsOnTextControlsEnabled = false;

nsresult
nsFrameSelection::MoveCaret(nsDirection       aDirection,
                            bool              aContinueSelection,
                            nsSelectionAmount aAmount,
                            CaretMovementStyle aMovementStyle)
{
  bool visualMovement = aMovementStyle == eVisual ||
    (aMovementStyle == eUsePrefStyle &&
      (mCaretMovementStyle == 1 ||
        (mCaretMovementStyle == 2 && !aContinueSelection)));

  NS_ENSURE_STATE(mShell);
  // Flush out layout, since we need it to be up to date to do caret
  // positioning.
  mShell->FlushPendingNotifications(FlushType::Layout);

  if (!mShell) {
    return NS_OK;
  }

  nsPresContext *context = mShell->GetPresContext();
  if (!context)
    return NS_ERROR_FAILURE;

  bool isCollapsed;
  nsPoint desiredPos(0, 0); //we must keep this around and revalidate it when its just UP/DOWN

  int8_t index = GetIndexFromSelectionType(SelectionType::eNormal);
  RefPtr<Selection> sel = mDomSelections[index];
  if (!sel)
    return NS_ERROR_NULL_POINTER;

  int32_t scrollFlags = Selection::SCROLL_FOR_CARET_MOVE;
  nsINode* focusNode = sel->GetFocusNode();
  if (focusNode &&
      (focusNode->IsEditable() ||
       (focusNode->IsElement() &&
        focusNode->AsElement()->State().
          HasState(NS_EVENT_STATE_MOZ_READWRITE)))) {
    // If caret moves in editor, it should cause scrolling even if it's in
    // overflow: hidden;.
    scrollFlags |= Selection::SCROLL_OVERFLOW_HIDDEN;
  }

  nsresult result = sel->GetIsCollapsed(&isCollapsed);
  if (NS_FAILED(result)) {
    return result;
  }

  int32_t caretStyle = Preferences::GetInt("layout.selection.caret_style", 0);
  if (caretStyle == 0
#ifdef XP_WIN
      && aAmount != eSelectLine
#endif
     ) {
    // Put caret at the selection edge in the |aDirection| direction.
    caretStyle = 2;
  }

  bool doCollapse = !isCollapsed && !aContinueSelection && caretStyle == 2 &&
                    aAmount <= eSelectLine;
  if (doCollapse) {
    if (aDirection == eDirPrevious) {
      PostReason(nsISelectionListener::COLLAPSETOSTART_REASON);
      mHint = CARET_ASSOCIATE_AFTER;
    } else {
      PostReason(nsISelectionListener::COLLAPSETOEND_REASON);
      mHint = CARET_ASSOCIATE_BEFORE;
    }
  } else {
    PostReason(nsISelectionListener::KEYPRESS_REASON);
  }

  AutoPrepareFocusRange prep(sel, aContinueSelection, false);

  if (aAmount == eSelectLine) {
    result = FetchDesiredPos(desiredPos);
    if (NS_FAILED(result)) {
      return result;
    }
    SetDesiredPos(desiredPos);
  }

  if (doCollapse) {
    const nsRange* anchorFocusRange = sel->GetAnchorFocusRange();
    if (anchorFocusRange) {
      nsINode* node;
      int32_t offset;
      if (aDirection == eDirPrevious) {
        node =  anchorFocusRange->GetStartParent();
        offset = anchorFocusRange->StartOffset();
      } else {
        node = anchorFocusRange->GetEndParent();
        offset = anchorFocusRange->EndOffset();
      }
      sel->Collapse(node, offset);
    }
    sel->ScrollIntoView(nsISelectionController::SELECTION_FOCUS_REGION,
                        nsIPresShell::ScrollAxis(),
                        nsIPresShell::ScrollAxis(), scrollFlags);
    return NS_OK;
  }

  nsIFrame *frame;
  int32_t offsetused = 0;
  result = sel->GetPrimaryFrameForFocusNode(&frame, &offsetused,
                                            visualMovement);

  if (NS_FAILED(result) || !frame)
    return NS_FAILED(result) ? result : NS_ERROR_FAILURE;

  //set data using mLimiter to stop on scroll views.  If we have a limiter then we stop peeking
  //when we hit scrollable views.  If no limiter then just let it go ahead
  nsPeekOffsetStruct pos(aAmount, eDirPrevious, offsetused, desiredPos,
                         true, mLimiter != nullptr, true, visualMovement,
                         aContinueSelection);

  nsBidiDirection paraDir = nsBidiPresUtils::ParagraphDirection(frame);

  CaretAssociateHint tHint(mHint); //temporary variable so we dont set mHint until it is necessary
  switch (aAmount){
   case eSelectCharacter:
    case eSelectCluster:
    case eSelectWord:
    case eSelectWordNoSpace:
      InvalidateDesiredPos();
      pos.mAmount = aAmount;
      pos.mDirection = (visualMovement && paraDir == NSBIDI_RTL)
                       ? nsDirection(1 - aDirection) : aDirection;
      break;
    case eSelectLine:
      pos.mAmount = aAmount;
      pos.mDirection = aDirection;
      break;
    case eSelectBeginLine:
    case eSelectEndLine:
      InvalidateDesiredPos();
      pos.mAmount = aAmount;
      pos.mDirection = (visualMovement && paraDir == NSBIDI_RTL)
                       ? nsDirection(1 - aDirection) : aDirection;
      break;
    default:
      return NS_ERROR_FAILURE;
  }

  if (NS_SUCCEEDED(result = frame->PeekOffset(&pos)) && pos.mResultContent)
  {
    nsIFrame *theFrame;
    int32_t currentOffset, frameStart, frameEnd;

    if (aAmount <= eSelectWordNoSpace)
    {
      // For left/right, PeekOffset() sets pos.mResultFrame correctly, but does not set pos.mAttachForward,
      // so determine the hint here based on the result frame and offset:
      // If we're at the end of a text frame, set the hint to ASSOCIATE_BEFORE to indicate that we
      // want the caret displayed at the end of this frame, not at the beginning of the next one.
      theFrame = pos.mResultFrame;
      theFrame->GetOffsets(frameStart, frameEnd);
      currentOffset = pos.mContentOffset;
      if (frameEnd == currentOffset && !(frameStart == 0 && frameEnd == 0))
        tHint = CARET_ASSOCIATE_BEFORE;
      else
        tHint = CARET_ASSOCIATE_AFTER;
    } else {
      // For up/down and home/end, pos.mResultFrame might not be set correctly, or not at all.
      // In these cases, get the frame based on the content and hint returned by PeekOffset().
      tHint = pos.mAttach;
      theFrame = GetFrameForNodeOffset(pos.mResultContent, pos.mContentOffset,
                                       tHint, &currentOffset);
      if (!theFrame)
        return NS_ERROR_FAILURE;

      theFrame->GetOffsets(frameStart, frameEnd);
    }

    if (context->BidiEnabled())
    {
      switch (aAmount) {
        case eSelectBeginLine:
        case eSelectEndLine: {
          // In Bidi contexts, PeekOffset calculates pos.mContentOffset
          // differently depending on whether the movement is visual or logical.
          // For visual movement, pos.mContentOffset depends on the direction-
          // ality of the first/last frame on the line (theFrame), and the caret
          // directionality must correspond.
          FrameBidiData bidiData = theFrame->GetBidiData();
          SetCaretBidiLevel(visualMovement ? bidiData.embeddingLevel
                                           : bidiData.baseLevel);
          break;
        }
        default:
          // If the current position is not a frame boundary, it's enough just
          // to take the Bidi level of the current frame
          if ((pos.mContentOffset != frameStart &&
               pos.mContentOffset != frameEnd) ||
              eSelectLine == aAmount) {
            SetCaretBidiLevel(theFrame->GetEmbeddingLevel());
          }
          else {
            BidiLevelFromMove(mShell, pos.mResultContent, pos.mContentOffset,
                              aAmount, tHint);
          }
      }
    }
    result = TakeFocus(pos.mResultContent, pos.mContentOffset, pos.mContentOffset,
                       tHint, aContinueSelection, false);
  } else if (aAmount <= eSelectWordNoSpace && aDirection == eDirNext &&
             !aContinueSelection) {
    // Collapse selection if PeekOffset failed, we either
    //  1. bumped into the BRFrame, bug 207623
    //  2. had select-all in a text input (DIV range), bug 352759.
    bool isBRFrame = frame->GetType() == nsGkAtoms::brFrame;
    sel->Collapse(sel->GetFocusNode(), sel->FocusOffset());
    // Note: 'frame' might be dead here.
    if (!isBRFrame) {
      mHint = CARET_ASSOCIATE_BEFORE; // We're now at the end of the frame to the left.
    }
    result = NS_OK;
  }
  if (NS_SUCCEEDED(result))
  {
    result = mDomSelections[index]->
      ScrollIntoView(nsISelectionController::SELECTION_FOCUS_REGION,
                     nsIPresShell::ScrollAxis(), nsIPresShell::ScrollAxis(),
                     scrollFlags);
  }

  return result;
}

//END nsFrameSelection methods


//BEGIN nsFrameSelection methods

NS_IMETHODIMP
Selection::ToString(nsAString& aReturn)
{
  // We need FlushType::Frames here to make sure frames have been created for
  // the selected content.  Use mFrameSelection->GetShell() which returns
  // null if the Selection has been disconnected (the shell is Destroyed).
  nsCOMPtr<nsIPresShell> shell =
    mFrameSelection ? mFrameSelection->GetShell() : nullptr;
  if (!shell) {
    aReturn.Truncate();
    return NS_OK;
  }
  shell->FlushPendingNotifications(FlushType::Frames);

  return ToStringWithFormat("text/plain",
                            nsIDocumentEncoder::SkipInvisibleContent,
                            0, aReturn);
}

void
Selection::Stringify(nsAString& aResult)
{
  // Eat the error code
  ToString(aResult);
}

NS_IMETHODIMP
Selection::ToStringWithFormat(const char* aFormatType, uint32_t aFlags,
                              int32_t aWrapCol, nsAString& aReturn)
{
  ErrorResult result;
  NS_ConvertUTF8toUTF16 format(aFormatType);
  ToStringWithFormat(format, aFlags, aWrapCol, aReturn, result);
  if (result.Failed()) {
    return result.StealNSResult();
  }
  return NS_OK;
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

  nsCOMPtr<nsIDOMDocument> domDoc = do_QueryInterface(doc);
  NS_ASSERTION(domDoc, "Need a document");

  // Flags should always include OutputSelectionOnly if we're coming from here:
  aFlags |= nsIDocumentEncoder::OutputSelectionOnly;
  nsAutoString readstring;
  readstring.Assign(aFormatType);
  rv = encoder->Init(domDoc, readstring, aFlags);
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

NS_IMETHODIMP
Selection::SetInterlinePosition(bool aHintRight)
{
  ErrorResult result;
  SetInterlinePosition(aHintRight, result);
  if (result.Failed()) {
    return result.StealNSResult();
  }
  return NS_OK;
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

NS_IMETHODIMP
Selection::GetInterlinePosition(bool* aHintRight)
{
  ErrorResult result;
  *aHintRight = GetInterlinePosition(result);
  if (result.Failed()) {
    return result.StealNSResult();
  }
  return NS_OK;
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

nsPrevNextBidiLevels
nsFrameSelection::GetPrevNextBidiLevels(nsIContent *aNode,
                                        uint32_t    aContentOffset,
                                        bool        aJumpLines) const
{
  return GetPrevNextBidiLevels(aNode, aContentOffset, mHint, aJumpLines);
}

nsPrevNextBidiLevels
nsFrameSelection::GetPrevNextBidiLevels(nsIContent*        aNode,
                                        uint32_t           aContentOffset,
                                        CaretAssociateHint aHint,
                                        bool               aJumpLines) const
{
  // Get the level of the frames on each side
  nsIFrame    *currentFrame;
  int32_t     currentOffset;
  int32_t     frameStart, frameEnd;
  nsDirection direction;
  
  nsPrevNextBidiLevels levels;
  levels.SetData(nullptr, nullptr, 0, 0);

  currentFrame = GetFrameForNodeOffset(aNode, aContentOffset,
                                       aHint, &currentOffset);
  if (!currentFrame)
    return levels;

  currentFrame->GetOffsets(frameStart, frameEnd);

  if (0 == frameStart && 0 == frameEnd)
    direction = eDirPrevious;
  else if (frameStart == currentOffset)
    direction = eDirPrevious;
  else if (frameEnd == currentOffset)
    direction = eDirNext;
  else {
    // we are neither at the beginning nor at the end of the frame, so we have no worries
    nsBidiLevel currentLevel = currentFrame->GetEmbeddingLevel();
    levels.SetData(currentFrame, currentFrame, currentLevel, currentLevel);
    return levels;
  }

  nsIFrame *newFrame;
  int32_t offset;
  bool jumpedLine, movedOverNonSelectableText;
  nsresult rv = currentFrame->GetFrameFromDirection(direction, false,
                                                    aJumpLines, true,
                                                    &newFrame, &offset, &jumpedLine,
                                                    &movedOverNonSelectableText);
  if (NS_FAILED(rv))
    newFrame = nullptr;

  FrameBidiData currentBidi = currentFrame->GetBidiData();
  nsBidiLevel currentLevel = currentBidi.embeddingLevel;
  nsBidiLevel newLevel = newFrame ? newFrame->GetEmbeddingLevel()
                                  : currentBidi.baseLevel;
  
  // If not jumping lines, disregard br frames, since they might be positioned incorrectly.
  // XXX This could be removed once bug 339786 is fixed.
  if (!aJumpLines) {
    if (currentFrame->GetType() == nsGkAtoms::brFrame) {
      currentFrame = nullptr;
      currentLevel = currentBidi.baseLevel;
    }
    if (newFrame && newFrame->GetType() == nsGkAtoms::brFrame) {
      newFrame = nullptr;
      newLevel = currentBidi.baseLevel;
    }
  }
  
  if (direction == eDirNext)
    levels.SetData(currentFrame, newFrame, currentLevel, newLevel);
  else
    levels.SetData(newFrame, currentFrame, newLevel, currentLevel);

  return levels;
}

nsresult
nsFrameSelection::GetFrameFromLevel(nsIFrame    *aFrameIn,
                                    nsDirection  aDirection,
                                    nsBidiLevel  aBidiLevel,
                                    nsIFrame   **aFrameOut) const
{
  NS_ENSURE_STATE(mShell);
  nsBidiLevel foundLevel = 0;
  nsIFrame *foundFrame = aFrameIn;

  nsCOMPtr<nsIFrameEnumerator> frameTraversal;
  nsresult result;
  nsCOMPtr<nsIFrameTraversal> trav(do_CreateInstance(kFrameTraversalCID,&result));
  if (NS_FAILED(result))
      return result;

  result = trav->NewFrameTraversal(getter_AddRefs(frameTraversal),
                                   mShell->GetPresContext(), aFrameIn,
                                   eLeaf,
                                   false, // aVisual
                                   false, // aLockInScrollView
                                   false, // aFollowOOFs
                                   false  // aSkipPopupChecks
                                   );
  if (NS_FAILED(result))
    return result;

  do {
    *aFrameOut = foundFrame;
    if (aDirection == eDirNext)
      frameTraversal->Next();
    else
      frameTraversal->Prev();

    foundFrame = frameTraversal->CurrentItem();
    if (!foundFrame)
      return NS_ERROR_FAILURE;
    foundLevel = foundFrame->GetEmbeddingLevel();

  } while (foundLevel > aBidiLevel);

  return NS_OK;
}


nsresult
nsFrameSelection::MaintainSelection(nsSelectionAmount aAmount)
{
  int8_t index = GetIndexFromSelectionType(SelectionType::eNormal);
  if (!mDomSelections[index])
    return NS_ERROR_NULL_POINTER;

  mMaintainedAmount = aAmount;

  const nsRange* anchorFocusRange =
    mDomSelections[index]->GetAnchorFocusRange();
  if (anchorFocusRange && aAmount != eSelectNoAmount) {
    mMaintainRange = anchorFocusRange->CloneRange();
    return NS_OK;
  }

  mMaintainRange = nullptr;
  return NS_OK;
}


/** After moving the caret, its Bidi level is set according to the following rules:
 *
 *  After moving over a character with left/right arrow, set to the Bidi level of the last moved over character.
 *  After Home and End, set to the paragraph embedding level.
 *  After up/down arrow, PageUp/Down, set to the lower level of the 2 surrounding characters.
 *  After mouse click, set to the level of the current frame.
 *
 *  The following two methods use GetPrevNextBidiLevels to determine the new Bidi level.
 *  BidiLevelFromMove is called when the caret is moved in response to a keyboard event
 *
 * @param aPresShell is the presentation shell
 * @param aNode is the content node
 * @param aContentOffset is the new caret position, as an offset into aNode
 * @param aAmount is the amount of the move that gave the caret its new position
 * @param aHint is the hint indicating in what logical direction the caret moved
 */
void nsFrameSelection::BidiLevelFromMove(nsIPresShell*      aPresShell,
                                         nsIContent*        aNode,
                                         uint32_t           aContentOffset,
                                         nsSelectionAmount  aAmount,
                                         CaretAssociateHint aHint)
{
  switch (aAmount) {

    // Movement within the line: the new cursor Bidi level is the level of the
    // last character moved over
    case eSelectCharacter:
    case eSelectCluster:
    case eSelectWord:
    case eSelectWordNoSpace:
    case eSelectBeginLine:
    case eSelectEndLine:
    case eSelectNoAmount:
    {
      nsPrevNextBidiLevels levels = GetPrevNextBidiLevels(aNode, aContentOffset,
                                                          aHint, false);

      SetCaretBidiLevel(aHint == CARET_ASSOCIATE_BEFORE ?
          levels.mLevelBefore : levels.mLevelAfter);
      break;
    }
      /*
    // Up and Down: the new cursor Bidi level is the smaller of the two surrounding characters      
    case eSelectLine:
    case eSelectParagraph:
      GetPrevNextBidiLevels(aContext, aNode, aContentOffset, &firstFrame, &secondFrame, &firstLevel, &secondLevel);
      aPresShell->SetCaretBidiLevel(std::min(firstLevel, secondLevel));
      break;
      */

    default:
      UndefineCaretBidiLevel();
  }
}

/**
 * BidiLevelFromClick is called when the caret is repositioned by clicking the mouse
 *
 * @param aNode is the content node
 * @param aContentOffset is the new caret position, as an offset into aNode
 */
void nsFrameSelection::BidiLevelFromClick(nsIContent *aNode,
                                          uint32_t    aContentOffset)
{
  nsIFrame* clickInFrame=nullptr;
  int32_t OffsetNotUsed;

  clickInFrame = GetFrameForNodeOffset(aNode, aContentOffset, mHint, &OffsetNotUsed);
  if (!clickInFrame)
    return;

  SetCaretBidiLevel(clickInFrame->GetEmbeddingLevel());
}


bool
nsFrameSelection::AdjustForMaintainedSelection(nsIContent *aContent,
                                               int32_t     aOffset)
{
  if (!mMaintainRange)
    return false;

  if (!aContent) {
    return false;
  }

  int8_t index = GetIndexFromSelectionType(SelectionType::eNormal);
  if (!mDomSelections[index])
    return false;

  nsINode* rangeStartNode = mMaintainRange->GetStartParent();
  nsINode* rangeEndNode = mMaintainRange->GetEndParent();
  int32_t rangeStartOffset = mMaintainRange->StartOffset();
  int32_t rangeEndOffset = mMaintainRange->EndOffset();

  int32_t relToStart =
    nsContentUtils::ComparePoints(rangeStartNode, rangeStartOffset,
                                  aContent, aOffset);
  int32_t relToEnd =
    nsContentUtils::ComparePoints(rangeEndNode, rangeEndOffset,
                                  aContent, aOffset);

  // If aContent/aOffset is inside the maintained selection, or if it is on the
  // "anchor" side of the maintained selection, we need to do something.
  if ((relToStart < 0 && relToEnd > 0) ||
      (relToStart > 0 &&
       mDomSelections[index]->GetDirection() == eDirNext) ||
      (relToEnd < 0 &&
       mDomSelections[index]->GetDirection() == eDirPrevious)) {
    // Set the current range to the maintained range. 
    mDomSelections[index]->ReplaceAnchorFocusRange(mMaintainRange);
    if (relToStart < 0 && relToEnd > 0) {
      // We're inside the maintained selection, just keep it selected.
      return true;
    }
    // Reverse the direction of the selection so that the anchor will be on the 
    // far side of the maintained selection, relative to aContent/aOffset.
    mDomSelections[index]->SetDirection(relToStart > 0 ? eDirPrevious : eDirNext);
  }
  return false;
}


nsresult
nsFrameSelection::HandleClick(nsIContent*        aNewFocus,
                              uint32_t           aContentOffset,
                              uint32_t           aContentEndOffset,
                              bool               aContinueSelection,
                              bool               aMultipleSelection,
                              CaretAssociateHint aHint)
{
  if (!aNewFocus)
    return NS_ERROR_INVALID_ARG;

  InvalidateDesiredPos();

  if (!aContinueSelection) {
    mMaintainRange = nullptr;
    if (!IsValidSelectionPoint(this, aNewFocus)) {
      mAncestorLimiter = nullptr;
    }
  }

  // Don't take focus when dragging off of a table
  if (!mDragSelectingCells)
  {
    BidiLevelFromClick(aNewFocus, aContentOffset);
    PostReason(nsISelectionListener::MOUSEDOWN_REASON + nsISelectionListener::DRAG_REASON);
    if (aContinueSelection &&
        AdjustForMaintainedSelection(aNewFocus, aContentOffset))
      return NS_OK; //shift clicked to maintained selection. rejected.

    int8_t index = GetIndexFromSelectionType(SelectionType::eNormal);
    AutoPrepareFocusRange prep(mDomSelections[index], aContinueSelection, aMultipleSelection);
    return TakeFocus(aNewFocus, aContentOffset, aContentEndOffset, aHint,
                     aContinueSelection, aMultipleSelection);
  }
  
  return NS_OK;
}

void
nsFrameSelection::HandleDrag(nsIFrame *aFrame, nsPoint aPoint)
{
  if (!aFrame || !mShell)
    return;

  nsresult result;
  nsIFrame *newFrame = 0;
  nsPoint   newPoint;

  result = ConstrainFrameAndPointToAnchorSubtree(aFrame, aPoint, &newFrame, newPoint);
  if (NS_FAILED(result))
    return;
  if (!newFrame)
    return;

  nsIFrame::ContentOffsets offsets =
      newFrame->GetContentOffsetsFromPoint(newPoint);
  if (!offsets.content)
    return;

  if (newFrame->IsSelected() &&
      AdjustForMaintainedSelection(offsets.content, offsets.offset))
    return;

  // Adjust offsets according to maintained amount
  if (mMaintainRange && 
      mMaintainedAmount != eSelectNoAmount) {    
    
    nsINode* rangenode = mMaintainRange->GetStartParent();
    int32_t rangeOffset = mMaintainRange->StartOffset();
    int32_t relativePosition =
      nsContentUtils::ComparePoints(rangenode, rangeOffset,
                                    offsets.content, offsets.offset);

    nsDirection direction = relativePosition > 0 ? eDirPrevious : eDirNext;
    nsSelectionAmount amount = mMaintainedAmount;
    if (amount == eSelectBeginLine && direction == eDirNext)
      amount = eSelectEndLine;

    int32_t offset;
    nsIFrame* frame = GetFrameForNodeOffset(offsets.content, offsets.offset,
        CARET_ASSOCIATE_AFTER, &offset);

    if (frame && amount == eSelectWord && direction == eDirPrevious) {
      // To avoid selecting the previous word when at start of word,
      // first move one character forward.
      nsPeekOffsetStruct charPos(eSelectCharacter, eDirNext, offset,
                                 nsPoint(0, 0), false, mLimiter != nullptr,
                                 false, false, false);
      if (NS_SUCCEEDED(frame->PeekOffset(&charPos))) {
        frame = charPos.mResultFrame;
        offset = charPos.mContentOffset;
      }
    }

    nsPeekOffsetStruct pos(amount, direction, offset, nsPoint(0, 0),
                           false, mLimiter != nullptr, false, false, false);

    if (frame && NS_SUCCEEDED(frame->PeekOffset(&pos)) && pos.mResultContent) {
      offsets.content = pos.mResultContent;
      offsets.offset = pos.mContentOffset;
    }
  }
  
  HandleClick(offsets.content, offsets.offset, offsets.offset,
              true, false, offsets.associate);
}

nsresult
nsFrameSelection::StartAutoScrollTimer(nsIFrame *aFrame,
                                       nsPoint   aPoint,
                                       uint32_t  aDelay)
{
  int8_t index = GetIndexFromSelectionType(SelectionType::eNormal);
  if (!mDomSelections[index]) {
    return NS_ERROR_NULL_POINTER;
  }

  return mDomSelections[index]->StartAutoScrollTimer(aFrame, aPoint, aDelay);
}

void
nsFrameSelection::StopAutoScrollTimer()
{
  int8_t index = GetIndexFromSelectionType(SelectionType::eNormal);
  if (!mDomSelections[index]) {
    return;
  }

  mDomSelections[index]->StopAutoScrollTimer();
}

/**
hard to go from nodes to frames, easy the other way!
 */
nsresult
nsFrameSelection::TakeFocus(nsIContent*        aNewFocus,
                            uint32_t           aContentOffset,
                            uint32_t           aContentEndOffset,
                            CaretAssociateHint aHint,
                            bool               aContinueSelection,
                            bool               aMultipleSelection)
{
  if (!aNewFocus)
    return NS_ERROR_NULL_POINTER;

  NS_ENSURE_STATE(mShell);

  if (!IsValidSelectionPoint(this,aNewFocus))
    return NS_ERROR_FAILURE;

  // Clear all table selection data
  mSelectingTableCellMode = 0;
  mDragSelectingCells = false;
  mStartSelectedCell = nullptr;
  mEndSelectedCell = nullptr;
  mAppendStartSelectedCell = nullptr;
  mHint = aHint;
  
  int8_t index = GetIndexFromSelectionType(SelectionType::eNormal);
  if (!mDomSelections[index])
    return NS_ERROR_NULL_POINTER;

  Maybe<Selection::AutoUserInitiated> userSelect;
  if (IsUserSelectionReason()) {
    userSelect.emplace(mDomSelections[index]);
  }

  //traverse through document and unselect crap here
  if (!aContinueSelection) {//single click? setting cursor down
    uint32_t batching = mBatching;//hack to use the collapse code.
    bool changes = mChangesDuringBatching;
    mBatching = 1;

    if (aMultipleSelection) {
      // Remove existing collapsed ranges as there's no point in having 
      // non-anchor/focus collapsed ranges.
      mDomSelections[index]->RemoveCollapsedRanges();

      RefPtr<nsRange> newRange = new nsRange(aNewFocus);

      newRange->SetStart(aNewFocus, aContentOffset);
      newRange->SetEnd(aNewFocus, aContentOffset);
      mDomSelections[index]->AddRange(newRange);
      mBatching = batching;
      mChangesDuringBatching = changes;
    } else {
      bool oldDesiredPosSet = mDesiredPosSet; //need to keep old desired position if it was set.
      mDomSelections[index]->Collapse(aNewFocus, aContentOffset);
      mDesiredPosSet = oldDesiredPosSet; //now reset desired pos back.
      mBatching = batching;
      mChangesDuringBatching = changes;
    }
    if (aContentEndOffset != aContentOffset) {
      mDomSelections[index]->Extend(aNewFocus, aContentEndOffset);
    }

    //find out if we are inside a table. if so, find out which one and which cell
    //once we do that, the next time we get a takefocus, check the parent tree. 
    //if we are no longer inside same table ,cell then switch to table selection mode.
    // BUT only do this in an editor

    NS_ENSURE_STATE(mShell);
    bool editableCell = false;
    RefPtr<nsPresContext> context = mShell->GetPresContext();
    if (context) {
      nsCOMPtr<nsIHTMLEditor> editor = do_QueryInterface(nsContentUtils::GetHTMLEditor(context));
      if (editor) {
        nsINode* cellparent = GetCellParent(aNewFocus);
        nsCOMPtr<nsINode> editorHostNode = editor->GetActiveEditingHost();
        editableCell = cellparent && editorHostNode && 
                   nsContentUtils::ContentIsDescendantOf(cellparent, editorHostNode);
        if (editableCell) {
          mCellParent = cellparent;
#ifdef DEBUG_TABLE_SELECTION
          printf(" * TakeFocus - Collapsing into new cell\n");
#endif
        }
      }
    }
  }
  else {
    // Now update the range list:
    if (aContinueSelection && aNewFocus)
    {
      int32_t offset;
      nsINode *cellparent = GetCellParent(aNewFocus);
      if (mCellParent && cellparent && cellparent != mCellParent) //switch to cell selection mode
      {
#ifdef DEBUG_TABLE_SELECTION
printf(" * TakeFocus - moving into new cell\n");
#endif
        WidgetMouseEvent event(false, eVoidEvent, nullptr,
                               WidgetMouseEvent::eReal);

        // Start selecting in the cell we were in before
        nsINode* parent = ParentOffset(mCellParent, &offset);
        if (parent)
          HandleTableSelection(parent, offset,
                               nsISelectionPrivate::TABLESELECTION_CELL, &event);

        // Find the parent of this new cell and extend selection to it
        parent = ParentOffset(cellparent, &offset);

        // XXXX We need to REALLY get the current key shift state
        //  (we'd need to add event listener -- let's not bother for now)
        event.mModifiers &= ~MODIFIER_SHIFT; //aContinueSelection;
        if (parent)
        {
          mCellParent = cellparent;
          // Continue selection into next cell
          HandleTableSelection(parent, offset,
                               nsISelectionPrivate::TABLESELECTION_CELL, &event);
        }
      }
      else
      {
        // XXXX Problem: Shift+click in browser is appending text selection to selected table!!!
        //   is this the place to erase seleced cells ?????
        if (mDomSelections[index]->GetDirection() == eDirNext && aContentEndOffset > aContentOffset) //didn't go far enough 
        {
          mDomSelections[index]->Extend(aNewFocus, aContentEndOffset);//this will only redraw the diff 
        }
        else
          mDomSelections[index]->Extend(aNewFocus, aContentOffset);
      }
    }
  }

  // Don't notify selection listeners if batching is on:
  if (GetBatching())
    return NS_OK;

  // Be aware, the Selection instance may be destroyed after this call.
  return NotifySelectionListeners(SelectionType::eNormal);
}


UniquePtr<SelectionDetails>
nsFrameSelection::LookUpSelection(nsIContent *aContent,
                                  int32_t aContentOffset,
                                  int32_t aContentLength,
                                  bool aSlowCheck) const
{
  if (!aContent || !mShell)
    return nullptr;

  UniquePtr<SelectionDetails> details;

  for (size_t j = 0; j < kPresentSelectionTypeCount; j++) {
    if (mDomSelections[j]) {
      details = mDomSelections[j]->LookUpSelection(aContent, aContentOffset,
                                                   aContentLength, Move(details),
                                                   ToSelectionType(1 << j),
                                                   aSlowCheck);
    }
  }

  return details;
}

void
nsFrameSelection::SetDragState(bool aState)
{
  if (mDragState == aState)
    return;

  mDragState = aState;
    
  if (!mDragState)
  {
    mDragSelectingCells = false;
    // Notify that reason is mouse up.
    PostReason(nsISelectionListener::MOUSEUP_REASON);
    // Be aware, the Selection instance may be destroyed after this call.
    NotifySelectionListeners(SelectionType::eNormal);
  }
}

Selection*
nsFrameSelection::GetSelection(SelectionType aSelectionType) const
{
  int8_t index = GetIndexFromSelectionType(aSelectionType);
  if (index < 0)
    return nullptr;

  return mDomSelections[index];
}

nsresult
nsFrameSelection::ScrollSelectionIntoView(SelectionType aSelectionType,
                                          SelectionRegion aRegion,
                                          int16_t         aFlags) const
{
  int8_t index = GetIndexFromSelectionType(aSelectionType);
  if (index < 0)
    return NS_ERROR_INVALID_ARG;

  if (!mDomSelections[index])
    return NS_ERROR_NULL_POINTER;

  nsIPresShell::ScrollAxis verticalScroll = nsIPresShell::ScrollAxis();
  int32_t flags = Selection::SCROLL_DO_FLUSH;
  if (aFlags & nsISelectionController::SCROLL_SYNCHRONOUS) {
    flags |= Selection::SCROLL_SYNCHRONOUS;
  } else if (aFlags & nsISelectionController::SCROLL_FIRST_ANCESTOR_ONLY) {
    flags |= Selection::SCROLL_FIRST_ANCESTOR_ONLY;
  }
  if (aFlags & nsISelectionController::SCROLL_OVERFLOW_HIDDEN) {
    flags |= Selection::SCROLL_OVERFLOW_HIDDEN;
  }
  if (aFlags & nsISelectionController::SCROLL_CENTER_VERTICALLY) {
    verticalScroll = nsIPresShell::ScrollAxis(
      nsIPresShell::SCROLL_CENTER, nsIPresShell::SCROLL_IF_NOT_FULLY_VISIBLE);
  }
  if (aFlags & nsISelectionController::SCROLL_FOR_CARET_MOVE) {
    flags |= Selection::SCROLL_FOR_CARET_MOVE;
  }

  // After ScrollSelectionIntoView(), the pending notifications might be
  // flushed and PresShell/PresContext/Frames may be dead. See bug 418470.
  RefPtr<Selection> sel = mDomSelections[index];
  return sel->ScrollIntoView(aRegion, verticalScroll,
                             nsIPresShell::ScrollAxis(), flags);
}

nsresult
nsFrameSelection::RepaintSelection(SelectionType aSelectionType)
{
  int8_t index = GetIndexFromSelectionType(aSelectionType);
  if (index < 0)
    return NS_ERROR_INVALID_ARG;
  if (!mDomSelections[index])
    return NS_ERROR_NULL_POINTER;
  NS_ENSURE_STATE(mShell);

// On macOS, update the selection cache to the new active selection
// aka the current selection.
#ifdef XP_MACOSX
  nsFocusManager* fm = nsFocusManager::GetFocusManager();
  // Check an active window exists otherwise there cannot be a current selection
  // and that it's a normal selection.
  if (fm->GetActiveWindow() && aSelectionType == SelectionType::eNormal) {
    UpdateSelectionCacheOnRepaintSelection(mDomSelections[index]);
  }
#endif
  return mDomSelections[index]->Repaint(mShell->GetPresContext());
}

nsIFrame*
nsFrameSelection::GetFrameForNodeOffset(nsIContent*        aNode,
                                        int32_t            aOffset,
                                        CaretAssociateHint aHint,
                                        int32_t*           aReturnOffset) const
{
  if (!aNode || !aReturnOffset || !mShell)
    return nullptr;

  if (aOffset < 0)
    return nullptr;

  if (!aNode->GetPrimaryFrame() &&
      !mShell->FrameManager()->GetDisplayContentsStyleFor(aNode)) {
    return nullptr;
  }

  nsIFrame* returnFrame = nullptr;
  nsCOMPtr<nsIContent> theNode;

  while (true) {
    *aReturnOffset = aOffset;

    theNode = aNode;

    if (aNode->IsElement()) {
      int32_t childIndex  = 0;
      int32_t numChildren = theNode->GetChildCount();

      if (aHint == CARET_ASSOCIATE_BEFORE) {
        if (aOffset > 0) {
          childIndex = aOffset - 1;
        } else {
          childIndex = aOffset;
        }
      } else {
        NS_ASSERTION(aHint == CARET_ASSOCIATE_AFTER, "unknown direction");
        if (aOffset >= numChildren) {
          if (numChildren > 0) {
            childIndex = numChildren - 1;
          } else {
            childIndex = 0;
          }
        } else {
          childIndex = aOffset;
        }
      }
      
      if (childIndex > 0 || numChildren > 0) {
        nsCOMPtr<nsIContent> childNode = theNode->GetChildAt(childIndex);

        if (!childNode) {
          break;
        }

        theNode = childNode;
      }

      // Now that we have the child node, check if it too
      // can contain children. If so, descend into child.
      if (theNode->IsElement() &&
          theNode->GetChildCount() &&
          !theNode->HasIndependentSelection()) {
        aNode = theNode;
        aOffset = aOffset > childIndex ? theNode->GetChildCount() : 0;
        continue;
      } else {
        // Check to see if theNode is a text node. If it is, translate
        // aOffset into an offset into the text node.

        nsCOMPtr<nsIDOMText> textNode = do_QueryInterface(theNode);
        if (textNode) {
          if (theNode->GetPrimaryFrame()) {
            if (aOffset > childIndex) {
              uint32_t textLength = 0;
              nsresult rv = textNode->GetLength(&textLength);
              if (NS_FAILED(rv)) {
                break;
              }

              *aReturnOffset = (int32_t)textLength;
            } else {
              *aReturnOffset = 0;
            }
          } else {
            int32_t numChildren = aNode->GetChildCount();
            int32_t newChildIndex =
              aHint == CARET_ASSOCIATE_BEFORE ? childIndex - 1 : childIndex + 1;

            if (newChildIndex >= 0 && newChildIndex < numChildren) {
              nsCOMPtr<nsIContent> newChildNode = aNode->GetChildAt(newChildIndex);
              if (!newChildNode) {
                return nullptr;
              }

              aNode = newChildNode;
              aOffset = aHint == CARET_ASSOCIATE_BEFORE ? aNode->GetChildCount() : 0;
              continue;
            } else {
              // newChildIndex is illegal which means we're at first or last
              // child. Just use original node to get the frame.
              theNode = aNode;
            }
          }
        }
      }
    }

    // If the node is a ShadowRoot, the frame needs to be adjusted,
    // because a ShadowRoot does not get a frame. Its children are rendered
    // as children of the host.
    mozilla::dom::ShadowRoot* shadowRoot =
      mozilla::dom::ShadowRoot::FromNode(theNode);
    if (shadowRoot) {
      theNode = shadowRoot->GetHost();
    }

    returnFrame = theNode->GetPrimaryFrame();
    if (!returnFrame) {
      if (aHint == CARET_ASSOCIATE_BEFORE) {
        if (aOffset > 0) {
          --aOffset;
          continue;
        } else {
          break;
        }
      } else {
        int32_t end = theNode->GetChildCount();
        if (aOffset < end) {
          ++aOffset;
          continue;
        } else {
          break;
        }
      }
    }

    break;
  } // end while

  if (!returnFrame)
    return nullptr;

  // If we ended up here and were asked to position the caret after a visible
  // break, let's return the frame on the next line instead if it exists.
  if (aOffset > 0 &&  (uint32_t) aOffset >= aNode->Length() &&
      theNode == aNode->GetLastChild()) {
    nsIFrame* newFrame;
    nsLayoutUtils::IsInvisibleBreak(theNode, &newFrame);
    if (newFrame) {
      returnFrame = newFrame;
      *aReturnOffset = 0;
    }
  }

  // find the child frame containing the offset we want
  returnFrame->GetChildFrameContainingOffset(*aReturnOffset, aHint == CARET_ASSOCIATE_AFTER,
                                             &aOffset, &returnFrame);
  return returnFrame;
}

void
nsFrameSelection::CommonPageMove(bool aForward,
                                 bool aExtend,
                                 nsIScrollableFrame* aScrollableFrame)
{
  // expected behavior for PageMove is to scroll AND move the caret
  // and remain relative position of the caret in view. see Bug 4302.

  //get the frame from the scrollable view

  nsIFrame* scrolledFrame = aScrollableFrame->GetScrolledFrame();
  if (!scrolledFrame)
    return;

  // find out where the caret is.
  // we should know mDesiredPos value of nsFrameSelection, but I havent seen that behavior in other windows applications yet.
  nsISelection* domSel = GetSelection(SelectionType::eNormal);
  if (!domSel) {
    return;
  }

  nsRect caretPos;
  nsIFrame* caretFrame = nsCaret::GetGeometry(domSel, &caretPos);
  if (!caretFrame) 
    return;
  
  //need to adjust caret jump by percentage scroll
  nsSize scrollDelta = aScrollableFrame->GetPageScrollAmount();

  if (aForward)
    caretPos.y += scrollDelta.height;
  else
    caretPos.y -= scrollDelta.height;

  caretPos += caretFrame->GetOffsetTo(scrolledFrame);
    
  // get a content at desired location
  nsPoint desiredPoint;
  desiredPoint.x = caretPos.x;
  desiredPoint.y = caretPos.y + caretPos.height/2;
  nsIFrame::ContentOffsets offsets =
      scrolledFrame->GetContentOffsetsFromPoint(desiredPoint);

  if (!offsets.content)
    return;

  // scroll one page
  mozilla::Telemetry::Accumulate(mozilla::Telemetry::SCROLL_INPUT_METHODS,
      (uint32_t) ScrollInputMethod::MainThreadScrollPage);
  aScrollableFrame->ScrollBy(nsIntPoint(0, aForward ? 1 : -1),
                             nsIScrollableFrame::PAGES,
                             nsIScrollableFrame::SMOOTH);

  // place the caret
  HandleClick(offsets.content, offsets.offset,
              offsets.offset, aExtend, false, CARET_ASSOCIATE_AFTER);
}

nsresult
nsFrameSelection::PhysicalMove(int16_t aDirection, int16_t aAmount,
                               bool aExtend)
{
  NS_ENSURE_STATE(mShell);
  // Flush out layout, since we need it to be up to date to do caret
  // positioning.
  mShell->FlushPendingNotifications(FlushType::Layout);

  if (!mShell) {
    return NS_OK;
  }

  // Check that parameters are safe
  if (aDirection < 0 || aDirection > 3 || aAmount < 0 || aAmount > 1) {
    return NS_ERROR_FAILURE;
  }

  nsPresContext *context = mShell->GetPresContext();
  if (!context) {
    return NS_ERROR_FAILURE;
  }

  int8_t index = GetIndexFromSelectionType(SelectionType::eNormal);
  RefPtr<Selection> sel = mDomSelections[index];
  if (!sel) {
    return NS_ERROR_NULL_POINTER;
  }

  // Map the abstract movement amounts (0-1) to direction-specific
  // selection units.
  static const nsSelectionAmount inlineAmount[] =
    { eSelectCluster, eSelectWord };
  static const nsSelectionAmount blockPrevAmount[] =
    { eSelectLine, eSelectBeginLine };
  static const nsSelectionAmount blockNextAmount[] =
    { eSelectLine, eSelectEndLine };

  struct PhysicalToLogicalMapping {
    nsDirection direction;
    const nsSelectionAmount *amounts;
  };
  static const PhysicalToLogicalMapping verticalLR[4] = {
    { eDirPrevious, blockPrevAmount },  // left
    { eDirNext, blockNextAmount },      // right
    { eDirPrevious, inlineAmount }, // up
    { eDirNext, inlineAmount }      // down
  };
  static const PhysicalToLogicalMapping verticalRL[4] = {
    { eDirNext, blockNextAmount },
    { eDirPrevious, blockPrevAmount },
    { eDirPrevious, inlineAmount },
    { eDirNext, inlineAmount }
  };
  static const PhysicalToLogicalMapping horizontal[4] = {
    { eDirPrevious, inlineAmount },
    { eDirNext, inlineAmount },
    { eDirPrevious, blockPrevAmount },
    { eDirNext, blockNextAmount }
  };

  WritingMode wm;
  nsIFrame *frame = nullptr;
  int32_t offsetused = 0;
  if (NS_SUCCEEDED(sel->GetPrimaryFrameForFocusNode(&frame, &offsetused,
                                                    true))) {
    if (frame) {
      if (!frame->StyleContext()->IsTextCombined()) {
        wm = frame->GetWritingMode();
      } else {
        // Using different direction for horizontal-in-vertical would
        // make it hard to navigate via keyboard. Inherit the moving
        // direction from its parent.
        MOZ_ASSERT(frame->GetType() == nsGkAtoms::textFrame);
        wm = frame->GetParent()->GetWritingMode();
        MOZ_ASSERT(wm.IsVertical(), "Text combined "
                   "can only appear in vertical text");
      }
    }
  }

  const PhysicalToLogicalMapping& mapping =
    wm.IsVertical()
      ? wm.IsVerticalLR() ? verticalLR[aDirection] : verticalRL[aDirection]
      : horizontal[aDirection];

  nsresult rv = MoveCaret(mapping.direction, aExtend, mapping.amounts[aAmount],
                          eVisual);
  if (NS_FAILED(rv)) {
    // If we tried to do a line move, but couldn't move in the given direction,
    // then we'll "promote" this to a line-edge move instead.
    if (mapping.amounts[aAmount] == eSelectLine) {
      rv = MoveCaret(mapping.direction, aExtend, mapping.amounts[aAmount + 1],
                     eVisual);
    }
    // And if it was a next-word move that failed (which can happen when
    // eat_space_to_next_word is true, see bug 1153237), then just move forward
    // to the line-edge.
    else if (mapping.amounts[aAmount] == eSelectWord &&
             mapping.direction == eDirNext) {
      rv = MoveCaret(eDirNext, aExtend, eSelectEndLine, eVisual);
    }
  }

  return rv;
}

nsresult
nsFrameSelection::CharacterMove(bool aForward, bool aExtend)
{
  return MoveCaret(aForward ? eDirNext : eDirPrevious, aExtend, eSelectCluster,
                   eUsePrefStyle);
}

nsresult
nsFrameSelection::CharacterExtendForDelete()
{
  return MoveCaret(eDirNext, true, eSelectCluster, eLogical);
}

nsresult
nsFrameSelection::CharacterExtendForBackspace()
{
  return MoveCaret(eDirPrevious, true, eSelectCharacter, eLogical);
}

nsresult
nsFrameSelection::WordMove(bool aForward, bool aExtend)
{
  return MoveCaret(aForward ? eDirNext : eDirPrevious, aExtend, eSelectWord,
                   eUsePrefStyle);
}

nsresult
nsFrameSelection::WordExtendForDelete(bool aForward)
{
  return MoveCaret(aForward ? eDirNext : eDirPrevious, true, eSelectWord,
                   eLogical);
}

nsresult
nsFrameSelection::LineMove(bool aForward, bool aExtend)
{
  return MoveCaret(aForward ? eDirNext : eDirPrevious, aExtend, eSelectLine,
                   eUsePrefStyle);
}

nsresult
nsFrameSelection::IntraLineMove(bool aForward, bool aExtend)
{
  if (aForward) {
    return MoveCaret(eDirNext, aExtend, eSelectEndLine, eLogical);
  } else {
    return MoveCaret(eDirPrevious, aExtend, eSelectBeginLine, eLogical);
  }
}

nsresult
nsFrameSelection::SelectAll()
{
  nsCOMPtr<nsIContent> rootContent;
  if (mLimiter)
  {
    rootContent = mLimiter;//addrefit
  }
  else if (mAncestorLimiter) {
    rootContent = mAncestorLimiter;
  }
  else
  {
    NS_ENSURE_STATE(mShell);
    nsIDocument *doc = mShell->GetDocument();
    if (!doc)
      return NS_ERROR_FAILURE;
    rootContent = doc->GetRootElement();
    if (!rootContent)
      return NS_ERROR_FAILURE;
  }
  int32_t numChildren = rootContent->GetChildCount();
  PostReason(nsISelectionListener::NO_REASON);
  int8_t index = GetIndexFromSelectionType(SelectionType::eNormal);
  AutoPrepareFocusRange prep(mDomSelections[index], false, false);
  return TakeFocus(rootContent, 0, numChildren, CARET_ASSOCIATE_BEFORE, false, false);
}

//////////END FRAMESELECTION

void
nsFrameSelection::StartBatchChanges()
{
  mBatching++;
}

void
nsFrameSelection::EndBatchChanges(int16_t aReason)
{
  mBatching--;
  NS_ASSERTION(mBatching >=0,"Bad mBatching");

  if (mBatching == 0 && mChangesDuringBatching) {
    int16_t postReason = PopReason() | aReason;
    PostReason(postReason);
    mChangesDuringBatching = false;
    // Be aware, the Selection instance may be destroyed after this call.
    NotifySelectionListeners(SelectionType::eNormal);
  }
}


nsresult
nsFrameSelection::NotifySelectionListeners(SelectionType aSelectionType)
{
  int8_t index = GetIndexFromSelectionType(aSelectionType);
  if (index >=0 && mDomSelections[index])
  {
    RefPtr<Selection> selection = mDomSelections[index];
    return selection->NotifySelectionListeners();
  }
  return NS_ERROR_FAILURE;
}

// Start of Table Selection methods

static bool IsCell(nsIContent *aContent)
{
  return aContent->IsAnyOfHTMLElements(nsGkAtoms::td, nsGkAtoms::th);
}

nsITableCellLayout* 
nsFrameSelection::GetCellLayout(nsIContent *aCellContent) const
{
  NS_ENSURE_TRUE(mShell, nullptr);
  nsITableCellLayout *cellLayoutObject =
    do_QueryFrame(aCellContent->GetPrimaryFrame());
  return cellLayoutObject;
}

nsresult
nsFrameSelection::ClearNormalSelection()
{
  int8_t index = GetIndexFromSelectionType(SelectionType::eNormal);
  if (!mDomSelections[index])
    return NS_ERROR_NULL_POINTER;

  return mDomSelections[index]->RemoveAllRanges();
}

static nsIContent*
GetFirstSelectedContent(nsRange* aRange)
{
  if (!aRange) {
    return nullptr;
  }

  NS_PRECONDITION(aRange->GetStartParent(), "Must have start parent!");
  NS_PRECONDITION(aRange->GetStartParent()->IsElement(),
                  "Unexpected parent");

  return aRange->GetStartParent()->GetChildAt(aRange->StartOffset());
}

// Table selection support.
// TODO: Separate table methods into a separate nsITableSelection interface
nsresult
nsFrameSelection::HandleTableSelection(nsINode* aParentContent,
                                       int32_t aContentOffset,
                                       int32_t aTarget,
                                       WidgetMouseEvent* aMouseEvent)
{
  NS_ENSURE_TRUE(aParentContent, NS_ERROR_NULL_POINTER);
  NS_ENSURE_TRUE(aMouseEvent, NS_ERROR_NULL_POINTER);

  if (mDragState && mDragSelectingCells && (aTarget & nsISelectionPrivate::TABLESELECTION_TABLE))
  {
    // We were selecting cells and user drags mouse in table border or inbetween cells,
    //  just do nothing
      return NS_OK;
  }

  nsresult result = NS_OK;

  nsIContent *childContent = aParentContent->GetChildAt(aContentOffset);

  // When doing table selection, always set the direction to next so
  // we can be sure that anchorNode's offset always points to the
  // selected cell
  int8_t index = GetIndexFromSelectionType(SelectionType::eNormal);
  if (!mDomSelections[index])
    return NS_ERROR_NULL_POINTER;

  mDomSelections[index]->SetDirection(eDirNext);

  // Stack-class to wrap all table selection changes in 
  //  BeginBatchChanges() / EndBatchChanges()
  SelectionBatcher selectionBatcher(mDomSelections[index]);

  int32_t startRowIndex, startColIndex, curRowIndex, curColIndex;
  if (mDragState && mDragSelectingCells)
  {
    // We are drag-selecting
    if (aTarget != nsISelectionPrivate::TABLESELECTION_TABLE)
    {
      // If dragging in the same cell as last event, do nothing
      if (mEndSelectedCell == childContent)
        return NS_OK;

#ifdef DEBUG_TABLE_SELECTION
      printf(" mStartSelectedCell = %p, mEndSelectedCell = %p, childContent = %p \n",
             mStartSelectedCell.get(), mEndSelectedCell.get(), childContent);
#endif
      // aTarget can be any "cell mode",
      //  so we can easily drag-select rows and columns 
      // Once we are in row or column mode,
      //  we can drift into any cell to stay in that mode
      //  even if aTarget = TABLESELECTION_CELL

      if (mSelectingTableCellMode == nsISelectionPrivate::TABLESELECTION_ROW ||
          mSelectingTableCellMode == nsISelectionPrivate::TABLESELECTION_COLUMN)
      {
        if (mEndSelectedCell)
        {
          // Also check if cell is in same row/col
          result = GetCellIndexes(mEndSelectedCell, startRowIndex, startColIndex);
          if (NS_FAILED(result)) return result;
          result = GetCellIndexes(childContent, curRowIndex, curColIndex);
          if (NS_FAILED(result)) return result;
        
#ifdef DEBUG_TABLE_SELECTION
printf(" curRowIndex = %d, startRowIndex = %d, curColIndex = %d, startColIndex = %d\n", curRowIndex, startRowIndex, curColIndex, startColIndex);
#endif
          if ((mSelectingTableCellMode == nsISelectionPrivate::TABLESELECTION_ROW && startRowIndex == curRowIndex) ||
              (mSelectingTableCellMode == nsISelectionPrivate::TABLESELECTION_COLUMN && startColIndex == curColIndex)) 
            return NS_OK;
        }
#ifdef DEBUG_TABLE_SELECTION
printf(" Dragged into a new column or row\n");
#endif
        // Continue dragging row or column selection
        return SelectRowOrColumn(childContent, mSelectingTableCellMode);
      }
      else if (mSelectingTableCellMode == nsISelectionPrivate::TABLESELECTION_CELL)
      {
#ifdef DEBUG_TABLE_SELECTION
printf("HandleTableSelection: Dragged into a new cell\n");
#endif
        // Trick for quick selection of rows and columns
        // Hold down shift, then start selecting in one direction
        // If next cell dragged into is in same row, select entire row,
        //   if next cell is in same column, select entire column
        if (mStartSelectedCell && aMouseEvent->IsShift())
        {
          result = GetCellIndexes(mStartSelectedCell, startRowIndex, startColIndex);
          if (NS_FAILED(result)) return result;
          result = GetCellIndexes(childContent, curRowIndex, curColIndex);
          if (NS_FAILED(result)) return result;
          
          if (startRowIndex == curRowIndex || 
              startColIndex == curColIndex)
          {
            // Force new selection block
            mStartSelectedCell = nullptr;
            mDomSelections[index]->RemoveAllRanges();

            if (startRowIndex == curRowIndex)
              mSelectingTableCellMode = nsISelectionPrivate::TABLESELECTION_ROW;
            else
              mSelectingTableCellMode = nsISelectionPrivate::TABLESELECTION_COLUMN;

            return SelectRowOrColumn(childContent, mSelectingTableCellMode);
          }
        }
        
        // Reselect block of cells to new end location
        return SelectBlockOfCells(mStartSelectedCell, childContent);
      }
    }
    // Do nothing if dragging in table, but outside a cell
    return NS_OK;
  }
  else 
  {
    // Not dragging  -- mouse event is down or up
    if (mDragState)
    {
#ifdef DEBUG_TABLE_SELECTION
printf("HandleTableSelection: Mouse down event\n");
#endif
      // Clear cell we stored in mouse-down
      mUnselectCellOnMouseUp = nullptr;
      
      if (aTarget == nsISelectionPrivate::TABLESELECTION_CELL)
      {
        bool isSelected = false;

        // Check if we have other selected cells
        nsIContent* previousCellNode =
          GetFirstSelectedContent(GetFirstCellRange());
        if (previousCellNode)
        {
          // We have at least 1 other selected cell

          // Check if new cell is already selected
          nsIFrame  *cellFrame = childContent->GetPrimaryFrame();
          if (!cellFrame) return NS_ERROR_NULL_POINTER;
          isSelected = cellFrame->IsSelected();
        }
        else
        {
          // No cells selected -- remove non-cell selection
          mDomSelections[index]->RemoveAllRanges();
        }
        mDragSelectingCells = true;    // Signal to start drag-cell-selection
        mSelectingTableCellMode = aTarget;
        // Set start for new drag-selection block (not appended)
        mStartSelectedCell = childContent;
        // The initial block end is same as the start
        mEndSelectedCell = childContent;
        
        if (isSelected)
        {
          // Remember this cell to (possibly) unselect it on mouseup
          mUnselectCellOnMouseUp = childContent;
#ifdef DEBUG_TABLE_SELECTION
printf("HandleTableSelection: Saving mUnselectCellOnMouseUp\n");
#endif
        }
        else
        {
          // Select an unselected cell
          // but first remove existing selection if not in same table
          if (previousCellNode &&
              !IsInSameTable(previousCellNode, childContent))
          {
            mDomSelections[index]->RemoveAllRanges();
            // Reset selection mode that is cleared in RemoveAllRanges
            mSelectingTableCellMode = aTarget;
          }

          return SelectCellElement(childContent);
        }

        return NS_OK;
      }
      else if (aTarget == nsISelectionPrivate::TABLESELECTION_TABLE)
      {
        //TODO: We currently select entire table when clicked between cells,
        //  should we restrict to only around border?
        //  *** How do we get location data for cell and click?
        mDragSelectingCells = false;
        mStartSelectedCell = nullptr;
        mEndSelectedCell = nullptr;

        // Remove existing selection and select the table
        mDomSelections[index]->RemoveAllRanges();
        return CreateAndAddRange(aParentContent, aContentOffset);
      }
      else if (aTarget == nsISelectionPrivate::TABLESELECTION_ROW || aTarget == nsISelectionPrivate::TABLESELECTION_COLUMN)
      {
#ifdef DEBUG_TABLE_SELECTION
printf("aTarget == %d\n", aTarget);
#endif

        // Start drag-selecting mode so multiple rows/cols can be selected
        // Note: Currently, nsFrame::GetDataForTableSelection
        //       will never call us for row or column selection on mouse down
        mDragSelectingCells = true;
      
        // Force new selection block
        mStartSelectedCell = nullptr;
        mDomSelections[index]->RemoveAllRanges();
        // Always do this AFTER RemoveAllRanges
        mSelectingTableCellMode = aTarget;
        return SelectRowOrColumn(childContent, aTarget);
      }
    }
    else
    {
#ifdef DEBUG_TABLE_SELECTION
      printf("HandleTableSelection: Mouse UP event. mDragSelectingCells=%d, mStartSelectedCell=%p\n",
             mDragSelectingCells, mStartSelectedCell.get());
#endif
      // First check if we are extending a block selection
      int32_t rangeCount;
      result = mDomSelections[index]->GetRangeCount(&rangeCount);
      if (NS_FAILED(result)) 
        return result;

      if (rangeCount > 0 && aMouseEvent->IsShift() && 
          mAppendStartSelectedCell && mAppendStartSelectedCell != childContent)
      {
        // Shift key is down: append a block selection
        mDragSelectingCells = false;
        return SelectBlockOfCells(mAppendStartSelectedCell, childContent);
      }

      if (mDragSelectingCells)
        mAppendStartSelectedCell = mStartSelectedCell;
        
      mDragSelectingCells = false;
      mStartSelectedCell = nullptr;
      mEndSelectedCell = nullptr;

      // Any other mouseup actions require that Ctrl or Cmd key is pressed
      //  else stop table selection mode
      bool doMouseUpAction = false;
#ifdef XP_MACOSX
      doMouseUpAction = aMouseEvent->IsMeta();
#else
      doMouseUpAction = aMouseEvent->IsControl();
#endif
      if (!doMouseUpAction)
      {
#ifdef DEBUG_TABLE_SELECTION
        printf("HandleTableSelection: Ending cell selection on mouseup: mAppendStartSelectedCell=%p\n",
               mAppendStartSelectedCell.get());
#endif
        return NS_OK;
      }
      // Unselect a cell only if it wasn't
      //  just selected on mousedown
      if( childContent == mUnselectCellOnMouseUp)
      {
        // Scan ranges to find the cell to unselect (the selection range to remove)
        // XXXbz it's really weird that this lives outside the loop, so once we
        // find one we keep looking at it even if we find no more cells...
        nsINode* previousCellParent = nullptr;
#ifdef DEBUG_TABLE_SELECTION
printf("HandleTableSelection: Unselecting mUnselectCellOnMouseUp; rangeCount=%d\n", rangeCount);
#endif
        for( int32_t i = 0; i < rangeCount; i++)
        {
          // Strong reference, because sometimes we want to remove
          // this range, and then we might be the only owner.
          RefPtr<nsRange> range = mDomSelections[index]->GetRangeAt(i);
          if (!range) return NS_ERROR_NULL_POINTER;

          nsINode* parent = range->GetStartParent();
          if (!parent) return NS_ERROR_NULL_POINTER;

          int32_t offset = range->StartOffset();
          // Be sure previous selection is a table cell
          nsIContent* child = parent->GetChildAt(offset);
          if (child && IsCell(child))
            previousCellParent = parent;

          // We're done if we didn't find parent of a previously-selected cell
          if (!previousCellParent) break;
        
          if (previousCellParent == aParentContent && offset == aContentOffset)
          {
            // Cell is already selected
            if (rangeCount == 1)
            {
#ifdef DEBUG_TABLE_SELECTION
printf("HandleTableSelection: Unselecting single selected cell\n");
#endif
              // This was the only cell selected.
              // Collapse to "normal" selection inside the cell
              mStartSelectedCell = nullptr;
              mEndSelectedCell = nullptr;
              mAppendStartSelectedCell = nullptr;
              //TODO: We need a "Collapse to just before deepest child" routine
              // Even better, should we collapse to just after the LAST deepest child
              //  (i.e., at the end of the cell's contents)?
              return mDomSelections[index]->Collapse(childContent, 0);
            }
#ifdef DEBUG_TABLE_SELECTION
printf("HandleTableSelection: Removing cell from multi-cell selection\n");
#endif
            // Unselecting the start of previous block 
            // XXX What do we use now!
            if (childContent == mAppendStartSelectedCell)
               mAppendStartSelectedCell = nullptr;

            // Deselect cell by removing its range from selection
            return mDomSelections[index]->RemoveRange(range);
          }
        }
        mUnselectCellOnMouseUp = nullptr;
      }
    }
  }
  return result;
}

nsresult
nsFrameSelection::SelectBlockOfCells(nsIContent *aStartCell, nsIContent *aEndCell)
{
  NS_ENSURE_TRUE(aStartCell, NS_ERROR_NULL_POINTER);
  NS_ENSURE_TRUE(aEndCell, NS_ERROR_NULL_POINTER);
  mEndSelectedCell = aEndCell;

  nsresult result = NS_OK;

  // If new end cell is in a different table, do nothing
  nsIContent* table = IsInSameTable(aStartCell, aEndCell);
  if (!table) {
    return NS_OK;
  }

  // Get starting and ending cells' location in the cellmap
  int32_t startRowIndex, startColIndex, endRowIndex, endColIndex;
  result = GetCellIndexes(aStartCell, startRowIndex, startColIndex);
  if(NS_FAILED(result)) return result;
  result = GetCellIndexes(aEndCell, endRowIndex, endColIndex);
  if(NS_FAILED(result)) return result;

  if (mDragSelectingCells)
  {
    // Drag selecting: remove selected cells outside of new block limits
    UnselectCells(table, startRowIndex, startColIndex, endRowIndex, endColIndex,
                  true);
  }

  // Note that we select block in the direction of user's mouse dragging,
  //  which means start cell may be after the end cell in either row or column
  return AddCellsToSelection(table, startRowIndex, startColIndex,
                             endRowIndex, endColIndex);
}

nsresult
nsFrameSelection::UnselectCells(nsIContent *aTableContent,
                                int32_t aStartRowIndex,
                                int32_t aStartColumnIndex,
                                int32_t aEndRowIndex,
                                int32_t aEndColumnIndex,
                                bool aRemoveOutsideOfCellRange)
{
  int8_t index = GetIndexFromSelectionType(SelectionType::eNormal);
  if (!mDomSelections[index])
    return NS_ERROR_NULL_POINTER;

  nsTableWrapperFrame* tableFrame = do_QueryFrame(aTableContent->GetPrimaryFrame());
  if (!tableFrame)
    return NS_ERROR_FAILURE;

  int32_t minRowIndex = std::min(aStartRowIndex, aEndRowIndex);
  int32_t maxRowIndex = std::max(aStartRowIndex, aEndRowIndex);
  int32_t minColIndex = std::min(aStartColumnIndex, aEndColumnIndex);
  int32_t maxColIndex = std::max(aStartColumnIndex, aEndColumnIndex);

  // Strong reference because we sometimes remove the range
  RefPtr<nsRange> range = GetFirstCellRange();
  nsIContent* cellNode = GetFirstSelectedContent(range);
  NS_PRECONDITION(!range || cellNode, "Must have cellNode if had a range");

  int32_t curRowIndex, curColIndex;
  while (cellNode)
  {
    nsresult result = GetCellIndexes(cellNode, curRowIndex, curColIndex);
    if (NS_FAILED(result))
      return result;

#ifdef DEBUG_TABLE_SELECTION
    if (!range)
      printf("RemoveCellsToSelection -- range is null\n");
#endif

    if (range) {
      if (aRemoveOutsideOfCellRange) {
        if (curRowIndex < minRowIndex || curRowIndex > maxRowIndex || 
            curColIndex < minColIndex || curColIndex > maxColIndex) {

          mDomSelections[index]->RemoveRange(range);
          // Since we've removed the range, decrement pointer to next range
          mSelectedCellIndex--;
        }

      } else {
        // Remove cell from selection if it belongs to the given cells range or
        // it is spanned onto the cells range.
        nsTableCellFrame* cellFrame =
          tableFrame->GetCellFrameAt(curRowIndex, curColIndex);

        int32_t origRowIndex, origColIndex;
        cellFrame->GetRowIndex(origRowIndex);
        cellFrame->GetColIndex(origColIndex);
        uint32_t actualRowSpan =
          tableFrame->GetEffectiveRowSpanAt(origRowIndex, origColIndex);
        uint32_t actualColSpan =
          tableFrame->GetEffectiveColSpanAt(curRowIndex, curColIndex);
        if (origRowIndex <= maxRowIndex && maxRowIndex >= 0 &&
            origRowIndex + actualRowSpan - 1 >= static_cast<uint32_t>(minRowIndex) &&
            origColIndex <= maxColIndex && maxColIndex >= 0 &&
            origColIndex + actualColSpan - 1 >= static_cast<uint32_t>(minColIndex)) {

          mDomSelections[index]->RemoveRange(range);
          // Since we've removed the range, decrement pointer to next range
          mSelectedCellIndex--;
        }
      }
    }

    range = GetNextCellRange();
    cellNode = GetFirstSelectedContent(range);
    NS_PRECONDITION(!range || cellNode, "Must have cellNode if had a range");
  }

  return NS_OK;
}

nsresult
nsFrameSelection::AddCellsToSelection(nsIContent *aTableContent,
                                      int32_t aStartRowIndex,
                                      int32_t aStartColumnIndex,
                                      int32_t aEndRowIndex,
                                      int32_t aEndColumnIndex)
{
  int8_t index = GetIndexFromSelectionType(SelectionType::eNormal);
  if (!mDomSelections[index])
    return NS_ERROR_NULL_POINTER;

  nsTableWrapperFrame* tableFrame = do_QueryFrame(aTableContent->GetPrimaryFrame());
  if (!tableFrame) // Check that |table| is a table.
    return NS_ERROR_FAILURE;

  nsresult result = NS_OK;
  int32_t row = aStartRowIndex;
  while(true)
  {
    int32_t col = aStartColumnIndex;
    while(true)
    {
      nsTableCellFrame* cellFrame = tableFrame->GetCellFrameAt(row, col);

      // Skip cells that are spanned from previous locations or are already selected
      if (cellFrame) {
        int32_t origRow, origCol;
        cellFrame->GetRowIndex(origRow);
        cellFrame->GetColIndex(origCol);
        if (origRow == row && origCol == col && !cellFrame->IsSelected()) {
          result = SelectCellElement(cellFrame->GetContent());
          if (NS_FAILED(result)) return result;
        }
      }
      // Done when we reach end column
      if (col == aEndColumnIndex) break;

      if (aStartColumnIndex < aEndColumnIndex)
        col ++;
      else
        col--;
    }
    if (row == aEndRowIndex) break;

    if (aStartRowIndex < aEndRowIndex)
      row++;
    else
      row--;
  }
  return result;
}

nsresult
nsFrameSelection::RemoveCellsFromSelection(nsIContent *aTable,
                                           int32_t aStartRowIndex,
                                           int32_t aStartColumnIndex,
                                           int32_t aEndRowIndex,
                                           int32_t aEndColumnIndex)
{
  return UnselectCells(aTable, aStartRowIndex, aStartColumnIndex,
                       aEndRowIndex, aEndColumnIndex, false);
}

nsresult
nsFrameSelection::RestrictCellsToSelection(nsIContent *aTable,
                                           int32_t aStartRowIndex,
                                           int32_t aStartColumnIndex,
                                           int32_t aEndRowIndex,
                                           int32_t aEndColumnIndex)
{
  return UnselectCells(aTable, aStartRowIndex, aStartColumnIndex,
                       aEndRowIndex, aEndColumnIndex, true);
}

nsresult
nsFrameSelection::SelectRowOrColumn(nsIContent *aCellContent, uint32_t aTarget)
{
  if (!aCellContent) return NS_ERROR_NULL_POINTER;

  nsIContent* table = GetParentTable(aCellContent);
  if (!table) return NS_ERROR_NULL_POINTER;

  // Get table and cell layout interfaces to access
  // cell data based on cellmap location
  // Frames are not ref counted, so don't use an nsCOMPtr
  nsTableWrapperFrame* tableFrame = do_QueryFrame(table->GetPrimaryFrame());
  if (!tableFrame) return NS_ERROR_FAILURE;
  nsITableCellLayout *cellLayout = GetCellLayout(aCellContent);
  if (!cellLayout) return NS_ERROR_FAILURE;

  // Get location of target cell:
  int32_t rowIndex, colIndex;
  nsresult result = cellLayout->GetCellIndexes(rowIndex, colIndex);
  if (NS_FAILED(result)) return result;

  // Be sure we start at proper beginning
  // (This allows us to select row or col given ANY cell!)
  if (aTarget == nsISelectionPrivate::TABLESELECTION_ROW)
    colIndex = 0;
  if (aTarget == nsISelectionPrivate::TABLESELECTION_COLUMN)
    rowIndex = 0;

  nsCOMPtr<nsIContent> firstCell, lastCell;
  while (true) {
    // Loop through all cells in column or row to find first and last
    nsCOMPtr<nsIContent> curCellContent =
      tableFrame->GetCellAt(rowIndex, colIndex);
    if (!curCellContent)
      break;

    if (!firstCell)
      firstCell = curCellContent;

    lastCell = curCellContent.forget();

    // Move to next cell in cellmap, skipping spanned locations
    if (aTarget == nsISelectionPrivate::TABLESELECTION_ROW)
      colIndex += tableFrame->GetEffectiveRowSpanAt(rowIndex, colIndex);
    else
      rowIndex += tableFrame->GetEffectiveRowSpanAt(rowIndex, colIndex);
  }

  // Use SelectBlockOfCells:
  // This will replace existing selection,
  //  but allow unselecting by dragging out of selected region
  if (firstCell && lastCell)
  {
    if (!mStartSelectedCell)
    {
      // We are starting a new block, so select the first cell
      result = SelectCellElement(firstCell);
      if (NS_FAILED(result)) return result;
      mStartSelectedCell = firstCell;
    }
    nsCOMPtr<nsIContent> lastCellContent = do_QueryInterface(lastCell);
    result = SelectBlockOfCells(mStartSelectedCell, lastCellContent);

    // This gets set to the cell at end of row/col, 
    //   but we need it to be the cell under cursor
    mEndSelectedCell = aCellContent;
    return result;
  }

#if 0
// This is a more efficient strategy that appends row to current selection,
//  but doesn't allow dragging OFF of an existing selection to unselect!
  do {
    // Loop through all cells in column or row
    result = tableLayout->GetCellDataAt(rowIndex, colIndex,
                                        getter_AddRefs(cellElement),
                                        curRowIndex, curColIndex,
                                        rowSpan, colSpan,
                                        actualRowSpan, actualColSpan,
                                        isSelected);
    if (NS_FAILED(result)) return result;
    // We're done when cell is not found
    if (!cellElement) break;


    // Check spans else we infinitely loop
    NS_ASSERTION(actualColSpan, "actualColSpan is 0!");
    NS_ASSERTION(actualRowSpan, "actualRowSpan is 0!");
    
    // Skip cells that are already selected or span from outside our region
    if (!isSelected && rowIndex == curRowIndex && colIndex == curColIndex)
    {
      result = SelectCellElement(cellElement);
      if (NS_FAILED(result)) return result;
    }
    // Move to next row or column in cellmap, skipping spanned locations
    if (aTarget == nsISelectionPrivate::TABLESELECTION_ROW)
      colIndex += actualColSpan;
    else
      rowIndex += actualRowSpan;
  }
  while (cellElement);
#endif

  return NS_OK;
}

nsIContent*
nsFrameSelection::GetFirstCellNodeInRange(nsRange *aRange) const
{
  if (!aRange) return nullptr;

  nsINode* startParent = aRange->GetStartParent();
  if (!startParent)
    return nullptr;

  int32_t offset = aRange->StartOffset();

  nsIContent* childContent = startParent->GetChildAt(offset);
  if (!childContent)
    return nullptr;
  // Don't return node if not a cell
  if (!IsCell(childContent))
    return nullptr;

  return childContent;
}

nsRange*
nsFrameSelection::GetFirstCellRange()
{
  int8_t index = GetIndexFromSelectionType(SelectionType::eNormal);
  if (!mDomSelections[index])
    return nullptr;

  nsRange* firstRange = mDomSelections[index]->GetRangeAt(0);
  if (!GetFirstCellNodeInRange(firstRange)) {
    return nullptr;
  }

  // Setup for next cell
  mSelectedCellIndex = 1;

  return firstRange;
}

nsRange*
nsFrameSelection::GetNextCellRange()
{
  int8_t index = GetIndexFromSelectionType(SelectionType::eNormal);
  if (!mDomSelections[index])
    return nullptr;

  nsRange* range = mDomSelections[index]->GetRangeAt(mSelectedCellIndex);

  // Get first node in next range of selection - test if it's a cell
  if (!GetFirstCellNodeInRange(range)) {
    return nullptr;
  }

  // Setup for next cell
  mSelectedCellIndex++;

  return range;
}

nsresult
nsFrameSelection::GetCellIndexes(nsIContent *aCell,
                                 int32_t    &aRowIndex,
                                 int32_t    &aColIndex)
{
  if (!aCell) return NS_ERROR_NULL_POINTER;

  aColIndex=0; // initialize out params
  aRowIndex=0;

  nsITableCellLayout *cellLayoutObject = GetCellLayout(aCell);
  if (!cellLayoutObject)  return NS_ERROR_FAILURE;
  return cellLayoutObject->GetCellIndexes(aRowIndex, aColIndex);
}

nsIContent*
nsFrameSelection::IsInSameTable(nsIContent  *aContent1,
                                nsIContent  *aContent2) const
{
  if (!aContent1 || !aContent2) return nullptr;
  
  nsIContent* tableNode1 = GetParentTable(aContent1);
  nsIContent* tableNode2 = GetParentTable(aContent2);

  // Must be in the same table.  Note that we want to return false for
  // the test if both tables are null.
  return (tableNode1 == tableNode2) ? tableNode1 : nullptr;
}

nsIContent*
nsFrameSelection::GetParentTable(nsIContent *aCell) const
{
  if (!aCell) {
    return nullptr;
  }

  for (nsIContent* parent = aCell->GetParent(); parent;
       parent = parent->GetParent()) {
    if (parent->IsHTMLElement(nsGkAtoms::table)) {
      return parent;
    }
  }

  return nullptr;
}

nsresult
nsFrameSelection::SelectCellElement(nsIContent *aCellElement)
{
  nsIContent *parent = aCellElement->GetParent();

  // Get child offset
  int32_t offset = parent->IndexOf(aCellElement);

  return CreateAndAddRange(parent, offset);
}

nsresult
Selection::getTableCellLocationFromRange(nsRange* aRange,
                                         int32_t* aSelectionType,
                                         int32_t* aRow, int32_t* aCol)
{
  if (!aRange || !aSelectionType || !aRow || !aCol)
    return NS_ERROR_NULL_POINTER;

  *aSelectionType = nsISelectionPrivate::TABLESELECTION_NONE;
  *aRow = 0;
  *aCol = 0;

  // Must have access to frame selection to get cell info
  if (!mFrameSelection) return NS_OK;

  nsresult result = GetTableSelectionType(aRange, aSelectionType);
  if (NS_FAILED(result)) return result;
  
  // Don't fail if range does not point to a single table cell,
  //  let aSelectionType tell user if we don't have a cell
  if (*aSelectionType  != nsISelectionPrivate::TABLESELECTION_CELL)
    return NS_OK;

  // Get the child content (the cell) pointed to by starting node of range
  // We do minimal checking since GetTableSelectionType assures
  //   us that this really is a table cell
  nsCOMPtr<nsIContent> content = do_QueryInterface(aRange->GetStartParent());
  if (!content)
    return NS_ERROR_FAILURE;

  nsIContent *child = content->GetChildAt(aRange->StartOffset());
  if (!child)
    return NS_ERROR_FAILURE;

  //Note: This is a non-ref-counted pointer to the frame
  nsITableCellLayout *cellLayout = mFrameSelection->GetCellLayout(child);
  if (NS_FAILED(result))
    return result;
  if (!cellLayout)
    return NS_ERROR_FAILURE;

  return cellLayout->GetCellIndexes(*aRow, *aCol);
}

nsresult
Selection::addTableCellRange(nsRange* aRange, bool* aDidAddRange,
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
  int32_t newRow, newCol, tableMode;
  result = getTableCellLocationFromRange(aRange, &tableMode, &newRow, &newCol);
  if (NS_FAILED(result)) return result;
  
  // If not adding a cell range, we are done here
  if (tableMode != nsISelectionPrivate::TABLESELECTION_CELL)
  {
    mFrameSelection->mSelectingTableCellMode = tableMode;
    // Don't fail if range isn't a selected cell, aDidAddRange tells caller if we didn't proceed
    return NS_OK;
  }
  
  // Set frame selection mode only if not already set to a table mode
  //  so we don't lose the select row and column flags (not detected by getTableCellLocation)
  if (mFrameSelection->mSelectingTableCellMode == TABLESELECTION_NONE)
    mFrameSelection->mSelectingTableCellMode = tableMode;

  *aDidAddRange = true;
  return AddItem(aRange, aOutIndex);
}

//TODO: Figure out TABLESELECTION_COLUMN and TABLESELECTION_ALLCELLS
nsresult
Selection::GetTableSelectionType(nsIDOMRange* aDOMRange,
                                 int32_t* aTableSelectionType)
{
  if (!aDOMRange || !aTableSelectionType)
    return NS_ERROR_NULL_POINTER;
  nsRange* range = static_cast<nsRange*>(aDOMRange);
  
  *aTableSelectionType = nsISelectionPrivate::TABLESELECTION_NONE;
 
  // Must have access to frame selection to get cell info
  if(!mFrameSelection) return NS_OK;

  nsINode* startNode = range->GetStartParent();
  if (!startNode) return NS_ERROR_FAILURE;
  
  nsINode* endNode = range->GetEndParent();
  if (!endNode) return NS_ERROR_FAILURE;

  // Not a single selected node
  if (startNode != endNode) return NS_OK;

  int32_t startOffset = range->StartOffset();
  int32_t endOffset = range->EndOffset();

  // Not a single selected node
  if ((endOffset - startOffset) != 1)
    return NS_OK;

  nsIContent* startContent = static_cast<nsIContent*>(startNode);
  if (!(startNode->IsElement() && startContent->IsHTMLElement())) {
    // Implies a check for being an element; if we ever make this work
    // for non-HTML, need to keep checking for elements.
    return NS_OK;
  }

  if (startContent->IsHTMLElement(nsGkAtoms::tr))
  {
    *aTableSelectionType = nsISelectionPrivate::TABLESELECTION_CELL;
  }
  else //check to see if we are selecting a table or row (column and all cells not done yet)
  {
    nsIContent *child = startNode->GetChildAt(startOffset);
    if (!child)
      return NS_ERROR_FAILURE;

    if (child->IsHTMLElement(nsGkAtoms::table))
      *aTableSelectionType = nsISelectionPrivate::TABLESELECTION_TABLE;
    else if (child->IsHTMLElement(nsGkAtoms::tr))
      *aTableSelectionType = nsISelectionPrivate::TABLESELECTION_ROW;
  }

  return NS_OK;
}

nsresult
nsFrameSelection::CreateAndAddRange(nsINode *aParentNode, int32_t aOffset)
{
  if (!aParentNode) return NS_ERROR_NULL_POINTER;

  RefPtr<nsRange> range = new nsRange(aParentNode);

  // Set range around child at given offset
  nsresult result = range->SetStart(aParentNode, aOffset);
  if (NS_FAILED(result)) return result;
  result = range->SetEnd(aParentNode, aOffset+1);
  if (NS_FAILED(result)) return result;
  
  int8_t index = GetIndexFromSelectionType(SelectionType::eNormal);
  if (!mDomSelections[index])
    return NS_ERROR_NULL_POINTER;

  return mDomSelections[index]->AddRange(range);
}

// End of Table Selection

void
nsFrameSelection::SetAncestorLimiter(nsIContent *aLimiter)
{
  if (mAncestorLimiter != aLimiter) {
    mAncestorLimiter = aLimiter;
    int8_t index = GetIndexFromSelectionType(SelectionType::eNormal);
    if (!mDomSelections[index])
      return;

    if (!IsValidSelectionPoint(this, mDomSelections[index]->GetFocusNode())) {
      ClearNormalSelection();
      if (mAncestorLimiter) {
        PostReason(nsISelectionListener::NO_REASON);
        TakeFocus(mAncestorLimiter, 0, 0, CARET_ASSOCIATE_BEFORE, false, false);
      }
    }
  }
}

//END nsFrameSelection methods


//BEGIN nsISelection interface implementations



nsresult
nsFrameSelection::DeleteFromDocument()
{
  nsresult res;

  // If we're already collapsed, then we do nothing (bug 719503).
  bool isCollapsed;
  int8_t index = GetIndexFromSelectionType(SelectionType::eNormal);
  if (!mDomSelections[index])
    return NS_ERROR_NULL_POINTER;

  mDomSelections[index]->GetIsCollapsed( &isCollapsed);
  if (isCollapsed)
  {
    return NS_OK;
  }

  RefPtr<Selection> selection = mDomSelections[index];
  for (uint32_t rangeIdx = 0; rangeIdx < selection->RangeCount(); ++rangeIdx) {
    RefPtr<nsRange> range = selection->GetRangeAt(rangeIdx);
    res = range->DeleteContents();
    if (NS_FAILED(res))
      return res;
  }

  // Collapse to the new location.
  // If we deleted one character, then we move back one element.
  // FIXME  We don't know how to do this past frame boundaries yet.
  if (isCollapsed)
    mDomSelections[index]->Collapse(mDomSelections[index]->GetAnchorNode(), mDomSelections[index]->AnchorOffset()-1);
  else if (mDomSelections[index]->AnchorOffset() > 0)
    mDomSelections[index]->Collapse(mDomSelections[index]->GetAnchorNode(), mDomSelections[index]->AnchorOffset());
#ifdef DEBUG
  else
    printf("Don't know how to set selection back past frame boundary\n");
#endif

  return NS_OK;
}

void
nsFrameSelection::SetDelayedCaretData(WidgetMouseEvent* aMouseEvent)
{
  if (aMouseEvent) {
    mDelayedMouseEventValid = true;
    mDelayedMouseEventIsShift = aMouseEvent->IsShift();
    mDelayedMouseEventClickCount = aMouseEvent->mClickCount;
  } else {
    mDelayedMouseEventValid = false;
  }
}

void
nsFrameSelection::DisconnectFromPresShell()
{
  RefPtr<AccessibleCaretEventHub> eventHub = mShell->GetAccessibleCaretEventHub();
  if (eventHub) {
    int8_t index = GetIndexFromSelectionType(SelectionType::eNormal);
    mDomSelections[index]->RemoveSelectionListener(eventHub);
  }

  StopAutoScrollTimer();
  for (size_t i = 0; i < kPresentSelectionTypeCount; i++) {
    mDomSelections[i]->Clear(nullptr);
  }
  mShell = nullptr;
}

//END nsISelection interface implementations

#if 0
#pragma mark -
#endif

// mozilla::dom::Selection implementation

// note: this can return a nil anchor node

Selection::Selection()
  : mCachedOffsetForFrame(nullptr)
  , mDirection(eDirNext)
  , mSelectionType(SelectionType::eNormal)
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
  , mUserInitiated(false)
  , mCalledByJS(false)
  , mSelectionChangeBlockerCount(0)
{
}

Selection::~Selection()
{
  setAnchorFocusRange(-1);

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

NS_IMPL_CYCLE_COLLECTION_CLASS(Selection)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(Selection)
  // Unlink the selection listeners *before* we do RemoveAllRanges since
  // we don't want to notify the listeners during JS GC (they could be
  // in JS!).
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mSelectionListeners)
  tmp->RemoveAllRanges();
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
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mFrameSelection)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSelectionListeners)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END
NS_IMPL_CYCLE_COLLECTION_TRACE_WRAPPERCACHE(Selection)

// QueryInterface implementation for Selection
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Selection)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISelection)
  NS_INTERFACE_MAP_ENTRY(nsISelectionPrivate)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsISelection)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(Selection)
NS_IMPL_CYCLE_COLLECTING_RELEASE(Selection)


NS_IMETHODIMP
Selection::GetAnchorNode(nsIDOMNode** aAnchorNode)
{
  nsINode* anchorNode = GetAnchorNode();
  if (anchorNode) {
    return CallQueryInterface(anchorNode, aAnchorNode);
  }

  *aAnchorNode = nullptr;
  return NS_OK;
}

nsINode*
Selection::GetAnchorNode()
{
  if (!mAnchorFocusRange)
    return nullptr;
   
  if (GetDirection() == eDirNext) {
    return mAnchorFocusRange->GetStartParent();
  }

  return mAnchorFocusRange->GetEndParent();
}

NS_IMETHODIMP
Selection::GetAnchorOffset(int32_t* aAnchorOffset)
{
  *aAnchorOffset = static_cast<int32_t>(AnchorOffset());
  return NS_OK;
}

// note: this can return a nil focus node
NS_IMETHODIMP
Selection::GetFocusNode(nsIDOMNode** aFocusNode)
{
  nsINode* focusNode = GetFocusNode();
  if (focusNode) {
    return CallQueryInterface(focusNode, aFocusNode);
  }

  *aFocusNode = nullptr;
  return NS_OK;
}

nsINode*
Selection::GetFocusNode()
{
  if (!mAnchorFocusRange)
    return nullptr;

  if (GetDirection() == eDirNext){
    return mAnchorFocusRange->GetEndParent();
  }

  return mAnchorFocusRange->GetStartParent();
}

NS_IMETHODIMP
Selection::GetFocusOffset(int32_t* aFocusOffset)
{
  *aFocusOffset = static_cast<int32_t>(FocusOffset());
  return NS_OK;
}

void
Selection::setAnchorFocusRange(int32_t indx)
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

uint32_t
Selection::AnchorOffset()
{
  if (!mAnchorFocusRange)
    return 0;

  if (GetDirection() == eDirNext){
    return mAnchorFocusRange->StartOffset();
  }

  return mAnchorFocusRange->EndOffset();
}

uint32_t
Selection::FocusOffset()
{
  if (!mAnchorFocusRange)
    return 0;

  if (GetDirection() == eDirNext){
    return mAnchorFocusRange->EndOffset();
  }

  return mAnchorFocusRange->StartOffset();
}

static nsresult
CompareToRangeStart(nsINode* aCompareNode, int32_t aCompareOffset,
                    nsRange* aRange, int32_t* aCmp)
{
  nsINode* start = aRange->GetStartParent();
  NS_ENSURE_STATE(aCompareNode && start);
  // If the nodes that we're comparing are not in the same document,
  // assume that aCompareNode will fall at the end of the ranges.
  if (aCompareNode->GetComposedDoc() != start->GetComposedDoc() ||
      !start->GetComposedDoc()) {
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
  nsINode* end = aRange->GetEndParent();
  NS_ENSURE_STATE(aCompareNode && end);
  // If the nodes that we're comparing are not in the same document,
  // assume that aCompareNode will fall at the end of the ranges.
  if (aCompareNode->GetComposedDoc() != end->GetComposedDoc() ||
      !end->GetComposedDoc()) {
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
  nsresult rv = CompareToRangeStart(range->GetStartParent(),
                                    range->StartOffset(),
                                    aSubtract, &cmp);
  NS_ENSURE_SUCCESS(rv, rv);

  // Also, make a comparison to the range end
  int32_t cmp2;
  rv = CompareToRangeEnd(range->GetEndParent(),
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
    RefPtr<nsRange> postOverlap = new nsRange(aSubtract->GetEndParent());

    rv =
      postOverlap->SetStart(aSubtract->GetEndParent(), aSubtract->EndOffset());
    NS_ENSURE_SUCCESS(rv, rv);
    rv =
     postOverlap->SetEnd(range->GetEndParent(), range->EndOffset());
    NS_ENSURE_SUCCESS(rv, rv);
    if (!postOverlap->Collapsed()) {
      if (!aOutput->InsertElementAt(0, RangeData(postOverlap)))
        return NS_ERROR_OUT_OF_MEMORY;
      (*aOutput)[0].mTextRangeStyle = aRange->mTextRangeStyle;
    }
  }

  if (cmp < 0) {
    // We need to add a new RangeData to the output, running from
    // the start of the range to the start of aSubtract
    RefPtr<nsRange> preOverlap = new nsRange(range->GetStartParent());

    nsresult rv =
     preOverlap->SetStart(range->GetStartParent(), range->StartOffset());
    NS_ENSURE_SUCCESS(rv, rv);
    rv =
     preOverlap->SetEnd(aSubtract->GetStartParent(), aSubtract->StartOffset());
    NS_ENSURE_SUCCESS(rv, rv);
    
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
    *aOutIndex = -1;

    nsIDocument* doc = GetParentObject();
    bool selectEventsEnabled =
      nsFrameSelection::sSelectionEventsEnabled ||
      (doc && nsContentUtils::IsSystemPrincipal(doc->NodePrincipal()));

    if (!aNoStartSelect &&
        mSelectionType == SelectionType::eNormal &&
        selectEventsEnabled && Collapsed() &&
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
        nsCOMPtr<nsINode> target = aItem->GetStartParent();
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
  nsresult rv = GetIndicesForInterval(aItem->GetStartParent(),
                                      aItem->StartOffset(),
                                      aItem->GetEndParent(),
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
  bool sameRange = EqualsRangeAtPoint(aItem->GetStartParent(),
                                        aItem->StartOffset(),
                                        aItem->GetEndParent(),
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
  rv = FindInsertionPoint(&temp, aItem->GetStartParent(),
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
    return NS_ERROR_INVALID_ARG;

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
  setAnchorFocusRange(-1);

  for (uint32_t i = 0; i < mRanges.Length(); ++i) {
    mRanges[i].mRange->SetSelection(nullptr);
    selectFrames(aPresContext, mRanges[i].mRange, false);
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

NS_IMETHODIMP
Selection::GetType(int16_t* aType)
{
  NS_ENSURE_ARG_POINTER(aType);
  *aType = ToRawSelectionType(Type());
  return NS_OK;
}

// RangeMatches*Point
//
//    Compares the range beginning or ending point, and returns true if it
//    exactly matches the given DOM point.

static inline bool
RangeMatchesBeginPoint(nsRange* aRange, nsINode* aNode, int32_t aOffset)
{
  return aRange->GetStartParent() == aNode && aRange->StartOffset() == aOffset;
}

static inline bool
RangeMatchesEndPoint(nsRange* aRange, nsINode* aNode, int32_t aOffset)
{
  return aRange->GetEndParent() == aNode && aRange->EndOffset() == aOffset;
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

// Selection::GetRangesForInterval
//
//    XPCOM wrapper for the nsTArray version

NS_IMETHODIMP
Selection::GetRangesForInterval(nsIDOMNode* aBeginNode, int32_t aBeginOffset,
                                nsIDOMNode* aEndNode, int32_t aEndOffset,
                                bool aAllowAdjacent,
                                uint32_t* aResultCount,
                                nsIDOMRange*** aResults)
{
  if (!aBeginNode || ! aEndNode || ! aResultCount || ! aResults)
    return NS_ERROR_NULL_POINTER;

  *aResultCount = 0;
  *aResults = nullptr;

  nsTArray<RefPtr<nsRange>> results;
  ErrorResult result;
  nsCOMPtr<nsINode> beginNode = do_QueryInterface(aBeginNode);
  nsCOMPtr<nsINode> endNode = do_QueryInterface(aEndNode);
  NS_ENSURE_TRUE(beginNode && endNode, NS_ERROR_NULL_POINTER);
  GetRangesForInterval(*beginNode, aBeginOffset, *endNode, aEndOffset,
                       aAllowAdjacent, results, result);
  if (result.Failed()) {
    return result.StealNSResult();
  }
  *aResultCount = results.Length();
  if (*aResultCount == 0) {
    return NS_OK;
  }

  *aResults = static_cast<nsIDOMRange**>
                         (moz_xmalloc(sizeof(nsIDOMRange*) * *aResultCount));
  NS_ENSURE_TRUE(*aResults, NS_ERROR_OUT_OF_MEMORY);

  for (uint32_t i = 0; i < *aResultCount; i++) {
    (*aResults)[i] = results[i].forget().take();
  }
  return NS_OK;
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

NS_IMETHODIMP
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

NS_IMETHODIMP
Selection::GetPrimaryFrameForFocusNode(nsIFrame** aReturnFrame,
                                       int32_t* aOffsetUsed,
                                       bool aVisual)
{
  if (!aReturnFrame)
    return NS_ERROR_NULL_POINTER;
  
  nsCOMPtr<nsIContent> content = do_QueryInterface(GetFocusNode());
  if (!content || !mFrameSelection)
    return NS_ERROR_FAILURE;
  
  int32_t frameOffset = 0;
  *aReturnFrame = 0;
  if (!aOffsetUsed)
    aOffsetUsed = &frameOffset;
    
  CaretAssociationHint hint = mFrameSelection->GetHint();

  if (aVisual) {
    nsBidiLevel caretBidiLevel = mFrameSelection->GetCaretBidiLevel();

    return nsCaret::GetCaretFrameForNodeOffset(mFrameSelection,
      content, FocusOffset(), hint, caretBidiLevel, aReturnFrame, aOffsetUsed);
  }
  
  *aReturnFrame = mFrameSelection->
    GetFrameForNodeOffset(content, FocusOffset(),
                          hint, aOffsetUsed);
  if (!*aReturnFrame)
    return NS_ERROR_FAILURE;

  return NS_OK;
}

//select all content children of aContent
nsresult
Selection::SelectAllFramesForContent(nsIContentIterator* aInnerIter,
                                     nsIContent* aContent,
                                     bool aSelected)
{
  nsresult result = aInnerIter->Init(aContent);
  nsIFrame *frame;
  if (NS_SUCCEEDED(result))
  {
    // First select frame of content passed in
    frame = aContent->GetPrimaryFrame();
    if (frame && frame->GetType() == nsGkAtoms::textFrame) {
      nsTextFrame* textFrame = static_cast<nsTextFrame*>(frame);
      textFrame->SetSelectedRange(0, aContent->GetText()->GetLength(),
                                  aSelected, mSelectionType);
    }
    // Now iterated through the child frames and set them
    while (!aInnerIter->IsDone()) {
      nsCOMPtr<nsIContent> innercontent =
        do_QueryInterface(aInnerIter->GetCurrentNode());

      frame = innercontent->GetPrimaryFrame();
      if (frame) {
        if (frame->GetType() == nsGkAtoms::textFrame) {
          nsTextFrame* textFrame = static_cast<nsTextFrame*>(frame);
          textFrame->SetSelectedRange(0, innercontent->GetText()->GetLength(),
                                      aSelected, mSelectionType);
        } else {
          frame->InvalidateFrameSubtree();  // frame continuations?
        }
      }

      aInnerIter->Next();
    }

    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

/**
 * The idea of this helper method is to select or deselect "top to bottom",
 * traversing through the frames
 */
nsresult
Selection::selectFrames(nsPresContext* aPresContext, nsRange* aRange,
                        bool aSelect)
{
  if (!mFrameSelection || !aPresContext || !aPresContext->GetPresShell()) {
    // nothing to do
    return NS_OK;
  }
  MOZ_ASSERT(aRange);

  if (mFrameSelection->GetTableCellSelection()) {
    nsINode* node = aRange->GetCommonAncestor();
    nsIFrame* frame = node->IsContent() ? node->AsContent()->GetPrimaryFrame()
                                : aPresContext->FrameManager()->GetRootFrame();
    if (frame) {
      frame->InvalidateFrameSubtree();
    }
    return NS_OK;
  }

  nsCOMPtr<nsIContentIterator> iter = NS_NewContentSubtreeIterator();
  iter->Init(aRange);

  // Loop through the content iterator for each content node; for each text
  // node, call SetSelected on it:
  nsCOMPtr<nsIContent> content = do_QueryInterface(aRange->GetStartParent());
  if (!content) {
    // Don't warn, bug 1055722
    return NS_ERROR_UNEXPECTED;
  }

  // We must call first one explicitly
  if (content->IsNodeOfType(nsINode::eTEXT)) {
    nsIFrame* frame = content->GetPrimaryFrame();
    // The frame could be an SVG text frame, in which case we'll ignore it.
    if (frame && frame->GetType() == nsGkAtoms::textFrame) {
      nsTextFrame* textFrame = static_cast<nsTextFrame*>(frame);
      uint32_t startOffset = aRange->StartOffset();
      uint32_t endOffset;
      if (aRange->GetEndParent() == content) {
        endOffset = aRange->EndOffset();
      } else {
        endOffset = content->Length();
      }
      textFrame->SetSelectedRange(startOffset, endOffset, aSelect,
                                  mSelectionType);
    }
  }

  iter->First();
  nsCOMPtr<nsIContentIterator> inneriter = NS_NewContentIterator();
  for (iter->First(); !iter->IsDone(); iter->Next()) {
    content = do_QueryInterface(iter->GetCurrentNode());
    SelectAllFramesForContent(inneriter, content, aSelect);
  }

  // We must now do the last one if it is not the same as the first
  if (aRange->GetEndParent() != aRange->GetStartParent()) {
    nsresult res;
    content = do_QueryInterface(aRange->GetEndParent(), &res);
    NS_ENSURE_SUCCESS(res, res);
    NS_ENSURE_TRUE(content, res);

    if (content->IsNodeOfType(nsINode::eTEXT)) {
      nsIFrame* frame = content->GetPrimaryFrame();
      // The frame could be an SVG text frame, in which case we'll ignore it.
      if (frame && frame->GetType() == nsGkAtoms::textFrame) {
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

  UniquePtr<SelectionDetails> detailsHead = Move(aDetailsHead);

  for (uint32_t i = 0; i < overlappingRanges.Length(); i++) {
    nsRange* range = overlappingRanges[i];
    nsINode* startNode = range->GetStartParent();
    nsINode* endNode = range->GetEndParent();
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

    newHead->mNext = Move(detailsHead);
    newHead->mStart = start;
    newHead->mEnd = end;
    newHead->mSelectionType = aSelectionType;
    RangeData *rd = FindRangeData(range);
    if (rd) {
      newHead->mTextRangeStyle = rd->mTextRangeStyle;
    }
    detailsHead = Move(newHead);
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
    nsresult rv = selectFrames(aPresContext, mRanges[i].mRange, true);

    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
Selection::GetCanCacheFrameOffset(bool* aCanCacheFrameOffset)
{ 
  NS_ENSURE_ARG_POINTER(aCanCacheFrameOffset);

  if (mCachedOffsetForFrame)
    *aCanCacheFrameOffset = mCachedOffsetForFrame->mCanCacheFrameOffset;
  else
    *aCanCacheFrameOffset = false;

  return NS_OK;
}

NS_IMETHODIMP    
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

  return NS_OK;
}

NS_IMETHODIMP    
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

NS_IMETHODIMP
Selection::GetAncestorLimiter(nsIContent** aContent)
{
  if (mFrameSelection) {
    nsCOMPtr<nsIContent> c = mFrameSelection->GetAncestorLimiter();
    c.forget(aContent);
  }
  return NS_OK;
}

NS_IMETHODIMP
Selection::SetAncestorLimiter(nsIContent* aContent)
{
  if (mFrameSelection) {
    RefPtr<nsFrameSelection> frameSelection = mFrameSelection;
    frameSelection->SetAncestorLimiter(aContent);
  }
  return NS_OK;
}

RangeData*
Selection::FindRangeData(nsIDOMRange* aRange)
{
  NS_ENSURE_TRUE(aRange, nullptr);
  for (uint32_t i = 0; i < mRanges.Length(); i++) {
    if (mRanges[i].mRange == aRange)
      return &mRanges[i];
  }
  return nullptr;
}

NS_IMETHODIMP
Selection::SetTextRangeStyle(nsIDOMRange* aRange,
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
Selection::StartAutoScrollTimer(nsIFrame* aFrame, nsPoint& aPoint,
                                uint32_t aDelay)
{
  NS_PRECONDITION(aFrame, "Need a frame");

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
Selection::DoAutoScroll(nsIFrame* aFrame, nsPoint& aPoint)
{
  NS_PRECONDITION(aFrame, "Need a frame");

  if (mAutoScrollTimer) {
    (void)mAutoScrollTimer->Stop();
  }

  nsPresContext* presContext = aFrame->PresContext();
  nsCOMPtr<nsIPresShell> shell = presContext->PresShell();
  nsRootPresContext* rootPC = presContext->GetRootPresContext();
  if (!rootPC)
    return NS_OK;
  nsIFrame* rootmostFrame = rootPC->PresShell()->FrameManager()->GetRootFrame();
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
      shell->FrameManager()->GetRootFrame()->GetOffsetToCrossDoc(rootmostFrame);
    mAutoScrollTimer->Start(presContext, presContextPoint);
  }

  return NS_OK;
}


/** RemoveAllRanges zeroes the selection
 */
NS_IMETHODIMP
Selection::RemoveAllRanges()
{
  ErrorResult result;
  RemoveAllRanges(result);
  return result.StealNSResult();
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

/** AddRange adds the specified range to the selection
 *  @param aRange is the range to be added
 */
NS_IMETHODIMP
Selection::AddRange(nsIDOMRange* aDOMRange)
{
  if (!aDOMRange) {
    return NS_ERROR_NULL_POINTER;
  }
  nsRange* range = static_cast<nsRange*>(aDOMRange);
  ErrorResult result;
  AddRange(*range, result);
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

  // This inserts a table cell range in proper document order
  // and returns NS_OK if range doesn't contain just one table cell
  bool didAddRange;
  int32_t rangeIndex;
  nsresult result = addTableCellRange(&aRange, &didAddRange, &rangeIndex);
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

  setAnchorFocusRange(rangeIndex);
  
  // Make sure the caret appears on the next line, if at a newline
  if (mSelectionType == SelectionType::eNormal) {
    SetInterlinePosition(true);
  }

  RefPtr<nsPresContext>  presContext = GetPresContext();
  selectFrames(presContext, &aRange, true);

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

nsresult
Selection::RemoveRange(nsIDOMRange* aDOMRange)
{
  if (!aDOMRange) {
    return NS_ERROR_INVALID_ARG;
  }
  nsRange* range = static_cast<nsRange*>(aDOMRange);
  ErrorResult result;
  RemoveRange(*range, result);
  return result.StealNSResult();
}

void
Selection::RemoveRange(nsRange& aRange, ErrorResult& aRv)
{
  nsresult rv = RemoveItem(&aRange);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return;
  }

  nsINode* beginNode = aRange.GetStartParent();
  nsINode* endNode = aRange.GetEndParent();

  if (!beginNode || !endNode) {
    // Detached range; nothing else to do here.
    return;
  }
  
  // find out the length of the end node, so we can select all of it
  int32_t beginOffset, endOffset;
  if (endNode->IsNodeOfType(nsINode::eTEXT)) {
    // Get the length of the text. We can't just use the offset because
    // another range could be touching this text node but not intersect our
    // range.
    beginOffset = 0;
    endOffset = static_cast<nsIContent*>(endNode)->TextLength();
  } else {
    // For non-text nodes, the given offsets should be sufficient.
    beginOffset = aRange.StartOffset();
    endOffset = aRange.EndOffset();
  }

  // clear the selected bit from the removed range's frames
  RefPtr<nsPresContext>  presContext = GetPresContext();
  selectFrames(presContext, &aRange, false);

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
    selectFrames(presContext, affectedRanges[i], true);
  }

  int32_t cnt = mRanges.Length();
  if (&aRange == mAnchorFocusRange) {
    // Reset anchor to LAST range or clear it if there are no ranges.
    setAnchorFocusRange(cnt - 1);

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
NS_IMETHODIMP
Selection::Collapse(nsIDOMNode* aParentNode, int32_t aOffset)
{
  nsCOMPtr<nsINode> parentNode = do_QueryInterface(aParentNode);
  return Collapse(parentNode, aOffset);
}

NS_IMETHODIMP
Selection::CollapseNative(nsINode* aParentNode, int32_t aOffset)
{
  return Collapse(aParentNode, aOffset);
}

void
Selection::CollapseJS(nsINode* aNode, uint32_t aOffset, ErrorResult& aRv)
{
  AutoRestore<bool> calledFromJSRestorer(mCalledByJS);
  mCalledByJS = true;
  if (!aNode) {
    RemoveAllRanges(aRv);
    return;
  }
  Collapse(*aNode, aOffset, aRv);
}

nsresult
Selection::Collapse(nsINode* aParentNode, int32_t aOffset)
{
  if (!aParentNode)
    return NS_ERROR_INVALID_ARG;

  ErrorResult result;
  Collapse(*aParentNode, static_cast<uint32_t>(aOffset), result);
  return result.StealNSResult();
}

void
Selection::Collapse(nsINode& aParentNode, uint32_t aOffset, ErrorResult& aRv)
{
  if (!mFrameSelection) {
    aRv.Throw(NS_ERROR_NOT_INITIALIZED); // Can't do selection
    return;
  }

  nsCOMPtr<nsINode> parentNode = &aParentNode;

  RefPtr<nsFrameSelection> frameSelection = mFrameSelection;
  frameSelection->InvalidateDesiredPos();
  if (!IsValidSelectionPoint(frameSelection, parentNode)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }
  nsresult result;

  RefPtr<nsPresContext> presContext = GetPresContext();
  if (!presContext || presContext->Document() != parentNode->OwnerDoc()) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  // Delete all of the current ranges
  Clear(presContext);

  // Turn off signal for table selection
  frameSelection->ClearTableCellSelection();

  // Hack to display the caret on the right line (bug 1237236).
  if (frameSelection->GetHint() != CARET_ASSOCIATE_AFTER &&
      parentNode->IsContent()) {
    int32_t frameOffset;
    nsTextFrame* f =
      do_QueryFrame(nsCaret::GetFrameAndOffset(this, parentNode,
                                               aOffset, &frameOffset));
    if (f && f->IsAtEndOfLine() && f->HasSignificantTerminalNewline()) {
      if ((parentNode->AsContent() == f->GetContent() &&
           f->GetContentEnd() == int32_t(aOffset)) ||
          (parentNode == f->GetContent()->GetParentNode() &&
           parentNode->IndexOf(f->GetContent()) + 1 == int32_t(aOffset))) {
        frameSelection->SetHint(CARET_ASSOCIATE_AFTER);
      }
    }
  }

  RefPtr<nsRange> range = new nsRange(parentNode);
  result = range->SetEnd(parentNode, aOffset);
  if (NS_FAILED(result)) {
    aRv.Throw(result);
    return;
  }
  result = range->SetStart(parentNode, aOffset);
  if (NS_FAILED(result)) {
    aRv.Throw(result);
    return;
  }

#ifdef DEBUG_SELECTION
  nsCOMPtr<nsIContent> content = do_QueryInterface(parentNode);
  nsCOMPtr<nsIDocument> doc = do_QueryInterface(parentNode);
  printf ("Sel. Collapse to %p %s %d\n", parentNode.get(),
          content ? nsAtomCString(content->NodeInfo()->NameAtom()).get()
                  : (doc ? "DOCUMENT" : "???"),
          aOffset);
#endif

  int32_t rangeIndex = -1;
  result = AddItem(range, &rangeIndex);
  if (NS_FAILED(result)) {
    aRv.Throw(result);
    return;
  }
  setAnchorFocusRange(0);
  selectFrames(presContext, range, true);

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
NS_IMETHODIMP
Selection::CollapseToStart()
{
  ErrorResult result;
  CollapseToStart(result);
  return result.StealNSResult();
}

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
  int32_t cnt;
  nsresult rv = GetRangeCount(&cnt);
  if (NS_FAILED(rv) || cnt <= 0) {
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
  nsINode* parent = firstRange->GetStartParent();
  if (!parent) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }
  Collapse(*parent, firstRange->StartOffset(), aRv);
}

/*
 * Sets the whole selection to be one point
 * at the end of the current selection
 */
NS_IMETHODIMP
Selection::CollapseToEnd()
{
  ErrorResult result;
  CollapseToEnd(result);
  return result.StealNSResult();
}

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
  int32_t cnt;
  nsresult rv = GetRangeCount(&cnt);
  if (NS_FAILED(rv) || cnt <= 0) {
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
  nsINode* parent = lastRange->GetEndParent();
  if (!parent) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }
  Collapse(*parent, lastRange->EndOffset(), aRv);
}

/*
 * IsCollapsed -- is the whole selection just one point, or unset?
 */
bool
Selection::IsCollapsed() const
{
  uint32_t cnt = mRanges.Length();
  if (cnt == 0) {
    return true;
  }

  if (cnt != 1) {
    return false;
  }

  return mRanges[0].mRange->Collapsed();
}

/* virtual */
bool
Selection::Collapsed()
{
  return IsCollapsed();
}

NS_IMETHODIMP
Selection::GetIsCollapsed(bool* aIsCollapsed)
{
  NS_ENSURE_TRUE(aIsCollapsed, NS_ERROR_NULL_POINTER);

  *aIsCollapsed = IsCollapsed();
  return NS_OK;
}

NS_IMETHODIMP
Selection::GetRangeCount(int32_t* aRangeCount)
{
  *aRangeCount = (int32_t)RangeCount();

  return NS_OK;
}

NS_IMETHODIMP
Selection::GetRangeAt(int32_t aIndex, nsIDOMRange** aReturn)
{
  ErrorResult result;
  *aReturn = GetRangeAt(aIndex, result);
  NS_IF_ADDREF(*aReturn);
  return result.StealNSResult();
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

  bool collapsed = Collapsed();

  nsresult res = RemoveItem(mAnchorFocusRange);
  if (NS_FAILED(res))
    return res;

  int32_t aOutIndex = -1;
  res = AddItem(aRange, &aOutIndex, !collapsed);
  if (NS_FAILED(res))
    return res;
  setAnchorFocusRange(aOutIndex);

  return NS_OK;
}

void
Selection::ReplaceAnchorFocusRange(nsRange* aRange)
{
  NS_ENSURE_TRUE_VOID(mAnchorFocusRange);
  RefPtr<nsPresContext> presContext = GetPresContext();
  if (presContext) {
    selectFrames(presContext, mAnchorFocusRange, false);
    SetAnchorFocusToRange(aRange);
    selectFrames(presContext, mAnchorFocusRange, true);
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
    setAnchorFocusRange(0);
  } else { // aDir == eDirNext
    firstRange->SetIsGenerated(true);
    lastRange->SetIsGenerated(false);
    setAnchorFocusRange(RangeCount() - 1);
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
NS_IMETHODIMP
Selection::Extend(nsIDOMNode* aParentNode, int32_t aOffset)
{
  nsCOMPtr<nsINode> parentNode = do_QueryInterface(aParentNode);
  return Extend(parentNode, aOffset);
}

NS_IMETHODIMP
Selection::ExtendNative(nsINode* aParentNode, int32_t aOffset)
{
  return Extend(aParentNode, aOffset);
}

void
Selection::ExtendJS(nsINode& aNode, uint32_t aOffset, ErrorResult& aRv)
{
  AutoRestore<bool> calledFromJSRestorer(mCalledByJS);
  mCalledByJS = true;
  Extend(aNode, aOffset, aRv);
}

nsresult
Selection::Extend(nsINode* aParentNode, int32_t aOffset)
{
  if (!aParentNode)
    return NS_ERROR_INVALID_ARG;

  ErrorResult result;
  Extend(*aParentNode, static_cast<uint32_t>(aOffset), result);
  return result.StealNSResult();
}

void
Selection::Extend(nsINode& aParentNode, uint32_t aOffset, ErrorResult& aRv)
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

  nsresult res;
  if (!IsValidSelectionPoint(mFrameSelection, &aParentNode)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  RefPtr<nsPresContext> presContext = GetPresContext();
  if (!presContext || presContext->Document() != aParentNode.OwnerDoc()) {
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

  nsINode* startNode = range->GetStartParent();
  nsINode* endNode = range->GetEndParent();
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
                                                  &aParentNode, aOffset,
                                                  &disconnected);
  //compare anchor to new cursor
  shouldClearRange |= disconnected;
  int32_t result3 = nsContentUtils::ComparePoints(anchorNode, anchorOffset,
                                                  &aParentNode, aOffset,
                                                  &disconnected);

  // If the points are disconnected, the range will be collapsed below,
  // resulting in a range that selects nothing.
  if (shouldClearRange) {
    // Repaint the current range with the selection removed.
    selectFrames(presContext, range, false);
  }

  RefPtr<nsRange> difRange = new nsRange(&aParentNode);
  if ((result1 == 0 && result3 < 0) || (result1 <= 0 && result2 < 0)){//a1,2  a,1,2
    //select from 1 to 2 unless they are collapsed
    range->SetEnd(aParentNode, aOffset, aRv);
    if (aRv.Failed()) {
      return;
    }
    SetDirection(eDirNext);
    res = difRange->SetEnd(range->GetEndParent(), range->EndOffset());
    nsresult tmp = difRange->SetStart(focusNode, focusOffset);
    if (NS_FAILED(tmp)) {
      res = tmp;
    }
    if (NS_FAILED(res)) {
      aRv.Throw(res);
      return;
    }
    selectFrames(presContext, difRange , true);
    res = SetAnchorFocusToRange(range);
    if (NS_FAILED(res)) {
      aRv.Throw(res);
      return;
    }
  }
  else if (result1 == 0 && result3 > 0){//2, a1
    //select from 2 to 1a
    SetDirection(eDirPrevious);
    range->SetStart(aParentNode, aOffset, aRv);
    if (aRv.Failed()) {
      return;
    }
    selectFrames(presContext, range, true);
    res = SetAnchorFocusToRange(range);
    if (NS_FAILED(res)) {
      aRv.Throw(res);
      return;
    }
  }
  else if (result3 <= 0 && result2 >= 0) {//a,2,1 or a2,1 or a,21 or a21
    //deselect from 2 to 1
    res = difRange->SetEnd(focusNode, focusOffset);
    difRange->SetStart(aParentNode, aOffset, aRv);
    if (aRv.Failed()) {
      return;
    }
    if (NS_FAILED(res)) {
      aRv.Throw(res);
      return;
    }

    range->SetEnd(aParentNode, aOffset, aRv);
    if (aRv.Failed()) {
      return;
    }
    res = SetAnchorFocusToRange(range);
    if (NS_FAILED(res)) {
      aRv.Throw(res);
      return;
    }
    selectFrames(presContext, difRange, false); // deselect now
    difRange->SetEnd(range->GetEndParent(), range->EndOffset());
    selectFrames(presContext, difRange, true); // must reselect last node maybe more
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
    range->SetEnd(aParentNode, aOffset, aRv);
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
      selectFrames(presContext, difRange , false);
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
    selectFrames(presContext, range , true);
  }
  else if (result2 <= 0 && result3 >= 0) {//1,2,a or 12,a or 1,2a or 12a
    //deselect from 1 to 2
    difRange->SetEnd(aParentNode, aOffset, aRv);
    res = difRange->SetStart(focusNode, focusOffset);
    if (aRv.Failed()) {
      return;
    }
    if (NS_FAILED(res)) {
      aRv.Throw(res);
      return;
    }
    SetDirection(eDirPrevious);
    range->SetStart(aParentNode, aOffset, aRv);
    if (aRv.Failed()) {
      return;
    }

    res = SetAnchorFocusToRange(range);
    if (NS_FAILED(res)) {
      aRv.Throw(res);
      return;
    }
    selectFrames(presContext, difRange , false);
    difRange->SetStart(range->GetStartParent(), range->StartOffset());
    selectFrames(presContext, difRange, true);//must reselect last node
  }
  else if (result3 >= 0 && result1 <= 0) {//2,a,1 or 2a,1 or 2,a1 or 2a1
    if (GetDirection() == eDirNext){
      range->SetEnd(startNode, startOffset);
    }
    SetDirection(eDirPrevious);
    range->SetStart(aParentNode, aOffset, aRv);
    if (aRv.Failed()) {
      return;
    }
    //deselect from a to 1
    if (focusNode != anchorNode || focusOffset!= anchorOffset) {//if collapsed diff dont do anything
      res = difRange->SetStart(anchorNode, anchorOffset);
      nsresult tmp = difRange->SetEnd(focusNode, focusOffset);
      if (NS_FAILED(tmp)) {
        res = tmp;
      }
      tmp = SetAnchorFocusToRange(range);
      if (NS_FAILED(tmp)) {
        res = tmp;
      }
      if (NS_FAILED(res)) {
        aRv.Throw(res);
        return;
      }
      selectFrames(presContext, difRange, false);
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
    selectFrames(presContext, range , true);
  }
  else if (result2 >= 0 && result1 >= 0) {//2,1,a or 21,a or 2,1a or 21a
    //select from 2 to 1
    range->SetStart(aParentNode, aOffset, aRv);
    if (aRv.Failed()) {
      return;
    }
    SetDirection(eDirPrevious);
    res = difRange->SetEnd(focusNode, focusOffset);
    nsresult tmp = difRange->SetStart(range->GetStartParent(), range->StartOffset());
    if (NS_FAILED(tmp)) {
      res = tmp;
    }
    if (NS_FAILED(res)) {
      aRv.Throw(res);
      return;
    }

    selectFrames(presContext, difRange, true);
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
      selectFrames(presContext, range, range->IsInSelection());
    }
  }

  DEBUG_OUT_RANGE(range);
#ifdef DEBUG_SELECTION
  if (GetDirection() != oldDirection) {
    printf("    direction changed to %s\n",
           GetDirection() == eDirNext? "eDirNext":"eDirPrevious");
  }
  nsCOMPtr<nsIContent> content = do_QueryInterface(&aParentNode);
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

NS_IMETHODIMP
Selection::SelectAllChildren(nsIDOMNode* aParentNode)
{
  ErrorResult result;
  nsCOMPtr<nsINode> node = do_QueryInterface(aParentNode);
  NS_ENSURE_TRUE(node, NS_ERROR_INVALID_ARG);
  SelectAllChildren(*node, result);
  return result.StealNSResult();
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

NS_IMETHODIMP
Selection::ContainsNode(nsIDOMNode* aNode, bool aAllowPartial, bool* aYes)
{
  if (!aYes) {
    return NS_ERROR_NULL_POINTER;
  }
  *aYes = false;

  nsCOMPtr<nsINode> node = do_QueryInterface(aNode);
  if (!node) {
    return NS_ERROR_NULL_POINTER;
  }
  ErrorResult result;
  *aYes = ContainsNode(*node, aAllowPartial, result);
  return result.StealNSResult();
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
  bool isData = aNode.IsNodeOfType(nsINode::eDATA_NODE);
  if (isData) {
    nodeLength = static_cast<nsIContent&>(aNode).TextLength();
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
  if (isData) {
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
                                range->GetStartParent(), range->StartOffset(),
                                range->GetEndParent(), range->EndOffset(),
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

nsIEditor*
Selection::GetEditor() const
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
  bool isText = node->IsNodeOfType(nsINode::eTEXT);

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
    aRect->x = frame->GetRect().width;
  }
  aRect->height = frame->GetRect().height;

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

  RefPtr<ScrollSelectionIntoViewEvent> ev =
    new ScrollSelectionIntoViewEvent(this, aRegion, aVertical, aHorizontal,
                                     aFlags);
  mScrollEvent = ev;
  nsresult rv;
  nsIDocument* doc = GetParentObject();
  if (doc) {
    rv = doc->Dispatch("ScrollSelectionIntoViewEvent",
                       TaskCategory::Other,
                       ev.forget());
  } else {
    rv = NS_DispatchToCurrentThread(ev);
  }

  if (NS_WARN_IF(NS_FAILED(rv))) {
    mScrollEvent = nullptr; // no need to hold on to the event
  }
  return rv;
}

NS_IMETHODIMP
Selection::ScrollIntoView(SelectionRegion aRegion, bool aIsSynchronous,
                          int16_t aVPercent, int16_t aHPercent)
{
  ErrorResult result;
  ScrollIntoView(aRegion, aIsSynchronous, aVPercent, aHPercent, result);
  if (result.Failed()) {
    return result.StealNSResult();
  }
  return NS_OK;
}

void
Selection::ScrollIntoView(int16_t aRegion, bool aIsSynchronous,
                          int16_t aVPercent, int16_t aHPercent,
                          ErrorResult& aRv)
{
  nsresult rv = ScrollIntoViewInternal(aRegion, aIsSynchronous,
                                       nsIPresShell::ScrollAxis(aVPercent),
                                       nsIPresShell::ScrollAxis(aHPercent));
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
  }
}

NS_IMETHODIMP
Selection::ScrollIntoViewInternal(SelectionRegion aRegion, bool aIsSynchronous,
                                  nsIPresShell::ScrollAxis aVertical,
                                  nsIPresShell::ScrollAxis aHorizontal)
{
  return ScrollIntoView(aRegion, aVertical, aHorizontal,
                        aIsSynchronous ? Selection::SCROLL_SYNCHRONOUS : 0);
}

nsresult
Selection::ScrollIntoView(SelectionRegion aRegion,
                          nsIPresShell::ScrollAxis aVertical,
                          nsIPresShell::ScrollAxis aHorizontal,
                          int32_t aFlags)
{
  if (!mFrameSelection)
    return NS_OK;//nothing to do

  nsCOMPtr<nsIPresShell> presShell = mFrameSelection->GetShell();
  if (!presShell)
    return NS_OK;

  if (mFrameSelection->GetBatching())
    return NS_OK;

  if (!(aFlags & Selection::SCROLL_SYNCHRONOUS))
    return PostScrollSelectionIntoViewEvent(aRegion, aFlags,
      aVertical, aHorizontal);

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

  if (aFlags & Selection::SCROLL_FOR_CARET_MOVE) {
    mozilla::Telemetry::Accumulate(mozilla::Telemetry::SCROLL_INPUT_METHODS,
        (uint32_t) ScrollInputMethod::MainThreadScrollCaretIntoView);
  }

  presShell->ScrollFrameRectIntoView(frame, rect, aVertical, aHorizontal,
    flags);
  return NS_OK;
}

NS_IMETHODIMP
Selection::AddSelectionListener(nsISelectionListener* aNewListener)
{
  if (!aNewListener)
    return NS_ERROR_NULL_POINTER;
  ErrorResult result;
  AddSelectionListener(aNewListener, result);
  if (result.Failed()) {
    return result.StealNSResult();
  }
  return NS_OK;
}

void
Selection::AddSelectionListener(nsISelectionListener* aNewListener,
                                ErrorResult& aRv)
{
  bool result = mSelectionListeners.AppendObject(aNewListener); // AddRefs
  if (!result) {
    aRv.Throw(NS_ERROR_FAILURE);
  }
}

NS_IMETHODIMP
Selection::RemoveSelectionListener(nsISelectionListener* aListenerToRemove)
{
  if (!aListenerToRemove)
    return NS_ERROR_NULL_POINTER;
  ErrorResult result;
  RemoveSelectionListener(aListenerToRemove, result);
  if (result.Failed()) {
    return result.StealNSResult();
  }
  return NS_OK;
}

void
Selection::RemoveSelectionListener(nsISelectionListener* aListenerToRemove,
                                   ErrorResult& aRv)
{
  bool result = mSelectionListeners.RemoveObject(aListenerToRemove); // Releases
  if (!result) {
    aRv.Throw(NS_ERROR_FAILURE);
  }
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
        GetEditor()) {
      RefPtr<Element> newEditingHost = GetCommonEditingHostForAllRanges();
      nsFocusManager* fm = nsFocusManager::GetFocusManager();
      nsCOMPtr<nsPIDOMWindowOuter> focusedWindow;
      nsIContent* focusedContent =
        fm->GetFocusedDescendant(window, false, getter_AddRefs(focusedWindow));
      nsCOMPtr<Element> focusedElement = do_QueryInterface(focusedContent);
      // When all selected ranges are in an editing host, it should take focus.
      if (newEditingHost && newEditingHost != focusedElement) {
        MOZ_ASSERT(!newEditingHost->IsInNativeAnonymousSubtree());
        nsCOMPtr<nsIDOMElement> domElementToFocus =
          do_QueryInterface(newEditingHost->AsDOMNode());
        // Note that don't steal focus from focused window if the window doesn't
        // have focus.
        fm->SetFocus(domElementToFocus, nsIFocusManager::FLAG_NOSWITCHFRAME);
      }
      // Otherwise, if current focused element is an editing host, it should
      // be blurred if there is no common editing host of the selected ranges.
      else if (!newEditingHost && focusedElement &&
               focusedElement == focusedElement->GetEditingHost()) {
        IgnoredErrorResult err;
        focusedElement->Blur(err);
        NS_WARNING_ASSERTION(!err.Failed(),
                             "Failed to blur focused element");
      }
    }
  }

  RefPtr<nsFrameSelection> frameSelection = mFrameSelection;
  if (frameSelection->GetBatching()) {
    frameSelection->SetDirty();
    return NS_OK;
  }
  nsCOMArray<nsISelectionListener> selectionListeners(mSelectionListeners);
  int32_t cnt = selectionListeners.Count();
  if (cnt != mSelectionListeners.Count()) {
    return NS_ERROR_OUT_OF_MEMORY;  // nsCOMArray is fallible
  }

  nsCOMPtr<nsIDOMDocument> domdoc;
  nsIPresShell* ps = GetPresShell();
  if (ps) {
    domdoc = do_QueryInterface(ps->GetDocument());
  }

  short reason = frameSelection->PopReason();
  for (int32_t i = 0; i < cnt; i++) {
    selectionListeners[i]->NotifySelectionChanged(domdoc, this, reason);
  }
  return NS_OK;
}

NS_IMETHODIMP
Selection::StartBatchChanges()
{
  if (mFrameSelection) {
    RefPtr<nsFrameSelection> frameSelection = mFrameSelection;
    frameSelection->StartBatchChanges();
  }
  return NS_OK;
}



NS_IMETHODIMP
Selection::EndBatchChanges()
{
  return EndBatchChangesInternal();
}

nsresult
Selection::EndBatchChangesInternal(int16_t aReason)
{
  if (mFrameSelection) {
    RefPtr<nsFrameSelection> frameSelection = mFrameSelection;
    frameSelection->EndBatchChanges(aReason);
  }
  return NS_OK;
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

NS_IMETHODIMP
Selection::DeleteFromDocument()
{
  ErrorResult result;
  DeleteFromDocument(result);
  return result.StealNSResult();
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

NS_IMETHODIMP
Selection::Modify(const nsAString& aAlter, const nsAString& aDirection,
                  const nsAString& aGranularity)
{
  ErrorResult result;
  Modify(aAlter, aDirection, aGranularity, result);
  return result.StealNSResult();
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

  RefPtr<nsRange> newRange;
  nsresult rv = nsRange::CreateRange(start, startOffset, end, endOffset,
                                     getter_AddRefs(newRange));
  // CreateRange returns IndexSizeError if any offset is out of bounds.
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return;
  }

  rv = RemoveAllRanges();
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return;
  }

  rv = AddRange(newRange);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return;
  }

  SetDirection(relativePosition > 0 ? eDirPrevious : eDirNext);
}

/** SelectionLanguageChange modifies the cursor Bidi level after a change in keyboard direction
 *  @param aLangRTL is true if the new language is right-to-left or false if the new language is left-to-right
 */
NS_IMETHODIMP
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

NS_IMETHODIMP_(nsDirection)
Selection::GetSelectionDirection() {
  return mDirection;
}

NS_IMETHODIMP_(void)
Selection::SetSelectionDirection(nsDirection aDirection) {
  mDirection = aDirection;
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

// nsAutoCopyListener

nsAutoCopyListener* nsAutoCopyListener::sInstance = nullptr;

NS_IMPL_ISUPPORTS(nsAutoCopyListener, nsISelectionListener)

/*
 * What we do now:
 * On every selection change, we copy to the clipboard anew, creating a
 * HTML buffer, a transferable, an nsISupportsString and
 * a huge mess every time.  This is basically what nsPresShell::DoCopy does
 * to move the selection into the clipboard for Edit->Copy.
 * 
 * What we should do, to make our end of the deal faster:
 * Create a singleton transferable with our own magic converter.  When selection
 * changes (use a quick cache to detect ``real'' changes), we put the new
 * nsISelection in the transferable.  Our magic converter will take care of
 * transferable->whatever-other-format when the time comes to actually
 * hand over the clipboard contents.
 *
 * Other issues:
 * - which X clipboard should we populate?
 * - should we use a different one than Edit->Copy, so that inadvertant
 *   selections (or simple clicks, which currently cause a selection
 *   notification, regardless of if they're in the document which currently has
 *   selection!) don't lose the contents of the ``application''?  Or should we
 *   just put some intelligence in the ``is this a real selection?'' code to
 *   protect our selection against clicks in other documents that don't create
 *   selections?
 * - maybe we should just never clear the X clipboard?  That would make this 
 *   problem just go away, which is very tempting.
 *
 * On macOS,
 * nsIClipboard::kSelectionCache is the flag for current selection cache.
 * Set the current selection cache on the parent process in
 * widget cocoa nsClipboard whenever selection changes.
 */

NS_IMETHODIMP
nsAutoCopyListener::NotifySelectionChanged(nsIDOMDocument *aDoc,
                                           nsISelection *aSel, int16_t aReason)
{
  if (mCachedClipboard == nsIClipboard::kSelectionCache) {
    nsFocusManager* fm = nsFocusManager::GetFocusManager();
    // If no active window, do nothing because a current selection changed
    // cannot occur unless it is in the active window.
    if (!fm->GetActiveWindow()) {
      return NS_OK;
    }
  }

  if (!(aReason & nsISelectionListener::MOUSEUP_REASON   || 
        aReason & nsISelectionListener::SELECTALL_REASON ||
        aReason & nsISelectionListener::KEYPRESS_REASON))
    return NS_OK; //dont care if we are still dragging

  bool collapsed;
  if (!aDoc || !aSel ||
      NS_FAILED(aSel->GetIsCollapsed(&collapsed)) || collapsed) {
#ifdef DEBUG_CLIPBOARD
    fprintf(stderr, "CLIPBOARD: no selection/collapsed selection\n");
#endif
    // If on macOS, clear the current selection transferable cached
    // on the parent process (nsClipboard) when the selection is empty.
    if (mCachedClipboard == nsIClipboard::kSelectionCache) {
      return nsCopySupport::ClearSelectionCache();
    }
    /* clear X clipboard? */
    return NS_OK;
  }

  nsCOMPtr<nsIDocument> doc = do_QueryInterface(aDoc);
  NS_ENSURE_TRUE(doc, NS_ERROR_FAILURE);

  // call the copy code
  return nsCopySupport::HTMLCopy(aSel, doc,
                                 mCachedClipboard, false);
}

/**
 * See Bug 1288453.
 *
 * Update the selection cache on repaint to handle when a pre-existing
 * selection becomes active aka the current selection.
 *
 * 1. Change the current selection by click n dragging another selection.
 *   - Make a selection on content page. Make a selection in a text editor.
 *   - You can click n drag the content selection to make it active again.
 * 2. Change the current selection when switching to a tab with a selection.
 *   - Make selection in tab.
 *   - Switching tabs will make its respective selection active.
 *
 * Therefore, we only update the selection cache on a repaint
 * if the current selection being repainted is not an empty selection.
 *
 * If the current selection is empty. The current selection cache
 * would be cleared by nsAutoCopyListener::NotifySelectionChanged.
 */
nsresult
nsFrameSelection::UpdateSelectionCacheOnRepaintSelection(Selection* aSel)
{
  nsIPresShell* ps = aSel->GetPresShell();
  if (!ps) {
    return NS_OK;
  }
  nsCOMPtr<nsIDocument> aDoc = ps->GetDocument();

  bool collapsed;
  if (aDoc && aSel &&
      NS_SUCCEEDED(aSel->GetIsCollapsed(&collapsed)) && !collapsed) {
    return nsCopySupport::HTMLCopy(aSel, aDoc,
                                   nsIClipboard::kSelectionCache, false);
  }

  return NS_OK;
}

// SelectionChangeListener

SelectionChangeListener::RawRangeData::RawRangeData(const nsRange* aRange)
{
  mozilla::ErrorResult rv;
  mStartParent = aRange->GetStartContainer(rv);
  rv.SuppressException();
  mEndParent = aRange->GetEndContainer(rv);
  rv.SuppressException();
  mStartOffset = aRange->GetStartOffset(rv);
  rv.SuppressException();
  mEndOffset = aRange->GetEndOffset(rv);
  rv.SuppressException();
}

bool
SelectionChangeListener::RawRangeData::Equals(const nsRange* aRange)
{
  mozilla::ErrorResult rv;
  bool eq = mStartParent == aRange->GetStartContainer(rv);
  rv.SuppressException();
  eq = eq && mEndParent == aRange->GetEndContainer(rv);
  rv.SuppressException();
  eq = eq && mStartOffset == aRange->GetStartOffset(rv);
  rv.SuppressException();
  eq = eq && mEndOffset == aRange->GetEndOffset(rv);
  rv.SuppressException();
  return eq;
}

inline void
ImplCycleCollectionTraverse(nsCycleCollectionTraversalCallback& aCallback,
                            SelectionChangeListener::RawRangeData& aField,
                            const char* aName,
                            uint32_t aFlags = 0)
{
  ImplCycleCollectionTraverse(aCallback, aField.mStartParent, "mStartParent", aFlags);
  ImplCycleCollectionTraverse(aCallback, aField.mEndParent, "mEndParent", aFlags);
}

NS_IMPL_CYCLE_COLLECTION_CLASS(SelectionChangeListener)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(SelectionChangeListener)
  tmp->mOldRanges.Clear();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(SelectionChangeListener)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mOldRanges);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(SelectionChangeListener)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsISelectionListener)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(SelectionChangeListener)
NS_IMPL_CYCLE_COLLECTING_RELEASE(SelectionChangeListener)

NS_IMETHODIMP
SelectionChangeListener::NotifySelectionChanged(nsIDOMDocument* aDoc,
                                                nsISelection* aSel, int16_t aReason)
{
  RefPtr<Selection> sel = aSel->AsSelection();

  nsIDocument* doc = sel->GetParentObject();
  if (!(doc && nsContentUtils::IsSystemPrincipal(doc->NodePrincipal())) &&
      !nsFrameSelection::sSelectionEventsEnabled) {
    return NS_OK;
  }

  // Check if the ranges have actually changed
  // Don't bother checking this if we are hiding changes.
  if (mOldRanges.Length() == sel->RangeCount() && !sel->IsBlockingSelectionChangeEvents()) {
    bool changed = false;

    for (size_t i = 0; i < mOldRanges.Length(); i++) {
      if (!mOldRanges[i].Equals(sel->GetRangeAt(i))) {
        changed = true;
        break;
      }
    }

    if (!changed) {
      return NS_OK;
    }
  }

  // The ranges have actually changed, update the mOldRanges array
  mOldRanges.ClearAndRetainStorage();
  for (size_t i = 0; i < sel->RangeCount(); i++) {
    mOldRanges.AppendElement(RawRangeData(sel->GetRangeAt(i)));
  }

  // If we are hiding changes, then don't do anything else. We do this after we
  // update mOldRanges so that changes after the changes stop being hidden don't
  // incorrectly trigger a change, even though they didn't change anything
  if (sel->IsBlockingSelectionChangeEvents()) {
    return NS_OK;
  }

  // The spec currently doesn't say that we should dispatch this event on text
  // controls, so for now we only support doing that under a pref, disabled by
  // default.
  // See https://github.com/w3c/selection-api/issues/53.
  if (nsFrameSelection::sSelectionEventsOnTextControlsEnabled) {
    nsCOMPtr<nsINode> target;

    // Check if we should be firing this event to a different node than the
    // document. The limiter of the nsFrameSelection will be within the native
    // anonymous subtree of the node we want to fire the event on. We need to
    // climb up the parent chain to escape the native anonymous subtree, and then
    // fire the event.
    if (const nsFrameSelection* fs = sel->GetFrameSelection()) {
      if (nsCOMPtr<nsIContent> root = fs->GetLimiter()) {
        while (root && root->IsInNativeAnonymousSubtree()) {
          root = root->GetParent();
        }

        target = root.forget();
      }
    }

    // If we didn't get a target before, we can instead fire the event at the document.
    if (!target) {
      nsCOMPtr<nsIDocument> doc = do_QueryInterface(aDoc);
      target = doc.forget();
    }

    if (target) {
      RefPtr<AsyncEventDispatcher> asyncDispatcher =
        new AsyncEventDispatcher(target, NS_LITERAL_STRING("selectionchange"), false);
      asyncDispatcher->PostDOMEvent();
    }
  } else {
    if (const nsFrameSelection* fs = sel->GetFrameSelection()) {
      if (nsCOMPtr<nsIContent> root = fs->GetLimiter()) {
        if (root->IsInNativeAnonymousSubtree()) {
          return NS_OK;
        }
      }
    }

    nsCOMPtr<nsIDocument> doc = do_QueryInterface(aDoc);
    if (doc) {
      RefPtr<AsyncEventDispatcher> asyncDispatcher =
        new AsyncEventDispatcher(doc, NS_LITERAL_STRING("selectionchange"), false);
      asyncDispatcher->PostDOMEvent();
    }
  }

  return NS_OK;
}
