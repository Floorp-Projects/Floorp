/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Implementation of DOM Core's nsIDOMAttr node.
 */

#include "nsDOMAttribute.h"
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

using namespace mozilla;
using namespace mozilla::dom;

//----------------------------------------------------------------------
bool nsDOMAttribute::sInitialized;

nsDOMAttribute::nsDOMAttribute(nsDOMAttributeMap *aAttrMap,
                               already_AddRefed<nsINodeInfo> aNodeInfo,
                               const nsAString   &aValue, bool aNsAware)
  : nsIAttribute(aAttrMap, aNodeInfo, aNsAware), mValue(aValue)
{
  NS_ABORT_IF_FALSE(mNodeInfo, "We must get a nodeinfo here!");
  NS_ABORT_IF_FALSE(mNodeInfo->NodeType() == nsIDOMNode::ATTRIBUTE_NODE,
                    "Wrong nodeType");

  // We don't add a reference to our content. It will tell us
  // to drop our reference when it goes away.
}

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsDOMAttribute)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS

  if (!nsINode::Traverse(tmp, cb)) {
    return NS_SUCCESS_INTERRUPTED_TRAVERSE;
  }
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(nsDOMAttribute)
  nsINode::Trace(tmp, aCallback, aClosure);
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsDOMAttribute)
  nsINode::Unlink(tmp);
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

DOMCI_NODE_DATA(Attr, nsDOMAttribute)

// QueryInterface implementation for nsDOMAttribute
NS_INTERFACE_TABLE_HEAD(nsDOMAttribute)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_NODE_INTERFACE_TABLE4(nsDOMAttribute, nsIDOMAttr, nsIAttribute, nsIDOMNode,
                           nsIDOMEventTarget)
  NS_INTERFACE_MAP_ENTRIES_CYCLE_COLLECTION(nsDOMAttribute)
  NS_INTERFACE_MAP_ENTRY_TEAROFF(nsISupportsWeakReference,
                                 new nsNodeSupportsWeakRefTearoff(this))
  NS_INTERFACE_MAP_ENTRY_TEAROFF(nsIDOMXPathNSResolver,
                                 new nsNode3Tearoff(this))
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(Attr)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsDOMAttribute)
NS_IMPL_CYCLE_COLLECTING_RELEASE_WITH_DESTROY(nsDOMAttribute,
                                              nsNodeUtils::LastRelease(this))

void
nsDOMAttribute::SetMap(nsDOMAttributeMap *aMap)
{
  if (mAttrMap && !aMap && sInitialized) {
    // We're breaking a relationship with content and not getting a new one,
    // need to locally cache value. GetValue() does that.
    GetValue(mValue);
  }

  mAttrMap = aMap;
}

nsIContent*
nsDOMAttribute::GetContent() const
{
  return GetContentInternal();
}

nsresult
nsDOMAttribute::SetOwnerDocument(nsIDocument* aDocument)
{
  NS_ASSERTION(aDocument, "Missing document");

  nsIDocument *doc = OwnerDoc();
  NS_ASSERTION(doc != aDocument, "bad call to nsDOMAttribute::SetOwnerDocument");
  doc->DeleteAllPropertiesFor(this);

  nsCOMPtr<nsINodeInfo> newNodeInfo;
  newNodeInfo = aDocument->NodeInfoManager()->
    GetNodeInfo(mNodeInfo->NameAtom(), mNodeInfo->GetPrefixAtom(),
                mNodeInfo->NamespaceID(),
                nsIDOMNode::ATTRIBUTE_NODE);
  NS_ENSURE_TRUE(newNodeInfo, NS_ERROR_OUT_OF_MEMORY);
  NS_ASSERTION(newNodeInfo, "GetNodeInfo lies");
  mNodeInfo.swap(newNodeInfo);

  return NS_OK;
}

NS_IMETHODIMP
nsDOMAttribute::GetName(nsAString& aName)
{
  aName = NodeName();
  return NS_OK;
}

already_AddRefed<nsIAtom>
nsDOMAttribute::GetNameAtom(nsIContent* aContent)
{
  nsIAtom* result = nullptr;
  if (!mNsAware &&
      mNodeInfo->NamespaceID() == kNameSpaceID_None &&
      aContent->IsInHTMLDocument() &&
      aContent->IsHTML()) {
    nsString name;
    mNodeInfo->GetName(name);
    nsAutoString lowercaseName;
    nsContentUtils::ASCIIToLower(name, lowercaseName);
    nsCOMPtr<nsIAtom> nameAtom = do_GetAtom(lowercaseName);
    nameAtom.swap(result);
  } else {
    nsCOMPtr<nsIAtom> nameAtom = mNodeInfo->NameAtom();
    nameAtom.swap(result);
  }
  return result;
}

NS_IMETHODIMP
nsDOMAttribute::GetValue(nsAString& aValue)
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

NS_IMETHODIMP
nsDOMAttribute::SetValue(const nsAString& aValue)
{
  nsIContent* content = GetContentInternal();
  if (!content) {
    mValue = aValue;
    return NS_OK;
  }

  nsCOMPtr<nsIAtom> nameAtom = GetNameAtom(content);
  return content->SetAttr(mNodeInfo->NamespaceID(),
                          nameAtom,
                          mNodeInfo->GetPrefixAtom(),
                          aValue,
                          true);
}


NS_IMETHODIMP
nsDOMAttribute::GetSpecified(bool* aSpecified)
{
  NS_ENSURE_ARG_POINTER(aSpecified);
  OwnerDoc()->WarnOnceAbout(nsIDocument::eSpecified);

  *aSpecified = true;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMAttribute::GetOwnerElement(nsIDOMElement** aOwnerElement)
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
nsDOMAttribute::GetNodeValueInternal(nsAString& aNodeValue)
{
  OwnerDoc()->WarnOnceAbout(nsIDocument::eNodeValue);

  GetValue(aNodeValue);
}

void
nsDOMAttribute::SetNodeValueInternal(const nsAString& aNodeValue, ErrorResult& aError)
{
  OwnerDoc()->WarnOnceAbout(nsIDocument::eNodeValue);

  aError = SetValue(aNodeValue);
}

nsresult
nsDOMAttribute::Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const
{
  nsAutoString value;
  const_cast<nsDOMAttribute*>(this)->GetValue(value);

  nsCOMPtr<nsINodeInfo> ni = aNodeInfo;
  *aResult = new nsDOMAttribute(nullptr, ni.forget(), value, mNsAware);
  if (!*aResult) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(*aResult);

  return NS_OK;
}

already_AddRefed<nsIURI>
nsDOMAttribute::GetBaseURI() const
{
  nsINode *parent = GetContentInternal();

  return parent ? parent->GetBaseURI() : nullptr;
}

void
nsDOMAttribute::GetTextContentInternal(nsAString& aTextContent)
{
  OwnerDoc()->WarnOnceAbout(nsIDocument::eTextContent);

  GetValue(aTextContent);
}

void
nsDOMAttribute::SetTextContentInternal(const nsAString& aTextContent,
                                       ErrorResult& aError)
{
  OwnerDoc()->WarnOnceAbout(nsIDocument::eTextContent);

  SetNodeValueInternal(aTextContent, aError);
}

NS_IMETHODIMP
nsDOMAttribute::GetIsId(bool* aReturn)
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
nsDOMAttribute::IsNodeOfType(uint32_t aFlags) const
{
    return !(aFlags & ~eATTRIBUTE);
}

uint32_t
nsDOMAttribute::GetChildCount() const
{
  return 0;
}

nsIContent *
nsDOMAttribute::GetChildAt(uint32_t aIndex) const
{
  return nullptr;
}

nsIContent * const *
nsDOMAttribute::GetChildArray(uint32_t* aChildCount) const
{
  *aChildCount = 0;
  return NULL;
}

int32_t
nsDOMAttribute::IndexOf(const nsINode* aPossibleChild) const
{
  return -1;
}

nsresult
nsDOMAttribute::InsertChildAt(nsIContent* aKid, uint32_t aIndex,
                              bool aNotify)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
nsDOMAttribute::AppendChildTo(nsIContent* aKid, bool aNotify)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

void
nsDOMAttribute::RemoveChildAt(uint32_t aIndex, bool aNotify)
{
}

nsresult
nsDOMAttribute::PreHandleEvent(nsEventChainPreVisitor& aVisitor)
{
  aVisitor.mCanHandle = true;
  return NS_OK;
}

void
nsDOMAttribute::Initialize()
{
  sInitialized = true;
}

void
nsDOMAttribute::Shutdown()
{
  sInitialized = false;
}
