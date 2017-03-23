/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * A class for handing out nodeinfos and ensuring sharing of them as needed.
 */

#include "nsNodeInfoManager.h"

#include "mozilla/DebugOnly.h"
#include "mozilla/Telemetry.h"
#include "mozilla/dom/NodeInfo.h"
#include "mozilla/dom/NodeInfoInlines.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsIAtom.h"
#include "nsIDocument.h"
#include "nsIPrincipal.h"
#include "nsIURI.h"
#include "nsContentUtils.h"
#include "nsReadableUtils.h"
#include "nsGkAtoms.h"
#include "nsComponentManagerUtils.h"
#include "nsLayoutStatics.h"
#include "nsBindingManager.h"
#include "nsHashKeys.h"
#include "nsCCUncollectableMarker.h"
#include "nsNameSpaceManager.h"
#include "nsDocument.h"
#include "NullPrincipal.h"

using namespace mozilla;
using mozilla::dom::NodeInfo;

#include "mozilla/Logging.h"

static LazyLogModule gNodeInfoManagerLeakPRLog("NodeInfoManagerLeak");

PLHashNumber
nsNodeInfoManager::GetNodeInfoInnerHashValue(const void *key)
{
  MOZ_ASSERT(key, "Null key passed to NodeInfo::GetHashValue!");

  auto *node = reinterpret_cast<const NodeInfo::NodeInfoInner*>(key);

  return node->mName ? node->mName->hash() : HashString(*(node->mNameString));
}


int
nsNodeInfoManager::NodeInfoInnerKeyCompare(const void *key1, const void *key2)
{
  MOZ_ASSERT(key1 && key2, "Null key passed to NodeInfoInnerKeyCompare!");

  auto *node1 = reinterpret_cast<const NodeInfo::NodeInfoInner*>(key1);
  auto *node2 = reinterpret_cast<const NodeInfo::NodeInfoInner*>(key2);

  if (node1->mPrefix != node2->mPrefix ||
      node1->mNamespaceID != node2->mNamespaceID ||
      node1->mNodeType != node2->mNodeType ||
      node1->mExtraName != node2->mExtraName) {
    return 0;
  }

  if (node1->mName) {
    if (node2->mName) {
      return (node1->mName == node2->mName);
    }
    return (node1->mName->Equals(*(node2->mNameString)));
  }
  if (node2->mName) {
    return (node2->mName->Equals(*(node1->mNameString)));
  }
  return (node1->mNameString->Equals(*(node2->mNameString)));
}


static void* PR_CALLBACK
AllocTable(void* pool, size_t size)
{
  return malloc(size);
}

static void PR_CALLBACK
FreeTable(void* pool, void* item)
{
  free(item);
}

static PLHashEntry* PR_CALLBACK
AllocEntry(void* pool, const void* key)
{
  return (PLHashEntry*)malloc(sizeof(PLHashEntry));
}

static void PR_CALLBACK
FreeEntry(void* pool, PLHashEntry* he, unsigned flag)
{
  if (flag == HT_FREE_ENTRY) {
    free(he);
  }
}

static PLHashAllocOps allocOps =
  { AllocTable, FreeTable, AllocEntry, FreeEntry };

nsNodeInfoManager::nsNodeInfoManager()
  : mDocument(nullptr),
    mNonDocumentNodeInfos(0),
    mTextNodeInfo(nullptr),
    mCommentNodeInfo(nullptr),
    mDocumentNodeInfo(nullptr)
{
  nsLayoutStatics::AddRef();

  if (gNodeInfoManagerLeakPRLog)
    MOZ_LOG(gNodeInfoManagerLeakPRLog, LogLevel::Debug,
           ("NODEINFOMANAGER %p created", this));

  mNodeInfoHash = PL_NewHashTable(32, GetNodeInfoInnerHashValue,
                                  NodeInfoInnerKeyCompare,
                                  PL_CompareValues, &allocOps, nullptr);
}


nsNodeInfoManager::~nsNodeInfoManager()
{
  if (mNodeInfoHash)
    PL_HashTableDestroy(mNodeInfoHash);

  // Note: mPrincipal may be null here if we never got inited correctly
  mPrincipal = nullptr;

  mBindingManager = nullptr;

  if (gNodeInfoManagerLeakPRLog)
    MOZ_LOG(gNodeInfoManagerLeakPRLog, LogLevel::Debug,
           ("NODEINFOMANAGER %p destroyed", this));

  nsLayoutStatics::Release();
}

NS_IMPL_CYCLE_COLLECTION_CLASS(nsNodeInfoManager)

NS_IMPL_CYCLE_COLLECTION_UNLINK_0(nsNodeInfoManager)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsNodeInfoManager)
  if (tmp->mNonDocumentNodeInfos) {
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE_RAWPTR(mDocument)
  }
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mBindingManager)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(nsNodeInfoManager, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(nsNodeInfoManager, Release)

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_BEGIN(nsNodeInfoManager)
  if (tmp->mDocument) {
    return NS_CYCLE_COLLECTION_PARTICIPANT(nsDocument)->CanSkip(tmp->mDocument, aRemovingAllowed);
  }
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_BEGIN(nsNodeInfoManager)
  if (tmp->mDocument) {
    return NS_CYCLE_COLLECTION_PARTICIPANT(nsDocument)->CanSkipInCC(tmp->mDocument);
  }
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_BEGIN(nsNodeInfoManager)
  if (tmp->mDocument) {
    return NS_CYCLE_COLLECTION_PARTICIPANT(nsDocument)->CanSkipThis(tmp->mDocument);
  }
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_END

nsresult
nsNodeInfoManager::Init(nsIDocument *aDocument)
{
  NS_ENSURE_TRUE(mNodeInfoHash, NS_ERROR_OUT_OF_MEMORY);

  NS_PRECONDITION(!mPrincipal,
                  "Being inited when we already have a principal?");

  mPrincipal = NullPrincipal::Create();

  if (aDocument) {
    mBindingManager = new nsBindingManager(aDocument);
  }

  mDefaultPrincipal = mPrincipal;

  mDocument = aDocument;

  if (gNodeInfoManagerLeakPRLog)
    MOZ_LOG(gNodeInfoManagerLeakPRLog, LogLevel::Debug,
           ("NODEINFOMANAGER %p Init document=%p", this, aDocument));

  return NS_OK;
}

// static
int
nsNodeInfoManager::DropNodeInfoDocument(PLHashEntry *he, int hashIndex, void *arg)
{
  static_cast<mozilla::dom::NodeInfo*>(he->value)->mDocument = nullptr;
  return HT_ENUMERATE_NEXT;
}

void
nsNodeInfoManager::DropDocumentReference()
{
  if (mBindingManager) {
    mBindingManager->DropDocumentReference();
  }

  // This is probably not needed anymore.
  PL_HashTableEnumerateEntries(mNodeInfoHash, DropNodeInfoDocument, nullptr);

  NS_ASSERTION(!mNonDocumentNodeInfos, "Shouldn't have non-document nodeinfos!");
  mDocument = nullptr;
}


already_AddRefed<mozilla::dom::NodeInfo>
nsNodeInfoManager::GetNodeInfo(nsIAtom *aName, nsIAtom *aPrefix,
                               int32_t aNamespaceID, uint16_t aNodeType,
                               nsIAtom* aExtraName /* = nullptr */)
{
  CheckValidNodeInfo(aNodeType, aName, aNamespaceID, aExtraName);

  NodeInfo::NodeInfoInner tmpKey(aName, aPrefix, aNamespaceID, aNodeType,
                                 aExtraName);

  void *node = PL_HashTableLookup(mNodeInfoHash, &tmpKey);

  if (node) {
    RefPtr<NodeInfo> nodeInfo = static_cast<NodeInfo*>(node);

    return nodeInfo.forget();
  }

  RefPtr<NodeInfo> newNodeInfo =
    new NodeInfo(aName, aPrefix, aNamespaceID, aNodeType, aExtraName, this);

  DebugOnly<PLHashEntry*> he =
    PL_HashTableAdd(mNodeInfoHash, &newNodeInfo->mInner, newNodeInfo);
  MOZ_ASSERT(he, "PL_HashTableAdd() failed");

  // Have to do the swap thing, because already_AddRefed<nsNodeInfo>
  // doesn't cast to already_AddRefed<mozilla::dom::NodeInfo>
  ++mNonDocumentNodeInfos;
  if (mNonDocumentNodeInfos == 1) {
    NS_IF_ADDREF(mDocument);
  }

  return newNodeInfo.forget();
}


nsresult
nsNodeInfoManager::GetNodeInfo(const nsAString& aName, nsIAtom *aPrefix,
                               int32_t aNamespaceID, uint16_t aNodeType,
                               NodeInfo** aNodeInfo)
{
#ifdef DEBUG
  {
    nsCOMPtr<nsIAtom> nameAtom = NS_Atomize(aName);
    CheckValidNodeInfo(aNodeType, nameAtom, aNamespaceID, nullptr);
  }
#endif

  NodeInfo::NodeInfoInner tmpKey(aName, aPrefix, aNamespaceID, aNodeType);

  void *node = PL_HashTableLookup(mNodeInfoHash, &tmpKey);

  if (node) {
    RefPtr<NodeInfo> nodeInfo = static_cast<NodeInfo*>(node);
    nodeInfo.forget(aNodeInfo);

    return NS_OK;
  }

  nsCOMPtr<nsIAtom> nameAtom = NS_Atomize(aName);
  NS_ENSURE_TRUE(nameAtom, NS_ERROR_OUT_OF_MEMORY);

  RefPtr<NodeInfo> newNodeInfo =
    new NodeInfo(nameAtom, aPrefix, aNamespaceID, aNodeType, nullptr, this);
  NS_ENSURE_TRUE(newNodeInfo, NS_ERROR_OUT_OF_MEMORY);

  PLHashEntry *he;
  he = PL_HashTableAdd(mNodeInfoHash, &newNodeInfo->mInner, newNodeInfo);
  NS_ENSURE_TRUE(he, NS_ERROR_FAILURE);

  ++mNonDocumentNodeInfos;
  if (mNonDocumentNodeInfos == 1) {
    NS_IF_ADDREF(mDocument);
  }

  newNodeInfo.forget(aNodeInfo);

  return NS_OK;
}


nsresult
nsNodeInfoManager::GetNodeInfo(const nsAString& aName, nsIAtom *aPrefix,
                               const nsAString& aNamespaceURI,
                               uint16_t aNodeType,
                               NodeInfo** aNodeInfo)
{
  int32_t nsid = kNameSpaceID_None;

  if (!aNamespaceURI.IsEmpty()) {
    nsresult rv = nsContentUtils::NameSpaceManager()->
      RegisterNameSpace(aNamespaceURI, nsid);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return GetNodeInfo(aName, aPrefix, nsid, aNodeType, aNodeInfo);
}

already_AddRefed<NodeInfo>
nsNodeInfoManager::GetTextNodeInfo()
{
  RefPtr<mozilla::dom::NodeInfo> nodeInfo;

  if (!mTextNodeInfo) {
    nodeInfo = GetNodeInfo(nsGkAtoms::textTagName, nullptr, kNameSpaceID_None,
                           nsIDOMNode::TEXT_NODE, nullptr);
    // Hold a weak ref; the nodeinfo will let us know when it goes away
    mTextNodeInfo = nodeInfo;
  } else {
    nodeInfo = mTextNodeInfo;
  }

  return nodeInfo.forget();
}

already_AddRefed<NodeInfo>
nsNodeInfoManager::GetCommentNodeInfo()
{
  RefPtr<NodeInfo> nodeInfo;

  if (!mCommentNodeInfo) {
    nodeInfo = GetNodeInfo(nsGkAtoms::commentTagName, nullptr,
                           kNameSpaceID_None, nsIDOMNode::COMMENT_NODE,
                           nullptr);
    // Hold a weak ref; the nodeinfo will let us know when it goes away
    mCommentNodeInfo = nodeInfo;
  }
  else {
    nodeInfo = mCommentNodeInfo;
  }

  return nodeInfo.forget();
}

already_AddRefed<NodeInfo>
nsNodeInfoManager::GetDocumentNodeInfo()
{
  RefPtr<NodeInfo> nodeInfo;

  if (!mDocumentNodeInfo) {
    NS_ASSERTION(mDocument, "Should have mDocument!");
    nodeInfo = GetNodeInfo(nsGkAtoms::documentNodeName, nullptr,
                           kNameSpaceID_None, nsIDOMNode::DOCUMENT_NODE,
                           nullptr);
    // Hold a weak ref; the nodeinfo will let us know when it goes away
    mDocumentNodeInfo = nodeInfo;

    --mNonDocumentNodeInfos;
    if (!mNonDocumentNodeInfos) {
      mDocument->Release(); // Don't set mDocument to null!
    }
  }
  else {
    nodeInfo = mDocumentNodeInfo;
  }

  return nodeInfo.forget();
}

void
nsNodeInfoManager::SetDocumentPrincipal(nsIPrincipal *aPrincipal)
{
  mPrincipal = nullptr;
  if (!aPrincipal) {
    aPrincipal = mDefaultPrincipal;
  }

  NS_ASSERTION(aPrincipal, "Must have principal by this point!");
  MOZ_DIAGNOSTIC_ASSERT(!nsContentUtils::IsExpandedPrincipal(aPrincipal),
                        "Documents shouldn't have an expanded principal");
  if (nsContentUtils::IsExpandedPrincipal(aPrincipal)) {
    Telemetry::Accumulate(Telemetry::DOCUMENT_WITH_EXPANDED_PRINCIPAL, 1);
  }

  mPrincipal = aPrincipal;
}

void
nsNodeInfoManager::RemoveNodeInfo(NodeInfo *aNodeInfo)
{
  NS_PRECONDITION(aNodeInfo, "Trying to remove null nodeinfo from manager!");

  if (aNodeInfo == mDocumentNodeInfo) {
    mDocumentNodeInfo = nullptr;
    mDocument = nullptr;
  } else {
    if (--mNonDocumentNodeInfos == 0) {
      if (mDocument) {
        // Note, whoever calls this method should keep NodeInfoManager alive,
        // even if mDocument gets deleted.
        mDocument->Release();
      }
    }
    // Drop weak reference if needed
    if (aNodeInfo == mTextNodeInfo) {
      mTextNodeInfo = nullptr;
    }
    else if (aNodeInfo == mCommentNodeInfo) {
      mCommentNodeInfo = nullptr;
    }
  }

#ifdef DEBUG
  bool ret =
#endif
  PL_HashTableRemove(mNodeInfoHash, &aNodeInfo->mInner);

  NS_POSTCONDITION(ret, "Can't find mozilla::dom::NodeInfo to remove!!!");
}
