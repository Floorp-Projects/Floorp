/*
 *  A simple iterator class for traversing the content in "close tag" order
 */
class nsGeneratedContentIterator : public nsIContentIterator,nsIGeneratedContentIterator 
{
public:
  NS_DECL_ISUPPORTS

  nsGeneratedContentIterator();
  virtual ~nsGeneratedContentIterator();

  // nsIContentIterator interface methods ------------------------------

  NS_IMETHOD Init(nsIContent* aRoot);

  NS_IMETHOD Init(nsIDOMRange* aRange);

  NS_IMETHOD Init(nsIFocusTracker *aTracker, nsIDOMRange* aRange, PRBool aSelectBefore, PRBool aSelectAfter);

  NS_IMETHOD Init(nsIFocusTracker *aTracker, nsIContent* aRange, PRBool aSelectBefore, PRBool aSelectAfter);

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

protected:

  static nsCOMPtr<nsIContent> GetDeepFirstChild(nsCOMPtr<nsIContent> aRoot);
  static nsCOMPtr<nsIContent> GetDeepLastChild(nsCOMPtr<nsIContent> aRoot);
  
  nsresult GetNextSibling(nsCOMPtr<nsIContent> aNode, nsCOMPtr<nsIContent> *aSibling);
  nsresult GetPrevSibling(nsCOMPtr<nsIContent> aNode, nsCOMPtr<nsIContent> *aSibling);
  
  nsresult NextNode(nsCOMPtr<nsIContent> *ioNextNode);
  nsresult PrevNode(nsCOMPtr<nsIContent> *ioPrevNode);

  void MakeEmpty();
  
  nsCOMPtr<nsIContent> mCurNode;
  nsCOMPtr<nsIContent> mFirst;
  nsCOMPtr<nsIContent> mLast;
  nsCOMPtr<nsIContent> mCommonParent;

  PRBool mIsDone;
  PRBool mPre;
  
private:
  PRBool mSelectBefore;//set at init time
  PRBool mSelectAfter;
  nsIFocusTracker *mTracker;//weak reference

  nsresult FillGenIter(PRInt8 aSide);

  // no copy's or assigns  FIX ME
  nsGeneratedContentIterator(const nsGeneratedContentIterator&);
  nsGeneratedContentIterator& operator=(const nsGeneratedContentIterator&);

};

/*************************
IMPLEMENTATION
**************************/
 
nsGeneratedContentIterator::nsGeneratedContentIterator()
:mSelectBefore(0),mSelectAfter(0), mCurSideOfContent(-1)
{
}

nsGeneratedContentIterator::~nsGeneratedContentIterator()
{
}


NS_IMPL_ADDREF(nsGeneratedContentIterator)
NS_IMPL_RELEASE(nsGeneratedContentIterator)

nsresult NS_NewGeneratedContentIterator(nsIGeneratedContentIterator** aInstancePtrResult)
{
  nsGeneratedContentIterator * iter = new nsGeneratedContentIterator();
  if (iter)
    return iter->QueryInterface(NS_GET_IID(nsIGeneratedContentIterator), (void**) aInstancePtrResult);
  return NS_ERROR_OUT_OF_MEMORY;
}

nsresult nsGeneratedContentIterator::QueryInterface(const nsIID& aIID,
                                     void** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null pointer");
  if (nsnull == aInstancePtrResult) 
  {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kISupportsIID)) 
  {
    *aInstancePtrResult = (void*)(nsISupports*)(nsIContentIterator*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsIGeneratedContentIterator))) 
  {
    *aInstancePtr = NS_STATIC_CAST(nsIGeneratedContentIterator *,this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsIContentIterator))) 
  {
    *aInstancePtrResult = (void*)(nsIContentIterator*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
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
  mLast = root;
  mCommonParent = root;
  mCurNode = mFirst;
  mSelectBefore = PR_FALSE;
  mSelectAfter = PR_FALSE;
  mTracker = nsnull;
  return NS_OK;
}


nsresult nsGeneratedContentIterator::Init(nsIDOMRange* aRange)
{
  if (!aRange) 
    return NS_ERROR_NULL_POINTER; 

  mSelectBefore = PR_FALSE;
  mSelectAfter = PR_FALSE;
  mTracker = nsnull;

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
  if (NS_FAILED(aRange->GetCommonParent(getter_AddRefs(dN))) || !dN)
    return NS_ERROR_FAILURE;
  mCommonParent = do_QueryInterface(dN);

  // get the start node and offset, convert to nsIContent
  aRange->GetStartParent(getter_AddRefs(startDOM));
  if (!startDOM) 
    return NS_ERROR_ILLEGAL_VALUE;
  startCon = do_QueryInterface(startDOM);
  if (!startCon) 
    return NS_ERROR_FAILURE;
  
  aRange->GetStartOffset(&startIndx);
  
  // get the end node and offset, convert to nsIContent
  aRange->GetEndParent(getter_AddRefs(endDOM));
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

NS_IMETHODIMP nsGeneratedContentIterator::Init(nsIFocusTracker *aTracker, nsIDOMRange* aRange, PRBool aSelectBefore, PRBool aSelectAfter)
{
  mTracker = aTracker;
  mSelectBefore = aSelectBefore;
  mSelectAfter = aSelectAfter;
  return Init(aRange);
}

NS_IMETHODIMP nsGeneratedContentIterator::Init(nsIFocusTracker *aTracker, nsIContent* aRange, PRBool aSelectBefore, PRBool aSelectAfter)
{
  mTracker = aTracker;
  mSelectBefore = aSelectBefore;
  mSelectAfter = aSelectAfter;
  return Init(aRange);
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
}

nsCOMPtr<nsIContent> nsGeneratedContentIterator::GetDeepFirstChild(nsCOMPtr<nsIContent> aRoot)
{
  nsCOMPtr<nsIContent> deepFirstChild;
  nsCOMPtr<nsIContent> cN = aRoot;
  nsCOMPtr<nsIContent> cChild;
  
  if (aRoot) 
  {  
    if (mTracker)
    {
      nsFrameState frameState;
      nsIFrame *genFrame;
      result = mTracker->GetPrimaryFrameFor(aRoot, &genFrame);
      if (NS_FAILED(result))
        return result;
      if (!genFrame)
        return NS_ERROR_FAILURE;
      nsCOMPtr<nsIPresContext> context;
      result = mTracker->GetPresContext(getter_AddRefs(context));
      if (NS_FAILED(result) || !context)
        return result?result:NS_ERROR_FAILURE;
      result = genFrame->FirstChild(context,nsnull,&genFrame);
      if (NS_FAILED(result) || !genFrame)
        return NS_OK;//fine nothing to do here
      //we SHOULD now have a generated content frame if one exists. we need to check the flag for gen content
      result = genFrame->GetFrameState(&frameState);
      if (NS_FAILED(result))
        return result;

      if (frameState & NS_FRAME_GENERATED_CONTENT) 
      {
        result = genFrame->GetContent(getter_AddRefs(cChild));
      }
    }
    if (!cChild)
      cN->ChildAt(0,*getter_AddRefs(cChild));
    while ( cChild )
    {
      cN = cChild;

    if (mTracker)
    {
      nsFrameState frameState;
      nsIFrame *genFrame;
      result = mTracker->GetPrimaryFrameFor(cN, &genFrame);
      if (NS_FAILED(result))
        return result;
      if (!genFrame)
        return NS_ERROR_FAILURE;
      nsCOMPtr<nsIPresContext> context;
      result = mTracker->GetPresContext(getter_AddRefs(context));
      if (NS_FAILED(result) || !context)
        return result?result:NS_ERROR_FAILURE;
      result = genFrame->FirstChild(context,nsnull,&genFrame);
      if (NS_FAILED(result) || !genFrame)
        return NS_OK;//fine nothing to do here
      //we SHOULD now have a generated content frame if one exists. we need to check the flag for gen content
      result = genFrame->GetFrameState(&frameState);
      if (NS_FAILED(result))
        return result;

      if (frameState & NS_FRAME_GENERATED_CONTENT) 
      {
        result = genFrame->GetContent(getter_AddRefs(cChild));
      }


      if (!cChild)
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
  
    cN->ChildCount(numChildren);

    while ( numChildren )
    {
      cN->ChildAt(--numChildren,*getter_AddRefs(cChild));
      if (cChild)
      {
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
  else if (parent != mCommonParent)
  {
    return GetNextSibling(parent, aSibling);
  }
  else
  {
    *aSibling = nsCOMPtr<nsIContent>();
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
  else if (parent != mCommonParent)
  {
    return GetPrevSibling(parent, aSibling);
  }
  else
  {
    *aSibling = nsCOMPtr<nsIContent>();
  }
  
  return NS_OK;
}

nsresult nsGeneratedContentIterator::NextNode(nsCOMPtr<nsIContent> *ioNextNode)
{
  if (!ioNextNode)
    return NS_ERROR_NULL_POINTER;
    
  if (mPre)  // if we are a Pre-order iterator, use pre-order
  {
    nsCOMPtr<nsIContent> cN = *ioNextNode;
    nsCOMPtr<nsIContent> cFirstChild;
    PRInt32 numChildren;
  
    cN->ChildCount(numChildren);
  
    // if it has children then next node is first child
    if (numChildren)
    {
      if (NS_FAILED(cN->ChildAt(0,*getter_AddRefs(cFirstChild))))
        return NS_ERROR_FAILURE;
      if (!cFirstChild)
        return NS_ERROR_FAILURE;
      *ioNextNode = cFirstChild;
      return NS_OK;
    }
  
    // else next sibling is next
    return GetNextSibling(cN, ioNextNode);
  }
  else  // post-order
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
  
    // else it's the parent
    *ioNextNode = parent;
  }
  return NS_OK;
}

nsresult nsGeneratedContentIterator::PrevNode(nsCOMPtr<nsIContent> *ioNextNode)
{
  if (!ioNextNode)
    return NS_ERROR_NULL_POINTER;
   
  if (mPre)  // if we are a Pre-order iterator, use pre-order
  {
    nsCOMPtr<nsIContent> cN = *ioNextNode;
    nsCOMPtr<nsIContent> cSibling;
    nsCOMPtr<nsIContent> parent;
    PRInt32              indx;
  
    // get prev sibling if there is one
    if (NS_FAILED(cN->GetParent(*getter_AddRefs(parent))))
      return NS_ERROR_FAILURE;
    if (!parent || NS_FAILED(parent->IndexOf(cN, indx)))
    {
      // a little noise to catch some iterator usage bugs.
      NS_NOTREACHED("nsGeneratedContentIterator::PrevNode() : no parent found");
      return NS_ERROR_FAILURE;
    }
    if (indx && NS_SUCCEEDED(parent->ChildAt(--indx,*getter_AddRefs(cSibling))) && cSibling)
    {
      // prev node is siblings "deep right" child
      *ioNextNode = GetDeepLastChild(cSibling); 
      return NS_OK;
    }
  
    // else it's the parent
    *ioNextNode = parent;
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
  if (mFirst == mCurNode) 
    return NS_OK;
  mCurNode = mFirst;
  return NS_OK;
}


nsresult nsGeneratedContentIterator::Last()
{
  if (!mLast) 
    return NS_ERROR_FAILURE;
  mIsDone = PR_FALSE;
  if (mLast == mCurNode) 
    return NS_OK;
  mCurNode = mLast;
  return NS_OK;
}


nsresult nsGeneratedContentIterator::Next()
{
  if (mIsDone) 
    return NS_OK;
  if (!mCurNode) 
    return NS_OK;
  if (mCurNode == mLast) 
  {
    mIsDone = PR_TRUE;
    return NS_OK;
  }
  
  return NextNode(&mCurNode);
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
  
  return PrevNode(&mCurNode);
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
  return mCurNode->QueryInterface(NS_GET_IID(nsIContent), (void**) aNode);
}


