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

#ifndef nsIDOMIterators_h__
#define nsIDOMIterators_h__

#include "nsDOM.h"
#include "nsISupports.h"

// forward declaration
class nsIDOMNode;

#define NS_IDOMNODEITERATOR_IID \
{ /* 8f6bca75-ce42-11d1-b724-00600891d8c9 */ \
0x8f6bca75, 0xce42, 0x11d1, \
  {0xb7, 0x24, 0x00, 0x60, 0x08, 0x91, 0xd8, 0xc9} }

/**
 * A NodeIterator is a very simple iterator class that can be used to provide 
 * a simple linear view of the document heirarchy. 
 */
class nsIDOMNodeIterator : public nsISupports {
public:
  /**
   * This method set or unsets some iterator state that controls the 
   * filter to apply when traversing the document tree. 
   *
   * @param aFilter [in]    An integer that uniquely indicates what the filter 
   *                        operation will be operating against.
   * @param aFilterOn [in]  When true, this flag states that the specified item is 
   *                        to be filtered, otherwise, it will not be
   * @return <b>NS_OK</b>   iff the function succeeds, otherwise an error code
   */
  NS_IMETHOD SetFilter(PRInt32 aFilter, PRBool aFilterOn) = 0;

  /**
   * This method returns the number of items that will be iterated over if the 
   * iterator is started at the beginning, and getNextNode() is called repeatedly 
   * until the end of the sequence is encountered. 
   *
   * @param aLength [out]   This method will always return the correct number of items 
   *                        in a single threaded environment, but may return misleading 
   *                        results in multithreaded, multiuser situations.
   * @return <b>NS_OK</b>   iff the function succeeds, otherwise an error code
   */
  NS_IMETHOD GetLength(PRUint32 *aLength) = 0;

  /**
   * This method returns the Node over which the iterator currentl rests. 
   *
   * @param aNode [out]   This method will return the Node at the current position in the interation.
   * @return <b>NS_OK</b> iff the function succeeds, otherwise an error code
   */
  NS_IMETHOD GetCurrentNode(nsIDOMNode **aNode) = 0;

  /**
   * This method alters the internal state of the iterator such that the node it 
   * references is the next in the sequence the iterator is presenting relative 
   * to the current position. When filtering, this will skip any items being filtered. 
   *
   * @param aNode [out]   This method returns the node that has been traversed to, 
   *                      or null when it is not possible to iterate any further.
   * @return <b>NS_OK</b> iff the function succeeds, otherwise an error code
   */
  NS_IMETHOD GetNextNode(nsIDOMNode **aNode) = 0;

  /**
   * This method alters the internal state of the iterator such that the node it 
   * references is the previous node in the sequence the iterator is presenting relative 
   * to the current position. When filtering, this will skip any items being filtered.
   *
   * @param aNode [out]   This method returns the node that has been traversed to, or 
   *                      null when it is not possible to iterate any further.
   * @return <b>NS_OK</b> iff the function succeeds, otherwise an error code
   */
  NS_IMETHOD GetPreviousNode(nsIDOMNode **aNode) = 0;

  /**
   * This method alters the internal state of the iterator such that the node it 
   * references is the first node in the sequence the iterator is presenting relative 
   * to the current position. When filtering, this will skip any items being filtered. 
   *
   * @param aNode [out]    This method will only return null when there are no items to iterate over.
   * @return <b>NS_OK</b> iff the function succeeds, otherwise an error code
   */
  NS_IMETHOD ToFirst(nsIDOMNode **aNode) = 0;

  /**
   * This method alters the internal state of the iterator such that the node it 
   * references is the last node in the sequence the iterator is presenting relative 
   * to the current position. When filtering, this will skip any items being filtered.
   *
   * @param aNode [out]    This method will only return null when there are no items to iterate over.
   * @return <b>NS_OK</b> iff the function succeeds, otherwise an error code
   */
  NS_IMETHOD ToLast(nsIDOMNode **aNode) = 0;

  /**
   * This method alters the internal state of the iterator such that the node it 
   * references is the Nth node in the sequence the iterator is presenting relative 
   * to the current position. When filtering, this will skip any items being filtered.
   *
   * @param aNth [in]     The position to move to
   * @param aNode [out]   This method will return null when position specified is out 
   *                      of the range of legal values.
   * @return <b>NS_OK</b> iff the function succeeds, otherwise an error code
   */
  NS_IMETHOD MoveTo(int aNth, nsIDOMNode **aNode) = 0;
};

#define NS_IDOMTREEITERATOR_IID \
{ /* 8f6bca76-ce42-11d1-b724-00600891d8c9 */ \
0x8f6bca76, 0xce42, 0x11d1, \
  {0xb7, 0x24, 0x00, 0x60, 0x08, 0x91, 0xd8, 0xc9} }

class nsIDOMTreeIterator : public nsIDOMNodeIterator {
public:
  NS_IMETHOD NumChildren(PRUint32 *aLength) = 0;
  NS_IMETHOD NumPreviousSiblings(PRUint32 *aLength) = 0;
  NS_IMETHOD NumNextSiblings(PRUint32 *aLength) = 0;
  NS_IMETHOD ToParent(nsIDOMNode **aNode) = 0;
  NS_IMETHOD ToPreviousSibling(nsIDOMNode **aNode) = 0;
  NS_IMETHOD ToNextSibling(nsIDOMNode **aNode) = 0;
  NS_IMETHOD ToFirstChild(nsIDOMNode **aNode) = 0;
  NS_IMETHOD ToLastChild(nsIDOMNode **aNode) = 0;
  NS_IMETHOD ToNthChild(nsIDOMNode **aNode) = 0;
};

#endif // nsIDOMIterators_h__

