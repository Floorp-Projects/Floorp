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

class nsIDOMNodeIterator : public nsISupports {
public:
  virtual nsresult            SetFilter(PRInt32 aFilter, PRBool aFilterOn) = 0;
  virtual nsresult            GetLength(PRUint32 *aLength) = 0;
  virtual nsresult            GetCurrentNode(nsIDOMNode **aNode) = 0;
  virtual nsresult            GetNextNode(nsIDOMNode **aNode) = 0;
  virtual nsresult            GetPreviousNode(nsIDOMNode **aNode) = 0;
  virtual nsresult            ToFirst(nsIDOMNode **aNode) = 0;
  virtual nsresult            ToLast(nsIDOMNode **aNode) = 0;
  virtual nsresult            MoveTo(int aNth, nsIDOMNode **aNode) = 0;
};

#define NS_IDOMTREEITERATOR_IID \
{ /* 8f6bca76-ce42-11d1-b724-00600891d8c9 */ \
0x8f6bca76, 0xce42, 0x11d1, \
  {0xb7, 0x24, 0x00, 0x60, 0x08, 0x91, 0xd8, 0xc9} }

class nsIDOMTreeIterator : public nsIDOMNodeIterator {
public:
  virtual nsresult            NumChildren(PRUint32 *aLength) = 0;
  virtual nsresult            NumPreviousSiblings(PRUint32 *aLength) = 0;
  virtual nsresult            NumNextSiblings(PRUint32 *aLength) = 0;
  virtual nsresult            ToParent(nsIDOMNode **aNode) = 0;
  virtual nsresult            ToPreviousSibling(nsIDOMNode **aNode) = 0;
  virtual nsresult            ToNextSibling(nsIDOMNode **aNode) = 0;
  virtual nsresult            ToFirstChild(nsIDOMNode **aNode) = 0;
  virtual nsresult            ToLastChild(nsIDOMNode **aNode) = 0;
  virtual nsresult            ToNthChild(nsIDOMNode **aNode) = 0;
};

#endif // nsIDOMIterators_h__

