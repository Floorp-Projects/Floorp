/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
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
#include "nsIFocusTracker.h"
#include "nsCOMPtr.h"
#include "nsIPresContext.h"
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
class nsGeneratedContentIterator : public nsIContentIterator ,public nsIGeneratedContentIterator//, public nsIEnumerator
{
public:
  NS_DECL_ISUPPORTS

  nsGeneratedContentIterator();
  virtual ~nsGeneratedContentIterator();

  // nsIContentIterator interface methods ------------------------------

  NS_IMETHOD Init(nsIContent* aRoot);

  NS_IMETHOD Init(nsIDOMRange* aRange);

  NS_IMETHOD First();

  NS_IMETHOD Last();
  
  NS_IMETHOD Next();

  NS_IMETHOD Prev();

  NS_IMETHOD CurrentNode(nsIContent **aNode);

  NS_IMETHOD IsDone();

  NS_IMETHOD PositionAt(nsIContent* aCurNode);

  NS_IMETHOD MakePre();

  NS_IMETHOD MakePost();

  
  // nsIEnumertor interface methods ------------------------------
  
  //NS_IMETHOD CurrentItem(nsISupports **aItem);

  //nsIGeneratedContentIterator
  NS_IMETHOD Init(nsIPresShell *aShell, nsIDOMRange* aRange);
  NS_IMETHOD Init(nsIPresShell *aShell, nsIContent* aContent);

protected:

  // Do these cause too much refcounting???
  nsCOMPtr<nsIContent> GetDeepFirstChild(nsCOMPtr<nsIContent> aRoot);
  nsCOMPtr<nsIContent> GetDeepLastChild(nsCOMPtr<nsIContent> aRoot);
  
  nsresult GetNextSibling(nsCOMPtr<nsIContent> aNode, nsCOMPtr<nsIContent> *aSibling);
  nsresult GetPrevSibling(nsCOMPtr<nsIContent> aNode, nsCOMPtr<nsIContent> *aSibling);
  
  nsresult NextNode(nsCOMPtr<nsIContent> *ioNextNode);
  nsresult PrevNode(nsCOMPtr<nsIContent> *ioPrevNode);

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
  PRBool mPre;
  
private:

  // no copy's or assigns  FIX ME
  nsGeneratedContentIterator(const nsGeneratedContentIterator&);
  nsGeneratedContentIterator& operator=(const nsGeneratedContentIterator&);

};


/******************************************************
 * repository cruft
 ******************************************************/

nsresult NS_NewGenRegularIterator(nsIContentIterator ** aInstancePtrResult)
{
  nsGeneratedContentIterator * iter = new nsGeneratedContentIterator();
  if (iter)
    return iter->QueryInterface(NS_GET_IID(nsIContentIterator), (void**) aInstancePtrResult);
  return NS_ERROR_OUT_OF_MEMORY;
}


/******************************************************
 * XPCOM cruft
 ******************************************************/
 
NS_IMPL_ADDREF(nsGeneratedContentIterator)
NS_IMPL_RELEASE(nsGeneratedContentIterator)

nsresult nsGeneratedContentIterator::QueryInterface(const nsIID& aIID,
                                     void** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null pointer");
  if (nsnull == aInstancePtrResult) 
  {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(NS_GET_IID(nsISupports)))
  {
    *aInstancePtrResult = (void*)(nsISupports*)(nsIContentIterator*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
/*  if (aIID.Equals(NS_GET_IID(nsIEnumerator))) 
  {
    *aInstancePtrResult = (void*)(nsIEnumerator*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }  */
  if (aIID.Equals(NS_GET_IID(nsIContentIterator))) 
  {
    *aInstancePtrResult = (void*)(nsIContentIterator*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsIGeneratedContentIterator))) 
  {
    *aInstancePtrResult = (void*)(nsIGeneratedContentIterator*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}


/******************************************************
 * constructor/destructor
 ******************************************************/

nsGeneratedContentIterator::nsGeneratedContentIterator() :
  // don't need to explicitly initialize |nsCOMPtr|s, they will automatically be NULL
  mIsDone(PR_FALSE), mPre(PR_FALSE)
{
  NS_INIT_REFCNT();
}


nsGeneratedContentIterator::~nsGeneratedContentIterator()
{
}


/******************************************************
 * Init routines
 ******************************************************/


nsresult nsGeneratedContentIterator::Init(nsIContent* aRoot)
{
  if (!aRoot) 
    return NS_ERROR_NULL_POINTER; 
  mIsDone = PR_FALSE;
  nsCOMPtr<nsIContent> root( do_QueryInterface(aRoot) );
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

NS_IMETHODIMP
nsGeneratedContentIterator::Init(nsIPresShell *aShell, nsIDOMRange* aRange)
{
  mPresShell = aShell;
  return Init(aRange);
}

NS_IMETHODIMP
nsGeneratedContentIterator::Init(nsIPresShell *aShell, nsIContent* aContent)
{
  mPresShell = aShell;
  return Init(aContent);
}


nsresult nsGeneratedContentIterator::Init(nsIDOMRange* aRange)
{
  if (!aRange) 
    return NS_ERROR_NULL_POINTER; 

  nsCOMPtr<nsIDOMNode> dN;
  nsCOMPtr<nsIContent> cChild;
  
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
  
  // short circuit when start node == end node
  if (startDOM == endDOM)
  {
    startCon->ChildAt(0,*getter_AddRefs(cChild));
  
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
  
  // find first node in range
  startCon->ChildAt(0,*getter_AddRefs(cChild));
  
  if (!cChild) // no children, must be a text node
  {
    mFirst = startCon; 
  }
  else
  {
    startCon->ChildAt(startIndx,*getter_AddRefs(cChild));
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
  
    if (!IsNodeIntersectsRange(mFirst, aRange))
    {
      MakeEmpty();
      return NS_OK;
    }
  }
  
  // find last node in range
  endCon->ChildAt(0,*getter_AddRefs(cChild));

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
    endCon->ChildAt(--endIndx,*getter_AddRefs(cChild));
    if (!cChild)  // offset after last child, last child is last node
    {
      endCon->ChildCount(endIndx);
      endCon->ChildAt(--endIndx,*getter_AddRefs(cChild)); 
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
    nsCOMPtr<nsIContent> cChild;

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
    cN->ChildAt(0,*getter_AddRefs(cChild));
    while ( cChild )
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

      cN->ChildAt(0,*getter_AddRefs(cChild));
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
    PRInt32 numChildren;
  
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

    cN->ChildCount(numChildren);

    while ( numChildren )
    {
      cN->ChildAt(--numChildren,*getter_AddRefs(cChild));
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
        cChild->ChildCount(numChildren);
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
nsresult nsGeneratedContentIterator::GetNextSibling(nsCOMPtr<nsIContent> aNode, nsCOMPtr<nsIContent> *aSibling)
{
  if (!aNode) 
    return NS_ERROR_NULL_POINTER;
  if (!aSibling) 
    return NS_ERROR_NULL_POINTER;
  
  nsCOMPtr<nsIContent> sib;
  nsCOMPtr<nsIContent> parent;
  PRInt32              indx;
  
  if (NS_FAILED(aNode->GetParent(*getter_AddRefs(parent))) || !parent)
    return NS_ERROR_FAILURE;

  if (NS_FAILED(parent->IndexOf(aNode, indx)))
    return NS_ERROR_FAILURE;

  if (NS_SUCCEEDED(parent->ChildAt(++indx, *getter_AddRefs(sib))) && sib)
  {
    *aSibling = sib;
  }
  else //CHECK AFTER CONTEXT ON PARENT
  {
#if DO_AFTER
    //CHECK FOR AFTERESTUFF
    nsresult result = NS_ERROR_FAILURE;
    if (mPresShell)
      result = mPresShell->GetGeneratedContentIterator(parent,nsIPresShell::After,getter_AddRefs(mGenIter));
    if (NS_SUCCEEDED(result) && mGenIter)
    { //ok we have a generated iter all bets are off
      mGenIter->First();
      mIterType = nsIPresShell::After;
      *aSibling = parent;
        return result;
    }
#endif    
    if (parent != mCommonParent)
    {
      return GetNextSibling(parent, aSibling);
    }
    else
    {
      *aSibling = nsCOMPtr<nsIContent>();
    }
  }
  
  return NS_OK;
}

// Get the prev sibling, or parents prev sibling, or grandpa's prev sibling...
nsresult nsGeneratedContentIterator::GetPrevSibling(nsCOMPtr<nsIContent> aNode, nsCOMPtr<nsIContent> *aSibling)
{
  if (!aNode) 
    return NS_ERROR_NULL_POINTER;
  if (!aSibling) 
    return NS_ERROR_NULL_POINTER;
  
  nsCOMPtr<nsIContent> sib;
  nsCOMPtr<nsIContent> parent;
  PRInt32              indx;
  
  if (NS_FAILED(aNode->GetParent(*getter_AddRefs(parent))) || !parent)
    return NS_ERROR_FAILURE;

  if (NS_FAILED(parent->IndexOf(aNode, indx)))
    return NS_ERROR_FAILURE;

  if (indx && NS_SUCCEEDED(parent->ChildAt(--indx, *getter_AddRefs(sib))) && sib)
  {
    *aSibling = sib;
  }
  else
  {
#if DO_BEFORE
    //CHECK FOR BEFORESTUFF
    nsresult result = NS_ERROR_FAILURE;
    if (mPresShell)
      result = mPresShell->GetGeneratedContentIterator(parent,nsIPresShell::Before,getter_AddRefs(mGenIter));
    if (NS_SUCCEEDED(result) && mGenIter)
    { //ok we have a generated iter all bets are off
      mGenIter->Last();
      *aSibling = parent;
      mIterType = nsIPresShell::Before;
      return result;
    }
    else
#endif
    if (parent != mCommonParent)
    {
      return GetPrevSibling(parent, aSibling);
    }
    else
    {
      *aSibling = nsCOMPtr<nsIContent>();
    }
  }  
  return NS_OK;
}

nsresult nsGeneratedContentIterator::NextNode(nsCOMPtr<nsIContent> *ioNextNode)
{
  if (!ioNextNode)
    return NS_ERROR_NULL_POINTER;
    
  if (mPre)  // if we are a Pre-order iterator, use pre-order
  {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  else  // post-order*/
  if (mGenIter)
  {
    if (mGenIter->IsDone())
      mGenIter = 0;
    else
       return mGenIter->Next();
    if (nsIPresShell::After == mIterType)//answer is parent
    {
      //*ioNextNode = parent; leave it the same
      return NS_OK;
    }
    nsCOMPtr<nsIContent>cN;
    (*ioNextNode)->ChildAt(0,*getter_AddRefs(cN));
    if (cN)
    {
      *ioNextNode=GetDeepFirstChild(cN);
      return NS_OK;
    }
  }

  {
    nsCOMPtr<nsIContent> cN = *ioNextNode;
    nsCOMPtr<nsIContent> cSibling;
    nsCOMPtr<nsIContent> parent;
    PRInt32              indx;
  
    // get next sibling if there is one
    if (NS_FAILED(cN->GetParent(*getter_AddRefs(parent))))
      return NS_ERROR_FAILURE;
    if (!parent || NS_FAILED(parent->IndexOf(cN, indx)))
    {
      // a little noise to catch some iterator usage bugs.
      NS_NOTREACHED("nsGeneratedContentIterator::NextNode() : no parent found");
      return NS_ERROR_FAILURE;
    }
    if (NS_SUCCEEDED(parent->ChildAt(++indx,*getter_AddRefs(cSibling))) && cSibling)
    {
      // next node is siblings "deep left" child
      *ioNextNode = GetDeepFirstChild(cSibling); 
      return NS_OK;
    }
      //CHECK FOR AFTERSTUFF
    if (mGenIter)//we allready had an afteriter. it must be done!
    {
      mGenIter = 0;
    }
    else//check for after node.
    {
      nsresult result = NS_ERROR_FAILURE;
      if (mPresShell)
        result = mPresShell->GetGeneratedContentIterator(parent,nsIPresShell::After,getter_AddRefs(mGenIter));
      if (NS_SUCCEEDED(result) && mGenIter)
      { //ok we have a generated iter all bets are off
        mGenIter->First();
        mIterType = nsIPresShell::After;
      }
      else
        mGenIter = 0;
    }

    // else it's the parent
    *ioNextNode = parent;
  }
  return NS_OK;
}


//THIS NEEDS TO USE A GENERATED SUBTREEITER HERE
nsresult nsGeneratedContentIterator::PrevNode(nsCOMPtr<nsIContent> *ioNextNode)
{
  if (!ioNextNode)
    return NS_ERROR_NULL_POINTER;
   
 if (mPre)  // if we are a Pre-order iterator, use pre-order
  {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  else  // post-order
  {
    nsCOMPtr<nsIContent> cN = *ioNextNode;
    nsCOMPtr<nsIContent> cLastChild;
    PRInt32 numChildren;
  
    cN->ChildCount(numChildren);
  
    // if it has children then prev node is last child
    if (numChildren)
    {
      if (NS_FAILED(cN->ChildAt(--numChildren,*getter_AddRefs(cLastChild))))
        return NS_ERROR_FAILURE;
      if (!cLastChild)
        return NS_ERROR_FAILURE;
      *ioNextNode = cLastChild;
      return NS_OK;
    }
  
    // else prev sibling is previous
    return GetPrevSibling(cN, ioNextNode);
  }
  return NS_OK;
}

/******************************************************
 * ContentIterator routines
 ******************************************************/

nsresult nsGeneratedContentIterator::First()
{
  if (!mFirst) 
    return NS_ERROR_FAILURE;
  mIsDone = PR_FALSE;
  mCurNode = mFirst;
  mGenIter = mFirstIter;
  if (mGenIter)//set directionback to before...
    mGenIter->First();
  return NS_OK;
}


nsresult nsGeneratedContentIterator::Last()
{
  if (!mLast) 
    return NS_ERROR_FAILURE;
  mIsDone = PR_FALSE;
  mGenIter = mLastIter;
  mCurNode = mLast;
  return NS_OK;
}


nsresult nsGeneratedContentIterator::Next()
{
  if (mIsDone) 
    return NS_OK;
  if (!mCurNode) 
    return NS_OK;
  nsCOMPtr<nsIContent> curnode;
  CurrentNode(getter_AddRefs(curnode));
  if (curnode == mLast) 
  {
    mIsDone = PR_TRUE;
    return NS_OK;
  }
  
  return NextNode(address_of(mCurNode));
}


nsresult nsGeneratedContentIterator::Prev()
{
  if (mIsDone) 
    return NS_OK;
  if (!mCurNode) 
    return NS_OK;
  if (mCurNode == mFirst) 
  {
    mIsDone = PR_TRUE;
    return NS_OK;
  }
  
  return PrevNode(address_of(mCurNode));
}


nsresult nsGeneratedContentIterator::IsDone()
{
  if (mIsDone) 
    return NS_OK;
  else 
    return NS_ENUMERATOR_FALSE;
}


nsresult nsGeneratedContentIterator::PositionAt(nsIContent* aCurNode)
{
  // XXX need to confirm that aCurNode is within range
  if (!aCurNode)
    return NS_ERROR_NULL_POINTER;
  mCurNode = do_QueryInterface(aCurNode);
  mIsDone = PR_FALSE;
  return NS_OK;
}

nsresult nsGeneratedContentIterator::MakePre()
{
  // XXX need to confirm mCurNode is within range
  mPre = PR_TRUE;
  return NS_OK;
}

nsresult nsGeneratedContentIterator::MakePost()
{
  // XXX need to confirm mCurNode is within range
  mPre = PR_FALSE;
  return NS_OK;
}


nsresult nsGeneratedContentIterator::CurrentNode(nsIContent **aNode)
{
  if (!mCurNode) 
    return NS_ERROR_FAILURE;
  if (mIsDone) 
    return NS_ERROR_FAILURE;
  if (mGenIter)
    return mGenIter->CurrentNode(aNode);
  return mCurNode->QueryInterface(NS_GET_IID(nsIContent), (void**) aNode);
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

  NS_IMETHOD Init(nsIContent* aRoot);

  NS_IMETHOD Init(nsIDOMRange* aRange);

  NS_IMETHOD Next();

  NS_IMETHOD Prev();

  NS_IMETHOD PositionAt(nsIContent* aCurNode);

  NS_IMETHOD MakePre();

  NS_IMETHOD MakePost();

  //nsIGeneratedContentIterator
  NS_IMETHOD Init(nsIPresShell *aShell, nsIDOMRange* aRange);
  NS_IMETHOD Init(nsIPresShell *aShell, nsIContent* aContent);
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
  if (iter)
    return iter->QueryInterface(NS_GET_IID(nsIContentIterator), (void**) aInstancePtrResult);
  return NS_ERROR_OUT_OF_MEMORY;
}



/******************************************************
 * Init routines
 ******************************************************/


nsresult nsGeneratedSubtreeIterator::Init(nsIContent* aRoot)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsGeneratedSubtreeIterator::Init(nsIPresShell *aShell, nsIDOMRange* aRange)
{
  mPresShell = aShell;
  return Init(aRange);
}

NS_IMETHODIMP
nsGeneratedSubtreeIterator::Init(nsIPresShell *aShell, nsIContent* aContent)
{
  mPresShell = aShell;
  return Init(aContent);
}

nsresult nsGeneratedSubtreeIterator::Init(nsIDOMRange* aRange)
{
  if (!aRange) 
    return NS_ERROR_NULL_POINTER; 

  mIsDone = PR_FALSE;

  mRange = do_QueryInterface(aRange);
  
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
    cStartP->ChildAt(0,*getter_AddRefs(cChild));
  
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
    if (NS_FAILED(GetNextSibling(cN, address_of(firstCandidate))))
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
  if (!mFirstIter && NS_FAILED(CompareNodeToRange(firstCandidate, aRange, &nodeBefore, &nodeAfter)))
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
    if (NS_FAILED(GetPrevSibling(cN, address_of(lastCandidate))))
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
  
  if (!mLastIter && NS_FAILED(CompareNodeToRange(lastCandidate, aRange, &nodeBefore, &nodeAfter)))
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

nsresult nsGeneratedSubtreeIterator::Next()
{
  if (mIsDone) 
    return NS_OK;
  nsCOMPtr<nsIContent> curnode;
  nsCOMPtr<nsIContent> nextNode;
  if (mGenIter)
  {
    if (mGenIter->IsDone())
    {
      mGenIter = 0;
      if (mIterType == nsIPresShell::After || NS_FAILED(mCurNode->ChildAt(0,*getter_AddRefs(nextNode))))
      {
        if (NS_FAILED(GetNextSibling(mCurNode, address_of(nextNode))))
          return NS_OK;
      }
    }
    else
       return mGenIter->Next();
  }  
  else
  {
    if (mCurNode == mLast) 
    {
      mIsDone = PR_TRUE;
      return NS_OK;
    }
    if (NS_FAILED(GetNextSibling(mCurNode, address_of(nextNode))))
      return NS_OK;
  }
  
  

  if (!mGenIter)
    nextNode = GetDeepFirstChild(nextNode);
  if (NS_SUCCEEDED(GetTopAncestorInRange(nextNode, address_of(mCurNode))))
  {
    mGenIter = 0;
  }
  else if (!mGenIter) //something bad happened and its not generated content iterators fault
    return NS_ERROR_FAILURE;
  else
    mCurNode = nextNode;//setting last candidate to parent of generated content this is ok
  return NS_OK;
}


nsresult nsGeneratedSubtreeIterator::Prev()
{
//notimplemented
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult nsGeneratedSubtreeIterator::PositionAt(nsIContent* aCurNode)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult nsGeneratedSubtreeIterator::MakePre()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult nsGeneratedSubtreeIterator::MakePost()
{
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
  if (NS_FAILED(CompareNodeToRange(aNode, mRange, &nodeBefore, &nodeAfter)))
    return NS_ERROR_FAILURE;
  if (nodeBefore || nodeAfter)
    return NS_ERROR_FAILURE;
  
  nsCOMPtr<nsIContent> parent;
  while (aNode)
  {
    if (NS_FAILED(aNode->GetParent(*getter_AddRefs(parent))) || !parent)
      return NS_ERROR_FAILURE;
    if (NS_FAILED(CompareNodeToRange(parent, mRange, &nodeBefore, &nodeAfter)))
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






