/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Implementation of the |attributes| property of DOM Core's nsIDOMNode object.
 */

#include "nsDOMAttributeMap.h"
#include "nsDOMAttribute.h"
#include "nsIDOMDocument.h"
#include "nsGenericElement.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsINameSpaceManager.h"
#include "nsDOMError.h"
#include "nsContentUtils.h"
#include "nsNodeInfoManager.h"
#include "nsAttrName.h"
#include "nsUnicharUtils.h"

//----------------------------------------------------------------------

nsDOMAttributeMap::nsDOMAttributeMap(Element* aContent)
  : mContent(aContent)
{
  // We don't add a reference to our content. If it goes away,
  // we'll be told to drop our reference
  mAttributeCache.Init();
}

/**
 * Clear map pointer for attributes.
 */
PLDHashOperator
RemoveMapRef(nsAttrHashKey::KeyType aKey, nsRefPtr<nsDOMAttribute>& aData,
             void* aUserArg)
{
  aData->SetMap(nsnull);

  return PL_DHASH_REMOVE;
}

nsDOMAttributeMap::~nsDOMAttributeMap()
{
  mAttributeCache.Enumerate(RemoveMapRef, nsnull);
}

void
nsDOMAttributeMap::DropReference()
{
  mAttributeCache.Enumerate(RemoveMapRef, nsnull);
  mContent = nsnull;
}

NS_IMPL_CYCLE_COLLECTION_CLASS(nsDOMAttributeMap)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsDOMAttributeMap)
  tmp->DropReference();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END


PLDHashOperator
TraverseMapEntry(nsAttrHashKey::KeyType aKey, nsRefPtr<nsDOMAttribute>& aData,
                 void* aUserArg)
{
  nsCycleCollectionTraversalCallback *cb = 
    static_cast<nsCycleCollectionTraversalCallback*>(aUserArg);

  cb->NoteXPCOMChild(static_cast<nsINode*>(aData.get()));

  return PL_DHASH_NEXT;
}

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsDOMAttributeMap)
  tmp->mAttributeCache.Enumerate(TraverseMapEntry, &cb);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

DOMCI_DATA(NamedNodeMap, nsDOMAttributeMap)

// QueryInterface implementation for nsDOMAttributeMap
NS_INTERFACE_TABLE_HEAD(nsDOMAttributeMap)
  NS_OFFSET_AND_INTERFACE_TABLE_BEGIN(nsDOMAttributeMap)
    NS_INTERFACE_TABLE_ENTRY(nsDOMAttributeMap, nsIDOMNamedNodeMap)
  NS_OFFSET_AND_INTERFACE_TABLE_END
  NS_OFFSET_AND_INTERFACE_TABLE_TO_MAP_SEGUE
  NS_INTERFACE_MAP_ENTRIES_CYCLE_COLLECTION(nsDOMAttributeMap)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(NamedNodeMap)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsDOMAttributeMap)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsDOMAttributeMap)

PLDHashOperator
SetOwnerDocumentFunc(nsAttrHashKey::KeyType aKey,
                     nsRefPtr<nsDOMAttribute>& aData,
                     void* aUserArg)
{
  nsresult rv = aData->SetOwnerDocument(static_cast<nsIDocument*>(aUserArg));

  return NS_FAILED(rv) ? PL_DHASH_STOP : PL_DHASH_NEXT;
}

nsresult
nsDOMAttributeMap::SetOwnerDocument(nsIDocument* aDocument)
{
  PRUint32 n = mAttributeCache.Enumerate(SetOwnerDocumentFunc, aDocument);
  NS_ENSURE_TRUE(n == mAttributeCache.Count(), NS_ERROR_FAILURE);

  return NS_OK;
}

void
nsDOMAttributeMap::DropAttribute(PRInt32 aNamespaceID, nsIAtom* aLocalName)
{
  nsAttrKey attr(aNamespaceID, aLocalName);
  nsDOMAttribute *node = mAttributeCache.GetWeak(attr);
  if (node) {
    // Break link to map
    node->SetMap(nsnull);

    // Remove from cache
    mAttributeCache.Remove(attr);
  }
}

nsresult
nsDOMAttributeMap::RemoveAttribute(nsINodeInfo* aNodeInfo, nsIDOMNode** aReturn)
{
  NS_ASSERTION(aNodeInfo, "RemoveAttribute() called with aNodeInfo == nsnull!");
  NS_ASSERTION(aReturn, "RemoveAttribute() called with aReturn == nsnull");

  *aReturn = nsnull;

  nsAttrKey attr(aNodeInfo->NamespaceID(), aNodeInfo->NameAtom());

  nsRefPtr<nsDOMAttribute> node;
  if (!mAttributeCache.Get(attr, getter_AddRefs(node))) {
    nsAutoString value;
    // As we are removing the attribute we need to set the current value in
    // the attribute node.
    mContent->GetAttr(aNodeInfo->NamespaceID(), aNodeInfo->NameAtom(), value);
    nsCOMPtr<nsINodeInfo> ni = aNodeInfo;
    nsCOMPtr<nsIDOMNode> newAttr =
      new nsDOMAttribute(nsnull, ni.forget(), value, true);
    if (!newAttr) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    newAttr.swap(*aReturn);
  }
  else {
    // Break link to map
    node->SetMap(nsnull);

    // Remove from cache
    mAttributeCache.Remove(attr);

    node.forget(aReturn);
  }

  return NS_OK;
}

nsDOMAttribute*
nsDOMAttributeMap::GetAttribute(nsINodeInfo* aNodeInfo, bool aNsAware)
{
  NS_ASSERTION(aNodeInfo, "GetAttribute() called with aNodeInfo == nsnull!");

  nsAttrKey attr(aNodeInfo->NamespaceID(), aNodeInfo->NameAtom());

  nsDOMAttribute* node = mAttributeCache.GetWeak(attr);
  if (!node) {
    nsCOMPtr<nsINodeInfo> ni = aNodeInfo;
    nsRefPtr<nsDOMAttribute> newAttr =
      new nsDOMAttribute(this, ni.forget(), EmptyString(), aNsAware);
    mAttributeCache.Put(attr, newAttr);
    node = newAttr;
  }

  return node;
}

nsDOMAttribute*
nsDOMAttributeMap::GetNamedItem(const nsAString& aAttrName, nsresult *aResult)
{
  *aResult = NS_OK;

  if (mContent) {
    nsCOMPtr<nsINodeInfo> ni =
      mContent->GetExistingAttrNameFromQName(aAttrName);
    if (ni) {
      return GetAttribute(ni, false);
    }
  }

  return nsnull;
}

NS_IMETHODIMP
nsDOMAttributeMap::GetNamedItem(const nsAString& aAttrName,
                                nsIDOMNode** aAttribute)
{
  NS_ENSURE_ARG_POINTER(aAttribute);

  nsresult rv;
  NS_IF_ADDREF(*aAttribute = GetNamedItem(aAttrName, &rv));

  return rv;
}

NS_IMETHODIMP
nsDOMAttributeMap::SetNamedItem(nsIDOMNode *aNode, nsIDOMNode **aReturn)
{
  return SetNamedItemInternal(aNode, aReturn, false);
}

NS_IMETHODIMP
nsDOMAttributeMap::SetNamedItemNS(nsIDOMNode *aNode, nsIDOMNode **aReturn)
{
  return SetNamedItemInternal(aNode, aReturn, true);
}

nsresult
nsDOMAttributeMap::SetNamedItemInternal(nsIDOMNode *aNode,
                                        nsIDOMNode **aReturn,
                                        bool aWithNS)
{
  NS_ENSURE_ARG_POINTER(aNode);
  NS_ENSURE_ARG_POINTER(aReturn);

  nsresult rv = NS_OK;
  *aReturn = nsnull;
  nsCOMPtr<nsIDOMNode> tmpReturn;

  if (mContent) {
    // XXX should check same-origin between mContent and aNode however
    // nsContentUtils::CheckSameOrigin can't deal with attributenodes yet
    
    nsCOMPtr<nsIAttribute> iAttribute(do_QueryInterface(aNode));
    if (!iAttribute) {
      return NS_ERROR_DOM_HIERARCHY_REQUEST_ERR;
    }

    nsDOMAttribute *attribute = static_cast<nsDOMAttribute*>(iAttribute.get());

    // Check that attribute is not owned by somebody else
    nsDOMAttributeMap* owner = iAttribute->GetMap();
    if (owner) {
      if (owner != this) {
        return NS_ERROR_DOM_INUSE_ATTRIBUTE_ERR;
      }

      // setting a preexisting attribute is a no-op, just return the same
      // node.
      NS_ADDREF(*aReturn = aNode);
      
      return NS_OK;
    }

    if (!mContent->HasSameOwnerDoc(iAttribute)) {
      nsCOMPtr<nsIDOMDocument> domDoc =
        do_QueryInterface(mContent->OwnerDoc(), &rv);
      NS_ENSURE_SUCCESS(rv, rv);

      nsCOMPtr<nsIDOMNode> adoptedNode;
      rv = domDoc->AdoptNode(aNode, getter_AddRefs(adoptedNode));
      NS_ENSURE_SUCCESS(rv, rv);

      NS_ASSERTION(adoptedNode == aNode, "Uh, adopt node changed nodes?");
    }

    // Get nodeinfo and preexisting attribute (if it exists)
    nsAutoString name;
    nsCOMPtr<nsINodeInfo> ni;

    // SetNamedItemNS()
    if (aWithNS) {
      // Return existing attribute, if present
      ni = iAttribute->NodeInfo();

      if (mContent->HasAttr(ni->NamespaceID(), ni->NameAtom())) {
        rv = RemoveAttribute(ni, getter_AddRefs(tmpReturn));
        NS_ENSURE_SUCCESS(rv, rv);
      }
    }
    else { // SetNamedItem()
      attribute->GetName(name);

      // get node-info of old attribute
      ni = mContent->GetExistingAttrNameFromQName(name);
      if (ni) {
        rv = RemoveAttribute(ni, getter_AddRefs(tmpReturn));
        NS_ENSURE_SUCCESS(rv, rv);
      }
      else {
        if (mContent->IsInHTMLDocument() &&
            mContent->IsHTML()) {
          nsContentUtils::ASCIIToLower(name);
        }

        rv = mContent->NodeInfo()->NodeInfoManager()->
          GetNodeInfo(name, nsnull, kNameSpaceID_None,
                      nsIDOMNode::ATTRIBUTE_NODE, getter_AddRefs(ni));
        NS_ENSURE_SUCCESS(rv, rv);
        // value is already empty
      }
    }

    nsAutoString value;
    attribute->GetValue(value);

    // Add the new attribute to the attribute map before updating
    // its value in the element. @see bug 364413.
    nsAttrKey attrkey(ni->NamespaceID(), ni->NameAtom());
    mAttributeCache.Put(attrkey, attribute);
    iAttribute->SetMap(this);

    rv = mContent->SetAttr(ni->NamespaceID(), ni->NameAtom(),
                           ni->GetPrefixAtom(), value, true);
    if (NS_FAILED(rv)) {
      DropAttribute(ni->NamespaceID(), ni->NameAtom());
    }
  }

  tmpReturn.swap(*aReturn); // transfers ref.

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

    NS_ADDREF(*aReturn = GetAttribute(ni, true));

    // This removes the attribute node from the attribute map.
    rv = mContent->UnsetAttr(ni->NamespaceID(), ni->NameAtom(), true);
  }

  return rv;
}


nsDOMAttribute*
nsDOMAttributeMap::GetItemAt(PRUint32 aIndex, nsresult *aResult)
{
  *aResult = NS_OK;

  nsDOMAttribute* node = nsnull;

  const nsAttrName* name;
  if (mContent && (name = mContent->GetAttrNameAt(aIndex))) {
    // Don't use the nodeinfo even if one exists since it can
    // have the wrong owner document.
    nsCOMPtr<nsINodeInfo> ni;
    ni = mContent->NodeInfo()->NodeInfoManager()->
      GetNodeInfo(name->LocalName(), name->GetPrefix(), name->NamespaceID(),
                  nsIDOMNode::ATTRIBUTE_NODE);
    if (ni) {
      node = GetAttribute(ni, true);
    }
    else {
      *aResult = NS_ERROR_OUT_OF_MEMORY;
    }
  }

  return node;
}

NS_IMETHODIMP
nsDOMAttributeMap::Item(PRUint32 aIndex, nsIDOMNode** aReturn)
{
  nsresult rv;
  NS_IF_ADDREF(*aReturn = GetItemAt(aIndex, &rv));
  return rv;
}

nsresult
nsDOMAttributeMap::GetLength(PRUint32 *aLength)
{
  NS_ENSURE_ARG_POINTER(aLength);

  if (mContent) {
    *aLength = mContent->GetAttrCount();
  }
  else {
    *aLength = 0;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMAttributeMap::GetNamedItemNS(const nsAString& aNamespaceURI,
                                  const nsAString& aLocalName,
                                  nsIDOMNode** aReturn)
{
  return GetNamedItemNSInternal(aNamespaceURI, aLocalName, aReturn);
}

nsresult
nsDOMAttributeMap::GetNamedItemNSInternal(const nsAString& aNamespaceURI,
                                          const nsAString& aLocalName,
                                          nsIDOMNode** aReturn,
                                          bool aRemove)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nsnull;

  if (!mContent) {
    return NS_OK;
  }

  PRInt32 nameSpaceID = kNameSpaceID_None;

  if (!aNamespaceURI.IsEmpty()) {
    nameSpaceID =
      nsContentUtils::NameSpaceManager()->GetNameSpaceID(aNamespaceURI);

    if (nameSpaceID == kNameSpaceID_Unknown) {
      return NS_OK;
    }
  }

  PRUint32 i, count = mContent->GetAttrCount();
  for (i = 0; i < count; ++i) {
    const nsAttrName* name = mContent->GetAttrNameAt(i);
    PRInt32 attrNS = name->NamespaceID();
    nsIAtom* nameAtom = name->LocalName();

    if (nameSpaceID == attrNS &&
        nameAtom->Equals(aLocalName)) {
      nsCOMPtr<nsINodeInfo> ni;
      ni = mContent->NodeInfo()->NodeInfoManager()->
        GetNodeInfo(nameAtom, name->GetPrefix(), nameSpaceID,
                    nsIDOMNode::ATTRIBUTE_NODE);
      NS_ENSURE_TRUE(ni, NS_ERROR_OUT_OF_MEMORY);

      if (aRemove) {
        return RemoveAttribute(ni, aReturn);
      }

      NS_ADDREF(*aReturn = GetAttribute(ni, true));

      return NS_OK;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMAttributeMap::RemoveNamedItemNS(const nsAString& aNamespaceURI,
                                     const nsAString& aLocalName,
                                     nsIDOMNode** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nsnull;

  nsresult rv = GetNamedItemNSInternal(aNamespaceURI,
                                       aLocalName,
                                       aReturn,
                                       true);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!*aReturn) {
    return NS_ERROR_DOM_NOT_FOUND_ERR;
  }

  nsCOMPtr<nsIAttribute> attr = do_QueryInterface(*aReturn);
  NS_ASSERTION(attr, "attribute returned from nsDOMAttributeMap::GetNameItemNS "
               "didn't implement nsIAttribute");
  NS_ENSURE_TRUE(attr, NS_ERROR_UNEXPECTED);

  nsINodeInfo *ni = attr->NodeInfo();
  mContent->UnsetAttr(ni->NamespaceID(), ni->NameAtom(), true);

  return NS_OK;
}

PRUint32
nsDOMAttributeMap::Count() const
{
  return mAttributeCache.Count();
}

PRUint32
nsDOMAttributeMap::Enumerate(AttrCache::EnumReadFunction aFunc,
                             void *aUserArg) const
{
  return mAttributeCache.EnumerateRead(aFunc, aUserArg);
}
