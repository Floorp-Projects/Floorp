/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nscore.h"
#include "nsLayoutCID.h"
#include "nsIAtom.h"
#include "nsStaticAtom.h"
#include "nsString.h"
#include "nsIEnumerator.h"
#include "nsIContent.h"
#include "nsIContentIterator.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMRange.h"
#include "nsIRangeUtils.h"
#include "nsISelection.h"
#include "nsIPlaintextEditor.h"
#include "nsTextServicesDocument.h"
#include "nsFilteredContentIterator.h"

#include "nsIDOMElement.h"
#include "nsIDOMHTMLElement.h"
#include "nsIDOMHTMLDocument.h"

#include "nsIWordBreakerFactory.h"
#include "nsLWBrkCIID.h"
#include "nsIServiceManager.h"

#define LOCK_DOC(doc)
#define UNLOCK_DOC(doc)


class OffsetEntry
{
public:
  OffsetEntry(nsIDOMNode *aNode, PRInt32 aOffset, PRInt32 aLength)
    : mNode(aNode), mNodeOffset(0), mStrOffset(aOffset), mLength(aLength),
      mIsInsertedText(PR_FALSE), mIsValid(PR_TRUE)
  {
    if (mStrOffset < 1)
      mStrOffset = 0;

    if (mLength < 1)
      mLength = 0;
  }

  virtual ~OffsetEntry()
  {
    mNode       = 0;
    mNodeOffset = 0;
    mStrOffset  = 0;
    mLength     = 0;
    mIsValid    = PR_FALSE;
  }

  nsIDOMNode *mNode;
  PRInt32 mNodeOffset;
  PRInt32 mStrOffset;
  PRInt32 mLength;
  PRBool  mIsInsertedText;
  PRBool  mIsValid;
};

#define TS_ATOM(name_, value_) nsIAtom* nsTextServicesDocument::name_ = 0;
#include "nsTSAtomList.h"
#undef TS_ATOM

nsIRangeUtils* nsTextServicesDocument::sRangeHelper;

nsTextServicesDocument::nsTextServicesDocument()
{
  mRefCnt         = 0;

  mSelStartIndex  = -1;
  mSelStartOffset = -1;
  mSelEndIndex    = -1;
  mSelEndOffset   = -1;

  mIteratorStatus = eIsDone;
}

nsTextServicesDocument::~nsTextServicesDocument()
{
  if (mEditor && mNotifier)
    mEditor->RemoveEditActionListener(mNotifier);

  ClearOffsetTable(&mOffsetTable);
}

/* static */
void
nsTextServicesDocument::RegisterAtoms()
{
  static const nsStaticAtom ts_atoms[] = {
#define TS_ATOM(name_, value_) { value_, &name_ },
#include "nsTSAtomList.h"
#undef TS_ATOM
  };

  NS_RegisterStaticAtoms(ts_atoms, NS_ARRAY_LENGTH(ts_atoms));
}

/* static */
void
nsTextServicesDocument::Shutdown()
{
  NS_IF_RELEASE(sRangeHelper);
}

#define DEBUG_TEXT_SERVICES__DOCUMENT_REFCNT 1

#ifdef DEBUG_TEXT_SERVICES__DOCUMENT_REFCNT

nsrefcnt nsTextServicesDocument::AddRef(void)
{
  return ++mRefCnt;
}

nsrefcnt nsTextServicesDocument::Release(void)
{
  NS_PRECONDITION(0 != mRefCnt, "dup release");
  if (--mRefCnt == 0) {
    NS_DELETEXPCOM(this);
    return 0;
  }
  return mRefCnt;
}

#else

NS_IMPL_ADDREF(nsTextServicesDocument)
NS_IMPL_RELEASE(nsTextServicesDocument)

#endif

NS_IMPL_QUERY_INTERFACE1(nsTextServicesDocument, nsITextServicesDocument)

NS_IMETHODIMP
nsTextServicesDocument::InitWithDocument(nsIDOMDocument *aDOMDocument, nsIPresShell *aPresShell)
{
  nsresult result = NS_OK;

  if (!aDOMDocument || !aPresShell)
    return NS_ERROR_NULL_POINTER;

  NS_ASSERTION(!mSelCon, "mSelCon already initialized!");

  if (mSelCon)
    return NS_ERROR_FAILURE;

  NS_ASSERTION(!mDOMDocument, "mDOMDocument already initialized!");

  if (mDOMDocument)
    return NS_ERROR_FAILURE;

  LOCK_DOC(this);

  mSelCon   = do_QueryInterface(aPresShell);
  mDOMDocument = do_QueryInterface(aDOMDocument);

  result = CreateDocumentContentIterator(getter_AddRefs(mIterator));

  if (NS_FAILED(result))
  {
    UNLOCK_DOC(this);
    return result;
  }

  mIteratorStatus = nsTextServicesDocument::eIsDone;

  result = FirstBlock();

  UNLOCK_DOC(this);

  return result;
}

NS_IMETHODIMP
nsTextServicesDocument::InitWithEditor(nsIEditor *aEditor)
{
  nsresult result = NS_OK;
  nsCOMPtr<nsISelectionController> selCon;
  nsCOMPtr<nsIDOMDocument> doc;

  if (!aEditor)
    return NS_ERROR_NULL_POINTER;

  LOCK_DOC(this);

  // Check to see if we already have an mSelCon. If we do, it
  // better be the same one the editor uses!

  result = aEditor->GetSelectionController(getter_AddRefs(selCon));

  if (NS_FAILED(result))
  {
    UNLOCK_DOC(this);
    return result;
  }

  if (!selCon || (mSelCon && selCon != mSelCon))
  {
    UNLOCK_DOC(this);
    return NS_ERROR_FAILURE;
  }

  if (!mSelCon)
    mSelCon = selCon;

  // Check to see if we already have an mDOMDocument. If we do, it
  // better be the same one the editor uses!

  result = aEditor->GetDocument(getter_AddRefs(doc));

  if (NS_FAILED(result))
  {
    UNLOCK_DOC(this);
    return result;
  }

  if (!doc || (mDOMDocument && doc != mDOMDocument))
  {
    UNLOCK_DOC(this);
    return NS_ERROR_FAILURE;
  }

  if (!mDOMDocument)
  {
    mDOMDocument = doc;

    result = CreateDocumentContentIterator(getter_AddRefs(mIterator));

    if (NS_FAILED(result))
    {
      UNLOCK_DOC(this);
      return result;
    }

    mIteratorStatus = nsTextServicesDocument::eIsDone;

    result = FirstBlock();

    if (NS_FAILED(result))
    {
      UNLOCK_DOC(this);
      return result;
    }
  }

  mEditor = do_QueryInterface(aEditor);

  nsTSDNotifier *notifier = new nsTSDNotifier(this);

  if (!notifier)
  {
    UNLOCK_DOC(this);
    return NS_ERROR_OUT_OF_MEMORY;
  }

  mNotifier = do_QueryInterface(notifier);

  result = mEditor->AddEditActionListener(mNotifier);

  UNLOCK_DOC(this);

  return result;
}

NS_IMETHODIMP 
nsTextServicesDocument::GetDocument(nsIDOMDocument **aDoc)
{
  if (!aDoc)
    return NS_ERROR_NULL_POINTER;

  *aDoc = nsnull; // init out param
  if (!mDOMDocument)
    return NS_ERROR_NOT_INITIALIZED;

  *aDoc = mDOMDocument;
  NS_ADDREF(*aDoc);

  return NS_OK;
}

NS_IMETHODIMP
nsTextServicesDocument::SetExtent(nsIDOMRange* aDOMRange)
{
  NS_ENSURE_ARG_POINTER(aDOMRange);
  NS_ENSURE_TRUE(mDOMDocument, NS_ERROR_FAILURE);
  NS_ENSURE_TRUE(mEditor, NS_ERROR_FAILURE);
  NS_ENSURE_TRUE(mNotifier, NS_ERROR_FAILURE);

  LOCK_DOC(this);

  // We need to store a copy of aDOMRange since we don't
  // know where it came from.

  nsresult result = aDOMRange->CloneRange(getter_AddRefs(mExtent));

  if (NS_FAILED(result))
  {
    UNLOCK_DOC(this);
    return result;
  }

  // Create a new iterator based on our new extent range.

  result = CreateContentIterator(mExtent, getter_AddRefs(mIterator));

  if (NS_FAILED(result))
  {
    UNLOCK_DOC(this);
    return result;
  }

  // Now position the iterator at the start of the first block
  // in the range.

  mIteratorStatus = nsTextServicesDocument::eIsDone;

  result = FirstBlock();

  UNLOCK_DOC(this);

  return result;
}

NS_IMETHODIMP
nsTextServicesDocument::GetExtent(nsIDOMRange** aDOMRange)
{
  NS_ENSURE_ARG_POINTER(aDOMRange);

  *aDOMRange = nsnull;

  if (mExtent)
  {
    // We have an extent range, so just return a
    // copy of it.

    return mExtent->CloneRange(aDOMRange);
  }

  // We don't have an extent range, so we must be
  // iterating over the entire document.

  return CreateDocumentContentRange(aDOMRange);
}

NS_IMETHODIMP
nsTextServicesDocument::ExpandRangeToWordBoundaries(nsIDOMRange *aRange)
{
  NS_ENSURE_ARG_POINTER(aRange);

  // Get the end points of the range.

  nsCOMPtr<nsIDOMNode> rngStartNode, rngEndNode;
  PRInt32 rngStartOffset, rngEndOffset;

  nsresult result =  GetRangeEndPoints(aRange,
                                       getter_AddRefs(rngStartNode),
                                       &rngStartOffset,
                                       getter_AddRefs(rngEndNode),
                                       &rngEndOffset);

  NS_ENSURE_SUCCESS(result, result);

  // Create a content iterator based on the range.

  nsCOMPtr<nsIContentIterator> iter;
  result = CreateContentIterator(aRange, getter_AddRefs(iter));

  NS_ENSURE_SUCCESS(result, result);

  // Find the first text node in the range.

  TSDIteratorStatus iterStatus;

  result = FirstTextNode(iter, &iterStatus);
  NS_ENSURE_SUCCESS(result, result);

  if (iterStatus == nsTextServicesDocument::eIsDone)
  {
    // No text was found so there's no adjustment necessary!
    return NS_OK;
  }

  nsIContent *firstTextContent = iter->GetCurrentNode();
  NS_ENSURE_TRUE(firstTextContent, NS_ERROR_FAILURE);

  // Find the last text node in the range.

  result = LastTextNode(iter, &iterStatus);
  NS_ENSURE_SUCCESS(result, result);

  if (iterStatus == nsTextServicesDocument::eIsDone)
  {
    // We should never get here because a first text block
    // was found above.
    NS_ASSERTION(PR_FALSE, "Found a first without a last!");
    return NS_ERROR_FAILURE;
  }

  nsIContent *lastTextContent = iter->GetCurrentNode();
  NS_ENSURE_TRUE(lastTextContent, NS_ERROR_FAILURE);

  // Now make sure our end points are in terms of text nodes in the range!

  nsCOMPtr<nsIDOMNode> firstTextNode = do_QueryInterface(firstTextContent);
  NS_ENSURE_TRUE(firstTextNode, NS_ERROR_FAILURE);

  if (rngStartNode != firstTextNode)
  {
    // The range includes the start of the first text node!
    rngStartNode = firstTextNode;
    rngStartOffset = 0;
  }

  nsCOMPtr<nsIDOMNode> lastTextNode = do_QueryInterface(lastTextContent);
  NS_ENSURE_TRUE(lastTextNode, NS_ERROR_FAILURE);

  if (rngEndNode != lastTextNode)
  {
    // The range includes the end of the last text node!
    rngEndNode = lastTextNode;
    nsAutoString str;
    result = lastTextNode->GetNodeValue(str);
    rngEndOffset = str.Length();
  }

  // Create a doc iterator so that we can scan beyond
  // the bounds of the extent range.

  nsCOMPtr<nsIContentIterator> docIter;
  result = CreateDocumentContentIterator(getter_AddRefs(docIter));
  NS_ENSURE_SUCCESS(result, result);

  // Get a word breaker to use.

  nsCOMPtr<nsIWordBreaker> wordBreaker;
  result = GetWordBreaker(getter_AddRefs(wordBreaker));
  NS_ENSURE_SUCCESS(result, result);
  NS_ENSURE_TRUE(wordBreaker, NS_ERROR_FAILURE);

  // Grab all the text in the block containing our
  // first text node.

  result = docIter->PositionAt(firstTextContent);
  NS_ENSURE_SUCCESS(result, result);

  iterStatus = nsTextServicesDocument::eValid;

  nsVoidArray offsetTable;
  nsAutoString blockStr;

  result = CreateOffsetTable(&offsetTable, docIter, &iterStatus,
                             nsnull, &blockStr);
  if (NS_FAILED(result))
  {
    ClearOffsetTable(&offsetTable);
    return result;
  }

  nsCOMPtr<nsIDOMNode> wordStartNode, wordEndNode;
  PRInt32 wordStartOffset, wordEndOffset;

  result = FindWordBounds(&offsetTable, &blockStr, wordBreaker,
                          rngStartNode, rngStartOffset,
                          getter_AddRefs(wordStartNode), &wordStartOffset,
                          getter_AddRefs(wordEndNode), &wordEndOffset);

  ClearOffsetTable(&offsetTable);

  NS_ENSURE_SUCCESS(result, result);

  rngStartNode = wordStartNode;
  rngStartOffset = wordStartOffset;

  // Grab all the text in the block containing our
  // last text node.

  result = docIter->PositionAt(lastTextContent);
  NS_ENSURE_SUCCESS(result, result);

  iterStatus = nsTextServicesDocument::eValid;

  result = CreateOffsetTable(&offsetTable, docIter, &iterStatus,
                             nsnull, &blockStr);
  if (NS_FAILED(result))
  {
    ClearOffsetTable(&offsetTable);
    return result;
  }

  result = FindWordBounds(&offsetTable, &blockStr, wordBreaker,
                          rngEndNode, rngEndOffset,
                          getter_AddRefs(wordStartNode), &wordStartOffset,
                          getter_AddRefs(wordEndNode), &wordEndOffset);

  ClearOffsetTable(&offsetTable);

  NS_ENSURE_SUCCESS(result, result);

  // To prevent expanding the range too much, we only change
  // rngEndNode and rngEndOffset if it isn't already at the start of the
  // word and isn't equivalent to rngStartNode and rngStartOffset.

  if (rngEndNode != wordStartNode || rngEndOffset != wordStartOffset ||
     (rngEndNode == rngStartNode  && rngEndOffset == rngStartOffset))
  {
    rngEndNode = wordEndNode;
    rngEndOffset = wordEndOffset;
  }

  // Now adjust the range so that it uses our new
  // end points.

  result = aRange->SetStart(rngStartNode, rngStartOffset);
  NS_ENSURE_SUCCESS(result, result);

  return aRange->SetEnd(rngEndNode, rngEndOffset);
}

NS_IMETHODIMP
nsTextServicesDocument::SetFilter(nsITextServicesFilter *aFilter)
{
  // Hang on to the filter so we can set it into the filtered iterator.
  mTxtSvcFilter = aFilter;

  return NS_OK;
}

NS_IMETHODIMP
nsTextServicesDocument::CanEdit(PRBool *aCanEdit)
{
  if (!aCanEdit)
    return NS_ERROR_NULL_POINTER;

  *aCanEdit = (mEditor) ? PR_TRUE : PR_FALSE;

  return NS_OK;
}

NS_IMETHODIMP
nsTextServicesDocument::GetCurrentTextBlock(nsString *aStr)
{
  nsresult result;

  if (!aStr)
    return NS_ERROR_NULL_POINTER;

  aStr->Truncate();

  if (!mIterator)
    return NS_ERROR_FAILURE;

  LOCK_DOC(this);

  result = CreateOffsetTable(&mOffsetTable, mIterator, &mIteratorStatus,
                             mExtent, aStr);

  UNLOCK_DOC(this);

  return result;
}

NS_IMETHODIMP
nsTextServicesDocument::FirstBlock()
{
  NS_ENSURE_TRUE(mIterator, NS_ERROR_FAILURE);

  LOCK_DOC(this);

  nsresult result = FirstTextNode(mIterator, &mIteratorStatus);

  if (NS_FAILED(result))
  {
    UNLOCK_DOC(this);
    return result;
  }

  // Keep track of prev and next blocks, just in case
  // the text service blows away the current block.

  if (mIteratorStatus == nsTextServicesDocument::eValid)
  {
    mPrevTextBlock  = nsnull;
    result = GetFirstTextNodeInNextBlock(getter_AddRefs(mNextTextBlock));
  }
  else
  {
    // There's no text block in the document!

    mPrevTextBlock  = nsnull;
    mNextTextBlock  = nsnull;
  }

  UNLOCK_DOC(this);

  return result;
}

NS_IMETHODIMP
nsTextServicesDocument::LastBlock()
{
  NS_ENSURE_TRUE(mIterator, NS_ERROR_FAILURE);

  LOCK_DOC(this);

  // Position the iterator on the last text node
  // in the tree, then walk backwards over adjacent
  // text nodes until we hit a block boundary:

  nsresult result = LastTextNode(mIterator, &mIteratorStatus);

  if (NS_FAILED(result))
  {
    UNLOCK_DOC(this);
    return result;
  }

  result = FirstTextNodeInCurrentBlock(mIterator);

  if (NS_FAILED(result))
    mIteratorStatus = nsTextServicesDocument::eIsDone;

  // Keep track of prev and next blocks, just in case
  // the text service blows away the current block.

  if (mIteratorStatus == nsTextServicesDocument::eValid)
  {
    result = GetFirstTextNodeInPrevBlock(getter_AddRefs(mPrevTextBlock));
    mNextTextBlock = nsnull;
  }
  else
  {
    // There's no text block in the document!

    mPrevTextBlock = nsnull;
    mNextTextBlock = nsnull;
  }

  UNLOCK_DOC(this);

  return result;
}

// XXX: CODE BLOAT ALERT. FirstSelectedBlock() and LastSelectedBlock()
//      are almost identical! Later, when we have time, we should try
//      and combine them into one method.

NS_IMETHODIMP
nsTextServicesDocument::FirstSelectedBlock(TSDBlockSelectionStatus *aSelStatus, PRInt32 *aSelOffset, PRInt32 *aSelLength)
{
  nsresult result = NS_OK;

  if (!aSelStatus || !aSelOffset || !aSelLength)
    return NS_ERROR_NULL_POINTER;

  LOCK_DOC(this);

  mIteratorStatus = nsTextServicesDocument::eIsDone;

  *aSelStatus = nsITextServicesDocument::eBlockNotFound;
  *aSelOffset = *aSelLength = -1;

  if (!mSelCon || !mIterator)
  {
    UNLOCK_DOC(this);
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsISelection> selection;
  PRBool isCollapsed = PR_FALSE;

  result = mSelCon->GetSelection(nsISelectionController::SELECTION_NORMAL, getter_AddRefs(selection));

  if (NS_FAILED(result))
  {
    UNLOCK_DOC(this);
    return result;
  }

  result = selection->GetIsCollapsed( &isCollapsed);

  if (NS_FAILED(result))
  {
    UNLOCK_DOC(this);
    return result;
  }

  nsCOMPtr<nsIContentIterator> iter;
  nsCOMPtr<nsIDOMRange>        range;
  nsCOMPtr<nsIDOMNode>         parent;
  PRInt32                      i, rangeCount, offset;

  if (isCollapsed)
  {
    // We have a caret. Check if the caret is in a text node.
    // If it is, make the text node's block the current block.
    // If the caret isn't in a text node, search backwards in
    // the document, till we find a text node.

    result = selection->GetRangeAt(0, getter_AddRefs(range));

    if (NS_FAILED(result))
    {
      UNLOCK_DOC(this);
      return result;
    }

    if (!range)
    {
      UNLOCK_DOC(this);
      return NS_ERROR_FAILURE;
    }

    result = range->GetStartContainer(getter_AddRefs(parent));

    if (NS_FAILED(result))
    {
      UNLOCK_DOC(this);
      return result;
    }

    if (!parent)
    {
      UNLOCK_DOC(this);
      return NS_ERROR_FAILURE;
    }

    result = range->GetStartOffset(&offset);

    if (NS_FAILED(result))
    {
      UNLOCK_DOC(this);
      return result;
    }

    if (IsTextNode(parent))
    {
      // The caret is in a text node. Find the beginning
      // of the text block containing this text node and
      // return.

      nsCOMPtr<nsIContent> content = do_QueryInterface(parent);

      if (!content)
      {
        UNLOCK_DOC(this);
        return NS_ERROR_FAILURE;
      }

      result = mIterator->PositionAt(content);

      if (NS_FAILED(result))
      {
        UNLOCK_DOC(this);
        return result;
      }

      result = FirstTextNodeInCurrentBlock(mIterator);

      if (NS_FAILED(result))
      {
        UNLOCK_DOC(this);
        return result;
      }

      mIteratorStatus = nsTextServicesDocument::eValid;

      result = CreateOffsetTable(&mOffsetTable, mIterator, &mIteratorStatus,
                                 mExtent, nsnull);

      if (NS_FAILED(result))
      {
        UNLOCK_DOC(this);
        return result;
      }

      result = GetSelection(aSelStatus, aSelOffset, aSelLength);

      if (NS_FAILED(result))
      {
        UNLOCK_DOC(this);
        return result;
      }

      if (*aSelStatus == nsITextServicesDocument::eBlockContains)
        result = SetSelectionInternal(*aSelOffset, *aSelLength, PR_FALSE);
    }
    else
    {
      // The caret isn't in a text node. Create an iterator
      // based on a range that extends from the beginning of
      // the document, to the current caret position, then
      // walk backwards till you find a text node, then find
      // the beginning of the block.

      result = CreateDocumentContentRootToNodeOffsetRange(parent, offset, PR_TRUE, getter_AddRefs(range));

      if (NS_FAILED(result))
      {
        UNLOCK_DOC(this);
        return result;
      }

      result = range->GetCollapsed(&isCollapsed);

      if (NS_FAILED(result))
      {
        UNLOCK_DOC(this);
        return result;
      }

      if (isCollapsed)
      {
        // If we get here, the range is collapsed because there is nothing before
        // the caret! Just return NS_OK;

        UNLOCK_DOC(this);
        return NS_OK;
      }

      result = CreateContentIterator(range, getter_AddRefs(iter));

      if (NS_FAILED(result))
      {
        UNLOCK_DOC(this);
        return result;
      }

      iter->Last();

      nsIContent *content = nsnull;
      while (!iter->IsDone())
      {
        content = iter->GetCurrentNode();

        if (IsTextNode(content))
          break;

        content = nsnull;

        iter->Prev();
      }

      if (!content)
      {
        UNLOCK_DOC(this);
        return NS_OK;
      }

      result = mIterator->PositionAt(content);

      if (NS_FAILED(result))
      {
        UNLOCK_DOC(this);
        return result;
      }

      result = FirstTextNodeInCurrentBlock(mIterator);

      if (NS_FAILED(result))
      {
        UNLOCK_DOC(this);
        return result;
      }

      mIteratorStatus = nsTextServicesDocument::eValid;

      result = CreateOffsetTable(&mOffsetTable, mIterator, &mIteratorStatus,
                                 mExtent, nsnull);

      if (NS_FAILED(result))
      {
        UNLOCK_DOC(this);
        return result;
      }

      result = GetSelection(aSelStatus, aSelOffset, aSelLength);

      if (NS_FAILED(result))
      {
        UNLOCK_DOC(this);
        return result;
      }
    }

    UNLOCK_DOC(this);

    return result;
  }

  // If we get here, we have an uncollapsed selection!
  // Look through each range in the selection till you
  // find the first text node. If you find one, find the
  // beginning of it's text block, and make it the current
  // block.

  result = selection->GetRangeCount(&rangeCount);

  if (NS_FAILED(result))
  {
    UNLOCK_DOC(this);
    return result;
  }

  NS_ASSERTION(rangeCount > 0, "Unexpected range count!");

  if (rangeCount <= 0)
  {
    UNLOCK_DOC(this);
    return NS_OK;
  }

  // XXX: We may need to add some code here to make sure
  //      the ranges are sorted in document appearance order!

  for (i = 0; i < rangeCount; i++)
  {
    // Get the i'th range from the selection.

    result = selection->GetRangeAt(i, getter_AddRefs(range));

    if (NS_FAILED(result))
    {
      UNLOCK_DOC(this);
      return result;
    }

    // Create an iterator for the range.

    result = CreateContentIterator(range, getter_AddRefs(iter));

    if (NS_FAILED(result))
    {
      UNLOCK_DOC(this);
      return result;
    }

    iter->First();

    // Now walk through the range till we find a text node.

    while (!iter->IsDone())
    {
      nsIContent *content = iter->GetCurrentNode();

      if (IsTextNode(content))
      {
        // We found a text node, so position the document's
        // iterator at the beginning of the block, then get
        // the selection in terms of the string offset.

        result = mIterator->PositionAt(content);

        if (NS_FAILED(result))
        {
          UNLOCK_DOC(this);
          return result;
        }

        result = FirstTextNodeInCurrentBlock(mIterator);

        if (NS_FAILED(result))
        {
          UNLOCK_DOC(this);
          return result;
        }

        mIteratorStatus = nsTextServicesDocument::eValid;

        result = CreateOffsetTable(&mOffsetTable, mIterator, &mIteratorStatus,
                                   mExtent, nsnull);

        if (NS_FAILED(result))
        {
          UNLOCK_DOC(this);
          return result;
        }

        result = GetSelection(aSelStatus, aSelOffset, aSelLength);

        UNLOCK_DOC(this);

        return result;

      }

      iter->Next();
    }
  }

  // If we get here, we didn't find any text node in the selection!
  // Create a range that extends from the beginning of the selection,
  // to the beginning of the document, then iterate backwards through
  // it till you find a text node!

  result = selection->GetRangeAt(0, getter_AddRefs(range));

  if (NS_FAILED(result))
  {
    UNLOCK_DOC(this);
    return result;
  }

  if (!range)
  {
    UNLOCK_DOC(this);
    return NS_ERROR_FAILURE;
  }

  result = range->GetStartContainer(getter_AddRefs(parent));

  if (NS_FAILED(result))
  {
    UNLOCK_DOC(this);
    return result;
  }

  if (!parent)
  {
    UNLOCK_DOC(this);
    return NS_ERROR_FAILURE;
  }

  result = range->GetStartOffset(&offset);

  if (NS_FAILED(result))
  {
    UNLOCK_DOC(this);
    return result;
  }

  result = CreateDocumentContentRootToNodeOffsetRange(parent, offset, PR_TRUE, getter_AddRefs(range));

  if (NS_FAILED(result))
  {
    UNLOCK_DOC(this);
    return result;
  }

  result = range->GetCollapsed(&isCollapsed);

  if (NS_FAILED(result))
  {
    UNLOCK_DOC(this);
    return result;
  }

  if (isCollapsed)
  {
    // If we get here, the range is collapsed because there is nothing before
    // the current selection! Just return NS_OK;

    UNLOCK_DOC(this);
    return NS_OK;
  }

  result = CreateContentIterator(range, getter_AddRefs(iter));

  if (NS_FAILED(result))
  {
    UNLOCK_DOC(this);
    return result;
  }

  iter->Last();

  while (!iter->IsDone())
  {
    nsIContent *content = iter->GetCurrentNode();

    if (IsTextNode(content))
    {
      // We found a text node! Adjust the document's iterator to point
      // to the beginning of it's text block, then get the current selection.

      result = mIterator->PositionAt(content);

      if (NS_FAILED(result))
      {
        UNLOCK_DOC(this);
        return result;
      }

      result = FirstTextNodeInCurrentBlock(mIterator);

      if (NS_FAILED(result))
      {
        UNLOCK_DOC(this);
        return result;
      }

      mIteratorStatus = nsTextServicesDocument::eValid;

      result = CreateOffsetTable(&mOffsetTable, mIterator, &mIteratorStatus,
                                 mExtent, nsnull);

      if (NS_FAILED(result))
      {
        UNLOCK_DOC(this);
        return result;
      }

      result = GetSelection(aSelStatus, aSelOffset, aSelLength);

      UNLOCK_DOC(this);

      return result;
    }

    iter->Prev();
  }

  // If we get here, we didn't find any block before or inside
  // the selection! Just return OK.

  UNLOCK_DOC(this);

  return NS_OK;
}

// XXX: CODE BLOAT ALERT. FirstSelectedBlock() and LastSelectedBlock()
//      are almost identical! Later, when we have time, we should try
//      and combine them into one method.

NS_IMETHODIMP
nsTextServicesDocument::LastSelectedBlock(TSDBlockSelectionStatus *aSelStatus,
                                          PRInt32 *aSelOffset,
                                          PRInt32 *aSelLength)
{
  nsresult result = NS_OK;

  if (!aSelStatus || !aSelOffset || !aSelLength)
    return NS_ERROR_NULL_POINTER;

  LOCK_DOC(this);

  mIteratorStatus = nsTextServicesDocument::eIsDone;

  *aSelStatus = nsITextServicesDocument::eBlockNotFound;
  *aSelOffset = *aSelLength = -1;

  if (!mSelCon || !mIterator)
  {
    UNLOCK_DOC(this);
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsISelection> selection;
  PRBool isCollapsed = PR_FALSE;

  result = mSelCon->GetSelection(nsISelectionController::SELECTION_NORMAL, getter_AddRefs(selection));

  if (NS_FAILED(result))
  {
    UNLOCK_DOC(this);
    return result;
  }

  result = selection->GetIsCollapsed(&isCollapsed);

  if (NS_FAILED(result))
  {
    UNLOCK_DOC(this);
    return result;
  }

  nsCOMPtr<nsIContentIterator> iter;
  nsCOMPtr<nsIDOMRange>        range;
  nsCOMPtr<nsIDOMNode>         parent;
  PRInt32                      i, rangeCount, offset;

  if (isCollapsed)
  {
    // We have a caret. Check if the caret is in a text node.
    // If it is, make the text node's block the current block.
    // If the caret isn't in a text node, search forwards in
    // the document, till we find a text node.

    result = selection->GetRangeAt(0, getter_AddRefs(range));

    if (NS_FAILED(result))
    {
      UNLOCK_DOC(this);
      return result;
    }

    if (!range)
    {
      UNLOCK_DOC(this);
      return NS_ERROR_FAILURE;
    }

    result = range->GetStartContainer(getter_AddRefs(parent));

    if (NS_FAILED(result))
    {
      UNLOCK_DOC(this);
      return result;
    }

    if (!parent)
    {
      UNLOCK_DOC(this);
      return NS_ERROR_FAILURE;
    }

    result = range->GetStartOffset(&offset);

    if (NS_FAILED(result))
    {
      UNLOCK_DOC(this);
      return result;
    }

    if (IsTextNode(parent))
    {
      // The caret is in a text node. Find the beginning
      // of the text block containing this text node and
      // return.

      nsCOMPtr<nsIContent> content(do_QueryInterface(parent));

      if (!content)
      {
        UNLOCK_DOC(this);
        return NS_ERROR_FAILURE;
      }

      result = mIterator->PositionAt(content);

      if (NS_FAILED(result))
      {
        UNLOCK_DOC(this);
        return result;
      }

      result = FirstTextNodeInCurrentBlock(mIterator);

      if (NS_FAILED(result))
      {
        UNLOCK_DOC(this);
        return result;
      }

      mIteratorStatus = nsTextServicesDocument::eValid;

      result = CreateOffsetTable(&mOffsetTable, mIterator, &mIteratorStatus,
                                 mExtent, nsnull);

      if (NS_FAILED(result))
      {
        UNLOCK_DOC(this);
        return result;
      }

      result = GetSelection(aSelStatus, aSelOffset, aSelLength);

      if (NS_FAILED(result))
      {
        UNLOCK_DOC(this);
        return result;
      }

      if (*aSelStatus == nsITextServicesDocument::eBlockContains)
        result = SetSelectionInternal(*aSelOffset, *aSelLength, PR_FALSE);
    }
    else
    {
      // The caret isn't in a text node. Create an iterator
      // based on a range that extends from the current caret
      // position to the end of the document, then walk forwards
      // till you find a text node, then find the beginning of it's block.

      result = CreateDocumentContentRootToNodeOffsetRange(parent, offset, PR_FALSE, getter_AddRefs(range));

      if (NS_FAILED(result))
      {
        UNLOCK_DOC(this);
        return result;
      }

      result = range->GetCollapsed(&isCollapsed);

      if (NS_FAILED(result))
      {
        UNLOCK_DOC(this);
        return result;
      }

      if (isCollapsed)
      {
        // If we get here, the range is collapsed because there is nothing after
        // the caret! Just return NS_OK;

        UNLOCK_DOC(this);
        return NS_OK;
      }

      result = CreateContentIterator(range, getter_AddRefs(iter));

      if (NS_FAILED(result))
      {
        UNLOCK_DOC(this);
        return result;
      }

      iter->First();

      nsIContent *content = nsnull;
      while (!iter->IsDone())
      {
        content = iter->GetCurrentNode();

        if (IsTextNode(content))
          break;

        content = nsnull;

        iter->Next();
      }

      if (!content)
      {
        UNLOCK_DOC(this);
        return NS_OK;
      }

      result = mIterator->PositionAt(content);

      if (NS_FAILED(result))
      {
        UNLOCK_DOC(this);
        return result;
      }

      result = FirstTextNodeInCurrentBlock(mIterator);

      if (NS_FAILED(result))
      {
        UNLOCK_DOC(this);
        return result;
      }

      mIteratorStatus = nsTextServicesDocument::eValid;

      result = CreateOffsetTable(&mOffsetTable, mIterator, &mIteratorStatus,
                                 mExtent, nsnull);

      if (NS_FAILED(result))
      {
        UNLOCK_DOC(this);
        return result;
      }

      result = GetSelection(aSelStatus, aSelOffset, aSelLength);

      if (NS_FAILED(result))
      {
        UNLOCK_DOC(this);
        return result;
      }
    }

    UNLOCK_DOC(this);

    return result;
  }

  // If we get here, we have an uncollapsed selection!
  // Look backwards through each range in the selection till you
  // find the first text node. If you find one, find the
  // beginning of it's text block, and make it the current
  // block.

  result = selection->GetRangeCount(&rangeCount);

  if (NS_FAILED(result))
  {
    UNLOCK_DOC(this);
    return result;
  }

  NS_ASSERTION(rangeCount > 0, "Unexpected range count!");

  if (rangeCount <= 0)
  {
    UNLOCK_DOC(this);
    return NS_OK;
  }

  // XXX: We may need to add some code here to make sure
  //      the ranges are sorted in document appearance order!

  for (i = rangeCount - 1; i >= 0; i--)
  {
    // Get the i'th range from the selection.

    result = selection->GetRangeAt(i, getter_AddRefs(range));

    if (NS_FAILED(result))
    {
      UNLOCK_DOC(this);
      return result;
    }

    // Create an iterator for the range.

    result = CreateContentIterator(range, getter_AddRefs(iter));

    if (NS_FAILED(result))
    {
      UNLOCK_DOC(this);
      return result;
    }

    iter->Last();

    // Now walk through the range till we find a text node.

    while (!iter->IsDone())
    {
      nsIContent *content = iter->GetCurrentNode();

      if (IsTextNode(content))
      {
        // We found a text node, so position the document's
        // iterator at the beginning of the block, then get
        // the selection in terms of the string offset.

        result = mIterator->PositionAt(content);

        if (NS_FAILED(result))
        {
          UNLOCK_DOC(this);
          return result;
        }

        result = FirstTextNodeInCurrentBlock(mIterator);

        if (NS_FAILED(result))
        {
          UNLOCK_DOC(this);
          return result;
        }

        mIteratorStatus = nsTextServicesDocument::eValid;

        result = CreateOffsetTable(&mOffsetTable, mIterator, &mIteratorStatus,
                                   mExtent, nsnull);

        if (NS_FAILED(result))
        {
          UNLOCK_DOC(this);
          return result;
        }

        result = GetSelection(aSelStatus, aSelOffset, aSelLength);

        UNLOCK_DOC(this);

        return result;

      }

      iter->Prev();
    }
  }

  // If we get here, we didn't find any text node in the selection!
  // Create a range that extends from the end of the selection,
  // to the end of the document, then iterate forwards through
  // it till you find a text node!

  result = selection->GetRangeAt(rangeCount - 1, getter_AddRefs(range));

  if (NS_FAILED(result))
  {
    UNLOCK_DOC(this);
    return result;
  }

  if (!range)
  {
    UNLOCK_DOC(this);
    return NS_ERROR_FAILURE;
  }

  result = range->GetEndContainer(getter_AddRefs(parent));

  if (NS_FAILED(result))
  {
    UNLOCK_DOC(this);
    return result;
  }

  if (!parent)
  {
    UNLOCK_DOC(this);
    return NS_ERROR_FAILURE;
  }

  result = range->GetEndOffset(&offset);

  if (NS_FAILED(result))
  {
    UNLOCK_DOC(this);
    return result;
  }

  result = CreateDocumentContentRootToNodeOffsetRange(parent, offset, PR_FALSE, getter_AddRefs(range));

  if (NS_FAILED(result))
  {
    UNLOCK_DOC(this);
    return result;
  }

  result = range->GetCollapsed(&isCollapsed);

  if (NS_FAILED(result))
  {
    UNLOCK_DOC(this);
    return result;
  }

  if (isCollapsed)
  {
    // If we get here, the range is collapsed because there is nothing after
    // the current selection! Just return NS_OK;

    UNLOCK_DOC(this);
    return NS_OK;
  }

  result = CreateContentIterator(range, getter_AddRefs(iter));

  if (NS_FAILED(result))
  {
    UNLOCK_DOC(this);
    return result;
  }

  iter->First();

  while (!iter->IsDone())
  {
    nsIContent *content = iter->GetCurrentNode();

    if (IsTextNode(content))
    {
      // We found a text node! Adjust the document's iterator to point
      // to the beginning of it's text block, then get the current selection.

      result = mIterator->PositionAt(content);

      if (NS_FAILED(result))
      {
        UNLOCK_DOC(this);
        return result;
      }

      result = FirstTextNodeInCurrentBlock(mIterator);

      if (NS_FAILED(result))
      {
        UNLOCK_DOC(this);
        return result;
      }


      mIteratorStatus = nsTextServicesDocument::eValid;

      result = CreateOffsetTable(&mOffsetTable, mIterator, &mIteratorStatus,
                                 mExtent, nsnull);

      if (NS_FAILED(result))
      {
        UNLOCK_DOC(this);
        return result;
      }

      result = GetSelection(aSelStatus, aSelOffset, aSelLength);

      UNLOCK_DOC(this);

      return result;
    }

    iter->Next();
  }

  // If we get here, we didn't find any block before or inside
  // the selection! Just return OK.

  UNLOCK_DOC(this);

  return NS_OK;
}

NS_IMETHODIMP
nsTextServicesDocument::PrevBlock()
{
  nsresult result = NS_OK;

  if (!mIterator)
    return NS_ERROR_FAILURE;

  LOCK_DOC(this);

  if (mIteratorStatus == nsTextServicesDocument::eIsDone)
    return NS_OK;

  switch (mIteratorStatus)
  {
    case nsTextServicesDocument::eValid:
    case nsTextServicesDocument::eNext:

      result = FirstTextNodeInPrevBlock(mIterator);

      if (NS_FAILED(result))
      {
        mIteratorStatus = nsTextServicesDocument::eIsDone;
        UNLOCK_DOC(this);
        return result;
      }

      if (mIterator->IsDone())
      {
        mIteratorStatus = nsTextServicesDocument::eIsDone;
        UNLOCK_DOC(this);
        return NS_OK;
      }

      mIteratorStatus = nsTextServicesDocument::eValid;
      break;

    case nsTextServicesDocument::ePrev:

      // The iterator already points to the previous
      // block, so don't do anything.

      mIteratorStatus = nsTextServicesDocument::eValid;
      break;

    default:

      mIteratorStatus = nsTextServicesDocument::eIsDone;
      break;
  }

  // Keep track of prev and next blocks, just in case
  // the text service blows away the current block.

  if (mIteratorStatus == nsTextServicesDocument::eValid)
  {
    result = GetFirstTextNodeInPrevBlock(getter_AddRefs(mPrevTextBlock));
    result = GetFirstTextNodeInNextBlock(getter_AddRefs(mNextTextBlock));
  }
  else
  {
    // We must be done!

    mPrevTextBlock = nsnull;
    mNextTextBlock = nsnull;
  }

  UNLOCK_DOC(this);

  return result;
}

NS_IMETHODIMP
nsTextServicesDocument::NextBlock()
{
  nsresult result = NS_OK;

  if (!mIterator)
    return NS_ERROR_FAILURE;

  LOCK_DOC(this);

  if (mIteratorStatus == nsTextServicesDocument::eIsDone)
    return NS_OK;

  switch (mIteratorStatus)
  {
    case nsTextServicesDocument::eValid:

      // Advance the iterator to the next text block.

      result = FirstTextNodeInNextBlock(mIterator);

      if (NS_FAILED(result))
      {
        mIteratorStatus = nsTextServicesDocument::eIsDone;
        UNLOCK_DOC(this);
        return result;
      }

      if (mIterator->IsDone())
      {
        mIteratorStatus = nsTextServicesDocument::eIsDone;
        UNLOCK_DOC(this);
        return NS_OK;
      }

      mIteratorStatus = nsTextServicesDocument::eValid;
      break;

    case nsTextServicesDocument::eNext:

      // The iterator already points to the next block,
      // so don't do anything to it!

      mIteratorStatus = nsTextServicesDocument::eValid;
      break;

    case nsTextServicesDocument::ePrev:

      // If the iterator is pointing to the previous block,
      // we know that there is no next text block! Just
      // fall through to the default case!

    default:

      mIteratorStatus = nsTextServicesDocument::eIsDone;
      break;
  }

  // Keep track of prev and next blocks, just in case
  // the text service blows away the current block.

  if (mIteratorStatus == nsTextServicesDocument::eValid)
  {
    result = GetFirstTextNodeInPrevBlock(getter_AddRefs(mPrevTextBlock));
    result = GetFirstTextNodeInNextBlock(getter_AddRefs(mNextTextBlock));
  }
  else
  {
    // We must be done.

    mPrevTextBlock = nsnull;
    mNextTextBlock = nsnull;
  }


  UNLOCK_DOC(this);

  return result;
}

NS_IMETHODIMP
nsTextServicesDocument::IsDone(PRBool *aIsDone)
{
  if (!aIsDone)
    return NS_ERROR_NULL_POINTER;

  *aIsDone = PR_FALSE;

  if (!mIterator)
    return NS_ERROR_FAILURE;

  LOCK_DOC(this);

  *aIsDone = (mIteratorStatus == nsTextServicesDocument::eIsDone) ? PR_TRUE : PR_FALSE;

  UNLOCK_DOC(this);

  return NS_OK;
}

NS_IMETHODIMP
nsTextServicesDocument::SetSelection(PRInt32 aOffset, PRInt32 aLength)
{
  nsresult result;

  if (!mSelCon || aOffset < 0 || aLength < 0)
    return NS_ERROR_FAILURE;

  LOCK_DOC(this);

  result = SetSelectionInternal(aOffset, aLength, PR_TRUE);

  UNLOCK_DOC(this);

  //**** KDEBUG ****
  // printf("\n * Sel: (%2d, %4d) (%2d, %4d)\n", mSelStartIndex, mSelStartOffset, mSelEndIndex, mSelEndOffset);
  //**** KDEBUG ****

  return result;
}

NS_IMETHODIMP
nsTextServicesDocument::ScrollSelectionIntoView()
{
  nsresult result;

  if (!mSelCon)
    return NS_ERROR_FAILURE;

  LOCK_DOC(this);

  result = mSelCon->ScrollSelectionIntoView(nsISelectionController::SELECTION_NORMAL, nsISelectionController::SELECTION_FOCUS_REGION, PR_TRUE);

  UNLOCK_DOC(this);

  return result;
}

NS_IMETHODIMP
nsTextServicesDocument::DeleteSelection()
{
  nsresult result = NS_OK;

  // We don't allow deletion during a collapsed selection!

  NS_ASSERTION(mEditor, "DeleteSelection called without an editor present!"); 
  NS_ASSERTION(SelectionIsValid(), "DeleteSelection called without a valid selection!"); 

  if (!mEditor || !SelectionIsValid())
    return NS_ERROR_FAILURE;

  if (SelectionIsCollapsed())
    return NS_OK;

  LOCK_DOC(this);

  //**** KDEBUG ****
  // printf("\n---- Before Delete\n");
  // printf("Sel: (%2d, %4d) (%2d, %4d)\n", mSelStartIndex, mSelStartOffset, mSelEndIndex, mSelEndOffset);
  // PrintOffsetTable();
  //**** KDEBUG ****

  // If we have an mExtent, save off it's current set of
  // end points so we can compare them against mExtent's
  // set after the deletion of the content.

  nsCOMPtr<nsIDOMNode> origStartNode, origEndNode;
  PRInt32 origStartOffset = 0, origEndOffset = 0;

  if (mExtent)
  {
    result = GetRangeEndPoints(mExtent,
                               getter_AddRefs(origStartNode), &origStartOffset,
                               getter_AddRefs(origEndNode), &origEndOffset);

    if (NS_FAILED(result))
    {
      UNLOCK_DOC(this);
      return result;
    }
  }

  PRInt32 i, selLength;
  OffsetEntry *entry, *newEntry;

  for (i = mSelStartIndex; i <= mSelEndIndex; i++)
  {
    entry = (OffsetEntry *)mOffsetTable[i];

    if (i == mSelStartIndex)
    {
      // Calculate the length of the selection. Note that the
      // selection length can be zero if the start of the selection
      // is at the very end of a text node entry.

      if (entry->mIsInsertedText)
      {
        // Inserted text offset entries have no width when
        // talking in terms of string offsets! If the beginning
        // of the selection is in an inserted text offset entry,
        // the caret is always at the end of the entry!

        selLength = 0;
      }
      else
        selLength = entry->mLength - (mSelStartOffset - entry->mStrOffset);

      if (selLength > 0 && mSelStartOffset > entry->mStrOffset)
      {
        // Selection doesn't start at the beginning of the
        // text node entry. We need to split this entry into
        // two pieces, the piece before the selection, and
        // the piece inside the selection.

        result = SplitOffsetEntry(i, selLength);

        if (NS_FAILED(result))
        {
          UNLOCK_DOC(this);
          return result;
        }

        // Adjust selection indexes to account for new entry:

        ++mSelStartIndex;
        ++mSelEndIndex;
        ++i;

        entry = (OffsetEntry *)mOffsetTable[i];
      }


      if (selLength > 0 && mSelStartIndex < mSelEndIndex)
      {
        // The entire entry is contained in the selection. Mark the
        // entry invalid.

        entry->mIsValid = PR_FALSE;
      }
    }

  //**** KDEBUG ****
  // printf("\n---- Middle Delete\n");
  // printf("Sel: (%2d, %4d) (%2d, %4d)\n", mSelStartIndex, mSelStartOffset, mSelEndIndex, mSelEndOffset);
  // PrintOffsetTable();
  //**** KDEBUG ****

    if (i == mSelEndIndex)
    {
      if (entry->mIsInsertedText)
      {
        // Inserted text offset entries have no width when
        // talking in terms of string offsets! If the end
        // of the selection is in an inserted text offset entry,
        // the selection includes the entire entry!

        entry->mIsValid = PR_FALSE;
      }
      else
      {
        // Calculate the length of the selection. Note that the
        // selection length can be zero if the end of the selection
        // is at the very beginning of a text node entry.

        selLength = mSelEndOffset - entry->mStrOffset;

        if (selLength > 0 && mSelEndOffset < entry->mStrOffset + entry->mLength)
        {
          // mStrOffset is guaranteed to be inside the selection, even
          // when mSelStartIndex == mSelEndIndex.

          result = SplitOffsetEntry(i, entry->mLength - selLength);

          if (NS_FAILED(result))
          {
            UNLOCK_DOC(this);
            return result;
          }

          // Update the entry fields:

          newEntry = (OffsetEntry *)mOffsetTable[i+1];
          newEntry->mNodeOffset = entry->mNodeOffset;
        }


        if (selLength > 0 && mSelEndOffset == entry->mStrOffset + entry->mLength)
        {
          // The entire entry is contained in the selection. Mark the
          // entry invalid.

          entry->mIsValid = PR_FALSE;
        }
      }
    }

    if (i != mSelStartIndex && i != mSelEndIndex)
    {
      // The entire entry is contained in the selection. Mark the
      // entry invalid.

      entry->mIsValid = PR_FALSE;
    }
  }

  // Make sure mIterator always points to something valid!

  AdjustContentIterator();

  // Now delete the actual content!

  result = mEditor->DeleteSelection(nsIEditor::ePrevious);

  if (NS_FAILED(result))
  {
    UNLOCK_DOC(this);
    return result;
  }

  // Now that we've actually deleted the selected content,
  // check to see if our mExtent has changed, if so, then
  // we have to create a new content iterator!

  if (origStartNode && origEndNode)
  {
    nsCOMPtr<nsIDOMNode> curStartNode, curEndNode;
    PRInt32 curStartOffset = 0, curEndOffset = 0;

    result = GetRangeEndPoints(mExtent,
                               getter_AddRefs(curStartNode), &curStartOffset,
                               getter_AddRefs(curEndNode), &curEndOffset);

    if (NS_FAILED(result))
    {
      UNLOCK_DOC(this);
      return result;
    }

    if (origStartNode != curStartNode || origEndNode != curEndNode)
    {
      // The range has changed, so we need to create a new content
      // iterator based on the new range.

      nsIContent *curContent = nsnull;

      if (mIteratorStatus != nsTextServicesDocument::eIsDone)
      {
        // The old iterator is still pointing to something valid,
        // so get it's current node so we can restore it after we
        // create the new iterator!

        curContent = mIterator->GetCurrentNode();
      }

      // Create the new iterator.

      result = CreateContentIterator(mExtent, getter_AddRefs(mIterator));

      if (NS_FAILED(result))
      {
        UNLOCK_DOC(this);
        return result;
      }

      // Now make the new iterator point to the content node
      // the old one was pointing at.

      if (curContent)
      {
        result = mIterator->PositionAt(curContent);

        if (NS_FAILED(result))
          mIteratorStatus = eIsDone;
        else
          mIteratorStatus = eValid;
      }
    }
  }

  entry = 0;

  // Move the caret to the end of the first valid entry.
  // Start with mSelStartIndex since it may still be valid.

  for (i = mSelStartIndex; !entry && i >= 0; i--)
  {
    entry = (OffsetEntry *)mOffsetTable[i];

    if (!entry->mIsValid)
      entry = 0;
    else
    {
      mSelStartIndex  = mSelEndIndex  = i;
      mSelStartOffset = mSelEndOffset = entry->mStrOffset + entry->mLength;
    }
  }

  // If we still don't have a valid entry, move the caret
  // to the next valid entry after the selection:

  for (i = mSelEndIndex; !entry && i < mOffsetTable.Count(); i++)
  {
    entry = (OffsetEntry *)mOffsetTable[i];

    if (!entry->mIsValid)
      entry = 0;
    else
    {
      mSelStartIndex = mSelEndIndex = i;
      mSelStartOffset = mSelEndOffset = entry->mStrOffset;
    }
  }

  if (entry)
    result = SetSelection(mSelStartOffset, 0);
  else
  {
    // Uuughh we have no valid offset entry to place our
    // caret ... just mark the selection invalid.

    mSelStartIndex  = mSelEndIndex  = -1;
    mSelStartOffset = mSelEndOffset = -1;
  }

  // Now remove any invalid entries from the offset table.

  result = RemoveInvalidOffsetEntries();

  //**** KDEBUG ****
  // printf("\n---- After Delete\n");
  // printf("Sel: (%2d, %4d) (%2d, %4d)\n", mSelStartIndex, mSelStartOffset, mSelEndIndex, mSelEndOffset);
  // PrintOffsetTable();
  //**** KDEBUG ****

  UNLOCK_DOC(this);

  return result;
}

NS_IMETHODIMP
nsTextServicesDocument::InsertText(const nsString *aText)
{
  nsresult result = NS_OK;

  NS_ASSERTION(mEditor, "InsertText called without an editor present!"); 

  if (!mEditor || !SelectionIsValid())
    return NS_ERROR_FAILURE;

  if (!aText)
    return NS_ERROR_NULL_POINTER;

  // If the selection is not collapsed, we need to save
  // off the selection offsets so we can restore the
  // selection and delete the selected content after we've
  // inserted the new text. This is necessary to try and
  // retain as much of the original style of the content
  // being deleted.

  PRBool collapsedSelection = SelectionIsCollapsed();
  PRInt32 savedSelOffset = mSelStartOffset;
  PRInt32 savedSelLength = mSelEndOffset - mSelStartOffset;

  if (!collapsedSelection)
  {
    // Collapse to the start of the current selection
    // for the insert!

    result = SetSelection(mSelStartOffset, 0);

    if (NS_FAILED(result))
      return result;
  }


  LOCK_DOC(this);

  result = mEditor->BeginTransaction();

  if (NS_FAILED(result))
  {
    UNLOCK_DOC(this);
    return result;
  }

  nsCOMPtr<nsIPlaintextEditor> textEditor (do_QueryInterface(mEditor, &result));
  if (textEditor)
    result = textEditor->InsertText(*aText);

  if (NS_FAILED(result))
  {
    mEditor->EndTransaction();
    UNLOCK_DOC(this);
    return result;
  }

  //**** KDEBUG ****
  // printf("\n---- Before Insert\n");
  // printf("Sel: (%2d, %4d) (%2d, %4d)\n", mSelStartIndex, mSelStartOffset, mSelEndIndex, mSelEndOffset);
  // PrintOffsetTable();
  //**** KDEBUG ****

  PRInt32 i, strLength = aText->Length();

  nsCOMPtr<nsISelection> selection;
  OffsetEntry *itEntry;
  OffsetEntry *entry = (OffsetEntry *)mOffsetTable[mSelStartIndex];
  void *node         = entry->mNode;

  NS_ASSERTION((entry->mIsValid), "Invalid insertion point!");

  if (entry->mStrOffset == mSelStartOffset)
  {
    if (entry->mIsInsertedText)
    {
      // If the caret is in an inserted text offset entry,
      // we simply insert the text at the end of the entry.

      entry->mLength += strLength;
    }
    else
    {
      // Insert an inserted text offset entry before the current
      // entry!

      itEntry = new OffsetEntry(entry->mNode, entry->mStrOffset, strLength);

      if (!itEntry)
      {
        mEditor->EndTransaction();
        UNLOCK_DOC(this);
        return NS_ERROR_OUT_OF_MEMORY;
      }

      itEntry->mIsInsertedText = PR_TRUE;
      itEntry->mNodeOffset = entry->mNodeOffset;

      if (!mOffsetTable.InsertElementAt(itEntry, mSelStartIndex))
      {
        mEditor->EndTransaction();
        UNLOCK_DOC(this);
        return NS_ERROR_FAILURE;
      }
    }
  }
  else if ((entry->mStrOffset + entry->mLength) == mSelStartOffset)
  {
    // We are inserting text at the end of the current offset entry.
    // Look at the next valid entry in the table. If it's an inserted
    // text entry, add to it's length and adjust it's node offset. If
    // it isn't, add a new inserted text entry.

    i       = mSelStartIndex + 1;
    itEntry = 0;

    if (mOffsetTable.Count() > i)
    {
      itEntry = (OffsetEntry *)mOffsetTable[i];

      if (!itEntry)
      {
        mEditor->EndTransaction();
        UNLOCK_DOC(this);
        return NS_ERROR_FAILURE;
      }

      // Check if the entry is a match. If it isn't, set
      // iEntry to zero.

      if (!itEntry->mIsInsertedText || itEntry->mStrOffset != mSelStartOffset)
        itEntry = 0;
    }

    if (!itEntry)
    {
      // We didn't find an inserted text offset entry, so
      // create one.

      itEntry = new OffsetEntry(entry->mNode, mSelStartOffset, 0);

      if (!itEntry)
      {
        mEditor->EndTransaction();
        UNLOCK_DOC(this);
        return NS_ERROR_OUT_OF_MEMORY;
      }

      itEntry->mNodeOffset = entry->mNodeOffset + entry->mLength;
      itEntry->mIsInsertedText = PR_TRUE;

      if (!mOffsetTable.InsertElementAt(itEntry, i))
      {
        delete itEntry;
        return NS_ERROR_FAILURE;
      }
    }

    // We have a valid inserted text offset entry. Update it's
    // length, adjust the selection indexes, and make sure the
    // caret is properly placed!

    itEntry->mLength += strLength;

    mSelStartIndex = mSelEndIndex = i;
          
    result = mSelCon->GetSelection(nsISelectionController::SELECTION_NORMAL, getter_AddRefs(selection));

    if (NS_FAILED(result))
    {
      mEditor->EndTransaction();
      UNLOCK_DOC(this);
      return result;
    }

    result = selection->Collapse(itEntry->mNode, itEntry->mNodeOffset + itEntry->mLength);
        
    if (NS_FAILED(result))
    {
      mEditor->EndTransaction();
      UNLOCK_DOC(this);
      return result;
    }
  }
  else if ((entry->mStrOffset + entry->mLength) > mSelStartOffset)
  {
    // We are inserting text into the middle of the current offset entry.
    // split the current entry into two parts, then insert an inserted text
    // entry between them!

    i = entry->mLength - (mSelStartOffset - entry->mStrOffset);

    result = SplitOffsetEntry(mSelStartIndex, i);

    if (NS_FAILED(result))
    {
      mEditor->EndTransaction();
      UNLOCK_DOC(this);
      return result;
    }

    itEntry = new OffsetEntry(entry->mNode, mSelStartOffset, strLength);

    if (!itEntry)
    {
      mEditor->EndTransaction();
      UNLOCK_DOC(this);
      return NS_ERROR_OUT_OF_MEMORY;
    }

    itEntry->mIsInsertedText = PR_TRUE;
    itEntry->mNodeOffset     = entry->mNodeOffset + entry->mLength;

    if (!mOffsetTable.InsertElementAt(itEntry, mSelStartIndex + 1))
    {
      mEditor->EndTransaction();
      UNLOCK_DOC(this);
      return NS_ERROR_FAILURE;
    }

    mSelEndIndex = ++mSelStartIndex;
  }

  // We've just finished inserting an inserted text offset entry.
  // update all entries with the same mNode pointer that follow
  // it in the table!

  for (i = mSelStartIndex + 1; i < mOffsetTable.Count(); i++)
  {
    entry = (OffsetEntry *)mOffsetTable[i];

    if (entry->mNode == node)
    {
      if (entry->mIsValid)
        entry->mNodeOffset += strLength;
    }
    else
      break;
  }

  //**** KDEBUG ****
  // printf("\n---- After Insert\n");
  // printf("Sel: (%2d, %4d) (%2d, %4d)\n", mSelStartIndex, mSelStartOffset, mSelEndIndex, mSelEndOffset);
  // PrintOffsetTable();
  //**** KDEBUG ****

  if (!collapsedSelection)
  {
    result = SetSelection(savedSelOffset, savedSelLength);

    if (NS_FAILED(result))
    {
      mEditor->EndTransaction();
      UNLOCK_DOC(this);
      return result;
    }

    result = DeleteSelection();
  
    if (NS_FAILED(result))
    {
      mEditor->EndTransaction();
      UNLOCK_DOC(this);
      return result;
    }
  }

  result = mEditor->EndTransaction();

  UNLOCK_DOC(this);

  return result;
}

NS_IMETHODIMP
nsTextServicesDocument::SetDisplayStyle(TSDDisplayStyle aStyle)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
nsTextServicesDocument::InsertNode(nsIDOMNode *aNode,
                                   nsIDOMNode *aParent,
                                   PRInt32 aPosition)
{
  //**** KDEBUG ****
  // printf("** InsertNode: 0x%.8x  0x%.8x  %d\n", aNode, aParent, aPosition);
  // fflush(stdout);
  //**** KDEBUG ****

  NS_ASSERTION(0, "InsertNode called, offset tables might be out of sync."); 

  return NS_OK;
}

nsresult
nsTextServicesDocument::DeleteNode(nsIDOMNode *aChild)
{
  //**** KDEBUG ****
  // printf("** DeleteNode: 0x%.8x\n", aChild);
  // fflush(stdout);
  //**** KDEBUG ****

  LOCK_DOC(this);

  PRInt32 nodeIndex, tcount;
  PRBool hasEntry;
  OffsetEntry *entry;

  nsresult result = NodeHasOffsetEntry(&mOffsetTable, aChild, &hasEntry, &nodeIndex);

  if (NS_FAILED(result))
  {
    UNLOCK_DOC(this);
    return result;
  }

  if (!hasEntry)
  {
    // It's okay if the node isn't in the offset table, the
    // editor could be cleaning house.

    UNLOCK_DOC(this);
    return NS_OK;
  }

  nsCOMPtr<nsIDOMNode> node = do_QueryInterface(mIterator->GetCurrentNode());

  if (node && node == aChild &&
      mIteratorStatus != nsTextServicesDocument::eIsDone)
  {
    // XXX: This should never really happen because
    // AdjustContentIterator() should have been called prior
    // to the delete to try and position the iterator on the
    // next valid text node in the offset table, and if there
    // wasn't a next, it would've set mIteratorStatus to eIsDone.

    NS_ASSERTION(0, "DeleteNode called for current iterator node."); 
  }

  tcount = mOffsetTable.Count();

  while (nodeIndex < tcount)
  {
    entry = (OffsetEntry *)mOffsetTable[nodeIndex];

    if (!entry)
    {
      UNLOCK_DOC(this);
      return NS_ERROR_FAILURE;
    }

    if (entry->mNode == aChild)
    {
      NS_ASSERTION(!entry->mIsValid, "DeleteNode called for a valid node! Offset table is out of sync."); 
      entry->mIsValid = PR_FALSE;
    }

    nodeIndex++;
  }

  UNLOCK_DOC(this);

  return NS_OK;
}

nsresult
nsTextServicesDocument::SplitNode(nsIDOMNode *aExistingRightNode,
                                  PRInt32 aOffset,
                                  nsIDOMNode *aNewLeftNode)
{
  //**** KDEBUG ****
  // printf("** SplitNode: 0x%.8x  %d  0x%.8x\n", aExistingRightNode, aOffset, aNewLeftNode);
  // fflush(stdout);
  //**** KDEBUG ****

  NS_ASSERTION(0, "SplitNode called, offset tables might be out of sync."); 

  return NS_OK;
}

nsresult
nsTextServicesDocument::JoinNodes(nsIDOMNode  *aLeftNode,
                                  nsIDOMNode  *aRightNode,
                                  nsIDOMNode  *aParent)
{
  PRInt32 i;
  PRUint16 type;
  nsresult result;

  //**** KDEBUG ****
  // printf("** JoinNodes: 0x%.8x  0x%.8x  0x%.8x\n", aLeftNode, aRightNode, aParent);
  // fflush(stdout);
  //**** KDEBUG ****

  // Make sure that both nodes are text nodes!

  result = aLeftNode->GetNodeType(&type);

  if (NS_FAILED(result))
    return PR_FALSE;

  if (nsIDOMNode::TEXT_NODE != type)
  {
    NS_ASSERTION(0, "JoinNode called with a non-text left node!");
    return NS_ERROR_FAILURE;
  }

  result = aRightNode->GetNodeType(&type);

  if (NS_FAILED(result))
    return PR_FALSE;

  if (nsIDOMNode::TEXT_NODE != type)
  {
    NS_ASSERTION(0, "JoinNode called with a non-text right node!");
    return NS_ERROR_FAILURE;
  }

  // Note: The editor merges the contents of the left node into the
  //       contents of the right.

  PRInt32 leftIndex, rightIndex;
  PRBool leftHasEntry, rightHasEntry;

  result = NodeHasOffsetEntry(&mOffsetTable, aLeftNode, &leftHasEntry, &leftIndex);

  if (NS_FAILED(result))
    return result;

  if (!leftHasEntry)
  {
    // XXX: Not sure if we should be throwing an error here!
    NS_ASSERTION(0, "JoinNode called with node not listed in offset table.");
    return NS_ERROR_FAILURE;
  }

  result = NodeHasOffsetEntry(&mOffsetTable, aRightNode, &rightHasEntry, &rightIndex);

  if (NS_FAILED(result))
    return result;

  if (!rightHasEntry)
  {
    // XXX: Not sure if we should be throwing an error here!
    NS_ASSERTION(0, "JoinNode called with node not listed in offset table.");
    return NS_ERROR_FAILURE;
  }

  NS_ASSERTION(leftIndex < rightIndex, "Indexes out of order.");

  if (leftIndex > rightIndex)
  {
    // Don't know how to handle this situation.
    return NS_ERROR_FAILURE;
  }

  LOCK_DOC(this);

  OffsetEntry *entry = (OffsetEntry *)mOffsetTable[rightIndex];
  NS_ASSERTION(entry->mNodeOffset == 0, "Unexpected offset value for rightIndex.");

  // Run through the table and change all entries referring to
  // the left node so that they now refer to the right node:

  nsAutoString str;
  result = aLeftNode->GetNodeValue(str);
  PRInt32 nodeLength = str.Length();

  for (i = leftIndex; i < rightIndex; i++)
  {
    entry = (OffsetEntry *)mOffsetTable[i];

    if (entry->mNode == aLeftNode)
    {
      if (entry->mIsValid)
        entry->mNode = aRightNode;
    }
    else
      break;
  }

  // Run through the table and adjust the node offsets
  // for all entries referring to the right node.

  for (i = rightIndex; i < mOffsetTable.Count(); i++)
  {
    entry = (OffsetEntry *)mOffsetTable[i];

    if (entry->mNode == aRightNode)
    {
      if (entry->mIsValid)
        entry->mNodeOffset += nodeLength;
    }
    else
      break;
  }

  // Now check to see if the iterator is pointing to the
  // left node. If it is, make it point to the right node!

  nsCOMPtr<nsIContent> leftContent = do_QueryInterface(aLeftNode);
  nsCOMPtr<nsIContent> rightContent = do_QueryInterface(aRightNode);

  if (!leftContent || !rightContent)
  {
    UNLOCK_DOC(this);
    return NS_ERROR_FAILURE;
  }

  if (mIterator->GetCurrentNode() == leftContent)
    result = mIterator->PositionAt(rightContent);

  UNLOCK_DOC(this);

  return NS_OK;
}

nsresult
nsTextServicesDocument::CreateContentIterator(nsIDOMRange *aRange, nsIContentIterator **aIterator)
{
  nsresult result;

  if (!aRange || !aIterator)
    return NS_ERROR_NULL_POINTER;

  *aIterator = 0;

  // Create a nsFilteredContentIterator
  // This class wraps the ContentIterator in order to give itself a chance 
  // to filter out certain content nodes
  nsFilteredContentIterator* filter = new nsFilteredContentIterator(mTxtSvcFilter);
  *aIterator = NS_STATIC_CAST(nsIContentIterator *, filter);
  if (*aIterator) {
    NS_IF_ADDREF(*aIterator);
    result = filter ? NS_OK : NS_ERROR_FAILURE;
  } else {
    delete filter;
    result = NS_ERROR_FAILURE;
  }
  NS_ENSURE_SUCCESS(result, result);

  if (!*aIterator)
    return NS_ERROR_NULL_POINTER;

  result = (*aIterator)->Init(aRange);

  if (NS_FAILED(result))
  {
    NS_RELEASE((*aIterator));
    *aIterator = 0;
    return result;
  }

  return NS_OK;
}

nsresult
nsTextServicesDocument::GetDocumentContentRootNode(nsIDOMNode **aNode)
{
  nsresult result;

  if (!aNode)
    return NS_ERROR_NULL_POINTER;

  *aNode = 0;

  if (!mDOMDocument)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMHTMLDocument> htmlDoc = do_QueryInterface(mDOMDocument);

  if (htmlDoc)
  {
    // For HTML documents, the content root node is the body.

    nsCOMPtr<nsIDOMHTMLElement> bodyElement;

    result = htmlDoc->GetBody(getter_AddRefs(bodyElement));

    if (NS_FAILED(result))
      return result;

    if (!bodyElement)
      return NS_ERROR_FAILURE;

    result = bodyElement->QueryInterface(NS_GET_IID(nsIDOMNode), (void **)aNode);
  }
  else
  {
    // For non-HTML documents, the content root node will be the document element.

    nsCOMPtr<nsIDOMElement> docElement;

    result = mDOMDocument->GetDocumentElement(getter_AddRefs(docElement));

    if (NS_FAILED(result))
      return result;

    if (!docElement)
      return NS_ERROR_FAILURE;

    result = docElement->QueryInterface(NS_GET_IID(nsIDOMNode), (void **)aNode);
  }

  return result;
}

nsresult
nsTextServicesDocument::CreateDocumentContentRange(nsIDOMRange **aRange)
{
  nsresult result;

  if (!aRange)
    return NS_ERROR_NULL_POINTER;

  *aRange = 0;

  nsCOMPtr<nsIDOMNode>node;

  result = GetDocumentContentRootNode(getter_AddRefs(node));

  if (NS_FAILED(result))
    return result;

  if (!node)
    return NS_ERROR_NULL_POINTER;

  result = nsComponentManager::CreateInstance("@mozilla.org/content/range;1", nsnull,
                                              NS_GET_IID(nsIDOMRange), 
                                              (void **)aRange);

  if (NS_FAILED(result))
    return result;

  if (!*aRange)
    return NS_ERROR_NULL_POINTER;

  result = (*aRange)->SelectNodeContents(node);

  if (NS_FAILED(result))
  {
    NS_RELEASE((*aRange));
    *aRange = 0;
    return result;
  }

  return NS_OK;
}

nsresult
nsTextServicesDocument::CreateDocumentContentRootToNodeOffsetRange(nsIDOMNode *aParent, PRInt32 aOffset, PRBool aToStart, nsIDOMRange **aRange)
{
  nsresult result;

  if (!aParent || !aRange)
    return NS_ERROR_NULL_POINTER;

  *aRange = 0;

  NS_ASSERTION(aOffset >= 0, "Invalid offset!");

  if (aOffset < 0)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMNode> bodyNode; 

  result = GetDocumentContentRootNode(getter_AddRefs(bodyNode));

  if (NS_FAILED(result))
    return result;

  if (!bodyNode)
    return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIDOMNode> startNode;
  nsCOMPtr<nsIDOMNode> endNode;
  PRInt32 startOffset, endOffset;

  if (aToStart)
  {
    // The range should begin at the start of the document
    // and extend up until (aParent, aOffset).

    startNode   = bodyNode;
    startOffset = 0;
    endNode     = do_QueryInterface(aParent);
    endOffset   = aOffset;
  }
  else
  {
    // The range should begin at (aParent, aOffset) and
    // extend to the end of the document.

    nsCOMPtr<nsIDOMNodeList> nodeList;
    PRUint32 nodeListLength;

    startNode   = do_QueryInterface(aParent);
    startOffset = aOffset;
    endNode     = bodyNode;
    endOffset   = 0;

    result = bodyNode->GetChildNodes(getter_AddRefs(nodeList));

    if (NS_FAILED(result))
      return NS_ERROR_FAILURE;

    if (nodeList)
    {
      result = nodeList->GetLength(&nodeListLength);

      if (NS_FAILED(result))
        return NS_ERROR_FAILURE;

      endOffset = (PRInt32)nodeListLength;
    }
  }

  result = nsComponentManager::CreateInstance("@mozilla.org/content/range;1", nsnull,
                                              NS_GET_IID(nsIDOMRange), 
                                              (void **)aRange);

  if (NS_FAILED(result))
    return result;

  if (!*aRange)
    return NS_ERROR_NULL_POINTER;

  result = (*aRange)->SetStart(startNode, startOffset);

  if (NS_SUCCEEDED(result))
    result = (*aRange)->SetEnd(endNode, endOffset);

  if (NS_FAILED(result))
  {
    NS_RELEASE((*aRange));
    *aRange = 0;
  }

  return result;
}

nsresult
nsTextServicesDocument::CreateDocumentContentIterator(nsIContentIterator **aIterator)
{
  nsresult result;

  if (!aIterator)
    return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIDOMRange> range;

  result = CreateDocumentContentRange(getter_AddRefs(range));

  if (NS_FAILED(result))
    return result;

  result = CreateContentIterator(range, aIterator);

  return result;
}

nsresult
nsTextServicesDocument::AdjustContentIterator()
{
  nsresult result = NS_OK;

  if (!mIterator)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMNode> node(do_QueryInterface(mIterator->GetCurrentNode()));

  if (!node)
    return NS_ERROR_FAILURE;

  nsIDOMNode *nodePtr = node.get();
  PRInt32 tcount      = mOffsetTable.Count();

  nsIDOMNode *prevValidNode = 0;
  nsIDOMNode *nextValidNode = 0;
  PRBool foundEntry = PR_FALSE;
  OffsetEntry *entry;

  for (PRInt32 i = 0; i < tcount && !nextValidNode; i++)
  {
    entry = (OffsetEntry *)mOffsetTable[i];

    if (!entry)
      return NS_ERROR_FAILURE;

    if (entry->mNode == nodePtr)
    {
      if (entry->mIsValid)
      {
        // The iterator is still pointing to something valid!
        // Do nothing!

        return NS_OK;
      }
      else
      {
        // We found an invalid entry that points to
        // the current iterator node. Stop looking for
        // a previous valid node!

        foundEntry = PR_TRUE;
      }
    }

    if (entry->mIsValid)
    {
      if (!foundEntry)
        prevValidNode = entry->mNode;
      else
        nextValidNode = entry->mNode;
    }
  }

  nsCOMPtr<nsIContent> content;

  if (prevValidNode)
    content = do_QueryInterface(prevValidNode);
  else if (nextValidNode)
    content = do_QueryInterface(nextValidNode);

  if (content)
  {
    result = mIterator->PositionAt(content);

    if (NS_FAILED(result))
      mIteratorStatus = eIsDone;
    else
      mIteratorStatus = eValid;

    return result;
  }

  // If we get here, there aren't any valid entries
  // in the offset table! Try to position the iterator
  // on the next text block first, then previous if
  // one doesn't exist!

  if (mNextTextBlock)
  {
    result = mIterator->PositionAt(mNextTextBlock);

    if (NS_FAILED(result))
    {
      mIteratorStatus = eIsDone;
      return result;
    }

    mIteratorStatus = eNext;
  }
  else if (mPrevTextBlock)
  {
    result = mIterator->PositionAt(mPrevTextBlock);

    if (NS_FAILED(result))
    {
      mIteratorStatus = eIsDone;
      return result;
    }

    mIteratorStatus = ePrev;
  }
  else
    mIteratorStatus = eIsDone;

  return NS_OK;
}

PRBool
nsTextServicesDocument::DidSkip(nsIContentIterator* aFilteredIter)
{
  // We can assume here that the Iterator is a nsFilteredContentIterator because
  // all the iterator are created in CreateContentIterator which create a 
  // nsFilteredContentIterator
  // So if the iterator bailed on one of the "filtered" content nodes then we 
  // consider that to be a block and bail with PR_TRUE
  if (aFilteredIter) {
    nsFilteredContentIterator* filter = NS_STATIC_CAST(nsFilteredContentIterator *, aFilteredIter);
    if (filter && filter->DidSkip()) {
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}

void
nsTextServicesDocument::ClearDidSkip(nsIContentIterator* aFilteredIter)
{
  // Clear filter's skip flag
  if (aFilteredIter) {
    nsFilteredContentIterator* filter = NS_STATIC_CAST(nsFilteredContentIterator *, aFilteredIter);
    filter->ClearDidSkip();
  }
}

PRBool
nsTextServicesDocument::IsBlockNode(nsIContent *aContent)
{
  nsIAtom *atom = aContent->Tag();

  return (sAAtom       != atom &&
          sAddressAtom != atom &&
          sBigAtom     != atom &&
          sBlinkAtom   != atom &&
          sBAtom       != atom &&
          sCiteAtom    != atom &&
          sCodeAtom    != atom &&
          sDfnAtom     != atom &&
          sEmAtom      != atom &&
          sFontAtom    != atom &&
          sIAtom       != atom &&
          sKbdAtom     != atom &&
          sKeygenAtom  != atom &&
          sNobrAtom    != atom &&
          sSAtom       != atom &&
          sSampAtom    != atom &&
          sSmallAtom   != atom &&
          sSpacerAtom  != atom &&
          sSpanAtom    != atom &&
          sStrikeAtom  != atom &&
          sStrongAtom  != atom &&
          sSubAtom     != atom &&
          sSupAtom     != atom &&
          sTtAtom      != atom &&
          sUAtom       != atom &&
          sVarAtom     != atom &&
          sWbrAtom     != atom);
}

PRBool
nsTextServicesDocument::HasSameBlockNodeParent(nsIContent *aContent1, nsIContent *aContent2)
{
  nsIContent* p1 = aContent1->GetParent();
  nsIContent* p2 = aContent2->GetParent();

  // Quick test:

  if (p1 == p2)
    return PR_TRUE;

  // Walk up the parent hierarchy looking for closest block boundary node:

  while (p1 && !IsBlockNode(p1))
  {
    p1 = p1->GetParent();
  }

  while (p2 && !IsBlockNode(p2))
  {
    p2 = p2->GetParent();
  }

  return p1 == p2;
}

PRBool
nsTextServicesDocument::IsTextNode(nsIContent *aContent)
{
  if (!aContent)
    return PR_FALSE;

  nsCOMPtr<nsIDOMNode> node = do_QueryInterface(aContent);

  return IsTextNode(node);
}

PRBool
nsTextServicesDocument::IsTextNode(nsIDOMNode *aNode)
{
  if (!aNode)
    return PR_FALSE;

  PRUint16 type;

  nsresult result = aNode->GetNodeType(&type);

  if (NS_FAILED(result))
    return PR_FALSE;

  return nsIDOMNode::TEXT_NODE == type;
}

nsresult
nsTextServicesDocument::SetSelectionInternal(PRInt32 aOffset, PRInt32 aLength, PRBool aDoUpdate)
{
  nsresult result = NS_OK;

  if (!mSelCon || aOffset < 0 || aLength < 0)
    return NS_ERROR_FAILURE;

  nsIDOMNode *sNode = 0, *eNode = 0;
  PRInt32 i, sOffset = 0, eOffset = 0;
  OffsetEntry *entry;

  // Find start of selection in node offset terms:

  for (i = 0; !sNode && i < mOffsetTable.Count(); i++)
  {
    entry = (OffsetEntry *)mOffsetTable[i];
    if (entry->mIsValid)
    {
      if (entry->mIsInsertedText)
      {
        // Caret can only be placed at the end of an
        // inserted text offset entry, if the offsets
        // match exactly!

        if (entry->mStrOffset == aOffset)
        {
          sNode   = entry->mNode;
          sOffset = entry->mNodeOffset + entry->mLength;
        }
      }
      else if (aOffset >= entry->mStrOffset)
      {
        PRBool foundEntry = PR_FALSE;
        PRInt32 strEndOffset = entry->mStrOffset + entry->mLength;

        if (aOffset < strEndOffset)
          foundEntry = PR_TRUE;
        else if (aOffset == strEndOffset)
        {
          // Peek after this entry to see if we have any
          // inserted text entries belonging to the same
          // entry->mNode. If so, we have to place the selection
          // after it!

          if ((i+1) < mOffsetTable.Count())
          {
            OffsetEntry *nextEntry = (OffsetEntry *)mOffsetTable[i+1];

            if (!nextEntry->mIsValid || nextEntry->mStrOffset != aOffset)
            {
              // Next offset entry isn't an exact match, so we'll
              // just use the current entry.
              foundEntry = PR_TRUE;
            }
          }
        }

        if (foundEntry)
        {
          sNode   = entry->mNode;
          sOffset = entry->mNodeOffset + aOffset - entry->mStrOffset;
        }
      }

      if (sNode)
      {
        mSelStartIndex  = i;
        mSelStartOffset = aOffset;
      }
    }
  }

  if (!sNode)
    return NS_ERROR_FAILURE;

  // XXX: If we ever get a SetSelection() method in nsIEditor, we should
  //      use it.

  nsCOMPtr<nsISelection> selection;

  if (aDoUpdate)
  {
    result = mSelCon->GetSelection(nsISelectionController::SELECTION_NORMAL, getter_AddRefs(selection));

    if (NS_FAILED(result))
      return result;

    result = selection->Collapse(sNode, sOffset);

    if (NS_FAILED(result))
      return result;
   }

  if (aLength <= 0)
  {
    // We have a collapsed selection. (Caret)

    mSelEndIndex  = mSelStartIndex;
    mSelEndOffset = mSelStartOffset;

   //**** KDEBUG ****
   // printf("\n* Sel: (%2d, %4d) (%2d, %4d)\n", mSelStartIndex, mSelStartOffset, mSelEndIndex, mSelEndOffset);
   //**** KDEBUG ****

    return NS_OK;
  }

  // Find the end of the selection in node offset terms:

  PRInt32 endOffset = aOffset + aLength;

  for (i = mOffsetTable.Count() - 1; !eNode && i >= 0; i--)
  {
    entry = (OffsetEntry *)mOffsetTable[i];
    
    if (entry->mIsValid)
    {
      if (entry->mIsInsertedText)
      {
        if (entry->mStrOffset == eOffset)
        {
          // If the selection ends on an inserted text offset entry,
          // the selection includes the entire entry!

          eNode   = entry->mNode;
          eOffset = entry->mNodeOffset + entry->mLength;
        }
      }
      else if (endOffset >= entry->mStrOffset && endOffset <= entry->mStrOffset + entry->mLength)
      {
        eNode   = entry->mNode;
        eOffset = entry->mNodeOffset + endOffset - entry->mStrOffset;
      }

      if (eNode)
      {
        mSelEndIndex  = i;
        mSelEndOffset = endOffset;
      }
    }
  }

  if (aDoUpdate && eNode)
  {
    result = selection->Extend(eNode, eOffset);

    if (NS_FAILED(result))
      return result;
  }

  //**** KDEBUG ****
  // printf("\n * Sel: (%2d, %4d) (%2d, %4d)\n", mSelStartIndex, mSelStartOffset, mSelEndIndex, mSelEndOffset);
  //**** KDEBUG ****

  return result;
}

nsresult
nsTextServicesDocument::GetSelection(nsITextServicesDocument::TSDBlockSelectionStatus *aSelStatus, PRInt32 *aSelOffset, PRInt32 *aSelLength)
{
  nsresult result;

  if (!aSelStatus || !aSelOffset || !aSelLength)
    return NS_ERROR_NULL_POINTER;

  *aSelStatus = nsITextServicesDocument::eBlockNotFound;
  *aSelOffset = -1;
  *aSelLength = -1;

  if (!mDOMDocument || !mSelCon)
    return NS_ERROR_FAILURE;

  if (mIteratorStatus == nsTextServicesDocument::eIsDone)
    return NS_OK;

  nsCOMPtr<nsISelection> selection;
  PRBool isCollapsed;

  result = mSelCon->GetSelection(nsISelectionController::SELECTION_NORMAL, getter_AddRefs(selection));

  if (NS_FAILED(result))
    return result;

  if (!selection)
    return NS_ERROR_FAILURE;

  result = selection->GetIsCollapsed(&isCollapsed);

  if (NS_FAILED(result))
    return result;

  // XXX: If we expose this method publicly, we need to
  //      add LOCK_DOC/UNLOCK_DOC calls!

  // LOCK_DOC(this);

  if (isCollapsed)
    result = GetCollapsedSelection(aSelStatus, aSelOffset, aSelLength);
  else
    result = GetUncollapsedSelection(aSelStatus, aSelOffset, aSelLength);

  // UNLOCK_DOC(this);

  return result;
}

nsresult
nsTextServicesDocument::GetCollapsedSelection(nsITextServicesDocument::TSDBlockSelectionStatus *aSelStatus, PRInt32 *aSelOffset, PRInt32 *aSelLength)
{
  nsresult result;
  nsCOMPtr<nsISelection> selection;

  result = mSelCon->GetSelection(nsISelectionController::SELECTION_NORMAL, getter_AddRefs(selection));

  if (NS_FAILED(result))
    return result;

  if (!selection)
    return NS_ERROR_FAILURE;

  // The calling function should have done the GetIsCollapsed()
  // check already. Just assume it's collapsed!

  nsCOMPtr<nsIDOMRange> range;
  OffsetEntry *entry;
  nsCOMPtr<nsIDOMNode> parent;
  PRInt32 offset, tableCount, i;
  PRInt32 e1s1, e2s1;

  OffsetEntry *eStart, *eEnd;
  PRInt32 eStartOffset, eEndOffset;


  *aSelStatus = nsITextServicesDocument::eBlockOutside;
  *aSelOffset = *aSelLength = -1;

  tableCount = mOffsetTable.Count();

  if (tableCount == 0)
    return NS_OK;

  // Get pointers to the first and last offset entries
  // in the table.

  eStart = (OffsetEntry *)mOffsetTable[0];

  if (tableCount > 1)
    eEnd = (OffsetEntry *)mOffsetTable[tableCount - 1];
  else
    eEnd = eStart;

  eStartOffset = eStart->mNodeOffset;
  eEndOffset   = eEnd->mNodeOffset + eEnd->mLength;

  result = selection->GetRangeAt(0, getter_AddRefs(range));

  if (NS_FAILED(result))
    return result;

  result = range->GetStartContainer(getter_AddRefs(parent));

  if (NS_FAILED(result))
    return result;

  result = range->GetStartOffset(&offset);

  if (NS_FAILED(result))
    return result;

  result = ComparePoints(eStart->mNode, eStartOffset, parent, offset, &e1s1);

  if (NS_FAILED(result))
    return result;

  result = ComparePoints(eEnd->mNode, eEndOffset, parent, offset, &e2s1);

  if (NS_FAILED(result))
    return result;

  if (e1s1 > 0 || e2s1 < 0)
  {
    // We're done if the caret is outside the
    // current text block.

    return NS_OK;
  }

  if (IsTextNode(parent))
  {
    // Good news, the caret is in a text node. Look
    // through the offset table for the entry that
    // matches it's parent and offset.

    for (i = 0; i < tableCount; i++)
    {
      entry = (OffsetEntry *)mOffsetTable[i];

      if (!entry)
        return NS_ERROR_FAILURE;

      if (entry->mNode == parent.get() &&
          entry->mNodeOffset <= offset && offset <= (entry->mNodeOffset + entry->mLength))
      {
        *aSelStatus = nsITextServicesDocument::eBlockContains;
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
  // this range, with it's initial position set to the closest
  // child of this non-text node. Then look for the closest text
  // node.

  nsCOMPtr<nsIDOMNode> node, saveNode;
  nsCOMPtr<nsIDOMNodeList> children;
  nsCOMPtr<nsIContentIterator> iter;
  PRBool hasChildren;

  result = CreateRange(eStart->mNode, eStartOffset, eEnd->mNode, eEndOffset, getter_AddRefs(range));

  if (NS_FAILED(result))
    return result;

  result = CreateContentIterator(range, getter_AddRefs(iter));

  if (NS_FAILED(result))
    return result;

  result = parent->HasChildNodes(&hasChildren);

  if (NS_FAILED(result))
    return result;

  if (hasChildren)
  {
    // XXX: We need to make sure that all of parent's
    //      children are in the text block.

    // If the parent has children, position the iterator
    // on the child that is to the left of the offset.

    PRUint32 childIndex = (PRUint32)offset;

    result = parent->GetChildNodes(getter_AddRefs(children));

    if (NS_FAILED(result))
      return result;

    if (!children)
      return NS_ERROR_FAILURE;

    if (childIndex > 0)
    {
      PRUint32 numChildren;

      result = children->GetLength(&numChildren);

      if (NS_FAILED(result))
        return result;

      NS_ASSERTION(childIndex <= numChildren, "Invalid selection offset!");

      if (childIndex > numChildren)
        childIndex = numChildren;

      childIndex -= 1;
    }

    result = children->Item(childIndex, getter_AddRefs(saveNode));

    if (NS_FAILED(result))
      return result;

    nsCOMPtr<nsIContent> content(do_QueryInterface(saveNode));

    if (!content)
      return NS_ERROR_FAILURE;

    result = iter->PositionAt(content);

    if (NS_FAILED(result))
      return result;
  }
  else
  {
    // The parent has no children, so position the iterator
    // on the parent.

    nsCOMPtr<nsIContent> content(do_QueryInterface(parent));

    if (!content)
      return NS_ERROR_FAILURE;

    result = iter->PositionAt(content);

    if (NS_FAILED(result))
      return result;

    saveNode = parent;
  }

  // Now iterate to the left, towards the beginning of
  // the text block, to find the first text node you
  // come across.

  while (!iter->IsDone())
  {
    nsIContent *content = iter->GetCurrentNode();

    if (IsTextNode(content))
    {
      node = do_QueryInterface(content);

      if (!node)
        return NS_ERROR_FAILURE;

      break;
    }

    node = nsnull;

    iter->Prev();
  }

  if (node)
  {
    // We found a node, now set the offset to the end
    // of the text node.

    nsAutoString str;
    result = node->GetNodeValue(str);

    if (NS_FAILED(result))
      return result;

    offset = str.Length();
  }
  else
  {
    // We should never really get here, but I'm paranoid.

    // We didn't find a text node above, so iterate to
    // the right, towards the end of the text block, looking
    // for a text node.

    {
      nsCOMPtr<nsIContent> content(do_QueryInterface(saveNode));

      result = iter->PositionAt(content);

      if (NS_FAILED(result))
        return result;
    }

    while (!iter->IsDone())
    {
      nsIContent *content = iter->GetCurrentNode();

      if (IsTextNode(content))
      {
        node = do_QueryInterface(content);

        if (!node)
          return NS_ERROR_FAILURE;

        break;
      }

      node = nsnull;

      iter->Next();
    }

    if (!node)
      return NS_ERROR_FAILURE;

    // We found a text node, so set the offset to
    // the begining of the node.

    offset = 0;
  }

  for (i = 0; i < tableCount; i++)
  {
    entry = (OffsetEntry *)mOffsetTable[i];

    if (!entry)
      return NS_ERROR_FAILURE;

    if (entry->mNode == node.get() &&
        entry->mNodeOffset <= offset && offset <= (entry->mNodeOffset + entry->mLength))
    {
      *aSelStatus = nsITextServicesDocument::eBlockContains;
      *aSelOffset = entry->mStrOffset + (offset - entry->mNodeOffset);
      *aSelLength = 0;

      // Now move the caret so that it is actually in the text node.
      // We do this to keep things in sync.
      //
      // In most cases, the user shouldn't see any movement in the caret
      // on screen.

      result = SetSelectionInternal(*aSelOffset, *aSelLength, PR_TRUE);

      return result;
    }
  }

  return NS_ERROR_FAILURE;
}

nsresult
nsTextServicesDocument::GetUncollapsedSelection(nsITextServicesDocument::TSDBlockSelectionStatus *aSelStatus, PRInt32 *aSelOffset, PRInt32 *aSelLength)
{
  nsresult result;

  nsCOMPtr<nsISelection> selection;
  nsCOMPtr<nsIDOMRange> range;
  OffsetEntry *entry;

  result = mSelCon->GetSelection(nsISelectionController::SELECTION_NORMAL, getter_AddRefs(selection));

  if (NS_FAILED(result))
    return result;

  if (!selection)
    return NS_ERROR_FAILURE;

  // It is assumed that the calling function has made sure that the
  // selection is not collapsed, and that the input params to this
  // method are initialized to some defaults.

  nsCOMPtr<nsIDOMNode> startParent, endParent;
  PRInt32 startOffset, endOffset;
  PRInt32 rangeCount, tableCount, i;
  PRInt32 e1s1, e1s2, e2s1, e2s2;

  OffsetEntry *eStart, *eEnd;
  PRInt32 eStartOffset, eEndOffset;

  tableCount = mOffsetTable.Count();

  // Get pointers to the first and last offset entries
  // in the table.

  eStart = (OffsetEntry *)mOffsetTable[0];

  if (tableCount > 1)
    eEnd = (OffsetEntry *)mOffsetTable[tableCount - 1];
  else
    eEnd = eStart;

  eStartOffset = eStart->mNodeOffset;
  eEndOffset   = eEnd->mNodeOffset + eEnd->mLength;

  result = selection->GetRangeCount(&rangeCount);

  if (NS_FAILED(result))
    return result;

  // Find the first range in the selection that intersects
  // the current text block.

  for (i = 0; i < rangeCount; i++)
  {
    result = selection->GetRangeAt(i, getter_AddRefs(range));

    if (NS_FAILED(result))
      return result;

    result = GetRangeEndPoints(range,
                               getter_AddRefs(startParent), &startOffset,
                               getter_AddRefs(endParent), &endOffset);

    if (NS_FAILED(result))
      return result;

    result = ComparePoints(eStart->mNode, eStartOffset, endParent, endOffset, &e1s2);

    if (NS_FAILED(result))
      return result;

    result = ComparePoints(eEnd->mNode, eEndOffset, startParent, startOffset, &e2s1);

    if (NS_FAILED(result))
      return result;

    // Break out of the loop if the text block intersects the current range.

    if (e1s2 <= 0 && e2s1 >= 0)
      break;
  }

  // We're done if we didn't find an intersecting range.

  if (rangeCount < 1 || e1s2 > 0 || e2s1 < 0)
  {
    *aSelStatus = nsITextServicesDocument::eBlockOutside;
    *aSelOffset = *aSelLength = -1;
    return NS_OK;
  }

  // Now that we have an intersecting range, find out more info:

  result = ComparePoints(eStart->mNode, eStartOffset, startParent, startOffset, &e1s1);

  if (NS_FAILED(result))
    return result;

  result = ComparePoints(eEnd->mNode, eEndOffset, endParent, endOffset, &e2s2);

  if (NS_FAILED(result))
    return result;

  if (rangeCount > 1)
  {
    // There are multiple selection ranges, we only deal
    // with the first one that intersects the current,
    // text block, so mark this a as a partial.

    *aSelStatus = nsITextServicesDocument::eBlockPartial;
  }
  else if (e1s1 > 0 && e2s2 < 0)
  {
    // The range extends beyond the start and
    // end of the current text block.

    *aSelStatus = nsITextServicesDocument::eBlockInside;
  }
  else if (e1s1 <= 0 && e2s2 >= 0)
  {
    // The current text block contains the entire
    // range.

    *aSelStatus = nsITextServicesDocument::eBlockContains;
  }
  else
  {
    // The range partially intersects the block.

    *aSelStatus = nsITextServicesDocument::eBlockPartial;
  }

  // Now create a range based on the intersection of the
  // text block and range:

  nsCOMPtr<nsIDOMNode> p1, p2;
  PRInt32     o1,  o2;

  // The start of the range will be the rightmost
  // start node.

  if (e1s1 >= 0)
  {
    p1 = do_QueryInterface(eStart->mNode);
    o1 = eStartOffset;
  }
  else
  {
    p1 = startParent;
    o1 = startOffset;
  }

  // The end of the range will be the leftmost
  // end node.

  if (e2s2 <= 0)
  {
    p2 = do_QueryInterface(eEnd->mNode);
    o2 = eEndOffset;
  }
  else
  {
    p2 = endParent;
    o2 = endOffset;
  }

  result = CreateRange(p1, o1, p2, o2, getter_AddRefs(range));

  if (NS_FAILED(result))
    return result;

  // Now iterate over this range to figure out the selection's
  // block offset and length.

  nsCOMPtr<nsIContentIterator> iter;

  result = CreateContentIterator(range, getter_AddRefs(iter));

  if (NS_FAILED(result))
    return result;

  // Find the first text node in the range.
  
  PRBool found;
  nsIContent *content;

  iter->First();

  if (!IsTextNode(p1))
  {
    found = PR_FALSE;

    while (!iter->IsDone())
    {
      content = iter->GetCurrentNode();

      if (IsTextNode(content))
      {
        p1 = do_QueryInterface(content);

        if (!p1)
          return NS_ERROR_FAILURE;

        o1 = 0;
        found = PR_TRUE;

        break;
      }

      iter->Next();
    }

    if (!found)
      return NS_ERROR_FAILURE;
  }

  // Find the last text node in the range.

  iter->Last();

  if (! IsTextNode(p2))
  {
    found = PR_FALSE;

    while (!iter->IsDone())
    {
      content = iter->GetCurrentNode();

      if (IsTextNode(content))
      {
        p2 = do_QueryInterface(content);

        if (!p2)
          return NS_ERROR_FAILURE;

        nsString str;

        result = p2->GetNodeValue(str);

        if (NS_FAILED(result))
          return result;

        o2 = str.Length();
        found = PR_TRUE;

        break;
      }

      iter->Prev();
    }

    if (!found)
      return NS_ERROR_FAILURE;
  }

  found    = PR_FALSE;
  *aSelLength = 0;

  for (i = 0; i < tableCount; i++)
  {
    entry = (OffsetEntry *)mOffsetTable[i];

    if (!entry)
      return NS_ERROR_FAILURE;

    if (!found)
    {
      if (entry->mNode == p1.get() &&
          entry->mNodeOffset <= o1 && o1 <= (entry->mNodeOffset + entry->mLength))
      {
        *aSelOffset = entry->mStrOffset + (o1 - entry->mNodeOffset);

        if (p1 == p2 &&
            entry->mNodeOffset <= o2 && o2 <= (entry->mNodeOffset + entry->mLength))
        {
          // The start and end of the range are in the same offset
          // entry. Calculate the length of the range then we're done.

          *aSelLength = o2 - o1;
          break;
        }
        else
        {
          // Add the length of the sub string in this offset entry
          // that follows the start of the range.

          *aSelLength = entry->mLength - (o1 - entry->mNodeOffset);
        }

        found = PR_TRUE;
      }
    }
    else // found
    {
      if (entry->mNode == p2.get() &&
          entry->mNodeOffset <= o2 && o2 <= (entry->mNodeOffset + entry->mLength))
      {
        // We found the end of the range. Calculate the length of the
        // sub string that is before the end of the range, then we're done.

        *aSelLength += o2 - entry->mNodeOffset;
        break;
      }
      else
      {
        // The entire entry must be in the range.

        *aSelLength += entry->mLength;
      }
    }
  }

  return result;
}

PRBool
nsTextServicesDocument::SelectionIsCollapsed()
{
  return(mSelStartIndex == mSelEndIndex && mSelStartOffset == mSelEndOffset);
}

PRBool
nsTextServicesDocument::SelectionIsValid()
{
  return(mSelStartIndex >= 0);
}

nsresult
nsTextServicesDocument::ComparePoints(nsIDOMNode* aParent1, PRInt32 aOffset1,
                                      nsIDOMNode* aParent2, PRInt32 aOffset2,
                                      PRInt32 *aResult)
{
  nsresult result;
  
  if (!sRangeHelper) {
    result = CallGetService("@mozilla.org/content/range-utils;1",
                            &sRangeHelper);
    if (!sRangeHelper)
      return result;
  }

  *aResult = sRangeHelper->ComparePoints(aParent1, aOffset1,
                                         aParent2, aOffset2);
  return NS_OK;
}

nsresult
nsTextServicesDocument::GetRangeEndPoints(nsIDOMRange *aRange,
                                          nsIDOMNode **aStartParent, PRInt32 *aStartOffset,
                                          nsIDOMNode **aEndParent, PRInt32 *aEndOffset)
{
  nsresult result;

  if (!aRange || !aStartParent || !aStartOffset || !aEndParent || !aEndOffset)
    return NS_ERROR_NULL_POINTER;

  result = aRange->GetStartContainer(aStartParent);

  if (NS_FAILED(result))
    return result;

  if (!aStartParent)
    return NS_ERROR_FAILURE;

  result = aRange->GetStartOffset(aStartOffset);

  if (NS_FAILED(result))
    return result;

  result = aRange->GetEndContainer(aEndParent);

  if (NS_FAILED(result))
    return result;

  if (!aEndParent)
    return NS_ERROR_FAILURE;

  result = aRange->GetEndOffset(aEndOffset);

  return result;
}


nsresult
nsTextServicesDocument::CreateRange(nsIDOMNode *aStartParent, PRInt32 aStartOffset,
                                    nsIDOMNode *aEndParent, PRInt32 aEndOffset,
                                    nsIDOMRange **aRange)
{
  nsresult result;

  result = nsComponentManager::CreateInstance("@mozilla.org/content/range;1", nsnull,
                                              NS_GET_IID(nsIDOMRange), 
                                              (void **)aRange);

  if (NS_FAILED(result))
    return result;

  if (!*aRange)
    return NS_ERROR_NULL_POINTER;

  result = (*aRange)->SetStart(aStartParent, aStartOffset);

  if (NS_SUCCEEDED(result))
    result = (*aRange)->SetEnd(aEndParent, aEndOffset);

  if (NS_FAILED(result))
  {
    NS_RELEASE((*aRange));
    *aRange = 0;
  }

  return result;
}

nsresult
nsTextServicesDocument::FirstTextNode(nsIContentIterator *aIterator,
                                      TSDIteratorStatus *aIteratorStatus)
{
  if (aIteratorStatus)
    *aIteratorStatus = nsTextServicesDocument::eIsDone;

  aIterator->First();

  while (!aIterator->IsDone())
  {
    nsIContent *content = aIterator->GetCurrentNode();

    if (IsTextNode(content))
    {
      if (aIteratorStatus)
        *aIteratorStatus = nsTextServicesDocument::eValid;
      break;
    }

    aIterator->Next();
  }

  return NS_OK;
}

nsresult
nsTextServicesDocument::LastTextNode(nsIContentIterator *aIterator,
                                     TSDIteratorStatus *aIteratorStatus)
{
  if (aIteratorStatus)
    *aIteratorStatus = nsTextServicesDocument::eIsDone;

  aIterator->Last();

  while (!aIterator->IsDone())
  {
    nsIContent *content = aIterator->GetCurrentNode();

    if (IsTextNode(content))
    {
      if (aIteratorStatus)
        *aIteratorStatus = nsTextServicesDocument::eValid;
      break;
    }

    aIterator->Prev();
  }

  return NS_OK;
}

nsresult
nsTextServicesDocument::FirstTextNodeInCurrentBlock(nsIContentIterator *iter)
{
  if (!iter)
    return NS_ERROR_NULL_POINTER;

  ClearDidSkip(iter);

  nsCOMPtr<nsIContent> last;

  // Walk backwards over adjacent text nodes until
  // we hit a block boundary:

  while (!iter->IsDone())
  {
    nsIContent *content = iter->GetCurrentNode();

    if (IsTextNode(content))
    {
      if (!last || HasSameBlockNodeParent(content, last))
        last = content;
      else
      {
        // We're done, the current text node is in a
        // different block.
        break;
      }
    }
    else if (last && IsBlockNode(content))
      break;

    iter->Prev();

    if (DidSkip(iter))
      break;
  }
  
  if (last)
    iter->PositionAt(last);

  // XXX: What should we return if last is null?

  return NS_OK;
}

nsresult
nsTextServicesDocument::FirstTextNodeInPrevBlock(nsIContentIterator *aIterator)
{
  nsCOMPtr<nsIContent> content;
  nsresult result;

  if (!aIterator)
    return NS_ERROR_NULL_POINTER;

  // XXX: What if mIterator is not currently on a text node?

  // Make sure mIterator is pointing to the first text node in the
  // current block:

  result = FirstTextNodeInCurrentBlock(aIterator);

  if (NS_FAILED(result))
    return NS_ERROR_FAILURE;

  // Point mIterator to the first node before the first text node:

  aIterator->Prev();

  if (aIterator->IsDone())
    return NS_ERROR_FAILURE;

  // Now find the first text node of the next block:

  return FirstTextNodeInCurrentBlock(aIterator);
}

nsresult
nsTextServicesDocument::FirstTextNodeInNextBlock(nsIContentIterator *aIterator)
{
  nsCOMPtr<nsIContent> prev;
  PRBool crossedBlockBoundary = PR_FALSE;

  if (!aIterator)
    return NS_ERROR_NULL_POINTER;

  ClearDidSkip(aIterator);

  while (!aIterator->IsDone())
  {
    nsIContent *content = aIterator->GetCurrentNode();

    if (IsTextNode(content))
    {
      if (!crossedBlockBoundary && (!prev || HasSameBlockNodeParent(prev, content)))
        prev = content;
      else
        break;
    }
    else if (!crossedBlockBoundary && IsBlockNode(content))
      crossedBlockBoundary = PR_TRUE;

    aIterator->Next();

    if (!crossedBlockBoundary && DidSkip(aIterator))
      crossedBlockBoundary = PR_TRUE;
  }

  return NS_OK;
}

nsresult
nsTextServicesDocument::GetFirstTextNodeInPrevBlock(nsIContent **aContent)
{
  nsresult result;

  if (!aContent)
    return NS_ERROR_NULL_POINTER;

  *aContent = 0;

  // Save the iterator's current content node so we can restore
  // it when we are done:

  nsIContent *content = mIterator->GetCurrentNode();

  result = FirstTextNodeInPrevBlock(mIterator);

  if (NS_FAILED(result))
  {
    // Try to restore the iterator before returning.
    mIterator->PositionAt(content);
    return result;
  }

  if (!mIterator->IsDone())
  {
    NS_ADDREF(*aContent = mIterator->GetCurrentNode());
  }

  // Restore the iterator:

  return mIterator->PositionAt(content);
}

nsresult
nsTextServicesDocument::GetFirstTextNodeInNextBlock(nsIContent **aContent)
{
  nsresult result;

  if (!aContent)
    return NS_ERROR_NULL_POINTER;

  *aContent = 0;

  // Save the iterator's current content node so we can restore
  // it when we are done:

  nsIContent *content = mIterator->GetCurrentNode();

  result = FirstTextNodeInNextBlock(mIterator);

  if (NS_FAILED(result))
  {
    // Try to restore the iterator before returning.
    mIterator->PositionAt(content);
    return result;
  }

  if (!mIterator->IsDone())
  {
    NS_ADDREF(*aContent = mIterator->GetCurrentNode());
  }

  // Restore the iterator:
  return mIterator->PositionAt(content);
}

nsresult
nsTextServicesDocument::CreateOffsetTable(nsVoidArray *aOffsetTable,
                                          nsIContentIterator *aIterator,
                                          TSDIteratorStatus *aIteratorStatus,
                                          nsIDOMRange *aIterRange,
                                          nsString *aStr)
{
  nsresult result = NS_OK;

  nsCOMPtr<nsIContent> first;
  nsCOMPtr<nsIContent> prev;

  if (!aIterator)
    return NS_ERROR_NULL_POINTER;

  ClearOffsetTable(aOffsetTable);

  if (aStr)
    aStr->Truncate();

  if (*aIteratorStatus == nsTextServicesDocument::eIsDone)
    return NS_OK;

  // If we have an aIterRange, retrieve the endpoints so
  // they can be used in the while loop below to trim entries
  // for text nodes that are partially selected by aIterRange.
  
  nsCOMPtr<nsIDOMNode> rngStartNode, rngEndNode;
  PRInt32 rngStartOffset = 0, rngEndOffset = 0;

  if (aIterRange)
  {
    result = GetRangeEndPoints(aIterRange,
                               getter_AddRefs(rngStartNode), &rngStartOffset,
                               getter_AddRefs(rngEndNode), &rngEndOffset);

    NS_ENSURE_SUCCESS(result, result);
  }

  // The text service could have added text nodes to the beginning
  // of the current block and called this method again. Make sure
  // we really are at the beginning of the current block:

  result = FirstTextNodeInCurrentBlock(aIterator);

  if (NS_FAILED(result))
    return result;

  PRInt32 offset = 0;

  ClearDidSkip(aIterator);

  while (!aIterator->IsDone())
  {
    nsIContent *content = aIterator->GetCurrentNode();

    if (IsTextNode(content))
    {
      if (!prev || HasSameBlockNodeParent(prev, content))
      {
        nsCOMPtr<nsIDOMNode> node = do_QueryInterface(content);

        if (node)
        {
          nsString str;

          result = node->GetNodeValue(str);

          if (NS_FAILED(result))
            return result;

          // Add an entry for this text node into the offset table:

          OffsetEntry *entry = new OffsetEntry(node, offset, str.Length());

          if (!entry)
            return NS_ERROR_OUT_OF_MEMORY;

          aOffsetTable->AppendElement((void *)entry);

          // If one or both of the endpoints of the iteration range
          // are in the text node for this entry, make sure the entry
          // only accounts for the portion of the text node that is
          // in the range.

          PRInt32 startOffset = 0;
          PRInt32 endOffset   = str.Length();
          PRBool adjustStr    = PR_FALSE;

          if (entry->mNode == rngStartNode)
          {
            entry->mNodeOffset = startOffset = rngStartOffset;
            adjustStr = PR_TRUE;
          }

          if (entry->mNode == rngEndNode)
          {
            endOffset = rngEndOffset;
            adjustStr = PR_TRUE;
          }

          if (adjustStr)
          {
            entry->mLength = endOffset - startOffset;
            str = Substring(str, startOffset, entry->mLength);
          }

          offset += str.Length();

          if (aStr)
          {
            // Append the text node's string to the output string:

            if (!first)
              *aStr = str;
            else
              *aStr += str;
          }
        }

        prev = content;

        if (!first)
          first = content;
      }
      else
        break;

    }
    else if (IsBlockNode(content))
      break;

    aIterator->Next();

    if (DidSkip(aIterator))
      break;
  }

  if (first)
  {
    // Always leave the iterator pointing at the first
    // text node of the current block!

    aIterator->PositionAt(first);
  }
  else
  {
    // If we never ran across a text node, the iterator
    // might have been pointing to something invalid to
    // begin with.

    *aIteratorStatus = nsTextServicesDocument::eIsDone;
  }

  return result;
}

nsresult
nsTextServicesDocument::RemoveInvalidOffsetEntries()
{
  OffsetEntry *entry;
  PRInt32 i = 0;

  while (i < mOffsetTable.Count())
  {
    entry = (OffsetEntry *)mOffsetTable[i];

    if (!entry->mIsValid)
    {
      if (!mOffsetTable.RemoveElementAt(i))
        return NS_ERROR_FAILURE;

      if (mSelStartIndex >= 0 && mSelStartIndex >= i)
      {
        // We are deleting an entry that comes before
        // mSelStartIndex, decrement mSelStartIndex so
        // that it points to the correct entry!

        NS_ASSERTION(i != mSelStartIndex, "Invalid selection index.");

        --mSelStartIndex;
        --mSelEndIndex;
      }
    }
    else
      i++;
  }

  return NS_OK;
}

nsresult
nsTextServicesDocument::ClearOffsetTable(nsVoidArray *aOffsetTable)
{
  PRInt32 i;

  for (i = 0; i < aOffsetTable->Count(); i++)
  {
    OffsetEntry *entry = (OffsetEntry *)(*aOffsetTable)[i];
    if (entry)
      delete entry;
  }

  aOffsetTable->Clear();

  return NS_OK;
}

nsresult
nsTextServicesDocument::SplitOffsetEntry(PRInt32 aTableIndex, PRInt32 aNewEntryLength)
{
  OffsetEntry *entry = (OffsetEntry *)mOffsetTable[aTableIndex];

  NS_ASSERTION((aNewEntryLength > 0), "aNewEntryLength <= 0");
  NS_ASSERTION((aNewEntryLength < entry->mLength), "aNewEntryLength >= mLength");

  if (aNewEntryLength < 1 || aNewEntryLength >= entry->mLength)
    return NS_ERROR_FAILURE;

  PRInt32 oldLength = entry->mLength - aNewEntryLength;

  OffsetEntry *newEntry = new OffsetEntry(entry->mNode,
                                          entry->mStrOffset + oldLength,
                                          aNewEntryLength);

  if (!newEntry)
    return NS_ERROR_OUT_OF_MEMORY;

  if (!mOffsetTable.InsertElementAt(newEntry, aTableIndex + 1))
  {
    delete newEntry;
    return NS_ERROR_FAILURE;
  }

   // Adjust entry fields:

   entry->mLength        = oldLength;
   newEntry->mNodeOffset = entry->mNodeOffset + oldLength;

  return NS_OK;
}

nsresult
nsTextServicesDocument::NodeHasOffsetEntry(nsVoidArray *aOffsetTable, nsIDOMNode *aNode, PRBool *aHasEntry, PRInt32 *aEntryIndex)
{
  OffsetEntry *entry;
  PRInt32 i;

  if (!aNode || !aHasEntry || !aEntryIndex)
    return NS_ERROR_NULL_POINTER;

  for (i = 0; i < aOffsetTable->Count(); i++)
  {
    entry = (OffsetEntry *)(*aOffsetTable)[i];

    if (!entry)
      return NS_ERROR_FAILURE;

    if (entry->mNode == aNode)
    {
      *aHasEntry   = PR_TRUE;
      *aEntryIndex = i;

      return NS_OK;
    }
  }

  *aHasEntry   = PR_FALSE;
  *aEntryIndex = -1;

  return NS_OK;
}

nsresult
nsTextServicesDocument::GetWordBreaker(nsIWordBreaker** aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);

  *aResult = nsnull;

  nsresult result;
  nsCOMPtr<nsIWordBreakerFactory> wbf(do_GetService(NS_LWBRK_CONTRACTID, &result));
  if (NS_SUCCEEDED(result) && wbf)
  {
    nsString wbarg;
    result = wbf->GetBreaker(wbarg, aResult);
  }

  return result;
}

nsresult
nsTextServicesDocument::FindWordBounds(nsVoidArray *aOffsetTable,
                                       nsString *aBlockStr,
                                       nsIWordBreaker *aWordBreaker,
                                       nsIDOMNode *aNode,
                                       PRInt32 aNodeOffset,
                                       nsIDOMNode **aWordStartNode,
                                       PRInt32 *aWordStartOffset,
                                       nsIDOMNode **aWordEndNode,
                                       PRInt32 *aWordEndOffset)
{
  // Initialize return values.

  if (aWordStartNode)
    *aWordStartNode = nsnull;
  if (aWordStartOffset)
    *aWordStartOffset = 0;
  if (aWordEndNode)
    *aWordEndNode = nsnull;
  if (aWordEndOffset)
    *aWordEndOffset = 0;

  PRInt32 entryIndex;
  PRBool hasEntry;

  // It's assumed that aNode is a text node. The first thing
  // we do is get it's index in the offset table so we can
  // calculate the dom point's string offset.

  nsresult result = NodeHasOffsetEntry(aOffsetTable, aNode, &hasEntry, &entryIndex);
  NS_ENSURE_SUCCESS(result, result);
  NS_ENSURE_TRUE(hasEntry, NS_ERROR_FAILURE);

  // Next we map aNodeOffset into a string offset.

  OffsetEntry *entry = (OffsetEntry *)(*aOffsetTable)[entryIndex];
  PRUint32 strOffset = entry->mStrOffset + aNodeOffset - entry->mNodeOffset;

  // Now we use the word breaker to find the beginning and end
  // of the word from our calculated string offset.

  const PRUnichar *str = aBlockStr->get();
  PRUint32 strLen = aBlockStr->Length();
  PRUint32 beginWord=0, endWord=0;

  result = aWordBreaker->FindWord(str, strLen, strOffset,
                                 &beginWord, &endWord);
  NS_ENSURE_SUCCESS(result, result);

  // Now that we have the string offsets for the beginning
  // and end of the word, run through the offset table and
  // convert them back into dom points.

  PRInt32 i, lastIndex = aOffsetTable->Count() - 1;

  for (i=0; i <= lastIndex; i++)
  {
    entry = (OffsetEntry *)(*aOffsetTable)[i];

    PRInt32 strEndOffset = entry->mStrOffset + entry->mLength;

    // Check to see if beginWord is within the range covered
    // by this entry. Note that if beginWord is after the last
    // character covered by this entry, we will use the next
    // entry if there is one.

    if (entry->mStrOffset <= beginWord &&
       (beginWord < strEndOffset || (beginWord == strEndOffset && i == lastIndex)))
    {
      if (aWordStartNode)
      {
        *aWordStartNode = entry->mNode;
        NS_IF_ADDREF(*aWordStartNode);
      }

      if (aWordStartOffset)
        *aWordStartOffset = entry->mNodeOffset + beginWord - entry->mStrOffset;

      if (!aWordEndNode && !aWordEndOffset)
      {
        // We've found our start entry, but if we're not looking
        // for end entries, we're done.

        break;
      }
    }

    // Check to see if endWord is within the range covered
    // by this entry.

    if (entry->mStrOffset <= endWord && endWord <= strEndOffset)
    {
      if (beginWord == endWord && endWord == strEndOffset && i != lastIndex)
      {
        // Wait for the next round so that we use the same entry
        // we did for aWordStartNode.

        continue;
      }

      if (aWordEndNode)
      {
        *aWordEndNode = entry->mNode;
        NS_IF_ADDREF(*aWordEndNode);
      }

      if (aWordEndOffset)
        *aWordEndOffset = entry->mNodeOffset + endWord - entry->mStrOffset;

      break;
    }
  }


  return NS_OK;
}

#ifdef DEBUG_kin
void
nsTextServicesDocument::PrintOffsetTable()
{
  OffsetEntry *entry;
  PRInt32 i;

  for (i = 0; i < mOffsetTable.Count(); i++)
  {
    entry = (OffsetEntry *)mOffsetTable[i];
    printf("ENTRY %4d: %p  %c  %c  %4d  %4d  %4d\n",
           i, entry->mNode,  entry->mIsValid ? 'V' : 'N',
           entry->mIsInsertedText ? 'I' : 'B',
           entry->mNodeOffset, entry->mStrOffset, entry->mLength);
  }

  fflush(stdout);
}

void
nsTextServicesDocument::PrintContentNode(nsIContent *aContent)
{
  nsString tmpStr, str;
  nsresult result;

  aContent->Tag()->ToString(tmpStr);
  printf("%s", NS_LossyConvertUCS2toASCII(tmpStr).get());

  nsCOMPtr<nsIDOMNode> node = do_QueryInterface(aContent);

  if (node)
  {
    PRUint16 type;

    result = node->GetNodeType(&type);

    if (NS_FAILED(result))
      return;

    if (nsIDOMNode::TEXT_NODE == type)
    {
      result = node->GetNodeValue(str);

      if (NS_FAILED(result))
        return;

      printf(":  \"%s\"", NS_LossyConvertUCS2toASCII(str).get());
    }
  }

  printf("\n");
  fflush(stdout);
}
#endif
