#include "nsFrameTraversal.h"
#include "nsFrameList.h"


static NS_DEFINE_IID(kIEnumeratorIID, NS_IENUMERATOR_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);


class nsFrameIterator: public nsIEnumerator
{
public:
  NS_DECL_ISUPPORTS

  virtual nsresult First();

  virtual nsresult Last();
  
  virtual nsresult Next()=0;

  virtual nsresult Prev()=0;

  virtual nsresult CurrentItem(nsISupports **aItem);

  virtual nsresult IsDone();//what does this mean??off edge? yes

  nsFrameIterator();
protected:
  void      setCurrent(nsIFrame *aFrame){mCurrent = aFrame;}
  nsIFrame *getCurrent(){return mCurrent;}
  void      setStart(nsIFrame *aFrame){mStart = aFrame;}
  nsIFrame *getStart(){return mStart;}
  nsIFrame *getLast(){return mLast;}
  void      setLast(nsIFrame *aFrame){mLast = aFrame;}
  PRInt8    getOffEdge(){return mOffEdge;}
  void      setOffEdge(PRInt8 aOffEdge){mOffEdge = aOffEdge;}
private:
  nsIFrame *mStart;
  nsIFrame *mCurrent;
  nsIFrame *mLast; //the last one that was in current;
  PRInt8    mOffEdge; //0= no -1 to far prev, 1 to far next;
};


/*
class nsFastFrameIterator: public nsFrameIterator
{
  nsFastFrameIterator(nsIFrame *start);
private :
  
  virtual nsresult Next();

  virtual nsresult Prev();

}
*/

class nsLeafIterator: public nsFrameIterator
{
public:
  nsLeafIterator(nsIFrame *start);
private :
  
  virtual nsresult Next();

  virtual nsresult Prev();

};

/************IMPLEMENTATIONS**************/

nsresult
NS_NewFrameTraversal(nsIEnumerator **aEnumerator, nsTraversalType aType, nsIFrame *aStart)
{
  if (!aEnumerator || !aStart)
    return NS_ERROR_NULL_POINTER;
  switch(aType)
  {
  case LEAF: {
    nsLeafIterator *trav = new nsLeafIterator(aStart);
    if (!trav)
      return NS_ERROR_OUT_OF_MEMORY;
    *aEnumerator = (nsIEnumerator *)trav;
    NS_ADDREF(trav);
             }
    break;
#if 0
  case EXTENSIVE:{
    nsExtensiveTraversal *trav = new nsExtensiveTraversal(aStart);
    if (!trav)
      return NS_ERROR_NOMEMORY;
    *aEnumerator = (nsIEnumerator *)trav;
    NS_ADDREF(trav);
                 }
    break;
  case FASTEST:{
    nsFastestTraversal *trav = new nsFastestTraversal(aStart);
    if (!trav)
      return NS_ERROR_NOMEMORY;
    *aEnumerator = (nsIEnumerator *)trav;
    NS_ADDREF(trav);
               }
#endif
  default:
    return NS_ERROR_NOT_IMPLEMENTED;
    break;
  }
  return NS_OK;
}



/*********nsFrameIterator************/
NS_IMPL_ADDREF(nsFrameIterator)

NS_IMPL_RELEASE(nsFrameIterator)



nsFrameIterator::nsFrameIterator()
{
  mOffEdge = 0;
  mLast = nsnull;
  mCurrent = nsnull;
  mStart = nsnull;
  NS_INIT_REFCNT();
}



nsresult
nsFrameIterator::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void *)(nsISupports *)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIEnumeratorIID)) {
    *aInstancePtr = (void*)(nsIEnumerator*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}



nsresult
nsFrameIterator::CurrentItem(nsISupports **aItem)
{
  if (!aItem)
    return NS_ERROR_NULL_POINTER;
  *aItem = mCurrent;
  return NS_OK;
}



nsresult
nsFrameIterator::IsDone()//what does this mean??off edge? yes
{
  if (mOffEdge != 0)
    return NS_OK;
  return NS_COMFALSE;
}



nsresult
nsFrameIterator::First()
{
  mCurrent = mStart;
  return NS_OK;
}



nsresult
nsFrameIterator::Last()
{
  return NS_ERROR_FAILURE;
}



/*********LEAFITERATOR**********/


nsLeafIterator::nsLeafIterator(nsIFrame *aStart)
{
  setStart(aStart);
  setCurrent(aStart);
  setLast(aStart);
}



nsresult
nsLeafIterator::Next()
{
//recursive-oid method to get next frame
  nsIFrame *result;
  nsIFrame *parent = getCurrent();
  if (!parent)
    parent = getLast();
  while(NS_SUCCEEDED(parent->FirstChild(nsnull,&result)) && result)
  {
    parent = result;
  }
  if (parent != getCurrent())
  {
    result = parent;
  }
  else {
    while(parent){
      if (NS_SUCCEEDED(parent->GetNextSibling(&result)) && result){
        parent = result;
        while(NS_SUCCEEDED(parent->FirstChild(nsnull,&result)) && result)
        {
          parent = result;
        }
        result = parent;
        break;
      }
      else
        if (NS_FAILED(parent->GetParent(&result)) || !result){
          result = nsnull;
          break;
        }
        else 
          parent = result;
    }
  }
  setCurrent(result);
  if (!result)
    setOffEdge(1);
  return nsnull;
}




nsresult
nsLeafIterator::Prev()
{
//recursive-oid method to get prev frame
  nsIFrame *result;
  nsIFrame *parent = getCurrent();
  if (!parent)
    return NS_ERROR_FAILURE;

  parent = getLast();
  while(parent){
    nsIFrame *grandParent;
    if (NS_SUCCEEDED(parent->GetParent(&grandParent)) && grandParent){
      nsIFrame * grandFchild;
      if (NS_SUCCEEDED(grandParent->FirstChild(nsnull,&grandFchild)) && grandFchild){
        nsFrameList list(grandFchild);
        if (result = list.GetPrevSiblingFor(parent) ){
          parent = result;
          while(NS_SUCCEEDED(parent->FirstChild(nsnull,&result)) && result){
            parent = result;
            while(NS_SUCCEEDED(parent->GetNextSibling(&result)) && result){
              parent = result;
            }
          }
          result = parent;
          break;
        }
        else
          if (NS_FAILED(parent->GetParent(&result)) || !result){
            result = nsnull;
            break;
          }
          else 
            parent = result;
      }
    }
  }
  setCurrent(result);
  if (!result)
    setOffEdge(-1);
  return nsnull;
}
