/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#include "nscore.h"
#include "nsLayoutCID.h"
#include "nsIAtom.h"
#include "nsString.h"
#include "nsIEnumerator.h"
#include "nsIContent.h"
#include "nsIContentIterator.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMRange.h"
#include "nsISelection.h"
#include "nsIPlaintextEditor.h"
#include "nsTextServicesDocument.h"

#include "nsIDOMElement.h"
#include "nsIDOMHTMLElement.h"
#include "nsIDOMHTMLDocument.h"

#define LOCK_DOC(doc)
#define UNLOCK_DOC(doc)

static NS_DEFINE_CID(kCContentIteratorCID, NS_CONTENTITERATOR_CID);
static NS_DEFINE_CID(kCDOMRangeCID, NS_RANGE_CID);

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kITextServicesDocumentIID, NS_ITEXTSERVICESDOCUMENT_IID);

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

// XXX: Should change these pointers to
//      nsCOMPtr<nsIAtom> pointers!

nsIAtom *nsTextServicesDocument::sAAtom;
nsIAtom *nsTextServicesDocument::sAddressAtom;
nsIAtom *nsTextServicesDocument::sBigAtom;
nsIAtom *nsTextServicesDocument::sBlinkAtom;
nsIAtom *nsTextServicesDocument::sBAtom;
nsIAtom *nsTextServicesDocument::sCiteAtom;
nsIAtom *nsTextServicesDocument::sCodeAtom;
nsIAtom *nsTextServicesDocument::sDfnAtom;
nsIAtom *nsTextServicesDocument::sEmAtom;
nsIAtom *nsTextServicesDocument::sFontAtom;
nsIAtom *nsTextServicesDocument::sIAtom;
nsIAtom *nsTextServicesDocument::sKbdAtom;
nsIAtom *nsTextServicesDocument::sKeygenAtom;
nsIAtom *nsTextServicesDocument::sNobrAtom;
nsIAtom *nsTextServicesDocument::sSAtom;
nsIAtom *nsTextServicesDocument::sSampAtom;
nsIAtom *nsTextServicesDocument::sSmallAtom;
nsIAtom *nsTextServicesDocument::sSpacerAtom;
nsIAtom *nsTextServicesDocument::sSpanAtom;      
nsIAtom *nsTextServicesDocument::sStrikeAtom;
nsIAtom *nsTextServicesDocument::sStrongAtom;
nsIAtom *nsTextServicesDocument::sSubAtom;
nsIAtom *nsTextServicesDocument::sSupAtom;
nsIAtom *nsTextServicesDocument::sTtAtom;
nsIAtom *nsTextServicesDocument::sUAtom;
nsIAtom *nsTextServicesDocument::sVarAtom;
nsIAtom *nsTextServicesDocument::sWbrAtom;

PRInt32 nsTextServicesDocument::sInstanceCount;

nsTextServicesDocument::nsTextServicesDocument()
{
  mRefCnt         = 0;

  mSelStartIndex  = -1;
  mSelStartOffset = -1;
  mSelEndIndex    = -1;
  mSelEndOffset   = -1;

  mIteratorStatus = eIsDone;

  if (sInstanceCount <= 0)
  {
    sAAtom = NS_NewAtom("a");
    sAddressAtom = NS_NewAtom("address");
    sBigAtom = NS_NewAtom("big");
    sBlinkAtom = NS_NewAtom("blink");
    sBAtom = NS_NewAtom("b");
    sCiteAtom = NS_NewAtom("cite");
    sCodeAtom = NS_NewAtom("code");
    sDfnAtom = NS_NewAtom("dfn");
    sEmAtom = NS_NewAtom("em");
    sFontAtom = NS_NewAtom("font");
    sIAtom = NS_NewAtom("i");
    sKbdAtom = NS_NewAtom("kbd");
    sKeygenAtom = NS_NewAtom("keygen");
    sNobrAtom = NS_NewAtom("nobr");
    sSAtom = NS_NewAtom("s");
    sSampAtom = NS_NewAtom("samp");
    sSmallAtom = NS_NewAtom("small");
    sSpacerAtom = NS_NewAtom("spacer");
    sSpanAtom = NS_NewAtom("span");      
    sStrikeAtom = NS_NewAtom("strike");
    sStrongAtom = NS_NewAtom("strong");
    sSubAtom = NS_NewAtom("sub");
    sSupAtom = NS_NewAtom("sup");
    sTtAtom = NS_NewAtom("tt");
    sUAtom = NS_NewAtom("u");
    sVarAtom = NS_NewAtom("var");
    sWbrAtom = NS_NewAtom("wbr");
  }

  ++sInstanceCount;
}

nsTextServicesDocument::~nsTextServicesDocument()
{
  if (mEditor && mNotifier)
    mEditor->RemoveEditActionListener(mNotifier);

  ClearOffsetTable();

  if (sInstanceCount <= 1)
  {
    NS_IF_RELEASE(sAAtom);
    NS_IF_RELEASE(sAddressAtom);
    NS_IF_RELEASE(sBigAtom);
    NS_IF_RELEASE(sBlinkAtom);
    NS_IF_RELEASE(sBAtom);
    NS_IF_RELEASE(sCiteAtom);
    NS_IF_RELEASE(sCodeAtom);
    NS_IF_RELEASE(sDfnAtom);
    NS_IF_RELEASE(sEmAtom);
    NS_IF_RELEASE(sFontAtom);
    NS_IF_RELEASE(sIAtom);
    NS_IF_RELEASE(sKbdAtom);
    NS_IF_RELEASE(sKeygenAtom);
    NS_IF_RELEASE(sNobrAtom);
    NS_IF_RELEASE(sSAtom);
    NS_IF_RELEASE(sSampAtom);
    NS_IF_RELEASE(sSmallAtom);
    NS_IF_RELEASE(sSpacerAtom);
    NS_IF_RELEASE(sSpanAtom);      
    NS_IF_RELEASE(sStrikeAtom);
    NS_IF_RELEASE(sStrongAtom);
    NS_IF_RELEASE(sSubAtom);
    NS_IF_RELEASE(sSupAtom);
    NS_IF_RELEASE(sTtAtom);
    NS_IF_RELEASE(sUAtom);
    NS_IF_RELEASE(sVarAtom);
    NS_IF_RELEASE(sWbrAtom);
  }

  --sInstanceCount;
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

NS_IMETHODIMP
nsTextServicesDocument::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (nsnull == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void*)(nsISupports*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kITextServicesDocumentIID)) {
    *aInstancePtr = (void*)(nsITextServicesDocument*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  *aInstancePtr = 0;
  return NS_NOINTERFACE;
}

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

  aStr->SetLength(0);

  if (!mIterator)
    return NS_ERROR_FAILURE;

  LOCK_DOC(this);

  result = CreateOffsetTable(aStr);

  UNLOCK_DOC(this);

  return result;
}

NS_IMETHODIMP
nsTextServicesDocument::FirstBlock()
{
  nsresult result;

  LOCK_DOC(this);

  mIteratorStatus = nsTextServicesDocument::eIsDone;

  if (!mIterator)
  {
    UNLOCK_DOC(this);
    return NS_ERROR_FAILURE;
  }

  // Position the iterator on the first text node
  // we run across:

  result = mIterator->First();

  if (NS_FAILED(result))
  {
    UNLOCK_DOC(this);
    return result;
  }

  nsCOMPtr<nsIContent> content;

  while (NS_ENUMERATOR_FALSE == mIterator->IsDone())
  {
  	result = mIterator->CurrentNode(getter_AddRefs(content));

    if (NS_FAILED(result))
    {
      UNLOCK_DOC(this);
      return result;
    }

    if (IsTextNode(content))
    {
      mIteratorStatus = nsTextServicesDocument::eValid;
      break;
    }

    result = mIterator->Next();

    if (NS_FAILED(result))
      return result;
  }

  // Keep track of prev and next blocks, just in case
  // the text service blows away the current block.

  if (mIteratorStatus == nsTextServicesDocument::eValid)
  {
    mPrevTextBlock  = nsCOMPtr<nsIContent>();
    result = GetFirstTextNodeInNextBlock(getter_AddRefs(mNextTextBlock));
  }
  else
  {
    // There's no text block in the document!

    mPrevTextBlock  = nsCOMPtr<nsIContent>();
    mNextTextBlock  = nsCOMPtr<nsIContent>();
  }

  UNLOCK_DOC(this);

  return result;
}

NS_IMETHODIMP
nsTextServicesDocument::LastBlock()
{
  nsresult result;

  LOCK_DOC(this);

  mIteratorStatus = nsTextServicesDocument::eIsDone;

  if (!mIterator)
  {
    UNLOCK_DOC(this);
    return NS_ERROR_FAILURE;
  }

  // Position the iterator on the last text node
  // in the tree, then walk backwards over adjacent
  // text nodes until we hit a block boundary:

  result = mIterator->Last();

  if (NS_FAILED(result))
  {
    UNLOCK_DOC(this);
    return result;
  }

  nsCOMPtr<nsIContent> content;
  nsCOMPtr<nsIContent> last;

  while (NS_ENUMERATOR_FALSE == mIterator->IsDone())
  {
  	result = mIterator->CurrentNode(getter_AddRefs(content));

    if (NS_FAILED(result))
    {
      UNLOCK_DOC(this);
      return result;
    }

    if (IsTextNode(content))
    {
      result = FirstTextNodeInCurrentBlock(mIterator);

      if (NS_FAILED(result))
      {
        UNLOCK_DOC(this);
        return result;
      }

      mIteratorStatus = nsTextServicesDocument::eValid;
      break;
    }

    result = mIterator->Prev();

    if (NS_FAILED(result))
      return result;
  }

  // Keep track of prev and next blocks, just in case
  // the text service blows away the current block.

  if (mIteratorStatus == nsTextServicesDocument::eValid)
  {
    result = GetFirstTextNodeInPrevBlock(getter_AddRefs(mPrevTextBlock));
    mNextTextBlock = nsCOMPtr<nsIContent>();
  }
  else
  {
    // There's no text block in the document!

    mPrevTextBlock = nsCOMPtr<nsIContent>();
    mNextTextBlock = nsCOMPtr<nsIContent>();
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
  nsCOMPtr<nsIContent>         content;
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

      content = do_QueryInterface(parent);

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

      result = CreateOffsetTable();

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

      result = iter->Last();

      if (NS_FAILED(result))
      {
        UNLOCK_DOC(this);
        return result;
      }

      while (iter->IsDone() == NS_ENUMERATOR_FALSE)
      {
        result = iter->CurrentNode(getter_AddRefs(content));

        if (NS_FAILED(result))
        {
          UNLOCK_DOC(this);
          return result;
        }
        
        if (!content)
        {
          UNLOCK_DOC(this);
          return NS_ERROR_FAILURE;
        }

        if (IsTextNode(content))
          break;

        content = nsCOMPtr<nsIContent>();

        result = iter->Prev();

        if (NS_FAILED(result))
          return result;
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

      result = CreateOffsetTable();

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

    result = iter->First();

    if (NS_FAILED(result))
    {
      UNLOCK_DOC(this);
      return result;
    }

    // Now walk through the range till we find a text node.

    while (iter->IsDone() == NS_ENUMERATOR_FALSE)
    {
      result = iter->CurrentNode(getter_AddRefs(content));

      if (NS_FAILED(result))
      {
        UNLOCK_DOC(this);
        return result;
      }

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

        result = CreateOffsetTable();

        if (NS_FAILED(result))
        {
          UNLOCK_DOC(this);
          return result;
        }

        result = GetSelection(aSelStatus, aSelOffset, aSelLength);

        UNLOCK_DOC(this);

        return result;

      }

      result = iter->Next();

      if (NS_FAILED(result))
        return result;
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

  result = iter->Last();

  if (NS_FAILED(result))
  {
    UNLOCK_DOC(this);
    return result;
  }

  while (iter->IsDone() == NS_ENUMERATOR_FALSE)
  {
    result = iter->CurrentNode(getter_AddRefs(content));

    if (NS_FAILED(result))
    {
      UNLOCK_DOC(this);
      return result;
    }

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

      result = CreateOffsetTable();

      if (NS_FAILED(result))
      {
        UNLOCK_DOC(this);
        return result;
      }

      result = GetSelection(aSelStatus, aSelOffset, aSelLength);

      UNLOCK_DOC(this);

      return result;
    }

    result = iter->Prev();

    if (NS_FAILED(result))
      return result;
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
nsTextServicesDocument::LastSelectedBlock(TSDBlockSelectionStatus *aSelStatus, PRInt32 *aSelOffset, PRInt32 *aSelLength)
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
  nsCOMPtr<nsIContent>         content;
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

      content = do_QueryInterface(parent);

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

      result = CreateOffsetTable();

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

      result = iter->First();

      if (NS_FAILED(result))
      {
        UNLOCK_DOC(this);
        return result;
      }

      while (iter->IsDone() == NS_ENUMERATOR_FALSE)
      {
        result = iter->CurrentNode(getter_AddRefs(content));

        if (NS_FAILED(result))
        {
          UNLOCK_DOC(this);
          return result;
        }
        
        if (!content)
        {
          UNLOCK_DOC(this);
          return NS_ERROR_FAILURE;
        }

        if (IsTextNode(content))
          break;

        content = nsCOMPtr<nsIContent>();

        result = iter->Next();

        if (NS_FAILED(result))
          return result;
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

      result = CreateOffsetTable();

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

    result = iter->Last();

    if (NS_FAILED(result))
    {
      UNLOCK_DOC(this);
      return result;
    }

    // Now walk through the range till we find a text node.

    while (iter->IsDone() == NS_ENUMERATOR_FALSE)
    {
      result = iter->CurrentNode(getter_AddRefs(content));

      if (NS_FAILED(result))
      {
        UNLOCK_DOC(this);
        return result;
      }

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

        result = CreateOffsetTable();

        if (NS_FAILED(result))
        {
          UNLOCK_DOC(this);
          return result;
        }

        result = GetSelection(aSelStatus, aSelOffset, aSelLength);

        UNLOCK_DOC(this);

        return result;

      }

      result = iter->Prev();

      if (NS_FAILED(result))
        return result;
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

  result = iter->First();

  if (NS_FAILED(result))
  {
    UNLOCK_DOC(this);
    return result;
  }

  while (iter->IsDone() == NS_ENUMERATOR_FALSE)
  {
    result = iter->CurrentNode(getter_AddRefs(content));

    if (NS_FAILED(result))
    {
      UNLOCK_DOC(this);
      return result;
    }

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

      result = CreateOffsetTable();

      if (NS_FAILED(result))
      {
        UNLOCK_DOC(this);
        return result;
      }

      result = GetSelection(aSelStatus, aSelOffset, aSelLength);

      UNLOCK_DOC(this);

      return result;
    }

    result = iter->Next();

    if (NS_FAILED(result))
      return result;
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

      if (mIterator->IsDone() != NS_ENUMERATOR_FALSE)
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

    mPrevTextBlock = nsCOMPtr<nsIContent>();
    mNextTextBlock = nsCOMPtr<nsIContent>();
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

      if (mIterator->IsDone() != NS_ENUMERATOR_FALSE)
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

    mPrevTextBlock = nsCOMPtr<nsIContent>();
    mNextTextBlock = nsCOMPtr<nsIContent>();
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

  result = mSelCon->ScrollSelectionIntoView(nsISelectionController::SELECTION_NORMAL, nsISelectionController::SELECTION_FOCUS_REGION);

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

  if (!mEditor)
    return NS_ERROR_FAILURE;

  if (!aText)
    return NS_ERROR_NULL_POINTER;


  LOCK_DOC(this);

  result = mEditor->BeginTransaction();

  if (NS_FAILED(result))
  {
    UNLOCK_DOC(this);
    return result;
  }

  // We have to call DeleteSelection(), just in case there is
  // a selection, to make sure that our offset table is kept in sync!

  result = DeleteSelection();
  
  if (NS_FAILED(result))
  {
    mEditor->EndTransaction();
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

  if (SelectionIsValid())
  {
    //**** KDEBUG ****
    // printf("\n---- Before Insert\n");
    // printf("Sel: (%2d, %4d) (%2d, %4d)\n", mSelStartIndex, mSelStartOffset, mSelEndIndex, mSelEndOffset);
    // PrintOffsetTable();
    //**** KDEBUG ****

    PRInt32 i, strLength  = aText->Length();

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
  }

  result = mEditor->EndTransaction();

  if (NS_FAILED(result))
  {
    UNLOCK_DOC(this);
    return result;
  }

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

  nsresult result = NodeHasOffsetEntry(aChild, &hasEntry, &nodeIndex);

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

  nsCOMPtr<nsIContent> content;
  nsCOMPtr<nsIDOMNode> node;
  
  result = mIterator->CurrentNode(getter_AddRefs(content));

  if (content)
  {
    node = do_QueryInterface(content);

    if (node && node.get() == aChild)
    {
      // The iterator points to the node that is about
      // to be deleted. Move it to the next valid text node
      // listed in the offset table!

      // XXX
    }
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
      entry->mIsValid = PR_FALSE;
    }

    nodeIndex++;
  }

  UNLOCK_DOC(this);

  NS_ASSERTION(0, "DeleteNode called, offset tables might be out of sync."); 

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

  result = NodeHasOffsetEntry(aLeftNode, &leftHasEntry, &leftIndex);

  if (NS_FAILED(result))
    return result;

  if (!leftHasEntry)
  {
    // XXX: Not sure if we should be throwing an error here!
    NS_ASSERTION(0, "JoinNode called with node not listed in offset table.");
    return NS_ERROR_FAILURE;
  }

  result = NodeHasOffsetEntry(aRightNode, &rightHasEntry, &rightIndex);

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

  OffsetEntry *entry = (OffsetEntry *)mOffsetTable[leftIndex];
  NS_ASSERTION(entry->mNodeOffset == 0, "Unexpected offset value for leftIndex.");

  entry = (OffsetEntry *)mOffsetTable[rightIndex];
  NS_ASSERTION(entry->mNodeOffset == 0, "Unexpected offset value for rightIndex.");

  // Run through the table and change all entries referring to
  // the left node so that they now refer to the right node:

  PRInt32 nodeLength = 0;

  for (i = leftIndex; i < rightIndex; i++)
  {
    entry = (OffsetEntry *)mOffsetTable[i];

    if (entry->mNode == aLeftNode)
    {
      if (entry->mIsValid)
      {
        entry->mNode = aRightNode;
        nodeLength += entry->mLength;
      }
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

  nsCOMPtr<nsIContent> currentContent;
  nsCOMPtr<nsIContent> leftContent = do_QueryInterface(aLeftNode);
  nsCOMPtr<nsIContent> rightContent = do_QueryInterface(aRightNode);

  if (!leftContent || !rightContent)
  {
    UNLOCK_DOC(this);
    return NS_ERROR_FAILURE;
  }

  result = mIterator->CurrentNode(getter_AddRefs(currentContent));

  if (NS_FAILED(result))
  {
    UNLOCK_DOC(this);
    return result;
  }

  if (currentContent == leftContent)
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

  result = nsComponentManager::CreateInstance(kCContentIteratorCID, nsnull,
                                        NS_GET_IID(nsIContentIterator), 
                                        (void **)aIterator);

  if (NS_FAILED(result))
    return result;

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

  result = nsComponentManager::CreateInstance(kCDOMRangeCID, nsnull,
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

  result = nsComponentManager::CreateInstance(kCDOMRangeCID, nsnull,
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
  nsCOMPtr<nsIContent> content;
  nsCOMPtr<nsIDOMNode> node;
  nsresult result;
  PRInt32 i;

  if (!mIterator)
    return NS_ERROR_FAILURE;

  result = mIterator->CurrentNode(getter_AddRefs(content));

  if (NS_FAILED(result))
    return result;

  if (!content)
    return NS_ERROR_FAILURE;

  node = do_QueryInterface(content);

  if (!node)
    return NS_ERROR_FAILURE;

  nsIDOMNode *nodePtr = node.get();
  PRInt32 tcount      = mOffsetTable.Count();

  nsIDOMNode *prevValidNode = 0;
  nsIDOMNode *nextValidNode = 0;
  PRBool foundEntry = PR_FALSE;
  OffsetEntry *entry;

  for (i = 0; i < tcount && !nextValidNode; i++)
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

  content = nsCOMPtr<nsIContent>();

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
nsTextServicesDocument::IsBlockNode(nsIContent *aContent)
{
  nsCOMPtr<nsIAtom> atom;

  aContent->GetTag(*getter_AddRefs(atom));

  if (!atom)
    return PR_TRUE;

  nsIAtom *atomPtr = atom.get();

  return (sAAtom       != atomPtr &&
          sAddressAtom != atomPtr &&
          sBigAtom     != atomPtr &&
          sBlinkAtom   != atomPtr &&
          sBAtom       != atomPtr &&
          sCiteAtom    != atomPtr &&
          sCodeAtom    != atomPtr &&
          sDfnAtom     != atomPtr &&
          sEmAtom      != atomPtr &&
          sFontAtom    != atomPtr &&
          sIAtom       != atomPtr &&
          sKbdAtom     != atomPtr &&
          sKeygenAtom  != atomPtr &&
          sNobrAtom    != atomPtr &&
          sSAtom       != atomPtr &&
          sSampAtom    != atomPtr &&
          sSmallAtom   != atomPtr &&
          sSpacerAtom  != atomPtr &&
          sSpanAtom    != atomPtr &&
          sStrikeAtom  != atomPtr &&
          sStrongAtom  != atomPtr &&
          sSubAtom     != atomPtr &&
          sSupAtom     != atomPtr &&
          sTtAtom      != atomPtr &&
          sUAtom       != atomPtr &&
          sVarAtom     != atomPtr &&
          sWbrAtom     != atomPtr);
}

PRBool
nsTextServicesDocument::HasSameBlockNodeParent(nsIContent *aContent1, nsIContent *aContent2)
{
  nsCOMPtr<nsIContent> p1;
  nsCOMPtr<nsIContent> p2;
  nsresult result;

  result = aContent1->GetParent(*getter_AddRefs(p1));

  if (NS_FAILED(result))
    return PR_FALSE;

  result = aContent2->GetParent(*getter_AddRefs(p2));

  if (NS_FAILED(result))
    return PR_FALSE;

  // Quick test:

  if (p1 == p2)
    return PR_TRUE;

  // Walk up the parent hierarchy looking for closest block boundary node:

  nsCOMPtr<nsIContent> tmp;

  while (p1 && !IsBlockNode(p1))
  {
    result = p1->GetParent(*getter_AddRefs(tmp));

    if (NS_FAILED(result))
      return PR_FALSE;

    p1 = tmp;
  }

  while (p2 && !IsBlockNode(p2))
  {
    result = p2->GetParent(*getter_AddRefs(tmp));

    if (NS_FAILED(result))
      return PR_FALSE;

    p2 = tmp;
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
      else if (aOffset >= entry->mStrOffset && aOffset <= entry->mStrOffset + entry->mLength)
      {
        sNode   = entry->mNode;
        sOffset = entry->mNodeOffset + aOffset - entry->mStrOffset;
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
  nsCOMPtr<nsIContent> content;
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

    content = do_QueryInterface(saveNode);

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

    content = do_QueryInterface(parent);

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

  while (iter->IsDone() == NS_ENUMERATOR_FALSE)
  {
    result = iter->CurrentNode(getter_AddRefs(content));

    if (NS_FAILED(result))
      return result;

    if (IsTextNode(content))
    {
      node = do_QueryInterface(content);

      if (!node)
        return NS_ERROR_FAILURE;

      break;
    }

    node = nsCOMPtr<nsIDOMNode>();

    result = iter->Prev();

    if (NS_FAILED(result))
      return result;
  }

  if (node)
  {
    // We found a node, now set the offset to the end
    // of the text node.

    nsString str;

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

    content = do_QueryInterface(saveNode);

    result = iter->PositionAt(content);

    if (NS_FAILED(result))
      return result;

    while (iter->IsDone() == NS_ENUMERATOR_FALSE)
    {
      result = iter->CurrentNode(getter_AddRefs(content));

      if (NS_FAILED(result))
        return result;

      if (IsTextNode(content))
      {
        node = do_QueryInterface(content);

        if (!node)
          return NS_ERROR_FAILURE;

        break;
      }

      node = nsCOMPtr<nsIDOMNode>();

      result = iter->Next();

      if (NS_FAILED(result))
        return result;
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
  nsCOMPtr<nsIContent> content;

  result = iter->First();
 
  if (NS_FAILED(result))
    return result;

  if (! IsTextNode(p1))
  {
    found = PR_FALSE;

    while (iter->IsDone() == NS_ENUMERATOR_FALSE)
    {
      result = iter->CurrentNode(getter_AddRefs(content));

      if (NS_FAILED(result))
        return result;

      if (!content)
        return NS_ERROR_FAILURE;

      if (IsTextNode(content))
      {
        p1 = do_QueryInterface(content);

        if (!p1)
          return NS_ERROR_FAILURE;

        o1 = 0;
        found = PR_TRUE;

        break;
      }

      result = iter->Next();

      if (NS_FAILED(result))
        return result;
    }

    if (!found)
      return NS_ERROR_FAILURE;
  }

  // Find the last text node in the range.

  result = iter->Last();
 
  if (NS_FAILED(result))
    return result;

  if (! IsTextNode(p2))
  {
    found = PR_FALSE;

    while (iter->IsDone() == NS_ENUMERATOR_FALSE)
    {
      result = iter->CurrentNode(getter_AddRefs(content));

      if (NS_FAILED(result))
        return result;

      if (!content)
        return NS_ERROR_FAILURE;

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

      result = iter->Prev();

      if (NS_FAILED(result))
        return result;
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

  // Compare two node/offset pairs.
  //
  // Return -1 if node1 <  node2
  // Return  0 if node1 == node2
  // Return  1 if node1 >  node2 

  *aResult = 0;

  if (aParent1 == aParent2 && aOffset1 == aOffset2)
    return NS_OK;

  nsCOMPtr<nsIDOMRange> range;

  result = nsComponentManager::CreateInstance(kCDOMRangeCID, nsnull,
                                              NS_GET_IID(nsIDOMRange), 
                                              getter_AddRefs(range));

  if (NS_FAILED(result))
    return result;

  if (!range)
    return NS_ERROR_FAILURE;

  result = range->SetStart(aParent1, aOffset1);

  if (NS_FAILED(result))
    return result;

  result = range->SetEnd(aParent2, aOffset2);

  if (NS_SUCCEEDED(result))
    *aResult = -1;
  else
    *aResult = 1;

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

  result = nsComponentManager::CreateInstance(kCDOMRangeCID, nsnull,
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
nsTextServicesDocument::FirstTextNodeInCurrentBlock(nsIContentIterator *iter)
{
  nsresult result;

  if (!iter)
    return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIContent> content;
  nsCOMPtr<nsIContent> last;

  // Walk backwards over adjacent text nodes until
  // we hit a block boundary:

  while (NS_ENUMERATOR_FALSE == iter->IsDone())
  {
  	result = iter->CurrentNode(getter_AddRefs(content));

    if (NS_FAILED(result))
      return result;

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

    result = iter->Prev();

    if (NS_FAILED(result))
      return result;
  }
  
  if (last)
    result = iter->PositionAt(last);

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

  result = aIterator->Prev();

  if (NS_FAILED(result))
    return result;

  // Now find the first text node of the next block:

  return FirstTextNodeInCurrentBlock(aIterator);
}

nsresult
nsTextServicesDocument::FirstTextNodeInNextBlock(nsIContentIterator *aIterator)
{
  nsCOMPtr<nsIContent> content;
  nsCOMPtr<nsIContent> prev;
  PRBool crossedBlockBoundary = PR_FALSE;
  nsresult result;

  if (!aIterator)
    return NS_ERROR_NULL_POINTER;

  while (NS_ENUMERATOR_FALSE == aIterator->IsDone())
  {
  	result = aIterator->CurrentNode(getter_AddRefs(content));
    if (NS_FAILED(result))
      return result;

    if (!content)
      return NS_ERROR_FAILURE;

    if (IsTextNode(content))
    {
      if (!crossedBlockBoundary && (!prev || HasSameBlockNodeParent(prev, content)))
        prev = content;
      else
        break;

    }
    else if (IsBlockNode(content))
      crossedBlockBoundary = PR_TRUE;

    result = aIterator->Next();

    if (NS_FAILED(result))
      return result;
  }

  return NS_OK;
}

nsresult
nsTextServicesDocument::GetFirstTextNodeInPrevBlock(nsIContent **aContent)
{
  nsCOMPtr<nsIContent> content;
  nsresult result;

  if (!aContent)
    return NS_ERROR_NULL_POINTER;

  *aContent = 0;

  // Save the iterator's current content node so we can restore
  // it when we are done:

  mIterator->CurrentNode(getter_AddRefs(content));

  result = FirstTextNodeInPrevBlock(mIterator);

  if (NS_FAILED(result))
  {
    // Try to restore the iterator before returning.
    mIterator->PositionAt(content);
    return result;
  }

  if (mIterator->IsDone() == NS_ENUMERATOR_FALSE)
  {
    result = mIterator->CurrentNode(aContent);

    if (NS_FAILED(result))
    {
      // Try to restore the iterator before returning.
      mIterator->PositionAt(content);
      return result;
    }
  }

  // Restore the iterator:

  result = mIterator->PositionAt(content);

  return result;
}

nsresult
nsTextServicesDocument::GetFirstTextNodeInNextBlock(nsIContent **aContent)
{
  nsCOMPtr<nsIContent> content;
  nsresult result;

  if (!aContent)
    return NS_ERROR_NULL_POINTER;

  *aContent = 0;

  // Save the iterator's current content node so we can restore
  // it when we are done:

  mIterator->CurrentNode(getter_AddRefs(content));

  result = FirstTextNodeInNextBlock(mIterator);

  if (NS_FAILED(result))
  {
    // Try to restore the iterator before returning.
    mIterator->PositionAt(content);
    return result;
  }

  if (mIterator->IsDone() == NS_ENUMERATOR_FALSE)
  {
    result = mIterator->CurrentNode(aContent);

    if (NS_FAILED(result))
    {
      // Try to restore the iterator before returning.
      mIterator->PositionAt(content);
      return result;
    }
  }

  // Restore the iterator:

  result = mIterator->PositionAt(content);

  return result;
}

nsresult
nsTextServicesDocument::CreateOffsetTable(nsString *aStr)
{
  nsresult result = NS_OK;

  nsCOMPtr<nsIContent> content;
  nsCOMPtr<nsIContent> first;
  nsCOMPtr<nsIContent> prev;

  if (!mIterator)
    return NS_ERROR_NULL_POINTER;

  ClearOffsetTable();

  if (aStr)
    aStr->SetLength(0);

  if (mIteratorStatus == nsTextServicesDocument::eIsDone)
    return NS_OK;

  // The text service could have added text nodes to the beginning
  // of the current block and called this method again. Make sure
  // we really are at the beginning of the current block:

  result = FirstTextNodeInCurrentBlock(mIterator);

  if (NS_FAILED(result))
    return result;

  PRInt32 offset = 0;

  while (NS_ENUMERATOR_FALSE == mIterator->IsDone())
  {
  	result = mIterator->CurrentNode(getter_AddRefs(content));

    if (NS_FAILED(result))
      return result;

    if (!content)
      return NS_ERROR_FAILURE;

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

          mOffsetTable.AppendElement((void *)entry);

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

    result = mIterator->Next();

    if (NS_FAILED(result))
      return result;
  }

  if (first)
  {
    // Always leave the iterator pointing at the first
    // text node of the current block!

    mIterator->PositionAt(first);
  }
  else
  {
    // If we never ran across a text node, the iterator
    // might have been pointing to something invalid to
    // begin with.

    mIteratorStatus = nsTextServicesDocument::eIsDone;
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
nsTextServicesDocument::ClearOffsetTable()
{
  PRInt32 i;

  for (i = 0; i < mOffsetTable.Count(); i++)
  {
    OffsetEntry *entry = (OffsetEntry *)mOffsetTable[i];
    if (entry)
      delete entry;
  }

  mOffsetTable.Clear();

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
nsTextServicesDocument::NodeHasOffsetEntry(nsIDOMNode *aNode, PRBool *aHasEntry, PRInt32 *aEntryIndex)
{
  OffsetEntry *entry;
  PRInt32 i;

  if (!aNode || !aHasEntry || !aEntryIndex)
    return NS_ERROR_NULL_POINTER;

  for (i = 0; i < mOffsetTable.Count(); i++)
  {
    entry = (OffsetEntry *)mOffsetTable[i];

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

void
nsTextServicesDocument::PrintOffsetTable()
{
  OffsetEntry *entry;
  PRInt32 i;

  for (i = 0; i < mOffsetTable.Count(); i++)
  {
    entry = (OffsetEntry *)mOffsetTable[i];
    printf("ENTRY %4d: 0x%.8p  %c  %c  %4d  %4d  %4d\n",
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
  char tmpBuf[256];
  nsIAtom *atom = 0;
  nsresult result;

  aContent->GetTag(atom);
  atom->ToString(tmpStr);
  tmpStr.ToCString(tmpBuf, 256);

  printf("%s", tmpBuf);

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

      str.ToCString(tmpBuf, 256);
      printf(":  \"%s\"", tmpBuf);
    }
  }

  printf("\n");
  fflush(stdout);
}
