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

/*
 * nsContentIterator.cpp: Implementation of the nsContentIterator object.
 * This ite
 */
#include "nsISupports.h"
#include "nsIDOMNodeList.h"
#include "nsIContentIterator.h"
#include "nsRange.h"
#include "nsIContent.h"
#include "nsIDOMText.h"
#include "nsISupportsArray.h"
#include "nsCOMPtr.h"
#include "nsPresContext.h"
#include "nsIComponentManager.h"
#include "nsContentCID.h"
#include "nsIPresShell.h"

#define DO_AFTER 1
#define DO_BEFORE 1

///////////////////////////////////////////////////////////////////////////
// GetNumChildren: returns the number of things inside aNode.
//
static PRUint32
GetNumChildren(nsIDOMNode *aNode)
{
  PRUint32 numChildren = 0;
  if (!aNode)
    return 0;

  PRBool hasChildNodes;
  aNode->HasChildNodes(&hasChildNodes);
  if (hasChildNodes)
  {
    nsCOMPtr<nsIDOMNodeList>nodeList;
    nsresult res = aNode->GetChildNodes(getter_AddRefs(nodeList));
    if (NS_SUCCEEDED(res) && nodeList)
      nodeList->GetLength(&numChildren);
  }
  return numChildren;
}

///////////////////////////////////////////////////////////////////////////
// GetChildAt: returns the node at this position index in the parent
//
static nsCOMPtr<nsIDOMNode>
GetChildAt(nsIDOMNode *aParent, PRInt32 aOffset)
{
  nsCOMPtr<nsIDOMNode> resultNode;

  if (!aParent)
    return resultNode;

  PRBool hasChildNodes;
  aParent->HasChildNodes(&hasChildNodes);
  if (PR_TRUE==hasChildNodes)
  {
    nsCOMPtr<nsIDOMNodeList>nodeList;
    nsresult res = aParent->GetChildNodes(getter_AddRefs(nodeList));
    if (NS_SUCCEEDED(res) && nodeList)
      nodeList->Item(aOffset, getter_AddRefs(resultNode));
  }

  return resultNode;
}


/*
 *  A simple iterator class for traversing the generated content in "close tag" order
 */
class nsGeneratedContentIterator : public nsIContentIterator,
                                   public nsIGeneratedContentIterator
{
public:
  NS_DECL_ISUPPORTS

  nsGeneratedContentIterator();
  virtual ~nsGeneratedContentIterator();

  // nsIContentIterator interface methods ------------------------------

  virtual nsresult Init(nsIContent* aRoot);

  virtual nsresult Init(nsIDOMRange* aRange);

  virtual void First();

  virtual void Last();

  virtual void Next();

  virtual void Prev();

  virtual nsIContent *GetCurrentNode();

  virtual PRBool IsDone();

  virtual nsresult PositionAt(nsIContent* aCurNode);

  // nsIEnumertor interface methods ------------------------------

  //NS_IMETHOD CurrentItem(nsISupports **aItem);

  //nsIGeneratedContentIterator
  virtual nsresult Init(nsIPresShell *aShell, nsIDOMRange* aRange);
  virtual nsresult Init(nsIPresShell *aShell, nsIContent* aContent);

protected:

  // Do these cause too much refcounting???
  nsCOMPtr<nsIContent> GetDeepFirstChild(nsCOMPtr<nsIContent> aRoot);
  nsCOMPtr<nsIContent> GetDeepLastChild(nsCOMPtr<nsIContent> aRoot);

  nsIContent *GetNextSibling(nsIContent *aNode);
  nsIContent *GetPrevSibling(nsIContent *aNode);

  nsIContent *NextNode(nsIContent *aNode);
  nsIContent *PrevNode(nsIContent *aNode);

  void MakeEmpty();

  nsCOMPtr<nsIContent> mCurNode;
  nsCOMPtr<nsIContent> mFirst;
  nsCOMPtr<nsIContent> mLast;
  nsCOMPtr<nsIContent> mCommonParent;

  nsCOMPtr<nsIContentIterator> mFirstIter;
  nsCOMPtr<nsIContentIterator> mLastIter;
  nsCOMPtr<nsIContentIterator> mGenIter;
  nsIPresShell::GeneratedContentType mIterType;
  nsIPresShell::GeneratedContentType mFirstIterType;
  nsIPresShell::GeneratedContentType mLastIterType;
  nsCOMPtr<nsIPresShell> mPresShell;
  PRBool mIsDone;

private:

  // no copy's or assigns  FIX ME
  nsGeneratedContentIterator(const nsGeneratedContentIterator&);
  nsGeneratedContentIterator& operator=(const nsGeneratedContentIterator&);

};


/******************************************************
 * repository cruft
 ******************************************************/

nsresult
NS_NewGenRegularIterator(nsIContentIterator ** aInstancePtrResult)
{
  nsGeneratedContentIterator * iter = new nsGeneratedContentIterator();
  if (!iter) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(*aInstancePtrResult = iter);

  return NS_OK;
}


/******************************************************
 * XPCOM cruft
 ******************************************************/

NS_IMPL_ISUPPORTS2(nsGeneratedContentIterator, nsIGeneratedContentIterator,
                   nsIContentIterator)


/******************************************************
 * constructor/destructor
 ******************************************************/

nsGeneratedContentIterator::nsGeneratedContentIterator()
  : mIsDone(PR_FALSE)
{
}


nsGeneratedContentIterator::~nsGeneratedContentIterator()
{
}


/******************************************************
 * Init routines
 ******************************************************/


nsresult
nsGeneratedContentIterator::Init(nsIContent* aRoot)
{
  if (!aRoot)
    return NS_ERROR_NULL_POINTER;
  mIsDone = PR_FALSE;
  nsCOMPtr<nsIContent> root(aRoot);
  mFirst = GetDeepFirstChild(root);
  if (mGenIter)//we have generated
  {
    mFirstIter = mGenIter;
    mFirstIterType = mIterType;
  }
  mLast = root;
  mCommonParent = root;
  mCurNode = mFirst;
  return NS_OK;
}

nsresult
nsGeneratedContentIterator::Init(nsIPresShell *aShell, nsIDOMRange* aRange)
{
  mPresShell = aShell;
  return Init(aRange);
}

nsresult
nsGeneratedContentIterator::Init(nsIPresShell *aShell, nsIContent* aContent)
{
  mPresShell = aShell;
  return Init(aContent);
}


nsresult
nsGeneratedContentIterator::Init(nsIDOMRange* aRange)
{
  if (!aRange)
    return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIDOMNode> dN;

  nsCOMPtr<nsIContent> startCon;
  nsCOMPtr<nsIDOMNode> startDOM;
  nsCOMPtr<nsIContent> endCon;
  nsCOMPtr<nsIDOMNode> endDOM;
  PRInt32 startIndx;
  PRInt32 endIndx;

  mIsDone = PR_FALSE;

  // get common content parent
  if (NS_FAILED(aRange->GetCommonAncestorContainer(getter_AddRefs(dN))) || !dN)
    return NS_ERROR_FAILURE;
  mCommonParent = do_QueryInterface(dN);

  // get the start node and offset, convert to nsIContent
  aRange->GetStartContainer(getter_AddRefs(startDOM));
  if (!startDOM)
    return NS_ERROR_ILLEGAL_VALUE;
  startCon = do_QueryInterface(startDOM);
  if (!startCon)
    return NS_ERROR_FAILURE;

  aRange->GetStartOffset(&startIndx);

  // get the end node and offset, convert to nsIContent
  aRange->GetEndContainer(getter_AddRefs(endDOM));
  if (!endDOM)
    return NS_ERROR_ILLEGAL_VALUE;
  endCon = do_QueryInterface(endDOM);
  if (!endCon)
    return NS_ERROR_FAILURE;

  aRange->GetEndOffset(&endIndx);

  // find first node in range
  nsIContent *cChild = startCon->GetChildAt(0);

  // short circuit when start node == end node
  if (startDOM == endDOM)
  {
    if (!cChild) // no children, must be a text node or empty container
    {
      mFirst = startCon;
      mLast = startCon;
      mCurNode = startCon;
      return NS_OK;
    }
    else
    {
      if (startIndx == endIndx)  // collapsed range
      {
        MakeEmpty();
        return NS_OK;
      }
    }
  }

  if (!cChild) // no children, must be a text node
  {
    mFirst = startCon;
  }
  else
  {
    cChild = startCon->GetChildAt(startIndx);
    if (!cChild)  // offset after last child, parent is first node
    {
      mFirst = startCon;
    }
    else
    {
      mFirst = GetDeepFirstChild(cChild);
      if (mGenIter)
      {
        mFirstIter = mGenIter;
        mFirstIterType = mIterType;
      }
    }
    // Does that first node really intersect the range?
    // the range could be collapsed, or the range could be
    // 'degenerate', ie not collapsed but still containing
    // no content.  In this case, we want the iterator to
    // be empty

    if (!nsRange::IsNodeIntersectsRange(mFirst, aRange))
    {
      MakeEmpty();
      return NS_OK;
    }
  }

  // find last node in range
  cChild = endCon->GetChildAt(0);

  if (!cChild) // no children, must be a text node
  {
    mLast = endCon;
  }
  else if (endIndx == 0) // before first child, parent is last node
  {
    mLast = endCon;
  }
  else
  {
    cChild = endCon->GetChildAt(--endIndx);
    if (!cChild)  // offset after last child, last child is last node
    {
      endIndx = (PRInt32)endCon->GetChildCount();
      cChild = endCon->GetChildAt(--endIndx);
      if (!cChild)
      {
        NS_NOTREACHED("nsGeneratedContentIterator::nsGeneratedContentIterator");
        return NS_ERROR_FAILURE;
      }
    }
    mLast = cChild;
  }

  mCurNode = mFirst;
  return NS_OK;
}


/******************************************************
 * Helper routines
 ******************************************************/

void nsGeneratedContentIterator::MakeEmpty()
{
  nsCOMPtr<nsIContent> noNode;
  mCurNode = noNode;
  mFirst = noNode;
  mLast = noNode;
  mCommonParent = noNode;
  mIsDone = PR_TRUE;
  mGenIter = 0;
  mFirstIter = 0;
  mIterType = nsIPresShell::Before;
}

nsCOMPtr<nsIContent> nsGeneratedContentIterator::GetDeepFirstChild(nsCOMPtr<nsIContent> aRoot)
{
  nsCOMPtr<nsIContent> deepFirstChild;

  if (aRoot)
  {
    nsCOMPtr<nsIContent> cN = aRoot;

#if DO_BEFORE
    //CHECK FOR BEFORESTUFF
    nsresult result = NS_ERROR_FAILURE;
    if (mPresShell)
      result = mPresShell->GetGeneratedContentIterator(cN,nsIPresShell::Before,getter_AddRefs(mGenIter));
    if (NS_SUCCEEDED(result) && mGenIter)
    { //ok we have a generated iter all bets are off
      mIterType = nsIPresShell::Before;
      mGenIter->First();
      return cN;
    }
#endif

    nsIContent *cChild;
    while ((cChild = cN->GetChildAt(0)))
    {
      cN = cChild;

#if DO_BEFORE
      //CHECK FOR BEFORESTUFF
      if (mPresShell)
        result = mPresShell->GetGeneratedContentIterator(cN,nsIPresShell::Before,getter_AddRefs(mGenIter));
      if (NS_SUCCEEDED(result) && mGenIter)
      { //ok we have a generated iter all bets are off
        mIterType = nsIPresShell::Before;
        mGenIter->First();
        return cN;
      }
#endif
    }

    deepFirstChild = cN;
  }

  return deepFirstChild;
}

nsCOMPtr<nsIContent> nsGeneratedContentIterator::GetDeepLastChild(nsCOMPtr<nsIContent> aRoot)
{
  nsCOMPtr<nsIContent> deepFirstChild;

  if (aRoot)
  {
    nsCOMPtr<nsIContent> cN = aRoot;
    nsCOMPtr<nsIContent> cChild;

#if DO_AFTER
    //CHECK FOR AFTER STUFF
    nsresult result = NS_ERROR_FAILURE;
    if (mPresShell)
      result = mPresShell->GetGeneratedContentIterator(cN,nsIPresShell::After,getter_AddRefs(mGenIter));
    if (NS_SUCCEEDED(result) && mGenIter)
    { //ok we have a generated iter all bets are off
      mIterType = nsIPresShell::After;
      mGenIter->First();
      return cN;
    }
#endif

    PRUint32 numChildren = cN->GetChildCount();

    while ( numChildren )
    {
      cChild = cN->GetChildAt(--numChildren);
      if (cChild)
      {
#if DO_AFTER
        //CHECK FOR AFTER STUFF
        if (mPresShell)
          result = mPresShell->GetGeneratedContentIterator(cChild,nsIPresShell::After,getter_AddRefs(mGenIter));
        if (NS_SUCCEEDED(result) && mGenIter)
        { //ok we have a generated iter all bets are off
          mGenIter->Last();
          mIterType = nsIPresShell::After;
          return cChild;
        }
#endif
        numChildren = cChild->GetChildCount();
        cN = cChild;
      }
      else
      {
        break;
      }
    }
    deepFirstChild = cN;
  }

  return deepFirstChild;
}

// Get the next sibling, or parents next sibling, or grandpa's next sibling...
nsIContent *
nsGeneratedContentIterator::GetNextSibling(nsIContent *aNode)
{
  if (!aNode)
    return nsnull;

  nsIContent *parent = aNode->GetParent();
  if (!parent)
    return nsnull;

  PRInt32 indx = parent->IndexOf(aNode);

  nsIContent *sib = parent->GetChildAt(++indx);

  if (!sib)
  {
#if DO_AFTER
    //CHECK FOR AFTERESTUFF
    if (mPresShell)
      mPresShell->GetGeneratedContentIterator(parent, nsIPresShell::After,
                                              getter_AddRefs(mGenIter));
    if (mGenIter)
    { //ok we have a generated iter all bets are off
      mGenIter->First();
      mIterType = nsIPresShell::After;

      return parent;
    }
#endif
    if (parent != mCommonParent)
    {
      return GetNextSibling(parent);
    }
    else
    {
      sib = nsnull;
    }
  }

  return sib;
}

// Get the prev sibling, or parents prev sibling, or grandpa's prev sibling...
nsIContent *
nsGeneratedContentIterator::GetPrevSibling(nsIContent *aNode)
{
  if (!aNode)
    return nsnull;

  nsIContent *parent = aNode->GetParent();
  if (!parent)
    return nsnull;

  PRInt32 indx = parent->IndexOf(aNode);
  nsIContent *sib = nsnull;

  if (indx < 1 || !(sib = parent->GetChildAt(--indx)))
  {
#if DO_BEFORE
    //CHECK FOR BEFORESTUFF
    if (mPresShell)
      mPresShell->GetGeneratedContentIterator(parent, nsIPresShell::Before,
                                              getter_AddRefs(mGenIter));
    if (mGenIter)
    { //ok we have a generated iter all bets are off
      mGenIter->Last();
      mIterType = nsIPresShell::Before;
      return parent;
    }
    else
#endif
    if (parent != mCommonParent)
    {
      return GetPrevSibling(parent);
    }
    else
    {
      sib = nsnull;
    }
  }

  return sib;
}

nsIContent *
nsGeneratedContentIterator::NextNode(nsIContent *aNode)
{
  if (!aNode)
    return nsnull;

  if (mGenIter)
  {
    if (mGenIter->IsDone())
      mGenIter = 0;
    else {
      mGenIter->Next();

      return nsnull;
    }

    if (nsIPresShell::After == mIterType)//answer is parent
    {
      //*ioNextNode = parent; leave it the same
      return nsnull;
    }
    nsIContent *cN = aNode->GetChildAt(0);
    if (cN)
    {
      return GetDeepFirstChild(cN);
    }
  }

  nsIContent *cN = aNode;

    // get next sibling if there is one
  nsIContent *parent = cN->GetParent();
  if (!parent)
  {
    // a little noise to catch some iterator usage bugs.
    NS_NOTREACHED("nsGeneratedContentIterator::NextNode() : no parent found");
    return nsnull;
  }

  PRInt32 indx = parent->IndexOf(cN);

  nsIContent *cSibling = parent->GetChildAt(++indx);
  if (cSibling)
  {
    // next node is siblings "deep left" child
    return GetDeepFirstChild(cSibling);
  }

  //CHECK FOR AFTERSTUFF
  if (mGenIter)//we allready had an afteriter. it must be done!
  {
    mGenIter = 0;
  }
  else//check for after node.
  {
    if (mPresShell)
      mPresShell->GetGeneratedContentIterator(parent, nsIPresShell::After,
                                              getter_AddRefs(mGenIter));
    if (mGenIter)
    { //ok we have a generated iter all bets are off
      mGenIter->First();
      mIterType = nsIPresShell::After;
    }
    else
      mGenIter = 0;
  }

  // else it's the parent
  return parent;
}


//THIS NEEDS TO USE A GENERATED SUBTREEITER HERE
nsIContent *
nsGeneratedContentIterator::PrevNode(nsIContent *aNode)
{
  PRUint32 numChildren = aNode->GetChildCount();

  // if it has children then prev node is last child
  if (numChildren > 0)
  {
    return aNode->GetChildAt(--numChildren);
  }

  // else prev sibling is previous
  return GetPrevSibling(aNode);
}

/******************************************************
 * ContentIterator routines
 ******************************************************/

void
nsGeneratedContentIterator::First()
{
  if (!mFirst) {
    mIsDone = PR_TRUE;

    return;
  }

  mIsDone = PR_FALSE;

  mCurNode = mFirst;
  mGenIter = mFirstIter;
  if (mGenIter)//set directionback to before...
    mGenIter->First();
}


void
nsGeneratedContentIterator::Last()
{
  if (!mLast) {
    mIsDone = PR_TRUE;

    return;
  }

  mIsDone = PR_FALSE;

  mGenIter = mLastIter;
  mCurNode = mLast;
}


void
nsGeneratedContentIterator::Next()
{
  if (mIsDone || !mCurNode)
    return;

  if (GetCurrentNode() == mLast)
  {
    mIsDone = PR_TRUE;
    return;
  }

  mCurNode = NextNode(mCurNode);
}


void
nsGeneratedContentIterator::Prev()
{
  if (mIsDone)
    return;
  if (!mCurNode)
    return;
  if (mCurNode == mFirst)
  {
    mIsDone = PR_TRUE;
    return;
  }

  mCurNode = PrevNode(mCurNode);
}


PRBool
nsGeneratedContentIterator::IsDone()
{
  return mIsDone;
}


nsresult
nsGeneratedContentIterator::PositionAt(nsIContent* aCurNode)
{
  // XXX need to confirm that aCurNode is within range
  if (!aCurNode)
    return NS_ERROR_NULL_POINTER;
  mCurNode = aCurNode;
  mIsDone = PR_FALSE;
  return NS_OK;
}


nsIContent *
nsGeneratedContentIterator::GetCurrentNode()
{
  if (!mCurNode || mIsDone) {
    return nsnull;
  }

  if (mGenIter) {
    return mGenIter->GetCurrentNode();
  }

  NS_ASSERTION(mCurNode, "Null current node in an iterator that's not done!");

  return mCurNode;
}





/*====================================================================================*/
//SUBTREE ITERATOR
/*====================================================================================*/
/******************************************************
 * nsGeneratedSubtreeIterator
 ******************************************************/


/*
 *  A simple iterator class for traversing the content in "top subtree" order
 */
class nsGeneratedSubtreeIterator : public nsGeneratedContentIterator
{
public:
  nsGeneratedSubtreeIterator() {};
  virtual ~nsGeneratedSubtreeIterator() {};

  // nsContentIterator overrides ------------------------------

  virtual nsresult Init(nsIContent* aRoot);

  virtual nsresult Init(nsIDOMRange* aRange);

  virtual void Next();

  virtual void Prev();

  virtual nsresult PositionAt(nsIContent* aCurNode);

  //nsIGeneratedContentIterator
  virtual nsresult Init(nsIPresShell *aShell, nsIDOMRange* aRange);
  virtual nsresult Init(nsIPresShell *aShell, nsIContent* aContent);
protected:

  nsresult GetTopAncestorInRange( nsCOMPtr<nsIContent> aNode,
                                  nsCOMPtr<nsIContent> *outAnestor);

  // no copy's or assigns  FIX ME
  nsGeneratedSubtreeIterator(const nsGeneratedSubtreeIterator&);
  nsGeneratedSubtreeIterator& operator=(const nsGeneratedSubtreeIterator&);
  nsCOMPtr<nsIDOMRange> mRange;
};

nsresult NS_NewGenSubtreeIterator(nsIContentIterator** aInstancePtrResult);




/******************************************************
 * repository cruft
 ******************************************************/

nsresult NS_NewGenSubtreeIterator(nsIContentIterator** aInstancePtrResult)
{
  nsGeneratedSubtreeIterator * iter = new nsGeneratedSubtreeIterator();
  if (!iter) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(*aInstancePtrResult = iter);

  return NS_OK;
}



/******************************************************
 * Init routines
 ******************************************************/


nsresult
nsGeneratedSubtreeIterator::Init(nsIContent* aRoot)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
nsGeneratedSubtreeIterator::Init(nsIPresShell *aShell, nsIDOMRange* aRange)
{
  mPresShell = aShell;
  return Init(aRange);
}

nsresult
nsGeneratedSubtreeIterator::Init(nsIPresShell *aShell, nsIContent* aContent)
{
  mPresShell = aShell;
  return Init(aContent);
}

nsresult
nsGeneratedSubtreeIterator::Init(nsIDOMRange* aRange)
{
  if (!aRange)
    return NS_ERROR_NULL_POINTER;

  mIsDone = PR_FALSE;

  mRange = aRange;

  // get the start node and offset, convert to nsIContent
  nsCOMPtr<nsIDOMNode> commonParent;
  nsCOMPtr<nsIDOMNode> startParent;
  nsCOMPtr<nsIDOMNode> endParent;
  nsCOMPtr<nsIContent> cStartP;
  nsCOMPtr<nsIContent> cEndP;
  nsCOMPtr<nsIContent> cN;
  nsCOMPtr<nsIContent> firstCandidate;
  nsCOMPtr<nsIContent> lastCandidate;
  nsCOMPtr<nsIDOMNode> dChild;
  nsCOMPtr<nsIContent> cChild;
  PRInt32 indx, startIndx, endIndx;
  PRInt32 numChildren;

  // get common content parent
  if (NS_FAILED(aRange->GetCommonAncestorContainer(getter_AddRefs(commonParent))) || !commonParent)
    return NS_ERROR_FAILURE;
  mCommonParent = do_QueryInterface(commonParent);

  // get start content parent
  if (NS_FAILED(aRange->GetStartContainer(getter_AddRefs(startParent))) || !startParent)
    return NS_ERROR_FAILURE;
  cStartP = do_QueryInterface(startParent);
  aRange->GetStartOffset(&startIndx);

  // get end content parent
  if (NS_FAILED(aRange->GetEndContainer(getter_AddRefs(endParent))) || !endParent)
    return NS_ERROR_FAILURE;
  cEndP = do_QueryInterface(endParent);
  aRange->GetEndOffset(&endIndx);

  // short circuit when start node == end node
  if (startParent == endParent)
  {
    cChild = cStartP->GetChildAt(0);

    if (!cChild) // no children, must be a text node or empty container
    {
      // all inside one text node - empty subtree iterator
      MakeEmpty();
      return NS_OK;
    }
    else
    {
      if (startIndx == endIndx)  // collapsed range
      {
        MakeEmpty();
        return NS_OK;
      }
    }
  }

  // find first node in range
  aRange->GetStartOffset(&indx);
  numChildren = GetNumChildren(startParent);

  if (!numChildren) // no children, must be a text node
  {
    cN = cStartP;
  }
  else
  {
    dChild = GetChildAt(startParent, indx);
    cChild = do_QueryInterface(dChild);
    if (!cChild)  // offset after last child
    {
      cN = cStartP;
    }
    else
    {
      firstCandidate = cChild;
    }
  }

  if (!firstCandidate)
  {
    // then firstCandidate is next node after cN
    firstCandidate = GetNextSibling(cN);

    if (!firstCandidate)
    {
      MakeEmpty();
      return NS_OK;
    }
  }
  if (mGenIter)
  {
    mFirstIter = mGenIter;
    mFirstIterType = mIterType;
  }
  if (!mFirstIter)
  {
    firstCandidate = GetDeepFirstChild(firstCandidate);
    if (mGenIter)
    {
      mFirstIter = mGenIter;
      mFirstIterType = mIterType;
    }
  }
  // confirm that this first possible contained node
  // is indeed contained.  Else we have a range that
  // does not fully contain any node.

  PRBool nodeBefore(PR_FALSE), nodeAfter(PR_FALSE);
  if (!mFirstIter &&
      NS_FAILED(nsRange::CompareNodeToRange(firstCandidate, aRange,
                                            &nodeBefore, &nodeAfter)))
    return NS_ERROR_FAILURE;

  if (nodeBefore || nodeAfter)
  {
    MakeEmpty();
    return NS_OK;
  }

  // cool, we have the first node in the range.  Now we walk
  // up it's ancestors to find the most senior that is still
  // in the range.  That's the real first node.
  if (NS_SUCCEEDED(GetTopAncestorInRange(firstCandidate, address_of(mFirst))))
  {
    mFirstIter = 0;//ancestor has one no
    mGenIter = 0;
  }
  else if (!mFirstIter) //something bad happened and its not generated content iterators fault
    return NS_ERROR_FAILURE;
  else
    mFirst = firstCandidate;//setting last candidate to parent of generated content this is ok



  // now to find the last node
  aRange->GetEndOffset(&indx);
  numChildren = GetNumChildren(endParent);

  if (indx > numChildren) indx = numChildren;
  if (!indx)
  {
    cN = cEndP;
  }
  else
  {
    if (!numChildren) // no children, must be a text node
    {
      cN = cEndP;
    }
    else
    {
      dChild = GetChildAt(endParent, --indx);
      cChild = do_QueryInterface(dChild);
      if (!cChild)  // shouldn't happen
      {
        NS_ASSERTION(0,"tree traversal trouble in nsGeneratedSubtreeIterator::Init");
        return NS_ERROR_FAILURE;
      }
      else
      {
        lastCandidate = cChild;
      }
    }
  }

  if (!lastCandidate)
  {
    // then lastCandidate is prev node before cN
    lastCandidate = GetPrevSibling(cN);

    if (!lastCandidate)
    {
      MakeEmpty();
      return NS_OK;
    }
  }
  if (mGenIter)
  {
    mLastIter = mGenIter;
    mLastIterType = mIterType;
  }
  if (!mLastIter)//dont ever set last candidate to a generated node!
  {
    lastCandidate = GetDeepLastChild(lastCandidate);
    if (mGenIter)
    {
      mLastIter = mGenIter;
      mLastIterType = mIterType;
    }
  }

  // confirm that this first possible contained node
  // is indeed contained.  Else we have a range that
  // does not fully contain any node.

  if (!mLastIter &&
      NS_FAILED(nsRange::CompareNodeToRange(lastCandidate, aRange, &nodeBefore,
                                            &nodeAfter)))
    return NS_ERROR_FAILURE;

  if (nodeBefore || nodeAfter)
  {
    MakeEmpty();
    return NS_OK;
  }

  // cool, we have the last node in the range.  Now we walk
  // up it's ancestors to find the most senior that is still
  // in the range.  That's the real first node.
  if (NS_SUCCEEDED(GetTopAncestorInRange(lastCandidate, address_of(mLast))))
  {
    mLastIter = 0;//ancestor has one no
    mGenIter = 0;
  }
  else if (!mLastIter) //something bad happened and its not generated content iterators fault
    return NS_ERROR_FAILURE;
  else
    mLast = lastCandidate;//setting last candidate to parent of generated content this is ok

  mCurNode = mFirst;
  mGenIter = mFirstIter;
  mIterType = mFirstIterType ;
  return NS_OK;
}


/****************************************************************
 * nsGeneratedSubtreeIterator overrides of ContentIterator routines
 ****************************************************************/

void
nsGeneratedSubtreeIterator::Next()
{
  if (mIsDone)
    return;
  nsCOMPtr<nsIContent> curnode;
  nsCOMPtr<nsIContent> nextNode;
  if (mGenIter)
  {
    if (mGenIter->IsDone())
    {
      mGenIter = 0;

      if (mIterType == nsIPresShell::After)
      {
        nextNode = GetNextSibling(mCurNode);

        if (!nextNode)
        {
          mIsDone = PR_TRUE;

          return;
        }
      }
      else
      {
        nextNode = mCurNode->GetChildAt(0);
      }
    }
    else {
       mGenIter->Next();

       return;
    }
  }
  else
  {
    if (mCurNode == mLast)
    {
      mIsDone = PR_TRUE;
      return;
    }
    nextNode = GetNextSibling(mCurNode);

    if (!nextNode)
    {
      mIsDone = PR_TRUE;

      return;
    }
  }

  if (!mGenIter)
    nextNode = GetDeepFirstChild(nextNode);
  if (NS_SUCCEEDED(GetTopAncestorInRange(nextNode, address_of(mCurNode))))
  {
    mGenIter = 0;
  }
  else if (!mGenIter) //something bad happened and its not generated content iterators fault
    return;
  else
    mCurNode = nextNode;//setting last candidate to parent of generated content this is ok
}


void
nsGeneratedSubtreeIterator::Prev()
{
//notimplemented
  NS_ERROR("Not implemented!");
}

nsresult
nsGeneratedSubtreeIterator::PositionAt(nsIContent* aCurNode)
{
  NS_ERROR("Not implemented!");

  return NS_ERROR_NOT_IMPLEMENTED;
}

/****************************************************************
 * nsGeneratedSubtreeIterator helper routines
 ****************************************************************/

nsresult nsGeneratedSubtreeIterator::GetTopAncestorInRange(
                                       nsCOMPtr<nsIContent> aNode,
                                       nsCOMPtr<nsIContent> *outAnestor)
{
  if (!aNode)
    return NS_ERROR_NULL_POINTER;
  if (!outAnestor)
    return NS_ERROR_NULL_POINTER;


  // sanity check: aNode is itself in the range
  PRBool nodeBefore, nodeAfter;
  if (NS_FAILED(nsRange::CompareNodeToRange(aNode, mRange, &nodeBefore,
                                            &nodeAfter)))
    return NS_ERROR_FAILURE;

  if (nodeBefore || nodeAfter)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIContent> parent;
  while (aNode)
  {
    parent = aNode->GetParent();
    if (!parent)
      return NS_ERROR_FAILURE;
    if (NS_FAILED(nsRange::CompareNodeToRange(parent, mRange, &nodeBefore,
                                              &nodeAfter)))
      return NS_ERROR_FAILURE;

    if (nodeBefore || nodeAfter)
    {
      *outAnestor = aNode;
      return NS_OK;
    }
    aNode = parent;
  }
  return NS_ERROR_FAILURE;
}






