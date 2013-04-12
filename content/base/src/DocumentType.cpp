/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Implementation of DOM Core's nsIDOMDocumentType node.
 */

#include "mozilla/dom/DocumentType.h"
#include "nsGkAtoms.h"
#include "nsCOMPtr.h"
#include "nsDOMString.h"
#include "nsNodeInfoManager.h"
#include "nsIXPConnect.h"
#include "xpcpublic.h"
#include "nsWrapperCacheInlines.h"
#include "mozilla/dom/DocumentTypeBinding.h"

nsresult
NS_NewDOMDocumentType(nsIDOMDocumentType** aDocType,
                      nsNodeInfoManager *aNodeInfoManager,
                      nsIAtom *aName,
                      const nsAString& aPublicId,
                      const nsAString& aSystemId,
                      const nsAString& aInternalSubset)
{
  NS_ENSURE_ARG_POINTER(aDocType);
  mozilla::ErrorResult rv;
  *aDocType = NS_NewDOMDocumentType(aNodeInfoManager, aName, aPublicId,
                                    aSystemId, aInternalSubset, rv).get();
  return rv.ErrorCode();
}

already_AddRefed<mozilla::dom::DocumentType>
NS_NewDOMDocumentType(nsNodeInfoManager* aNodeInfoManager,
                      nsIAtom *aName,
                      const nsAString& aPublicId,
                      const nsAString& aSystemId,
                      const nsAString& aInternalSubset,
                      mozilla::ErrorResult& rv)
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

  nsRefPtr<mozilla::dom::DocumentType> docType =
    new mozilla::dom::DocumentType(ni.forget(), aPublicId, aSystemId, aInternalSubset);
  return docType.forget();
}

namespace mozilla {
namespace dom {

JSObject*
DocumentType::WrapNode(JSContext *cx, JSObject *scope)
{
  return DocumentTypeBinding::Wrap(cx, scope, this);
}

DocumentType::DocumentType(already_AddRefed<nsINodeInfo> aNodeInfo,
                           const nsAString& aPublicId,
                           const nsAString& aSystemId,
                           const nsAString& aInternalSubset) :
  DocumentTypeForward(aNodeInfo),
  mPublicId(aPublicId),
  mSystemId(aSystemId),
  mInternalSubset(aInternalSubset)
{
  SetIsDOMBinding();
  NS_ABORT_IF_FALSE(mNodeInfo->NodeType() == nsIDOMNode::DOCUMENT_TYPE_NODE,
                    "Bad NodeType in aNodeInfo");
}

DocumentType::~DocumentType()
{
}

NS_IMPL_ISUPPORTS_INHERITED2(DocumentType, nsGenericDOMDataNode, nsIDOMNode,
                             nsIDOMDocumentType)

bool
DocumentType::IsNodeOfType(uint32_t aFlags) const
{
  // Don't claim to be eDATA_NODE since we're just inheriting
  // nsGenericDOMDataNode for convinience. Doctypes aren't really
  // data nodes (they have a null .nodeValue and don't implement
  // nsIDOMCharacterData)
  return !(aFlags & ~eCONTENT);
}

const nsTextFragment*
DocumentType::GetText()
{
  return nullptr;
}

NS_IMETHODIMP    
DocumentType::GetName(nsAString& aName)
{
  aName = NodeName();
  return NS_OK;
}

NS_IMETHODIMP
DocumentType::GetPublicId(nsAString& aPublicId)
{
  aPublicId = mPublicId;

  return NS_OK;
}

NS_IMETHODIMP
DocumentType::GetSystemId(nsAString& aSystemId)
{
  aSystemId = mSystemId;

  return NS_OK;
}

NS_IMETHODIMP
DocumentType::GetInternalSubset(nsAString& aInternalSubset)
{
  aInternalSubset = mInternalSubset;
  return NS_OK;
}

nsGenericDOMDataNode*
DocumentType::CloneDataNode(nsINodeInfo *aNodeInfo, bool aCloneText) const
{
  nsCOMPtr<nsINodeInfo> ni = aNodeInfo;
  return new DocumentType(ni.forget(), mPublicId, mSystemId,
                          mInternalSubset);
}

} // namespace dom
} // namespace mozilla

