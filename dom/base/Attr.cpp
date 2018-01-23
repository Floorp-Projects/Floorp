/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Implementation of DOM Core's Attr node.
 */

#include "mozilla/dom/Attr.h"
#include "mozilla/dom/AttrBinding.h"
#include "mozilla/dom/Element.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/InternalMutationEvent.h"
#include "nsContentCreatorFunctions.h"
#include "nsError.h"
#include "nsUnicharUtils.h"
#include "nsDOMString.h"
#include "nsIContentInlines.h"
#include "nsIDocument.h"
#include "nsGkAtoms.h"
#include "nsCOMArray.h"
#include "nsNameSpaceManager.h"
#include "nsNodeUtils.h"
#include "nsTextNode.h"
#include "mozAutoDocUpdate.h"
#include "nsWrapperCacheInlines.h"

nsIAttribute::nsIAttribute(nsDOMAttributeMap* aAttrMap,
                           already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
: nsINode(aNodeInfo), mAttrMap(aAttrMap)
{
}

nsIAttribute::~nsIAttribute()
{
}

namespace mozilla {
namespace dom {

//----------------------------------------------------------------------
bool Attr::sInitialized;

Attr::Attr(nsDOMAttributeMap *aAttrMap,
           already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo,
           const nsAString  &aValue)
  : nsIAttribute(aAttrMap, aNodeInfo), mValue(aValue)
{
  MOZ_ASSERT(mNodeInfo, "We must get a nodeinfo here!");
  MOZ_ASSERT(mNodeInfo->NodeType() == nsIDOMNode::ATTRIBUTE_NODE,
             "Wrong nodeType");

  // We don't add a reference to our content. It will tell us
  // to drop our reference when it goes away.
}

NS_IMPL_CYCLE_COLLECTION_CLASS(Attr)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(Attr)
  if (!nsINode::Traverse(tmp, cb)) {
    return NS_SUCCESS_INTERRUPTED_TRAVERSE;
  }
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mAttrMap)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_WRAPPERCACHE(Attr)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(Attr)
  nsINode::Unlink(tmp);
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mAttrMap)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_BEGIN(Attr)
  Element* ownerElement = tmp->GetElement();
  if (tmp->HasKnownLiveWrapper()) {
    if (ownerElement) {
      // The attribute owns the element via attribute map so we can
      // mark it when the attribute is certainly alive.
      mozilla::dom::FragmentOrElement::MarkNodeChildren(ownerElement);
    }
    return true;
  }
  if (ownerElement &&
      mozilla::dom::FragmentOrElement::CanSkip(ownerElement, true)) {
    return true;
  }
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_BEGIN(Attr)
  return tmp->HasKnownLiveWrapperAndDoesNotNeedTracing(static_cast<nsIAttribute*>(tmp));
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_BEGIN(Attr)
  return tmp->HasKnownLiveWrapper();
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_END

// QueryInterface implementation for Attr
NS_INTERFACE_TABLE_HEAD(Attr)
  NS_WRAPPERCACHE_INTERFACE_TABLE_ENTRY
  NS_INTERFACE_TABLE(Attr, nsINode, nsIAttribute, nsIDOMNode,
                     nsIDOMEventTarget, EventTarget)
  NS_INTERFACE_TABLE_TO_MAP_SEGUE_CYCLE_COLLECTION(Attr)
  NS_INTERFACE_MAP_ENTRY_TEAROFF(nsISupportsWeakReference,
                                 new nsNodeSupportsWeakRefTearoff(this))
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(Attr)
NS_IMPL_CYCLE_COLLECTING_RELEASE_WITH_LAST_RELEASE(Attr,
                                                   nsNodeUtils::LastRelease(this))

void
Attr::SetMap(nsDOMAttributeMap *aMap)
{
  if (mAttrMap && !aMap && sInitialized) {
    // We're breaking a relationship with content and not getting a new one,
    // need to locally cache value. GetValue() does that.
    GetValue(mValue);
  }

  mAttrMap = aMap;
}

Element*
Attr::GetElement() const
{
  if (!mAttrMap) {
    return nullptr;
  }
  nsIContent* content = mAttrMap->GetContent();
  return content ? content->AsElement() : nullptr;
}

nsresult
Attr::SetOwnerDocument(nsIDocument* aDocument)
{
  NS_ASSERTION(aDocument, "Missing document");

  nsIDocument *doc = OwnerDoc();
  NS_ASSERTION(doc != aDocument, "bad call to Attr::SetOwnerDocument");
  doc->DeleteAllPropertiesFor(this);

  RefPtr<mozilla::dom::NodeInfo> newNodeInfo;
  newNodeInfo = aDocument->NodeInfoManager()->
    GetNodeInfo(mNodeInfo->NameAtom(), mNodeInfo->GetPrefixAtom(),
                mNodeInfo->NamespaceID(),
                nsIDOMNode::ATTRIBUTE_NODE);
  NS_ASSERTION(newNodeInfo, "GetNodeInfo lies");
  mNodeInfo.swap(newNodeInfo);

  return NS_OK;
}

void
Attr::GetName(nsAString& aName)
{
  aName = NodeName();
}

void
Attr::GetValue(nsAString& aValue)
{
  Element* element = GetElement();
  if (element) {
    RefPtr<nsAtom> nameAtom = mNodeInfo->NameAtom();
    element->GetAttr(mNodeInfo->NamespaceID(), nameAtom, aValue);
  }
  else {
    aValue = mValue;
  }
}

void
Attr::SetValue(const nsAString& aValue, nsIPrincipal* aTriggeringPrincipal, ErrorResult& aRv)
{
  Element* element = GetElement();
  if (!element) {
    mValue = aValue;
    return;
  }

  RefPtr<nsAtom> nameAtom = mNodeInfo->NameAtom();
  aRv = element->SetAttr(mNodeInfo->NamespaceID(),
                         nameAtom,
                         mNodeInfo->GetPrefixAtom(),
                         aValue,
                         aTriggeringPrincipal,
                         true);
}

void
Attr::SetValue(const nsAString& aValue, ErrorResult& aRv)
{
  SetValue(aValue, nullptr, aRv);
}

bool
Attr::Specified() const
{
  return true;
}

Element*
Attr::GetOwnerElement(ErrorResult& aRv)
{
  return GetElement();
}

void
Attr::GetNodeValueInternal(nsAString& aNodeValue)
{
  GetValue(aNodeValue);
}

void
Attr::SetNodeValueInternal(const nsAString& aNodeValue, ErrorResult& aError)
{
  SetValue(aNodeValue, nullptr, aError);
}

nsresult
Attr::Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult,
            bool aPreallocateChildren) const
{
  nsAutoString value;
  const_cast<Attr*>(this)->GetValue(value);

  RefPtr<mozilla::dom::NodeInfo> ni = aNodeInfo;
  *aResult = new Attr(nullptr, ni.forget(), value);
  if (!*aResult) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(*aResult);

  return NS_OK;
}

already_AddRefed<nsIURI>
Attr::GetBaseURI(bool aTryUseXHRDocBaseURI) const
{
  Element* parent = GetElement();

  return parent ? parent->GetBaseURI(aTryUseXHRDocBaseURI) : nullptr;
}

void
Attr::GetTextContentInternal(nsAString& aTextContent,
                             OOMReporter& aError)
{
  GetValue(aTextContent);
}

void
Attr::SetTextContentInternal(const nsAString& aTextContent,
                             nsIPrincipal* aSubjectPrincipal,
                             ErrorResult& aError)
{
  SetNodeValueInternal(aTextContent, aError);
}

bool
Attr::IsNodeOfType(uint32_t aFlags) const
{
    return !(aFlags & ~eATTRIBUTE);
}

uint32_t
Attr::GetChildCount() const
{
  return 0;
}

nsIContent *
Attr::GetChildAt_Deprecated(uint32_t aIndex) const
{
  return nullptr;
}

int32_t
Attr::ComputeIndexOf(const nsINode* aPossibleChild) const
{
  return -1;
}

nsresult
Attr::InsertChildAt(nsIContent* aKid, uint32_t aIndex,
                              bool aNotify)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

void
Attr::RemoveChildAt_Deprecated(uint32_t aIndex, bool aNotify)
{
}

void
Attr::RemoveChildNode(nsIContent* aKid, bool aNotify)
{
}

nsresult
Attr::GetEventTargetParent(EventChainPreVisitor& aVisitor)
{
  aVisitor.mCanHandle = true;
  return NS_OK;
}

void
Attr::Initialize()
{
  sInitialized = true;
}

void
Attr::Shutdown()
{
  sInitialized = false;
}

JSObject*
Attr::WrapNode(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return AttrBinding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla
