#include "nsICollection.h"
#include "nsIFactory.h"
#include "nsIEnumerator.h"
#include "nsIDOMRange.h"
#include "nsISelection.h"

static NS_DEFINE_IID(kIRangeListIterator, NS_IENUMERATOR_IID);
static NS_DEFINE_IID(kICollectionIID, NS_ICOLLECTION_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kISelectionIID, NS_ISELECTION_IID);


class nsRangeListIterator;

class nsRangeList : public nsICollection
{
public:
/*BEGIN nsICollection interfaces
see the nsICollection for more details*/

  /*interfaces for addref and release and queryinterface*/
  
  NS_DECL_ISUPPORTS
  virtual nsresult AddItem(nsISupports *aRange);

  virtual nsresult RemoveItem(nsISupports *aRange);

  virtual nsresult Clear();
/*END nsICollection interfaces*/
/*BEGIN nsISelection interfaces*/
  virtual nsresult HandleKeyEvent(nsGUIEvent *aGuiEvent, nsIFrame *aFrame);
/*END nsISelection interfacse*/
  nsRangeList();
  virtual ~nsRangeList();

private:
  friend class nsRangeListIterator;

  void ResizeBuffer(PRUint32 aNewBufSize);
  nsIDOMRange **mRangeArray;
  PRUint32 mCount;
  PRUint32 mBufferSize;
};


class nsRangeListIterator : public nsIEnumerator
{
public:
/*BEGIN nsIEnumerator interfaces
see the nsIEnumerator for more details*/

  NS_DECL_ISUPPORTS

  virtual nsresult First();

  virtual nsresult Last();

  virtual nsresult Next();

  virtual nsresult Prev();

  virtual nsresult CurrentItem(nsISupports **aRange);

  virtual nsresult IsDone();

/*END nsIEnumerator interfaces*/

private:
  friend class nsRangeList;
  nsRangeListIterator(nsRangeList *);
  virtual ~nsRangeListIterator();

  nsRangeList *mRangeList;
  PRUint32 mIndex;
};



nsresult NS_NewRangeList(nsICollection **aRangeList)
{
  nsRangeList *rlist = new nsRangeList;
  if (!rlist)
    return NS_ERROR_OUT_OF_MEMORY;
  nsresult result = rlist->QueryInterface(kICollectionIID , (void **)aRangeList);
  if (!NS_SUCCEEDED(result))
  {
    delete rlist;
  }
  return result;
}




///////////BEGIN nsRangeListIterator methods

nsRangeListIterator::nsRangeListIterator(nsRangeList *aList)
:mIndex(0)
{
  if (!aList)
  {
    NS_NOTREACHED("nsRangeList");
    return;
  }
  mRangeList = aList;
  NS_INIT_REFCNT();
}



nsRangeListIterator::~nsRangeListIterator()
{
}



////////////END nsRangeListIterator methods

////////////BEGIN nsIRangeListIterator methods



nsresult
nsRangeListIterator::Next()
{
  if (mIndex < mRangeList->mCount -1 )
  {
    mIndex++;
    return NS_OK;
  }
  mIndex = mRangeList->mCount -1;
  return NS_ERROR_FAILURE;
}



nsresult
nsRangeListIterator::Prev()
{
  if (mIndex > 0 )
  {
    mIndex--;
    return NS_OK;
  }
  mIndex = 0;
  return NS_ERROR_FAILURE;
}



nsresult
nsRangeListIterator::First()
{
  if (!mRangeList)
    return NS_ERROR_NULL_POINTER;
  mIndex = 0;
  return NS_OK;
}



nsresult
nsRangeListIterator::Last()
{
  if (!mRangeList)
    return NS_ERROR_NULL_POINTER;
  mIndex = mRangeList->mCount -1;
  return NS_OK;
}



nsresult 
nsRangeListIterator::CurrentItem(nsISupports **aItem)
{
  if (!aItem)
    return NS_ERROR_NULL_POINTER;
  if (mIndex >=0 && mIndex< mRangeList->mCount)
  {
    return mRangeList->mRangeArray[mIndex]->QueryInterface(kISupportsIID, (void **)aItem);
  }
  return NS_ERROR_FAILURE;
}



nsresult
nsRangeListIterator::IsDone()
{
  if ((mIndex == mRangeList->mCount -1) || !mRangeList->mCount) { //empty lists are always done
    return NS_OK;
  }
  else{
    return NS_COMFALSE;
  }
  return NS_OK;
}



NS_IMPL_ADDREF(nsRangeListIterator)

NS_IMPL_RELEASE(nsRangeListIterator)



nsresult
nsRangeListIterator::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void*)(nsISupports*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kICollectionIID)) {
    *aInstancePtr = (void *)mRangeList;
    NS_ADDREF(mRangeList);
    return NS_OK;
  }
  if (aIID.Equals(kIRangeListIterator)) {
    *aInstancePtr = (void*)(nsIEnumerator*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

////////////END nsIRangeListIterator methods

////////////BEGIN nsRangeList methods



nsRangeList::nsRangeList()
:mCount(0),mRangeArray(NULL),mBufferSize(0)
{
  NS_INIT_REFCNT();
}



nsRangeList::~nsRangeList()
{
  for (PRUint32 i=0;i < mCount; i++)
  {
    NS_IF_RELEASE(mRangeArray[i]);
  }
  delete []mRangeArray;
  mCount = 0;
  mBufferSize = 0;
}



void
nsRangeList::ResizeBuffer(PRUint32 aNewBufSize)
{
  if (aNewBufSize <=mCount)
  {
    NS_NOTREACHED("ResizeBuffer");
    return;
  }
  if (aNewBufSize <= mBufferSize)
    return;
  nsIDOMRange **rangeArray = new nsIDOMRange *[aNewBufSize];
  if (!rangeArray)
  {
    NS_NOTREACHED("nsRangeList::ResizeBuffer");
    return ;
  }
  memcpy(rangeArray,mRangeArray, aNewBufSize * sizeof (nsIDOMRange *));
  delete [] mRangeArray;
  mRangeArray = rangeArray;
  mBufferSize = aNewBufSize;
}



//END nsRangeList methods

//BEGIN nsICollection interface implementations


NS_IMPL_ADDREF(nsRangeList)

NS_IMPL_RELEASE(nsRangeList)


nsresult
nsRangeList::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void*)(nsISupports*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kICollectionIID)) {
    *aInstancePtr = (void*)(nsICollection *)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(  kISelectionIID)) {
    *aInstancePtr = (void*)(nsISelection *)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIRangeListIterator)) {
    nsRangeListIterator *iterator =  new nsRangeListIterator(this);
    iterator->QueryInterface(kIRangeListIterator,aInstancePtr);
    *aInstancePtr = (void*)(iterator);
    return NS_OK;
  }
  return NS_NOINTERFACE;
}



nsresult
nsRangeList::AddItem(nsISupports *aItem)
{
  if (!aItem)
    return NS_ERROR_NULL_POINTER;
  ResizeBuffer(++mCount);
  aItem->QueryInterface(kISupportsIID,(void **)mRangeArray[mCount -1]);
  return NS_OK;
}



nsresult
nsRangeList::RemoveItem(nsISupports *aItem)
{
  if (!aItem)
    return NS_ERROR_NULL_POINTER;
  for (PRUint32 i = 0; i < mCount;i++)
  {
    if (mRangeArray[i] == aItem)
    {
      NS_RELEASE(mRangeArray[i]);
      mRangeArray[i] = mRangeArray[--mCount];
      return NS_OK;
    }
  }
  return NS_COMFALSE;
}



nsresult
nsRangeList::Clear()
{
  for (PRUint32 i = 0; i < mCount;i++)
  {
    NS_RELEASE(mRangeArray[i]);
  }
  if (mRangeArray)
    delete []mRangeArray;
  mRangeArray = nsnull;
  return NS_OK;
}



//END nsICollection methods

//BEGIN nsISelection methods



nsresult
nsRangeList::HandleKeyEvent(nsGUIEvent *aGuiEvent, nsIFrame *aFrame)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


//END nsISelection methods
