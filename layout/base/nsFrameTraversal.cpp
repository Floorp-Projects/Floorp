/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#include "nsCOMPtr.h"
#include "nsLayoutAtoms.h"

#include "nsFrameTraversal.h"
#include "nsFrameList.h"


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
  nsLeafIterator(nsIPresContext* aPresContext, nsIFrame *start);
  void SetExtensive(PRBool aExtensive) {mExtensive = aExtensive;}
  PRBool GetExtensive(){return mExtensive;}
private :
  
  NS_IMETHOD Next();

  NS_IMETHOD Prev();

  nsIPresContext* mPresContext;
  PRBool mExtensive;
};

/************IMPLEMENTATIONS**************/

nsresult
NS_NewFrameTraversal(nsIBidirectionalEnumerator **aEnumerator,
                     nsTraversalType aType,
                     nsIPresContext* aPresContext,
                     nsIFrame *aStart)
{
  if (!aEnumerator || !aStart)
    return NS_ERROR_NULL_POINTER;
  switch(aType)
  {
  case LEAF: {
    nsLeafIterator *trav = new nsLeafIterator(aPresContext, aStart);
    if (!trav)
      return NS_ERROR_OUT_OF_MEMORY;
    *aEnumerator = NS_STATIC_CAST(nsIBidirectionalEnumerator*, trav);
    NS_ADDREF(trav);
    trav->SetExtensive(PR_FALSE);
             }
    break;
  case EXTENSIVE:{
    nsLeafIterator *trav = new nsLeafIterator(aPresContext, aStart);
    if (!trav)
      return NS_ERROR_OUT_OF_MEMORY;
    *aEnumerator = NS_STATIC_CAST(nsIBidirectionalEnumerator*, trav);
    NS_ADDREF(trav);
    trav->SetExtensive(PR_TRUE);
             }
    break;
#if 0
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
NS_IMPL_ISUPPORTS2(nsFrameIterator, nsIEnumerator, nsIBidirectionalEnumerator)

nsFrameIterator::nsFrameIterator()
{
  mOffEdge = 0;
  mLast = nsnull;
  mCurrent = nsnull;
  mStart = nsnull;
  NS_INIT_REFCNT();
}



NS_IMETHODIMP
nsFrameIterator::CurrentItem(nsISupports **aItem)
{
  if (!aItem)
    return NS_ERROR_NULL_POINTER;
  *aItem = mCurrent;
  if (mOffEdge)
    return NS_ENUMERATOR_FALSE;
  return NS_OK;
}



NS_IMETHODIMP
nsFrameIterator::IsDone()//what does this mean??off edge? yes
{
  if (mOffEdge != 0)
    return NS_OK;
  return NS_ENUMERATOR_FALSE;
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


nsLeafIterator::nsLeafIterator(nsIPresContext* aPresContext, nsIFrame *aStart)
  : mPresContext(aPresContext)
{
  setStart(aStart);
  setCurrent(aStart);
  setLast(aStart);
}

static PRBool
IsRootFrame(nsIFrame* aFrame)
{
  nsCOMPtr<nsIAtom>atom;
  
  aFrame->GetFrameType(getter_AddRefs(atom));
  return (atom.get() == nsLayoutAtoms::canvasFrame) ||
         (atom.get() == nsLayoutAtoms::rootFrame);
}

NS_IMETHODIMP
nsLeafIterator::Next()
{
//recursive-oid method to get next frame
  nsIFrame *result = nsnull;
  nsIFrame *parent = getCurrent();
  if (!parent)
    parent = getLast();
  if (!mExtensive)
  {
     while(NS_SUCCEEDED(parent->FirstChild(mPresContext, nsnull,&result)) && result)
    {
      parent = result;
    }
  }
  if (parent != getCurrent())
  {
    result = parent;
  }
  else {
    while(parent && !IsRootFrame(parent)) {
      if (NS_SUCCEEDED(parent->GetNextSibling(&result)) && result){
        parent = result;
        while(NS_SUCCEEDED(parent->FirstChild(mPresContext, nsnull,&result)) && result)
        {
          parent = result;
        }
        result = parent;
        break;
      }
      else
      {
        parent->GetParent(&result);
        if (!result || IsRootFrame(result)) {
          result = nsnull;
          break;
        }
        else 
        {
          parent = result;
          if (mExtensive)
            break;
        }
      }
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
      NS_SUCCEEDED(grandParent->FirstChild(mPresContext, nsnull,&result))){
      nsFrameList list(result);
      result = list.GetPrevSiblingFor(parent);
      if (result){
        parent = result;
        while(NS_SUCCEEDED(parent->FirstChild(mPresContext, nsnull,&result)) && result){
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
      {
        parent = result;
        if (mExtensive)
          break;
      }
    }
    else{
      setLast(parent);
      result = nsnull;
      break;
    }
  }

  setCurrent(result);
  if (!result)
    setOffEdge(-1);
  return NS_OK;
}
