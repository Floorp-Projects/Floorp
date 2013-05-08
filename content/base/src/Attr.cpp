/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Implementation of DOM Core's nsIDOMAttr node.
 */

#include "mozilla/dom/Attr.h"
#include "mozilla/dom/AttrBinding.h"
#include "mozilla/dom/Element.h"
#include "nsContentCreatorFunctions.h"
#include "nsINameSpaceManager.h"
#include "nsError.h"
#include "nsUnicharUtils.h"
#include "nsDOMString.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMUserDataHandler.h"
#include "nsEventDispatcher.h"
#include "nsGkAtoms.h"
#include "nsCOMArray.h"
#include "nsNodeUtils.h"
#include "nsEventListenerManager.h"
#include "nsTextNode.h"
#include "mozAutoDocUpdate.h"
#include "nsMutationEvent.h"
#include "nsAsyncDOMEvent.h"
#include "nsWrapperCacheInlines.h"

nsIAttribute::nsIAttribute(nsDOMAttributeMap* aAttrMap,
                           already_AddRefed<nsINodeInfo> aNodeInfo,
                           bool aNsAware)
: nsINode(aNodeInfo), mAttrMap(aAttrMap), mNsAware(aNsAware)
{
}

namespace mozilla {
namespace dom {

//----------------------------------------------------------------------
bool Attr::sInitialized;

Attr::Attr(nsDOMAttributeMap *aAttrMap,
           already_AddRefed<nsINodeInfo> aNodeInfo,
           const nsAString  &aValue, bool aNsAware)
  : nsIAttribute(aAttrMap, aNodeInfo, aNsAware), mValue(aValue)
{
  NS_ABORT_IF_FALSE(mNodeInfo, "We must get a nodeinfo here!");
  NS_ABORT_IF_FALSE(mNodeInfo->NodeType() == nsIDOMNode::ATTRIBUTE_NODE,
                    "Wrong nodeType");

  // We don't add a reference to our content. It will tell us
  // to drop our reference when it goes away.

  SetIsDOMBinding();
}

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(Attr)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS

  if (!nsINode::Traverse(tmp, cb)) {
    return NS_SUCCESS_INTERRUPTED_TRAVERSE;
  }
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mAttrMap)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(Attr)
  nsINode::Trace(tmp, aCallback, aClosure);
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(Attr)
  nsINode::Unlink(tmp);
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mAttrMap)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_BEGIN(Attr)
  Element* ownerElement = tmp->GetContentInternal();
  if (tmp->IsBlack()) {
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
  return tmp->IsBlackAndDoesNotNeedTracing(static_cast<nsIAttribute*>(tmp));
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_BEGIN(Attr)
  return tmp->IsBlack();
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_END

// QueryInterface implementation for Attr
NS_INTERFACE_TABLE_HEAD(Attr)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_NODE_INTERFACE_TABLE5(Attr, nsIDOMAttr, nsIAttribute, nsIDOMNode,
                           nsIDOMEventTarget, EventTarget)
  NS_INTERFACE_MAP_ENTRIES_CYCLE_COLLECTION(Attr)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIAttribute)
  NS_INTERFACE_MAP_ENTRY_TEAROFF(nsISupportsWeakReference,
                                 new nsNodeSupportsWeakRefTearoff(this))
  NS_INTERFACE_MAP_ENTRY_TEAROFF(nsIDOMXPathNSResolver,
                                 new nsNode3Tearoff(this))
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(Attr)
NS_IMPL_CYCLE_COLLECTING_RELEASE_WITH_DESTROY(Attr,
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

nsIContent*
Attr::GetContent() const
{
  return GetContentInternal();
}

nsresult
Attr::SetOwnerDocument(nsIDocument* aDocument)
{
  NS_ASSERTION(aDocument, "Missing document");

  nsIDocument *doc = OwnerDoc();
  NS_ASSERTION(doc != aDocument, "bad call to Attr::SetOwnerDocument");
  doc->DeleteAllPropertiesFor(this);

  nsCOMPtr<nsINodeInfo> newNodeInfo;
  newNodeInfo = aDocument->NodeInfoManager()->
    GetNodeInfo(mNodeInfo->NameAtom(), mNodeInfo->GetPrefixAtom(),
                mNodeInfo->NamespaceID(),
                nsIDOMNode::ATTRIBUTE_NODE);
  NS_ASSERTION(newNodeInfo, "GetNodeInfo lies");
  mNodeInfo.swap(newNodeInfo);

  return NS_OK;
}

NS_IMETHODIMP
Attr::GetName(nsAString& aName)
{
  aName = NodeName();
  return NS_OK;
}

already_AddRefed<nsIAtom>
Attr::GetNameAtom(nsIContent* aContent)
{
  if (!mNsAware &&
      mNodeInfo->NamespaceID() == kNameSpaceID_None &&
      aContent->IsInHTMLDocument() &&
      aContent->IsHTML()) {
    nsString name;
    mNodeInfo->GetName(name);
    nsAutoString lowercaseName;
    nsContentUtils::ASCIIToLower(name, lowercaseName);
    return do_GetAtom(lowercaseName);
  }
  nsCOMPtr<nsIAtom> nameAtom = mNodeInfo->NameAtom();
  return nameAtom.forget();
}

NS_IMETHODIMP
Attr::GetValue(nsAString& aValue)
{
  nsIContent* content = GetContentInternal();
  if (content) {
    nsCOMPtr<nsIAtom> nameAtom = GetNameAtom(content);
    content->GetAttr(mNodeInfo->NamespaceID(), nameAtom, aValue);
  }
  else {
    aValue = mValue;
  }

  return NS_OK;
}

void
Attr::SetValue(const nsAString& aValue, ErrorResult& aRv)
{
  nsIContent* content = GetContentInternal();
  if (!content) {
    mValue = aValue;
    return;
  }

  nsCOMPtr<nsIAtom> nameAtom = GetNameAtom(content);
  aRv = content->SetAttr(mNodeInfo->NamespaceID(),
                         nameAtom,
                         mNodeInfo->GetPrefixAtom(),
                         aValue,
                         true);
}

NS_IMETHODIMP
Attr::SetValue(const nsAString& aValue)
{
  ErrorResult rv;
  SetValue(aValue, rv);
  return rv.ErrorCode();
}

bool
Attr::Specified() const
{
  OwnerDoc()->WarnOnceAbout(nsIDocument::eSpecified);
  return true;
}

NS_IMETHODIMP
Attr::GetSpecified(bool* aSpecified)
{
  NS_ENSURE_ARG_POINTER(aSpecified);
  *aSpecified = Specified();
  return NS_OK;
}

Element*
Attr::GetOwnerElement(ErrorResult& aRv)
{
  OwnerDoc()->WarnOnceAbout(nsIDocument::eOwnerElement);
  return GetContentInternal();
}

NS_IMETHODIMP
Attr::GetOwnerElement(nsIDOMElement** aOwnerElement)
{
  NS_ENSURE_ARG_POINTER(aOwnerElement);
  OwnerDoc()->WarnOnceAbout(nsIDocument::eOwnerElement);

  nsIContent* content = GetContentInternal();
  if (content) {
    return CallQueryInterface(content, aOwnerElement);
  }

  *aOwnerElement = nullptr;

  return NS_OK;
}

void
Attr::GetNodeValueInternal(nsAString& aNodeValue)
{
  OwnerDoc()->WarnOnceAbout(nsIDocument::eNodeValue);

  GetValue(aNodeValue);
}

void
Attr::SetNodeValueInternal(const nsAString& aNodeValue, ErrorResult& aError)
{
  OwnerDoc()->WarnOnceAbout(nsIDocument::eNodeValue);

  aError = SetValue(aNodeValue);
}

nsresult
Attr::Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const
{
  nsAutoString value;
  const_cast<Attr*>(this)->GetValue(value);

  nsCOMPtr<nsINodeInfo> ni = aNodeInfo;
  *aResult = new Attr(nullptr, ni.forget(), value, mNsAware);
  if (!*aResult) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(*aResult);

  return NS_OK;
}

already_AddRefed<nsIURI>
Attr::GetBaseURI() const
{
  nsINode *parent = GetContentInternal();

  return parent ? parent->GetBaseURI() : nullptr;
}

void
Attr::GetTextContentInternal(nsAString& aTextContent)
{
  OwnerDoc()->WarnOnceAbout(nsIDocument::eTextContent);

  GetValue(aTextContent);
}

void
Attr::SetTextContentInternal(const nsAString& aTextContent,
                             ErrorResult& aError)
{
  OwnerDoc()->WarnOnceAbout(nsIDocument::eTextContent);

  SetNodeValueInternal(aTextContent, aError);
}

NS_IMETHODIMP
Attr::GetIsId(bool* aReturn)
{
  nsIContent* content = GetContentInternal();
  if (!content)
  {
    *aReturn = false;
    return NS_OK;
  }

  nsIAtom* idAtom = content->GetIDAttributeName();
  if (!idAtom)
  {
    *aReturn = false;
    return NS_OK;
  }

  *aReturn = mNodeInfo->Equals(idAtom, kNameSpaceID_None);
  return NS_OK;
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
Attr::GetChildAt(uint32_t aIndex) const
{
  return nullptr;
}

nsIContent * const *
Attr::GetChildArray(uint32_t* aChildCount) const
{
  *aChildCount = 0;
  return nullptr;
}

int32_t
Attr::IndexOf(const nsINode* aPossibleChild) const
{
  return -1;
}

nsresult
Attr::InsertChildAt(nsIContent* aKid, uint32_t aIndex,
                              bool aNotify)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
Attr::AppendChildTo(nsIContent* aKid, bool aNotify)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

void
Attr::RemoveChildAt(uint32_t aIndex, bool aNotify)
{
}

nsresult
Attr::PreHandleEvent(nsEventChainPreVisitor& aVisitor)
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
Attr::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return AttrBinding::Wrap(aCx, aScope, this);
}

Element*
Attr::GetContentInternal() const
{
  return mAttrMap ? mAttrMap->GetContent() : nullptr;
}

} // namespace dom
} // namespace mozilla
