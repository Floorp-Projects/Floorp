/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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

#ifndef __nsContentIterator_h___
#define __nsContentIterator_h___

//#include "nsIEnumerator.h"
#include "nsISupports.h"

class nsIContent;
class nsIDOMRange;

/*
 *  A simple iterator class for traversing the content in "left to right" order
 */
class nsContentIterator // : public nsIEnumerator
{
public:


  // constructors/destructor ------------------------------------
   
  // Creates an iterator for the subtree rooted by the node aRoot
  nsContentIterator(nsIContent* aRoot);

  // Creates an iterator for the subtree defined by the range aRange
  nsContentIterator(nsIDOMRange* aRange);
    
  virtual ~nsContentIterator();


  // nsIEnumertor interface methods ------------------------------

  virtual nsresult First();

  virtual nsresult Last();
  
  virtual nsresult Next();

  virtual nsresult Prev();

  virtual nsresult CurrentNode(nsIContent **aItem);

  virtual nsresult IsDone();
  
protected:

  static nsIContent* GetDeepFirstChild(nsIContent* aRoot);

  nsIContent *mCurNode;
  nsIContent *mFirst;
  nsIContent *mLast;

  PRBool mIsDone;
  
private:

  // no copy's or assigns  FIX ME
  nsContentIterator(const nsContentIterator&);
  nsContentIterator& operator=(const nsContentIterator&);

};


#endif // __nsContentIterator_h___

