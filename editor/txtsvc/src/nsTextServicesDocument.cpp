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
#include "nsIDOMRange.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMSelection.h"
#include "nsTextServicesDocument.h"

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
    : mNode(aNode), mNodeOffset(0), mStrOffset(aOffset), mLength(aLength), mIsValid(PR_TRUE)
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
  PRBool  mIsValid;
};

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
  // we really are at the beginning of the curren block:

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
        // XXX: Add text node to the offset table here.

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
  if (!mIterator)
    return NS_ERROR_FAILURE;

  LOCK_DOC(this);

  nsresult result = FirstTextNodeInPrevBlock();

  UNLOCK_DOC(this);

  return result;
}

NS_IMETHODIMP
nsTextServicesDocument::NextBlock()
{
  if (!mIterator)
    return NS_ERROR_FAILURE;

  LOCK_DOC(this);

  nsresult result = FirstTextNodeInNextBlock();

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
    
    if (entry->mIsValid && aOffset >= entry->mStrOffset && aOffset <= entry->mStrOffset + entry->mLength)
    {
      sNode = entry->mNode;
      sOffset = entry->mNodeOffset + aOffset - entry->mStrOffset;

      mSelStartIndex  = i;
      mSelStartOffset = aOffset;
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

  // Find the end of the selection in node offset terms:

  PRInt32 endOffset = aOffset + aLength;

  for (i = 0; !eNode && i < mOffsetTable.Count(); i++)
  {
    entry = (OffsetEntry *)mOffsetTable[i];
    
    if (entry->mIsValid && endOffset >= entry->mStrOffset && endOffset <= entry->mStrOffset + entry->mLength)
    {
      eNode = entry->mNode;
      eOffset = entry->mNodeOffset + endOffset - entry->mStrOffset;

      mSelEndIndex  = i;
      mSelEndOffset = endOffset;
    }
  }

  if (eNode)
  {
    selection->Extend(eNode, eOffset);
  }

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

      selLength = entry->mLength - (mSelStartOffset - entry->mStrOffset);

      if (selLength > 0 && mSelStartOffset > entry->mStrOffset)
      {
        // Selection doesn't start at the beginning of the
        // text node entry. We need to split this entry into
        // two pieces, the piece before the selection, and
        // the piece inside the selection.

        selLength = entry->mLength - (mSelStartOffset - entry->mStrOffset);

        newEntry = new OffsetEntry(entry->mNode, mSelStartOffset, selLength);

        if (!newEntry)
          return NS_ERROR_OUT_OF_MEMORY;


        if (!mOffsetTable.InsertElementAt(newEntry, ++i))
          return NS_ERROR_FAILURE;

        // Adjust entry fields:

        entry->mLength        = entry->mLength - selLength;
        newEntry->mNodeOffset = entry->mNodeOffset + entry->mLength;

        // Adjust selection indexes to account for new entry:

        ++mSelStartIndex;
        ++mSelEndIndex;

        entry = newEntry;
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
  // PrintOffsetTable();
  //**** KDEBUG ****

    if (i == mSelEndIndex)
    {
      if (mSelEndOffset < entry->mStrOffset + entry->mLength)
      {
        // mStrOffset is guaranteed to be inside the selection, even
        // when mSelStartIndex == mSelEndIndex.

        selLength = mSelEndOffset - entry->mStrOffset;

        newEntry = new OffsetEntry(entry->mNode, entry->mStrOffset + selLength,
                                     entry->mLength - selLength);

        if (!newEntry)
          return NS_ERROR_OUT_OF_MEMORY;

        if (!mOffsetTable.InsertElementAt(newEntry, i+1))
          return NS_ERROR_FAILURE;

        // Update the entry fields:

        entry->mLength = selLength;
        newEntry->mNodeOffset = entry->mNodeOffset;
      }


      if (mSelEndOffset == entry->mStrOffset + entry->mLength)
      {
        // The entire entry is contained in the selection. Mark the
        // entry invalid.

        entry->mIsValid = PR_FALSE;
      }
    }

    if (i != mSelStartIndex && i != mSelEndIndex)
    {
      // The entire entry is contained in the selection. Mark the
      // entry invalid.

      entry->mIsValid = PR_FALSE;
    }
  }

  //**** KDEBUG ****
  // printf("\n---- After Delete\n");
  // PrintOffsetTable();
  //**** KDEBUG ****

  result = mEditor->DeleteSelection(nsIEditor::eRTL);

  if (NS_FAILED(result))
    return result;

  entry = 0;

  // Move the cursor to the end of the first valid entry:

  for (i = mSelStartIndex; !entry && i >= 0; i--)
  {
    entry = (OffsetEntry *)mOffsetTable[i];

    if (!entry->mIsValid)
      entry = 0;
    else
    {
      mSelStartIndex = mSelEndIndex = i;
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

    mSelStartIndex = -1;
  }

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

  // We have to call DeleteSelection(), just in case there is
  // a selection, to make sure that our offset table is kept in sync!

  result = DeleteSelection();
  
  if (NS_FAILED(result))
    return result;

  LOCK_DOC(this);

  result = mEditor->InsertText(*aText);

  if (NS_FAILED(result))
  {
    UNLOCK_DOC(this);
    return result;
  }

  if (SelectionIsValid())
  {
    PRInt32 i, strLength  = aText->Length();

    OffsetEntry *entry = (OffsetEntry *)mOffsetTable[mSelStartIndex];
    void *node         = entry->mNode;

    if (entry->mStrOffset == mSelStartOffset)
      entry->mNodeOffset += strLength;

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
  }

  UNLOCK_DOC(this);

  return result;
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
  // Create a range containing the document bodies contents:
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

  UNLOCK_DOC(this);

  return result;
}


PRBool
nsTextServicesDocument::IsBlockNode(nsIContent *aContent)
{
  nsCOMPtr<nsIAtom> atom;

  aContent->GetTag(*getter_AddRefs(atom));

  if (!atom)
    return PR_TRUE;

  return (sAAtom != atom &&
          sAddressAtom != atom &&
          sBigAtom != atom &&
          sBlinkAtom != atom &&
          sBAtom != atom &&
          sCiteAtom != atom &&
          sCodeAtom != atom &&
          sDfnAtom != atom &&
          sEmAtom != atom &&
          sFontAtom != atom &&
          sIAtom != atom &&
          sKbdAtom != atom &&
          sKeygenAtom != atom &&
          sNobrAtom != atom &&
          sSAtom != atom &&
          sSampAtom != atom &&
          sSmallAtom != atom &&
          sSpacerAtom != atom &&
          sSpanAtom != atom &&
          sStrikeAtom != atom &&
          sStrongAtom != atom &&
          sSubAtom != atom &&
          sSupAtom != atom &&
          sTtAtom != atom &&
          sUAtom != atom &&
          sVarAtom != atom &&
          sWbrAtom != atom);
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

  while (NS_COMFALSE == mIterator->IsDone())
  {
  	result = mIterator->CurrentNode(getter_AddRefs(content));

    if (NS_FAILED(result))
      return result;

    if (IsTextNode(content))
      break;

    mIterator->Next();
  }

  return NS_OK;
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

  while (NS_COMFALSE == mIterator->IsDone())
  {
  	result = mIterator->CurrentNode(getter_AddRefs(content));

    if (NS_FAILED(result))
      return result;

    if (IsTextNode(content))
      return FirstTextNodeInCurrentBlock();

    mIterator->Prev();
  }
  
  return NS_OK;
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

void
nsTextServicesDocument::PrintOffsetTable()
{
  OffsetEntry *entry;
  PRInt32 i;

  for (i = 0; i < mOffsetTable.Count(); i++)
  {
    entry = (OffsetEntry *)mOffsetTable[i];
    printf("ENTRY: 0x%.8x  %c  %4d  %4d  %4d\n",
           entry->mNode,  entry->mIsValid ? 'V' : 'N',
           entry->mNodeOffset, entry->mStrOffset, entry->mLength);
  }
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
