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

#include "nsDOMAttributes.h"
#include "nsIContent.h"
#include "nsIHTMLContent.h"
#include "nsString.h"
#include "nsIAtom.h"
#include "nsISupportsArray.h"

nsDOMAttribute::nsDOMAttribute(nsString &aName, nsString &aValue)
{
  mName = new nsString(aName);
  mValue = new nsString(aValue);
  mRefCnt = 1;
  mScriptObject = nsnull;
}

nsDOMAttribute::~nsDOMAttribute()
{
  NS_PRECONDITION(nsnull != mName && nsnull != mValue, "attribute must be valid");
  delete mName;
  delete mValue;
}

nsresult nsDOMAttribute::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
  static NS_DEFINE_IID(kIDOMAttributeIID, NS_IDOMATTRIBUTE_IID);
  static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
  if (aIID.Equals(kIDOMAttributeIID)) {
    *aInstancePtr = (void*)(nsIDOMAttribute*)this;
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kIScriptObjectOwnerIID)) {
    *aInstancePtr = (void*)(nsIScriptObjectOwner*)this;
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void*)(nsISupports*)(nsIDOMAttribute*)this;
    AddRef();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

NS_IMPL_ADDREF(nsDOMAttribute)

NS_IMPL_RELEASE(nsDOMAttribute)


nsresult nsDOMAttribute::GetScriptObject(JSContext *aContext, void** aScriptObject)
{
  nsresult res = NS_OK;
  if (nsnull == mScriptObject) {
    res = NS_NewScriptAttribute(aContext, this, nsnull, (JSObject**)&mScriptObject);
  }
  *aScriptObject = mScriptObject;
  return res;
}

nsresult nsDOMAttribute::ResetScriptObject()
{
  mScriptObject = nsnull;
  return NS_OK;
}

//
// nsIDOMAttribute interface
//
nsresult nsDOMAttribute::GetName(nsString &aName)
{
  aName = *mName;
  return NS_OK;
}

nsresult nsDOMAttribute::GetValue(nsString &aValue)
{
  aValue = *mValue;
  return NS_OK;
}

nsresult nsDOMAttribute::SetValue(nsString &aValue)
{
  delete mValue;
  mValue = new nsString(aValue);
  return NS_OK;
}

nsresult nsDOMAttribute::GetSpecified()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult nsDOMAttribute::SetSpecified(PRBool specified)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult nsDOMAttribute::ToString(nsString &aString)
{
  aString = *mName;
  aString += " = ";
  aString += *mValue;
  return NS_OK;
}



//
// nsDOMAttributeList interface
//
nsDOMAttributeList::nsDOMAttributeList(nsIHTMLContent &aContent) :
                                          mContent(aContent)
{
  mRefCnt = 1;
  mContent.AddRef();
  mScriptObject = nsnull;
}

nsDOMAttributeList::~nsDOMAttributeList()
{
  mContent.Release();
}

nsresult nsDOMAttributeList::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
  static NS_DEFINE_IID(kIDOMAttributeListIID, NS_IDOMATTRIBUTELIST_IID);
  static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
  if (aIID.Equals(kIDOMAttributeListIID)) {
    *aInstancePtr = (void*)(nsIDOMAttributeList*)this;
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kIScriptObjectOwnerIID)) {
    *aInstancePtr = (void*)(nsIScriptObjectOwner*)this;
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void*)(nsISupports*)(nsIDOMAttributeList*)this;
    AddRef();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

NS_IMPL_ADDREF(nsDOMAttributeList)

NS_IMPL_RELEASE(nsDOMAttributeList)

nsresult nsDOMAttributeList::GetScriptObject(JSContext *aContext, void** aScriptObject)
{
  nsresult res = NS_OK;
  if (nsnull == mScriptObject) {
    res = NS_NewScriptAttributeList(aContext, this, nsnull, (JSObject**)&mScriptObject);
  }
  *aScriptObject = mScriptObject;
  return res;
}

nsresult nsDOMAttributeList::ResetScriptObject()
{
  mScriptObject = nsnull;
  return NS_OK;
}

nsresult nsDOMAttributeList::GetAttribute(nsString &aAttrName, nsIDOMAttribute** aAttribute)
{
  nsAutoString value;
  mContent.GetAttribute(aAttrName, value);
  *aAttribute  = new nsDOMAttribute(aAttrName, value); 
  return NS_OK;
}

nsresult nsDOMAttributeList::SetAttribute(nsIDOMAttribute *aAttribute)
{
  nsAutoString name, value;
  aAttribute->GetName(name);
  aAttribute->GetValue(value);
  mContent.SetAttribute(name, value);
  return NS_OK;
}

nsresult nsDOMAttributeList::Remove(nsString &attrName, nsIDOMAttribute** aAttribute)
{
  nsAutoString name, upper;
  (*aAttribute)->GetName(name);
  name.ToUpperCase(upper);
  nsIAtom* attr = NS_NewAtom(upper);
  mContent.UnsetAttribute(attr);
  return NS_OK;
}

nsresult nsDOMAttributeList::Item(PRUint32 aIndex, nsIDOMAttribute** aAttribute)
{
  nsresult res = NS_ERROR_FAILURE;
  nsAutoString name, value;
  nsISupportsArray *attributes = nsnull;
  if (NS_OK == NS_NewISupportsArray(&attributes)) {
    PRInt32 count = mContent.GetAllAttributeNames(attributes);
    if (count > 0) {
      if ((PRInt32)aIndex < count) {
        nsISupports *att = attributes->ElementAt(aIndex);
        static NS_DEFINE_IID(kIAtom, NS_IATOM_IID);
        nsIAtom *atName = nsnull;
        if (nsnull != att && NS_OK == att->QueryInterface(kIAtom, (void**)&atName)) {
          atName->ToString(name);
          if (eContentAttr_NotThere != mContent.GetAttribute(name, value)) {
            *aAttribute = new nsDOMAttribute(name, value);
            res = NS_OK;
          }
          NS_RELEASE(atName);
        }
      }
    }
    NS_RELEASE(attributes);
  }

  return res;
}

nsresult nsDOMAttributeList::GetLength(PRUint32 *aLength)
{
  *aLength = mContent.GetAttributeCount();
  return NS_OK;
}



