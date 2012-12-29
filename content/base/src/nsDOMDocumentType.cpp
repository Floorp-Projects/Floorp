/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Implementation of DOM Core's nsIDOMDocumentType node.
 */

#include "nsDOMDocumentType.h"
#include "nsDOMAttributeMap.h"
#include "nsIDOMNamedNodeMap.h"
#include "nsGkAtoms.h"
#include "nsCOMPtr.h"
#include "nsDOMString.h"
#include "nsNodeInfoManager.h"
#include "nsIXPConnect.h"
#include "xpcpublic.h"
#include "nsWrapperCacheInlines.h"
#include "mozilla/dom/DocumentTypeBinding.h"

using namespace mozilla;

JSObject*
nsDOMDocumentType::WrapNode(JSContext *cx, JSObject *scope, bool *triedToWrap)
{
  return dom::DocumentTypeBinding::Wrap(cx, scope, this, triedToWrap);
}

nsresult
NS_NewDOMDocumentType(nsIDOMDocumentType** aDocType,
                      nsNodeInfoManager *aNodeInfoManager,
                      nsIAtom *aName,
                      const nsAString& aPublicId,
                      const nsAString& aSystemId,
                      const nsAString& aInternalSubset)
{
  NS_ENSURE_ARG_POINTER(aDocType);
  ErrorResult rv;
  *aDocType = NS_NewDOMDocumentType(aNodeInfoManager, aName, aPublicId,
                                    aSystemId, aInternalSubset, rv).get();
  return rv.ErrorCode();
}

already_AddRefed<nsDOMDocumentType>
NS_NewDOMDocumentType(nsNodeInfoManager* aNodeInfoManager,
                      nsIAtom *aName,
                      const nsAString& aPublicId,
                      const nsAString& aSystemId,
                      const nsAString& aInternalSubset,
                      ErrorResult& rv)
{
  if (!aName) {
    rv.Throw(NS_ERROR_INVALID_POINTER);
    return nullptr;
  }

  nsCOMPtr<nsINodeInfo> ni =
    aNodeInfoManager->GetNodeInfo(nsGkAtoms::documentTypeNodeName, nullptr,
                                  kNameSpaceID_None,
                                  nsIDOMNode::DOCUMENT_TYPE_NODE,
                                  aName);
  if (!ni) {
    rv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return nullptr;
  }

  nsRefPtr<nsDOMDocumentType> docType =
    new nsDOMDocumentType(ni.forget(), aPublicId, aSystemId, aInternalSubset);
  return docType.forget();
}

nsDOMDocumentType::nsDOMDocumentType(already_AddRefed<nsINodeInfo> aNodeInfo,
                                     const nsAString& aPublicId,
                                     const nsAString& aSystemId,
                                     const nsAString& aInternalSubset) :
  nsDOMDocumentTypeForward(aNodeInfo),
  mPublicId(aPublicId),
  mSystemId(aSystemId),
  mInternalSubset(aInternalSubset)
{
  SetIsDOMBinding();
  NS_ABORT_IF_FALSE(mNodeInfo->NodeType() == nsIDOMNode::DOCUMENT_TYPE_NODE,
                    "Bad NodeType in aNodeInfo");
}

nsDOMDocumentType::~nsDOMDocumentType()
{
}

DOMCI_NODE_DATA(DocumentType, nsDOMDocumentType)

// QueryInterface implementation for nsDOMDocumentType
NS_INTERFACE_TABLE_HEAD(nsDOMDocumentType)
  NS_NODE_INTERFACE_TABLE2(nsDOMDocumentType, nsIDOMNode, nsIDOMDocumentType)
  NS_INTERFACE_MAP_ENTRIES_CYCLE_COLLECTION(nsDOMDocumentType)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(DocumentType)
NS_INTERFACE_MAP_END_INHERITING(nsGenericDOMDataNode)


NS_IMPL_ADDREF_INHERITED(nsDOMDocumentType, nsGenericDOMDataNode)
NS_IMPL_RELEASE_INHERITED(nsDOMDocumentType, nsGenericDOMDataNode)

bool
nsDOMDocumentType::IsNodeOfType(uint32_t aFlags) const
{
  // Don't claim to be eDATA_NODE since we're just inheriting
  // nsGenericDOMDataNode for convinience. Doctypes aren't really
  // data nodes (they have a null .nodeValue and don't implement
  // nsIDOMCharacterData)
  return !(aFlags & ~eCONTENT);
}

const nsTextFragment*
nsDOMDocumentType::GetText()
{
  return nullptr;
}

NS_IMETHODIMP    
nsDOMDocumentType::GetName(nsAString& aName)
{
  aName = NodeName();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMDocumentType::GetPublicId(nsAString& aPublicId)
{
  aPublicId = mPublicId;

  return NS_OK;
}

NS_IMETHODIMP
nsDOMDocumentType::GetSystemId(nsAString& aSystemId)
{
  aSystemId = mSystemId;

  return NS_OK;
}

NS_IMETHODIMP
nsDOMDocumentType::GetInternalSubset(nsAString& aInternalSubset)
{
  aInternalSubset = mInternalSubset;
  return NS_OK;
}

nsGenericDOMDataNode*
nsDOMDocumentType::CloneDataNode(nsINodeInfo *aNodeInfo, bool aCloneText) const
{
  nsCOMPtr<nsINodeInfo> ni = aNodeInfo;
  return new nsDOMDocumentType(ni.forget(), mPublicId, mSystemId,
                               mInternalSubset);
}

