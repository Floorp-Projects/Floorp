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
#include "nsUnicharUtils.h"


//----------------------------------------------------------------------

nsDOMAttribute::nsDOMAttribute(nsIContent* aContent, nsINodeInfo *aNodeInfo,
                               const nsAString& aValue)
  : mContent(aContent), mNodeInfo(aNodeInfo), mValue(aValue), mChild(nsnull),
    mChildList(nsnull)
{
  NS_ABORT_IF_FALSE(mNodeInfo, "We must get a nodeinfo here!");

  NS_INIT_ISUPPORTS();

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
nsDOMAttribute::GetName(nsAString& aName)
{
  NS_ENSURE_TRUE(mNodeInfo, NS_ERROR_FAILURE);

  return mNodeInfo->GetQualifiedName(aName);
}

nsresult
nsDOMAttribute::GetValue(nsAString& aValue)
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
nsDOMAttribute::SetValue(const nsAString& aValue)
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
nsDOMAttribute::GetNodeName(nsAString& aNodeName)
{
  return GetName(aNodeName);
}

NS_IMETHODIMP
nsDOMAttribute::GetNodeValue(nsAString& aNodeValue)
{
  return GetValue(aNodeValue);
}

NS_IMETHODIMP
nsDOMAttribute::SetNodeValue(const nsAString& aNodeValue)
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
    if (!value.IsEmpty()) {
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
  if (!value.IsEmpty()) {
    if (!mChild) {      
      nsCOMPtr<nsITextContent> content;

      result = NS_NewTextNode(getter_AddRefs(content));
      if (NS_OK != result) {
        return result;
      }
      result = content->QueryInterface(NS_GET_IID(nsIDOMText), (void**)&mChild);
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
nsDOMAttribute::GetNamespaceURI(nsAString& aNamespaceURI)
{
  NS_ENSURE_TRUE(mNodeInfo, NS_ERROR_FAILURE);

  return mNodeInfo->GetNamespaceURI(aNamespaceURI);
}

NS_IMETHODIMP 
nsDOMAttribute::GetPrefix(nsAString& aPrefix)
{
  NS_ENSURE_TRUE(mNodeInfo, NS_ERROR_FAILURE);

  return mNodeInfo->GetPrefix(aPrefix);
}

NS_IMETHODIMP 
nsDOMAttribute::SetPrefix(const nsAString& aPrefix)
{
  NS_ENSURE_TRUE(mNodeInfo, NS_ERROR_FAILURE);
  nsCOMPtr<nsINodeInfo> newNodeInfo;
  nsCOMPtr<nsIAtom> prefix;
  nsresult rv = NS_OK;

  if (!aPrefix.IsEmpty() && !DOMStringIsNull(aPrefix))
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
nsDOMAttribute::GetLocalName(nsAString& aLocalName)
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
nsDOMAttribute::IsSupported(const nsAString& aFeature,
                            const nsAString& aVersion,
                            PRBool* aReturn)
{
  return nsGenericElement::InternalIsSupported(aFeature, aVersion, aReturn);
}

NS_IMETHODIMP
nsDOMAttribute::GetBaseURI(nsAString &aURI)
{
  aURI.Truncate();
  nsresult rv = NS_OK;
  nsCOMPtr<nsIDOM3Node> node(do_QueryInterface(mContent));
  if (node)
    rv = node->GetBaseURI(aURI);
  return rv;
}

NS_IMETHODIMP
nsDOMAttribute::CompareTreePosition(nsIDOMNode* aOther,
                                    PRUint16* aReturn)
{
  NS_ENSURE_ARG_POINTER(aOther);
  PRUint16 mask = nsIDOMNode::TREE_POSITION_DISCONNECTED;

  nsCOMPtr<nsIDOMElement> el;
  GetOwnerElement(getter_AddRefs(el));
  if (el) {
    // Check to see if the other node is also an attribute
    PRUint16 nodeType = 0;
    aOther->GetNodeType(&nodeType);
    if (nodeType == nsIDOMNode::ATTRIBUTE_NODE) {
      nsCOMPtr<nsIDOMAttr> otherAttr(do_QueryInterface(aOther));
      nsCOMPtr<nsIDOMElement> otherEl;
      otherAttr->GetOwnerElement(getter_AddRefs(otherEl));
      if (el == otherEl) {
        // same parent node, the two attributes have equivalent position
        mask |= nsIDOMNode::TREE_POSITION_EQUIVALENT;
        PRBool sameNode = PR_FALSE;
        IsSameNode(aOther, &sameNode);
        if (sameNode) {
          mask |= nsIDOMNode::TREE_POSITION_SAME_NODE;
        }
      }
    }
    else {
      // The other node isn't an attribute.
      // Compare position relative to this attribute's owner element.
      nsCOMPtr<nsIDOM3Node> parent(do_QueryInterface(el));
      PRUint16 parentMask;
      parent->CompareTreePosition(aOther, &parentMask);
      if (parentMask & nsIDOMNode::TREE_POSITION_SAME_NODE) {
        mask |= nsIDOMNode::TREE_POSITION_PRECEDING;
      }
      else {
        mask |= parentMask & (nsIDOMNode::TREE_POSITION_FOLLOWING |
                              nsIDOMNode::TREE_POSITION_PRECEDING);
      }
    }
  }

  *aReturn = mask;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMAttribute::IsSameNode(nsIDOMNode* aOther,
                           PRBool* aReturn)
{
  PRBool sameNode = PR_FALSE;

  // XXXcaa Comparing pointers on two attributes is not yet reliable.
  // When bug 93614 is fixed, this should be changed to simple pointer
  // comparisons. But for now, check owner elements and node names.
  PRUint16 otherType = 0;
  aOther->GetNodeType(&otherType);
  if (nsIDOMNode::ATTRIBUTE_NODE == otherType) {
    nsCOMPtr<nsIDOMElement> nodeOwner;
    GetOwnerElement(getter_AddRefs(nodeOwner));
    nsCOMPtr<nsIDOMAttr> other(do_QueryInterface(aOther));
    nsCOMPtr<nsIDOMElement> otherOwner;
    other->GetOwnerElement(getter_AddRefs(otherOwner));
    nsCOMPtr<nsIDOM3Node> owner(do_QueryInterface(nodeOwner));
    PRBool sameOwners = PR_FALSE;
    owner->IsSameNode(otherOwner, &sameOwners);

    // Do these attributes belong to the same element?
    if (sameOwners) {
      PRBool ci = PR_FALSE;
      nsCOMPtr<nsIContent> content(do_QueryInterface(nodeOwner));
      // Check to see if we're in HTML.
      if (content->IsContentOfType(nsIContent::eHTML)) {
        nsCOMPtr<nsINodeInfo> ni;
        content->GetNodeInfo(*getter_AddRefs(ni));
        if (ni) {
          // If there is no namespace, we're in HTML (as opposed to XHTML)
          // and we'll need to compare node names case insensitively.
          ci = ni->NamespaceEquals(kNameSpaceID_None);
        }
      }

      nsAutoString nodeName;
      nsAutoString otherName;
      GetNodeName(nodeName);
      aOther->GetNodeName(otherName);
      // Compare node names
      sameNode = ci ? nodeName.Equals(otherName,
                                      nsCaseInsensitiveStringComparator())
                    : nodeName.Equals(otherName);
    }
  }

  *aReturn = sameNode;
  return NS_OK;
}

NS_IMETHODIMP    
nsDOMAttribute::LookupNamespacePrefix(const nsAString& aNamespaceURI,
                                      nsAString& aPrefix) 
{
  aPrefix.Truncate();
  nsresult rv = NS_OK;
  nsCOMPtr<nsIDOM3Node> node(do_QueryInterface(mContent));
  if (node)
    rv = node->LookupNamespacePrefix(aNamespaceURI, aPrefix);
  return rv;
}

NS_IMETHODIMP    
nsDOMAttribute::LookupNamespaceURI(const nsAString& aNamespacePrefix,
                                   nsAString& aNamespaceURI)
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
  *aLength = 0;
  if (mAttribute) {
    nsAutoString value;
    mAttribute->GetValue(value);
    if (!value.IsEmpty()) {
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
