/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsDOMAttributeMap.h"
#include "nsDOMAttribute.h"
#include "nsGenericElement.h"
#include "nsIContent.h"
#include "nsINameSpaceManager.h"
#include "nsDOMError.h"
#include "nsContentUtils.h"
#include "nsNodeInfoManager.h"

//----------------------------------------------------------------------

nsDOMAttributeMap::nsDOMAttributeMap(nsIContent* aContent)
  : mContent(aContent)
{
  // We don't add a reference to our content. If it goes away,
  // we'll be told to drop our reference
}

nsDOMAttributeMap::~nsDOMAttributeMap()
{
}

void
nsDOMAttributeMap::DropReference()
{
  mContent = nsnull;
}


// QueryInterface implementation for nsDOMAttributeMap
NS_INTERFACE_MAP_BEGIN(nsDOMAttributeMap)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNamedNodeMap)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(NamedNodeMap)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsDOMAttributeMap)
NS_IMPL_RELEASE(nsDOMAttributeMap)

NS_IMETHODIMP
nsDOMAttributeMap::GetNamedItem(const nsAString& aAttrName,
                                nsIDOMNode** aAttribute)
{
  NS_ENSURE_ARG_POINTER(aAttribute);
  *aAttribute = nsnull;

  if (mContent) {
    nsCOMPtr<nsINodeInfo> ni =
      mContent->GetExistingAttrNameFromQName(aAttrName);
    if (!ni) {
      return NS_OK;
    }

    nsAutoString value;
    // Eventually we shouldn't need to get the value here at all
    nsresult rv = mContent->GetAttr(ni->NamespaceID(),
                                    ni->NameAtom(), value);
    NS_ASSERTION(rv != NS_CONTENT_ATTR_NOT_THERE, "unable to get attribute");
    NS_ENSURE_SUCCESS(rv, rv);

    nsDOMAttribute* domAttribute = new nsDOMAttribute(mContent, ni, value);
    if (!domAttribute) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    NS_ADDREF(*aAttribute = domAttribute);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMAttributeMap::SetNamedItem(nsIDOMNode *aNode, nsIDOMNode **aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);

  if (!aNode) {
    return NS_ERROR_NULL_POINTER;
  }

  nsresult rv = NS_OK;
  *aReturn = nsnull;

  if (mContent) {
    // XXX should check same-origin between mContent and aNode however
    // nsContentUtils::CheckSameOrigin can't deal with attributenodes yet
    
    nsCOMPtr<nsIDOMAttr> attribute(do_QueryInterface(aNode));

    if (!attribute) {
      return NS_ERROR_DOM_HIERARCHY_REQUEST_ERR;
    }

    nsCOMPtr<nsIDOMElement> owner;
    attribute->GetOwnerElement(getter_AddRefs(owner));
    if (owner) {
      nsCOMPtr<nsISupports> ownerSupports = do_QueryInterface(owner);
      nsCOMPtr<nsISupports> thisSupports = do_QueryInterface(mContent);
      if (ownerSupports != thisSupports) {
        return NS_ERROR_DOM_INUSE_ATTRIBUTE_ERR;
      }
    }

    // get node-info and value of old attribute
    nsAutoString name, value;
    attribute->GetName(name);

    nsCOMPtr<nsINodeInfo> ni = mContent->GetExistingAttrNameFromQName(name);
    if (ni) {
      rv = mContent->GetAttr(ni->NamespaceID(), ni->NameAtom(), value);
      NS_ASSERTION(rv != NS_CONTENT_ATTR_NOT_THERE, "unable to get attribute");
      NS_ENSURE_SUCCESS(rv, rv);

      // Create the attributenode to pass back. We pass a null content here
      // since the attr node we return isn't tied to this content anymore.
      nsDOMAttribute* domAttribute = new nsDOMAttribute(nsnull, ni, value);
      if (!domAttribute) {
        return NS_ERROR_OUT_OF_MEMORY;
      }

      NS_ADDREF(*aReturn = domAttribute);
    }
    else {
      nsINodeInfo *contentNi = mContent->GetNodeInfo();
      NS_ENSURE_TRUE(contentNi, NS_ERROR_FAILURE);

      rv = contentNi->NodeInfoManager()->GetNodeInfo(name, nsnull,
                                                     kNameSpaceID_None,
                                                     getter_AddRefs(ni));
      NS_ENSURE_SUCCESS(rv, rv);
      // value is already empty
    }

    // Set the new attributevalue
    attribute->GetValue(value);
    if (ni->NamespaceID() == kNameSpaceID_None &&
        mContent->IsContentOfType(nsIContent::eHTML)) {
      // Set via setAttribute(), which may do normalization on the
      // attribute name for HTML
      nsCOMPtr<nsIDOMElement> ourElement(do_QueryInterface(mContent));
      NS_ASSERTION(ourElement, "HTML content that's not an element?");
      ourElement->SetAttribute(name, value);
    } else {
      // It's OK to just use SetAttr
      rv = mContent->SetAttr(ni->NamespaceID(), ni->NameAtom(),
                             ni->GetPrefixAtom(), value, PR_TRUE);
    }

    // Not all attributes implement nsIAttribute.
    nsCOMPtr<nsIAttribute> attr = do_QueryInterface(aNode);
    if (attr) {
      attr->SetContent(mContent);
    }
  }

  return rv;
}

NS_IMETHODIMP
nsDOMAttributeMap::RemoveNamedItem(const nsAString& aName,
                                   nsIDOMNode** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nsnull;

  nsresult rv = NS_OK;

  if (mContent) {
    nsCOMPtr<nsINodeInfo> ni = mContent->GetExistingAttrNameFromQName(aName);
    if (!ni) {
      return NS_ERROR_DOM_NOT_FOUND_ERR;
    }

    PRInt32 nsid = ni->NamespaceID();
    nsIAtom *nameAtom = ni->NameAtom();

    nsAutoString value;
    rv = mContent->GetAttr(nsid, nameAtom, value);
    NS_ASSERTION(rv != NS_CONTENT_ATTR_NOT_THERE, "unable to get attribute");
    NS_ENSURE_SUCCESS(rv, rv);

    nsDOMAttribute* domAttribute = new nsDOMAttribute(nsnull, ni, value);
    if (!domAttribute) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    NS_ADDREF(*aReturn = domAttribute);

    rv = mContent->UnsetAttr(nsid, nameAtom, PR_TRUE);
  }

  return rv;
}


NS_IMETHODIMP
nsDOMAttributeMap::Item(PRUint32 aIndex, nsIDOMNode** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  PRInt32 nameSpaceID;
  nsCOMPtr<nsIAtom> nameAtom, prefix;

  nsresult rv = NS_OK;
  if (mContent &&
      NS_SUCCEEDED(mContent->GetAttrNameAt(aIndex,
                                           &nameSpaceID,
                                           getter_AddRefs(nameAtom),
                                           getter_AddRefs(prefix)))) {
    nsAutoString value;
    mContent->GetAttr(nameSpaceID, nameAtom, value);

    nsINodeInfo *contentNi = mContent->GetNodeInfo();
    NS_ENSURE_TRUE(contentNi, NS_ERROR_FAILURE);

    nsCOMPtr<nsINodeInfo> ni;
    contentNi->NodeInfoManager()->GetNodeInfo(nameAtom, prefix, nameSpaceID,
                                              getter_AddRefs(ni));
    NS_ENSURE_TRUE(ni, NS_ERROR_FAILURE);

    nsDOMAttribute* domAttribute = new nsDOMAttribute(mContent, ni, value);
    NS_ENSURE_TRUE(domAttribute, NS_ERROR_OUT_OF_MEMORY);

    rv = CallQueryInterface(domAttribute, aReturn);
  }
  else {
    *aReturn = nsnull;
  }

  return rv;
}

nsresult
nsDOMAttributeMap::GetLength(PRUint32 *aLength)
{
  NS_ENSURE_ARG_POINTER(aLength);

  if (mContent) {
    *aLength = mContent->GetAttrCount();
  } else {
    *aLength = 0;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMAttributeMap::GetNamedItemNS(const nsAString& aNamespaceURI,
                                  const nsAString& aLocalName,
                                  nsIDOMNode** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nsnull;

  if (!mContent) {
    return NS_OK;
  }

  NS_ConvertUTF16toUTF8 utf8Name(aLocalName);
  PRInt32 nameSpaceID = kNameSpaceID_None;

  if (!aNamespaceURI.IsEmpty()) {
    nsContentUtils::GetNSManagerWeakRef()->GetNameSpaceID(aNamespaceURI,
                                                          &nameSpaceID);

    if (nameSpaceID == kNameSpaceID_Unknown) {
      return NS_OK;
    }
  }

  PRUint32 i, count = mContent->GetAttrCount();
  for (i = 0; i < count; ++i) {
    PRInt32 attrNS;
    nsCOMPtr<nsIAtom> nameAtom, prefix;
    mContent->GetAttrNameAt(i, &attrNS, getter_AddRefs(nameAtom),
                            getter_AddRefs(prefix));
    if (nameSpaceID == attrNS &&
        nameAtom->EqualsUTF8(utf8Name)) {
      nsCOMPtr<nsINodeInfo> ni;
      mContent->GetNodeInfo()->NodeInfoManager()->
        GetNodeInfo(nameAtom, prefix, nameSpaceID, getter_AddRefs(ni));
      NS_ENSURE_TRUE(ni, NS_ERROR_FAILURE);

      nsAutoString value;
      mContent->GetAttr(nameSpaceID, nameAtom, value);

      nsDOMAttribute* domAttribute = new nsDOMAttribute(mContent, ni, value);
      NS_ENSURE_TRUE(domAttribute, NS_ERROR_OUT_OF_MEMORY);

      NS_ADDREF(*aReturn = domAttribute);

      return NS_OK;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMAttributeMap::SetNamedItemNS(nsIDOMNode* aArg, nsIDOMNode** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);

  nsresult rv = NS_OK;
  *aReturn = nsnull;

  if (mContent) {
    // XXX should check same-origin between mContent and aNode however
    // nsContentUtils::CheckSameOrigin can't deal with attributenodes yet

    nsCOMPtr<nsIDOMAttr> attribute(do_QueryInterface(aArg));

    if (!attribute) {
      return NS_ERROR_DOM_HIERARCHY_REQUEST_ERR;
    }

    nsCOMPtr<nsIDOMElement> owner;
    attribute->GetOwnerElement(getter_AddRefs(owner));
    if (owner) {
      nsCOMPtr<nsISupports> ownerSupports = do_QueryInterface(owner);
      nsCOMPtr<nsISupports> thisSupports = do_QueryInterface(mContent);
      if (ownerSupports != thisSupports) {
        return NS_ERROR_DOM_INUSE_ATTRIBUTE_ERR;
      }
    }

    nsAutoString name, nsURI, value;

    attribute->GetName(name);
    attribute->GetNamespaceURI(nsURI);

    nsINodeInfo *contentNi = mContent->GetNodeInfo();
    NS_ENSURE_TRUE(contentNi, NS_ERROR_FAILURE);

    nsCOMPtr<nsINodeInfo> ni;
    contentNi->NodeInfoManager()->GetNodeInfo(name, nsURI, getter_AddRefs(ni));
    NS_ENSURE_TRUE(ni, NS_ERROR_FAILURE);

    nsresult attrResult = mContent->GetAttr(ni->NamespaceID(),
                                            ni->NameAtom(), value);

    if (NS_CONTENT_ATTR_NOT_THERE != attrResult && NS_SUCCEEDED(attrResult)) {
      // We pass a null content here since the attr node we return isn't
      // tied to this content anymore.
      nsDOMAttribute* domAttribute = new nsDOMAttribute(nsnull, ni, value);
      if (!domAttribute) {
        return NS_ERROR_OUT_OF_MEMORY;
      }

      NS_ADDREF(*aReturn = domAttribute);
    }

    attribute->GetValue(value);

    rv = mContent->SetAttr(ni->NamespaceID(), ni->NameAtom(),
                           ni->GetPrefixAtom(), value, PR_TRUE);

    // Not all attributes implement nsIAttribute.
    nsCOMPtr<nsIAttribute> attr = do_QueryInterface(aArg);
    if (attr) {
      attr->SetContent(mContent);
    }
  }

  return rv;
}

NS_IMETHODIMP
nsDOMAttributeMap::RemoveNamedItemNS(const nsAString& aNamespaceURI,
                                     const nsAString& aLocalName,
                                     nsIDOMNode** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nsnull;

  nsresult rv = GetNamedItemNS(aNamespaceURI, aLocalName, aReturn);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!*aReturn) {
    return NS_OK;
  }

  nsCOMPtr<nsIAttribute> attr = do_QueryInterface(*aReturn);
  NS_ASSERTION(attr, "attribute returned from nsDOMAttributeMap::GetNameItemNS "
               "didn't implement nsIAttribute");
  NS_ENSURE_TRUE(attr, NS_ERROR_UNEXPECTED);

  nsINodeInfo *ni = attr->NodeInfo();
  mContent->UnsetAttr(ni->NamespaceID(), ni->NameAtom(), PR_TRUE);

  return NS_OK;
}
