/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Class that represents a prefix/namespace/localName triple; a single
 * nodeinfo is shared by all elements in a document that have that
 * prefix, namespace, and localName.
 */

#include "mozilla/dom/NodeInfo.h"
#include "mozilla/dom/NodeInfoInlines.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/Likely.h"

#include "nsNodeInfoManager.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsAtom.h"
#include "nsDOMString.h"
#include "nsCRT.h"
#include "nsINode.h"
#include "nsContentUtils.h"
#include "nsReadableUtils.h"
#include "mozilla/Sprintf.h"
#include "mozilla/dom/Document.h"
#include "nsGkAtoms.h"
#include "nsCCUncollectableMarker.h"
#include "nsNameSpaceManager.h"

using namespace mozilla;
using mozilla::dom::NodeInfo;

NodeInfo::~NodeInfo() {
  mOwnerManager->RemoveNodeInfo(this);

  // We can't use NS_IF_RELEASE because mName is const.
  if (mInner.mName) {
    mInner.mName->Release();
  }
  NS_IF_RELEASE(mInner.mPrefix);
  NS_IF_RELEASE(mInner.mExtraName);
}

NodeInfo::NodeInfo(nsAtom* aName, nsAtom* aPrefix, int32_t aNamespaceID,
                   uint16_t aNodeType, nsAtom* aExtraName,
                   nsNodeInfoManager* aOwnerManager)
    : mDocument(aOwnerManager->GetDocument()),
      mInner(aName, aPrefix, aNamespaceID, aNodeType, aExtraName),
      mOwnerManager(aOwnerManager) {
  CheckValidNodeInfo(aNodeType, aName, aNamespaceID, aExtraName);

  NS_IF_ADDREF(mInner.mName);
  NS_IF_ADDREF(mInner.mPrefix);
  NS_IF_ADDREF(mInner.mExtraName);

  // Now compute our cached members.

  // Qualified name.  If we have no prefix, use ToString on
  // mInner.mName so that we get to share its buffer.
  if (aPrefix) {
    mQualifiedName = nsDependentAtomString(mInner.mPrefix) + u":"_ns +
                     nsDependentAtomString(mInner.mName);
  } else {
    mInner.mName->ToString(mQualifiedName);
  }

  MOZ_ASSERT_IF(aNodeType != nsINode::ELEMENT_NODE &&
                    aNodeType != nsINode::ATTRIBUTE_NODE &&
                    aNodeType != UINT16_MAX,
                aNamespaceID == kNameSpaceID_None && !aPrefix);

  switch (aNodeType) {
    case nsINode::ELEMENT_NODE:
    case nsINode::ATTRIBUTE_NODE:
      // Correct the case for HTML
      if (aNodeType == nsINode::ELEMENT_NODE &&
          aNamespaceID == kNameSpaceID_XHTML && GetDocument() &&
          GetDocument()->IsHTMLDocument()) {
        nsContentUtils::ASCIIToUpper(mQualifiedName, mNodeName);
      } else {
        mNodeName = mQualifiedName;
      }
      mInner.mName->ToString(mLocalName);
      break;
    case nsINode::TEXT_NODE:
    case nsINode::CDATA_SECTION_NODE:
    case nsINode::COMMENT_NODE:
    case nsINode::DOCUMENT_NODE:
    case nsINode::DOCUMENT_FRAGMENT_NODE:
      mInner.mName->ToString(mNodeName);
      SetDOMStringToNull(mLocalName);
      break;
    case nsINode::PROCESSING_INSTRUCTION_NODE:
    case nsINode::DOCUMENT_TYPE_NODE:
      mInner.mExtraName->ToString(mNodeName);
      SetDOMStringToNull(mLocalName);
      break;
    default:
      MOZ_ASSERT(aNodeType == UINT16_MAX, "Unknown node type");
  }
}

// nsISupports

NS_IMPL_CYCLE_COLLECTION_CLASS(NodeInfo)

NS_IMPL_CYCLE_COLLECTION_UNLINK_0(NodeInfo)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INTERNAL(NodeInfo)
  if (MOZ_UNLIKELY(cb.WantDebugInfo())) {
    char name[72];
    uint32_t nsid = tmp->NamespaceID();
    nsAtomCString localName(tmp->NameAtom());
    const char* nsuri = nsNameSpaceManager::GetNameSpaceDisplayName(nsid);
    SprintfLiteral(name, "NodeInfo %s %s", nsuri, localName.get());

    cb.DescribeRefCountedNode(tmp->mRefCnt.get(), name);
  } else {
    NS_IMPL_CYCLE_COLLECTION_DESCRIBE(NodeInfo, tmp->mRefCnt.get())
  }

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mOwnerManager)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_BEGIN(NodeInfo)
  return nsCCUncollectableMarker::sGeneration && tmp->CanSkip();
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_BEGIN(NodeInfo)
  return nsCCUncollectableMarker::sGeneration && tmp->CanSkip();
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_BEGIN(NodeInfo)
  return nsCCUncollectableMarker::sGeneration && tmp->CanSkip();
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_END

void NodeInfo::GetName(nsAString& aName) const {
  mInner.mName->ToString(aName);
}

void NodeInfo::GetPrefix(nsAString& aPrefix) const {
  if (mInner.mPrefix) {
    mInner.mPrefix->ToString(aPrefix);
  } else {
    SetDOMStringToNull(aPrefix);
  }
}

void NodeInfo::GetNamespaceURI(nsAString& aNameSpaceURI) const {
  if (mInner.mNamespaceID > 0) {
    nsresult rv = nsNameSpaceManager::GetInstance()->GetNameSpaceURI(
        mInner.mNamespaceID, aNameSpaceURI);
    // How can we possibly end up with a bogus namespace ID here?
    if (NS_FAILED(rv)) {
      MOZ_CRASH();
    }
  } else {
    SetDOMStringToNull(aNameSpaceURI);
  }
}

bool NodeInfo::NamespaceEquals(const nsAString& aNamespaceURI) const {
  int32_t nsid = nsNameSpaceManager::GetInstance()->GetNameSpaceID(
      aNamespaceURI, nsContentUtils::IsChromeDoc(mOwnerManager->GetDocument()));

  return mozilla::dom::NodeInfo::NamespaceEquals(nsid);
}

void NodeInfo::DeleteCycleCollectable() {
  RefPtr<nsNodeInfoManager> kungFuDeathGrip = mOwnerManager;
  mozilla::Unused
      << kungFuDeathGrip;  // Just keeping value alive for longer than this
  delete this;
}

bool NodeInfo::CanSkip() {
  return mDocument && nsCCUncollectableMarker::InGeneration(
                          mDocument->GetMarkedCCGeneration());
}
