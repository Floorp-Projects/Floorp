/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsLayoutCID.h"
#include "nsIAtom.h"
#include "nsIContent.h"
#include "nsIContentIterator.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMRange.h"
#include "nsIDOMSelection.h"
#include "nsTextServicesDocument.h"

// #define HAVE_EDIT_ACTION_LISTENERS 1

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

  mIteratorStatus = eUninitialized;

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
#ifdef HAVE_EDIT_ACTION_LISTENERS
  if (mEditor && mNotifier)
    mEditor->RemoveEditActionListener(mNotifier);
#endif // HAVE_EDIT_ACTION_LISTENERS

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
nsTextServicesDocument::Init(nsIDOMDocument *aDOMDocument, nsIPresShell *aPresShell)
{
  nsresult result = NS_OK;

  if (!aDOMDocument || !aPresShell)
    return NS_ERROR_NULL_POINTER;

  NS_ASSERTION(!mPresShell, "mPresShell already initialized!");

  if (mPresShell)
    return NS_ERROR_FAILURE;

  NS_ASSERTION(!mDOMDocument, "mDOMDocument already initialized!");

  if (mDOMDocument)
    return NS_ERROR_FAILURE;

  LOCK_DOC(this);

  mPresShell   = do_QueryInterface(aPresShell);
  mDOMDocument = do_QueryInterface(aDOMDocument);

  InitContentIterator();

  UNLOCK_DOC(this);

  return result;
}

NS_IMETHODIMP
nsTextServicesDocument::SetEditor(nsIEditor *aEditor)
{
  nsresult result = NS_OK;
  nsCOMPtr<nsIPresShell> presShell;
  nsCOMPtr<nsIDOMDocument> doc;

  if (!aEditor)
    return NS_ERROR_NULL_POINTER;

  LOCK_DOC(this);

  // Check to see if we already have an mPresShell. If we do, it
  // better be the same one the editor uses!

  result = aEditor->GetPresShell(getter_AddRefs(presShell));

  if (NS_FAILED(result))
  {
    UNLOCK_DOC(this);
    return result;
  }

  if (!presShell || (mPresShell && presShell != mPresShell))
  {
    UNLOCK_DOC(this);
    return NS_ERROR_FAILURE;
  }

  if (!mPresShell)
    mPresShell = presShell;

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

  if (!mDOMDocument) {
    mDOMDocument = doc;
    InitContentIterator();
  }

  mEditor = do_QueryInterface(aEditor);

  nsTSDNotifier *notifier = new nsTSDNotifier(this);

  if (!notifier)
  {
    UNLOCK_DOC(this);
    return NS_ERROR_OUT_OF_MEMORY;
  }

  mNotifier = do_QueryInterface(notifier);

#ifdef HAVE_EDIT_ACTION_LISTENERS
  result = mEditor->AddEditActionListener(mNotifier);
#endif // HAVE_EDIT_ACTION_LISTENERS

  UNLOCK_DOC(this);

  return result;
}

NS_IMETHODIMP
nsTextServicesDocument::GetCurrentTextBlock(nsString *aStr)
{
  nsresult result = NS_OK;

  if (!aStr || !mIterator)
    return NS_ERROR_NULL_POINTER;

  LOCK_DOC(this);

  nsCOMPtr<nsIContent> content;
  nsCOMPtr<nsIContent> first;
  nsCOMPtr<nsIContent> prev;

  ClearOffsetTable();

  *aStr = "";

  // The text service could have added text nodes to the beginning
  // of the current block and called this method again. Make sure
  // we really are at the beginning of the current block:

  result = FirstTextNodeInCurrentBlock();

  if (NS_FAILED(result))
    return result;

  while (NS_COMFALSE == mIterator->IsDone())
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

          OffsetEntry *entry = new OffsetEntry(node, aStr->Length(), str.Length());

          if (!entry)
            return NS_ERROR_OUT_OF_MEMORY;

          mOffsetTable.AppendElement((void *)entry);

          // Append the text node's string to the output string:

          if (!first)
            *aStr = str;
          else
            *aStr += str;
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

    mIterator->Next();
  }

  // Always leave the iterator pointing at the first
  // text node of the current block!

  if (first)
    mIterator->PositionAt(first);

  UNLOCK_DOC(this);

  return result;
}

NS_IMETHODIMP
nsTextServicesDocument::PrevBlock()
{
  nsresult result;

  if (!mIterator)
    return NS_ERROR_FAILURE;

  LOCK_DOC(this);

  if (mIteratorStatus == nsTextServicesDocument::eValid || mIteratorStatus == nsTextServicesDocument::eNext)
  {
    result = FirstTextNodeInPrevBlock();

    if (NS_FAILED(result))
    {
      UNLOCK_DOC(this);
      return result;
    }

    // XXX: Check to make sure the iterator is pointing
    //      to a valid text block, and set mIteratorStatus
    //      accordingly!
  }
  else if (mIteratorStatus == nsTextServicesDocument::ePrev)
  {
    // The iterator already points to the previous
    // block, so don't do anything.

    mIteratorStatus = nsTextServicesDocument::eValid;
  }

  // Keep track of prev and next blocks, just in case
  // the text service blows away the current block.

  if (eValid)
  {
    result = FindFirstTextNodeInPrevBlock(getter_AddRefs(mPrevTextBlock));
    result = FindFirstTextNodeInNextBlock(getter_AddRefs(mNextTextBlock));
  }

  UNLOCK_DOC(this);

  return result;
}

NS_IMETHODIMP
nsTextServicesDocument::NextBlock()
{
  nsresult result;

  if (!mIterator)
    return NS_ERROR_FAILURE;

  LOCK_DOC(this);

  if (mIteratorStatus == nsTextServicesDocument::eValid)
  {
    result = FirstTextNodeInNextBlock();

    if (NS_FAILED(result))
    {
      UNLOCK_DOC(this);
      return result;
    }

    // XXX: check if the iterator is pointing to a text node,
    //      then set mIteratorStatus.
  }
  else if (mIteratorStatus == nsTextServicesDocument::eNext)
  {
    // The iterator already points to the next block,
    // so don't do anything to it!

    mIteratorStatus = nsTextServicesDocument::eValid;
  }
  else if (mIteratorStatus == nsTextServicesDocument::ePrev)
  {
    // If the iterator is pointing to the previous block,
    // we know that there is no next text block!

    mIteratorStatus = nsTextServicesDocument::eInvalid;
  }

  // Keep track of prev and next blocks, just in case
  // the text service blows away the current block.

  if (mIteratorStatus == nsTextServicesDocument::eValid)
  {
    result = FindFirstTextNodeInPrevBlock(getter_AddRefs(mPrevTextBlock));
    result = FindFirstTextNodeInNextBlock(getter_AddRefs(mNextTextBlock));
  }


  UNLOCK_DOC(this);

  return result;
}

NS_IMETHODIMP
nsTextServicesDocument::IsDone()
{
  if (!mIterator)
    return NS_ERROR_FAILURE;

  LOCK_DOC(this);

  nsresult result = mIterator->IsDone();

  UNLOCK_DOC(this);

  return result;
}

NS_IMETHODIMP
nsTextServicesDocument::SetSelection(PRInt32 aOffset, PRInt32 aLength)
{
  nsresult result       = NS_OK;

  if ((!mPresShell && !mEditor) || aOffset < 0 || aLength < 0)
    return NS_ERROR_FAILURE;

  LOCK_DOC(this);

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
        // Cursor can only be placed at the end of an
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
  {
    UNLOCK_DOC(this);
    return NS_ERROR_FAILURE;
  }

  // XXX: If we ever get a SetSelection() method in nsIEditor, we should
  //      use it.

  nsCOMPtr<nsIDOMSelection> selection;

  result = mPresShell->GetSelection(getter_AddRefs(selection));

  if (NS_FAILED(result))
  {
    UNLOCK_DOC(this);
    return result;
  }

  selection->Collapse(sNode, sOffset);

  if (aLength <= 0)
  {
    // We have a collapsed selection. (Cursor/Caret)

    mSelEndIndex  = mSelStartIndex;
    mSelEndOffset = mSelStartOffset;
    UNLOCK_DOC(this);

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

  if (eNode)
    selection->Extend(eNode, eOffset);

  UNLOCK_DOC(this);

  //**** KDEBUG ****
  // printf("\n * Sel: (%2d, %4d) (%2d, %4d)\n", mSelStartIndex, mSelStartOffset, mSelEndIndex, mSelEndOffset);
  //**** KDEBUG ****

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
  printf("\n---- Before Delete\n");
  printf("Sel: (%2d, %4d) (%2d, %4d)\n", mSelStartIndex, mSelStartOffset, mSelEndIndex, mSelEndOffset);
  PrintOffsetTable();
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
        // the cursor is always at the end of the entry!

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

  result = mEditor->DeleteSelection(nsIEditor::eRTL);

  if (NS_FAILED(result))
  {
    UNLOCK_DOC(this);
    return result;
  }

  entry = 0;

  // Move the cursor to the end of the first valid entry.
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

  // If we still don't have a valid entry, move the cursor
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
    // cursor ... just mark the selection invalid.

    mSelStartIndex  = mSelEndIndex  = -1;
    mSelStartOffset = mSelEndOffset = -1;
  }

  // Now remove any invalid entries from the offset table.

  result = RemoveInvalidOffsetEntries();

  //**** KDEBUG ****
  printf("\n---- After Delete\n");
  printf("Sel: (%2d, %4d) (%2d, %4d)\n", mSelStartIndex, mSelStartOffset, mSelEndIndex, mSelEndOffset);
  PrintOffsetTable();
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

  result = mEditor->InsertText(*aText);

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

    nsCOMPtr<nsIDOMSelection> selection;
    OffsetEntry *itEntry;
    OffsetEntry *entry = (OffsetEntry *)mOffsetTable[mSelStartIndex];
    void *node         = entry->mNode;

    NS_ASSERTION((entry->mIsValid), "Invalid insertion point!");

    if (entry->mStrOffset == mSelStartOffset)
    {
      if (entry->mIsInsertedText)
      {
        // If the cursor is in an inserted text offset entry,
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
      // cursor is properly placed!

      itEntry->mLength += strLength;

      mSelStartIndex = mSelEndIndex = i;
          
      result = mPresShell->GetSelection(getter_AddRefs(selection));

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
nsTextServicesDocument::InitContentIterator()
{
  nsCOMPtr<nsIDOMRange> range;

  nsresult result;

  if (!mDOMDocument)
    return NS_ERROR_NULL_POINTER;

  LOCK_DOC(this);

  result = nsComponentManager::CreateInstance(kCContentIteratorCID, nsnull,
                                        nsIContentIterator::GetIID(), 
                                        getter_AddRefs(mIterator));

  if (NS_FAILED(result))
  {
    UNLOCK_DOC(this);
    return result;
  }

  if (!mIterator)
  {
    UNLOCK_DOC(this);
    return NS_ERROR_NULL_POINTER;
  }

  //
  // Create a range containing the document body's contents:
  //

  nsCOMPtr<nsIDOMNodeList>nodeList; 
  nsString bodyTag = "body"; 

  result = mDOMDocument->GetElementsByTagName(bodyTag, getter_AddRefs(nodeList));

  if (NS_FAILED(result))
  {
    UNLOCK_DOC(this);
    return result;
  }
  
  if (!nodeList)
  {
    UNLOCK_DOC(this);
    return NS_ERROR_NULL_POINTER;
  }

  PRUint32 count; 
  nodeList->GetLength(&count);

  NS_ASSERTION(1==count, "there is not exactly 1 body in the document!"); 

  if (count < 1)
  {
    UNLOCK_DOC(this);
    return NS_ERROR_FAILURE;
  }

  // Use the first body node in the list:

  nsCOMPtr<nsIDOMNode>node; 
  result = nodeList->Item(0, getter_AddRefs(node)); 

  if (NS_FAILED(result))
  {
    UNLOCK_DOC(this);
    return result;
  }

  if (!node)
  {
    UNLOCK_DOC(this);
    return NS_ERROR_NULL_POINTER;
  }

  result = nsComponentManager::CreateInstance(kCDOMRangeCID, nsnull,
                                        nsIDOMRange::GetIID(), 
                                        getter_AddRefs(range));

  if (NS_FAILED(result))
  {
    UNLOCK_DOC(this);
    return result;
  }

  if (!range)
  {
    UNLOCK_DOC(this);
    return NS_ERROR_NULL_POINTER;
  }

  result = range->SelectNodeContents(node);

  if (NS_FAILED(result))
  {
    UNLOCK_DOC(this);
    return result;
  }

  result = mIterator->Init(range);

  mIteratorStatus = eInvalid;

  UNLOCK_DOC(this);

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
      mIteratorStatus = eInvalid;
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
      mIteratorStatus = eInvalid;
      return result;
    }

    mIteratorStatus = eNext;
  }
  else if (mPrevTextBlock)
  {
    result = mIterator->PositionAt(mPrevTextBlock);

    if (NS_FAILED(result))
    {
      mIteratorStatus = eInvalid;
      return result;
    }

    mIteratorStatus = ePrev;
  }
  else
    mIteratorStatus = eInvalid;

  return NS_OK;
}

PRBool
nsTextServicesDocument::IsBlockNode(nsIContent *aContent)
{
  nsCOMPtr<nsIAtom> atom;

  aContent->GetTag(*getter_AddRefs(atom));

  if (!atom)
    return PR_TRUE;

  return (sAAtom       != atom.get() &&
          sAddressAtom != atom.get() &&
          sBigAtom     != atom.get() &&
          sBlinkAtom   != atom.get() &&
          sBAtom       != atom.get() &&
          sCiteAtom    != atom.get() &&
          sCodeAtom    != atom.get() &&
          sDfnAtom     != atom.get() &&
          sEmAtom      != atom.get() &&
          sFontAtom    != atom.get() &&
          sIAtom       != atom.get() &&
          sKbdAtom     != atom.get() &&
          sKeygenAtom  != atom.get() &&
          sNobrAtom    != atom.get() &&
          sSAtom       != atom.get() &&
          sSampAtom    != atom.get() &&
          sSmallAtom   != atom.get() &&
          sSpacerAtom  != atom.get() &&
          sSpanAtom    != atom.get() &&
          sStrikeAtom  != atom.get() &&
          sStrongAtom  != atom.get() &&
          sSubAtom     != atom.get() &&
          sSupAtom     != atom.get() &&
          sTtAtom      != atom.get() &&
          sUAtom       != atom.get() &&
          sVarAtom     != atom.get() &&
          sWbrAtom     != atom.get());
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

  if (!node)
    return PR_FALSE;

  PRUint16 type;

  nsresult result = node->GetNodeType(&type);

  if (NS_FAILED(result))
    return PR_FALSE;

  return nsIDOMNode::TEXT_NODE == type;
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
nsTextServicesDocument::FirstBlock()
{
  nsresult result;

  if (!mIterator)
    return NS_ERROR_FAILURE;

  // Position the iterator on the first text node
  // we run across:

  result = mIterator->First();

  if (NS_FAILED(result))
    return result;

  nsCOMPtr<nsIContent> content;
  PRBool foundTextBlock = PR_FALSE;

  while (NS_COMFALSE == mIterator->IsDone())
  {
  	result = mIterator->CurrentNode(getter_AddRefs(content));

    if (NS_FAILED(result))
      return result;

    if (IsTextNode(content))
    {
      foundTextBlock = PR_TRUE;
      break;
    }

    mIterator->Next();
  }

  // Keep track of prev and next blocks, just in case
  // the text service blows away the current block.

  if (foundTextBlock)
  {
    mIteratorStatus = nsTextServicesDocument::eValid;
    mPrevTextBlock  = nsCOMPtr<nsIContent>();
    result = FindFirstTextNodeInNextBlock(getter_AddRefs(mNextTextBlock));
  }
  else
  {
    // There's no text block in the document!

    mIteratorStatus = nsTextServicesDocument::eInvalid;
    mPrevTextBlock  = nsCOMPtr<nsIContent>();
    mNextTextBlock  = nsCOMPtr<nsIContent>();
  }

  return result;
}

nsresult
nsTextServicesDocument::LastBlock()
{
  nsresult result;

  if (!mIterator)
    return NS_ERROR_FAILURE;

  // Position the iterator on the last text node
  // in the tree, then walk backwards over adjacent
  // text nodes until we hit a block boundary:

  result = mIterator->Last();

  if (NS_FAILED(result))
    return result;

  nsCOMPtr<nsIContent> content;
  nsCOMPtr<nsIContent> last;
  PRBool foundTextBlock = PR_FALSE;

  while (NS_COMFALSE == mIterator->IsDone())
  {
  	result = mIterator->CurrentNode(getter_AddRefs(content));

    if (NS_FAILED(result))
      return result;

    if (IsTextNode(content))
    {
      foundTextBlock = PR_TRUE;

      result = FirstTextNodeInCurrentBlock();

      if (NS_FAILED(result))
        return result;

      break;
    }

    mIterator->Prev();
  }

  // Keep track of prev and next blocks, just in case
  // the text service blows away the current block.

  if (foundTextBlock)
  {
    mIteratorStatus = eValid;
    result = FindFirstTextNodeInPrevBlock(getter_AddRefs(mPrevTextBlock));
    mNextTextBlock = nsCOMPtr<nsIContent>();
  }
  else
  {
    // There's no text block in the document!

    mIteratorStatus = eInvalid;
    mPrevTextBlock = nsCOMPtr<nsIContent>();
    mNextTextBlock = nsCOMPtr<nsIContent>();
  }

  return result;
}

nsresult
nsTextServicesDocument::FirstTextNodeInCurrentBlock()
{
  nsresult result;

  if (!mIterator)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIContent> content;
  nsCOMPtr<nsIContent> last;

  // Walk backwards over adjacent text nodes until
  // we hit a block boundary:

  while (NS_COMFALSE == mIterator->IsDone())
  {
  	result = mIterator->CurrentNode(getter_AddRefs(content));

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

    mIterator->Prev();
  }
  
  if (last)
    result = mIterator->PositionAt(last);

  // XXX: What should we return if last is null?

  return NS_OK;
}

nsresult
nsTextServicesDocument::FirstTextNodeInPrevBlock()
{
  nsCOMPtr<nsIContent> content;
  nsresult result;

  // XXX: What if mIterator is not currently on a text node?

  // Make sure mIterator is pointing to the first text node in the
  // current block:

  result = FirstTextNodeInCurrentBlock();

  if (NS_FAILED(result))
    return NS_ERROR_FAILURE;

  // Point mIterator to the first node before the first text node:

  result = mIterator->Prev();

  if (NS_FAILED(result))
    return result;

  // Now find the first text node of the next block:

  return FirstTextNodeInCurrentBlock();
}

nsresult
nsTextServicesDocument::FirstTextNodeInNextBlock()
{
  nsCOMPtr<nsIContent> content;
  nsCOMPtr<nsIContent> prev;
  PRBool crossedBlockBoundary = PR_FALSE;
  nsresult result;

  while (NS_COMFALSE == mIterator->IsDone())
  {
  	result = mIterator->CurrentNode(getter_AddRefs(content));
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

    mIterator->Next();
  }

  return NS_OK;
}

nsresult
nsTextServicesDocument::FindFirstTextNodeInPrevBlock(nsIContent **aContent)
{
  nsCOMPtr<nsIContent> content;
  nsresult result;

  if (!aContent)
    return NS_ERROR_NULL_POINTER;

  *aContent = 0;

  // Save the iterator's current content node so we can restore
  // it when we are done:

  mIterator->CurrentNode(getter_AddRefs(content));

  result = FirstTextNodeInPrevBlock();

  if (NS_FAILED(result))
  {
    // Try to restore the iterator before returning.
    mIterator->PositionAt(content);
    return result;
  }

  if (mIterator->IsDone() == NS_COMFALSE)
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
nsTextServicesDocument::FindFirstTextNodeInNextBlock(nsIContent **aContent)
{
  nsCOMPtr<nsIContent> content;
  nsresult result;

  if (!aContent)
    return NS_ERROR_NULL_POINTER;

  *aContent = 0;

  // Save the iterator's current content node so we can restore
  // it when we are done:

  mIterator->CurrentNode(getter_AddRefs(content));

  result = FirstTextNodeInNextBlock();

  if (NS_FAILED(result))
  {
    // Try to restore the iterator before returning.
    mIterator->PositionAt(content);
    return result;
  }

  if (mIterator->IsDone() == NS_COMFALSE)
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
    printf("ENTRY %4d: 0x%.8x  %c  %c  %4d  %4d  %4d\n",
           i, (PRInt32)entry->mNode,  entry->mIsValid ? 'V' : 'N',
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
      nsString str;

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
