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
#include "mozilla/dom/NodeInfoInlines.h"
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

nsDOMAttributeMap::~nsDOMAttributeMap()
{
  DropReference();
}

void
nsDOMAttributeMap::DropReference()
{
  for (auto iter = mAttributeCache.Iter(); !iter.Done(); iter.Next()) {
    iter.Data()->SetMap(nullptr);
    iter.Remove();
  }
  mContent = nullptr;
}

NS_IMPL_CYCLE_COLLECTION_CLASS(nsDOMAttributeMap)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsDOMAttributeMap)
  tmp->DropReference();
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mContent)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END


NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsDOMAttributeMap)
  for (auto iter = tmp->mAttributeCache.Iter(); !iter.Done(); iter.Next()) {
    cb.NoteXPCOMChild(static_cast<nsINode*>(iter.Data().get()));
  }
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

nsresult
nsDOMAttributeMap::SetOwnerDocument(nsIDocument* aDocument)
{
  for (auto iter = mAttributeCache.Iter(); !iter.Done(); iter.Next()) {
    nsresult rv = iter.Data()->SetOwnerDocument(aDocument);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);
  }
  return NS_OK;
}

void
nsDOMAttributeMap::DropAttribute(int32_t aNamespaceID, nsIAtom* aLocalName)
{
  nsAttrKey attr(aNamespaceID, aLocalName);
  Attr *node = mAttributeCache.GetWeak(attr);
  if (node) {
    // Break link to map
    node->SetMap(nullptr);

    // Remove from cache
    mAttributeCache.Remove(attr);
  }
}

Attr*
nsDOMAttributeMap::GetAttribute(mozilla::dom::NodeInfo* aNodeInfo)
{
  NS_ASSERTION(aNodeInfo, "GetAttribute() called with aNodeInfo == nullptr!");

  nsAttrKey attr(aNodeInfo->NamespaceID(), aNodeInfo->NameAtom());

  Attr* node = mAttributeCache.GetWeak(attr);
  if (!node) {
    RefPtr<mozilla::dom::NodeInfo> ni = aNodeInfo;
    RefPtr<Attr> newAttr =
      new Attr(this, ni.forget(), EmptyString());
    mAttributeCache.Put(attr, newAttr);
    node = newAttr;
  }

  return node;
}

Attr*
nsDOMAttributeMap::NamedGetter(const nsAString& aAttrName, bool& aFound)
{
  aFound = false;
  NS_ENSURE_TRUE(mContent, nullptr);

  RefPtr<mozilla::dom::NodeInfo> ni = mContent->GetExistingAttrNameFromQName(aAttrName);
  if (!ni) {
    return nullptr;
  }

  aFound = true;
  return GetAttribute(ni);
}

void
nsDOMAttributeMap::GetSupportedNames(nsTArray<nsString>& aNames)
{
  // For HTML elements in HTML documents, only include names that are still the
  // same after ASCII-lowercasing, since our named getter will end up
  // ASCII-lowercasing the given string.
  bool lowercaseNamesOnly =
    mContent->IsHTMLElement() && mContent->IsInHTMLDocument();

  const uint32_t count = mContent->GetAttrCount();
  bool seenNonAtomName = false;
  for (uint32_t i = 0; i < count; i++) {
    const nsAttrName* name = mContent->GetAttrNameAt(i);
    seenNonAtomName = seenNonAtomName || !name->IsAtom();
    nsString qualifiedName;
    name->GetQualifiedName(qualifiedName);

    if (lowercaseNamesOnly &&
        nsContentUtils::StringContainsASCIIUpper(qualifiedName)) {
      continue;
    }

    // Omit duplicates.  We only need to do this check if we've seen a non-atom
    // name, because that's the only way we can have two identical qualified
    // names.
    if (seenNonAtomName && aNames.Contains(qualifiedName)) {
      continue;
    }

    aNames.AppendElement(qualifiedName);
  }
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

NS_IMETHODIMP
nsDOMAttributeMap::SetNamedItem(nsIDOMAttr* aAttr, nsIDOMAttr** aReturn)
{
  Attr* attribute = static_cast<Attr*>(aAttr);
  NS_ENSURE_ARG(attribute);

  ErrorResult rv;
  *aReturn = SetNamedItemNS(*attribute, rv).take();
  return rv.StealNSResult();
}

NS_IMETHODIMP
nsDOMAttributeMap::SetNamedItemNS(nsIDOMAttr* aAttr, nsIDOMAttr** aReturn)
{
  Attr* attribute = static_cast<Attr*>(aAttr);
  NS_ENSURE_ARG(attribute);

  ErrorResult rv;
  *aReturn = SetNamedItemNS(*attribute, rv).take();
  return rv.StealNSResult();
}

already_AddRefed<Attr>
nsDOMAttributeMap::SetNamedItemNS(Attr& aAttr, ErrorResult& aError)
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
    RefPtr<Attr> attribute = &aAttr;
    return attribute.forget();
  }

  nsresult rv;
  if (mContent->OwnerDoc() != aAttr.OwnerDoc()) {
    DebugOnly<void*> adoptedNode =
      mContent->OwnerDoc()->AdoptNode(aAttr, aError);
    if (aError.Failed()) {
      return nullptr;
    }

    NS_ASSERTION(adoptedNode == &aAttr, "Uh, adopt node changed nodes?");
  }

  // Get nodeinfo and preexisting attribute (if it exists)
  RefPtr<NodeInfo> oldNi;

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

  RefPtr<Attr> oldAttr;

  if (oldNi) {
    oldAttr = GetAttribute(oldNi);

    if (oldAttr == &aAttr) {
      return oldAttr.forget();
    }

    if (oldAttr) {
      // Just remove it from our hashtable.  This has no side-effects, so we
      // don't have to recheck anything after we do it.  Then we'll add our new
      // Attr to the hashtable and do the actual attr set on the element.  This
      // will make the whole thing look like a single attribute mutation (with
      // the new attr node in place) as opposed to a removal and addition.
      DropAttribute(oldNi->NamespaceID(), oldNi->NameAtom());
    }
  }

  nsAutoString value;
  aAttr.GetValue(value);

  RefPtr<NodeInfo> ni = aAttr.NodeInfo();

  // Add the new attribute to the attribute map before updating
  // its value in the element. @see bug 364413.
  nsAttrKey attrkey(ni->NamespaceID(), ni->NameAtom());
  mAttributeCache.Put(attrkey, &aAttr);
  aAttr.SetMap(this);

  rv = mContent->SetAttr(ni->NamespaceID(), ni->NameAtom(),
                         ni->GetPrefixAtom(), value, true);
  if (NS_FAILED(rv)) {
    aError.Throw(rv);
    DropAttribute(ni->NamespaceID(), ni->NameAtom());
  }

  return oldAttr.forget();
}

already_AddRefed<Attr>
nsDOMAttributeMap::RemoveNamedItem(NodeInfo* aNodeInfo, ErrorResult& aError)
{
  RefPtr<Attr> attribute = GetAttribute(aNodeInfo);
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
  return rv.StealNSResult();
}

already_AddRefed<Attr>
nsDOMAttributeMap::RemoveNamedItem(const nsAString& aName, ErrorResult& aError)
{
  if (!mContent) {
    aError.Throw(NS_ERROR_DOM_NOT_FOUND_ERR);
    return nullptr;
  }

  RefPtr<mozilla::dom::NodeInfo> ni = mContent->GetExistingAttrNameFromQName(aName);
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
  RefPtr<mozilla::dom::NodeInfo> ni = mContent->NodeInfo()->NodeInfoManager()->
    GetNodeInfo(name->LocalName(), name->GetPrefix(), name->NamespaceID(),
                nsIDOMNode::ATTRIBUTE_NODE);
  return GetAttribute(ni);
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
  RefPtr<mozilla::dom::NodeInfo> ni = GetAttrNodeInfo(aNamespaceURI, aLocalName);
  if (!ni) {
    return nullptr;
  }

  return GetAttribute(ni);
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
      nsContentUtils::NameSpaceManager()->GetNameSpaceID(aNamespaceURI,
                                                         nsContentUtils::IsChromeDoc(mContent->OwnerDoc()));

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
      RefPtr<mozilla::dom::NodeInfo> ni;
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
  return rv.StealNSResult();
}

already_AddRefed<Attr>
nsDOMAttributeMap::RemoveNamedItemNS(const nsAString& aNamespaceURI,
                                     const nsAString& aLocalName,
                                     ErrorResult& aError)
{
  RefPtr<mozilla::dom::NodeInfo> ni = GetAttrNodeInfo(aNamespaceURI, aLocalName);
  if (!ni) {
    aError.Throw(NS_ERROR_DOM_NOT_FOUND_ERR);
    return nullptr;
  }

  return RemoveNamedItem(ni, aError);
}

uint32_t
nsDOMAttributeMap::Count() const
{
  return mAttributeCache.Count();
}

size_t
nsDOMAttributeMap::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  size_t n = aMallocSizeOf(this);

  n += mAttributeCache.ShallowSizeOfExcludingThis(aMallocSizeOf);
  for (auto iter = mAttributeCache.ConstIter(); !iter.Done(); iter.Next()) {
    n += aMallocSizeOf(iter.Data().get());
  }

  // NB: mContent is non-owning and thus not counted.
  return n;
}

/* virtual */ JSObject*
nsDOMAttributeMap::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return NamedNodeMapBinding::Wrap(aCx, this, aGivenProto);
}
