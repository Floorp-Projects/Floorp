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

#ifndef nsDOMAttributes_h__
#define nsDOMAttributes_h__

#include "nsIDOMAttribute.h"
#include "nsIScriptObjectOwner.h"

class nsIContent;
class nsIHTMLContent;

class nsDOMAttribute : public nsIDOMAttribute, public nsIScriptObjectOwner {
public:
  nsDOMAttribute(nsString &aName, nsString &aValue);
  virtual ~nsDOMAttribute();

  NS_DECL_ISUPPORTS

  NS_IMETHOD GetScriptObject(JSContext *aContext, void** aScriptObject);
  NS_IMETHOD ResetScriptObject();

  // nsIDOMAttribute interface
  NS_IMETHOD GetName(nsString &aName);
  NS_IMETHOD GetValue(nsString &aValue);
  NS_IMETHOD SetValue(nsString &aValue);
  NS_IMETHOD GetSpecified();
  NS_IMETHOD SetSpecified(PRBool specified);
  NS_IMETHOD ToString(nsString &aString);

private:
  nsString *mName;
  nsString *mValue;
  void *mScriptObject;
};


class nsDOMAttributeList : public nsIDOMAttributeList, public nsIScriptObjectOwner {
public:
  nsDOMAttributeList(nsIHTMLContent &aContent);
  virtual ~nsDOMAttributeList();

  NS_DECL_ISUPPORTS

  NS_IMETHOD GetScriptObject(JSContext *aContext, void** aScriptObject);
  NS_IMETHOD ResetScriptObject();

  // nsIDOMAttributeList interface
  NS_IMETHOD GetAttribute(nsString &aAttrName, nsIDOMAttribute** aAttribute);
  NS_IMETHOD SetAttribute(nsIDOMAttribute *aAttribute);
  NS_IMETHOD Remove(nsString &attrName, nsIDOMAttribute** aAttribute);
  NS_IMETHOD Item(PRUint32 aIndex, nsIDOMAttribute** aAttribute);
  NS_IMETHOD GetLength(PRUint32 *aLength);

private:
  nsIHTMLContent &mContent;
  void *mScriptObject;
};


#endif

