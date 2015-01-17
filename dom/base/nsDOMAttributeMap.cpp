/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Implementation of the |attributes| property of DOM Core's Element object.
 */

#include "nsDOMAttributeMap.h"

#include "mozilla/MemoryReporting.h"
#include "mozilla/dom/Attr.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/NamedNodeMapBinding.h"
#include "nsAttrName.h"
#include "nsContentUtils.h"
#include "nsError.h"
#include "nsIContentInlines.h"
#include "nsIDocument.h"
#include "nsNameSpaceManager.h"
#include "nsNodeInfoManager.h"
#include "nsUnicharUtils.h"
#include "nsWrapperCacheInlines.h"

using namespace mozilla;
using namespace mozilla::dom;

//----------------------------------------------------------------------

nsDOMAttributeMap::nsDOMAttributeMap(Element* aContent)
  : mContent(aContent)
{
  // We don't add a reference to our content. If it goes away,
  // we'll be told to drop our reference
}

/**
 * Clear map pointer for attributes.
 */
PLDHashOperator
RemoveMapRef(nsAttrHashKey::KeyType aKey, nsRefPtr<Attr>& aData,
             void* aUserArg)
{
  aData->SetMap(nullptr);

  return PL_DHASH_REMOVE;
}

nsDOMAttributeMap::~nsDOMAttributeMap()
{
  if (mAttributeCache) {
    mAttributeCache->Enumerate(RemoveMapRef, nullptr);
  }
}

void
nsDOMAttributeMap::DropReference()
{
  if (mAttributeCache) {
    mAttributeCache->Enumerate(RemoveMapRef, nullptr);
  }
  mContent = nullptr;
}

NS_IMPL_CYCLE_COLLECTION_CLASS(nsDOMAttributeMap)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsDOMAttributeMap)
  tmp->DropReference();
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mContent)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END


PLDHashOperator
TraverseMapEntry(nsAttrHashKey::KeyType aKey, nsRefPtr<Attr>& aData,
                 void* aUserArg)
{
  nsCycleCollectionTraversalCallback *cb = 
    static_cast<nsCycleCollectionTraversalCallback*>(aUserArg);

  cb->NoteXPCOMChild(static_cast<nsINode*>(aData.get()));

  return PL_DHASH_NEXT;
}

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsDOMAttributeMap)
  if (tmp->mAttributeCache) {
    tmp->mAttributeCache->Enumerate(TraverseMapEntry, &cb);
  }
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mContent)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_WRAPPERCACHE(nsDOMAttributeMap)

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_BEGIN(nsDOMAttributeMap)
  if (tmp->IsBlack()) {
    if (tmp->mContent) {
      // The map owns the element so we can mark it when the
      // map itself is certainly alive.
      mozilla::dom::FragmentOrElement::MarkNodeChildren(tmp->mContent);
    }
    return true;
  }
  if (tmp->mContent &&
      mozilla::dom::FragmentOrElement::CanSkip(tmp->mContent, true)) {
    return true;
  }
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_BEGIN(nsDOMAttributeMap)
  return tmp->IsBlackAndDoesNotNeedTracing(tmp);
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_BEGIN(nsDOMAttributeMap)
  return tmp->IsBlack();
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_END

// QueryInterface implementation for nsDOMAttributeMap
NS_INTERFACE_TABLE_HEAD(nsDOMAttributeMap)
  NS_INTERFACE_TABLE(nsDOMAttributeMap, nsIDOMMozNamedAttrMap)
  NS_INTERFACE_TABLE_TO_MAP_SEGUE
  NS_WRAPPERCACHE_INTERFACE_TABLE_ENTRY
  NS_INTERFACE_MAP_ENTRIES_CYCLE_COLLECTION(nsDOMAttributeMap)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsDOMAttributeMap)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsDOMAttributeMap)

PLDHashOperator
SetOwnerDocumentFunc(nsAttrHashKey::KeyType aKey,
                     nsRefPtr<Attr>& aData,
                     void* aUserArg)
{
  nsresult rv = aData->SetOwnerDocument(static_cast<nsIDocument*>(aUserArg));

  return NS_FAILED(rv) ? PL_DHASH_STOP : PL_DHASH_NEXT;
}

nsresult
nsDOMAttributeMap::SetOwnerDocument(nsIDocument* aDocument)
{
  if (mAttributeCache) {
    uint32_t n = mAttributeCache->Enumerate(SetOwnerDocumentFunc, aDocument);
    NS_ENSURE_TRUE(n == mAttributeCache->Count(), NS_ERROR_FAILURE);
  }
  return NS_OK;
}

void
nsDOMAttributeMap::DropAttribute(int32_t aNamespaceID, nsIAtom* aLocalName)
{
  nsAttrKey attr(aNamespaceID, aLocalName);
  if (mAttributeCache) {
    Attr *node = mAttributeCache->GetWeak(attr);
    if (node) {
      // Break link to map
      node->SetMap(nullptr);

      // Remove from cache
      mAttributeCache->Remove(attr);
    }
  }
}

already_AddRefed<Attr>
nsDOMAttributeMap::RemoveAttribute(mozilla::dom::NodeInfo* aNodeInfo)
{
  NS_ASSERTION(aNodeInfo, "RemoveAttribute() called with aNodeInfo == nullptr!");

  nsAttrKey attr(aNodeInfo->NamespaceID(), aNodeInfo->NameAtom());

  nsRefPtr<Attr> node;
  if (mAttributeCache && mAttributeCache->Get(attr, getter_AddRefs(node))) {
    // Break link to map
    node->SetMap(nullptr);

    // Remove from cache
    mAttributeCache->Remove(attr);
  } else {
    nsAutoString value;
    // As we are removing the attribute we need to set the current value in
    // the attribute node.
    mContent->GetAttr(aNodeInfo->NamespaceID(), aNodeInfo->NameAtom(), value);
    nsRefPtr<mozilla::dom::NodeInfo> ni = aNodeInfo;
    node = new Attr(nullptr, ni.forget(), value, true);
  }

  return node.forget();
}

Attr*
nsDOMAttributeMap::GetAttribute(mozilla::dom::NodeInfo* aNodeInfo, bool aNsAware)
{
  NS_ASSERTION(aNodeInfo, "GetAttribute() called with aNodeInfo == nullptr!");

  nsAttrKey attr(aNodeInfo->NamespaceID(), aNodeInfo->NameAtom());

  EnsureAttributeCache();
  Attr* node = mAttributeCache->GetWeak(attr);
  if (!node) {
    nsRefPtr<mozilla::dom::NodeInfo> ni = aNodeInfo;
    nsRefPtr<Attr> newAttr =
      new Attr(this, ni.forget(), EmptyString(), aNsAware);
    mAttributeCache->Put(attr, newAttr);
    node = newAttr;
  }

  return node;
}

Attr*
nsDOMAttributeMap::NamedGetter(const nsAString& aAttrName, bool& aFound)
{
  aFound = false;
  NS_ENSURE_TRUE(mContent, nullptr);

  nsRefPtr<mozilla::dom::NodeInfo> ni = mContent->GetExistingAttrNameFromQName(aAttrName);
  if (!ni) {
    return nullptr;
  }

  aFound = true;
  return GetAttribute(ni, false);
}

bool
nsDOMAttributeMap::NameIsEnumerable(const nsAString& aName)
{
  return true;
}

Attr*
nsDOMAttributeMap::GetNamedItem(const nsAString& aAttrName)
{
  bool dummy;
  return NamedGetter(aAttrName, dummy);
}

NS_IMETHODIMP
nsDOMAttributeMap::GetNamedItem(const nsAString& aAttrName,
                                nsIDOMAttr** aAttribute)
{
  NS_ENSURE_ARG_POINTER(aAttribute);

  NS_IF_ADDREF(*aAttribute = GetNamedItem(aAttrName));

  return NS_OK;
}

void
nsDOMAttributeMap::EnsureAttributeCache()
{
  if (!mAttributeCache) {
    mAttributeCache = MakeUnique<AttrCache>();
  }
}

NS_IMETHODIMP
nsDOMAttributeMap::SetNamedItem(nsIDOMAttr* aAttr, nsIDOMAttr** aReturn)
{
  Attr* attribute = static_cast<Attr*>(aAttr);
  NS_ENSURE_ARG(attribute);

  ErrorResult rv;
  *aReturn = SetNamedItem(*attribute, rv).take();
  return rv.ErrorCode();
}

NS_IMETHODIMP
nsDOMAttributeMap::SetNamedItemNS(nsIDOMAttr* aAttr, nsIDOMAttr** aReturn)
{
  Attr* attribute = static_cast<Attr*>(aAttr);
  NS_ENSURE_ARG(attribute);

  ErrorResult rv;
  *aReturn = SetNamedItemNS(*attribute, rv).take();
  return rv.ErrorCode();
}

already_AddRefed<Attr>
nsDOMAttributeMap::SetNamedItemInternal(Attr& aAttr,
                                        bool aWithNS,
                                        ErrorResult& aError)
{
  NS_ENSURE_TRUE(mContent, nullptr);

  // XXX should check same-origin between mContent and aAttr however
  // nsContentUtils::CheckSameOrigin can't deal with attributenodes yet

  // Check that attribute is not owned by somebody else
  nsDOMAttributeMap* owner = aAttr.GetMap();
  if (owner) {
    if (owner != this) {
      aError.Throw(NS_ERROR_DOM_INUSE_ATTRIBUTE_ERR);
      return nullptr;
    }

    // setting a preexisting attribute is a no-op, just return the same
    // node.
    nsRefPtr<Attr> attribute = &aAttr;
    return attribute.forget();
  }

  nsresult rv;
  if (mContent->OwnerDoc() != aAttr.OwnerDoc()) {
    nsCOMPtr<nsINode> adoptedNode =
      mContent->OwnerDoc()->AdoptNode(aAttr, aError);
    if (aError.Failed()) {
      return nullptr;
    }

    NS_ASSERTION(adoptedNode == &aAttr, "Uh, adopt node changed nodes?");
  }

  // Get nodeinfo and preexisting attribute (if it exists)
  nsRefPtr<NodeInfo> oldNi;

  if (!aWithNS) {
    nsAutoString name;
    aAttr.GetName(name);
    oldNi = mContent->GetExistingAttrNameFromQName(name);
  }
  else {
    uint32_t i, count = mContent->GetAttrCount();
    for (i = 0; i < count; ++i) {
      const nsAttrName* name = mContent->GetAttrNameAt(i);
      int32_t attrNS = name->NamespaceID();
      nsIAtom* nameAtom = name->LocalName();

      // we're purposefully ignoring the prefix.
      if (aAttr.NodeInfo()->Equals(nameAtom, attrNS)) {
        oldNi = mContent->NodeInfo()->NodeInfoManager()->
          GetNodeInfo(nameAtom, name->GetPrefix(), aAttr.NodeInfo()->NamespaceID(),
                      nsIDOMNode::ATTRIBUTE_NODE);
        break;
      }
    }
  }

  nsRefPtr<Attr> attr;

  if (oldNi) {
    nsRefPtr<Attr> oldAttr = GetAttribute(oldNi, true);

    if (oldAttr == &aAttr) {
      return oldAttr.forget();
    }

    if (oldAttr) {
      attr = RemoveNamedItem(oldNi, aError);
      NS_ASSERTION(attr->NodeInfo()->NameAndNamespaceEquals(oldNi),
        "RemoveNamedItem() called, attr->NodeInfo() should be equal to oldNi!");
    }
  }

  nsAutoString value;
  aAttr.GetValue(value);

  nsRefPtr<NodeInfo> ni = aAttr.NodeInfo();

  // Add the new attribute to the attribute map before updating
  // its value in the element. @see bug 364413.
  nsAttrKey attrkey(ni->NamespaceID(), ni->NameAtom());
  EnsureAttributeCache();
  mAttributeCache->Put(attrkey, &aAttr);
  aAttr.SetMap(this);

  rv = mContent->SetAttr(ni->NamespaceID(), ni->NameAtom(),
                         ni->GetPrefixAtom(), value, true);
  if (NS_FAILED(rv)) {
    aError.Throw(rv);
    DropAttribute(ni->NamespaceID(), ni->NameAtom());
  }

  return attr.forget();
}

already_AddRefed<Attr>
nsDOMAttributeMap::RemoveNamedItem(NodeInfo* aNodeInfo, ErrorResult& aError)
{
  nsRefPtr<Attr> attribute = GetAttribute(aNodeInfo, true);
  // This removes the attribute node from the attribute map.
  aError = mContent->UnsetAttr(aNodeInfo->NamespaceID(), aNodeInfo->NameAtom(), true);
  return attribute.forget();
}

NS_IMETHODIMP
nsDOMAttributeMap::RemoveNamedItem(const nsAString& aName,
                                   nsIDOMAttr** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);

  ErrorResult rv;
  *aReturn = RemoveNamedItem(aName, rv).take();
  return rv.ErrorCode();
}

already_AddRefed<Attr>
nsDOMAttributeMap::RemoveNamedItem(const nsAString& aName, ErrorResult& aError)
{
  if (!mContent) {
    aError.Throw(NS_ERROR_DOM_NOT_FOUND_ERR);
    return nullptr;
  }

  nsRefPtr<mozilla::dom::NodeInfo> ni = mContent->GetExistingAttrNameFromQName(aName);
  if (!ni) {
    aError.Throw(NS_ERROR_DOM_NOT_FOUND_ERR);
    return nullptr;
  }

  return RemoveNamedItem(ni, aError);
}


Attr*
nsDOMAttributeMap::IndexedGetter(uint32_t aIndex, bool& aFound)
{
  aFound = false;
  NS_ENSURE_TRUE(mContent, nullptr);

  const nsAttrName* name = mContent->GetAttrNameAt(aIndex);
  NS_ENSURE_TRUE(name, nullptr);

  aFound = true;
  // Don't use the nodeinfo even if one exists since it can have the wrong
  // owner document.
  nsRefPtr<mozilla::dom::NodeInfo> ni = mContent->NodeInfo()->NodeInfoManager()->
    GetNodeInfo(name->LocalName(), name->GetPrefix(), name->NamespaceID(),
                nsIDOMNode::ATTRIBUTE_NODE);
  return GetAttribute(ni, true);
}

Attr*
nsDOMAttributeMap::Item(uint32_t aIndex)
{
  bool dummy;
  return IndexedGetter(aIndex, dummy);
}

NS_IMETHODIMP
nsDOMAttributeMap::Item(uint32_t aIndex, nsIDOMAttr** aReturn)
{
  NS_IF_ADDREF(*aReturn = Item(aIndex));
  return NS_OK;
}

uint32_t
nsDOMAttributeMap::Length() const
{
  NS_ENSURE_TRUE(mContent, 0);

  return mContent->GetAttrCount();
}

nsresult
nsDOMAttributeMap::GetLength(uint32_t *aLength)
{
  NS_ENSURE_ARG_POINTER(aLength);
  *aLength = Length();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMAttributeMap::GetNamedItemNS(const nsAString& aNamespaceURI,
                                  const nsAString& aLocalName,
                                  nsIDOMAttr** aReturn)
{
  NS_IF_ADDREF(*aReturn = GetNamedItemNS(aNamespaceURI, aLocalName));
  return NS_OK;
}

Attr*
nsDOMAttributeMap::GetNamedItemNS(const nsAString& aNamespaceURI,
                                  const nsAString& aLocalName)
{
  nsRefPtr<mozilla::dom::NodeInfo> ni = GetAttrNodeInfo(aNamespaceURI, aLocalName);
  if (!ni) {
    return nullptr;
  }

  return GetAttribute(ni, true);
}

already_AddRefed<mozilla::dom::NodeInfo>
nsDOMAttributeMap::GetAttrNodeInfo(const nsAString& aNamespaceURI,
                                   const nsAString& aLocalName)
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

    // we're purposefully ignoring the prefix.
    if (nameSpaceID == attrNS &&
        nameAtom->Equals(aLocalName)) {
      nsRefPtr<mozilla::dom::NodeInfo> ni;
      ni = mContent->NodeInfo()->NodeInfoManager()->
        GetNodeInfo(nameAtom, name->GetPrefix(), nameSpaceID,
                    nsIDOMNode::ATTRIBUTE_NODE);

      return ni.forget();
    }
  }

  return nullptr;
}

NS_IMETHODIMP
nsDOMAttributeMap::RemoveNamedItemNS(const nsAString& aNamespaceURI,
                                     const nsAString& aLocalName,
                                     nsIDOMAttr** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  ErrorResult rv;
  *aReturn = RemoveNamedItemNS(aNamespaceURI, aLocalName, rv).take();
  return rv.ErrorCode();
}

already_AddRefed<Attr>
nsDOMAttributeMap::RemoveNamedItemNS(const nsAString& aNamespaceURI,
                                     const nsAString& aLocalName,
                                     ErrorResult& aError)
{
  nsRefPtr<mozilla::dom::NodeInfo> ni = GetAttrNodeInfo(aNamespaceURI, aLocalName);
  if (!ni) {
    aError.Throw(NS_ERROR_DOM_NOT_FOUND_ERR);
    return nullptr;
  }

  return RemoveNamedItem(ni, aError);
}

uint32_t
nsDOMAttributeMap::Count() const
{
  return mAttributeCache ? mAttributeCache->Count() : 0;
}

uint32_t
nsDOMAttributeMap::Enumerate(AttrCache::EnumReadFunction aFunc,
                             void *aUserArg) const
{
  return mAttributeCache ? mAttributeCache->EnumerateRead(aFunc, aUserArg) : 0;
}

size_t
AttrCacheSizeEnumerator(const nsAttrKey& aKey,
                        const nsRefPtr<Attr>& aValue,
                        MallocSizeOf aMallocSizeOf,
                        void* aUserArg)
{
  return aMallocSizeOf(aValue.get());
}

size_t
nsDOMAttributeMap::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  size_t n = aMallocSizeOf(this);
  n += mAttributeCache
     ? mAttributeCache->SizeOfExcludingThis(AttrCacheSizeEnumerator,
                                            aMallocSizeOf)
     : 0;

  // NB: mContent is non-owning and thus not counted.
  return n;
}

/* virtual */ JSObject*
nsDOMAttributeMap::WrapObject(JSContext* aCx)
{
  return NamedNodeMapBinding::Wrap(aCx, this);
}
