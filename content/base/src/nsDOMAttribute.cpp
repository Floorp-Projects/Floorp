/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsDOMAttribute.h"
#include "nsGenericElement.h"
#include "nsIContent.h"
#include "nsITextContent.h"
#include "nsINameSpaceManager.h"
#include "nsDOMError.h"
#include "nsContentUtils.h"


//----------------------------------------------------------------------

nsDOMAttribute::nsDOMAttribute(nsIContent* aContent, nsINodeInfo *aNodeInfo,
                               const nsAReadableString& aValue)
  : mNodeInfo(aNodeInfo), mValue(aValue), mContent(aContent), mChild(nsnull),
    mChildList(nsnull)
{
  NS_ABORT_IF_FALSE(mNodeInfo, "We must get a nodeinfo here!");

  NS_INIT_REFCNT();

  // We don't add a reference to our content. It will tell us
  // to drop our reference when it goes away.
}

nsDOMAttribute::~nsDOMAttribute()
{
  NS_IF_RELEASE(mChild);
  NS_IF_RELEASE(mChildList);
}


// QueryInterface implementation for nsDOMAttribute
NS_INTERFACE_MAP_BEGIN(nsDOMAttribute)
  NS_INTERFACE_MAP_ENTRY(nsIDOMAttr)
  NS_INTERFACE_MAP_ENTRY(nsIDOMAttributePrivate)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNode)
  NS_INTERFACE_MAP_ENTRY(nsIDOM3Node)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMAttr)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(Attr)
NS_INTERFACE_MAP_END


NS_IMPL_ADDREF(nsDOMAttribute)
NS_IMPL_RELEASE(nsDOMAttribute)


NS_IMETHODIMP
nsDOMAttribute::DropReference()
{
  mContent = nsnull;

  return NS_OK;
}

NS_IMETHODIMP
nsDOMAttribute::SetContent(nsIContent* aContent)
{
  mContent = aContent;

  return NS_OK;
}

NS_IMETHODIMP
nsDOMAttribute::GetContent(nsIContent** aContent)
{
  *aContent = mContent;
  NS_IF_ADDREF(*aContent);

  return NS_OK;
}

nsresult
nsDOMAttribute::GetName(nsAWritableString& aName)
{
  NS_ENSURE_TRUE(mNodeInfo, NS_ERROR_FAILURE);

  return mNodeInfo->GetQualifiedName(aName);
}

nsresult
nsDOMAttribute::GetValue(nsAWritableString& aValue)
{
  NS_ENSURE_TRUE(mNodeInfo, NS_ERROR_FAILURE);

  nsresult result = NS_OK;
  if (mContent) {
    nsresult attrResult;
    PRInt32 nameSpaceID;
    nsCOMPtr<nsIAtom> name;

    mNodeInfo->GetNameAtom(*getter_AddRefs(name));
    mNodeInfo->GetNamespaceID(nameSpaceID);

    nsAutoString tmpValue;
    attrResult = mContent->GetAttr(nameSpaceID, name, tmpValue);
    if (NS_CONTENT_ATTR_NOT_THERE != attrResult) {
      mValue = tmpValue;
    }
  }
  aValue=mValue;
  return result;
}

nsresult
nsDOMAttribute::SetValue(const nsAReadableString& aValue)
{
  NS_ENSURE_TRUE(mNodeInfo, NS_ERROR_FAILURE);

  nsresult result = NS_OK;
  if (mContent) {
    result = mContent->SetAttr(mNodeInfo, aValue, PR_TRUE);
  }
  mValue=aValue;

  return result;
}

nsresult
nsDOMAttribute::GetSpecified(PRBool* aSpecified)
{
  NS_ENSURE_TRUE(mNodeInfo, NS_ERROR_FAILURE);
  NS_ENSURE_ARG_POINTER(aSpecified);

  nsresult result = NS_OK;
  if (nsnull == mContent) {
    *aSpecified = PR_FALSE;
  } else {
    nsAutoString value;
    nsresult attrResult;
    PRInt32 nameSpaceID;
    nsCOMPtr<nsIAtom> name;

    mNodeInfo->GetNameAtom(*getter_AddRefs(name));
    mNodeInfo->GetNamespaceID(nameSpaceID);

    attrResult = mContent->GetAttr(nameSpaceID, name, value);
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
nsDOMAttribute::GetOwnerElement(nsIDOMElement** aOwnerElement)
{
  NS_ENSURE_ARG_POINTER(aOwnerElement);

  if (mContent) {
    return mContent->QueryInterface(NS_GET_IID(nsIDOMElement),
                                    (void **)aOwnerElement);
  }

  *aOwnerElement = nsnull;

  return NS_OK;
}

NS_IMETHODIMP
nsDOMAttribute::GetNodeName(nsAWritableString& aNodeName)
{
  return GetName(aNodeName);
}

NS_IMETHODIMP
nsDOMAttribute::GetNodeValue(nsAWritableString& aNodeValue)
{
  return GetValue(aNodeValue);
}

NS_IMETHODIMP
nsDOMAttribute::SetNodeValue(const nsAReadableString& aNodeValue)
{
  return SetValue(aNodeValue);
}

NS_IMETHODIMP
nsDOMAttribute::GetNodeType(PRUint16* aNodeType)
{
  NS_ENSURE_ARG_POINTER(aNodeType);

  *aNodeType = (PRUint16)nsIDOMNode::ATTRIBUTE_NODE;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMAttribute::GetParentNode(nsIDOMNode** aParentNode)
{
  NS_ENSURE_ARG_POINTER(aParentNode);

  *aParentNode = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMAttribute::GetChildNodes(nsIDOMNodeList** aChildNodes)
{
  if (!mChildList) {
    mChildList = new nsAttributeChildList(this);
    NS_ENSURE_TRUE(mChildList, NS_ERROR_OUT_OF_MEMORY);

    NS_ADDREF(mChildList);
  }

  return CallQueryInterface(mChildList, aChildNodes);
}

NS_IMETHODIMP
nsDOMAttribute::HasChildNodes(PRBool* aHasChildNodes)
{
  *aHasChildNodes = PR_FALSE;
  if (mChild) {
    *aHasChildNodes = PR_TRUE;
  }
  else if (mContent) {
    nsAutoString value;

    GetValue(value);
    if (0 < value.Length()) {
      *aHasChildNodes = PR_TRUE;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMAttribute::HasAttributes(PRBool* aHasAttributes)
{
  NS_ENSURE_ARG_POINTER(aHasAttributes);

  *aHasAttributes = PR_FALSE;

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
    if (!mChild) {      
      nsIContent* content;

      result = NS_NewTextNode(&content);
      if (NS_OK != result) {
        return result;
      }
      result = content->QueryInterface(NS_GET_IID(nsIDOMText), (void**)&mChild);
      NS_RELEASE(content);
    }
    mChild->SetData(value);
    result = mChild->QueryInterface(NS_GET_IID(nsIDOMNode), (void**)aFirstChild);
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
  NS_ENSURE_ARG_POINTER(aPreviousSibling);

  *aPreviousSibling = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMAttribute::GetNextSibling(nsIDOMNode** aNextSibling)
{
  NS_ENSURE_ARG_POINTER(aNextSibling);

  *aNextSibling = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMAttribute::GetAttributes(nsIDOMNamedNodeMap** aAttributes)
{
  NS_ENSURE_ARG_POINTER(aAttributes);

  *aAttributes = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMAttribute::InsertBefore(nsIDOMNode* aNewChild, nsIDOMNode* aRefChild, nsIDOMNode** aReturn)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsDOMAttribute::ReplaceChild(nsIDOMNode* aNewChild, nsIDOMNode* aOldChild, nsIDOMNode** aReturn)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsDOMAttribute::RemoveChild(nsIDOMNode* aOldChild, nsIDOMNode** aReturn)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsDOMAttribute::AppendChild(nsIDOMNode* aNewChild, nsIDOMNode** aReturn)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsDOMAttribute::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  nsDOMAttribute* newAttr;

  if (mContent) {
    nsAutoString value;
    PRInt32 nameSpaceID;
    nsCOMPtr<nsIAtom> name;

    mNodeInfo->GetNameAtom(*getter_AddRefs(name));
    mNodeInfo->GetNamespaceID(nameSpaceID);
  
    mContent->GetAttr(nameSpaceID, name, value);
    newAttr = new nsDOMAttribute(nsnull, mNodeInfo, value); 
  }
  else {
    newAttr = new nsDOMAttribute(nsnull, mNodeInfo, mValue); 
  }

  if (!newAttr) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return newAttr->QueryInterface(NS_GET_IID(nsIDOMNode), (void**)aReturn);
}

NS_IMETHODIMP 
nsDOMAttribute::GetOwnerDocument(nsIDOMDocument** aOwnerDocument)
{
  nsresult result = NS_OK;
  if (mContent) {
    nsIDOMNode* node;
    result = mContent->QueryInterface(NS_GET_IID(nsIDOMNode), (void**)&node);
    if (NS_SUCCEEDED(result)) {
      result = node->GetOwnerDocument(aOwnerDocument);
      NS_RELEASE(node);
    }
  }
  else {
    *aOwnerDocument = nsnull;
  }

  return result;
}

NS_IMETHODIMP 
nsDOMAttribute::GetNamespaceURI(nsAWritableString& aNamespaceURI)
{
  NS_ENSURE_TRUE(mNodeInfo, NS_ERROR_FAILURE);

  return mNodeInfo->GetNamespaceURI(aNamespaceURI);
}

NS_IMETHODIMP 
nsDOMAttribute::GetPrefix(nsAWritableString& aPrefix)
{
  NS_ENSURE_TRUE(mNodeInfo, NS_ERROR_FAILURE);

  return mNodeInfo->GetPrefix(aPrefix);
}

NS_IMETHODIMP 
nsDOMAttribute::SetPrefix(const nsAReadableString& aPrefix)
{
  NS_ENSURE_TRUE(mNodeInfo, NS_ERROR_FAILURE);
  nsCOMPtr<nsINodeInfo> newNodeInfo;
  nsCOMPtr<nsIAtom> prefix;
  nsresult rv = NS_OK;

  if (aPrefix.Length() && !DOMStringIsNull(aPrefix))
    prefix = dont_AddRef(NS_NewAtom(aPrefix));

  rv = mNodeInfo->PrefixChanged(prefix, *getter_AddRefs(newNodeInfo));
  NS_ENSURE_SUCCESS(rv, rv);

  if (mContent) {
    nsCOMPtr<nsIAtom> name;
    PRInt32 nameSpaceID;
    nsAutoString tmpValue;

    mNodeInfo->GetNameAtom(*getter_AddRefs(name));
    mNodeInfo->GetNamespaceID(nameSpaceID);

    rv = mContent->GetAttr(nameSpaceID, name, tmpValue);
    if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
      mContent->UnsetAttr(nameSpaceID, name, PR_TRUE);

      mContent->SetAttr(newNodeInfo, tmpValue, PR_TRUE);
    }
  }

  mNodeInfo = newNodeInfo;

  return NS_OK;
}

NS_IMETHODIMP 
nsDOMAttribute::GetLocalName(nsAWritableString& aLocalName)
{
  NS_ENSURE_TRUE(mNodeInfo, NS_ERROR_FAILURE);

  return mNodeInfo->GetLocalName(aLocalName);
}

NS_IMETHODIMP 
nsDOMAttribute::Normalize()
{
  // Nothing to do here
  return NS_OK;
}

NS_IMETHODIMP
nsDOMAttribute::IsSupported(const nsAReadableString& aFeature,
                            const nsAReadableString& aVersion,
                            PRBool* aReturn)
{
  return nsGenericElement::InternalIsSupported(aFeature, aVersion, aReturn);
}

NS_IMETHODIMP
nsDOMAttribute::GetBaseURI(nsAWritableString &aURI)
{
  aURI.Truncate();
  nsresult rv = NS_OK;
  nsCOMPtr<nsIDOM3Node> node(do_QueryInterface(mContent));
  if (node)
    rv = node->GetBaseURI(aURI);
  return rv;
}

NS_IMETHODIMP    
nsDOMAttribute::LookupNamespacePrefix(const nsAReadableString& aNamespaceURI,
                                      nsAWritableString& aPrefix) 
{
  aPrefix.Truncate();
  nsresult rv = NS_OK;
  nsCOMPtr<nsIDOM3Node> node(do_QueryInterface(mContent));
  if (node)
    rv = node->LookupNamespacePrefix(aNamespaceURI, aPrefix);
  return rv;
}

NS_IMETHODIMP    
nsDOMAttribute::LookupNamespaceURI(const nsAReadableString& aNamespacePrefix,
                                   nsAWritableString& aNamespaceURI)
{
  aNamespaceURI.Truncate();
  nsresult rv = NS_OK;
  nsCOMPtr<nsIDOM3Node> node(do_QueryInterface(mContent));
  if (node)
    rv = node->LookupNamespaceURI(aNamespacePrefix, aNamespaceURI);
  return rv;
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
  if (mAttribute) {
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
  if (mAttribute && 0 == aIndex) {
    mAttribute->GetFirstChild(aReturn);
  }

  return NS_OK;
}

void 
nsAttributeChildList::DropReference()
{
  mAttribute = nsnull;
}
