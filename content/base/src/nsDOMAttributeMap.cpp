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
#include "mozilla/dom/Element.h"
#include "nsIDocument.h"
#include "nsINameSpaceManager.h"
#include "nsError.h"
#include "nsContentUtils.h"
#include "nsNodeInfoManager.h"
#include "nsAttrName.h"
#include "nsUnicharUtils.h"

using namespace mozilla;

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
  aData->SetMap(nullptr);

  return PL_DHASH_REMOVE;
}

nsDOMAttributeMap::~nsDOMAttributeMap()
{
  mAttributeCache.Enumerate(RemoveMapRef, nullptr);
}

void
nsDOMAttributeMap::DropReference()
{
  mAttributeCache.Enumerate(RemoveMapRef, nullptr);
  mContent = nullptr;
}

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
  uint32_t n = mAttributeCache.Enumerate(SetOwnerDocumentFunc, aDocument);
  NS_ENSURE_TRUE(n == mAttributeCache.Count(), NS_ERROR_FAILURE);

  return NS_OK;
}

void
nsDOMAttributeMap::DropAttribute(int32_t aNamespaceID, nsIAtom* aLocalName)
{
  nsAttrKey attr(aNamespaceID, aLocalName);
  nsDOMAttribute *node = mAttributeCache.GetWeak(attr);
  if (node) {
    // Break link to map
    node->SetMap(nullptr);

    // Remove from cache
    mAttributeCache.Remove(attr);
  }
}

already_AddRefed<nsDOMAttribute>
nsDOMAttributeMap::RemoveAttribute(nsINodeInfo* aNodeInfo)
{
  NS_ASSERTION(aNodeInfo, "RemoveAttribute() called with aNodeInfo == nullptr!");

  nsAttrKey attr(aNodeInfo->NamespaceID(), aNodeInfo->NameAtom());

  nsRefPtr<nsDOMAttribute> node;
  if (!mAttributeCache.Get(attr, getter_AddRefs(node))) {
    nsAutoString value;
    // As we are removing the attribute we need to set the current value in
    // the attribute node.
    mContent->GetAttr(aNodeInfo->NamespaceID(), aNodeInfo->NameAtom(), value);
    nsCOMPtr<nsINodeInfo> ni = aNodeInfo;
    node = new nsDOMAttribute(nullptr, ni.forget(), value, true);
  }
  else {
    // Break link to map
    node->SetMap(nullptr);

    // Remove from cache
    mAttributeCache.Remove(attr);
  }

  return node.forget();
}

nsDOMAttribute*
nsDOMAttributeMap::GetAttribute(nsINodeInfo* aNodeInfo, bool aNsAware)
{
  NS_ASSERTION(aNodeInfo, "GetAttribute() called with aNodeInfo == nullptr!");

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
nsDOMAttributeMap::GetNamedItem(const nsAString& aAttrName)
{
  if (mContent) {
    nsCOMPtr<nsINodeInfo> ni =
      mContent->GetExistingAttrNameFromQName(aAttrName);
    if (ni) {
      return GetAttribute(ni, false);
    }
  }

  return nullptr;
}

NS_IMETHODIMP
nsDOMAttributeMap::GetNamedItem(const nsAString& aAttrName,
                                nsIDOMNode** aAttribute)
{
  NS_ENSURE_ARG_POINTER(aAttribute);

  NS_IF_ADDREF(*aAttribute = GetNamedItem(aAttrName));

  return NS_OK;
}

NS_IMETHODIMP
nsDOMAttributeMap::SetNamedItem(nsIDOMNode *aNode, nsIDOMNode **aReturn)
{
  ErrorResult rv;
  *aReturn = SetNamedItemInternal(aNode, false, rv).get();
  return rv.ErrorCode();
}

NS_IMETHODIMP
nsDOMAttributeMap::SetNamedItemNS(nsIDOMNode *aNode, nsIDOMNode **aReturn)
{
  ErrorResult rv;
  *aReturn = SetNamedItemInternal(aNode, true, rv).get();
  return rv.ErrorCode();
}

already_AddRefed<nsDOMAttribute>
nsDOMAttributeMap::SetNamedItemInternal(nsIDOMNode *aNode,
                                        bool aWithNS,
                                        ErrorResult& aError)
{
  if (mContent) {
    // XXX should check same-origin between mContent and aNode however
    // nsContentUtils::CheckSameOrigin can't deal with attributenodes yet
    
    nsCOMPtr<nsIAttribute> iAttribute(do_QueryInterface(aNode));
    if (!iAttribute) {
      aError.Throw(NS_ERROR_DOM_HIERARCHY_REQUEST_ERR);
      return nullptr;
    }

    nsDOMAttribute *attribute = static_cast<nsDOMAttribute*>(iAttribute.get());

    // Check that attribute is not owned by somebody else
    nsDOMAttributeMap* owner = iAttribute->GetMap();
    if (owner) {
      if (owner != this) {
        aError.Throw(NS_ERROR_DOM_INUSE_ATTRIBUTE_ERR);
        return nullptr;
      }

      // setting a preexisting attribute is a no-op, just return the same
      // node.
      NS_ADDREF(attribute);
      return attribute;
    }

    nsresult rv;
    if (!mContent->HasSameOwnerDoc(iAttribute)) {
      nsCOMPtr<nsIDOMDocument> domDoc =
        do_QueryInterface(mContent->OwnerDoc(), &rv);
      if (NS_FAILED(rv)) {
        aError.Throw(rv);
        return nullptr;
      }

      nsCOMPtr<nsIDOMNode> adoptedNode;
      rv = domDoc->AdoptNode(aNode, getter_AddRefs(adoptedNode));
      if (NS_FAILED(rv)) {
        aError.Throw(rv);
        return nullptr;
      }

      NS_ASSERTION(adoptedNode == aNode, "Uh, adopt node changed nodes?");
    }

    // Get nodeinfo and preexisting attribute (if it exists)
    nsAutoString name;
    nsCOMPtr<nsINodeInfo> ni;

    nsRefPtr<nsDOMAttribute> attr;
    // SetNamedItemNS()
    if (aWithNS) {
      // Return existing attribute, if present
      ni = iAttribute->NodeInfo();

      if (mContent->HasAttr(ni->NamespaceID(), ni->NameAtom())) {
        attr = RemoveAttribute(ni);
      }
    }
    else { // SetNamedItem()
      attribute->GetName(name);

      // get node-info of old attribute
      ni = mContent->GetExistingAttrNameFromQName(name);
      if (ni) {
        attr = RemoveAttribute(ni);
      }
      else {
        if (mContent->IsInHTMLDocument() &&
            mContent->IsHTML()) {
          nsContentUtils::ASCIIToLower(name);
        }

        rv = mContent->NodeInfo()->NodeInfoManager()->
          GetNodeInfo(name, nullptr, kNameSpaceID_None,
                      nsIDOMNode::ATTRIBUTE_NODE, getter_AddRefs(ni));
        if (NS_FAILED(rv)) {
          aError.Throw(rv);
          return nullptr;
        }
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
      aError.Throw(rv);
      DropAttribute(ni->NamespaceID(), ni->NameAtom());
    }

    return attr.forget();
  }

  return nullptr;
}

NS_IMETHODIMP
nsDOMAttributeMap::RemoveNamedItem(const nsAString& aName,
                                   nsIDOMNode** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nullptr;

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
nsDOMAttributeMap::GetItemAt(uint32_t aIndex, nsresult *aResult)
{
  *aResult = NS_OK;

  nsDOMAttribute* node = nullptr;

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
nsDOMAttributeMap::Item(uint32_t aIndex, nsIDOMNode** aReturn)
{
  nsresult rv;
  NS_IF_ADDREF(*aReturn = GetItemAt(aIndex, &rv));
  return rv;
}

nsresult
nsDOMAttributeMap::GetLength(uint32_t *aLength)
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
  ErrorResult rv;
  NS_IF_ADDREF(*aReturn = GetNamedItemNS(aNamespaceURI, aLocalName, rv));
  return rv.ErrorCode();
}

nsDOMAttribute*
nsDOMAttributeMap::GetNamedItemNS(const nsAString& aNamespaceURI,
                                  const nsAString& aLocalName,
                                  ErrorResult& aError)
{
  nsCOMPtr<nsINodeInfo> ni = GetAttrNodeInfo(aNamespaceURI, aLocalName, aError);
  if (!ni) {
    return nullptr;
  }

  return GetAttribute(ni, true);
}

already_AddRefed<nsINodeInfo>
nsDOMAttributeMap::GetAttrNodeInfo(const nsAString& aNamespaceURI,
                                   const nsAString& aLocalName,
                                   mozilla::ErrorResult& aError)
{
  if (!mContent) {
    return nullptr;
  }

  int32_t nameSpaceID = kNameSpaceID_None;

  if (!aNamespaceURI.IsEmpty()) {
    nameSpaceID =
      nsContentUtils::NameSpaceManager()->GetNameSpaceID(aNamespaceURI);

    if (nameSpaceID == kNameSpaceID_Unknown) {
      return nullptr;
    }
  }

  uint32_t i, count = mContent->GetAttrCount();
  for (i = 0; i < count; ++i) {
    const nsAttrName* name = mContent->GetAttrNameAt(i);
    int32_t attrNS = name->NamespaceID();
    nsIAtom* nameAtom = name->LocalName();

    if (nameSpaceID == attrNS &&
        nameAtom->Equals(aLocalName)) {
      nsCOMPtr<nsINodeInfo> ni;
      ni = mContent->NodeInfo()->NodeInfoManager()->
        GetNodeInfo(nameAtom, name->GetPrefix(), nameSpaceID,
                    nsIDOMNode::ATTRIBUTE_NODE);
      if (!ni) {
        aError.Throw(NS_ERROR_OUT_OF_MEMORY);
      }

      return ni.forget();
    }
  }

  return nullptr;
}

NS_IMETHODIMP
nsDOMAttributeMap::RemoveNamedItemNS(const nsAString& aNamespaceURI,
                                     const nsAString& aLocalName,
                                     nsIDOMNode** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nullptr;

  ErrorResult error;
  nsCOMPtr<nsINodeInfo> ni = GetAttrNodeInfo(aNamespaceURI, aLocalName, error);
  if (error.Failed()) {
    return error.ErrorCode();
  }

  if (!ni) {
    return NS_ERROR_DOM_NOT_FOUND_ERR;
  }

  nsRefPtr<nsDOMAttribute> attr = RemoveAttribute(ni);
  nsINodeInfo *attrNi = attr->NodeInfo();
  mContent->UnsetAttr(attrNi->NamespaceID(), attrNi->NameAtom(), true);

  attr.forget(aReturn);
  return NS_OK;
}

uint32_t
nsDOMAttributeMap::Count() const
{
  return mAttributeCache.Count();
}

uint32_t
nsDOMAttributeMap::Enumerate(AttrCache::EnumReadFunction aFunc,
                             void *aUserArg) const
{
  return mAttributeCache.EnumerateRead(aFunc, aUserArg);
}

size_t
AttrCacheSizeEnumerator(const nsAttrKey& aKey,
                        const nsRefPtr<nsDOMAttribute>& aValue,
                        nsMallocSizeOfFun aMallocSizeOf,
                        void* aUserArg)
{
  return aMallocSizeOf(aValue.get());
}

size_t
nsDOMAttributeMap::SizeOfIncludingThis(nsMallocSizeOfFun aMallocSizeOf) const
{
  size_t n = aMallocSizeOf(this);
  n += mAttributeCache.SizeOfExcludingThis(AttrCacheSizeEnumerator,
                                           aMallocSizeOf);

  // NB: mContent is non-owning and thus not counted.
  return n;
}
