/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

#include "nsDOMAttribute.h"
#include "nsGenericElement.h"
#include "nsIContent.h"
#include "nsIDOMScriptObjectFactory.h"
#include "nsITextContent.h"

static NS_DEFINE_IID(kIDOMAttrIID, NS_IDOMATTR_IID);
static NS_DEFINE_IID(kIDOMAttributePrivateIID, NS_IDOMATTRIBUTEPRIVATE_IID);
static NS_DEFINE_IID(kIDOMTextIID, NS_IDOMTEXT_IID);
static NS_DEFINE_IID(kIDOMNodeListIID, NS_IDOMNODELIST_IID);

//----------------------------------------------------------------------

nsDOMAttribute::nsDOMAttribute(nsIContent* aContent,
                               const nsString& aName,
                               const nsString& aValue)
  : mName(aName), mValue(aValue)
{
  NS_INIT_REFCNT();
  // We don't add a reference to our content. It will tell us
  // to drop our reference when it goes away.
  mContent = aContent;
  mScriptObject = nsnull;
  mChild = nsnull;
  mChildList = nsnull;
}

nsDOMAttribute::~nsDOMAttribute()
{
  NS_IF_RELEASE(mChild);
  NS_IF_RELEASE(mChildList);
}

nsresult
nsDOMAttribute::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kIDOMAttrIID)) {
    nsIDOMAttr* tmp = this;
    *aInstancePtr = (void*)tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIScriptObjectOwnerIID)) {
    nsIScriptObjectOwner* tmp = this;
    *aInstancePtr = (void*)tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIDOMAttributePrivateIID)) {
    nsIDOMAttributePrivate* tmp = this;
    *aInstancePtr = (void*)tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIDOMNodeIID)) {
    nsIDOMNode* tmp = this;
    *aInstancePtr = (void*)tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    nsIDOMAttr* tmp1 = this;
    nsISupports* tmp2 = tmp1;
    *aInstancePtr = (void*)tmp2;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

NS_IMPL_ADDREF(nsDOMAttribute)
NS_IMPL_RELEASE(nsDOMAttribute)

void 
nsDOMAttribute::DropReference()
{
  mContent = nsnull;
}

void 
nsDOMAttribute::SetContent(nsIContent* aContent)
{
  mContent = aContent;
}

void 
nsDOMAttribute::SetName(const nsString& aName)
{
  mName=aName;
}

nsresult
nsDOMAttribute::GetScriptObject(nsIScriptContext *aContext,
                                void** aScriptObject)
{
  nsresult res = NS_OK;
  if (nsnull == mScriptObject) {
    nsIDOMScriptObjectFactory *factory;
    
    res = nsGenericElement::GetScriptObjectFactory(&factory);
    if (NS_OK != res) {
      return res;
    }

    res = factory->NewScriptAttr(aContext, 
                                 (nsISupports *)(nsIDOMAttr *)this, 
                                 (nsISupports *)mContent,
                                 (void **)&mScriptObject);
    NS_RELEASE(factory);
  }
  *aScriptObject = mScriptObject;
  return res;
}

nsresult
nsDOMAttribute::SetScriptObject(void *aScriptObject)
{
  mScriptObject = aScriptObject;
  return NS_OK;
}

nsresult
nsDOMAttribute::GetName(nsString& aName)
{
  aName=mName;
  return NS_OK;
}

nsresult
nsDOMAttribute::GetValue(nsString& aValue)
{
  nsresult result = NS_OK;
  if (nsnull != mContent) {
    nsIAtom* nameAtom;
    PRInt32 nameSpaceID;
    nsresult attrResult;

    mContent->ParseAttributeString(mName, nameAtom, nameSpaceID);
    attrResult = mContent->GetAttribute(nameSpaceID, nameAtom, mValue);
    if (NS_CONTENT_ATTR_NOT_THERE == attrResult) {
      mValue.Truncate();
    }
    NS_IF_RELEASE(nameAtom);
  }
  aValue=mValue;
  return result;
}

nsresult
nsDOMAttribute::SetValue(const nsString& aValue)
{
  nsresult result = NS_OK;
  if (nsnull != mContent) {
    nsIAtom* nameAtom;
    PRInt32 nameSpaceID;

    mContent->ParseAttributeString(mName, nameAtom, nameSpaceID);
    result = mContent->SetAttribute(nameSpaceID, nameAtom, aValue, PR_TRUE);
    NS_IF_RELEASE(nameAtom);
  }
  mValue=aValue;

  return result;
}

nsresult
nsDOMAttribute::GetSpecified(PRBool* aSpecified)
{
  nsresult result = NS_OK;
  if (nsnull == mContent) {
    *aSpecified = PR_FALSE;
  }
  else {
    nsAutoString value;
    nsresult attrResult;
    nsIAtom* nameAtom;
    PRInt32 nameSpaceID;

    mContent->ParseAttributeString(mName, nameAtom, nameSpaceID);    
    attrResult = mContent->GetAttribute(nameSpaceID, nameAtom, value);
    NS_IF_RELEASE(nameAtom);
    if (NS_CONTENT_ATTR_HAS_VALUE == attrResult) {
      *aSpecified = PR_TRUE;
    }
    else {
      *aSpecified = PR_FALSE;
    }
  }

  return result;
}

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
  return SetValue(aNodeValue);
}

NS_IMETHODIMP
nsDOMAttribute::GetNodeType(PRUint16* aNodeType)
{
  *aNodeType = (PRUint16)nsIDOMNode::ATTRIBUTE_NODE;
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
  if (nsnull == mChildList) {
    mChildList = new nsAttributeChildList(this);
    if (nsnull == mChildList) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    NS_ADDREF(mChildList);
  }

  return mChildList->QueryInterface(kIDOMNodeListIID, (void**)aChildNodes);
}

NS_IMETHODIMP
nsDOMAttribute::HasChildNodes(PRBool* aHasChildNodes)
{
  *aHasChildNodes = PR_FALSE;
  if (nsnull != mChild) {
    *aHasChildNodes = PR_TRUE;
  }
  else if (nsnull != mContent) {
    nsAutoString value;
    
    GetValue(value);
    if (0 < value.Length()) {
      *aHasChildNodes = PR_TRUE;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMAttribute::GetFirstChild(nsIDOMNode** aFirstChild)
{
  nsAutoString value;
  nsresult result;

  result = GetValue(value);
  if (NS_OK != result) {
    return result;
  }
  if (0 < value.Length()) {
    if (nsnull == mChild) {      
      nsIContent* content;

      result = NS_NewTextNode(&content);
      if (NS_OK != result) {
        return result;
      }
      result = content->QueryInterface(kIDOMTextIID, (void**)&mChild);
      NS_RELEASE(content);
    }
    mChild->SetData(value);
    result = mChild->QueryInterface(kIDOMNodeIID, (void**)aFirstChild);
  }
  else {
    *aFirstChild = nsnull;
  }
  return result;
}

NS_IMETHODIMP
nsDOMAttribute::GetLastChild(nsIDOMNode** aLastChild)
{
  return GetFirstChild(aLastChild);
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
nsDOMAttribute::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  nsDOMAttribute* newAttr;

  if (nsnull != mContent) {
    nsAutoString value;
    nsIAtom* nameAtom;
    PRInt32 nameSpaceID;
  
    mContent->ParseAttributeString(mName, nameAtom, nameSpaceID);    
    mContent->GetAttribute(nameSpaceID, nameAtom, value);
    newAttr = new nsDOMAttribute(nsnull, mName, value); 
  }
  else {
    newAttr = new nsDOMAttribute(nsnull, mName, mValue); 
  }

  if (nsnull == newAttr) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return newAttr->QueryInterface(kIDOMNodeIID, (void**)aReturn);
}

NS_IMETHODIMP 
nsDOMAttribute::GetOwnerDocument(nsIDOMDocument** aOwnerDocument)
{
  // XXX TBI
  return NS_ERROR_NOT_IMPLEMENTED;
}

//----------------------------------------------------------------------

nsAttributeChildList::nsAttributeChildList(nsDOMAttribute* aAttribute)
{
  // Don't increment the reference count. The attribute will tell
  // us when it's going away
  mAttribute = aAttribute;
}

nsAttributeChildList::~nsAttributeChildList()
{
}

NS_IMETHODIMP    
nsAttributeChildList::GetLength(PRUint32* aLength)
{
  nsAutoString value;
  
  *aLength = 0;
  if (nsnull != mAttribute) {
    mAttribute->GetValue(value);
    if (0 < value.Length()) {
      *aLength = 1;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP    
nsAttributeChildList::Item(PRUint32 aIndex, nsIDOMNode** aReturn)
{
  *aReturn = nsnull;
  if ((nsnull != mAttribute) && (0 == aIndex)) {
    mAttribute->GetFirstChild(aReturn);
  }

  return NS_OK;
}

void 
nsAttributeChildList::DropReference()
{
  mAttribute = nsnull;
}
