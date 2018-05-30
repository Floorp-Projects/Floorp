/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Implementation of nsFrameSelection
 */

#include "nsFrameSelection.h"

#include "mozilla/Attributes.h"
#include "mozilla/AutoRestore.h"
#include "mozilla/EventStates.h"
#include "mozilla/HTMLEditor.h"
#include "mozilla/PresShell.h"

#include "nsCOMPtr.h"
#include "nsString.h"
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
#include "nsCSSFrameConstructor.h"

#include "nsGkAtoms.h"
#include "nsIFrameTraversal.h"
#include "nsLayoutUtils.h"
#include "nsLayoutCID.h"
#include "nsBidiPresUtils.h"
static NS_DEFINE_CID(kFrameTraversalCID, NS_FRAMETRAVERSAL_CID);
#include "nsTextFrame.h"

#include "nsContentUtils.h"
#include "nsThreadUtils.h"
#include "mozilla/Preferences.h"

#include "nsPresContext.h"
#include "nsIPresShell.h"
#include "nsCaret.h"
#include "AccessibleCaretEventHub.h"

#include "mozilla/MouseEvents.h"
#include "mozilla/TextEvents.h"

#include "nsITimer.h"
// notifications
#include "nsIDocument.h"

#include "nsISelectionController.h" //for the enums
#include "nsAutoCopyListener.h"
#include "SelectionChangeListener.h"
#include "nsCopySupport.h"
#include "nsIClipboard.h"
#include "nsIFrameInlines.h"

#include "nsIBidiKeyboard.h"

#include "nsError.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/Selection.h"
#include "mozilla/dom/ShadowRoot.h"
#include "mozilla/dom/Text.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/SelectionBinding.h"
#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/Telemetry.h"

#include "nsFocusManager.h"
#include "nsPIDOMWindow.h"

using namespace mozilla;
using namespace mozilla::dom;

//#define DEBUG_TABLE 1

static bool IsValidSelectionPoint(nsFrameSelection *aFrameSel, nsINode *aNode);

static nsAtom *GetTag(nsINode *aNode);
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

// Array which contains index of each SelecionType in Selection::mDOMSelections.
// For avoiding using if nor switch to retrieve the index, this needs to have
// -1 for SelectionTypes which won't be created its Selection instance.
static const int8_t kIndexOfSelections[] = {
  -1,    // SelectionType::eInvalid
  -1,    // SelectionType::eNone
  0,     // SelectionType::eNormal
  1,     // SelectionType::eSpellCheck
  2,     // SelectionType::eIMERawClause
  3,     // SelectionType::eIMESelectedRawClause
  4,     // SelectionType::eIMEConvertedClause
  5,     // SelectionType::eIMESelectedClause
  6,     // SelectionType::eAccessibility
  7,     // SelectionType::eFind
  8,     // SelectionType::eURLSecondary
  9,     // SelectionType::eURLStrikeout
};

inline int8_t
GetIndexFromSelectionType(SelectionType aSelectionType)
{
  // The enum value of eInvalid is -1 and the others are sequential value
  // starting from 0.  Therefore, |SelectionType + 1| is the index of
  // kIndexOfSelections.
  return kIndexOfSelections[static_cast<int8_t>(aSelectionType) + 1];
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

////////////BEGIN nsFrameSelection methods

nsFrameSelection::nsFrameSelection()
{
  for (size_t i = 0; i < ArrayLength(mDomSelections); i++) {
    mDomSelections[i] = new Selection(this);
    mDomSelections[i]->SetType(kPresentSelectionTypes[i]);
  }

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
}

nsFrameSelection::~nsFrameSelection()
{
}

NS_IMPL_CYCLE_COLLECTION_CLASS(nsFrameSelection)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsFrameSelection)
  for (size_t i = 0; i < ArrayLength(tmp->mDomSelections); ++i) {
    tmp->mDomSelections[i] = nullptr;
  }

  NS_IMPL_CYCLE_COLLECTION_UNLINK(mCellParent)
  tmp->mSelectingTableCellMode = TableSelection::None;
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
  for (size_t i = 0; i < ArrayLength(tmp->mDomSelections); ++i) {
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
nsFrameSelection::ConstrainFrameAndPointToAnchorSubtree(nsIFrame* aFrame,
                                                        const nsPoint& aPoint,
                                                        nsIFrame** aRetFrame,
                                                        nsPoint& aRetPoint)
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

  int8_t index = GetIndexFromSelectionType(SelectionType::eNormal);
  if (!mDomSelections[index])
    return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIContent> anchorContent =
    do_QueryInterface(mDomSelections[index]->GetAnchorNode());
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
      nsIFrame* rootFrame = mShell->GetRootFrame();
      nsPoint ptInRoot = aPoint + aFrame->GetOffsetTo(rootFrame);
      nsIFrame* cursorFrame =
        nsLayoutUtils::GetFrameForPoint(rootFrame, ptInRoot);

      // If the mouse cursor in on a frame which is descendant of same
      // selection root, we can expand the selection to the frame.
      if (cursorFrame && cursorFrame->PresShell() == mShell)
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

static
nsAtom *GetTag(nsINode *aNode)
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
    *aChildOffset = parent->ComputeIndexOf(aNode);

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
      nsAtom* tag = GetTag(current);
      if (tag == nsGkAtoms::td || tag == nsGkAtoms::th)
        return current;
      current = current->GetParent();
    }
    return nullptr;
}

void
nsFrameSelection::Init(nsIPresShell *aShell, nsIContent *aLimiter,
                       bool aAccessibleCaretEnabled)
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

  mAccessibleCaretEnabled = aAccessibleCaretEnabled;
  if (mAccessibleCaretEnabled) {
    RefPtr<AccessibleCaretEventHub> eventHub = mShell->GetAccessibleCaretEventHub();
    if (eventHub) {
      int8_t index = GetIndexFromSelectionType(SelectionType::eNormal);
      if (mDomSelections[index]) {
        mDomSelections[index]->AddSelectionListener(eventHub);
      }
    }
  }

  bool plaintextControl = (aLimiter != nullptr);
  bool initSelectEvents = plaintextControl ?
                            sSelectionEventsOnTextControlsEnabled :
                            sSelectionEventsEnabled;

  nsIDocument* doc = aShell->GetDocument();
  if (initSelectEvents ||
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

  int32_t caretStyle = Preferences::GetInt("layout.selection.caret_style", 0);
  if (caretStyle == 0
#ifdef XP_WIN
      && aAmount != eSelectLine
#endif
     ) {
    // Put caret at the selection edge in the |aDirection| direction.
    caretStyle = 2;
  }

  bool doCollapse = !sel->IsCollapsed() && !aContinueSelection && caretStyle == 2 &&
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
    nsresult result = FetchDesiredPos(desiredPos);
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
        node =  anchorFocusRange->GetStartContainer();
        offset = anchorFocusRange->StartOffset();
      } else {
        node = anchorFocusRange->GetEndContainer();
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
  nsresult result = sel->GetPrimaryFrameForFocusNode(&frame, &offsetused,
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
    bool isBRFrame = frame->IsBrFrame();
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
    if (currentFrame->IsBrFrame()) {
      currentFrame = nullptr;
      currentLevel = currentBidi.baseLevel;
    }
    if (newFrame && newFrame->IsBrFrame()) {
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

  nsINode* rangeStartNode = mMaintainRange->GetStartContainer();
  nsINode* rangeEndNode = mMaintainRange->GetEndContainer();
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
nsFrameSelection::HandleDrag(nsIFrame* aFrame, const nsPoint& aPoint)
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

    nsINode* rangenode = mMaintainRange->GetStartContainer();
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
nsFrameSelection::StartAutoScrollTimer(nsIFrame* aFrame,
                                       const nsPoint& aPoint,
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
  mSelectingTableCellMode = TableSelection::None;
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

      newRange->CollapseTo(aNewFocus, aContentOffset);
      mDomSelections[index]->AddRange(*newRange, IgnoreErrors());
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
    mCellParent = nullptr;
    RefPtr<nsPresContext> context = mShell->GetPresContext();
    if (context) {
      RefPtr<HTMLEditor> htmlEditor = nsContentUtils::GetHTMLEditor(context);
      if (htmlEditor) {
        nsINode* cellparent = GetCellParent(aNewFocus);
        nsCOMPtr<nsINode> editorHostNode = htmlEditor->GetActiveEditingHost();
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
          HandleTableSelection(parent, offset, TableSelection::Cell, &event);

        // Find the parent of this new cell and extend selection to it
        parent = ParentOffset(cellparent, &offset);

        // XXXX We need to REALLY get the current key shift state
        //  (we'd need to add event listener -- let's not bother for now)
        event.mModifiers &= ~MODIFIER_SHIFT; //aContinueSelection;
        if (parent)
        {
          mCellParent = cellparent;
          // Continue selection into next cell
          HandleTableSelection(parent, offset, TableSelection::Cell, &event);
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

  for (size_t j = 0; j < ArrayLength(mDomSelections); j++) {
    if (mDomSelections[j]) {
      details = mDomSelections[j]->LookUpSelection(aContent, aContentOffset,
                                                   aContentLength, Move(details),
                                                   kPresentSelectionTypes[j],
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

static bool
IsDisplayContents(const nsIContent* aContent)
{
  return aContent->IsElement() && aContent->AsElement()->IsDisplayContents();
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

  if (!aNode->GetPrimaryFrame() && !IsDisplayContents(aNode)) {
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
        nsCOMPtr<nsIContent> childNode = theNode->GetChildAt_Deprecated(childIndex);

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

        RefPtr<Text> textNode = theNode->GetAsText();
        if (textNode) {
          if (theNode->GetPrimaryFrame()) {
            if (aOffset > childIndex) {
              uint32_t textLength = textNode->Length();

              *aReturnOffset = (int32_t)textLength;
            } else {
              *aReturnOffset = 0;
            }
          } else {
            int32_t numChildren = aNode->GetChildCount();
            int32_t newChildIndex =
              aHint == CARET_ASSOCIATE_BEFORE ? childIndex - 1 : childIndex + 1;

            if (newChildIndex >= 0 && newChildIndex < numChildren) {
              nsCOMPtr<nsIContent> newChildNode = aNode->GetChildAt_Deprecated(newChildIndex);
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
    if (ShadowRoot* shadow = ShadowRoot::FromNode(theNode)) {
      theNode = shadow->GetHost();
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
  Selection* domSel = GetSelection(SelectionType::eNormal);
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
      if (!frame->Style()->IsTextCombined()) {
        wm = frame->GetWritingMode();
      } else {
        // Using different direction for horizontal-in-vertical would
        // make it hard to navigate via keyboard. Inherit the moving
        // direction from its parent.
        MOZ_ASSERT(frame->IsTextFrame());
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

  ErrorResult err;
  mDomSelections[index]->RemoveAllRanges(err);
  return err.StealNSResult();
}

static nsIContent*
GetFirstSelectedContent(nsRange* aRange)
{
  if (!aRange) {
    return nullptr;
  }

  MOZ_ASSERT(aRange->GetStartContainer(), "Must have start parent!");
  MOZ_ASSERT(aRange->GetStartContainer()->IsElement(), "Unexpected parent");

  return aRange->GetChildAtStartOffset();
}

// Table selection support.
// TODO: Separate table methods into a separate nsITableSelection interface
nsresult
nsFrameSelection::HandleTableSelection(nsINode* aParentContent,
                                       int32_t aContentOffset,
                                       TableSelection aTarget,
                                       WidgetMouseEvent* aMouseEvent)
{
  NS_ENSURE_TRUE(aParentContent, NS_ERROR_NULL_POINTER);
  NS_ENSURE_TRUE(aMouseEvent, NS_ERROR_NULL_POINTER);

  if (mDragState && mDragSelectingCells && aTarget == TableSelection::Table)
  {
    // We were selecting cells and user drags mouse in table border or inbetween cells,
    //  just do nothing
      return NS_OK;
  }

  nsresult result = NS_OK;

  nsIContent *childContent = aParentContent->GetChildAt_Deprecated(aContentOffset);

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
    if (aTarget != TableSelection::Table)
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
      //  even if aTarget = TableSelection::Cell

      if (mSelectingTableCellMode == TableSelection::Row ||
          mSelectingTableCellMode == TableSelection::Column)
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
          if ((mSelectingTableCellMode == TableSelection::Row && startRowIndex == curRowIndex) ||
              (mSelectingTableCellMode == TableSelection::Column && startColIndex == curColIndex))
            return NS_OK;
        }
#ifdef DEBUG_TABLE_SELECTION
printf(" Dragged into a new column or row\n");
#endif
        // Continue dragging row or column selection
        return SelectRowOrColumn(childContent, mSelectingTableCellMode);
      }
      else if (mSelectingTableCellMode == TableSelection::Cell)
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
            mDomSelections[index]->RemoveAllRanges(IgnoreErrors());

            if (startRowIndex == curRowIndex)
              mSelectingTableCellMode = TableSelection::Row;
            else
              mSelectingTableCellMode = TableSelection::Column;

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

      if (aTarget == TableSelection::Cell)
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
          mDomSelections[index]->RemoveAllRanges(IgnoreErrors());
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
            mDomSelections[index]->RemoveAllRanges(IgnoreErrors());
            // Reset selection mode that is cleared in RemoveAllRanges
            mSelectingTableCellMode = aTarget;
          }

          return SelectCellElement(childContent);
        }

        return NS_OK;
      }
      else if (aTarget == TableSelection::Table)
      {
        //TODO: We currently select entire table when clicked between cells,
        //  should we restrict to only around border?
        //  *** How do we get location data for cell and click?
        mDragSelectingCells = false;
        mStartSelectedCell = nullptr;
        mEndSelectedCell = nullptr;

        // Remove existing selection and select the table
        mDomSelections[index]->RemoveAllRanges(IgnoreErrors());
        return CreateAndAddRange(aParentContent, aContentOffset);
      }
      else if (aTarget == TableSelection::Row || aTarget == TableSelection::Column)
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
        mDomSelections[index]->RemoveAllRanges(IgnoreErrors());
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
      uint32_t rangeCount = mDomSelections[index]->RangeCount();

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
        for (uint32_t i = 0; i < rangeCount; i++)
        {
          // Strong reference, because sometimes we want to remove
          // this range, and then we might be the only owner.
          RefPtr<nsRange> range = mDomSelections[index]->GetRangeAt(i);
          if (!range) return NS_ERROR_NULL_POINTER;

          nsINode* container = range->GetStartContainer();
          if (!container) {
            return NS_ERROR_NULL_POINTER;
          }

          int32_t offset = range->StartOffset();
          // Be sure previous selection is a table cell
          nsIContent* child = range->GetChildAtStartOffset();
          if (child && IsCell(child)) {
            previousCellParent = container;
          }

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
            ErrorResult err;
            mDomSelections[index]->RemoveRange(*range, err);
            return err.StealNSResult();
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
  MOZ_ASSERT(!range || cellNode, "Must have cellNode if had a range");

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

          mDomSelections[index]->RemoveRange(*range, IgnoreErrors());
          // Since we've removed the range, decrement pointer to next range
          mSelectedCellIndex--;
        }

      } else {
        // Remove cell from selection if it belongs to the given cells range or
        // it is spanned onto the cells range.
        nsTableCellFrame* cellFrame =
          tableFrame->GetCellFrameAt(curRowIndex, curColIndex);

        uint32_t origRowIndex = cellFrame->RowIndex();
        uint32_t origColIndex = cellFrame->ColIndex();
        uint32_t actualRowSpan =
          tableFrame->GetEffectiveRowSpanAt(origRowIndex, origColIndex);
        uint32_t actualColSpan =
          tableFrame->GetEffectiveColSpanAt(curRowIndex, curColIndex);
        if (origRowIndex <= static_cast<uint32_t>(maxRowIndex) && maxRowIndex >= 0 &&
            origRowIndex + actualRowSpan - 1 >= static_cast<uint32_t>(minRowIndex) &&
            origColIndex <= static_cast<uint32_t>(maxColIndex) && maxColIndex >= 0 &&
            origColIndex + actualColSpan - 1 >= static_cast<uint32_t>(minColIndex)) {

          mDomSelections[index]->RemoveRange(*range, IgnoreErrors());
          // Since we've removed the range, decrement pointer to next range
          mSelectedCellIndex--;
        }
      }
    }

    range = GetNextCellRange();
    cellNode = GetFirstSelectedContent(range);
    MOZ_ASSERT(!range || cellNode, "Must have cellNode if had a range");
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
  uint32_t row = aStartRowIndex;
  while(true)
  {
    uint32_t col = aStartColumnIndex;
    while(true)
    {
      nsTableCellFrame* cellFrame = tableFrame->GetCellFrameAt(row, col);

      // Skip cells that are spanned from previous locations or are already selected
      if (cellFrame) {
        uint32_t origRow = cellFrame->RowIndex();
        uint32_t origCol = cellFrame->ColIndex();
        if (origRow == row && origCol == col && !cellFrame->IsSelected()) {
          result = SelectCellElement(cellFrame->GetContent());
          if (NS_FAILED(result)) return result;
        }
      }
      // Done when we reach end column
      if (col == static_cast<uint32_t>(aEndColumnIndex)) break;

      if (aStartColumnIndex < aEndColumnIndex)
        col ++;
      else
        col--;
    }
    if (row == static_cast<uint32_t>(aEndRowIndex)) break;

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
nsFrameSelection::SelectRowOrColumn(nsIContent *aCellContent, TableSelection aTarget)
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
  if (aTarget == TableSelection::Row)
    colIndex = 0;
  if (aTarget == TableSelection::Column)
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
    if (aTarget == TableSelection::Row)
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
    if (aTarget == TableSelection::Row)
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

  nsIContent* childContent = aRange->GetChildAtStartOffset();
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
  int32_t offset = parent->ComputeIndexOf(aCellElement);

  return CreateAndAddRange(parent, offset);
}

nsresult
nsFrameSelection::CreateAndAddRange(nsINode* aContainer, int32_t aOffset)
{
  if (!aContainer) {
    return NS_ERROR_NULL_POINTER;
  }

  RefPtr<nsRange> range = new nsRange(aContainer);

  // Set range around child at given offset
  nsresult rv = range->SetStartAndEnd(aContainer, aOffset,
                                      aContainer, aOffset + 1);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  int8_t index = GetIndexFromSelectionType(SelectionType::eNormal);
  if (!mDomSelections[index])
    return NS_ERROR_NULL_POINTER;

  ErrorResult err;
  mDomSelections[index]->AddRange(*range, err);
  return err.StealNSResult();
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

nsresult
nsFrameSelection::DeleteFromDocument()
{
  // If we're already collapsed, then we do nothing (bug 719503).
  int8_t index = GetIndexFromSelectionType(SelectionType::eNormal);
  if (!mDomSelections[index])
    return NS_ERROR_NULL_POINTER;

  if (mDomSelections[index]->IsCollapsed())
  {
    return NS_OK;
  }

  RefPtr<Selection> selection = mDomSelections[index];
  for (uint32_t rangeIdx = 0; rangeIdx < selection->RangeCount(); ++rangeIdx) {
    RefPtr<nsRange> range = selection->GetRangeAt(rangeIdx);
    ErrorResult res;
    range->DeleteContents(res);
    if (res.Failed()) {
      return res.StealNSResult();
    }
  }

  // Collapse to the new location.
  // If we deleted one character, then we move back one element.
  // FIXME  We don't know how to do this past frame boundaries yet.
  if (mDomSelections[index]->AnchorOffset() > 0)
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
  if (mAccessibleCaretEnabled) {
    RefPtr<AccessibleCaretEventHub> eventHub = mShell->GetAccessibleCaretEventHub();
    if (eventHub) {
      int8_t index = GetIndexFromSelectionType(SelectionType::eNormal);
      mDomSelections[index]->RemoveSelectionListener(eventHub);
    }
  }

  StopAutoScrollTimer();
  for (size_t i = 0; i < ArrayLength(mDomSelections); i++) {
    mDomSelections[i]->Clear(nullptr);
  }
  mShell = nullptr;
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

  if (aDoc && aSel && !aSel->IsCollapsed()) {
    return nsCopySupport::HTMLCopy(aSel, aDoc,
                                   nsIClipboard::kSelectionCache, false);
  }

  return NS_OK;
}

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
 * Selection in the transferable.  Our magic converter will take care of
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
nsAutoCopyListener::NotifySelectionChanged(nsIDocument *aDoc,
                                           Selection *aSel, int16_t aReason)
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

  if (!aDoc || !aSel || aSel->IsCollapsed()) {
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

  NS_ENSURE_TRUE(aDoc, NS_ERROR_FAILURE);

  // call the copy code
  return nsCopySupport::HTMLCopy(aSel, aDoc,
                                 mCachedClipboard, false);
}
