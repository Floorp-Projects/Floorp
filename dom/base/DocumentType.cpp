/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Implementation of DOM Core's DocumentType node.
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

already_AddRefed<mozilla::dom::DocumentType>
NS_NewDOMDocumentType(nsNodeInfoManager* aNodeInfoManager,
                      nsAtom *aName,
                      const nsAString& aPublicId,
                      const nsAString& aSystemId,
                      const nsAString& aInternalSubset)
{
  MOZ_ASSERT(aName, "Must have a name");

  already_AddRefed<mozilla::dom::NodeInfo> ni =
    aNodeInfoManager->GetNodeInfo(nsGkAtoms::documentTypeNodeName, nullptr,
                                  kNameSpaceID_None,
                                  nsINode::DOCUMENT_TYPE_NODE, aName);

  RefPtr<mozilla::dom::DocumentType> docType =
    new mozilla::dom::DocumentType(ni, aPublicId, aSystemId, aInternalSubset);
  return docType.forget();
}

namespace mozilla {
namespace dom {

JSObject*
DocumentType::WrapNode(JSContext *cx, JS::Handle<JSObject*> aGivenProto)
{
  return DocumentType_Binding::Wrap(cx, this, aGivenProto);
}

DocumentType::DocumentType(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo,
                           const nsAString& aPublicId,
                           const nsAString& aSystemId,
                           const nsAString& aInternalSubset) :
  CharacterData(aNodeInfo),
  mPublicId(aPublicId),
  mSystemId(aSystemId),
  mInternalSubset(aInternalSubset)
{
  MOZ_ASSERT(mNodeInfo->NodeType() == DOCUMENT_TYPE_NODE,
             "Bad NodeType in aNodeInfo");
  MOZ_ASSERT(!IsCharacterData());
}

DocumentType::~DocumentType() = default;

bool
DocumentType::IsNodeOfType(uint32_t aFlags) const
{
  return false;
}

const nsTextFragment*
DocumentType::GetText()
{
  return nullptr;
}

void
DocumentType::GetName(nsAString& aName) const
{
  aName = NodeName();
}

void
DocumentType::GetPublicId(nsAString& aPublicId) const
{
  aPublicId = mPublicId;
}

void
DocumentType::GetSystemId(nsAString& aSystemId) const
{
  aSystemId = mSystemId;
}

void
DocumentType::GetInternalSubset(nsAString& aInternalSubset) const
{
  aInternalSubset = mInternalSubset;
}

already_AddRefed<CharacterData>
DocumentType::CloneDataNode(mozilla::dom::NodeInfo *aNodeInfo, bool aCloneText) const
{
  already_AddRefed<mozilla::dom::NodeInfo> ni = RefPtr<mozilla::dom::NodeInfo>(aNodeInfo).forget();
  return do_AddRef(new DocumentType(ni, mPublicId, mSystemId, mInternalSubset));
}

} // namespace dom
} // namespace mozilla

