/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TextServicesDocument.h"

#include "mozilla/Assertions.h"         // for MOZ_ASSERT, etc
#include "mozilla/dom/Element.h"
#include "mozilla/dom/Selection.h"
#include "mozilla/mozalloc.h"           // for operator new, etc
#include "mozilla/TextEditor.h"         // for TextEditor
#include "nsAString.h"                  // for nsAString::Length, etc
#include "nsContentUtils.h"             // for nsContentUtils
#include "nsDebug.h"                    // for NS_ENSURE_TRUE, etc
#include "nsDependentSubstring.h"       // for Substring
#include "nsError.h"                    // for NS_OK, NS_ERROR_FAILURE, etc
#include "nsFilteredContentIterator.h"  // for nsFilteredContentIterator
#include "nsGenericHTMLElement.h"       // for nsGenericHTMLElement
#include "nsIContent.h"                 // for nsIContent, etc
#include "nsIContentIterator.h"         // for nsIContentIterator
#include "nsID.h"                       // for NS_GET_IID
#include "nsIDOMNode.h"                 // for nsIDOMNode, etc
#include "nsIEditor.h"                  // for nsIEditor, etc
#include "nsINode.h"                    // for nsINode
#include "nsIPlaintextEditor.h"         // for nsIPlaintextEditor
#include "nsISelection.h"               // for nsISelection
#include "nsISelectionController.h"     // for nsISelectionController, etc
#include "nsISupportsBase.h"            // for nsISupports
#include "nsISupportsUtils.h"           // for NS_IF_ADDREF, NS_ADDREF, etc
#include "nsITextServicesFilter.h"      // for nsITextServicesFilter
#include "mozilla/intl/WordBreaker.h"   // for WordRange, WordBreaker
#include "nsRange.h"                    // for nsRange
#include "nsString.h"                   // for nsString, nsAutoString
#include "nscore.h"                     // for nsresult, NS_IMETHODIMP, etc

#define LOCK_DOC(doc)
#define UNLOCK_DOC(doc)

namespace mozilla {

using namespace dom;

class OffsetEntry final
{
public:
  OffsetEntry(nsINode* aNode,
              int32_t aOffset,
              int32_t aLength)
    : mNode(aNode)
    , mNodeOffset(0)
    , mStrOffset(aOffset)
    , mLength(aLength)
    , mIsInsertedText(false)
    , mIsValid(true)
  {
    if (mStrOffset < 1) {
      mStrOffset = 0;
    }
    if (mLength < 1) {
      mLength = 0;
    }
  }

  virtual ~OffsetEntry()
  {
  }

  nsINode* mNode;
  int32_t mNodeOffset;
  int32_t mStrOffset;
  int32_t mLength;
  bool    mIsInsertedText;
  bool    mIsValid;
};

TextServicesDocument::TextServicesDocument()
  : mSelStartIndex(-1)
  , mSelStartOffset(-1)
  , mSelEndIndex(-1)
  , mSelEndOffset(-1)
  , mIteratorStatus(IteratorStatus::eDone)
{
}

TextServicesDocument::~TextServicesDocument()
{
  ClearOffsetTable(&mOffsetTable);
}

NS_IMPL_CYCLE_COLLECTING_ADDREF(TextServicesDocument)
NS_IMPL_CYCLE_COLLECTING_RELEASE(TextServicesDocument)

NS_INTERFACE_MAP_BEGIN(TextServicesDocument)
  NS_INTERFACE_MAP_ENTRY(nsIEditActionListener)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIEditActionListener)
  NS_INTERFACE_MAP_ENTRIES_CYCLE_COLLECTION(TextServicesDocument)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION(TextServicesDocument,
                         mDocument,
                         mSelCon,
                         mTextEditor,
                         mIterator,
                         mPrevTextBlock,
                         mNextTextBlock,
                         mExtent,
                         mTxtSvcFilter)

nsresult
TextServicesDocument::InitWithEditor(nsIEditor* aEditor)
{
  nsCOMPtr<nsISelectionController> selCon;

  NS_ENSURE_TRUE(aEditor, NS_ERROR_NULL_POINTER);

  LOCK_DOC(this);

  // Check to see if we already have an mSelCon. If we do, it
  // better be the same one the editor uses!

  nsresult rv = aEditor->GetSelectionController(getter_AddRefs(selCon));

  if (NS_FAILED(rv)) {
    UNLOCK_DOC(this);
    return rv;
  }

  if (!selCon || (mSelCon && selCon != mSelCon)) {
    UNLOCK_DOC(this);
    return NS_ERROR_FAILURE;
  }

  if (!mSelCon) {
    mSelCon = selCon;
  }

  // Check to see if we already have an mDocument. If we do, it
  // better be the same one the editor uses!

  nsCOMPtr<nsIDocument> doc = aEditor->AsEditorBase()->GetDocument();
  if (!doc || (mDocument && doc != mDocument)) {
    UNLOCK_DOC(this);
    return NS_ERROR_FAILURE;
  }

  if (!mDocument) {
    mDocument = doc;

    rv = CreateDocumentContentIterator(getter_AddRefs(mIterator));

    if (NS_FAILED(rv)) {
      UNLOCK_DOC(this);
      return rv;
    }

    mIteratorStatus = IteratorStatus::eDone;

    rv = FirstBlock();

    if (NS_FAILED(rv)) {
      UNLOCK_DOC(this);
      return rv;
    }
  }

  mTextEditor = aEditor->AsTextEditor();

  rv = aEditor->AddEditActionListener(this);

  UNLOCK_DOC(this);

  return rv;
}

nsresult
TextServicesDocument::SetExtent(nsRange* aRange)
{
  NS_ENSURE_ARG_POINTER(aRange);
  NS_ENSURE_TRUE(mDocument, NS_ERROR_FAILURE);

  LOCK_DOC(this);

  // We need to store a copy of aDOMRange since we don't
  // know where it came from.

  mExtent = aRange->CloneRange();

  // Create a new iterator based on our new extent range.

  nsresult rv = CreateContentIterator(mExtent, getter_AddRefs(mIterator));

  if (NS_FAILED(rv)) {
    UNLOCK_DOC(this);
    return rv;
  }

  // Now position the iterator at the start of the first block
  // in the range.

  mIteratorStatus = IteratorStatus::eDone;

  rv = FirstBlock();

  UNLOCK_DOC(this);

  return rv;
}

nsresult
TextServicesDocument::ExpandRangeToWordBoundaries(nsRange* aRange)
{
  NS_ENSURE_ARG_POINTER(aRange);

  // Get the end points of the range.

  nsCOMPtr<nsINode> rngStartNode, rngEndNode;
  int32_t rngStartOffset, rngEndOffset;

  nsresult rv = GetRangeEndPoints(aRange, getter_AddRefs(rngStartNode),
                                  &rngStartOffset,
                                  getter_AddRefs(rngEndNode),
                                  &rngEndOffset);

  NS_ENSURE_SUCCESS(rv, rv);

  // Create a content iterator based on the range.

  nsCOMPtr<nsIContentIterator> iter;
  rv = CreateContentIterator(aRange, getter_AddRefs(iter));

  NS_ENSURE_SUCCESS(rv, rv);

  // Find the first text node in the range.

  IteratorStatus iterStatus = IteratorStatus::eDone;

  rv = FirstTextNode(iter, &iterStatus);
  NS_ENSURE_SUCCESS(rv, rv);

  if (iterStatus == IteratorStatus::eDone) {
    // No text was found so there's no adjustment necessary!
    return NS_OK;
  }

  nsINode *firstText = iter->GetCurrentNode();
  NS_ENSURE_TRUE(firstText, NS_ERROR_FAILURE);

  // Find the last text node in the range.

  rv = LastTextNode(iter, &iterStatus);
  NS_ENSURE_SUCCESS(rv, rv);

  if (iterStatus == IteratorStatus::eDone) {
    // We should never get here because a first text block
    // was found above.
    NS_ASSERTION(false, "Found a first without a last!");
    return NS_ERROR_FAILURE;
  }

  nsINode *lastText = iter->GetCurrentNode();
  NS_ENSURE_TRUE(lastText, NS_ERROR_FAILURE);

  // Now make sure our end points are in terms of text nodes in the range!

  if (rngStartNode != firstText) {
    // The range includes the start of the first text node!
    rngStartNode = firstText;
    rngStartOffset = 0;
  }

  if (rngEndNode != lastText) {
    // The range includes the end of the last text node!
    rngEndNode = lastText;
    rngEndOffset = lastText->Length();
  }

  // Create a doc iterator so that we can scan beyond
  // the bounds of the extent range.

  nsCOMPtr<nsIContentIterator> docIter;
  rv = CreateDocumentContentIterator(getter_AddRefs(docIter));
  NS_ENSURE_SUCCESS(rv, rv);

  // Grab all the text in the block containing our
  // first text node.

  rv = docIter->PositionAt(firstText);
  NS_ENSURE_SUCCESS(rv, rv);

  iterStatus = IteratorStatus::eValid;

  nsTArray<OffsetEntry*> offsetTable;
  nsAutoString blockStr;

  rv = CreateOffsetTable(&offsetTable, docIter, &iterStatus,
                         nullptr, &blockStr);
  if (NS_FAILED(rv)) {
    ClearOffsetTable(&offsetTable);
    return rv;
  }

  nsCOMPtr<nsINode> wordStartNode, wordEndNode;
  int32_t wordStartOffset, wordEndOffset;

  rv = FindWordBounds(&offsetTable, &blockStr,
                      rngStartNode, rngStartOffset,
                      getter_AddRefs(wordStartNode), &wordStartOffset,
                      getter_AddRefs(wordEndNode), &wordEndOffset);

  ClearOffsetTable(&offsetTable);

  NS_ENSURE_SUCCESS(rv, rv);

  rngStartNode = wordStartNode;
  rngStartOffset = wordStartOffset;

  // Grab all the text in the block containing our
  // last text node.

  rv = docIter->PositionAt(lastText);
  NS_ENSURE_SUCCESS(rv, rv);

  iterStatus = IteratorStatus::eValid;

  rv = CreateOffsetTable(&offsetTable, docIter, &iterStatus,
                         nullptr, &blockStr);
  if (NS_FAILED(rv)) {
    ClearOffsetTable(&offsetTable);
    return rv;
  }

  rv = FindWordBounds(&offsetTable, &blockStr,
                      rngEndNode, rngEndOffset,
                      getter_AddRefs(wordStartNode), &wordStartOffset,
                      getter_AddRefs(wordEndNode), &wordEndOffset);

  ClearOffsetTable(&offsetTable);

  NS_ENSURE_SUCCESS(rv, rv);

  // To prevent expanding the range too much, we only change
  // rngEndNode and rngEndOffset if it isn't already at the start of the
  // word and isn't equivalent to rngStartNode and rngStartOffset.

  if (rngEndNode != wordStartNode ||
      rngEndOffset != wordStartOffset ||
      (rngEndNode == rngStartNode && rngEndOffset == rngStartOffset)) {
    rngEndNode = wordEndNode;
    rngEndOffset = wordEndOffset;
  }

  // Now adjust the range so that it uses our new
  // end points.
  rv = aRange->SetStartAndEnd(rngStartNode, rngStartOffset,
                              rngEndNode, rngEndOffset);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

nsresult
TextServicesDocument::SetFilter(nsITextServicesFilter* aFilter)
{
  // Hang on to the filter so we can set it into the filtered iterator.
  mTxtSvcFilter = aFilter;

  return NS_OK;
}

nsresult
TextServicesDocument::GetCurrentTextBlock(nsString *aStr)
{
  NS_ENSURE_TRUE(aStr, NS_ERROR_NULL_POINTER);

  aStr->Truncate();

  NS_ENSURE_TRUE(mIterator, NS_ERROR_FAILURE);

  LOCK_DOC(this);

  nsresult rv = CreateOffsetTable(&mOffsetTable, mIterator, &mIteratorStatus,
                                  mExtent, aStr);

  UNLOCK_DOC(this);

  return rv;
}

nsresult
TextServicesDocument::FirstBlock()
{
  NS_ENSURE_TRUE(mIterator, NS_ERROR_FAILURE);

  LOCK_DOC(this);

  nsresult rv = FirstTextNode(mIterator, &mIteratorStatus);

  if (NS_FAILED(rv)) {
    UNLOCK_DOC(this);
    return rv;
  }

  // Keep track of prev and next blocks, just in case
  // the text service blows away the current block.

  if (mIteratorStatus == IteratorStatus::eValid) {
    mPrevTextBlock  = nullptr;
    rv = GetFirstTextNodeInNextBlock(getter_AddRefs(mNextTextBlock));
  } else {
    // There's no text block in the document!

    mPrevTextBlock  = nullptr;
    mNextTextBlock  = nullptr;
  }

  UNLOCK_DOC(this);

  // XXX Result of FirstTextNode() or GetFirstTextNodeInNextBlock().
  return rv;
}

nsresult
TextServicesDocument::LastSelectedBlock(BlockSelectionStatus* aSelStatus,
                                        int32_t* aSelOffset,
                                        int32_t* aSelLength)
{
  NS_ENSURE_TRUE(aSelStatus && aSelOffset && aSelLength, NS_ERROR_NULL_POINTER);

  LOCK_DOC(this);

  mIteratorStatus = IteratorStatus::eDone;

  *aSelStatus = BlockSelectionStatus::eBlockNotFound;
  *aSelOffset = *aSelLength = -1;

  if (!mSelCon || !mIterator) {
    UNLOCK_DOC(this);
    return NS_ERROR_FAILURE;
  }

  RefPtr<Selection> selection =
    mSelCon->GetDOMSelection(nsISelectionController::SELECTION_NORMAL);
  if (NS_WARN_IF(!selection)) {
    UNLOCK_DOC(this);
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIContentIterator> iter;
  RefPtr<nsRange> range;
  nsCOMPtr<nsINode> parent;

  if (selection->IsCollapsed()) {
    // We have a caret. Check if the caret is in a text node.
    // If it is, make the text node's block the current block.
    // If the caret isn't in a text node, search forwards in
    // the document, till we find a text node.

    range = selection->GetRangeAt(0);

    if (!range) {
      UNLOCK_DOC(this);
      return NS_ERROR_FAILURE;
    }

    parent = range->GetStartContainer();
    if (!parent) {
      UNLOCK_DOC(this);
      return NS_ERROR_FAILURE;
    }

    nsresult rv;
    if (parent->IsText()) {
      // The caret is in a text node. Find the beginning
      // of the text block containing this text node and
      // return.

      rv = mIterator->PositionAt(parent);

      if (NS_FAILED(rv)) {
        UNLOCK_DOC(this);
        return rv;
      }

      rv = FirstTextNodeInCurrentBlock(mIterator);

      if (NS_FAILED(rv)) {
        UNLOCK_DOC(this);
        return rv;
      }

      mIteratorStatus = IteratorStatus::eValid;

      rv = CreateOffsetTable(&mOffsetTable, mIterator, &mIteratorStatus,
                             mExtent, nullptr);

      if (NS_FAILED(rv)) {
        UNLOCK_DOC(this);
        return rv;
      }

      rv = GetSelection(aSelStatus, aSelOffset, aSelLength);

      if (NS_FAILED(rv)) {
        UNLOCK_DOC(this);
        return rv;
      }

      if (*aSelStatus == BlockSelectionStatus::eBlockContains) {
        rv = SetSelectionInternal(*aSelOffset, *aSelLength, false);
      }
    } else {
      // The caret isn't in a text node. Create an iterator
      // based on a range that extends from the current caret
      // position to the end of the document, then walk forwards
      // till you find a text node, then find the beginning of it's block.

      range = CreateDocumentContentRootToNodeOffsetRange(
                parent, range->StartOffset(), false);

      if (NS_WARN_IF(!range)) {
        UNLOCK_DOC(this);
        return NS_ERROR_FAILURE;
      }

      if (range->Collapsed()) {
        // If we get here, the range is collapsed because there is nothing after
        // the caret! Just return NS_OK;

        UNLOCK_DOC(this);
        return NS_OK;
      }

      rv = CreateContentIterator(range, getter_AddRefs(iter));

      if (NS_FAILED(rv)) {
        UNLOCK_DOC(this);
        return rv;
      }

      iter->First();

      nsIContent* content = nullptr;
      while (!iter->IsDone()) {
        nsINode* currentNode = iter->GetCurrentNode();
        if (currentNode->IsText()) {
          content = currentNode->AsContent();
          break;
        }
        iter->Next();
      }

      if (!content) {
        UNLOCK_DOC(this);
        return NS_OK;
      }

      rv = mIterator->PositionAt(content);

      if (NS_FAILED(rv)) {
        UNLOCK_DOC(this);
        return rv;
      }

      rv = FirstTextNodeInCurrentBlock(mIterator);

      if (NS_FAILED(rv)) {
        UNLOCK_DOC(this);
        return rv;
      }

      mIteratorStatus = IteratorStatus::eValid;

      rv = CreateOffsetTable(&mOffsetTable, mIterator, &mIteratorStatus,
                             mExtent, nullptr);

      if (NS_FAILED(rv)) {
        UNLOCK_DOC(this);
        return rv;
      }

      rv = GetSelection(aSelStatus, aSelOffset, aSelLength);

      if (NS_FAILED(rv)) {
        UNLOCK_DOC(this);
        return rv;
      }
    }

    UNLOCK_DOC(this);

    // Result of SetSelectionInternal() in the |if| block or NS_OK.
    return rv;
  }

  // If we get here, we have an uncollapsed selection!
  // Look backwards through each range in the selection till you
  // find the first text node. If you find one, find the
  // beginning of its text block, and make it the current
  // block.

  int32_t rangeCount = static_cast<int32_t>(selection->RangeCount());
  NS_ASSERTION(rangeCount > 0, "Unexpected range count!");

  if (rangeCount <= 0) {
    UNLOCK_DOC(this);
    return NS_OK;
  }

  // XXX: We may need to add some code here to make sure
  //      the ranges are sorted in document appearance order!

  for (int32_t i = rangeCount - 1; i >= 0; i--) {
    // Get the i'th range from the selection.

    range = selection->GetRangeAt(i);

    if (!range) {
      UNLOCK_DOC(this);
      return NS_OK; // XXX Really?
    }

    // Create an iterator for the range.

    nsresult rv = CreateContentIterator(range, getter_AddRefs(iter));

    if (NS_FAILED(rv)) {
      UNLOCK_DOC(this);
      return rv;
    }

    iter->Last();

    // Now walk through the range till we find a text node.

    while (!iter->IsDone()) {
      if (iter->GetCurrentNode()->NodeType() == nsINode::TEXT_NODE) {
        // We found a text node, so position the document's
        // iterator at the beginning of the block, then get
        // the selection in terms of the string offset.

        rv = mIterator->PositionAt(iter->GetCurrentNode());

        if (NS_FAILED(rv)) {
          UNLOCK_DOC(this);
          return rv;
        }

        rv = FirstTextNodeInCurrentBlock(mIterator);

        if (NS_FAILED(rv)) {
          UNLOCK_DOC(this);
          return rv;
        }

        mIteratorStatus = IteratorStatus::eValid;

        rv = CreateOffsetTable(&mOffsetTable, mIterator, &mIteratorStatus,
                               mExtent, nullptr);

        if (NS_FAILED(rv)) {
          UNLOCK_DOC(this);
          return rv;
        }

        rv = GetSelection(aSelStatus, aSelOffset, aSelLength);

        UNLOCK_DOC(this);

        return rv;

      }

      iter->Prev();
    }
  }

  // If we get here, we didn't find any text node in the selection!
  // Create a range that extends from the end of the selection,
  // to the end of the document, then iterate forwards through
  // it till you find a text node!

  range = selection->GetRangeAt(rangeCount - 1);

  if (!range) {
    UNLOCK_DOC(this);
    return NS_ERROR_FAILURE;
  }

  parent = range->GetEndContainer();
  if (!parent) {
    UNLOCK_DOC(this);
    return NS_ERROR_FAILURE;
  }

  range = CreateDocumentContentRootToNodeOffsetRange(
            parent, range->EndOffset(), false);

  if (NS_WARN_IF(!range)) {
    UNLOCK_DOC(this);
    return NS_ERROR_FAILURE;
  }

  if (range->Collapsed()) {
    // If we get here, the range is collapsed because there is nothing after
    // the current selection! Just return NS_OK;

    UNLOCK_DOC(this);
    return NS_OK;
  }

  nsresult rv = CreateContentIterator(range, getter_AddRefs(iter));

  if (NS_FAILED(rv)) {
    UNLOCK_DOC(this);
    return rv;
  }

  iter->First();

  while (!iter->IsDone()) {
    if (iter->GetCurrentNode()->NodeType() == nsINode::TEXT_NODE) {
      // We found a text node! Adjust the document's iterator to point
      // to the beginning of its text block, then get the current selection.
      rv = mIterator->PositionAt(iter->GetCurrentNode());

      if (NS_FAILED(rv)) {
        UNLOCK_DOC(this);
        return rv;
      }

      rv = FirstTextNodeInCurrentBlock(mIterator);

      if (NS_FAILED(rv)) {
        UNLOCK_DOC(this);
        return rv;
      }


      mIteratorStatus = IteratorStatus::eValid;

      rv = CreateOffsetTable(&mOffsetTable, mIterator, &mIteratorStatus,
                             mExtent, nullptr);

      if (NS_FAILED(rv)) {
        UNLOCK_DOC(this);
        return rv;
      }

      rv = GetSelection(aSelStatus, aSelOffset, aSelLength);

      UNLOCK_DOC(this);

      return rv;
    }

    iter->Next();
  }

  // If we get here, we didn't find any block before or inside
  // the selection! Just return OK.

  UNLOCK_DOC(this);

  return NS_OK;
}

nsresult
TextServicesDocument::PrevBlock()
{
  NS_ENSURE_TRUE(mIterator, NS_ERROR_FAILURE);

  LOCK_DOC(this);

  if (mIteratorStatus == IteratorStatus::eDone) {
    return NS_OK;
  }

  switch (mIteratorStatus) {
    case IteratorStatus::eValid:
    case IteratorStatus::eNext: {

      nsresult rv = FirstTextNodeInPrevBlock(mIterator);

      if (NS_FAILED(rv)) {
        mIteratorStatus = IteratorStatus::eDone;
        UNLOCK_DOC(this);
        return rv;
      }

      if (mIterator->IsDone()) {
        mIteratorStatus = IteratorStatus::eDone;
        UNLOCK_DOC(this);
        return NS_OK;
      }

      mIteratorStatus = IteratorStatus::eValid;
      break;
    }
    case IteratorStatus::ePrev:

      // The iterator already points to the previous
      // block, so don't do anything.

      mIteratorStatus = IteratorStatus::eValid;
      break;

    default:

      mIteratorStatus = IteratorStatus::eDone;
      break;
  }

  // Keep track of prev and next blocks, just in case
  // the text service blows away the current block.
  nsresult rv = NS_OK;
  if (mIteratorStatus == IteratorStatus::eValid) {
    GetFirstTextNodeInPrevBlock(getter_AddRefs(mPrevTextBlock));
    rv = GetFirstTextNodeInNextBlock(getter_AddRefs(mNextTextBlock));
  } else {
    // We must be done!
    mPrevTextBlock = nullptr;
    mNextTextBlock = nullptr;
  }

  UNLOCK_DOC(this);

  // XXX The result of GetFirstTextNodeInNextBlock() or NS_OK.
  return rv;
}

nsresult
TextServicesDocument::NextBlock()
{
  NS_ENSURE_TRUE(mIterator, NS_ERROR_FAILURE);

  LOCK_DOC(this);

  if (mIteratorStatus == IteratorStatus::eDone) {
    return NS_OK;
  }

  switch (mIteratorStatus) {
    case IteratorStatus::eValid: {

      // Advance the iterator to the next text block.

      nsresult rv = FirstTextNodeInNextBlock(mIterator);

      if (NS_FAILED(rv)) {
        mIteratorStatus = IteratorStatus::eDone;
        UNLOCK_DOC(this);
        return rv;
      }

      if (mIterator->IsDone()) {
        mIteratorStatus = IteratorStatus::eDone;
        UNLOCK_DOC(this);
        return NS_OK;
      }

      mIteratorStatus = IteratorStatus::eValid;
      break;
    }
    case IteratorStatus::eNext:

      // The iterator already points to the next block,
      // so don't do anything to it!

      mIteratorStatus = IteratorStatus::eValid;
      break;

    case IteratorStatus::ePrev:

      // If the iterator is pointing to the previous block,
      // we know that there is no next text block! Just
      // fall through to the default case!

    default:

      mIteratorStatus = IteratorStatus::eDone;
      break;
  }

  // Keep track of prev and next blocks, just in case
  // the text service blows away the current block.
  nsresult rv = NS_OK;
  if (mIteratorStatus == IteratorStatus::eValid) {
    GetFirstTextNodeInPrevBlock(getter_AddRefs(mPrevTextBlock));
    rv = GetFirstTextNodeInNextBlock(getter_AddRefs(mNextTextBlock));
  } else {
    // We must be done.
    mPrevTextBlock = nullptr;
    mNextTextBlock = nullptr;
  }

  UNLOCK_DOC(this);

  // The result of GetFirstTextNodeInNextBlock() or NS_OK.
  return rv;
}

nsresult
TextServicesDocument::IsDone(bool* aIsDone)
{
  NS_ENSURE_TRUE(aIsDone, NS_ERROR_NULL_POINTER);

  *aIsDone = false;

  NS_ENSURE_TRUE(mIterator, NS_ERROR_FAILURE);

  LOCK_DOC(this);

  *aIsDone = mIteratorStatus == IteratorStatus::eDone;

  UNLOCK_DOC(this);

  return NS_OK;
}

nsresult
TextServicesDocument::SetSelection(int32_t aOffset,
                                   int32_t aLength)
{
  NS_ENSURE_TRUE(mSelCon && aOffset >= 0 && aLength >= 0, NS_ERROR_FAILURE);

  LOCK_DOC(this);

  nsresult rv = SetSelectionInternal(aOffset, aLength, true);

  UNLOCK_DOC(this);

  //**** KDEBUG ****
  // printf("\n * Sel: (%2d, %4d) (%2d, %4d)\n", mSelStartIndex, mSelStartOffset, mSelEndIndex, mSelEndOffset);
  //**** KDEBUG ****

  return rv;
}

nsresult
TextServicesDocument::ScrollSelectionIntoView()
{
  NS_ENSURE_TRUE(mSelCon, NS_ERROR_FAILURE);

  LOCK_DOC(this);

  // After ScrollSelectionIntoView(), the pending notifications might be flushed
  // and PresShell/PresContext/Frames may be dead. See bug 418470.
  nsresult rv =
    mSelCon->ScrollSelectionIntoView(
      nsISelectionController::SELECTION_NORMAL,
      nsISelectionController::SELECTION_FOCUS_REGION,
      nsISelectionController::SCROLL_SYNCHRONOUS);

  UNLOCK_DOC(this);

  return rv;
}

nsresult
TextServicesDocument::DeleteSelection()
{
  if (NS_WARN_IF(!mTextEditor) || NS_WARN_IF(!SelectionIsValid())) {
    return NS_ERROR_FAILURE;
  }

  if (SelectionIsCollapsed()) {
    return NS_OK;
  }

  LOCK_DOC(this);

  // If we have an mExtent, save off its current set of
  // end points so we can compare them against mExtent's
  // set after the deletion of the content.

  nsCOMPtr<nsINode> origStartNode, origEndNode;
  int32_t origStartOffset = 0, origEndOffset = 0;

  if (mExtent) {
    nsresult rv =
      GetRangeEndPoints(mExtent,
                        getter_AddRefs(origStartNode), &origStartOffset,
                        getter_AddRefs(origEndNode), &origEndOffset);

    if (NS_FAILED(rv)) {
      UNLOCK_DOC(this);
      return rv;
    }
  }

  int32_t selLength;
  OffsetEntry *entry, *newEntry;

  for (int32_t i = mSelStartIndex; i <= mSelEndIndex; i++) {
    entry = mOffsetTable[i];

    if (i == mSelStartIndex) {
      // Calculate the length of the selection. Note that the
      // selection length can be zero if the start of the selection
      // is at the very end of a text node entry.

      if (entry->mIsInsertedText) {
        // Inserted text offset entries have no width when
        // talking in terms of string offsets! If the beginning
        // of the selection is in an inserted text offset entry,
        // the caret is always at the end of the entry!

        selLength = 0;
      } else {
        selLength = entry->mLength - (mSelStartOffset - entry->mStrOffset);
      }

      if (selLength > 0 && mSelStartOffset > entry->mStrOffset) {
        // Selection doesn't start at the beginning of the
        // text node entry. We need to split this entry into
        // two pieces, the piece before the selection, and
        // the piece inside the selection.

        nsresult rv = SplitOffsetEntry(i, selLength);

        if (NS_FAILED(rv)) {
          UNLOCK_DOC(this);
          return rv;
        }

        // Adjust selection indexes to account for new entry:

        ++mSelStartIndex;
        ++mSelEndIndex;
        ++i;

        entry = mOffsetTable[i];
      }


      if (selLength > 0 && mSelStartIndex < mSelEndIndex) {
        // The entire entry is contained in the selection. Mark the
        // entry invalid.
        entry->mIsValid = false;
      }
    }

    if (i == mSelEndIndex) {
      if (entry->mIsInsertedText) {
        // Inserted text offset entries have no width when
        // talking in terms of string offsets! If the end
        // of the selection is in an inserted text offset entry,
        // the selection includes the entire entry!

        entry->mIsValid = false;
      } else {
        // Calculate the length of the selection. Note that the
        // selection length can be zero if the end of the selection
        // is at the very beginning of a text node entry.

        selLength = mSelEndOffset - entry->mStrOffset;

        if (selLength > 0 &&
            mSelEndOffset < entry->mStrOffset + entry->mLength) {
          // mStrOffset is guaranteed to be inside the selection, even
          // when mSelStartIndex == mSelEndIndex.

          nsresult rv = SplitOffsetEntry(i, entry->mLength - selLength);

          if (NS_FAILED(rv)) {
            UNLOCK_DOC(this);
            return rv;
          }

          // Update the entry fields:

          newEntry = mOffsetTable[i+1];
          newEntry->mNodeOffset = entry->mNodeOffset;
        }


        if (selLength > 0 &&
            mSelEndOffset == entry->mStrOffset + entry->mLength) {
          // The entire entry is contained in the selection. Mark the
          // entry invalid.
          entry->mIsValid = false;
        }
      }
    }

    if (i != mSelStartIndex && i != mSelEndIndex) {
      // The entire entry is contained in the selection. Mark the
      // entry invalid.
      entry->mIsValid = false;
    }
  }

  // Make sure mIterator always points to something valid!

  AdjustContentIterator();

  // Now delete the actual content!
  RefPtr<TextEditor> textEditor = mTextEditor;
  nsresult rv =
    textEditor->DeleteSelection(nsIEditor::ePrevious, nsIEditor::eStrip);
  if (NS_FAILED(rv)) {
    UNLOCK_DOC(this);
    return rv;
  }

  // Now that we've actually deleted the selected content,
  // check to see if our mExtent has changed, if so, then
  // we have to create a new content iterator!

  if (origStartNode && origEndNode) {
    nsCOMPtr<nsINode> curStartNode, curEndNode;
    int32_t curStartOffset = 0, curEndOffset = 0;

    rv = GetRangeEndPoints(mExtent,
                           getter_AddRefs(curStartNode), &curStartOffset,
                           getter_AddRefs(curEndNode), &curEndOffset);

    if (NS_FAILED(rv)) {
      UNLOCK_DOC(this);
      return rv;
    }

    if (origStartNode != curStartNode || origEndNode != curEndNode) {
      // The range has changed, so we need to create a new content
      // iterator based on the new range.

      nsCOMPtr<nsIContent> curContent;

      if (mIteratorStatus != IteratorStatus::eDone) {
        // The old iterator is still pointing to something valid,
        // so get its current node so we can restore it after we
        // create the new iterator!

        curContent = mIterator->GetCurrentNode()
                     ? mIterator->GetCurrentNode()->AsContent()
                     : nullptr;
      }

      // Create the new iterator.

      rv = CreateContentIterator(mExtent, getter_AddRefs(mIterator));

      if (NS_FAILED(rv)) {
        UNLOCK_DOC(this);
        return rv;
      }

      // Now make the new iterator point to the content node
      // the old one was pointing at.

      if (curContent) {
        rv = mIterator->PositionAt(curContent);

        if (NS_FAILED(rv)) {
          mIteratorStatus = IteratorStatus::eDone;
        } else {
          mIteratorStatus = IteratorStatus::eValid;
        }
      }
    }
  }

  entry = 0;

  // Move the caret to the end of the first valid entry.
  // Start with mSelStartIndex since it may still be valid.

  for (int32_t i = mSelStartIndex; !entry && i >= 0; i--) {
    entry = mOffsetTable[i];

    if (!entry->mIsValid) {
      entry = 0;
    } else {
      mSelStartIndex  = mSelEndIndex  = i;
      mSelStartOffset = mSelEndOffset = entry->mStrOffset + entry->mLength;
    }
  }

  // If we still don't have a valid entry, move the caret
  // to the next valid entry after the selection:

  for (int32_t i = mSelEndIndex;
       !entry && i < static_cast<int32_t>(mOffsetTable.Length()); i++) {
    entry = mOffsetTable[i];

    if (!entry->mIsValid) {
      entry = 0;
    } else {
      mSelStartIndex = mSelEndIndex = i;
      mSelStartOffset = mSelEndOffset = entry->mStrOffset;
    }
  }

  if (entry) {
    SetSelection(mSelStartOffset, 0);
  } else {
    // Uuughh we have no valid offset entry to place our
    // caret ... just mark the selection invalid.
    mSelStartIndex  = mSelEndIndex  = -1;
    mSelStartOffset = mSelEndOffset = -1;
  }

  // Now remove any invalid entries from the offset table.

  rv = RemoveInvalidOffsetEntries();

  //**** KDEBUG ****
  // printf("\n---- After Delete\n");
  // printf("Sel: (%2d, %4d) (%2d, %4d)\n", mSelStartIndex, mSelStartOffset, mSelEndIndex, mSelEndOffset);
  // PrintOffsetTable();
  //**** KDEBUG ****

  UNLOCK_DOC(this);

  return rv;
}

nsresult
TextServicesDocument::InsertText(const nsString* aText)
{
  if (NS_WARN_IF(!aText)) {
    return NS_ERROR_INVALID_ARG;
  }

  if (NS_WARN_IF(!mTextEditor) || NS_WARN_IF(!SelectionIsValid())) {
    return NS_ERROR_FAILURE;
  }

  // If the selection is not collapsed, we need to save
  // off the selection offsets so we can restore the
  // selection and delete the selected content after we've
  // inserted the new text. This is necessary to try and
  // retain as much of the original style of the content
  // being deleted.

  bool collapsedSelection = SelectionIsCollapsed();
  int32_t savedSelOffset = mSelStartOffset;
  int32_t savedSelLength = mSelEndOffset - mSelStartOffset;

  if (!collapsedSelection) {
    // Collapse to the start of the current selection
    // for the insert!

    nsresult rv = SetSelection(mSelStartOffset, 0);

    NS_ENSURE_SUCCESS(rv, rv);
  }


  LOCK_DOC(this);

  RefPtr<TextEditor> textEditor = mTextEditor;
  nsresult rv = textEditor->BeginTransaction();
  if (NS_FAILED(rv)) {
    UNLOCK_DOC(this);
    return rv;
  }

  rv = textEditor->InsertText(*aText);
  if (NS_FAILED(rv)) {
    textEditor->EndTransaction();
    UNLOCK_DOC(this);
    return rv;
  }

  int32_t strLength = aText->Length();

  OffsetEntry *itEntry;
  OffsetEntry *entry = mOffsetTable[mSelStartIndex];
  void *node         = entry->mNode;

  NS_ASSERTION((entry->mIsValid), "Invalid insertion point!");

  if (entry->mStrOffset == mSelStartOffset) {
    if (entry->mIsInsertedText) {
      // If the caret is in an inserted text offset entry,
      // we simply insert the text at the end of the entry.

      entry->mLength += strLength;
    } else {
      // Insert an inserted text offset entry before the current
      // entry!

      itEntry = new OffsetEntry(entry->mNode, entry->mStrOffset, strLength);

      if (!itEntry) {
        textEditor->EndTransaction();
        UNLOCK_DOC(this);
        return NS_ERROR_OUT_OF_MEMORY;
      }

      itEntry->mIsInsertedText = true;
      itEntry->mNodeOffset = entry->mNodeOffset;

      if (!mOffsetTable.InsertElementAt(mSelStartIndex, itEntry)) {
        textEditor->EndTransaction();
        UNLOCK_DOC(this);
        return NS_ERROR_FAILURE;
      }
    }
  } else if (entry->mStrOffset + entry->mLength == mSelStartOffset) {
    // We are inserting text at the end of the current offset entry.
    // Look at the next valid entry in the table. If it's an inserted
    // text entry, add to its length and adjust its node offset. If
    // it isn't, add a new inserted text entry.

    // XXX Rename this!
    uint32_t i = mSelStartIndex + 1;
    itEntry = 0;

    if (mOffsetTable.Length() > i) {
      itEntry = mOffsetTable[i];

      if (!itEntry) {
        textEditor->EndTransaction();
        UNLOCK_DOC(this);
        return NS_ERROR_FAILURE;
      }

      // Check if the entry is a match. If it isn't, set
      // iEntry to zero.

      if (!itEntry->mIsInsertedText || itEntry->mStrOffset != mSelStartOffset) {
        itEntry = 0;
      }
    }

    if (!itEntry) {
      // We didn't find an inserted text offset entry, so
      // create one.

      itEntry = new OffsetEntry(entry->mNode, mSelStartOffset, 0);

      if (!itEntry) {
        textEditor->EndTransaction();
        UNLOCK_DOC(this);
        return NS_ERROR_OUT_OF_MEMORY;
      }

      itEntry->mNodeOffset = entry->mNodeOffset + entry->mLength;
      itEntry->mIsInsertedText = true;

      if (!mOffsetTable.InsertElementAt(i, itEntry)) {
        delete itEntry;
        return NS_ERROR_FAILURE;
      }
    }

    // We have a valid inserted text offset entry. Update its
    // length, adjust the selection indexes, and make sure the
    // caret is properly placed!

    itEntry->mLength += strLength;

    mSelStartIndex = mSelEndIndex = i;

    RefPtr<Selection> selection =
      mSelCon->GetDOMSelection(nsISelectionController::SELECTION_NORMAL);
    if (NS_WARN_IF(!selection)) {
      textEditor->EndTransaction();
      UNLOCK_DOC(this);
      return rv;
    }

    rv = selection->Collapse(itEntry->mNode,
                             itEntry->mNodeOffset + itEntry->mLength);

    if (NS_FAILED(rv)) {
      textEditor->EndTransaction();
      UNLOCK_DOC(this);
      return rv;
    }
  } else if (entry->mStrOffset + entry->mLength > mSelStartOffset) {
    // We are inserting text into the middle of the current offset entry.
    // split the current entry into two parts, then insert an inserted text
    // entry between them!

    // XXX Rename this!
    uint32_t i = entry->mLength - (mSelStartOffset - entry->mStrOffset);

    rv = SplitOffsetEntry(mSelStartIndex, i);

    if (NS_FAILED(rv)) {
      textEditor->EndTransaction();
      UNLOCK_DOC(this);
      return rv;
    }

    itEntry = new OffsetEntry(entry->mNode, mSelStartOffset, strLength);

    if (!itEntry) {
      textEditor->EndTransaction();
      UNLOCK_DOC(this);
      return NS_ERROR_OUT_OF_MEMORY;
    }

    itEntry->mIsInsertedText = true;
    itEntry->mNodeOffset     = entry->mNodeOffset + entry->mLength;

    if (!mOffsetTable.InsertElementAt(mSelStartIndex + 1, itEntry)) {
      textEditor->EndTransaction();
      UNLOCK_DOC(this);
      return NS_ERROR_FAILURE;
    }

    mSelEndIndex = ++mSelStartIndex;
  }

  // We've just finished inserting an inserted text offset entry.
  // update all entries with the same mNode pointer that follow
  // it in the table!

  for (size_t i = mSelStartIndex + 1; i < mOffsetTable.Length(); i++) {
    entry = mOffsetTable[i];
    if (entry->mNode != node) {
      break;
    }
    if (entry->mIsValid) {
      entry->mNodeOffset += strLength;
    }
  }

  //**** KDEBUG ****
  // printf("\n---- After Insert\n");
  // printf("Sel: (%2d, %4d) (%2d, %4d)\n", mSelStartIndex, mSelStartOffset, mSelEndIndex, mSelEndOffset);
  // PrintOffsetTable();
  //**** KDEBUG ****

  if (!collapsedSelection) {
    rv = SetSelection(savedSelOffset, savedSelLength);

    if (NS_FAILED(rv)) {
      textEditor->EndTransaction();
      UNLOCK_DOC(this);
      return rv;
    }

    rv = DeleteSelection();

    if (NS_FAILED(rv)) {
      textEditor->EndTransaction();
      UNLOCK_DOC(this);
      return rv;
    }
  }

  rv = textEditor->EndTransaction();

  UNLOCK_DOC(this);

  return rv;
}

void
TextServicesDocument::DidDeleteNode(nsINode* aChild)
{
  if (NS_WARN_IF(!mIterator)) {
    return;
  }

  LOCK_DOC(this);

  int32_t nodeIndex = 0;
  bool hasEntry = false;
  OffsetEntry *entry;

  nsresult rv =
    NodeHasOffsetEntry(&mOffsetTable, aChild, &hasEntry, &nodeIndex);
  if (NS_FAILED(rv)) {
    UNLOCK_DOC(this);
    return;
  }

  if (!hasEntry) {
    // It's okay if the node isn't in the offset table, the
    // editor could be cleaning house.
    UNLOCK_DOC(this);
    return;
  }

  nsINode* node = mIterator->GetCurrentNode();
  if (node && node == aChild &&
      mIteratorStatus != IteratorStatus::eDone) {
    // XXX: This should never really happen because
    // AdjustContentIterator() should have been called prior
    // to the delete to try and position the iterator on the
    // next valid text node in the offset table, and if there
    // wasn't a next, it would've set mIteratorStatus to eIsDone.

    NS_ERROR("DeleteNode called for current iterator node.");
  }

  int32_t tcount = mOffsetTable.Length();
  while (nodeIndex < tcount) {
    entry = mOffsetTable[nodeIndex];

    if (!entry) {
      UNLOCK_DOC(this);
      return;
    }

    if (entry->mNode == aChild) {
      entry->mIsValid = false;
    }

    nodeIndex++;
  }

  UNLOCK_DOC(this);
}

void
TextServicesDocument::DidJoinNodes(nsINode& aLeftNode,
                                   nsINode& aRightNode)
{
  // Make sure that both nodes are text nodes -- otherwise we don't care.
  if (!aLeftNode.IsText() || !aRightNode.IsText()) {
    return;
  }

  // Note: The editor merges the contents of the left node into the
  //       contents of the right.

  int32_t leftIndex = 0;
  int32_t rightIndex = 0;
  bool leftHasEntry = false;
  bool rightHasEntry = false;

  nsresult rv =
    NodeHasOffsetEntry(&mOffsetTable, &aLeftNode, &leftHasEntry, &leftIndex);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  if (!leftHasEntry) {
    // It's okay if the node isn't in the offset table, the
    // editor could be cleaning house.
    return;
  }

  rv = NodeHasOffsetEntry(&mOffsetTable, &aRightNode,
                          &rightHasEntry, &rightIndex);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  if (!rightHasEntry) {
    // It's okay if the node isn't in the offset table, the
    // editor could be cleaning house.
    return;
  }

  NS_ASSERTION(leftIndex < rightIndex, "Indexes out of order.");

  if (leftIndex > rightIndex) {
    // Don't know how to handle this situation.
    return;
  }

  LOCK_DOC(this);

  OffsetEntry *entry = mOffsetTable[rightIndex];
  NS_ASSERTION(entry->mNodeOffset == 0, "Unexpected offset value for rightIndex.");

  // Run through the table and change all entries referring to
  // the left node so that they now refer to the right node:
  uint32_t nodeLength = aLeftNode.Length();
  for (int32_t i = leftIndex; i < rightIndex; i++) {
    entry = mOffsetTable[i];
    if (entry->mNode != &aLeftNode) {
      break;
    }
    if (entry->mIsValid) {
      entry->mNode = &aRightNode;
    }
  }

  // Run through the table and adjust the node offsets
  // for all entries referring to the right node.
  for (int32_t i = rightIndex;
       i < static_cast<int32_t>(mOffsetTable.Length()); i++) {
    entry = mOffsetTable[i];
    if (entry->mNode != &aRightNode) {
      break;
    }
    if (entry->mIsValid) {
      entry->mNodeOffset += nodeLength;
    }
  }

  // Now check to see if the iterator is pointing to the
  // left node. If it is, make it point to the right node!

  if (mIterator->GetCurrentNode() == &aLeftNode) {
    mIterator->PositionAt(&aRightNode);
  }

  UNLOCK_DOC(this);
}

nsresult
TextServicesDocument::CreateContentIterator(nsRange* aRange,
                                            nsIContentIterator** aIterator)
{
  NS_ENSURE_TRUE(aRange && aIterator, NS_ERROR_NULL_POINTER);

  *aIterator = nullptr;

  // Create a nsFilteredContentIterator
  // This class wraps the ContentIterator in order to give itself a chance
  // to filter out certain content nodes
  RefPtr<nsFilteredContentIterator> filter = new nsFilteredContentIterator(mTxtSvcFilter);

  nsresult rv = filter->Init(aRange);
  if (NS_FAILED(rv)) {
    return rv;
  }

  filter.forget(aIterator);
  return NS_OK;
}

Element*
TextServicesDocument::GetDocumentContentRootNode() const
{
  if (NS_WARN_IF(!mDocument)) {
    return nullptr;
  }

  if (mDocument->IsHTMLOrXHTML()) {
    // For HTML documents, the content root node is the body.
    return mDocument->GetBody();
  }

  // For non-HTML documents, the content root node will be the document element.
  return mDocument->GetDocumentElement();
}

already_AddRefed<nsRange>
TextServicesDocument::CreateDocumentContentRange()
{
  nsCOMPtr<nsINode> node = GetDocumentContentRootNode();
  if (NS_WARN_IF(!node)) {
    return nullptr;
  }

  RefPtr<nsRange> range = new nsRange(node);
  ErrorResult errorResult;
  range->SelectNodeContents(*node, errorResult);
  if (NS_WARN_IF(errorResult.Failed())) {
    errorResult.SuppressException();
    return nullptr;
  }

  return range.forget();
}

already_AddRefed<nsRange>
TextServicesDocument::CreateDocumentContentRootToNodeOffsetRange(
                        nsINode* aParent,
                        uint32_t aOffset,
                        bool aToStart)
{
  if (NS_WARN_IF(!aParent)) {
    return nullptr;
  }

  nsCOMPtr<nsINode> bodyNode = GetDocumentContentRootNode();
  if (NS_WARN_IF(!bodyNode)) {
    return nullptr;
  }

  nsCOMPtr<nsINode> startNode;
  nsCOMPtr<nsINode> endNode;
  uint32_t startOffset, endOffset;

  if (aToStart) {
    // The range should begin at the start of the document
    // and extend up until (aParent, aOffset).

    startNode = bodyNode;
    startOffset = 0;
    endNode     = aParent;
    endOffset   = aOffset;
  } else {
    // The range should begin at (aParent, aOffset) and
    // extend to the end of the document.

    startNode   = aParent;
    startOffset = aOffset;
    endNode = bodyNode;
    endOffset = endNode ? endNode->GetChildCount() : 0;
  }

  RefPtr<nsRange> range;
  nsresult rv = nsRange::CreateRange(startNode, startOffset, endNode, endOffset,
                                     getter_AddRefs(range));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }
  return range.forget();
}

nsresult
TextServicesDocument::CreateDocumentContentIterator(
                        nsIContentIterator** aIterator)
{
  NS_ENSURE_TRUE(aIterator, NS_ERROR_NULL_POINTER);

  RefPtr<nsRange> range  = CreateDocumentContentRange();
  if (NS_WARN_IF(!range)) {
    *aIterator = nullptr;
    return NS_ERROR_FAILURE;
  }

  return CreateContentIterator(range, aIterator);
}

nsresult
TextServicesDocument::AdjustContentIterator()
{
  NS_ENSURE_TRUE(mIterator, NS_ERROR_FAILURE);

  nsCOMPtr<nsINode> node = mIterator->GetCurrentNode();
  NS_ENSURE_TRUE(node, NS_ERROR_FAILURE);

  size_t tcount = mOffsetTable.Length();

  nsINode* prevValidNode = nullptr;
  nsINode* nextValidNode = nullptr;
  bool foundEntry = false;
  OffsetEntry *entry;

  for (size_t i = 0; i < tcount && !nextValidNode; i++) {
    entry = mOffsetTable[i];

    NS_ENSURE_TRUE(entry, NS_ERROR_FAILURE);

    if (entry->mNode == node) {
      if (entry->mIsValid) {
        // The iterator is still pointing to something valid!
        // Do nothing!
        return NS_OK;
      }
      // We found an invalid entry that points to
      // the current iterator node. Stop looking for
      // a previous valid node!
      foundEntry = true;
    }

    if (entry->mIsValid) {
      if (!foundEntry) {
        prevValidNode = entry->mNode;
      } else {
        nextValidNode = entry->mNode;
      }
    }
  }

  nsCOMPtr<nsIContent> content;

  if (prevValidNode) {
    if (prevValidNode->IsContent()) {
      content = prevValidNode->AsContent();
    }
  } else if (nextValidNode) {
    if (nextValidNode->IsContent()) {
      content = nextValidNode->AsContent();
    }
  }

  if (content) {
    nsresult rv = mIterator->PositionAt(content);

    if (NS_FAILED(rv)) {
      mIteratorStatus = IteratorStatus::eDone;
    } else {
      mIteratorStatus = IteratorStatus::eValid;
    }
    return rv;
  }

  // If we get here, there aren't any valid entries
  // in the offset table! Try to position the iterator
  // on the next text block first, then previous if
  // one doesn't exist!

  if (mNextTextBlock) {
    nsresult rv = mIterator->PositionAt(mNextTextBlock);

    if (NS_FAILED(rv)) {
      mIteratorStatus = IteratorStatus::eDone;
      return rv;
    }

    mIteratorStatus = IteratorStatus::eNext;
  } else if (mPrevTextBlock) {
    nsresult rv = mIterator->PositionAt(mPrevTextBlock);

    if (NS_FAILED(rv)) {
      mIteratorStatus = IteratorStatus::eDone;
      return rv;
    }

    mIteratorStatus = IteratorStatus::ePrev;
  } else {
    mIteratorStatus = IteratorStatus::eDone;
  }
  return NS_OK;
}

// static
bool
TextServicesDocument::DidSkip(nsIContentIterator* aFilteredIter)
{
  // We can assume here that the Iterator is a nsFilteredContentIterator because
  // all the iterator are created in CreateContentIterator which create a
  // nsFilteredContentIterator
  // So if the iterator bailed on one of the "filtered" content nodes then we
  // consider that to be a block and bail with true
  if (aFilteredIter) {
    nsFilteredContentIterator* filter = static_cast<nsFilteredContentIterator *>(aFilteredIter);
    if (filter && filter->DidSkip()) {
      return true;
    }
  }
  return false;
}

// static
void
TextServicesDocument::ClearDidSkip(nsIContentIterator* aFilteredIter)
{
  // Clear filter's skip flag
  if (aFilteredIter) {
    nsFilteredContentIterator* filter = static_cast<nsFilteredContentIterator *>(aFilteredIter);
    filter->ClearDidSkip();
  }
}

// static
bool
TextServicesDocument::IsBlockNode(nsIContent* aContent)
{
  if (!aContent) {
    NS_ERROR("How did a null pointer get passed to IsBlockNode?");
    return false;
  }

  nsAtom *atom = aContent->NodeInfo()->NameAtom();

  return (nsGkAtoms::a       != atom &&
          nsGkAtoms::address != atom &&
          nsGkAtoms::big     != atom &&
          nsGkAtoms::b       != atom &&
          nsGkAtoms::cite    != atom &&
          nsGkAtoms::code    != atom &&
          nsGkAtoms::dfn     != atom &&
          nsGkAtoms::em      != atom &&
          nsGkAtoms::font    != atom &&
          nsGkAtoms::i       != atom &&
          nsGkAtoms::kbd     != atom &&
          nsGkAtoms::keygen  != atom &&
          nsGkAtoms::nobr    != atom &&
          nsGkAtoms::s       != atom &&
          nsGkAtoms::samp    != atom &&
          nsGkAtoms::small   != atom &&
          nsGkAtoms::spacer  != atom &&
          nsGkAtoms::span    != atom &&
          nsGkAtoms::strike  != atom &&
          nsGkAtoms::strong  != atom &&
          nsGkAtoms::sub     != atom &&
          nsGkAtoms::sup     != atom &&
          nsGkAtoms::tt      != atom &&
          nsGkAtoms::u       != atom &&
          nsGkAtoms::var     != atom &&
          nsGkAtoms::wbr     != atom);
}

// static
bool
TextServicesDocument::HasSameBlockNodeParent(nsIContent* aContent1,
                                             nsIContent* aContent2)
{
  nsIContent* p1 = aContent1->GetParent();
  nsIContent* p2 = aContent2->GetParent();

  // Quick test:

  if (p1 == p2) {
    return true;
  }

  // Walk up the parent hierarchy looking for closest block boundary node:

  while (p1 && !IsBlockNode(p1)) {
    p1 = p1->GetParent();
  }

  while (p2 && !IsBlockNode(p2)) {
    p2 = p2->GetParent();
  }

  return p1 == p2;
}

// static
bool
TextServicesDocument::IsTextNode(nsIContent* aContent)
{
  NS_ENSURE_TRUE(aContent, false);
  return nsINode::TEXT_NODE == aContent->NodeType();
}

// static
bool
TextServicesDocument::IsTextNode(nsIDOMNode* aNode)
{
  NS_ENSURE_TRUE(aNode, false);

  nsCOMPtr<nsIContent> content = do_QueryInterface(aNode);
  return IsTextNode(content);
}

nsresult
TextServicesDocument::SetSelectionInternal(int32_t aOffset,
                                           int32_t aLength,
                                           bool aDoUpdate)
{
  NS_ENSURE_TRUE(mSelCon && aOffset >= 0 && aLength >= 0, NS_ERROR_FAILURE);

  nsCOMPtr<nsINode> startNode;
  int32_t startNodeOffset = 0;
  OffsetEntry *entry;

  // Find start of selection in node offset terms:

  for (size_t i = 0; !startNode && i < mOffsetTable.Length(); i++) {
    entry = mOffsetTable[i];
    if (entry->mIsValid) {
      if (entry->mIsInsertedText) {
        // Caret can only be placed at the end of an
        // inserted text offset entry, if the offsets
        // match exactly!

        if (entry->mStrOffset == aOffset) {
          startNode = entry->mNode;
          startNodeOffset = entry->mNodeOffset + entry->mLength;
        }
      } else if (aOffset >= entry->mStrOffset) {
        bool foundEntry = false;
        int32_t strEndOffset = entry->mStrOffset + entry->mLength;

        if (aOffset < strEndOffset) {
          foundEntry = true;
        } else if (aOffset == strEndOffset) {
          // Peek after this entry to see if we have any
          // inserted text entries belonging to the same
          // entry->mNode. If so, we have to place the selection
          // after it!

          if (i + 1 < mOffsetTable.Length()) {
            OffsetEntry *nextEntry = mOffsetTable[i+1];

            if (!nextEntry->mIsValid || nextEntry->mStrOffset != aOffset) {
              // Next offset entry isn't an exact match, so we'll
              // just use the current entry.
              foundEntry = true;
            }
          }
        }

        if (foundEntry) {
          startNode = entry->mNode;
          startNodeOffset = entry->mNodeOffset + aOffset - entry->mStrOffset;
        }
      }

      if (startNode) {
        mSelStartIndex = static_cast<int32_t>(i);
        mSelStartOffset = aOffset;
      }
    }
  }

  NS_ENSURE_TRUE(startNode, NS_ERROR_FAILURE);

  // XXX: If we ever get a SetSelection() method in nsIEditor, we should
  //      use it.

  nsCOMPtr<nsISelection> selection;

  if (aDoUpdate) {
    nsresult rv =
      mSelCon->GetSelection(nsISelectionController::SELECTION_NORMAL,
                            getter_AddRefs(selection));

    NS_ENSURE_SUCCESS(rv, rv);

    rv = selection->AsSelection()->Collapse(startNode, startNodeOffset);

    NS_ENSURE_SUCCESS(rv, rv);
   }

  if (aLength <= 0) {
    // We have a collapsed selection. (Caret)

    mSelEndIndex  = mSelStartIndex;
    mSelEndOffset = mSelStartOffset;

   //**** KDEBUG ****
   // printf("\n* Sel: (%2d, %4d) (%2d, %4d)\n", mSelStartIndex, mSelStartOffset, mSelEndIndex, mSelEndOffset);
   //**** KDEBUG ****

    return NS_OK;
  }

  // Find the end of the selection in node offset terms:
  nsCOMPtr<nsINode> endNode;
  int32_t endNodeOffset = 0;
  int32_t endOffset = aOffset + aLength;
  for (int32_t i = mOffsetTable.Length() - 1; !endNode && i >= 0; i--) {
    entry = mOffsetTable[i];

    if (entry->mIsValid) {
      if (entry->mIsInsertedText) {
        if (entry->mStrOffset == endNodeOffset) {
          // If the selection ends on an inserted text offset entry,
          // the selection includes the entire entry!

          endNode = entry->mNode;
          endNodeOffset = entry->mNodeOffset + entry->mLength;
        }
      } else if (endOffset >= entry->mStrOffset &&
                 endOffset <= entry->mStrOffset + entry->mLength) {
        endNode = entry->mNode;
        endNodeOffset = entry->mNodeOffset + endOffset - entry->mStrOffset;
      }

      if (endNode) {
        mSelEndIndex = i;
        mSelEndOffset = endOffset;
      }
    }
  }

  if (aDoUpdate && endNode) {
    nsresult rv = selection->AsSelection()->Extend(endNode, endNodeOffset);

    NS_ENSURE_SUCCESS(rv, rv);
  }

  //**** KDEBUG ****
  // printf("\n * Sel: (%2d, %4d) (%2d, %4d)\n", mSelStartIndex, mSelStartOffset, mSelEndIndex, mSelEndOffset);
  //**** KDEBUG ****

  return NS_OK;
}

nsresult
TextServicesDocument::GetSelection(BlockSelectionStatus* aSelStatus,
                                   int32_t* aSelOffset,
                                   int32_t* aSelLength)
{
  NS_ENSURE_TRUE(aSelStatus && aSelOffset && aSelLength, NS_ERROR_NULL_POINTER);

  *aSelStatus = BlockSelectionStatus::eBlockNotFound;
  *aSelOffset = -1;
  *aSelLength = -1;

  NS_ENSURE_TRUE(mDocument && mSelCon, NS_ERROR_FAILURE);

  if (mIteratorStatus == IteratorStatus::eDone) {
    return NS_OK;
  }

  RefPtr<Selection> selection =
    mSelCon->GetDOMSelection(nsISelectionController::SELECTION_NORMAL);
  NS_ENSURE_TRUE(selection, NS_ERROR_FAILURE);

  // XXX: If we expose this method publicly, we need to
  //      add LOCK_DOC/UNLOCK_DOC calls!

  // LOCK_DOC(this);

  nsresult rv;
  if (selection->IsCollapsed()) {
    rv = GetCollapsedSelection(aSelStatus, aSelOffset, aSelLength);
  } else {
    rv = GetUncollapsedSelection(aSelStatus, aSelOffset, aSelLength);
  }

  // UNLOCK_DOC(this);

  // XXX The result of GetCollapsedSelection() or GetUncollapsedSelection().
  return rv;
}

nsresult
TextServicesDocument::GetCollapsedSelection(BlockSelectionStatus* aSelStatus,
                                            int32_t* aSelOffset,
                                            int32_t* aSelLength)
{
  nsCOMPtr<nsISelection> domSelection;
  nsresult rv =
    mSelCon->GetSelection(nsISelectionController::SELECTION_NORMAL,
                          getter_AddRefs(domSelection));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(domSelection, NS_ERROR_FAILURE);

  RefPtr<Selection> selection = domSelection->AsSelection();

  // The calling function should have done the GetIsCollapsed()
  // check already. Just assume it's collapsed!
  *aSelStatus = BlockSelectionStatus::eBlockOutside;
  *aSelOffset = *aSelLength = -1;

  int32_t tableCount = mOffsetTable.Length();

  if (!tableCount) {
    return NS_OK;
  }

  // Get pointers to the first and last offset entries
  // in the table.

  OffsetEntry* eStart = mOffsetTable[0];
  OffsetEntry* eEnd;
  if (tableCount > 1) {
    eEnd = mOffsetTable[tableCount - 1];
  } else {
    eEnd = eStart;
  }

  int32_t eStartOffset = eStart->mNodeOffset;
  int32_t eEndOffset   = eEnd->mNodeOffset + eEnd->mLength;

  RefPtr<nsRange> range = selection->GetRangeAt(0);
  NS_ENSURE_STATE(range);

  nsCOMPtr<nsINode> parent = range->GetStartContainer();
  MOZ_ASSERT(parent);

  uint32_t offset = range->StartOffset();

  int32_t e1s1 = nsContentUtils::ComparePoints(eStart->mNode, eStartOffset,
                                               parent,
                                               static_cast<int32_t>(offset));
  int32_t e2s1 = nsContentUtils::ComparePoints(eEnd->mNode, eEndOffset,
                                               parent,
                                               static_cast<int32_t>(offset));

  if (e1s1 > 0 || e2s1 < 0) {
    // We're done if the caret is outside the current text block.
    return NS_OK;
  }

  if (parent->NodeType() == nsINode::TEXT_NODE) {
    // Good news, the caret is in a text node. Look
    // through the offset table for the entry that
    // matches its parent and offset.

    for (int32_t i = 0; i < tableCount; i++) {
      OffsetEntry* entry = mOffsetTable[i];
      NS_ENSURE_TRUE(entry, NS_ERROR_FAILURE);

      if (entry->mNode == parent &&
          entry->mNodeOffset <= static_cast<int32_t>(offset) &&
          static_cast<int32_t>(offset) <= entry->mNodeOffset + entry->mLength) {
        *aSelStatus = BlockSelectionStatus::eBlockContains;
        *aSelOffset = entry->mStrOffset + (offset - entry->mNodeOffset);
        *aSelLength = 0;

        return NS_OK;
      }
    }

    // If we get here, we didn't find a text node entry
    // in our offset table that matched.

    return NS_ERROR_FAILURE;
  }

  // The caret is in our text block, but it's positioned in some
  // non-text node (ex. <b>). Create a range based on the start
  // and end of the text block, then create an iterator based on
  // this range, with its initial position set to the closest
  // child of this non-text node. Then look for the closest text
  // node.

  rv = nsRange::CreateRange(eStart->mNode, eStartOffset, eEnd->mNode,
                            eEndOffset, getter_AddRefs(range));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIContentIterator> iter;
  rv = CreateContentIterator(range, getter_AddRefs(iter));
  NS_ENSURE_SUCCESS(rv, rv);

  nsIContent* saveNode;
  if (parent->HasChildren()) {
    // XXX: We need to make sure that all of parent's
    //      children are in the text block.

    // If the parent has children, position the iterator
    // on the child that is to the left of the offset.

    nsIContent* content = range->GetChildAtStartOffset();
    if (content && parent->GetFirstChild() != content) {
      content = content->GetPreviousSibling();
    }
    NS_ENSURE_TRUE(content, NS_ERROR_FAILURE);

    rv = iter->PositionAt(content);
    NS_ENSURE_SUCCESS(rv, rv);

    saveNode = content;
  } else {
    // The parent has no children, so position the iterator
    // on the parent.
    NS_ENSURE_TRUE(parent->IsContent(), NS_ERROR_FAILURE);
    nsCOMPtr<nsIContent> content = parent->AsContent();

    rv = iter->PositionAt(content);
    NS_ENSURE_SUCCESS(rv, rv);

    saveNode = content;
  }

  // Now iterate to the left, towards the beginning of
  // the text block, to find the first text node you
  // come across.

  nsIContent* node = nullptr;
  while (!iter->IsDone()) {
    nsINode* current = iter->GetCurrentNode();
    if (current->NodeType() == nsINode::TEXT_NODE) {
      node = current->AsContent();
      break;
    }

    iter->Prev();
  }

  if (node) {
    // We found a node, now set the offset to the end
    // of the text node.
    offset = node->TextLength();
  } else {
    // We should never really get here, but I'm paranoid.

    // We didn't find a text node above, so iterate to
    // the right, towards the end of the text block, looking
    // for a text node.

    rv = iter->PositionAt(saveNode);
    NS_ENSURE_SUCCESS(rv, rv);

    node = nullptr;
    while (!iter->IsDone()) {
      nsINode* current = iter->GetCurrentNode();

      if (current->NodeType() == nsINode::TEXT_NODE) {
        node = current->AsContent();
        break;
      }

      iter->Next();
    }

    NS_ENSURE_TRUE(node, NS_ERROR_FAILURE);

    // We found a text node, so set the offset to
    // the beginning of the node.

    offset = 0;
  }

  for (int32_t i = 0; i < tableCount; i++) {
    OffsetEntry* entry = mOffsetTable[i];
    NS_ENSURE_TRUE(entry, NS_ERROR_FAILURE);

    if (entry->mNode == node &&
        entry->mNodeOffset <= static_cast<int32_t>(offset) &&
        static_cast<int32_t>(offset) <= entry->mNodeOffset + entry->mLength) {
      *aSelStatus = BlockSelectionStatus::eBlockContains;
      *aSelOffset = entry->mStrOffset + (offset - entry->mNodeOffset);
      *aSelLength = 0;

      // Now move the caret so that it is actually in the text node.
      // We do this to keep things in sync.
      //
      // In most cases, the user shouldn't see any movement in the caret
      // on screen.

      return SetSelectionInternal(*aSelOffset, *aSelLength, true);
    }
  }

  return NS_ERROR_FAILURE;
}

nsresult
TextServicesDocument::GetUncollapsedSelection(
                        BlockSelectionStatus* aSelStatus,
                        int32_t* aSelOffset,
                        int32_t* aSelLength)
{
  RefPtr<nsRange> range;
  OffsetEntry *entry;

  nsCOMPtr<nsISelection> domSelection;
  nsresult rv = mSelCon->GetSelection(nsISelectionController::SELECTION_NORMAL,
                                      getter_AddRefs(domSelection));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(domSelection, NS_ERROR_FAILURE);

  RefPtr<Selection> selection = domSelection->AsSelection();

  // It is assumed that the calling function has made sure that the
  // selection is not collapsed, and that the input params to this
  // method are initialized to some defaults.

  nsCOMPtr<nsINode> startContainer, endContainer;
  int32_t startOffset, endOffset;
  int32_t tableCount;
  int32_t e1s1 = 0, e1s2 = 0, e2s1 = 0, e2s2 = 0;

  OffsetEntry *eStart, *eEnd;
  int32_t eStartOffset, eEndOffset;

  tableCount = mOffsetTable.Length();

  // Get pointers to the first and last offset entries
  // in the table.

  eStart = mOffsetTable[0];

  if (tableCount > 1) {
    eEnd = mOffsetTable[tableCount - 1];
  } else {
    eEnd = eStart;
  }

  eStartOffset = eStart->mNodeOffset;
  eEndOffset   = eEnd->mNodeOffset + eEnd->mLength;

  uint32_t rangeCount = selection->RangeCount();

  // Find the first range in the selection that intersects
  // the current text block.

  for (uint32_t i = 0; i < rangeCount; i++) {
    range = selection->GetRangeAt(i);
    NS_ENSURE_STATE(range);

    rv = GetRangeEndPoints(range,
                           getter_AddRefs(startContainer), &startOffset,
                           getter_AddRefs(endContainer), &endOffset);

    NS_ENSURE_SUCCESS(rv, rv);

    e1s2 = nsContentUtils::ComparePoints(eStart->mNode, eStartOffset,
                                         endContainer, endOffset);
    e2s1 = nsContentUtils::ComparePoints(eEnd->mNode, eEndOffset,
                                         startContainer, startOffset);

    // Break out of the loop if the text block intersects the current range.

    if (e1s2 <= 0 && e2s1 >= 0) {
      break;
    }
  }

  // We're done if we didn't find an intersecting range.

  if (rangeCount < 1 || e1s2 > 0 || e2s1 < 0) {
    *aSelStatus = BlockSelectionStatus::eBlockOutside;
    *aSelOffset = *aSelLength = -1;
    return NS_OK;
  }

  // Now that we have an intersecting range, find out more info:

  e1s1 = nsContentUtils::ComparePoints(eStart->mNode, eStartOffset,
                                       startContainer, startOffset);
  e2s2 = nsContentUtils::ComparePoints(eEnd->mNode, eEndOffset,
                                       endContainer, endOffset);

  if (rangeCount > 1) {
    // There are multiple selection ranges, we only deal
    // with the first one that intersects the current,
    // text block, so mark this a as a partial.
    *aSelStatus = BlockSelectionStatus::eBlockPartial;
  } else if (e1s1 > 0 && e2s2 < 0) {
    // The range extends beyond the start and
    // end of the current text block.
    *aSelStatus = BlockSelectionStatus::eBlockInside;
  } else if (e1s1 <= 0 && e2s2 >= 0) {
    // The current text block contains the entire
    // range.
    *aSelStatus = BlockSelectionStatus::eBlockContains;
  } else {
    // The range partially intersects the block.
    *aSelStatus = BlockSelectionStatus::eBlockPartial;
  }

  // Now create a range based on the intersection of the
  // text block and range:

  nsCOMPtr<nsINode> p1, p2;
  int32_t     o1,  o2;

  // The start of the range will be the rightmost
  // start node.

  if (e1s1 >= 0) {
    p1 = eStart->mNode;
    o1 = eStartOffset;
  } else {
    p1 = startContainer;
    o1 = startOffset;
  }

  // The end of the range will be the leftmost
  // end node.

  if (e2s2 <= 0) {
    p2 = eEnd->mNode;
    o2 = eEndOffset;
  } else {
    p2 = endContainer;
    o2 = endOffset;
  }

  rv = nsRange::CreateRange(p1, o1, p2, o2, getter_AddRefs(range));

  NS_ENSURE_SUCCESS(rv, rv);

  // Now iterate over this range to figure out the selection's
  // block offset and length.

  nsCOMPtr<nsIContentIterator> iter;

  rv = CreateContentIterator(range, getter_AddRefs(iter));

  NS_ENSURE_SUCCESS(rv, rv);

  // Find the first text node in the range.

  bool found;
  nsCOMPtr<nsIContent> content;

  iter->First();

  if (!p1->IsText()) {
    found = false;

    while (!iter->IsDone()) {
      nsINode* node = iter->GetCurrentNode();

      if (node->IsText()) {
        p1 = node;
        o1 = 0;
        found = true;

        break;
      }

      iter->Next();
    }

    NS_ENSURE_TRUE(found, NS_ERROR_FAILURE);
  }

  // Find the last text node in the range.

  iter->Last();

  if (!p2->IsText()) {
    found = false;
    while (!iter->IsDone()) {
      nsINode* node = iter->GetCurrentNode();
      if (node->IsText()) {
        p2 = node;
        o2 = p2->Length();
        found = true;

        break;
      }

      iter->Prev();
    }

    NS_ENSURE_TRUE(found, NS_ERROR_FAILURE);
  }

  found    = false;
  *aSelLength = 0;

  for (int32_t i = 0; i < tableCount; i++) {
    entry = mOffsetTable[i];
    NS_ENSURE_TRUE(entry, NS_ERROR_FAILURE);
    if (!found) {
      if (entry->mNode == p1.get() &&
          entry->mNodeOffset <= o1 &&
          o1 <= entry->mNodeOffset + entry->mLength) {
        *aSelOffset = entry->mStrOffset + (o1 - entry->mNodeOffset);
        if (p1 == p2 &&
            entry->mNodeOffset <= o2 &&
            o2 <= entry->mNodeOffset + entry->mLength) {
          // The start and end of the range are in the same offset
          // entry. Calculate the length of the range then we're done.
          *aSelLength = o2 - o1;
          break;
        }
        // Add the length of the sub string in this offset entry
        // that follows the start of the range.
        *aSelLength = entry->mLength - (o1 - entry->mNodeOffset);
        found = true;
      }
    } else { // Found.
      if (entry->mNode == p2.get() &&
          entry->mNodeOffset <= o2 &&
          o2 <= entry->mNodeOffset + entry->mLength) {
        // We found the end of the range. Calculate the length of the
        // sub string that is before the end of the range, then we're done.
        *aSelLength += o2 - entry->mNodeOffset;
        break;
      }
      // The entire entry must be in the range.
      *aSelLength += entry->mLength;
    }
  }

  return NS_OK;
}

bool
TextServicesDocument::SelectionIsCollapsed()
{
  return mSelStartIndex == mSelEndIndex && mSelStartOffset == mSelEndOffset;
}

bool
TextServicesDocument::SelectionIsValid()
{
  return mSelStartIndex >= 0;
}

// static
nsresult
TextServicesDocument::GetRangeEndPoints(nsRange* aRange,
                                        nsINode** aStartContainer,
                                        int32_t* aStartOffset,
                                        nsINode** aEndContainer,
                                        int32_t* aEndOffset)
{
  NS_ENSURE_TRUE(aRange && aStartContainer && aStartOffset &&
                 aEndContainer && aEndOffset, NS_ERROR_NULL_POINTER);

  NS_IF_ADDREF(*aStartContainer = aRange->GetStartContainer());
  NS_ENSURE_TRUE(aStartContainer, NS_ERROR_FAILURE);

  *aStartOffset = static_cast<int32_t>(aRange->StartOffset());

  NS_IF_ADDREF(*aEndContainer = aRange->GetEndContainer());
  NS_ENSURE_TRUE(aEndContainer, NS_ERROR_FAILURE);

  *aEndOffset = static_cast<int32_t>(aRange->EndOffset());
  return NS_OK;
}

// static
nsresult
TextServicesDocument::FirstTextNode(nsIContentIterator* aIterator,
                                    IteratorStatus* aIteratorStatus)
{
  if (aIteratorStatus) {
    *aIteratorStatus = IteratorStatus::eDone;
  }

  aIterator->First();

  while (!aIterator->IsDone()) {
    if (aIterator->GetCurrentNode()->NodeType() == nsINode::TEXT_NODE) {
      if (aIteratorStatus) {
        *aIteratorStatus = IteratorStatus::eValid;
      }
      break;
    }
    aIterator->Next();
  }

  return NS_OK;
}

// static
nsresult
TextServicesDocument::LastTextNode(nsIContentIterator* aIterator,
                                   IteratorStatus* aIteratorStatus)
{
  if (aIteratorStatus) {
    *aIteratorStatus = IteratorStatus::eDone;
  }

  aIterator->Last();

  while (!aIterator->IsDone()) {
    if (aIterator->GetCurrentNode()->NodeType() == nsINode::TEXT_NODE) {
      if (aIteratorStatus) {
        *aIteratorStatus = IteratorStatus::eValid;
      }
      break;
    }
    aIterator->Prev();
  }

  return NS_OK;
}

// static
nsresult
TextServicesDocument::FirstTextNodeInCurrentBlock(nsIContentIterator* aIter)
{
  NS_ENSURE_TRUE(aIter, NS_ERROR_NULL_POINTER);

  ClearDidSkip(aIter);

  nsCOMPtr<nsIContent> last;

  // Walk backwards over adjacent text nodes until
  // we hit a block boundary:

  while (!aIter->IsDone()) {
    nsCOMPtr<nsIContent> content =
      aIter->GetCurrentNode()->IsContent() ?
        aIter->GetCurrentNode()->AsContent() : nullptr;
    if (last && IsBlockNode(content)) {
      break;
    }
    if (IsTextNode(content)) {
      if (last && !HasSameBlockNodeParent(content, last)) {
        // We're done, the current text node is in a
        // different block.
        break;
      }
      last = content;
    }

    aIter->Prev();

    if (DidSkip(aIter)) {
      break;
    }
  }

  if (last) {
    aIter->PositionAt(last);
  }

  // XXX: What should we return if last is null?

  return NS_OK;
}

// static
nsresult
TextServicesDocument::FirstTextNodeInPrevBlock(nsIContentIterator* aIterator)
{
  NS_ENSURE_TRUE(aIterator, NS_ERROR_NULL_POINTER);

  // XXX: What if mIterator is not currently on a text node?

  // Make sure mIterator is pointing to the first text node in the
  // current block:

  nsresult rv = FirstTextNodeInCurrentBlock(aIterator);

  NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

  // Point mIterator to the first node before the first text node:

  aIterator->Prev();

  if (aIterator->IsDone()) {
    return NS_ERROR_FAILURE;
  }

  // Now find the first text node of the next block:

  return FirstTextNodeInCurrentBlock(aIterator);
}

// static
nsresult
TextServicesDocument::FirstTextNodeInNextBlock(nsIContentIterator* aIterator)
{
  nsCOMPtr<nsIContent> prev;
  bool crossedBlockBoundary = false;

  NS_ENSURE_TRUE(aIterator, NS_ERROR_NULL_POINTER);

  ClearDidSkip(aIterator);

  while (!aIterator->IsDone()) {
    nsCOMPtr<nsIContent> content = aIterator->GetCurrentNode()->IsContent()
                                   ? aIterator->GetCurrentNode()->AsContent()
                                   : nullptr;

    if (IsTextNode(content)) {
      if (crossedBlockBoundary ||
          (prev && !HasSameBlockNodeParent(prev, content))) {
        break;
      }
      prev = content;
    } else if (!crossedBlockBoundary && IsBlockNode(content)) {
      crossedBlockBoundary = true;
    }

    aIterator->Next();

    if (!crossedBlockBoundary && DidSkip(aIterator)) {
      crossedBlockBoundary = true;
    }
  }

  return NS_OK;
}

nsresult
TextServicesDocument::GetFirstTextNodeInPrevBlock(nsIContent** aContent)
{
  NS_ENSURE_TRUE(aContent, NS_ERROR_NULL_POINTER);

  *aContent = 0;

  // Save the iterator's current content node so we can restore
  // it when we are done:

  nsINode* node = mIterator->GetCurrentNode();

  nsresult rv = FirstTextNodeInPrevBlock(mIterator);

  if (NS_FAILED(rv)) {
    // Try to restore the iterator before returning.
    mIterator->PositionAt(node);
    return rv;
  }

  if (!mIterator->IsDone()) {
    nsCOMPtr<nsIContent> current = mIterator->GetCurrentNode()->IsContent()
                                   ? mIterator->GetCurrentNode()->AsContent()
                                   : nullptr;
    current.forget(aContent);
  }

  // Restore the iterator:

  return mIterator->PositionAt(node);
}

nsresult
TextServicesDocument::GetFirstTextNodeInNextBlock(nsIContent** aContent)
{
  NS_ENSURE_TRUE(aContent, NS_ERROR_NULL_POINTER);

  *aContent = 0;

  // Save the iterator's current content node so we can restore
  // it when we are done:

  nsINode* node = mIterator->GetCurrentNode();

  nsresult rv = FirstTextNodeInNextBlock(mIterator);

  if (NS_FAILED(rv)) {
    // Try to restore the iterator before returning.
    mIterator->PositionAt(node);
    return rv;
  }

  if (!mIterator->IsDone()) {
    nsCOMPtr<nsIContent> current = mIterator->GetCurrentNode()->IsContent()
                                   ? mIterator->GetCurrentNode()->AsContent()
                                   : nullptr;
    current.forget(aContent);
  }

  // Restore the iterator:
  return mIterator->PositionAt(node);
}

nsresult
TextServicesDocument::CreateOffsetTable(nsTArray<OffsetEntry*>* aOffsetTable,
                                        nsIContentIterator* aIterator,
                                        IteratorStatus* aIteratorStatus,
                                        nsRange* aIterRange,
                                        nsString* aStr)
{
  nsCOMPtr<nsIContent> first;
  nsCOMPtr<nsIContent> prev;

  NS_ENSURE_TRUE(aIterator, NS_ERROR_NULL_POINTER);

  ClearOffsetTable(aOffsetTable);

  if (aStr) {
    aStr->Truncate();
  }

  if (*aIteratorStatus == IteratorStatus::eDone) {
    return NS_OK;
  }

  // If we have an aIterRange, retrieve the endpoints so
  // they can be used in the while loop below to trim entries
  // for text nodes that are partially selected by aIterRange.

  nsCOMPtr<nsINode> rngStartNode, rngEndNode;
  int32_t rngStartOffset = 0, rngEndOffset = 0;

  if (aIterRange) {
    nsresult rv =
      GetRangeEndPoints(aIterRange,
                        getter_AddRefs(rngStartNode), &rngStartOffset,
                        getter_AddRefs(rngEndNode), &rngEndOffset);

    NS_ENSURE_SUCCESS(rv, rv);
  }

  // The text service could have added text nodes to the beginning
  // of the current block and called this method again. Make sure
  // we really are at the beginning of the current block:

  nsresult rv = FirstTextNodeInCurrentBlock(aIterator);

  NS_ENSURE_SUCCESS(rv, rv);

  int32_t offset = 0;

  ClearDidSkip(aIterator);

  while (!aIterator->IsDone()) {
    nsCOMPtr<nsIContent> content = aIterator->GetCurrentNode()->IsContent()
                                   ? aIterator->GetCurrentNode()->AsContent()
                                   : nullptr;
    if (IsTextNode(content)) {
      if (prev && !HasSameBlockNodeParent(prev, content)) {
        break;
      }

      nsString str;
      content->GetNodeValue(str);

      // Add an entry for this text node into the offset table:

      OffsetEntry *entry = new OffsetEntry(content, offset, str.Length());
      aOffsetTable->AppendElement(entry);

      // If one or both of the endpoints of the iteration range
      // are in the text node for this entry, make sure the entry
      // only accounts for the portion of the text node that is
      // in the range.

      int32_t startOffset = 0;
      int32_t endOffset   = str.Length();
      bool adjustStr    = false;

      if (entry->mNode == rngStartNode) {
        entry->mNodeOffset = startOffset = rngStartOffset;
        adjustStr = true;
      }

      if (entry->mNode == rngEndNode) {
        endOffset = rngEndOffset;
        adjustStr = true;
      }

      if (adjustStr) {
        entry->mLength = endOffset - startOffset;
        str = Substring(str, startOffset, entry->mLength);
      }

      offset += str.Length();

      if (aStr) {
        // Append the text node's string to the output string:
        if (!first) {
          *aStr = str;
        } else {
          *aStr += str;
        }
      }

      prev = content;

      if (!first) {
        first = content;
      }
    }
    // XXX This should be checked before IsTextNode(), but IsBlockNode() returns
    //     true even if content is a text node.  See bug 1311934.
    else if (IsBlockNode(content)) {
      break;
    }

    aIterator->Next();

    if (DidSkip(aIterator)) {
      break;
    }
  }

  if (first) {
    // Always leave the iterator pointing at the first
    // text node of the current block!
    aIterator->PositionAt(first);
  } else {
    // If we never ran across a text node, the iterator
    // might have been pointing to something invalid to
    // begin with.
    *aIteratorStatus = IteratorStatus::eDone;
  }

  return NS_OK;
}

nsresult
TextServicesDocument::RemoveInvalidOffsetEntries()
{
  for (size_t i = 0; i < mOffsetTable.Length(); ) {
    OffsetEntry* entry = mOffsetTable[i];
    if (!entry->mIsValid) {
      mOffsetTable.RemoveElementAt(i);
      if (mSelStartIndex >= 0 && static_cast<size_t>(mSelStartIndex) >= i) {
        // We are deleting an entry that comes before
        // mSelStartIndex, decrement mSelStartIndex so
        // that it points to the correct entry!

        NS_ASSERTION(i != static_cast<size_t>(mSelStartIndex),
                     "Invalid selection index.");

        --mSelStartIndex;
        --mSelEndIndex;
      }
    } else {
      i++;
    }
  }

  return NS_OK;
}

// static
nsresult
TextServicesDocument::ClearOffsetTable(nsTArray<OffsetEntry*>* aOffsetTable)
{
  for (size_t i = 0; i < aOffsetTable->Length(); i++) {
    delete aOffsetTable->ElementAt(i);
  }

  aOffsetTable->Clear();

  return NS_OK;
}

nsresult
TextServicesDocument::SplitOffsetEntry(int32_t aTableIndex,
                                       int32_t aNewEntryLength)
{
  OffsetEntry *entry = mOffsetTable[aTableIndex];

  NS_ASSERTION((aNewEntryLength > 0), "aNewEntryLength <= 0");
  NS_ASSERTION((aNewEntryLength < entry->mLength), "aNewEntryLength >= mLength");

  if (aNewEntryLength < 1 || aNewEntryLength >= entry->mLength) {
    return NS_ERROR_FAILURE;
  }

  int32_t oldLength = entry->mLength - aNewEntryLength;

  OffsetEntry *newEntry = new OffsetEntry(entry->mNode,
                                          entry->mStrOffset + oldLength,
                                          aNewEntryLength);

  if (!mOffsetTable.InsertElementAt(aTableIndex + 1, newEntry)) {
    delete newEntry;
    return NS_ERROR_FAILURE;
  }

   // Adjust entry fields:

   entry->mLength        = oldLength;
   newEntry->mNodeOffset = entry->mNodeOffset + oldLength;

  return NS_OK;
}

// static
nsresult
TextServicesDocument::NodeHasOffsetEntry(nsTArray<OffsetEntry*>* aOffsetTable,
                                         nsINode* aNode,
                                         bool* aHasEntry,
                                         int32_t* aEntryIndex)
{
  NS_ENSURE_TRUE(aNode && aHasEntry && aEntryIndex, NS_ERROR_NULL_POINTER);

  for (size_t i = 0; i < aOffsetTable->Length(); i++) {
    OffsetEntry* entry = (*aOffsetTable)[i];

    NS_ENSURE_TRUE(entry, NS_ERROR_FAILURE);

    if (entry->mNode == aNode) {
      *aHasEntry   = true;
      *aEntryIndex = i;
      return NS_OK;
    }
  }

  *aHasEntry   = false;
  *aEntryIndex = -1;
  return NS_OK;
}

// Spellchecker code has this. See bug 211343
#define IS_NBSP_CHAR(c) (((unsigned char)0xa0)==(c))

// static
nsresult
TextServicesDocument::FindWordBounds(nsTArray<OffsetEntry*>* aOffsetTable,
                                     nsString* aBlockStr,
                                     nsINode* aNode,
                                     int32_t aNodeOffset,
                                     nsINode** aWordStartNode,
                                     int32_t* aWordStartOffset,
                                     nsINode** aWordEndNode,
                                     int32_t* aWordEndOffset)
{
  // Initialize return values.

  if (aWordStartNode) {
    *aWordStartNode = nullptr;
  }
  if (aWordStartOffset) {
    *aWordStartOffset = 0;
  }
  if (aWordEndNode) {
    *aWordEndNode = nullptr;
  }
  if (aWordEndOffset) {
    *aWordEndOffset = 0;
  }

  int32_t entryIndex = 0;
  bool hasEntry = false;

  // It's assumed that aNode is a text node. The first thing
  // we do is get its index in the offset table so we can
  // calculate the dom point's string offset.

  nsresult rv = NodeHasOffsetEntry(aOffsetTable, aNode, &hasEntry, &entryIndex);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(hasEntry, NS_ERROR_FAILURE);

  // Next we map aNodeOffset into a string offset.

  OffsetEntry *entry = (*aOffsetTable)[entryIndex];
  uint32_t strOffset = entry->mStrOffset + aNodeOffset - entry->mNodeOffset;

  // Now we use the word breaker to find the beginning and end
  // of the word from our calculated string offset.

  const char16_t *str = aBlockStr->get();
  uint32_t strLen = aBlockStr->Length();

  mozilla::intl::WordBreaker* wordBreaker = nsContentUtils::WordBreaker();
  mozilla::intl::WordRange res = wordBreaker->FindWord(str, strLen, strOffset);
  if (res.mBegin > strLen) {
    return str ? NS_ERROR_ILLEGAL_VALUE : NS_ERROR_NULL_POINTER;
  }

  // Strip out the NBSPs at the ends
  while (res.mBegin <= res.mEnd && IS_NBSP_CHAR(str[res.mBegin])) {
    res.mBegin++;
  }
  if (str[res.mEnd] == (unsigned char)0x20) {
    uint32_t realEndWord = res.mEnd - 1;
    while (realEndWord > res.mBegin && IS_NBSP_CHAR(str[realEndWord])) {
      realEndWord--;
    }
    if (realEndWord < res.mEnd - 1) {
      res.mEnd = realEndWord + 1;
    }
  }

  // Now that we have the string offsets for the beginning
  // and end of the word, run through the offset table and
  // convert them back into dom points.

  size_t lastIndex = aOffsetTable->Length() - 1;
  for (size_t i = 0; i <= lastIndex; i++) {
    entry = (*aOffsetTable)[i];

    int32_t strEndOffset = entry->mStrOffset + entry->mLength;

    // Check to see if res.mBegin is within the range covered
    // by this entry. Note that if res.mBegin is after the last
    // character covered by this entry, we will use the next
    // entry if there is one.

    if (uint32_t(entry->mStrOffset) <= res.mBegin &&
        (res.mBegin < static_cast<uint32_t>(strEndOffset) ||
         (res.mBegin == static_cast<uint32_t>(strEndOffset) &&
          i == lastIndex))) {
      if (aWordStartNode) {
        *aWordStartNode = entry->mNode;
        NS_IF_ADDREF(*aWordStartNode);
      }

      if (aWordStartOffset) {
        *aWordStartOffset = entry->mNodeOffset + res.mBegin - entry->mStrOffset;
      }

      if (!aWordEndNode && !aWordEndOffset) {
        // We've found our start entry, but if we're not looking
        // for end entries, we're done.
        break;
      }
    }

    // Check to see if res.mEnd is within the range covered
    // by this entry.

    if (static_cast<uint32_t>(entry->mStrOffset) <= res.mEnd &&
        res.mEnd <= static_cast<uint32_t>(strEndOffset)) {
      if (res.mBegin == res.mEnd &&
          res.mEnd == static_cast<uint32_t>(strEndOffset) &&
          i != lastIndex) {
        // Wait for the next round so that we use the same entry
        // we did for aWordStartNode.
        continue;
      }

      if (aWordEndNode) {
        *aWordEndNode = entry->mNode;
        NS_IF_ADDREF(*aWordEndNode);
      }

      if (aWordEndOffset) {
        *aWordEndOffset = entry->mNodeOffset + res.mEnd - entry->mStrOffset;
      }
      break;
    }
  }


  return NS_OK;
}

/**
 * nsIEditActionListener implementation:
 *   Don't implement the behavior directly here.  The methods won't be called
 *   if the instance is created for inline spell checker created for editor.
 *   If you need to listen a new edit action, you need to add similar
 *   non-virtual method and you need to call it from EditorBase directly.
 */

NS_IMETHODIMP
TextServicesDocument::DidInsertNode(nsIDOMNode* aNode,
                                    nsresult aResult)
{
  return NS_OK;
}

NS_IMETHODIMP
TextServicesDocument::DidDeleteNode(nsIDOMNode* aChild,
                                    nsresult aResult)
{
  if (NS_WARN_IF(NS_FAILED(aResult))) {
    return NS_OK;
  }
  nsCOMPtr<nsINode> child = do_QueryInterface(aChild);
  DidDeleteNode(child);
  return NS_OK;
}

NS_IMETHODIMP
TextServicesDocument::DidSplitNode(nsIDOMNode* aExistingRightNode,
                                   nsIDOMNode* aNewLeftNode)
{
  return NS_OK;
}

NS_IMETHODIMP
TextServicesDocument::DidJoinNodes(nsIDOMNode* aLeftNode,
                                   nsIDOMNode* aRightNode,
                                   nsIDOMNode* aParent,
                                   nsresult aResult)
{
  if (NS_WARN_IF(NS_FAILED(aResult))) {
    return NS_OK;
  }
  nsCOMPtr<nsINode> leftNode = do_QueryInterface(aLeftNode);
  nsCOMPtr<nsINode> rightNode = do_QueryInterface(aRightNode);
  if (NS_WARN_IF(!leftNode) || NS_WARN_IF(!rightNode)) {
    return NS_OK;
  }
  DidJoinNodes(*leftNode, *rightNode);
  return NS_OK;
}

NS_IMETHODIMP
TextServicesDocument::DidCreateNode(const nsAString& aTag,
                                    nsIDOMNode* aNewNode,
                                    nsresult aResult)
{
  return NS_OK;
}

NS_IMETHODIMP
TextServicesDocument::DidInsertText(CharacterData* aTextNode,
                                    int32_t aOffset,
                                    const nsAString& aString,
                                    nsresult aResult)
{
  return NS_OK;
}

NS_IMETHODIMP
TextServicesDocument::WillDeleteText(CharacterData* aTextNode,
                                     int32_t aOffset,
                                     int32_t aLength)
{
  return NS_OK;
}

NS_IMETHODIMP
TextServicesDocument::DidDeleteText(CharacterData* aTextNode,
                                    int32_t aOffset,
                                    int32_t aLength,
                                    nsresult aResult)
{
  return NS_OK;
}

NS_IMETHODIMP
TextServicesDocument::WillDeleteSelection(nsISelection* aSelection)
{
  return NS_OK;
}

NS_IMETHODIMP
TextServicesDocument::DidDeleteSelection(nsISelection* aSelection)
{
  return NS_OK;
}

} // namespace mozilla
