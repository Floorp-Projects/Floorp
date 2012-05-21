/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Implementation of DOM Core's nsIDOMAttr node.
 */

#include "nsDOMAttribute.h"
#include "nsGenericElement.h"
#include "mozilla/dom/Element.h"
#include "nsContentCreatorFunctions.h"
#include "nsINameSpaceManager.h"
#include "nsDOMError.h"
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

NS_IMPL_CYCLE_COLLECTION_CLASS(nsDOMAttribute)

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
  nsIAtom* result = nsnull;
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

  *aOwnerElement = nsnull;

  return NS_OK;
}

NS_IMETHODIMP
nsDOMAttribute::GetNodeName(nsAString& aNodeName)
{
  OwnerDoc()->WarnOnceAbout(nsIDocument::eNodeName);

  return GetName(aNodeName);
}

NS_IMETHODIMP
nsDOMAttribute::GetNodeValue(nsAString& aNodeValue)
{
  OwnerDoc()->WarnOnceAbout(nsIDocument::eNodeValue);

  return GetValue(aNodeValue);
}

NS_IMETHODIMP
nsDOMAttribute::SetNodeValue(const nsAString& aNodeValue)
{
  OwnerDoc()->WarnOnceAbout(nsIDocument::eNodeValue);

  return SetValue(aNodeValue);
}

NS_IMETHODIMP
nsDOMAttribute::GetNodeType(PRUint16* aNodeType)
{
  NS_ENSURE_ARG_POINTER(aNodeType);
  OwnerDoc()->WarnOnceAbout(nsIDocument::eNodeType);

  *aNodeType = (PRUint16)nsIDOMNode::ATTRIBUTE_NODE;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMAttribute::GetParentNode(nsIDOMNode** aParentNode)
{
  NS_ENSURE_ARG_POINTER(aParentNode);
  OwnerDoc()->WarnOnceAbout(nsIDocument::eParentNode);

  *aParentNode = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMAttribute::GetParentElement(nsIDOMElement** aParentElement)
{
  *aParentElement = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMAttribute::GetChildNodes(nsIDOMNodeList** aChildNodes)
{
  OwnerDoc()->WarnOnceAbout(nsIDocument::eChildNodes);

  return nsINode::GetChildNodes(aChildNodes);
}

NS_IMETHODIMP
nsDOMAttribute::HasChildNodes(bool* aHasChildNodes)
{
  OwnerDoc()->WarnOnceAbout(nsIDocument::eHasChildNodes);

  *aHasChildNodes = false;

  return NS_OK;
}

NS_IMETHODIMP
nsDOMAttribute::HasAttributes(bool* aHasAttributes)
{
  NS_ENSURE_ARG_POINTER(aHasAttributes);
  OwnerDoc()->WarnOnceAbout(nsIDocument::eHasAttributes);

  *aHasAttributes = false;

  return NS_OK;
}

NS_IMETHODIMP
nsDOMAttribute::GetFirstChild(nsIDOMNode** aFirstChild)
{
  *aFirstChild = nsnull;

  OwnerDoc()->WarnOnceAbout(nsIDocument::eFirstChild);

  return NS_OK;
}

NS_IMETHODIMP
nsDOMAttribute::GetLastChild(nsIDOMNode** aLastChild)
{
  OwnerDoc()->WarnOnceAbout(nsIDocument::eLastChild);

  return GetFirstChild(aLastChild);
}

NS_IMETHODIMP
nsDOMAttribute::GetPreviousSibling(nsIDOMNode** aPreviousSibling)
{
  NS_ENSURE_ARG_POINTER(aPreviousSibling);

  OwnerDoc()->WarnOnceAbout(nsIDocument::ePreviousSibling);

  *aPreviousSibling = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMAttribute::GetNextSibling(nsIDOMNode** aNextSibling)
{
  NS_ENSURE_ARG_POINTER(aNextSibling);

  OwnerDoc()->WarnOnceAbout(nsIDocument::eNextSibling);

  *aNextSibling = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMAttribute::GetAttributes(nsIDOMNamedNodeMap** aAttributes)
{
  NS_ENSURE_ARG_POINTER(aAttributes);

  OwnerDoc()->WarnOnceAbout(nsIDocument::eAttributes);

  *aAttributes = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMAttribute::InsertBefore(nsIDOMNode* aNewChild, nsIDOMNode* aRefChild, nsIDOMNode** aReturn)
{
  OwnerDoc()->WarnOnceAbout(nsIDocument::eInsertBefore);

  return ReplaceOrInsertBefore(false, aNewChild, aRefChild, aReturn);
}

NS_IMETHODIMP
nsDOMAttribute::ReplaceChild(nsIDOMNode* aNewChild, nsIDOMNode* aOldChild, nsIDOMNode** aReturn)
{
  OwnerDoc()->WarnOnceAbout(nsIDocument::eReplaceChild);

  return ReplaceOrInsertBefore(true, aNewChild, aOldChild, aReturn);
}

NS_IMETHODIMP
nsDOMAttribute::RemoveChild(nsIDOMNode* aOldChild, nsIDOMNode** aReturn)
{
  OwnerDoc()->WarnOnceAbout(nsIDocument::eRemoveChild);

  return nsINode::RemoveChild(aOldChild, aReturn);
}

NS_IMETHODIMP
nsDOMAttribute::AppendChild(nsIDOMNode* aNewChild, nsIDOMNode** aReturn)
{
  OwnerDoc()->WarnOnceAbout(nsIDocument::eAppendChild);

  return InsertBefore(aNewChild, nsnull, aReturn);
}

nsresult
nsDOMAttribute::Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const
{
  nsAutoString value;
  const_cast<nsDOMAttribute*>(this)->GetValue(value);

  nsCOMPtr<nsINodeInfo> ni = aNodeInfo;
  *aResult = new nsDOMAttribute(nsnull, ni.forget(), value, mNsAware);
  if (!*aResult) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(*aResult);

  return NS_OK;
}

NS_IMETHODIMP
nsDOMAttribute::CloneNode(bool aDeep, PRUint8 aOptionalArgc, nsIDOMNode** aResult)
{
  OwnerDoc()->WarnOnceAbout(nsIDocument::eCloneNode);

  if (!aOptionalArgc) {
    aDeep = true;
  }

  return nsNodeUtils::CloneNodeImpl(this, aDeep, true, aResult);
}

NS_IMETHODIMP
nsDOMAttribute::GetOwnerDocument(nsIDOMDocument** aOwnerDocument)
{
  OwnerDoc()->WarnOnceAbout(nsIDocument::eOwnerDocument);

  return nsINode::GetOwnerDocument(aOwnerDocument);
}

NS_IMETHODIMP
nsDOMAttribute::GetNamespaceURI(nsAString& aNamespaceURI)
{
  return mNodeInfo->GetNamespaceURI(aNamespaceURI);
}

NS_IMETHODIMP
nsDOMAttribute::GetPrefix(nsAString& aPrefix)
{
  mNodeInfo->GetPrefix(aPrefix);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMAttribute::GetLocalName(nsAString& aLocalName)
{
  mNodeInfo->GetName(aLocalName);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMAttribute::Normalize()
{
  OwnerDoc()->WarnOnceAbout(nsIDocument::eNormalize);

  // Nothing to do here
  return NS_OK;
}

NS_IMETHODIMP
nsDOMAttribute::IsSupported(const nsAString& aFeature,
                            const nsAString& aVersion,
                            bool* aReturn)
{
  OwnerDoc()->WarnOnceAbout(nsIDocument::eIsSupported);

  return nsGenericElement::InternalIsSupported(static_cast<nsIDOMAttr*>(this), 
                                               aFeature, aVersion, aReturn);
}

already_AddRefed<nsIURI>
nsDOMAttribute::GetBaseURI() const
{
  nsINode *parent = GetContentInternal();

  return parent ? parent->GetBaseURI() : nsnull;
}

NS_IMETHODIMP
nsDOMAttribute::GetDOMBaseURI(nsAString &aURI)
{
  return nsINode::GetDOMBaseURI(aURI);
}

NS_IMETHODIMP
nsDOMAttribute::CompareDocumentPosition(nsIDOMNode *other,
                                        PRUint16 *aResult)
{
  return nsINode::CompareDocumentPosition(other, aResult);
}

NS_IMETHODIMP
nsDOMAttribute::IsEqualNode(nsIDOMNode* aOther, bool* aResult)
{
  OwnerDoc()->WarnOnceAbout(nsIDocument::eIsEqualNode);

  return nsINode::IsEqualNode(aOther, aResult);
}

NS_IMETHODIMP
nsDOMAttribute::GetTextContent(nsAString &aTextContent)
{
  OwnerDoc()->WarnOnceAbout(nsIDocument::eTextContent);

  return GetNodeValue(aTextContent);
}

NS_IMETHODIMP
nsDOMAttribute::SetTextContent(const nsAString& aTextContent)
{
  OwnerDoc()->WarnOnceAbout(nsIDocument::eTextContent);

  return SetNodeValue(aTextContent);
}

NS_IMETHODIMP
nsDOMAttribute::Contains(nsIDOMNode* aOther, bool* aReturn)
{
  return nsINode::Contains(aOther, aReturn);
}

NS_IMETHODIMP
nsDOMAttribute::LookupPrefix(const nsAString & namespaceURI,
                             nsAString & aResult)
{
  SetDOMStringToNull(aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMAttribute::IsDefaultNamespace(const nsAString & namespaceURI,
                                   bool *aResult)
{
  *aResult = namespaceURI.IsEmpty();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMAttribute::LookupNamespaceURI(const nsAString & prefix,
                              nsAString & aResult)
{
  SetDOMStringToNull(aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMAttribute::SetUserData(const nsAString & key,
                            nsIVariant *data, nsIDOMUserDataHandler *handler,
                            nsIVariant **aResult)
{
  return nsINode::SetUserData(key, data, handler, aResult);
}

NS_IMETHODIMP
nsDOMAttribute::GetUserData(const nsAString & key, nsIVariant **aResult)
{
  return nsINode::GetUserData(key, aResult);
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
nsDOMAttribute::IsNodeOfType(PRUint32 aFlags) const
{
    return !(aFlags & ~eATTRIBUTE);
}

PRUint32
nsDOMAttribute::GetChildCount() const
{
  return 0;
}

nsIContent *
nsDOMAttribute::GetChildAt(PRUint32 aIndex) const
{
  return nsnull;
}

nsIContent * const *
nsDOMAttribute::GetChildArray(PRUint32* aChildCount) const
{
  *aChildCount = 0;
  return NULL;
}

PRInt32
nsDOMAttribute::IndexOf(nsINode* aPossibleChild) const
{
  return -1;
}

nsresult
nsDOMAttribute::InsertChildAt(nsIContent* aKid, PRUint32 aIndex,
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
nsDOMAttribute::RemoveChildAt(PRUint32 aIndex, bool aNotify)
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
