/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; 
c-basic-offset: 2 -*- */
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
 *   Roger B. Sidje <rbs@maths.uq.edu.au> (added find in <textarea> & text <input>)
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
#include "nsIDOMNode.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMDocumentRange.h"
#include "nsIDOMDocumentTraversal.h"
#include "nsISelection.h"
#include "nsISelectionController.h"
#include "nsIPresShell.h"
#include "nsIFrame.h"
#include "nsITextControlFrame.h"
#include "nsIFormControl.h"
#include "nsIEditor.h"
#include "nsIPlaintextEditor.h"
#include "nsIDocument.h"
#include "nsTextFragment.h"
#include "nsString.h"
#include "nsIAtom.h"
#include "nsParserCIID.h"
#include "nsServiceManagerUtils.h"
#include "nsUnicharUtils.h"
#include "nsIDOMElement.h"
#include "nsIWordBreaker.h"
#include "nsCRT.h"

// Yikes!  Casting a char to unichar can fill with ones!
#define CHAR_TO_UNICHAR(c) ((PRUnichar)(const unsigned char)c)

static NS_DEFINE_CID(kCContentIteratorCID, NS_CONTENTITERATOR_CID);
static NS_DEFINE_CID(kCPreContentIteratorCID, NS_PRECONTENTITERATOR_CID);
static NS_DEFINE_IID(kRangeCID, NS_RANGE_CID);

// -----------------------------------------------------------------------
// nsFindContentIterator is a special iterator that also goes through
// any existing <textarea>'s or text <input>'s editor to lookup the
// anonymous DOM content there.
//
// Details:
// 1) We use two iterators: The "outer-iterator" goes through the
// normal DOM. The "inner-iterator" goes through the anonymous DOM
// inside the editor.
//
// 2) [MaybeSetupInnerIterator] As soon as the outer-iterator's current
// node is changed, a check is made to see if the node is a <textarea> or
// a text <input> node. If so, an inner-iterator is created to lookup the
// anynomous contents of the editor underneath the text control.
//
// 3) When the inner-iterator is created, we position the outer-iterator
// 'after' (or 'before' in backward search) the text control to avoid
// revisiting that control.
//
// 4) As a consequence of searching through text controls, we can be
// called via FindNext with the current selection inside a <textarea>
// or a text <input>. This means that we can be given an initial search
// range that stretches across the anonymous DOM and the normal DOM. To
// cater for this situation, we split the anonymous part into the 
// inner-iterator and then reposition the outer-iterator outside.
//
// 5) The implementation assumes that First() and Next() are only called
// in find-forward mode, while Last() and Prev() are used in find-backward.

class nsFindContentIterator : public nsIContentIterator
{
public:
  nsFindContentIterator(PRBool aFindBackward)
    : mOuterIterator(nsnull)
    , mInnerIterator(nsnull)
    , mRange(nsnull)
    , mStartOuterNode(nsnull)
    , mEndOuterNode(nsnull)
    , mFindBackward(aFindBackward)
  {
  }

  virtual ~nsFindContentIterator()
  {
  }

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIContentIterator
  virtual nsresult Init(nsIContent* aRoot)
  {
    NS_NOTREACHED("internal error");
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  virtual nsresult Init(nsIDOMRange* aRange);
  virtual void First();
  virtual void Last();
  virtual void Next();
  virtual void Prev();
  virtual nsIContent* GetCurrentNode();
  virtual PRBool IsDone();
  virtual nsresult PositionAt(nsIContent* aCurNode);

private:
  nsCOMPtr<nsIContentIterator> mOuterIterator;
  nsCOMPtr<nsIContentIterator> mInnerIterator;
  nsCOMPtr<nsIDOMRange> mRange;
  nsCOMPtr<nsIDOMNode> mStartOuterNode;
  nsCOMPtr<nsIDOMNode> mEndOuterNode;
  PRBool mFindBackward;

  void Reset();
  void MaybeSetupInnerIterator();
  void SetupInnerIterator(nsIContent* aContent);
};

NS_IMPL_ISUPPORTS1(nsFindContentIterator, nsIContentIterator)

nsresult
nsFindContentIterator::Init(nsIDOMRange* aRange)
{
  if (!mOuterIterator) {
    if (mFindBackward) {
      // Use post-order in the reverse case, so we get parents
      // before children in case we want to prevent descending
      // into a node.
      mOuterIterator = do_CreateInstance(kCContentIteratorCID);
    }
    else {
      // Use pre-order in the forward case, so we get parents
      // before children in case we want to prevent descending
      // into a node.
      mOuterIterator = do_CreateInstance(kCPreContentIteratorCID);
    }
    NS_ENSURE_ARG_POINTER(mOuterIterator);
  }

  // mRange is the search range that we will examine
  return aRange->CloneRange(getter_AddRefs(mRange));
}

void
nsFindContentIterator::First()
{
  Reset();
}

void
nsFindContentIterator::Last()
{
  Reset();
}

void
nsFindContentIterator::Next()
{
  if (mInnerIterator) {
    mInnerIterator->Next();
    if (!mInnerIterator->IsDone())
      return;

    // by construction, mOuterIterator is already on the next node
  }
  else {
    mOuterIterator->Next();
  }
  MaybeSetupInnerIterator();  
}

void
nsFindContentIterator::Prev()
{
  if (mInnerIterator) {
    mInnerIterator->Prev();
    if (!mInnerIterator->IsDone())
      return;

    // by construction, mOuterIterator is already on the previous node
  }
  else {
    mOuterIterator->Prev();
  }
  MaybeSetupInnerIterator();
}

nsIContent*
nsFindContentIterator::GetCurrentNode()
{
  if (mInnerIterator && !mInnerIterator->IsDone()) {
    return mInnerIterator->GetCurrentNode();
  }
  return mOuterIterator->GetCurrentNode();
}

PRBool
nsFindContentIterator::IsDone() {
  if (mInnerIterator && !mInnerIterator->IsDone()) {
    return PR_FALSE;
  }
  return mOuterIterator->IsDone();
}

nsresult
nsFindContentIterator::PositionAt(nsIContent* aCurNode)
{
  nsIContent* oldNode = mOuterIterator->GetCurrentNode();
  nsresult rv = mOuterIterator->PositionAt(aCurNode);
  if (NS_SUCCEEDED(rv)) {
    MaybeSetupInnerIterator();
  }
  else {
    mOuterIterator->PositionAt(oldNode);
    if (mInnerIterator)
      rv = mInnerIterator->PositionAt(aCurNode);
  }
  return rv;
}

void
nsFindContentIterator::Reset()
{
  mInnerIterator = nsnull;
  mStartOuterNode = nsnull;
  mEndOuterNode = nsnull;

  // As a consequence of searching through text controls, we may have been
  // initialized with a selection inside a <textarea> or a text <input>.

  // see if the start node is an anonymous text node inside a text control
  nsCOMPtr<nsIDOMNode> startNode;
  mRange->GetStartContainer(getter_AddRefs(startNode));
  nsCOMPtr<nsIContent> startContent(do_QueryInterface(startNode));
  for ( ; startContent; startContent = startContent->GetParent()) {
    if (!startContent->IsNativeAnonymous()) {
      mStartOuterNode = do_QueryInterface(startContent);
      break;
    }
  }

  // see if the end node is an anonymous text node inside a text control
  nsCOMPtr<nsIDOMNode> endNode;
  mRange->GetEndContainer(getter_AddRefs(endNode));
  nsCOMPtr<nsIContent> endContent(do_QueryInterface(endNode));
  for ( ; endContent; endContent = endContent->GetParent()) {
    if (!endContent->IsNativeAnonymous()) {
      mEndOuterNode = do_QueryInterface(endContent);
      break;
    }
  }

  mOuterIterator->Init(mRange);

  if (!mFindBackward) {
    if (mStartOuterNode != startNode) {
      // the start node was an anonymous text node
      SetupInnerIterator(startContent);
      if (mInnerIterator)
        mInnerIterator->First();
    }
    mOuterIterator->First();
  }
  else {
    if (mEndOuterNode != endNode) {
      // the end node was an anonymous text node
      SetupInnerIterator(endContent);
      if (mInnerIterator)
        mInnerIterator->Last();
    }
    mOuterIterator->Last();
  }

  // if we didn't create an inner-iterator, the boundary node could still be
  // a text control, in which case we also need an inner-iterator straightaway
  if (!mInnerIterator) {
    MaybeSetupInnerIterator();
  }
}

void
nsFindContentIterator::MaybeSetupInnerIterator()
{
  mInnerIterator = nsnull;

  nsIContent* content = mOuterIterator->GetCurrentNode();
  if (!content || !content->IsNodeOfType(nsINode::eHTML_FORM_CONTROL))
    return;

  nsCOMPtr<nsIFormControl> formControl(do_QueryInterface(content));
  PRInt32 controlType = formControl->GetType();
  if (controlType != NS_FORM_TEXTAREA && 
      controlType != NS_FORM_INPUT_TEXT)
    return;

  SetupInnerIterator(content);
  if (mInnerIterator) {
    if (!mFindBackward) {
      mInnerIterator->First();
      // finish setup: position mOuterIterator on the actual "next"
      // node (this completes its re-init, @see SetupInnerIterator)
      mOuterIterator->First();
    }
    else {
      mInnerIterator->Last();
      // finish setup: position mOuterIterator on the actual "previous"
      // node (this completes its re-init, @see SetupInnerIterator)
      mOuterIterator->Last();
    }
  }
}

void
nsFindContentIterator::SetupInnerIterator(nsIContent* aContent)
{
  NS_ASSERTION(aContent && !aContent->IsNativeAnonymous(), "invalid call");

  nsIDocument* doc = aContent->GetDocument();
  nsIPresShell* shell = doc ? doc->GetShellAt(0) : nsnull;
  if (!shell)
    return;

  nsIFrame* frame = shell->GetPrimaryFrameFor(aContent);
  if (!frame)
    return;

  nsITextControlFrame* tcFrame = nsnull;
  CallQueryInterface(frame, &tcFrame);
  if (!tcFrame)
    return;

  nsCOMPtr<nsIEditor> editor;
  tcFrame->GetEditor(getter_AddRefs(editor));
  if (!editor)
    return;

  // don't mess with disabled input fields
  PRUint32 editorFlags = 0;
  editor->GetFlags(&editorFlags);
  if (editorFlags & nsIPlaintextEditor::eEditorDisabledMask)
    return;

  nsCOMPtr<nsIDOMElement> rootElement;
  editor->GetRootElement(getter_AddRefs(rootElement));
  nsCOMPtr<nsIContent> rootContent(do_QueryInterface(rootElement));

  // now create the inner-iterator
  mInnerIterator = do_CreateInstance(kCPreContentIteratorCID);

  if (mInnerIterator) {
    nsCOMPtr<nsIDOMNode> node(do_QueryInterface(rootContent));
    nsCOMPtr<nsIDOMRange> range(do_CreateInstance(kRangeCID));
    range->SelectNodeContents(node);

    // fix up the inner bounds, we may have to only lookup a portion
    // of the text control if the current node is a boundary point
    PRInt32 offset;
    nsCOMPtr<nsIDOMNode> outerNode(do_QueryInterface(aContent));
    if (outerNode == mStartOuterNode) {
      mRange->GetStartOffset(&offset);
      mRange->GetStartContainer(getter_AddRefs(node));
      range->SetStart(node, offset);
    }
    if (outerNode == mEndOuterNode) {
      mRange->GetEndOffset(&offset);
      mRange->GetEndContainer(getter_AddRefs(node));
      range->SetEnd(node, offset);
    }
    // Note: we just init here. We do First() or Last() later. 
    mInnerIterator->Init(range);

    // make sure to place the outer-iterator outside
    // the text control so that we don't go there again.
    nsresult res;
    mRange->CloneRange(getter_AddRefs(range));
    if (!mFindBackward) { // find forward
      // cut the outer-iterator after the current node
      res = range->SetStartAfter(outerNode);
    }
    else { // find backward
      // cut the outer-iterator before the current node
      res = range->SetEndBefore(outerNode);
    }
    if (NS_FAILED(res)) {
      // we are done with the outer-iterator, the 
      // inner-iterator will traverse what we want
      range->Collapse(PR_TRUE);
    }
    // Note: we just re-init here, using the segment of mRange that is
    // yet to be visited. Thus when we later do mOuterIterator->First() 
    // [or mOuterIterator->Last()], we will effectively be on the next
    // node [or the previous node] _with respect to_ mRange. 
    mOuterIterator->Init(range);
  }
}

nsresult
NS_NewFindContentIterator(PRBool aFindBackward,
                          nsIContentIterator** aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  if (!aResult) {
    return NS_ERROR_NULL_POINTER;
  }

  nsFindContentIterator* it = new nsFindContentIterator(aFindBackward);
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(NS_GET_IID(nsIContentIterator), (void **)aResult);
}
// --------------------------------------------------------------------

// Sure would be nice if we could just get these from somewhere else!
PRInt32 nsFind::sInstanceCount = 0;
nsIAtom* nsFind::sImgAtom = nsnull;
nsIAtom* nsFind::sHRAtom = nsnull;
nsIAtom* nsFind::sScriptAtom = nsnull;
nsIAtom* nsFind::sNoframesAtom = nsnull;
nsIAtom* nsFind::sSelectAtom = nsnull;
nsIAtom* nsFind::sTextareaAtom = nsnull;
nsIAtom* nsFind::sThAtom = nsnull;
nsIAtom* nsFind::sTdAtom = nsnull;

NS_IMPL_ISUPPORTS1(nsFind, nsIFind)

nsFind::nsFind()
  : mFindBackward(PR_FALSE)
  , mCaseSensitive(PR_FALSE)
  , mIterOffset(0)
{
  // Initialize the atoms if they aren't already:
  if (sInstanceCount <= 0)
  {
    sImgAtom = NS_NewAtom("img");
    sHRAtom = NS_NewAtom("hr");
    sScriptAtom = NS_NewAtom("script");
    sNoframesAtom = NS_NewAtom("noframes");
    sSelectAtom = NS_NewAtom("select");
    sTextareaAtom = NS_NewAtom("textarea");
    sThAtom = NS_NewAtom("th");
    sTdAtom = NS_NewAtom("td");
  }
  ++sInstanceCount;
}

nsFind::~nsFind()
{
  if (sInstanceCount <= 1)
  {
    NS_IF_RELEASE(sImgAtom);
    NS_IF_RELEASE(sHRAtom);
    NS_IF_RELEASE(sScriptAtom);
    NS_IF_RELEASE(sNoframesAtom);
    NS_IF_RELEASE(sSelectAtom);
    NS_IF_RELEASE(sTextareaAtom);
    NS_IF_RELEASE(sThAtom);
    NS_IF_RELEASE(sTdAtom);
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
  nsCOMPtr<nsIContent> textContent (do_QueryInterface(aNode));
  if (textContent && textContent->IsNodeOfType(nsINode::eTEXT))
  {
    nsAutoString newText;
    textContent->AppendTextTo(newText);
    printf(">>>> Text node (node name %s): '%s'\n",
           NS_LossyConvertUTF16toASCII(nodeName).get(),
           NS_LossyConvertUTF16toASCII(newText).get());
  }
  else
    printf(">>>> Node: %s\n", NS_LossyConvertUTF16toASCII(nodeName).get());
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
    rv = NS_NewFindContentIterator(mFindBackward, getter_AddRefs(mIterator));
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_ARG_POINTER(mIterator);
  }

  NS_ENSURE_ARG_POINTER(aSearchRange);

#ifdef DEBUG_FIND
  printf("InitIterator search range:\n"); DumpRange(aSearchRange);
#endif

  rv = mIterator->Init(aSearchRange);
  NS_ENSURE_SUCCESS(rv, rv);
  if (mFindBackward) {
    mIterator->Last();
  }
  else {
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
  *aWordBreaker = mWordBreaker;
  NS_IF_ADDREF(*aWordBreaker);
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

  nsIContent *content = nsnull;

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

    content = mIterator->GetCurrentNode();
#ifdef DEBUG_FIND
    nsCOMPtr<nsIDOMNode> dnode (do_QueryInterface(content));
    printf(":::::: Got the first node "); DumpNode(dnode);
#endif
    if (content && content->IsNodeOfType(nsINode::eTEXT) &&
        !SkipNode(content))
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
      mIterator->Prev();
    else
      mIterator->Next();

    content = mIterator->GetCurrentNode();
    if (!content)
      break;

#ifdef DEBUG_FIND
    nsCOMPtr<nsIDOMNode> dnode (do_QueryInterface(content));
    printf(":::::: Got another node "); DumpNode(dnode);
#endif

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
    if (SkipNode(content))
      continue;

    if (content->IsNodeOfType(nsINode::eTEXT))
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
  if (!aContent->IsNodeOfType(nsINode::eHTML)) {
    return PR_FALSE;
  }

  nsIAtom *atom = aContent->Tag();

  if (atom == sImgAtom ||
      atom == sHRAtom ||
      atom == sThAtom ||
      atom == sTdAtom)
    return PR_TRUE;

  if (!mParserService) {
    mParserService = do_GetService(NS_PARSERSERVICE_CONTRACTID);
    if (!mParserService)
      return PR_FALSE;
  }

  PRBool isBlock = PR_FALSE;
  mParserService->IsBlock(mParserService->HTMLAtomTagToId(atom), isBlock);
  return isBlock;
}

PRBool nsFind::IsTextNode(nsIDOMNode* aNode)
{
  PRUint16 nodeType;
  aNode->GetNodeType(&nodeType);

  return nodeType == nsIDOMNode::TEXT_NODE ||
         nodeType == nsIDOMNode::CDATA_SECTION_NODE;
}

PRBool nsFind::IsVisibleNode(nsIDOMNode *aDOMNode)
{
  nsCOMPtr<nsIContent> content(do_QueryInterface(aDOMNode));
  if (!content)
    return PR_FALSE;

  nsCOMPtr<nsIDocument> doc = content->GetDocument();
  if (!doc)
    return PR_FALSE;

  nsIPresShell *presShell = doc->GetShellAt(0);
  if (!presShell)
    return PR_FALSE;

  nsIFrame *frame = presShell->GetPrimaryFrameFor(content);
  if (!frame) {
    // No frame! Not visible then.
    return PR_FALSE;
  }

  return frame->GetStyleVisibility()->IsVisible();
}

PRBool nsFind::SkipNode(nsIContent* aContent)
{
  nsIAtom *atom;

#ifdef HAVE_BIDI_ITERATOR
  atom = aContent->Tag();

  // We may not need to skip comment nodes,
  // now that IsTextNode distinguishes them from real text nodes.
  return (aContent->IsNodeOfType(nsINode::eCOMMENT) ||
          (aContent->IsNodeOfType(nsINode::eHTML) &&
           (atom == sScriptAtom ||
            atom == sNoframesAtom ||
            atom == sSelectAtom)));

#else /* HAVE_BIDI_ITERATOR */
  // Temporary: eventually we will have an iterator to do this,
  // but for now, we have to climb up the tree for each node
  // and see whether any parent is a skipped node,
  // and take the performance hit.

  nsIContent *content = aContent;
  while (content)
  {
    atom = content->Tag();

    if (aContent->IsNodeOfType(nsINode::eCOMMENT) ||
        (content->IsNodeOfType(nsINode::eHTML) &&
         (atom == sScriptAtom ||
          atom == sNoframesAtom ||
          atom == sSelectAtom)))
    {
#ifdef DEBUG_FIND
      printf("Skipping node: ");
      nsCOMPtr<nsIDOMNode> node (do_QueryInterface(content));
      DumpNode(node);
#endif

      return PR_TRUE;
    }

    // Only climb to the nearest block node
    if (IsBlockNode(content))
      return PR_FALSE;

    content = content->GetParent();
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
         NS_LossyConvertUTF16toASCII(aPatText).get(),
         mFindBackward ? " (backward)" : " (forward)",
         (void*)aSearchRange, (void*)aStartPoint, (void*)aEndPoint);
#endif

  NS_ENSURE_ARG_POINTER(aRangeRet);
  *aRangeRet = 0;

  if (!aPatText)
    return NS_ERROR_NULL_POINTER;

  ResetAll();

  nsAutoString patAutoStr(aPatText);
  if (!mCaseSensitive)
    ToLowerCase(patAutoStr);
  const PRUnichar* patStr = patAutoStr.get();
  PRInt32 patLen = patAutoStr.Length() - 1;

  // current offset into the pattern -- reset to beginning/end:
  PRInt32 pindex = (mFindBackward ? patLen : 0);

  // Current offset into the fragment
  PRInt32 findex = 0;

  // Direction to move pindex and ptr*
  int incr = (mFindBackward ? -1 : 1);

  nsCOMPtr<nsIContent> tc;
  const nsTextFragment *frag = nsnull;
  PRInt32 fragLen = 0;

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

  // Get the end point, so we know when to end searches:
  nsCOMPtr<nsIDOMNode> endNode;
  PRInt32 endOffset;
  aEndPoint->GetEndContainer(getter_AddRefs(endNode));
  aEndPoint->GetEndOffset(&endOffset);

  PRUnichar prevChar = 0;
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
        return NS_OK;
      }

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
        matchAnchorOffset = 0;
        pindex = (mFindBackward ? patLen : 0);
        inWhitespace = PR_FALSE;
      }
 
      // Get the text content:
      tc = do_QueryInterface(mIterNode);
      if (!tc || !(frag = tc->GetText())) // Out of nodes
      {
        mIterator = nsnull;
        mLastBlockParent = 0;
        ResetAll();
        return NS_OK;
      }

      fragLen = frag->GetLength();

      // Set our starting point in this node.
      // If we're going back to the anchor node, which means that we
      // just ended a partial match, use the saved offset:
      if (mIterNode == matchAnchorNode)
        findex = matchAnchorOffset + (mFindBackward ? 1 : 0);

      // mIterOffset, if set, is the range's idea of an offset,
      // and points between characters.  But when translated
      // to a string index, it points to a character.  If we're
      // going backward, this is one character too late and
      // we'll match part of our previous pattern.
      else if (mIterOffset >= 0)
        findex = mIterOffset - (mFindBackward ? 1 : 0);

      // Otherwise, just start at the appropriate end of the fragment:
      else if (mFindBackward)
        findex = fragLen - 1;
      else
        findex = 0;

      // Offset can only apply to the first node:
      mIterOffset = -1;

      // If this is outside the bounds of the string, then skip this node:
      if (findex < 0 || findex > fragLen-1)
      {
#ifdef DEBUG_FIND
        printf("At the end of a text node -- skipping to the next\n");
#endif
        frag = 0;
        continue;
      }

#ifdef DEBUG_FIND
      printf("Starting from offset %d\n", findex);
#endif
      if (frag->Is2b())
      {
        t2b = frag->Get2b();
        t1b = nsnull;
#ifdef DEBUG_FIND
        nsAutoString str2(t2b, fragLen);
        printf("2 byte, '%s'\n", NS_LossyConvertUTF16toASCII(str2).get());
#endif
      }
      else
      {
        t1b = frag->Get1b();
        t2b = nsnull;
#ifdef DEBUG_FIND
        nsCAutoString str1(t1b, fragLen);
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
      if (mFindBackward ? (findex < 0) : (findex >= fragLen))
      {
#ifdef DEBUG_FIND
        printf("Will need to pull a new node: mAO = %d, frag len=%d\n",
               matchAnchorOffset, fragLen);
#endif
        // Done with this node.  Pull a new one.
        frag = nsnull;
        continue;
      }
    }

    // Have we gone past the endpoint yet?
    // If we have, and we're not in the middle of a match, return.
    if (mIterNode == endNode && !continuing &&
        ((mFindBackward && (findex < endOffset)) ||
         (!mFindBackward && (findex > endOffset))))
    {
      ResetAll();
      return NS_OK;
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

    // a '\n' between CJ characters is ignored
    if (pindex != (mFindBackward ? patLen : 0) && c != patc && !inWhitespace) {
      if (c == '\n' && t2b && IS_CJ_CHAR(prevChar)) {
        PRInt32 nindex = findex + incr;
        if (mFindBackward ? (nindex >= 0) : (nindex < fragLen)) {
          if (IS_CJ_CHAR(t2b[nindex]))
            continue;
        }
      }
    }

    // Compare
    if ((c == patc) || (inWhitespace && IsSpace(c)))
    {
      prevChar = c;
#ifdef DEBUG_FIND
      if (inWhitespace)
        printf("YES (whitespace)(%d of %d)\n", pindex, patLen);
      else
        printf("YES! '%c' == '%c' (%d of %d)\n", c, patc, pindex, patLen);
#endif

      // Save the range anchors if we haven't already:
      if (!matchAnchorNode) {
        matchAnchorNode = mIterNode;
        matchAnchorOffset = findex;
      }

      // Are we done?
      if (DONE_WITH_PINDEX)
        // Matched the whole string!
      {
#ifdef DEBUG_FIND
        printf("Found a match!\n");
#endif

        // Make the range:
        nsCOMPtr<nsIDOMNode> startParent;
        nsCOMPtr<nsIDOMNode> endParent;
        nsCOMPtr<nsIDOMRange> range (do_CreateInstance(kRangeCID));
        if (range)
        {
          PRInt32 matchStartOffset, matchEndOffset;
          // convert char index to range point:
          PRInt32 mao = matchAnchorOffset + (mFindBackward ? 1 : 0);
          if (mFindBackward)
          {
            startParent = do_QueryInterface(tc);
            endParent = matchAnchorNode;
            matchStartOffset = findex;
            matchEndOffset = mao;
          }
          else
          {
            startParent = matchAnchorNode;
            endParent = do_QueryInterface(tc);
            matchStartOffset = mao;
            matchEndOffset = findex+1;
          }
          if (startParent && endParent && 
              IsVisibleNode(startParent) && IsVisibleNode(endParent))
          {
            range->SetStart(startParent, matchStartOffset);
            range->SetEnd(endParent, matchEndOffset);
            *aRangeRet = range.get();
            NS_ADDREF(*aRangeRet);
          }
          else {
            startParent = nsnull; // This match is no good -- invisible or bad range
          }
        }

        if (startParent) {
          // If startParent == nsnull, we didn't successfully make range
          // or, we didn't make a range because the start or end node were invisible
          // Reset the offset to the other end of the found string:
          mIterOffset = findex + (mFindBackward ? 1 : 0);
  #ifdef DEBUG_FIND
          printf("mIterOffset = %d, mIterNode = ", mIterOffset);
          DumpNode(mIterNode);
  #endif

          ResetAll();
          return NS_OK;
        }
        matchAnchorNode = nsnull;  // This match is no good, continue on in document
      }

      if (matchAnchorNode) {
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
    }

#ifdef DEBUG_FIND
    printf("NOT: %c == %c\n", c, patc);
#endif
    // If we were continuing, then this ends our search.
    if (continuing) {
      ResetAll();
      return NS_OK;
    }

    // If we didn't match, go back to the beginning of patStr,
    // and set findex back to the next char after
    // we started the current match.
    if (matchAnchorNode)    // we're ending a partial match
    {
      findex = matchAnchorOffset;
      mIterOffset = matchAnchorOffset;
          // +incr will be added to findex when we continue

      // Are we going back to a previous node?
      if (matchAnchorNode != mIterNode)
      {
        nsCOMPtr<nsIContent> content (do_QueryInterface(matchAnchorNode));
        nsresult rv = NS_ERROR_UNEXPECTED;
        if (content)
          rv = mIterator->PositionAt(content);
        frag = 0;
        NS_ASSERTION(NS_SUCCEEDED(rv), "Text content wasn't nsIContent!");
#ifdef DEBUG_FIND
        printf("Repositioned anchor node\n");
#endif
      }
#ifdef DEBUG_FIND
      printf("Ending a partial match; findex -> %d, mIterOffset -> %d\n",
             findex, mIterOffset);
#endif
    }
    matchAnchorNode = nsnull;
    matchAnchorOffset = 0;
    inWhitespace = PR_FALSE;
    pindex = (mFindBackward ? patLen : 0);
#ifdef DEBUG_FIND
    printf("Setting findex back to %d, pindex to %d\n", findex, pindex);
           
#endif
  } // end while loop

  // Out of nodes, and didn't match.
  ResetAll();
  return NS_OK;
}


