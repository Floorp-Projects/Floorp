/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * A class for handing out nodeinfos and ensuring sharing of them as needed.
 */

#include "nsNodeInfoManager.h"

#include "mozilla/BasePrincipal.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/NodeInfo.h"
#include "mozilla/dom/NodeInfoInlines.h"
#include "mozilla/dom/DocGroup.h"
#include "mozilla/NullPrincipal.h"
#include "mozilla/StaticPrefs_dom.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsAtom.h"
#include "nsIPrincipal.h"
#include "nsContentUtils.h"
#include "nsReadableUtils.h"
#include "nsGkAtoms.h"
#include "nsComponentManagerUtils.h"
#include "nsLayoutStatics.h"
#include "nsHashKeys.h"
#include "nsCCUncollectableMarker.h"
#include "nsNameSpaceManager.h"
#include "nsWindowSizes.h"

using namespace mozilla;
using mozilla::dom::NodeInfo;

#include "mozilla/Logging.h"

static LazyLogModule gNodeInfoManagerLeakPRLog("NodeInfoManagerLeak");
static const uint32_t kInitialNodeInfoHashSize = 32;

nsNodeInfoManager::nsNodeInfoManager()
    : mNodeInfoHash(kInitialNodeInfoHashSize),
      mDocument(nullptr),
      mNonDocumentNodeInfos(0),
      mTextNodeInfo(nullptr),
      mCommentNodeInfo(nullptr),
      mDocumentNodeInfo(nullptr),
      mRecentlyUsedNodeInfos(),
      mArena(nullptr) {
  nsLayoutStatics::AddRef();

  if (gNodeInfoManagerLeakPRLog)
    MOZ_LOG(gNodeInfoManagerLeakPRLog, LogLevel::Debug,
            ("NODEINFOMANAGER %p created", this));
}

nsNodeInfoManager::~nsNodeInfoManager() {
  // Note: mPrincipal may be null here if we never got inited correctly
  mPrincipal = nullptr;

  mArena = nullptr;

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
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(nsNodeInfoManager, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(nsNodeInfoManager, Release)

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_BEGIN(nsNodeInfoManager)
  if (tmp->mDocument) {
    return NS_CYCLE_COLLECTION_PARTICIPANT(mozilla::dom::Document)
        ->CanSkip(tmp->mDocument, aRemovingAllowed);
  }
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_BEGIN(nsNodeInfoManager)
  if (tmp->mDocument) {
    return NS_CYCLE_COLLECTION_PARTICIPANT(mozilla::dom::Document)
        ->CanSkipInCC(tmp->mDocument);
  }
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_BEGIN(nsNodeInfoManager)
  if (tmp->mDocument) {
    return NS_CYCLE_COLLECTION_PARTICIPANT(mozilla::dom::Document)
        ->CanSkipThis(tmp->mDocument);
  }
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_END

nsresult nsNodeInfoManager::Init(mozilla::dom::Document* aDocument) {
  MOZ_ASSERT(!mPrincipal, "Being inited when we already have a principal?");

  mPrincipal = NullPrincipal::CreateWithoutOriginAttributes();

  mDefaultPrincipal = mPrincipal;

  mDocument = aDocument;

  if (gNodeInfoManagerLeakPRLog)
    MOZ_LOG(gNodeInfoManagerLeakPRLog, LogLevel::Debug,
            ("NODEINFOMANAGER %p Init document=%p", this, aDocument));

  return NS_OK;
}

void nsNodeInfoManager::DropDocumentReference() {
  // This is probably not needed anymore.
  for (auto iter = mNodeInfoHash.Iter(); !iter.Done(); iter.Next()) {
    iter.Data()->mDocument = nullptr;
  }

  NS_ASSERTION(!mNonDocumentNodeInfos,
               "Shouldn't have non-document nodeinfos!");
  mDocument = nullptr;
}

already_AddRefed<mozilla::dom::NodeInfo> nsNodeInfoManager::GetNodeInfo(
    nsAtom* aName, nsAtom* aPrefix, int32_t aNamespaceID, uint16_t aNodeType,
    nsAtom* aExtraName /* = nullptr */) {
  CheckValidNodeInfo(aNodeType, aName, aNamespaceID, aExtraName);

  NodeInfo::NodeInfoInner tmpKey(aName, aPrefix, aNamespaceID, aNodeType,
                                 aExtraName);

  auto p = mRecentlyUsedNodeInfos.Lookup(tmpKey);
  if (p) {
    RefPtr<NodeInfo> nodeInfo = p.Data();
    return nodeInfo.forget();
  }

  // We don't use WithEntryHandle here as that would end up storing the
  // temporary key instead of using `mInner`.
  RefPtr<NodeInfo> nodeInfo = mNodeInfoHash.Get(&tmpKey);
  if (!nodeInfo) {
    ++mNonDocumentNodeInfos;
    if (mNonDocumentNodeInfos == 1) {
      NS_IF_ADDREF(mDocument);
    }

    nodeInfo =
        new NodeInfo(aName, aPrefix, aNamespaceID, aNodeType, aExtraName, this);
    mNodeInfoHash.InsertOrUpdate(&nodeInfo->mInner, nodeInfo);
  }

  // Have to do the swap thing, because already_AddRefed<nsNodeInfo>
  // doesn't cast to already_AddRefed<mozilla::dom::NodeInfo>
  p.Set(nodeInfo);
  return nodeInfo.forget();
}

nsresult nsNodeInfoManager::GetNodeInfo(const nsAString& aName, nsAtom* aPrefix,
                                        int32_t aNamespaceID,
                                        uint16_t aNodeType,
                                        NodeInfo** aNodeInfo) {
  // TODO(erahm): Combine this with the atom version.
#ifdef DEBUG
  {
    RefPtr<nsAtom> nameAtom = NS_Atomize(aName);
    CheckValidNodeInfo(aNodeType, nameAtom, aNamespaceID, nullptr);
  }
#endif

  NodeInfo::NodeInfoInner tmpKey(aName, aPrefix, aNamespaceID, aNodeType);

  auto p = mRecentlyUsedNodeInfos.Lookup(tmpKey);
  if (p) {
    RefPtr<NodeInfo> nodeInfo = p.Data();
    nodeInfo.forget(aNodeInfo);
    return NS_OK;
  }

  RefPtr<NodeInfo> nodeInfo = mNodeInfoHash.Get(&tmpKey);
  if (!nodeInfo) {
    ++mNonDocumentNodeInfos;
    if (mNonDocumentNodeInfos == 1) {
      NS_IF_ADDREF(mDocument);
    }

    RefPtr<nsAtom> nameAtom = NS_Atomize(aName);
    nodeInfo =
        new NodeInfo(nameAtom, aPrefix, aNamespaceID, aNodeType, nullptr, this);
    mNodeInfoHash.InsertOrUpdate(&nodeInfo->mInner, nodeInfo);
  }

  p.Set(nodeInfo);
  nodeInfo.forget(aNodeInfo);

  return NS_OK;
}

nsresult nsNodeInfoManager::GetNodeInfo(const nsAString& aName, nsAtom* aPrefix,
                                        const nsAString& aNamespaceURI,
                                        uint16_t aNodeType,
                                        NodeInfo** aNodeInfo) {
  int32_t nsid = kNameSpaceID_None;

  if (!aNamespaceURI.IsEmpty()) {
    nsresult rv = nsContentUtils::NameSpaceManager()->RegisterNameSpace(
        aNamespaceURI, nsid);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return GetNodeInfo(aName, aPrefix, nsid, aNodeType, aNodeInfo);
}

already_AddRefed<NodeInfo> nsNodeInfoManager::GetTextNodeInfo() {
  RefPtr<mozilla::dom::NodeInfo> nodeInfo;

  if (!mTextNodeInfo) {
    nodeInfo = GetNodeInfo(nsGkAtoms::textTagName, nullptr, kNameSpaceID_None,
                           nsINode::TEXT_NODE, nullptr);
    // Hold a weak ref; the nodeinfo will let us know when it goes away
    mTextNodeInfo = nodeInfo;
  } else {
    nodeInfo = mTextNodeInfo;
  }

  return nodeInfo.forget();
}

already_AddRefed<NodeInfo> nsNodeInfoManager::GetCommentNodeInfo() {
  RefPtr<NodeInfo> nodeInfo;

  if (!mCommentNodeInfo) {
    nodeInfo = GetNodeInfo(nsGkAtoms::commentTagName, nullptr,
                           kNameSpaceID_None, nsINode::COMMENT_NODE, nullptr);
    // Hold a weak ref; the nodeinfo will let us know when it goes away
    mCommentNodeInfo = nodeInfo;
  } else {
    nodeInfo = mCommentNodeInfo;
  }

  return nodeInfo.forget();
}

already_AddRefed<NodeInfo> nsNodeInfoManager::GetDocumentNodeInfo() {
  RefPtr<NodeInfo> nodeInfo;

  if (!mDocumentNodeInfo) {
    NS_ASSERTION(mDocument, "Should have mDocument!");
    nodeInfo = GetNodeInfo(nsGkAtoms::documentNodeName, nullptr,
                           kNameSpaceID_None, nsINode::DOCUMENT_NODE, nullptr);
    // Hold a weak ref; the nodeinfo will let us know when it goes away
    mDocumentNodeInfo = nodeInfo;

    --mNonDocumentNodeInfos;
    if (!mNonDocumentNodeInfos) {
      mDocument->Release();  // Don't set mDocument to null!
    }
  } else {
    nodeInfo = mDocumentNodeInfo;
  }

  return nodeInfo.forget();
}

void* nsNodeInfoManager::Allocate(size_t aSize) {
  if (!mHasAllocated) {
    if (mozilla::StaticPrefs::dom_arena_allocator_enabled_AtStartup()) {
      if (!mArena) {
        mozilla::dom::DocGroup* docGroup = GetDocument()->GetDocGroupOrCreate();
        if (docGroup) {
          MOZ_ASSERT(!GetDocument()->HasChildren());
          mArena = docGroup->ArenaAllocator();
        }
      }
#ifdef DEBUG
      else {
        mozilla::dom::DocGroup* docGroup = GetDocument()->GetDocGroup();
        MOZ_ASSERT(docGroup);
        MOZ_ASSERT(mArena == docGroup->ArenaAllocator());
      }
#endif
    }
    mHasAllocated = true;
  }

#ifdef DEBUG
  if (!mozilla::StaticPrefs::dom_arena_allocator_enabled_AtStartup()) {
    MOZ_ASSERT(!mArena, "mArena should not set if the pref is not on");
  };
#endif

  if (mArena) {
    return mArena->Allocate(aSize);
  }
  return malloc(aSize);
}

void nsNodeInfoManager::SetArenaAllocator(mozilla::dom::DOMArena* aArena) {
  MOZ_DIAGNOSTIC_ASSERT_IF(mArena, mArena == aArena);
  MOZ_DIAGNOSTIC_ASSERT(!mHasAllocated);
  MOZ_DIAGNOSTIC_ASSERT(
      mozilla::StaticPrefs::dom_arena_allocator_enabled_AtStartup());

  if (!mArena) {
    mArena = aArena;
  }
}

void nsNodeInfoManager::SetDocumentPrincipal(nsIPrincipal* aPrincipal) {
  mPrincipal = nullptr;
  if (!aPrincipal) {
    aPrincipal = mDefaultPrincipal;
  }

  NS_ASSERTION(aPrincipal, "Must have principal by this point!");
  MOZ_DIAGNOSTIC_ASSERT(!nsContentUtils::IsExpandedPrincipal(aPrincipal),
                        "Documents shouldn't have an expanded principal");

  mPrincipal = aPrincipal;
}

void nsNodeInfoManager::RemoveNodeInfo(NodeInfo* aNodeInfo) {
  MOZ_ASSERT(aNodeInfo, "Trying to remove null nodeinfo from manager!");

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
    } else if (aNodeInfo == mCommentNodeInfo) {
      mCommentNodeInfo = nullptr;
    }
  }

  mRecentlyUsedNodeInfos.Remove(aNodeInfo->mInner);
  DebugOnly<bool> ret = mNodeInfoHash.Remove(&aNodeInfo->mInner);
  MOZ_ASSERT(ret, "Can't find mozilla::dom::NodeInfo to remove!!!");
}

static bool IsSystemOrAddonOrAboutPrincipal(nsIPrincipal* aPrincipal) {
  return aPrincipal->IsSystemPrincipal() ||
         BasePrincipal::Cast(aPrincipal)->AddonPolicy() ||
         // NOTE: about:blank and about:srcdoc inherit the principal of their
         // parent, so aPrincipal->SchemeIs("about") returns false for them.
         aPrincipal->SchemeIs("about");
}

bool nsNodeInfoManager::InternalSVGEnabled() {
  MOZ_ASSERT(!mSVGEnabled, "Caller should use the cached mSVGEnabled!");

  // If the svg.disabled pref. is true, convert all SVG nodes into
  // disabled SVG nodes by swapping the namespace.
  nsNameSpaceManager* nsmgr = nsNameSpaceManager::GetInstance();
  nsCOMPtr<nsILoadInfo> loadInfo;
  bool SVGEnabled = false;

  if (nsmgr && !nsmgr->mSVGDisabled) {
    SVGEnabled = true;
  } else {
    nsCOMPtr<nsIChannel> channel = mDocument->GetChannel();
    // We don't have a channel for SVGs constructed inside a SVG script
    if (channel) {
      loadInfo = channel->LoadInfo();
    }
  }

  // We allow SVG (regardless of the pref) if this is a system or add-on
  // principal or about: page, or if this load was requested for a system or
  // add-on principal or about: page (e.g. a remote image being served as part
  // of system or add-on UI or about: page)
  bool conclusion =
      (SVGEnabled || IsSystemOrAddonOrAboutPrincipal(mPrincipal) ||
       (loadInfo &&
        (loadInfo->GetExternalContentPolicyType() ==
             ExtContentPolicy::TYPE_IMAGE ||
         loadInfo->GetExternalContentPolicyType() ==
             ExtContentPolicy::TYPE_OTHER) &&
        (IsSystemOrAddonOrAboutPrincipal(loadInfo->GetLoadingPrincipal()) ||
         IsSystemOrAddonOrAboutPrincipal(loadInfo->TriggeringPrincipal()))));
  mSVGEnabled = Some(conclusion);
  return conclusion;
}

bool nsNodeInfoManager::InternalMathMLEnabled() {
  MOZ_ASSERT(!mMathMLEnabled, "Caller should use the cached mMathMLEnabled!");

  // If the mathml.disabled pref. is true, convert all MathML nodes into
  // disabled MathML nodes by swapping the namespace.
  nsNameSpaceManager* nsmgr = nsNameSpaceManager::GetInstance();
  bool conclusion =
      ((nsmgr && !nsmgr->mMathMLDisabled) || mPrincipal->IsSystemPrincipal());
  mMathMLEnabled = Some(conclusion);
  return conclusion;
}

void nsNodeInfoManager::AddSizeOfIncludingThis(nsWindowSizes& aSizes) const {
  aSizes.mDOMOtherSize += aSizes.mState.mMallocSizeOf(this);

  // Measurement of the following members may be added later if DMD finds it
  // is worthwhile:
  // - mNodeInfoHash
}
