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
 * The Original Code is the guts of the find algorithm.
 *
 * The Initial Developer of the Original Code is Akkana Peck.
 *
 * Portions created by the Initial Developer are Copyright (C) 2___
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Akkana Peck <akkana@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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


//#define DEBUG_FIND 1

#include "nsFind.h"
#include "nsContentCID.h"
#include "nsIEnumerator.h"
#include "nsITextContent.h"
#include "nsIDOMNode.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMDocumentRange.h"
#include "nsIDOMDocumentTraversal.h"
#include "nsISelection.h"
#include "nsISelectionController.h"
#include "nsIPresShell.h"
#include "nsTextFragment.h"
#include "nsString.h"
#include "nsIAtom.h"
#include "nsParserCIID.h"
#include "nsIServiceManagerUtils.h"
#include "nsUnicharUtils.h"
#include "nsIDOMElement.h"
#include "nsIDOMHTMLElement.h"
#include "nsIDOMHTMLDocument.h"

static NS_DEFINE_CID(kCContentIteratorCID, NS_CONTENTITERATOR_CID);
static NS_DEFINE_CID(kParserServiceCID, NS_PARSERSERVICE_CID);
static NS_DEFINE_IID(kRangeCID, NS_RANGE_CID);

// Sure would be nice if we could just get these from somewhere else!
PRInt32 nsFind::sInstanceCount = 0;
nsIAtom* nsFind::sScriptAtom = nsnull;

NS_IMPL_ISUPPORTS1(nsFind, nsIFind)

nsFind::nsFind()
  : mFindBackward(PR_FALSE)
  , mCaseSensitive(PR_FALSE)
  , mWrapFind(PR_FALSE)
  , mEntireWord(PR_FALSE)
  , mWrappedOnce(PR_FALSE)
  , mIterOffset(0)
  , mNodeQ(0)
  , mLengthInQ(0)
{
  NS_INIT_ISUPPORTS();

  // Initialize the atoms if they aren't already:
  if (sInstanceCount <= 0)
  {
    sScriptAtom = NS_NewAtom("script");
  }
  ++sInstanceCount;
}

nsFind::~nsFind()
{
  ClearQ();

  if (sInstanceCount <= 1)
    NS_IF_RELEASE(sScriptAtom);
  --sInstanceCount;
}

NS_IMETHODIMP
nsFind::SetDomDoc(nsIDOMDocument* aDomDoc, nsIPresShell* aPresShell)
{
#ifdef DEBUG_FIND
  printf("Setting dom document ...\n");
#endif
  mDoc = aDomDoc;
  mSelCon = do_QueryInterface(aPresShell);

  mRange = nsnull;
  mIterator = nsnull;

  SetRangeAroundDocument();

  return NS_OK;
}

// Adapted from nsTextServicesDocument::GetDocumentContentRootNode
nsresult
nsFind::GetRootNode(nsIDOMNode **aNode)
{
  nsresult rv;

  NS_ENSURE_ARG_POINTER(aNode);
  *aNode = 0;

  if (!mDoc)
    return NS_ERROR_NOT_INITIALIZED;

  nsCOMPtr<nsIDOMHTMLDocument> htmlDoc = do_QueryInterface(mDoc);
  if (htmlDoc)
  {
    // For HTML documents, the content root node is the body.
    nsCOMPtr<nsIDOMHTMLElement> bodyElement;
    rv = htmlDoc->GetBody(getter_AddRefs(bodyElement));
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_ARG_POINTER(bodyElement);
    return bodyElement->QueryInterface(NS_GET_IID(nsIDOMNode),
                                       (void **)aNode);
  }

  // For non-HTML documents, the content root node will be the doc element.
  nsCOMPtr<nsIDOMElement> docElement;
  rv = mDoc->GetDocumentElement(getter_AddRefs(docElement));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_ARG_POINTER(docElement);
  return docElement->QueryInterface(NS_GET_IID(nsIDOMNode), (void **)aNode);
}

void nsFind::SetRangeAroundDocument()
{
#ifdef DEBUG_FIND
  printf("nsFind::SetRangeAroundDocument()\n");
#endif

  // Zero everything in case we have problems:
  mIterOffset = 0;
  mIterNode = nsnull;
  mWrappedOnce = PR_FALSE;
  mRange = nsnull;

  nsCOMPtr<nsIDOMNode> bodyNode;
  nsresult rv = GetRootNode(getter_AddRefs(bodyNode));
  nsCOMPtr<nsIContent> bodyContent (do_QueryInterface(bodyNode));
#ifdef DEBUG_FIND
  if (!bodyContent)
    printf("Document has no root content\n");
#endif
  if (NS_FAILED(rv) || !bodyContent)
    return;

  PRInt32 childCount;
  rv = bodyContent->ChildCount(childCount);
  if (NS_FAILED(rv)) return;
#ifdef DEBUG_FIND
  printf("Body has %d children\n", childCount);
#endif

  if (!mRange)
    mRange = do_CreateInstance(kRangeCID);

  mRange->SetStart(bodyNode, 0);
  mRange->SetEnd(bodyNode, childCount);
}

// Set the range to go from the end of the current selection to the
// end of the document (forward), or beginning to beginning (reverse).
// or around the whole document if there's no selection.
nsresult nsFind::SetRangeToSelection()
{
#ifdef DEBUG_FIND
  printf("nsFind::SetRangeToSelection()\n");
#endif

  if (!mSelCon) return NS_ERROR_NOT_INITIALIZED;

  if (!mRange)
    mRange = do_CreateInstance(kRangeCID);

  nsCOMPtr<nsISelection> selection;
  mSelCon->GetSelection(nsISelectionController::SELECTION_NORMAL,
                        getter_AddRefs(selection));
  if (!selection) return NS_ERROR_BASE;

  // There is a selection.
  PRInt32 count = -1;
  nsresult rv = selection->GetRangeCount(&count);
  if (count < 1) return NS_ERROR_UNEXPECTED;
  nsCOMPtr<nsIDOMNode> node;
  if (mFindBackward)
  {
    nsCOMPtr<nsIDOMRange> range;
    // This isn't quite right, since the selection's ranges aren't
    // necessarily in order; but hopefully they usually will be.
    selection->GetRangeAt(count-1, getter_AddRefs(range));

    range->GetStartContainer(getter_AddRefs(node));
    if (!node) return NS_ERROR_UNEXPECTED;

    PRInt32 offset;
    range->GetStartOffset(&offset);

    // Need bodyNode, for the start of the document
    nsCOMPtr<nsIDOMNode> bodyNode;
    rv = GetRootNode(getter_AddRefs(bodyNode));
    NS_ENSURE_SUCCESS(rv, rv);

    // Set mRange to go from the beginning of the doc to this node
    mRange->SetStart(bodyNode, 0);
    mRange->SetEnd(node, offset);
  }
  else // forward
  {
    nsCOMPtr<nsIDOMRange> range;
    selection->GetRangeAt(0, getter_AddRefs(range));

    range->GetEndContainer(getter_AddRefs(node));
    if (!node) return NS_ERROR_UNEXPECTED;

    PRInt32 offset;
    range->GetEndOffset(&offset);

    // Need bodyNode, for the start of the document
    nsCOMPtr<nsIDOMNode> bodyNode;
    rv = GetRootNode(getter_AddRefs(bodyNode));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIContent> bodyContent (do_QueryInterface(bodyNode));
    NS_ENSURE_ARG_POINTER(bodyContent);
    PRInt32 childCount;
    rv = bodyContent->ChildCount(childCount);

    // Set mRange to go from end node to end of doc
    mRange->SetStart(node, offset);
    mRange->SetEnd(bodyNode, childCount);
  }
  return NS_OK;
}

nsresult nsFind::InitIterator()
{
  nsresult rv;
  if (!mIterator)
  {
    rv = nsComponentManager::CreateInstance(kCContentIteratorCID,
                                            nsnull,
                                            NS_GET_IID(nsIContentIterator), 
                                            getter_AddRefs(mIterator));
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_ARG_POINTER(mIterator);
  }

  // Initialize the range if it isn't already
  if (!mRange)
  {
    rv = SetRangeToSelection();
    if (NS_FAILED(rv))
      SetRangeAroundDocument();
  }

  mIterator->Init(mRange);
  if (mFindBackward) {
    // Use post-order in the reverse case,
    // so we get parents before children,
    // in case we want to precent descending into a node.
    mIterator->MakePost();
    mIterator->Last();
  } else {
    // Use pre-order in the forward case.
    // Pre order is currently broken (will skip nodes!), so don't use it.
    //mIterator->MakePre();
    mIterator->First();
  }
  return NS_OK;
}

nsresult
nsFind::SetRange(nsIDOMRange* aRange)
{
  mRange = aRange;
  mIterOffset = 0;
  mIterNode = nsnull;
  mWrappedOnce = PR_FALSE;
  return NS_OK;
}

/* attribute boolean findBackward; */
NS_IMETHODIMP
nsFind::GetFindBackwards(PRBool *aFindBackward)
{
  if (!aFindBackward)
    return NS_ERROR_NULL_POINTER;

  *aFindBackward = mFindBackward;

  return NS_OK;
}
NS_IMETHODIMP
nsFind::SetFindBackwards(PRBool aFindBackward)
{
  // If we're changing directions, reset wrapping as well:
  if (aFindBackward != mFindBackward)
    mWrappedOnce = PR_FALSE;

  mFindBackward = aFindBackward;

  return NS_OK;
}

/* attribute boolean caseSensitive; */
NS_IMETHODIMP
nsFind::GetCaseSensitive(PRBool *aCaseSensitive)
{
  if (!aCaseSensitive)
    return NS_ERROR_NULL_POINTER;

  *aCaseSensitive = mCaseSensitive;

  return NS_OK;
}
NS_IMETHODIMP
nsFind::SetCaseSensitive(PRBool aCaseSensitive)
{
  mCaseSensitive = aCaseSensitive;

  return NS_OK;
}

/* attribute boolean wrapFind; */
NS_IMETHODIMP
nsFind::GetWrapFind(PRBool *aWrapFind)
{
  if (!aWrapFind)
    return NS_ERROR_NULL_POINTER;

  *aWrapFind = mWrapFind;

  return NS_OK;
}
NS_IMETHODIMP
nsFind::SetWrapFind(PRBool aWrapFind)
{
  mWrapFind = aWrapFind;

  return NS_OK;
}

/* attribute boolean entireWord; */
NS_IMETHODIMP
nsFind::GetEntireWord(PRBool *aEntireWord)
{
  if (!aEntireWord)
    return NS_ERROR_NULL_POINTER;

  *aEntireWord = mEntireWord;

  return NS_OK;
}
NS_IMETHODIMP
nsFind::SetEntireWord(PRBool aEntireWord)
{
  mEntireWord = aEntireWord;

  return NS_OK;
}

/* readonly attribute boolean replaceEnabled; */
NS_IMETHODIMP
nsFind::GetReplaceEnabled(PRBool *aReplaceEnabled)
{
  if (!aReplaceEnabled)
    return NS_ERROR_NULL_POINTER;

  *aReplaceEnabled = PR_FALSE;

  nsresult result = NS_OK;

  return result;
}

#ifdef DEBUG_FIND
static void DumpNode(nsIDOMNode* aNode)
{
  if (!aNode)
  {
    printf(">>>> Node: NULL\n");
    return;
  }
  nsAutoString nodeName;
  aNode->GetNodeName(nodeName);
  nsCOMPtr<nsITextContent> textContent (do_QueryInterface(aNode));
  if (textContent)
  {
    nsAutoString newText;
    textContent->CopyText(newText);
    printf(">>>> Text node: '%s'\n",
           NS_LossyConvertUCS2toASCII(newText).get());
  }
  else
    printf(">>>> Node: %s\n", NS_LossyConvertUCS2toASCII(nodeName).get());
}
#endif

//
// Here begins the find code.
// A ten-thousand-foot view of how it works:
// Find needs to be able to compare across inline (but not block) nodes,
// e.g. find for "abc" should match a<b>b</b>c.
// So after we've searched a node, we're not done with it;
// in the case of a partial match we may need to go back and
// look at the node's string again, so we save the last few
// nodes visited in a deque (a double-ended queue).
// When we're comparing, we look through the queue first, then
// start pulling new nodes from the tree, queueing them as necessary.
//
// Text nodes store their text in an nsTextFragment, which is 
// effectively a union of a one-byte string or a two-byte string.
// Single and double strings are intermixed in the dom.
// We don't have string classes which can deal with intermixed strings,
// so all the handling is done explicitly here.
//
// Ongoing questions:
// 1. We remember the node and offset from the last find.
//    This is wrong, and will break when we replace.  We should either:
//    a) Remember a range (and let range gravity manage it);
//    b) Always search forward or backward from the current selection.
//    A poll on IRC seemed to indicate it was okay to use the selection.
//
// 2. Rather than having NextNode know how to wrap, it might be better
//    if FindInQ only searched once, then we could handle wrapping here.
//    Arguable.
//
// 3. Some people want to be able to search in text controls,
//    not just text nodes.  It should be possible.
//    This would be especially useful for webmail users
//    (e.g. if the caret is in a text control, search only there).
//

/* boolean Find (in wstring aFindText); */
NS_IMETHODIMP
nsFind::Find(const PRUnichar *aPatText, PRBool *aDidFind)
{
#ifdef DEBUG_FIND
  printf("============== nsFind::Find(%s)\n",
         NS_LossyConvertUCS2toASCII(aPatText).get());
#endif

  if (!aPatText || !aDidFind)
    return NS_ERROR_NULL_POINTER;

  *aDidFind = PR_FALSE;

  if (!mDoc)
    return NS_ERROR_NOT_INITIALIZED;

  nsAutoString patStr(aPatText);
  if (!mCaseSensitive)
    ToLowerCase(patStr);
  aPatText = patStr.get();
  PRInt32 patLen = patStr.Length();

  nsCOMPtr<nsIDOMRange> range;
  PRBool found = FindInQ(aPatText, patLen-1, getter_AddRefs(range));
  if (found)
  {
#ifdef DEBUG_FIND
    printf("Matched!\n");
#endif
    *aDidFind = PR_TRUE;

    // Set the selection:
    // XXX The intent for the future is that nsFind will not set selection;
    // instead, it will return a range, and nsWebBrowserFind will be a
    // wrapper that sets the selection and scrolls.
    SetSelectionAndScroll(range);
  }

  // Clear queue and range; we want to be stateless.
  // Caller should reset our range before calling us again
  // (or else we'll reset it according to the selection).
  ClearQ();
  mRange = 0;
  mIterator = 0;
  mLastBlockParent = 0;

  return NS_OK;
}

// Add this node to the deque, popping the top node off if needed.
void
nsFind::AddIterNode(PRInt32 aPatLen)
{
#ifdef DEBUG_FIND
  printf("AddIterNode\n");
#endif
  nsCOMPtr<nsITextContent> textContent (do_QueryInterface(mIterNode));
  if (!textContent) return;

  // If it's only whitespace, don't waste our time.
  // This is probably worthwhile even though it has to loop over the string,
  // since it may save us several loops in future iterations.
  PRBool onlyWhite = PR_FALSE;
  nsresult rv = textContent->IsOnlyWhitespace(&onlyWhite);
  if (onlyWhite) return;

  PRInt32 newLen;
  rv = textContent->GetTextLength(&newLen);
  if (NS_FAILED(rv)) return;

  // Now look through the queue and see if we can pop the oldest node off:
  // We know mLengthInQ and aPatLen, can get length of the oldest item:
  while (mNodeQ.GetSize() > 0)
  {
    nsITextContent* oldestItem =
      mFindBackward ? NS_STATIC_CAST(nsITextContent*, mNodeQ.PeekFront())
                    : NS_STATIC_CAST(nsITextContent*, mNodeQ.Peek());
    NS_ASSERTION(oldestItem, "Queue nonzero but can't peek!");
    if (!oldestItem)
      break;

    PRInt32 oldestLength;
    rv = oldestItem->GetTextLength(&oldestLength);
    if (NS_SUCCEEDED(rv))   // If we can't get text length, pop it anyway
    {
      if (mLengthInQ - oldestLength - mIterOffset < aPatLen)
        break;

      // still long enough, we're going to pop one:
      mLengthInQ -= oldestLength;
    }
    // Pop-and-release whether or not it had a length:
    nsITextContent* tc = (mFindBackward
                          ? NS_STATIC_CAST(nsITextContent*, mNodeQ.Pop())
                          : NS_STATIC_CAST(nsITextContent*, mNodeQ.PopFront())
                         );
    NS_IF_RELEASE(tc);
    // If we popped, then mIterOffset no longer matters:
    mIterOffset = 0;
#ifdef DEBUG_FIND
    printf("Popping something of length %d, queue now %d in %d nodes\n",
           oldestLength, mLengthInQ, mNodeQ.GetSize());
#endif
  }

  // Push the new node onto the queue:
  // nsDeque doesn't addref, so we have to do it explicitly:
  mLengthInQ += newLen;
#ifdef DEBUG_FIND
  printf("Adding %d chars to make %d\n", newLen, mLengthInQ);
#endif
  // Windows compiler won't let us addref the results of a nsCOMPtr::get
  nsITextContent* rawtxt = textContent;
  NS_ADDREF(rawtxt);
  if (mFindBackward)
    mNodeQ.PushFront(rawtxt);
  else
    mNodeQ.Push(rawtxt);
}

// Unfortunately, we can't use Erase(), because that doesn't release
// the nodes in the queue.  So we have to pop and release each one.
void nsFind::ClearQ()
{
#ifdef DEBUG_FIND
  printf("Clearing Deque\n");
#endif
  nsITextContent* tc;
  while ((tc = NS_STATIC_CAST(nsITextContent*, mNodeQ.PopFront())))
    NS_RELEASE(tc);
  mLengthInQ = 0;

  // If we have no queue, then mIterOffset is no longer useful:
  mIterOffset = 0;
}

void
nsFind::SetSelectionAndScroll(nsIDOMRange* aRange)
{
  // foundStrLen is the true # chars in the text nodes in the dom tree,
  // so we can use it when searching forward/back.

  nsCOMPtr<nsISelection> selection;
  if (!mSelCon) return;

  mSelCon->GetSelection(nsISelectionController::SELECTION_NORMAL,
                        getter_AddRefs(selection));
  if (!selection) return;
  selection->RemoveAllRanges();
  selection->AddRange(aRange);

  // Scroll if necessary to make the selection visible:
  mSelCon->ScrollSelectionIntoView
    (nsISelectionController::SELECTION_NORMAL,
     nsISelectionController::SELECTION_FOCUS_REGION);
}

nsresult
nsFind::NextNode()
{
  nsresult rv;

  nsCOMPtr<nsIContent> content;
  nsCOMPtr<nsITextContent> tc;

  if (!mIterator)
  {
    rv = InitIterator();
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mIterator->CurrentNode(getter_AddRefs(content));
#ifdef DEBUG_FIND
    nsCOMPtr<nsIDOMNode> dnode (do_QueryInterface(content));
    printf(":::::::::::::::::::::::::: Got the first node "); DumpNode(dnode);
#endif
    tc = do_QueryInterface(content);
    if (tc)
    {
      mIterNode = do_QueryInterface(content);
      return NS_OK;
    }
  }

  while (1)
  {
    if (mFindBackward)
      rv = mIterator->Prev();
    else
      rv = mIterator->Next();
    if (NS_FAILED(rv)) break;
    rv = mIterator->CurrentNode(getter_AddRefs(content));
#ifdef DEBUG_FIND
    nsCOMPtr<nsIDOMNode> dnode (do_QueryInterface(content));
    printf(":::::::::::::::::::::::::: Got another node "); DumpNode(dnode);
#endif
    // nsIContentIterator.h says Next() will return error at end,
    // but it doesn't really, so we have to check:
    if (NS_FAILED(rv) || !content)
      break;
    tc = do_QueryInterface(content);
    if (tc)
      break;
#ifdef DEBUG_FIND
    dnode = do_QueryInterface(content);
    printf("Not a text node: "); DumpNode(dnode);
#endif

    // If we ever cross a block node, flush the queue --
    // we don't match patterns extending across block boundaries.
    //if (IsBlockNode(content))
    //  ClearQ();

    // Now see if we need to skip this node --
    // e.g. is it part of a script or other invisible node?
    // Note that we don't ask for CSS information;
    // a node can be invisible due to CSS, and we'd still find it.
    while (SkipNode(content))
    {
      nsCOMPtr<nsIDOMNode> node (do_QueryInterface(content));
      if (node)
      {
        nsCOMPtr<nsIDOMNode> sib;
        if (mFindBackward)
          node->GetPreviousSibling(getter_AddRefs(sib));
        else
          node->GetNextSibling(getter_AddRefs(sib));
        content = do_QueryInterface(sib);
        if (content)
          mIterator->PositionAt(content);
        else {
          // Can a nsIDOMNode ever not be an nsIContent?
          // If so, should we loop until we find one?
          NS_ASSERTION(content, "Find: Node is not content\n");
        }
      }
    }
  }

  if (content)
    mIterNode = do_QueryInterface(content);
  else
    mIterNode = nsnull;

  // See if we need to wrap around
  if (!mIterNode && mWrapFind && !mWrappedOnce)
  {
#ifdef DEBUG_FIND
    printf("Wrapping ...\n");
#endif
    mWrappedOnce = 1;
    mIterator = nsnull;
    return NextNode();    // tail recurse
  }

#ifdef DEBUG_FIND
  printf("Iterator gave: "); DumpNode(mIterNode);
#endif
  return NS_OK;
}

PRBool nsFind::IsBlockNode(nsIContent* aContent)
{
  nsCOMPtr<nsIAtom> atom;
  aContent->GetTag(*getter_AddRefs(atom));

  if (!mParserService) {
    nsresult rv;
    mParserService = do_GetService(kParserServiceCID, &rv);
    if (NS_FAILED(rv) || !mParserService) return PR_FALSE;
  }

  PRInt32 id;
  mParserService->HTMLAtomTagToId(atom, &id);

  PRBool isBlock = PR_FALSE;
  mParserService->IsBlock(id, isBlock);
  return isBlock;
}

PRBool nsFind::IsTextNode(nsIDOMNode* aNode)
{
  nsCOMPtr<nsITextContent> content (do_QueryInterface(aNode));
  if (content) return PR_TRUE;
  else return PR_FALSE;
}

PRBool nsFind::SkipNode(nsIContent* aContent)
{
  // Skipping depends on the iterator using the right order.
  // Currently, post order (used by reverse find) works,
  // but pre order has problems.  So turn off skipping for forward find.
  if (!mFindBackward)
    return PR_FALSE;

  nsCOMPtr<nsIAtom> atom;
  aContent->GetTag(*getter_AddRefs(atom));
  if (!atom)
    return PR_TRUE;
  nsIAtom *atomPtr = atom.get();
  return (sScriptAtom == atomPtr);
}

nsresult nsFind::GetBlockParent(nsIDOMNode* aNode, nsIDOMNode** aParent)
{
  while (aNode)
  {
    nsCOMPtr<nsIDOMNode> parent;
    nsresult rv = aNode->GetParentNode(getter_AddRefs(parent));
    NS_ENSURE_SUCCESS(rv, rv);
    nsCOMPtr<nsIContent> content (do_QueryInterface(parent));
    if (content && IsBlockNode(content))
    {
      *aParent = parent;
      NS_ADDREF(*aParent);
      return NS_OK;
    }
    aNode = parent;
  }
  return NS_ERROR_FAILURE;
}

#define NBSP_CHARCODE ((PRUnichar)160)
#define IsSpace(c) (nsCRT::IsAsciiSpace(c) || (c) == NBSP_CHARCODE)

//
// FindInQ:
// If we have nodes in the queue, search them first.
// Then take nodes out of the tree with NextNode,
// until null (NextNode will return 0 at the end of our range).
// Save intermediates in the deque.
//
PRBool nsFind::FindInQ(const PRUnichar* aPatStr, PRInt32 aPatLen,
                       nsIDOMRange** aRangeRet)
{
#ifdef DEBUG_FIND
  printf("FindInQ for '%s'%s\n",
         NS_LossyConvertUCS2toASCII(aPatStr).get(),
         mFindBackward ? " (backward)" : "");
#endif
  // The offset only matters for the first node in the queue,
  // so make a local copy so that we can zero it when we loop:
  PRInt32 offset = mIterOffset;

  // current offset into the pattern -- reset to beginning/end:
  PRInt32 pindex = (mFindBackward ? aPatLen : 0);

  // Current offset into the fragment
  PRInt32 findex = 0;

  // Place in the fragment to re-start the match:
  PRInt32 restart = 0;

  // Direction to move pindex and ptr*
  int incr = (mFindBackward ? -1 : 1);

  // Loop over nodes in our queue:
  nsDequeIterator iter (mFindBackward ? mNodeQ.End() : mNodeQ.Begin());
  nsDequeIterator iterEnd (mFindBackward ? mNodeQ.Begin() : mNodeQ.End());
  nsITextContent* tc;
  const nsTextFragment *frag = nsnull;
  PRBool fromQ = PR_TRUE;

  // Pointers into the current fragment:
  const PRUnichar *t2b = nsnull;
  const char      *t1b = nsnull;

  // Keep track of when we're in whitespace:
  // (only matters when we're matching)
  PRBool inWhitespace = PR_FALSE;

  // Place to save the range start point in case we find a match:
  nsITextContent* matchAnchorTC = nsnull;
  PRInt32 matchAnchorOffset = 0;

  //while ((tc = NS_STATIC_CAST(nsITextContent*, iter.GetCurrent())) != nsnull)
  while (1)
  {
#ifdef DEBUG_FIND
    printf("Loop ...\n");
#endif

    // If this is our first time on a new node, reset the pointers:
    if (!frag)
    {
#ifdef DEBUG_FIND
      printf("New node -- Resetting pointers. Deque size is %d\n",
             mNodeQ.GetSize());
#endif

      // Get the current node, either from the deque or the tree.
      if (fromQ && (iter != iterEnd))
        tc = NS_STATIC_CAST(nsITextContent*, iter.GetCurrent());
      else
        tc = nsnull;
      if (tc == nsnull)  // no more deque
      {
#ifdef DEBUG_FIND
        printf("No more deque, looking in tree\n");
#endif
        fromQ = PR_FALSE;
        NextNode();
        if (!mIterNode)    // Out of nodes
        {
          // Reset the iterator, so this nsFind will be usable if
          // the user wants to search again (from beginning/end).
          mIterator = nsnull;
          return PR_FALSE;
        }

        // We have a new text content.  If its block parent is different
        // from the block parent of the last text content, then we
        // need to clear the queue since we don't want to find
        // across block boundaries.
        nsCOMPtr<nsIDOMNode> blockParent;
        GetBlockParent(mIterNode, getter_AddRefs(blockParent));
        if (blockParent && (blockParent != mLastBlockParent))
        {
          ClearQ();
          mLastBlockParent = blockParent;
        }

        // Push mIterNode onto the queue:
        mIterNode->QueryInterface(nsITextContent::GetIID(), (void**)&tc);
        AddIterNode(aPatLen);
      }
      if (tc == nsnull)         // Out of nodes
        return PR_FALSE;
#ifdef DEBUG_FIND
      else printf("Got something from the queue\n");
#endif

      nsresult rv = tc->GetText(&frag);
      if (NS_FAILED(rv)) continue;
      if (offset > 0)
        restart = offset;
      else if (mFindBackward)
        restart = frag->GetLength()-1;
      else
        restart = 0;
      findex = restart;
#ifdef DEBUG_FIND
      printf("Starting from offset %d\n", findex);
#endif
      if (frag->Is2b())
      {
        t2b = frag->Get2b();
        t1b = nsnull;
#ifdef DEBUG_FIND
        printf("2 byte, '%s'\n", NS_LossyConvertUCS2toASCII(t2b).get());
#endif
      }
      else
      {
        t1b = frag->Get1b();
        t2b = nsnull;
#ifdef DEBUG_FIND
        // t1b isn't null terminated, so we may print garbage after the string.
        // If we want to be accurate, would be better to make a new nsCString.
        printf("1 byte, '%s'\n", t1b);
#endif
      }
    }
    else // still on the old node
    {
      // Still on the old node.  Advance the pointers,
      // then see if we need to pull a new node from the queue.
      findex += incr;
#ifdef DEBUG_FIND
      printf("Same node -- (%d, %d)\n", pindex, findex);
#endif
      if (mFindBackward ? (findex < 0) : (findex >= frag->GetLength()))
      {
#ifdef DEBUG_FIND
        printf("Pulling a new node: restart=%d, frag len=%d\n",
               restart, frag->GetLength());
#endif
        // Done with this node.  Pull a new one.
        // If restart is the first/last char,
        // then it's safe to pop this because we aren't using the
        // contents of the node any more.
        mLengthInQ -= frag->GetLength();
        if (mFindBackward) {
          --iter;
          if (restart <= 0)
            mNodeQ.Pop();
        } else {
          ++iter;
          if (restart >= frag->GetLength())
            mNodeQ.PopFront();
        }

        frag = nsnull;
        // Offset can only apply to the first node:
        offset = -1;
        continue;
      }
    }

    // The two characters we'll be comparing:
    PRUnichar c = (t2b ? t2b[findex] : (PRUnichar)(t1b[findex]));
    PRUnichar patc = aPatStr[pindex];

    // Is it time to go back to non-whitespace mode?
    if (inWhitespace)
    {
      if (!IsSpace(c))
        inWhitespace = PR_FALSE;
    }
    // Have we just hit a space?
    else if (IsSpace(patc))
    {
      inWhitespace = PR_TRUE;
      // go forward to the last whitespace in the pat str
      while ((mFindBackward ? pindex > 0 : pindex < aPatLen-1)
             && IsSpace(aPatStr[pindex+incr]))
        pindex += incr;
    }
    // convert to lower case if necessary
    if (!mCaseSensitive && !inWhitespace && IsUpperCase(c))
      c = ToLowerCase(c);

    // Compare
    if ((inWhitespace && IsSpace(c)) || (patc == c))
    {
      // Are we done?
      if (mFindBackward ? pindex <= 0 : pindex >= aPatLen)
        // Matched the whole string!
      {
#ifdef DEBUG_FIND
        printf("Found a match!\n");
#endif

        // Make the range:
        nsCOMPtr<nsIDOMRange> range;
        nsCOMPtr<nsIDOMDocumentRange> docr (do_QueryInterface(mDoc));
        if (docr && aRangeRet)
        {
          docr->CreateRange(getter_AddRefs(range));
          PRInt32 matchStartOffset, matchEndOffset;
          if (range)
          {
            nsCOMPtr<nsIDOMNode> startParent;
            nsCOMPtr<nsIDOMNode> endParent;
            if (mFindBackward)
            {
              startParent = do_QueryInterface(tc);
              endParent = do_QueryInterface(matchAnchorTC);
              matchStartOffset = findex;
              matchEndOffset = matchAnchorOffset;
            }
            else
            {
              startParent = do_QueryInterface(matchAnchorTC);
              endParent = do_QueryInterface(tc);
              matchStartOffset = matchAnchorOffset;
              matchEndOffset = findex+1;
            }
            if (startParent && endParent)
            {
              range->SetStart(startParent, matchStartOffset);
              range->SetEnd(endParent, matchEndOffset);
              *aRangeRet = range.get();
              NS_ADDREF(*aRangeRet);
            }
          }
        }

        // Pop all but the last node off the queue
        ClearQ();
        NS_ADDREF(tc);
        mNodeQ.Push(tc);
        mLengthInQ = frag->GetLength();
        // Reset the offset to the other end of the found string:
        mIterOffset = findex + (mFindBackward ? 1 : 0);
#ifdef DEBUG_FIND
        printf("mIterOffset = %d, mIterNode = ", mIterOffset);
        DumpNode(mIterNode);
#endif

        return PR_TRUE;
      }

      // Not done, but still matching.
#ifdef DEBUG_FIND
      printf("YES! %c == %c (%d of %d)\n", inWhitespace ? ' ' : patc, c,
             pindex, aPatLen);
#endif

      // Save the range anchors if we haven't already:
      if (!matchAnchorTC) {
        matchAnchorTC = tc;
        matchAnchorOffset = findex + (mFindBackward ? 1 : 0);
      }

      // Advance and loop around for the next characters.
      pindex += incr;
      //findex += incr;
      continue;
    }

#ifdef DEBUG_FIND
    printf("NOT: %c == %c\n", inWhitespace ? ' ' : patc, c);
#endif
    // If we didn't match, go back to the beginning of patStr,
    // and set findex back to the next char after
    // we started the current match.
    matchAnchorTC = nsnull;
    pindex = (mFindBackward ? aPatLen : 0);
    inWhitespace = PR_FALSE;
    //findex = restart;  // +incr will be added when we continue
  }

  // Out of nodes, and didn't match.
  return PR_FALSE;
}


