/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#include "nsFrameTraversal.h"
#include "nsFrameList.h"


static NS_DEFINE_IID(kIEnumeratorIID, NS_IENUMERATOR_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);


class nsFrameIterator: public nsIBidirectionalEnumerator
{
public:
  NS_DECL_ISUPPORTS

  NS_IMETHOD First();

  NS_IMETHOD Last();
  
  NS_IMETHOD Next()=0;

  NS_IMETHOD Prev()=0;

  NS_IMETHOD CurrentItem(nsISupports **aItem);

  NS_IMETHOD IsDone();//what does this mean??off edge? yes

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
  
  NS_IMETHOD Next();

  NS_IMETHOD Prev();

};

/************IMPLEMENTATIONS**************/

nsresult
NS_NewFrameTraversal(nsIBidirectionalEnumerator **aEnumerator, nsTraversalType aType, nsIFrame *aStart)
{
  if (!aEnumerator || !aStart)
    return NS_ERROR_NULL_POINTER;
  switch(aType)
  {
  case LEAF: {
    nsLeafIterator *trav = new nsLeafIterator(aStart);
    if (!trav)
      return NS_ERROR_OUT_OF_MEMORY;
    *aEnumerator = NS_STATIC_CAST(nsIBidirectionalEnumerator*, trav);
    NS_ADDREF(trav);
             }
    break;
#if 0
  case EXTENSIVE:{
    nsExtensiveTraversal *trav = new nsExtensiveTraversal(aStart);
    if (!trav)
      return NS_ERROR_NOMEMORY;
    *aEnumerator = NS_STATIC_CAST(nsIBidirectionalEnumerator*, trav);
    NS_ADDREF(trav);
                 }
    break;
  case FASTEST:{
    nsFastestTraversal *trav = new nsFastestTraversal(aStart);
    if (!trav)
      return NS_ERROR_NOMEMORY;
    *aEnumerator = NS_STATIC_CAST(nsIBidirectionalEnumerator*, trav);
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



NS_IMETHODIMP
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
    *aInstancePtr = (void*)NS_STATIC_CAST(nsIEnumerator*, this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}



NS_IMETHODIMP
nsFrameIterator::CurrentItem(nsISupports **aItem)
{
  if (!aItem)
    return NS_ERROR_NULL_POINTER;
  *aItem = mCurrent;
  if (mOffEdge)
    return NS_COMFALSE;
  return NS_OK;
}



NS_IMETHODIMP
nsFrameIterator::IsDone()//what does this mean??off edge? yes
{
  if (mOffEdge != 0)
    return NS_OK;
  return NS_COMFALSE;
}



NS_IMETHODIMP
nsFrameIterator::First()
{
  mCurrent = mStart;
  return NS_OK;
}



NS_IMETHODIMP
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



NS_IMETHODIMP
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
  return NS_OK;
}




NS_IMETHODIMP
nsLeafIterator::Prev()
{
//recursive-oid method to get prev frame
  nsIFrame *result;
  nsIFrame *parent = getCurrent();
  if (!parent)
    parent = getLast();
  while(parent){
    nsIFrame *grandParent;
    if (NS_SUCCEEDED(parent->GetParent(&grandParent)) && grandParent &&
      NS_SUCCEEDED(grandParent->FirstChild(nsnull,&result))){
      nsFrameList list(result);
      result = list.GetPrevSiblingFor(parent);
      if (result){
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
      else if (NS_FAILED(parent->GetParent(&result)) || !result){
          result = nsnull;
          break;
      }
      else 
        parent = result;
    }
    else{
      setLast(parent);
      result = nsnull;
      break;
    }
  }


/*  while(parent){
    nsIFrame *grandParent;
    if (NS_SUCCEEDED(parent->GetParent(&grandParent)) && grandParent){
      nsIFrame * grandFchild;
      if (NS_SUCCEEDED(grandParent->FirstChild(nsnull,&grandFchild)) && grandFchild){
        nsFrameList list(grandFchild);
        if (nsnull != (result = list.GetPrevSiblingFor(parent)) ){
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
  */
  setCurrent(result);
  if (!result)
    setOffEdge(-1);
  return NS_OK;
}
