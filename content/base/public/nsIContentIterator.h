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

#ifndef __nsIContentIterator_h___
#define __nsIContentIterator_h___

#include "nsISupports.h"

class nsIContent;
class nsIDOMRange;

#define NS_ICONTENTITERTOR_IID \
{0xa6cf90e4, 0x15b3, 0x11d2,   \
{0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32} }

class nsIContentIterator : public nsISupports {
public:

  static const nsIID& GetIID() { static nsIID iid = NS_ICONTENTITERTOR_IID; return iid; }

  /* Initializes an iterator for the subtree rooted by the node aRoot
   */
  NS_IMETHOD Init(nsIContent* aRoot)=0;

  /* Initializes an iterator for the subtree defined by the range aRange
   */
  NS_IMETHOD Init(nsIDOMRange* aRange)=0;
  
  /** First will reset the list. will return NS_FAILED if no items
   */
  NS_IMETHOD First()=0;

  /** Last will reset the list to the end. will return NS_FAILED if no items
   */
  NS_IMETHOD Last()=0;
  
  /** Next will advance the list. will return failed if allready at end
   */
  NS_IMETHOD Next()=0;

  /** Prev will decrement the list. will return failed if allready at beginning
   */
  NS_IMETHOD Prev()=0;

  /** CurrentItem will return the CurrentItem item it will fail if the list is empty
   *  @param aItem return value
   */
  NS_IMETHOD CurrentNode(nsIContent **aNode)=0;

  /** return if the collection is at the end.  that is the beginning following a call to Prev
   *  and it is the end of the list following a call to next
   *  @param aItem return value
   */
  NS_IMETHOD IsDone()=0;

  /** PositionAt will position the iterator to the supplied node
   */
  NS_IMETHOD PositionAt(nsIContent* aCurNode)=0;

  /** MakePre will make the iterator a pre-order iterator
   */
  NS_IMETHOD MakePre()=0;

  /** MakePost will make the iterator a post-order iterator
   */
  NS_IMETHOD MakePost()=0;

};


#endif // __nsIContentIterator_h___

