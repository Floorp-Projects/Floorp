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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef nsAVLTree_h___
#define nsAVLTree_h___


#include "nscore.h"


enum eAVLStatus {eAVL_unknown,eAVL_ok,eAVL_fail,eAVL_duplicate};


struct nsAVLNode;

/**
 * 
 * @update	gess12/26/98
 * @param anObject1 is the first object to be compared
 * @param anObject2 is the second object to be compared
 * @return -1,0,1 if object1 is less, equal, greater than object2
 */
class NS_COM nsAVLNodeComparitor {
public:
  virtual PRInt32 operator()(void* anItem1,void* anItem2)=0;
}; 

class NS_COM nsAVLNodeFunctor {
public:
  virtual void* operator()(void* anItem)=0;
};

class NS_COM nsAVLTree {
public:
              nsAVLTree(nsAVLNodeComparitor& aComparitor, nsAVLNodeFunctor* aDeallocator);
              ~nsAVLTree(void);

  PRBool      operator==(const nsAVLTree& aOther) const;
  PRInt32     GetCount(void) const {return mCount;}

              //main functions...
  eAVLStatus  AddItem(void* anItem);
  eAVLStatus  RemoveItem(void* anItem);
  void*       FindItem(void* anItem) const;
  void        ForEach(nsAVLNodeFunctor& aFunctor) const;
  void        ForEachDepthFirst(nsAVLNodeFunctor& aFunctor) const;
  void*       FirstThat(nsAVLNodeFunctor& aFunctor) const;

protected: 

  nsAVLNode*  mRoot;
  PRInt32     mCount;
  nsAVLNodeComparitor&  mComparitor;
  nsAVLNodeFunctor*     mDeallocator;
};


#endif /* nsAVLTree_h___ */

