/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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

#ifndef __nsIContentIterator_h___
#define __nsIContentIterator_h___

#include "nsISupports.h"


class nsIFocusTracker;
class nsIContent;
class nsIDOMRange;

#define NS_ICONTENTITERTOR_IID \
{0xa6cf90e4, 0x15b3, 0x11d2,   \
{0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32} }

// {B4BC9F63-D9BA-11d3-9938-00108301233C}
#define NS_IGENERATEDCONTENTITERTOR_IID \
{ 0xb4bc9f63, 0xd9ba, 0x11d3,  \
{ 0x99, 0x38, 0x0, 0x10, 0x83, 0x1, 0x23, 0x3c } }


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

class nsIPresShell;

class nsIGeneratedContentIterator : public nsISupports {
public:

  static const nsIID& GetIID() { static nsIID iid = NS_IGENERATEDCONTENTITERTOR_IID; return iid; }

  /* Initializes an iterator for the subtree rooted by the node aRoot
   */
  NS_IMETHOD Init(nsIPresShell *aShell, nsIDOMRange* aRange)=0;

  NS_IMETHOD Init(nsIPresShell *aShell, nsIContent* aContent)=0;
};


#endif // __nsIContentIterator_h___

