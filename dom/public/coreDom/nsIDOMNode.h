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

#ifndef nsIDOMNode_h__
#define nsIDOMNode_h__

#include "nsDOM.h"
#include "nsISupports.h"

// forward declaration
class nsIDOMNodeIterator;

#define NS_IDOMNODE_IID \
{ /* 8f6bca74-ce42-11d1-b724-00600891d8c9 */ \
0x8f6bca74, 0xce42, 0x11d1, \
{0xb7, 0x24, 0x00, 0x60, 0x08, 0x91, 0xd8, 0xc9} }

class nsIDOMNode : public nsISupports {
public:
  // NodeType
  enum NodeType {
    DOCUMENT             = 1,
    ELEMENT              = 2,
    ATTRIBUTE            = 3,
    PI                   = 4,
    COMMENT              = 5,
    TEXT                 = 6
  };

  virtual nsresult            GetNodeType(PRInt32 *aType) = 0;
  virtual nsresult            GetParentNode(nsIDOMNode **aNode) = 0;
  virtual nsresult            GetChildNodes(nsIDOMNodeIterator **aIterator) = 0;
  virtual nsresult            HasChildNodes() = 0;
  virtual nsresult            GetFirstChild(nsIDOMNode **aNode) = 0;
  virtual nsresult            GetPreviousSibling(nsIDOMNode **aNode) = 0;
  virtual nsresult            GetNextSibling(nsIDOMNode **aNode) = 0;
  virtual nsresult            InsertBefore(nsIDOMNode *newChild, nsIDOMNode *refChild) = 0;
  virtual nsresult            ReplaceChild(nsIDOMNode *newChild, nsIDOMNode *oldChild) = 0;
  virtual nsresult            RemoveChild(nsIDOMNode *oldChild) = 0;
};

#endif // nsIDOMNode_h__

