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

#ifndef nsIDOMAttribute_h__
#define nsIDOMAttribute_h__

#include "nsDOM.h"
#include "nsISupports.h"

// forward declaration
class nsIDOMNode;

#define NS_IDOMATTRIBUTE_IID \
{ /* 8f6bca77-ce42-11d1-b724-00600891d8c9 */ \
0x8f6bca77, 0xce42, 0x11d1, \
  {0xb7, 0x24, 0x00, 0x60, 0x08, 0x91, 0xd8, 0xc9} }

class nsIDOMAttribute : public nsISupports {
public:
  virtual nsresult            GetName(nsString &aName) = 0;
  //attribute Node           value;
  virtual nsresult            GetValue(nsString &aName /*nsIDOMNode **aValue*/) = 0;
  virtual nsresult            SetValue(nsString &aName /*nsIDOMNode *aValue*/) = 0;
  //attribute boolean        specified;
  virtual nsresult            GetSpecified() = 0;
  virtual nsresult            SetSpecified(PRBool specified) = 0;
  virtual nsresult            ToString(nsString &aString) = 0;
};

#define NS_IDOMATTRIBUTELIST_IID \
{ /* 8f6bca78-ce42-11d1-b724-00600891d8c9 */ \
0x8f6bca78, 0xce42, 0x11d1, \
  {0xb7, 0x24, 0x00, 0x60, 0x08, 0x91, 0xd8, 0xc9} }

class nsIDOMAttributeList : public nsISupports {
public:
  virtual nsresult            GetAttribute(nsString &aAttrName, nsIDOMAttribute** aAttribute) = 0;
  virtual nsresult            SetAttribute(nsIDOMAttribute *attr) = 0;
  virtual nsresult            Remove(nsString &attrName, nsIDOMAttribute** aAttribute) = 0;
  virtual nsresult            Item(PRUint32 aIndex, nsIDOMAttribute** aAttribute) = 0;
  virtual nsresult            GetLength(PRUint32 *aLength) = 0;
};


#endif // nsIDOMAttribute_h__

