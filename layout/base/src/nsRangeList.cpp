#include "nsIRangeList.h"
#include "nsIFactory.h"


static NS_DEFINE_IID(kIRangeListIterator, NS_IRANGELISTITERATOR);
static NS_DEFINE_IID(kIRangeList, NS_IRANGELIST);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);


class nsRangeListIterator;

class nsRangeList : public nsIRangeList
{
public:
/*BEGIN nsIRangeList interfaces
see the nsIRangeList for more details*/

  /*interfaces for addref and release and queryinterface*/
  
  NS_DECL_ISUPPORTS
  virtual nsresult AddRange(nsIDOMRange *aRange);

  virtual nsresult RemoveRange(nsIDOMRange *aRange);

  virtual nsresult Clear();
/*END nsIRangeList interfaces*/

  nsRangeList();
  virtual ~nsRangeList();

private:
  friend class nsRangeListIterator;

  void ResizeBuffer(PRUint32 aNewBufSize);
  nsIDOMRange **mRangeArray;
  PRUint32 mCount;
  PRUint32 mBufferSize;
};


class nsRangeListIterator : public nsIRangeListIterator
{
public:
/*BEGIN nsIRangeListIterator interfaces
see the nsIRangeListIterator for more details*/

  NS_DECL_ISUPPORTS

  nsresult Next(nsIDOMRange **aRange);

  nsresult Prev(nsIDOMRange **aRange);

  nsresult Reset();

/*END nsIRangeListIterator interfaces*/

private:
  friend class nsRangeList;
  nsRangeListIterator(nsRangeList *);
  virtual ~nsRangeListIterator();

  nsRangeList *mRangeList;
  PRUint32 mIndex;
};



nsresult NS_NewRangeList(nsIRangeList **aRangeList)
{
  nsRangeList *rlist = new nsRangeList;
  if (!rlist)
    return NS_ERROR_OUT_OF_MEMORY;
  nsresult result = rlist->QueryInterface(kIRangeList , (void **)aRangeList);
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
}



nsRangeListIterator::~nsRangeListIterator()
{
}



////////////END nsRangeListIterator methods

////////////BEGIN nsIRangeListIterator methods



nsresult
nsRangeListIterator::Next(nsIDOMRange **aRange)
{
  if (!aRange)
    return NS_ERROR_NULL_POINTER;
  if (mIndex < mRangeList->mCount)
  {
    *aRange = mRangeList->mRangeArray[mIndex++];
    NS_ADDREF(*aRange);
    return NS_OK;
  }
  return NS_COMFALSE;
}



nsresult
nsRangeListIterator::Prev(nsIDOMRange **aRange)
{
  if (!aRange)
    return NS_ERROR_NULL_POINTER;
  if (mIndex > 0)
  {
    *aRange = mRangeList->mRangeArray[mIndex--];
    NS_ADDREF(*aRange);
    return NS_OK;
  }
  return NS_COMFALSE;
}



nsresult
nsRangeListIterator::Reset()
{
  if (!mRangeList)
    return NS_ERROR_NULL_POINTER;
  mIndex = 0;
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
  if (aIID.Equals(kIRangeList)) {
    *aInstancePtr = mRangeList;
    NS_ADDREF(mRangeList);
    return NS_OK;
  }
  if (aIID.Equals(kIRangeListIterator)) {
    *aInstancePtr = (void*)(nsIRangeListIterator*)this;
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

//BEGIN nsIRangeList interface implementations


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
  if (aIID.Equals(kIRangeList)) {
    *aInstancePtr = (void*)(nsIRangeList*)this;
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
nsRangeList::AddRange(nsIDOMRange *aRange)
{
  if (!aRange)
    return NS_ERROR_NULL_POINTER;
  ResizeBuffer(++mCount);
  mRangeArray[mCount -1] = aRange;
  NS_ADDREF(aRange);
  return NS_OK;
}



nsresult
nsRangeList::RemoveRange(nsIDOMRange *aRange)
{
  if (!aRange)
    return NS_ERROR_NULL_POINTER;
  for (PRUint32 i = 0; i < mCount;i++)
  {
    if (mRangeArray[i] == aRange)
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



//END nsIDOMRangeInterface methods

