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

#ifndef nsIDOMElement_h__
#define nsIDOMElement_h__

#include "nsDOM.h"
#include "nsIDOMNode.h"

// forward declaration
class nsIDOMAttribute;
class nsIDOMAttributeList;
class nsIDOMNodeIterator;

#define NS_IDOMELEMENT_IID \
{ /* 8f6bca79-ce42-11d1-b724-00600891d8c9 */ \
0x8f6bca79, 0xce42, 0x11d1, \
  {0xb7, 0x24, 0x00, 0x60, 0x08, 0x91, 0xd8, 0xc9} }

class nsIDOMElement : public nsIDOMNode {
public:
  virtual nsresult              GetTagName(nsString &aName) = 0;
  virtual nsresult              GetAttributes(nsIDOMAttributeList **aAttributeList) = 0;
  virtual nsresult              GetDOMAttribute(nsString &aName, nsString &aValue) = 0;
  virtual nsresult              SetDOMAttribute(nsString &aName, nsString &aValue) = 0;
  virtual nsresult              RemoveAttribute(nsString &aName) = 0;
  virtual nsresult              GetAttributeNode(nsString &aName, nsIDOMAttribute **aAttribute) = 0;
  virtual nsresult              SetAttributeNode(nsIDOMAttribute *aAttribute) = 0;
  virtual nsresult              RemoveAttributeNode(nsIDOMAttribute *aAttribute) = 0;
  virtual nsresult              GetElementsByTagName(nsString &aName,nsIDOMNodeIterator **aIterator) = 0;
  virtual nsresult              Normalize() = 0;
};

#endif // nsIDOMElement_h__

