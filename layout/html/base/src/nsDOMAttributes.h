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

  virtual nsresult            GetScriptObject(JSContext *aContext, void** aScriptObject);
  virtual nsresult            ResetScriptObject();

  // nsIDOMAttribute interface
  virtual nsresult            GetName(nsString &aName);
  virtual nsresult            GetValue(nsString &aValue /*nsIDOMNode **aValue*/);
  virtual nsresult            SetValue(nsString &aValue /*nsIDOMNode *aValue*/);
  virtual nsresult            GetSpecified();
  virtual nsresult            SetSpecified(PRBool specified);
  virtual nsresult            ToString(nsString &aString);

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

  virtual nsresult            GetScriptObject(JSContext *aContext, void** aScriptObject);
  virtual nsresult            ResetScriptObject();

  // nsIDOMAttributeList interface
  virtual nsresult            GetAttribute(nsString &aAttrName, nsIDOMAttribute** aAttribute);
  virtual nsresult            SetAttribute(nsIDOMAttribute *aAttribute);
  virtual nsresult            Remove(nsString &attrName, nsIDOMAttribute** aAttribute);
  virtual nsresult            Item(PRUint32 aIndex, nsIDOMAttribute** aAttribute);
  virtual nsresult            GetLength(PRUint32 *aLength);

private:
  nsIHTMLContent &mContent;
  void *mScriptObject;
};


#endif

