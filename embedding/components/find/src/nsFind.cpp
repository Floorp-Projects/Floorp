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

// Yikes!  Casting a char to unichar can fill with ones!
#define CHAR_TO_UNICHAR(c) ((PRUnichar)(const unsigned char)c)

static NS_DEFINE_CID(kCContentIteratorCID, NS_CONTENTITERATOR_CID);
static NS_DEFINE_CID(kParserServiceCID, NS_PARSERSERVICE_CID);
static NS_DEFINE_IID(kRangeCID, NS_RANGE_CID);

// Sure would be nice if we could just get these from somewhere else!
PRInt32 nsFind::sInstanceCount = 0;
nsIAtom* nsFind::sScriptAtom = nsnull;
nsIAtom* nsFind::sTextAtom = nsnull;
nsIAtom* nsFind::sCommentAtom = nsnull;
nsIAtom* nsFind::sImgAtom = nsnull;
nsIAtom* nsFind::sHRAtom = nsnull;

NS_IMPL_ISUPPORTS1(nsFind, nsIFind)

nsFind::nsFind()
  : mFindBackward(PR_FALSE)
  , mCaseSensitive(PR_FALSE)
  , mIterOffset(0)
  , mNodeQ(0)
  , mLengthInQ(0)
{
  NS_INIT_ISUPPORTS();

  // Initialize the atoms if they aren't already:
  if (sInstanceCount <= 0)
  {
    sScriptAtom = NS_NewAtom("script");
    sTextAtom = NS_NewAtom("__moz_text");
    sCommentAtom = NS_NewAtom("__moz_comment");
    sImgAtom = NS_NewAtom("img");
    sHRAtom = NS_NewAtom("hr");
  }
  ++sInstanceCount;
}

nsFind::~nsFind()
{
  ClearQ();

  if (sInstanceCount <= 1)
  {
    NS_IF_RELEASE(sScriptAtom);
    NS_IF_RELEASE(sTextAtom);
    NS_IF_RELEASE(sCommentAtom);
    NS_IF_RELEASE(sImgAtom);
    NS_IF_RELEASE(sHRAtom);
  }
  --sInstanceCount;
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
    printf(">>>> Text node (node name %s): '%s'\n",
           NS_LossyConvertUCS2toASCII(nodeName).get(),
           NS_LossyConvertUCS2toASCII(newText).get());
  }
  else
    printf(">>>> Node: %s\n", NS_LossyConvertUCS2toASCII(nodeName).get());
}

static void DumpRange(nsIDOMRange* aRange)
{
  nsCOMPtr<nsIDOMNode> startN;
  nsCOMPtr<nsIDOMNode> endN;
  PRInt32 startO, endO;
  aRange->GetStartContainer(getter_AddRefs(startN));
  aRange->GetStartOffset(&startO);
  aRange->GetEndContainer(getter_AddRefs(endN));
  aRange->GetEndOffset(&endO);
  printf(" -- start %d, ", startO); DumpNode(startN);
  printf(" -- end %d, ", endO); DumpNode(endN);
}
#endif

nsresult
nsFind::InitIterator(nsIDOMRange* aSearchRange)
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

  NS_ENSURE_ARG_POINTER(aSearchRange);

#ifdef DEBUG_FIND
  printf("InitIterator search range:\n"); DumpRange(aSearchRange);
#endif

  mIterator->Init(aSearchRange);
  if (mFindBackward) {
    // Use post-order in the reverse case,
    // so we get parents before children,
    // in case we want to prevent descending into a node.
    mIterator->MakePost();
    mIterator->Last();
  }
  else {
    // Use pre-order in the forward case.
    // Pre order is currently broken (will skip nodes!), so don't use it.
    //mIterator->MakePre();
    mIterator->First();
  }
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

NS_IMETHODIMP
nsFind::GetWordBreaker(nsIWordBreaker** aWordBreaker)
{
  NS_ADDREF(*aWordBreaker = mWordBreaker);
  return NS_OK;
}

NS_IMETHODIMP
nsFind::SetWordBreaker(nsIWordBreaker* aWordBreaker)
{
  mWordBreaker = aWordBreaker;
  return NS_OK;
}

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

/* boolean Find (in wstring aFindText); */
NS_IMETHODIMP
nsFind::Find(const PRUnichar *aPatText, nsIDOMRange* aSearchRange,
             nsIDOMRange* aStartPoint, nsIDOMRange* aEndPoint,
             nsIDOMRange** aFoundRange)
{
#ifdef DEBUG_FIND
  printf("============== nsFind::Find('%s', %p, %p, %p)\n",
         NS_LossyConvertUCS2toASCII(aPatText).get(),
         (void*)aSearchRange, (void*)aStartPoint, (void*)aEndPoint);
#endif

  if (!aPatText)
    return NS_ERROR_NULL_POINTER;

  mIterator = 0;

  nsAutoString patStr(aPatText);
  if (!mCaseSensitive)
    ToLowerCase(patStr);
  aPatText = patStr.get();
  PRInt32 patLen = patStr.Length();

  nsCOMPtr<nsIDOMRange> range;
  FindInQ(aPatText, patLen-1, aSearchRange, aStartPoint, aEndPoint,
          aFoundRange);

  // Clear queue and range; we want to be stateless.
  // Caller should reset our range before calling us again.
  ClearQ();
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
  printf("Clear the deque!\n");
#endif
  nsITextContent* tc;
  while ((tc = NS_STATIC_CAST(nsITextContent*, mNodeQ.PopFront())))
    NS_RELEASE(tc);
  mLengthInQ = 0;

  // If we have no queue, then mIterOffset is no longer useful:
  mIterOffset = 0;
}

nsresult
nsFind::NextNode(nsIDOMRange* aSearchRange,
                 nsIDOMRange* aStartPoint, nsIDOMRange* aEndPoint,
                 PRBool aContinueOk)
{
  nsresult rv;

  nsCOMPtr<nsIContent> content;
  nsCOMPtr<nsITextContent> tc;

  if (!mIterator || aContinueOk)
  {
      // If we are continuing, that means we have a match in progress.
      // In that case, we want to continue from the end point
      // (where we are now) to the beginning/end of the search range.
    nsCOMPtr<nsIDOMRange> newRange (do_CreateInstance(kRangeCID));
    if (aContinueOk)
    {
#ifdef DEBUG_FIND
      printf("Match in progress: continuing past endpoint\n");
#endif
      nsCOMPtr<nsIDOMNode> startNode;
      nsCOMPtr<nsIDOMNode> endNode;
      PRInt32 startOffset, endOffset;
      if (mFindBackward) {
        aSearchRange->GetStartContainer(getter_AddRefs(startNode));
        aSearchRange->GetStartOffset(&startOffset);
        aEndPoint->GetStartContainer(getter_AddRefs(endNode));
        aEndPoint->GetStartOffset(&endOffset);
      } else {     // forward
        aEndPoint->GetEndContainer(getter_AddRefs(startNode));
        aEndPoint->GetEndOffset(&startOffset);
        aSearchRange->GetEndContainer(getter_AddRefs(endNode));
        aSearchRange->GetEndOffset(&endOffset);
      }
      newRange->SetStart(startNode, startOffset);
      newRange->SetEnd(endNode, endOffset);
    }
    else  // Normal, not continuing
    {
      nsCOMPtr<nsIDOMNode> startNode;
      nsCOMPtr<nsIDOMNode> endNode;
      PRInt32 startOffset, endOffset;
      if (mFindBackward) {
        aSearchRange->GetStartContainer(getter_AddRefs(startNode));
        aSearchRange->GetStartOffset(&startOffset);
        aStartPoint->GetEndContainer(getter_AddRefs(endNode));
        aStartPoint->GetEndOffset(&endOffset);
      } else {     // forward
        aStartPoint->GetStartContainer(getter_AddRefs(startNode));
        aStartPoint->GetStartOffset(&startOffset);
        aEndPoint->GetEndContainer(getter_AddRefs(endNode));
        aEndPoint->GetEndOffset(&endOffset);
      }
      newRange->SetStart(startNode, startOffset);
      newRange->SetEnd(endNode, endOffset);
    }

    rv = InitIterator(newRange);
    NS_ENSURE_SUCCESS(rv, rv);
    if (!aStartPoint)
      aStartPoint = aSearchRange;

    rv = mIterator->CurrentNode(getter_AddRefs(content));
#ifdef DEBUG_FIND
    nsCOMPtr<nsIDOMNode> dnode (do_QueryInterface(content));
    printf(":::::::::::::::::::::::::: Got the first node "); DumpNode(dnode);
#endif
    tc = do_QueryInterface(content);
    if (tc)
    {
      mIterNode = do_QueryInterface(content);
      // Also set mIterOffset if appropriate:
      nsCOMPtr<nsIDOMNode> node;
      if (mFindBackward) {
        aStartPoint->GetEndContainer(getter_AddRefs(node));
        if (mIterNode.get() == node.get())
          aStartPoint->GetEndOffset(&mIterOffset);
        else
          mIterOffset = -1;   // sign to start from end
      }
      else
      {
        aStartPoint->GetStartContainer(getter_AddRefs(node));
        if (mIterNode.get() == node.get())
          aStartPoint->GetStartOffset(&mIterOffset);
        else
          mIterOffset = 0;
      }
#ifdef DEBUG_FIND
      printf("Setting initial offset to %d\n", mIterOffset);
#endif
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
#if DEBUG_FIND
        else {
          // What should we do if node is not an nsIContent?
          // Should we loop until we find something that is?
          // In practice, this doesn't seem to cause problems.
          NS_ASSERTION(content, "Find: Node is not content\n");
        }
#endif
      }
    }

    tc = do_QueryInterface(content);
    if (tc)
      break;
#ifdef DEBUG_FIND
    dnode = do_QueryInterface(content);
    printf("Not a text node: "); DumpNode(dnode);
#endif
  }

  if (content)
    mIterNode = do_QueryInterface(content);
  else
    mIterNode = nsnull;
  mIterOffset = -1;

#ifdef DEBUG_FIND
  printf("Iterator gave: "); DumpNode(mIterNode);
#endif
  return NS_OK;
}

PRBool nsFind::IsBlockNode(nsIContent* aContent)
{
  nsCOMPtr<nsIAtom> atom;
  aContent->GetTag(*getter_AddRefs(atom));

  if (atom.get() == sImgAtom || atom.get() == sHRAtom)
    return PR_TRUE;

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
  // Can't just QI for nsITextContent, because nsCommentNode
  // also implements that interface.
  nsCOMPtr<nsIContent> content (do_QueryInterface(aNode));
  if (!content) return PR_FALSE;
  nsCOMPtr<nsIAtom> atom;
  content->GetTag(*getter_AddRefs(atom));
  if (atom.get() == sTextAtom)
    return PR_TRUE;
  return PR_FALSE;
}

PRBool nsFind::SkipNode(nsIContent* aContent)
{
  nsCOMPtr<nsIAtom> atom;

#ifdef HAVE_BIDI_ITERATOR
  aContent->GetTag(*getter_AddRefs(atom));
  if (!atom)
    return PR_TRUE;
  nsIAtom *atomPtr = atom.get();

  return (sScriptAtom == atomPtr || sCommentAtom == atomPtr);

#else /* HAVE_BIDI_ITERATOR */
  // Temporary: eventually we will have an iterator to do this,
  // but for now, we have to climb up the tree for each node
  // and see whether any parent is a skipped node,
  // and take the performance hit.

  nsCOMPtr<nsIDOMNode> node (do_QueryInterface(aContent));
  while (node)
  {
    nsCOMPtr<nsIContent> content (do_QueryInterface(node));
    if (!content) return PR_FALSE;
    content->GetTag(*getter_AddRefs(atom));
    if (!atom)
      return PR_FALSE;
    nsAutoString atomName;
    atom->ToString(atomName);
    //printf("Atom name is %s\n", 
    //       NS_LossyConvertUCS2toASCII(atomName).get());
    nsIAtom *atomPtr = atom.get();
    if (atomPtr == sScriptAtom || atomPtr == sCommentAtom)
    {
#ifdef DEBUG_FIND
      printf("Skipping node: ");
      DumpNode(node);
#endif
      return PR_TRUE;
    }
    // Only climb to the nearest block node
    if (IsBlockNode(content))
      return PR_FALSE;

    nsCOMPtr<nsIDOMNode> parent;
    nsresult rv = node->GetParentNode(getter_AddRefs(parent));
    if (NS_FAILED(rv)) return PR_FALSE;

    node = parent;
  }
  return PR_FALSE;
#endif /* HAVE_BIDI_ITERATOR */
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

#define NBSP_CHARCODE (CHAR_TO_UNICHAR(160))
#define IsSpace(c) (nsCRT::IsAsciiSpace(c) || (c) == NBSP_CHARCODE)
#define OVERFLOW_PINDEX (mFindBackward ? pindex < 0 : pindex > aPatLen)
#define DONE_WITH_PINDEX (mFindBackward ? pindex <= 0 : pindex >= aPatLen)
#define ALMOST_DONE_WITH_PINDEX (mFindBackward ? pindex <= 0 : pindex >= aPatLen-1)

//
// FindInQ:
// If we have nodes in the queue, search them first.
// Then take nodes out of the tree with NextNode,
// until null (NextNode will return 0 at the end of our range).
// Save intermediates in the deque.
//
PRBool nsFind::FindInQ(const PRUnichar* aPatStr, PRInt32 aPatLen,
                       nsIDOMRange* aSearchRange,
                       nsIDOMRange* aStartPoint, nsIDOMRange* aEndPoint,
                       nsIDOMRange** aRangeRet)
{
#ifdef DEBUG_FIND
  printf("FindInQ for '%s'%s\n",
         NS_LossyConvertUCS2toASCII(aPatStr).get(),
         mFindBackward ? " (backward)" : " (forward)");
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

  // Have we extended a search past the endpoint?
  PRBool continuing = PR_FALSE;

  // Place to save the range start point in case we find a match:
  nsITextContent* matchAnchorTC = nsnull;
  PRInt32 matchAnchorOffset = 0;

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
        NextNode(aSearchRange, aStartPoint, aEndPoint, PR_FALSE);
        if (!mIterNode)    // Out of nodes
        {
          // Are we in the middle of a match?
          // If so, try again with continuation.
          if (matchAnchorTC && !continuing)
            NextNode(aSearchRange, aStartPoint, aEndPoint, PR_TRUE);

          if (!mIterNode)
          {
            // Reset the iterator, so this nsFind will be usable if
            // the user wants to search again (from beginning/end).
            mIterator = nsnull;
          }
          return PR_FALSE;
        }

        // mIterOffset, if set, is the range's idea of an offset,
        // and points between characters.  But when translated
        // to a string index, it points to a character.  If we're
        // going backward, this is one character too late and
        // we'll match part of our last pattern.
        offset = mIterOffset - (mFindBackward ? 1 : 0);

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
      if (offset >= 0)
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
        printf("Will need to pull a new node: restart=%d, frag len=%d\n",
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
    PRUnichar c = (t2b ? t2b[findex] : CHAR_TO_UNICHAR(t1b[findex]));
    PRUnichar patc = aPatStr[pindex];
#ifdef DEBUG_FIND
    printf("Comparing '%c'=%x to '%c' (%d of %d)%s\n",
           (char)c, (int)c, patc, pindex, aPatLen,
           inWhitespace ? " (inWhitespace)" : "");
#endif

    // Do we need to go back to non-whitespace mode?
    // If inWhitespace, then this space in the pat str
    // has already matched at least one space in the document.
    if (inWhitespace && !IsSpace(c))
    {
      inWhitespace = PR_FALSE;
      pindex += incr;
#ifdef DEBUG
      // This shouldn't happen -- if we were still matching, and we
      // were at the end of the pat string, then we should have
      // caught it in the last iteration and returned success.
      if (OVERFLOW_PINDEX)
        NS_ASSERTION(PR_FALSE, "Missed a whitespace match\n");
#endif
      patc = aPatStr[pindex];
    }
    if (!inWhitespace && IsSpace(patc))
      inWhitespace = PR_TRUE;

    // convert to lower case if necessary
    else if (!inWhitespace && !mCaseSensitive && IsUpperCase(c))
      c = ToLowerCase(c);

    // Compare
    if ((c == patc) || (inWhitespace && IsSpace(c)))
    {
#ifdef DEBUG_FIND
      if (inWhitespace)
        printf("YES (whitespace)(%d of %d)\n", pindex, aPatLen);
      else
        printf("YES! '%c' == '%c' (%d of %d)\n", c, patc, pindex, aPatLen);
#endif

      // Save the range anchors if we haven't already:
      if (!matchAnchorTC) {
        matchAnchorTC = tc;
        matchAnchorOffset = findex + (mFindBackward ? 1 : 0);
      }

      // Are we done?
      if (DONE_WITH_PINDEX)
        // Matched the whole string!
      {
#ifdef DEBUG_FIND
        printf("Found a match!\n");
#endif

        // Make the range:
        if (aRangeRet)
        {
          nsCOMPtr<nsIDOMRange> range (do_CreateInstance(kRangeCID));
          if (range)
          {
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

      // Advance and loop around for the next characters.
      // But don't advance from a space to a non-space:
      if (!inWhitespace || DONE_WITH_PINDEX || IsSpace(aPatStr[pindex+incr]))
      {
        pindex += incr;
        inWhitespace = PR_FALSE;
#ifdef DEBUG_FIND
        printf("Advancing pindex to %d\n", pindex);
#endif
      }
      
      continue;
    }

#ifdef DEBUG_FIND
    printf("NOT: %c == %c\n", c, patc);
#endif
    // If we were continuing, then this ends our search.
    if (continuing) {
      mIterator = nsnull;
      return PR_FALSE;
    }

    // If we didn't match, go back to the beginning of patStr,
    // and set findex back to the next char after
    // we started the current match.
    matchAnchorTC = nsnull;
    pindex = (mFindBackward ? aPatLen : 0);
    //findex = restart;  // +incr will be added when we continue
  } // end while loop

  // Out of nodes, and didn't match.
  return PR_FALSE;
}


