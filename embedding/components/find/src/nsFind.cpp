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
 * Portions created by the Initial Developer are Copyright (C) 2002
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
nsIAtom* nsFind::sTextAtom = nsnull;
nsIAtom* nsFind::sImgAtom = nsnull;
nsIAtom* nsFind::sHRAtom = nsnull;
nsIAtom* nsFind::sCommentAtom = nsnull;
nsIAtom* nsFind::sScriptAtom = nsnull;
nsIAtom* nsFind::sSelectAtom = nsnull;
nsIAtom* nsFind::sTextareaAtom = nsnull;

NS_IMPL_ISUPPORTS1(nsFind, nsIFind)

nsFind::nsFind()
  : mFindBackward(PR_FALSE)
  , mCaseSensitive(PR_FALSE)
  , mIterOffset(0)
{
  NS_INIT_ISUPPORTS();

  // Initialize the atoms if they aren't already:
  if (sInstanceCount <= 0)
  {
    sTextAtom = NS_NewAtom("__moz_text");
    sImgAtom = NS_NewAtom("img");
    sHRAtom = NS_NewAtom("hr");
    sCommentAtom = NS_NewAtom("__moz_comment");
    sScriptAtom = NS_NewAtom("script");
    sSelectAtom = NS_NewAtom("select");
    sTextareaAtom = NS_NewAtom("textarea");
  }
  ++sInstanceCount;
}

nsFind::~nsFind()
{
  if (sInstanceCount <= 1)
  {
    NS_IF_RELEASE(sTextAtom);
    NS_IF_RELEASE(sImgAtom);
    NS_IF_RELEASE(sHRAtom);
    NS_IF_RELEASE(sCommentAtom);
    NS_IF_RELEASE(sScriptAtom);
    NS_IF_RELEASE(sSelectAtom);
    NS_IF_RELEASE(sTextareaAtom);
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
// in the case of a partial match we may need to reset the
// iterator to go back to a previously visited node,
// so we always save the "match anchor" node and offset.
//
// Text nodes store their text in an nsTextFragment, which is 
// effectively a union of a one-byte string or a two-byte string.
// Single and double strings are intermixed in the dom.
// We don't have string classes which can deal with intermixed strings,
// so all the handling is done explicitly here.
//

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
        // XXX Needs work:
        // Problem with this approach: if there is a match which starts
        // just before the current selection and continues into the selection,
        // we will miss it, because our search algorithm only starts
        // searching from the end of the word, so we would have to
        // search the current selection but discount any matches
        // that fall entirely inside it.
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

    // If we ever cross a block node, we might want to reset
    // the match anchor:
    // we don't match patterns extending across block boundaries.
    // But we can't depend on this test here now, because the iterator
    // doesn't give us the parent going in and going out, and we
    // need it both times to depend on this.
    //if (IsBlockNode(content))

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
          //NS_ASSERTION(content, "Find: Node is not content\n");
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

  // We may not need to skip comment nodes,
  // now that IsTextNode distinguishes them from real text nodes.
  return (sScriptAtom == atomPtr || sCommentAtom == atomPtr
          || sSelectAtom == atomPtr || sTextareaAtom == atomPtr)

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
    if (atomPtr == sScriptAtom || atomPtr == sCommentAtom
        || sSelectAtom == atomPtr || sTextareaAtom == atomPtr)
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

// Call ResetAll before returning,
// to remove all references to external objects.
void nsFind::ResetAll()
{
  mIterator = nsnull;
  mLastBlockParent = nsnull;
}

#define NBSP_CHARCODE (CHAR_TO_UNICHAR(160))
#define IsSpace(c) (nsCRT::IsAsciiSpace(c) || (c) == NBSP_CHARCODE)
#define OVERFLOW_PINDEX (mFindBackward ? pindex < 0 : pindex > patLen)
#define DONE_WITH_PINDEX (mFindBackward ? pindex <= 0 : pindex >= patLen)
#define ALMOST_DONE_WITH_PINDEX (mFindBackward ? pindex <= 0 : pindex >= patLen-1)

//
// Find:
// Take nodes out of the tree with NextNode,
// until null (NextNode will return 0 at the end of our range).
//
NS_IMETHODIMP
nsFind::Find(const PRUnichar *aPatText, nsIDOMRange* aSearchRange,
             nsIDOMRange* aStartPoint, nsIDOMRange* aEndPoint,
             nsIDOMRange** aRangeRet)
{
#ifdef DEBUG_FIND
  printf("============== nsFind::Find('%s'%s, %p, %p, %p)\n",
         NS_LossyConvertUCS2toASCII(aPatText).get(),
         mFindBackward ? " (backward)" : " (forward)",
         (void*)aSearchRange, (void*)aStartPoint, (void*)aEndPoint);
#endif

  if (!aPatText)
    return NS_ERROR_NULL_POINTER;

  ResetAll();

  nsAutoString patAutoStr(aPatText);
  if (!mCaseSensitive)
    ToLowerCase(patAutoStr);
  const PRUnichar* patStr = patAutoStr.get();
  PRInt32 patLen = patAutoStr.Length() - 1;

  // The offset only matters for the first node,
  // so make a local copy so that we can zero it when we loop:
  PRInt32 offset = mIterOffset;

  // current offset into the pattern -- reset to beginning/end:
  PRInt32 pindex = (mFindBackward ? patLen : 0);

  // Current offset into the fragment
  PRInt32 findex = 0;

  // Place in the fragment to re-start the match:
  PRInt32 restart = 0;

  // Direction to move pindex and ptr*
  int incr = (mFindBackward ? -1 : 1);

  nsCOMPtr<nsITextContent> tc;
  const nsTextFragment *frag = nsnull;

  // Pointers into the current fragment:
  const PRUnichar *t2b = nsnull;
  const char      *t1b = nsnull;

  // Keep track of when we're in whitespace:
  // (only matters when we're matching)
  PRBool inWhitespace = PR_FALSE;

  // Have we extended a search past the endpoint?
  PRBool continuing = PR_FALSE;

  // Place to save the range start point in case we find a match:
  nsCOMPtr<nsIDOMNode> matchAnchorNode;
  PRInt32 matchAnchorOffset = 0;

  while (1)
  {
#ifdef DEBUG_FIND
    printf("Loop ...\n");
#endif

    // If this is our first time on a new node, reset the pointers:
    if (!frag)
    {

      tc = nsnull;
      NextNode(aSearchRange, aStartPoint, aEndPoint, PR_FALSE);
      if (!mIterNode)    // Out of nodes
      {
        // Are we in the middle of a match?
        // If so, try again with continuation.
        if (matchAnchorNode && !continuing)
          NextNode(aSearchRange, aStartPoint, aEndPoint, PR_TRUE);

        // Reset the iterator, so this nsFind will be usable if
        // the user wants to search again (from beginning/end).
        ResetAll();
        return PR_FALSE;
      }

      offset = mIterOffset;

      // We have a new text content.  If its block parent is different
      // from the block parent of the last text content, then we
      // need to clear the match since we don't want to find
      // across block boundaries.
      nsCOMPtr<nsIDOMNode> blockParent;
      GetBlockParent(mIterNode, getter_AddRefs(blockParent));
#ifdef DEBUG_FIND
      printf("New node: old blockparent = %p, new = %p\n",
             (void*)mLastBlockParent.get(), (void*)blockParent.get());
#endif
      if (blockParent != mLastBlockParent)
      {
#ifdef DEBUG_FIND
        printf("Different block parent!\n");
#endif
        mLastBlockParent = blockParent;
        // End any pending match:
        matchAnchorNode = nsnull;
        pindex = (mFindBackward ? patLen : 0);
      }
 
     // Get the text content:
      tc = do_QueryInterface(mIterNode);
      if (!tc)         // Out of nodes
      {
        mIterator = nsnull;
        mLastBlockParent = 0;
        return PR_FALSE;
      }

      nsresult rv = tc->GetText(&frag);
      if (NS_FAILED(rv)) continue;
      // mIterOffset, if set, is the range's idea of an offset,
      // and points between characters.  But when translated
      // to a string index, it points to a character.  If we're
      // going backward, this is one character too late and
      // we'll match part of our previous pattern.
      if (offset >= 0)
        restart = offset - (mFindBackward ? 1 : 0);
      else if (mFindBackward)
        restart = frag->GetLength()-1;
      else
        restart = 0;
      // If this is outside the bounds of the string, then skip this node:
      if (restart < 0 || restart > frag->GetLength()-1)
      {
#ifdef DEBUG_FIND
        printf("At the end of a text node -- skipping to the next\n");
#endif
        frag = 0;
        continue;
      }
      findex = restart;
#ifdef DEBUG_FIND
      printf("Starting from offset %d\n", findex);
#endif
      if (frag->Is2b())
      {
        t2b = frag->Get2b();
        t1b = nsnull;
#ifdef DEBUG_FIND
        nsAutoString str2(t2b, frag->GetLength());
        printf("2 byte, '%s'\n", NS_LossyConvertUCS2toASCII(str2).get());
#endif
      }
      else
      {
        t1b = frag->Get1b();
        t2b = nsnull;
#ifdef DEBUG_FIND
        nsCAutoString str1(t1b, frag->GetLength());
        printf("1 byte, '%s'\n", str1.get());
#endif
      }
    }
    else // still on the old node
    {
      // Still on the old node.  Advance the pointers,
      // then see if we need to pull a new node.
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
        frag = nsnull;
        // Offset can only apply to the first node:
        offset = -1;
        continue;
      }
    }

    // The two characters we'll be comparing:
    PRUnichar c = (t2b ? t2b[findex] : CHAR_TO_UNICHAR(t1b[findex]));
    PRUnichar patc = patStr[pindex];
#ifdef DEBUG_FIND
    printf("Comparing '%c'=%x to '%c' (%d of %d), findex=%d%s\n",
           (char)c, (int)c, patc, pindex, patLen, findex,
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
      patc = patStr[pindex];
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
        printf("YES (whitespace)(%d of %d)\n", pindex, patLen);
      else
        printf("YES! '%c' == '%c' (%d of %d)\n", c, patc, pindex, patLen);
#endif

      // Save the range anchors if we haven't already:
      if (!matchAnchorNode) {
        matchAnchorNode = mIterNode;
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
                endParent = matchAnchorNode;
                matchStartOffset = findex;
                matchEndOffset = matchAnchorOffset;
              }
              else
              {
                startParent = matchAnchorNode;
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

        // Reset the offset to the other end of the found string:
        mIterOffset = findex + (mFindBackward ? 1 : 0);
#ifdef DEBUG_FIND
        printf("mIterOffset = %d, mIterNode = ", mIterOffset);
        DumpNode(mIterNode);
#endif

        ResetAll();
        return PR_TRUE;
      }

      // Not done, but still matching.

      // Advance and loop around for the next characters.
      // But don't advance from a space to a non-space:
      if (!inWhitespace || DONE_WITH_PINDEX || IsSpace(patStr[pindex+incr]))
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
      ResetAll();
      return PR_FALSE;
    }

    // If we didn't match, go back to the beginning of patStr,
    // and set findex back to the next char after
    // we started the current match.
    if (matchAnchorNode)    // we're ending a partial match
    {
      findex = restart;   // +incr will be added to findex when we continue
      nsCOMPtr<nsIContent> content (do_QueryInterface(matchAnchorNode));
      nsresult rv = NS_ERROR_UNEXPECTED;
      NS_ASSERTION(content, "Text content isn't nsIContent!");
      if (content)
        rv = mIterator->PositionAt(content);
      // Should check return value -- but what would we do if it failed??
      // We're in big trouble if that happens.
      NS_ASSERTION(NS_SUCCEEDED(rv), "Text content wasn't nsIContent!");
    }
    restart = findex + incr;
    matchAnchorNode = nsnull;
    inWhitespace = PR_FALSE;
    pindex = (mFindBackward ? patLen : 0);
#ifdef DEBUG_FIND
    printf("Setting restart back to %d, findex back to %d, pindex to %d\n",
           restart, findex, pindex);
#endif
  } // end while loop

  // Out of nodes, and didn't match.
  ResetAll();
  return PR_FALSE;
}


