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

nsDOMAttribute::nsDOMAttribute(const nsString &aName, const nsString &aValue)
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


nsresult nsDOMAttribute::GetScriptObject(nsIScriptContext *aContext, void** aScriptObject)
{
  nsresult res = NS_OK;
  if (nsnull == mScriptObject) {
    res = NS_NewScriptAttribute(aContext, (nsISupports *)(nsIDOMAttribute *)this, nsnull, (void **)&mScriptObject);
  }
  *aScriptObject = mScriptObject;
  return res;
}

nsresult nsDOMAttribute::SetScriptObject(void *aScriptObject)
{
  mScriptObject = aScriptObject;
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

nsresult nsDOMAttribute::GetSpecified(PRBool *aSpecified)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult nsDOMAttribute::SetSpecified(PRBool specified)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

// nsIDOMNode interface
NS_IMETHODIMP    
nsDOMAttribute::GetNodeName(nsString& aNodeName)
{
  return GetName(aNodeName);
}

NS_IMETHODIMP    
nsDOMAttribute::GetNodeValue(nsString& aNodeValue)
{
  return GetValue(aNodeValue);
}

NS_IMETHODIMP    
nsDOMAttribute::SetNodeValue(const nsString& aNodeValue)
{
  // You can't actually do this, but we'll fail silently
  return NS_OK;
}

NS_IMETHODIMP    
nsDOMAttribute::GetNodeType(PRInt32* aNodeType)
{
  *aNodeType = (PRInt32)nsIDOMNode::ATTRIBUTE;
  return NS_OK;
}

NS_IMETHODIMP    
nsDOMAttribute::GetParentNode(nsIDOMNode** aParentNode)
{
  *aParentNode = nsnull;
  return NS_OK;
}

NS_IMETHODIMP    
nsDOMAttribute::GetChildNodes(nsIDOMNodeList** aChildNodes)
{
  *aChildNodes = nsnull;
  return NS_OK;
}

NS_IMETHODIMP    
nsDOMAttribute::GetHasChildNodes(PRBool* aHasChildNodes)
{
  *aHasChildNodes = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP    
nsDOMAttribute::GetFirstChild(nsIDOMNode** aFirstChild)
{
  *aFirstChild = nsnull;
  return NS_OK;
}

NS_IMETHODIMP    
nsDOMAttribute::GetLastChild(nsIDOMNode** aLastChild)
{
  *aLastChild = nsnull;
  return NS_OK;
}

NS_IMETHODIMP    
nsDOMAttribute::GetPreviousSibling(nsIDOMNode** aPreviousSibling)
{
  *aPreviousSibling = nsnull;
  return NS_OK;
}

NS_IMETHODIMP    
nsDOMAttribute::GetNextSibling(nsIDOMNode** aNextSibling)
{
  *aNextSibling = nsnull;
  return NS_OK;
}

NS_IMETHODIMP    
nsDOMAttribute::GetAttributes(nsIDOMNamedNodeMap** aAttributes)
{
  *aAttributes = nsnull;
  return NS_OK;
}

NS_IMETHODIMP    
nsDOMAttribute::InsertBefore(nsIDOMNode* aNewChild, nsIDOMNode* aRefChild, nsIDOMNode** aReturn)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP    
nsDOMAttribute::ReplaceChild(nsIDOMNode* aNewChild, nsIDOMNode* aOldChild, nsIDOMNode** aReturn)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP    
nsDOMAttribute::RemoveChild(nsIDOMNode* aOldChild, nsIDOMNode** aReturn)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP    
nsDOMAttribute::AppendChild(nsIDOMNode* aNewChild, nsIDOMNode** aReturn)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP    
nsDOMAttribute::CloneNode(nsIDOMNode** aReturn)
{
  nsDOMAttribute *newAttr = new nsDOMAttribute(*mName, *mValue);
  if (nsnull == newAttr) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  
  *aReturn = newAttr;
  return NS_OK;
}

NS_IMETHODIMP    
nsDOMAttribute::Equals(nsIDOMNode* aNode, PRBool aDeep, PRBool* aReturn)
{
  // XXX TBI
  return NS_OK;
}


//
// nsDOMAttributeList interface
//
nsDOMAttributeMap::nsDOMAttributeMap(nsIHTMLContent &aContent) :
  mContent(aContent)
{
  mRefCnt = 1;
  mContent.AddRef();
  mScriptObject = nsnull;
}

nsDOMAttributeMap::~nsDOMAttributeMap()
{
  mContent.Release();
}

nsresult nsDOMAttributeMap::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
  static NS_DEFINE_IID(kIDOMNamedNodeMapIID, NS_IDOMNAMEDNODEMAP_IID);
  static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
  if (aIID.Equals(kIDOMNamedNodeMapIID)) {
    *aInstancePtr = (void*)(nsIDOMNamedNodeMap*)this;
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kIScriptObjectOwnerIID)) {
    *aInstancePtr = (void*)(nsIScriptObjectOwner*)this;
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void*)(nsISupports*)(nsIDOMNamedNodeMap*)this;
    AddRef();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

NS_IMPL_ADDREF(nsDOMAttributeMap)

NS_IMPL_RELEASE(nsDOMAttributeMap)

nsresult nsDOMAttributeMap::GetScriptObject(nsIScriptContext *aContext, void** aScriptObject)
{
  nsresult res = NS_OK;
  if (nsnull == mScriptObject) {
    res = NS_NewScriptNamedNodeMap(aContext, (nsISupports *)(nsIDOMNamedNodeMap *)this, nsnull, (void**)&mScriptObject);
  }
  *aScriptObject = mScriptObject;
  return res;
}

nsresult nsDOMAttributeMap::SetScriptObject(void *aScriptObject)
{
  mScriptObject = aScriptObject;
  return NS_OK;
}

nsresult nsDOMAttributeMap::GetNamedItem(const nsString &aAttrName, nsIDOMNode** aAttribute)
{
  nsAutoString value;
  mContent.GetAttribute(aAttrName, value);
  *aAttribute  = (nsIDOMNode *)new nsDOMAttribute(aAttrName, value); 
  return NS_OK;
}

nsresult nsDOMAttributeMap::SetNamedItem(nsIDOMNode *aNode)
{
  nsIDOMAttribute *attribute;
  nsAutoString name, value;
  nsresult err;
  static NS_DEFINE_IID(kIDOMAttributeIID, NS_IDOMATTRIBUTE_IID);

  if (NS_OK != (err = aNode->QueryInterface(kIDOMAttributeIID, (void **)&attribute))) {
    return err;
  }

  attribute->GetName(name);
  attribute->GetValue(value);
  NS_RELEASE(attribute);

  mContent.SetAttribute(name, value, PR_TRUE);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMAttributeMap::RemoveNamedItem(const nsString& aName, nsIDOMNode** aReturn)
{
  nsresult res = GetNamedItem(aName, aReturn);
  if (NS_OK == res) {
    nsAutoString upper;
    aName.ToUpperCase(upper);
    nsIAtom* attr = NS_NewAtom(upper);
    mContent.UnsetAttribute(attr);
  }

  return res;
}

nsresult nsDOMAttributeMap::Item(PRUint32 aIndex, nsIDOMNode** aReturn)
{
  nsresult res = NS_ERROR_FAILURE;
  nsAutoString name, value;
  nsISupportsArray *attributes = nsnull;
  if (NS_OK == NS_NewISupportsArray(&attributes)) {
    PRInt32 count;
    mContent.GetAllAttributeNames(attributes, count);
    if (count > 0) {
      if ((PRInt32)aIndex < count) {
        nsISupports *att = attributes->ElementAt(aIndex);
        static NS_DEFINE_IID(kIAtom, NS_IATOM_IID);
        nsIAtom *atName = nsnull;
        if (nsnull != att && NS_OK == att->QueryInterface(kIAtom, (void**)&atName)) {
          atName->ToString(name);
          if (NS_CONTENT_ATTR_NOT_THERE != mContent.GetAttribute(name, value)) {
            *aReturn = (nsIDOMNode *)new nsDOMAttribute(name, value);
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

nsresult nsDOMAttributeMap::GetLength(PRUint32 *aLength)
{
  PRInt32 n;
  mContent.GetAttributeCount(n);
  *aLength = PRUint32(n);
  return NS_OK;
}



