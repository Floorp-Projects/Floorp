/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsFrameLoaderOwner.h"
#include "mozilla/dom/BrowserParent.h"
#include "nsFrameLoader.h"
#include "nsFocusManager.h"
#include "nsNetUtil.h"
#include "nsSubDocumentFrame.h"
#include "nsQueryObject.h"
#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/Logging.h"
#include "mozilla/dom/CanonicalBrowsingContext.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/FrameLoaderBinding.h"
#include "mozilla/dom/HTMLIFrameElement.h"
#include "mozilla/dom/MozFrameLoaderOwnerBinding.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/dom/BrowserBridgeChild.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/BrowserBridgeHost.h"
#include "mozilla/dom/BrowserHost.h"
#include "mozilla/StaticPrefs_fission.h"
#include "mozilla/EventStateManager.h"

extern mozilla::LazyLogModule gSHIPBFCacheLog;

using namespace mozilla;
using namespace mozilla::dom;

already_AddRefed<nsFrameLoader> nsFrameLoaderOwner::GetFrameLoader() {
  return do_AddRef(mFrameLoader);
}

void nsFrameLoaderOwner::SetFrameLoader(nsFrameLoader* aNewFrameLoader) {
  mFrameLoader = aNewFrameLoader;
}

mozilla::dom::BrowsingContext* nsFrameLoaderOwner::GetBrowsingContext() {
  if (mFrameLoader) {
    return mFrameLoader->GetBrowsingContext();
  }
  return nullptr;
}

mozilla::dom::BrowsingContext* nsFrameLoaderOwner::GetExtantBrowsingContext() {
  if (mFrameLoader) {
    return mFrameLoader->GetExtantBrowsingContext();
  }
  return nullptr;
}

bool nsFrameLoaderOwner::UseRemoteSubframes() {
  RefPtr<Element> owner = do_QueryObject(this);

  nsILoadContext* loadContext = owner->OwnerDoc()->GetLoadContext();
  MOZ_DIAGNOSTIC_ASSERT(loadContext);

  return loadContext->UseRemoteSubframes();
}

nsFrameLoaderOwner::ChangeRemotenessContextType
nsFrameLoaderOwner::ShouldPreserveBrowsingContext(
    bool aIsRemote, bool aReplaceBrowsingContext) {
  if (aReplaceBrowsingContext) {
    return ChangeRemotenessContextType::DONT_PRESERVE;
  }

  if (XRE_IsParentProcess()) {
    // Don't preserve for remote => parent loads.
    if (!aIsRemote) {
      return ChangeRemotenessContextType::DONT_PRESERVE;
    }

    // Don't preserve for parent => remote loads.
    if (mFrameLoader && !mFrameLoader->IsRemoteFrame()) {
      return ChangeRemotenessContextType::DONT_PRESERVE;
    }
  }

  // We will preserve our browsing context if either fission is enabled, or the
  // `preserve_browsing_contexts` pref is active.
  if (UseRemoteSubframes() ||
      StaticPrefs::fission_preserve_browsing_contexts()) {
    return ChangeRemotenessContextType::PRESERVE;
  }
  return ChangeRemotenessContextType::DONT_PRESERVE;
}

void nsFrameLoaderOwner::ChangeRemotenessCommon(
    const ChangeRemotenessContextType& aContextType,
    const NavigationIsolationOptions& aOptions, bool aSwitchingInProgressLoad,
    bool aIsRemote, BrowsingContextGroup* aGroup,
    std::function<void()>& aFrameLoaderInit, mozilla::ErrorResult& aRv) {
  MOZ_ASSERT_IF(aGroup, aContextType != ChangeRemotenessContextType::PRESERVE);

  RefPtr<mozilla::dom::BrowsingContext> bc;
  bool networkCreated = false;

  // In this case, we're not reparenting a frameloader, we're just destroying
  // our current one and creating a new one, so we can use ourselves as the
  // owner.
  RefPtr<Element> owner = do_QueryObject(this);
  MOZ_ASSERT(owner);

  // When we destroy the original frameloader, it will stop blocking the parent
  // document's load event, and immediately trigger the load event if there are
  // no other blockers. Since we're going to be adding a new blocker as soon as
  // we recreate the frame loader, this is not what we want, so add our own
  // blocker until the process is complete.
  Document* doc = owner->OwnerDoc();
  doc->BlockOnload();
  auto cleanup = MakeScopeExit([&]() { doc->UnblockOnload(false); });

  // If we store the previous nsFrameLoader in the bfcache, this will be filled
  // with the SessionHistoryEntry which now owns the frame.
  RefPtr<SessionHistoryEntry> bfcacheEntry;

  {
    // Introduce a script blocker to ensure no JS is executed during the
    // nsFrameLoader teardown & recreation process. Unload listeners will be run
    // for the previous document, and the load will be started for the new one,
    // at the end of this block.
    nsAutoScriptBlocker sb;

    // If we already have a Frameloader, destroy it, possibly preserving its
    // browsing context.
    if (mFrameLoader) {
      // Calling `GetBrowsingContext` here will force frameloader
      // initialization if it hasn't already happened, which we neither need
      // or want, so we use the initial (possibly pending) browsing context
      // directly, instead.
      bc = mFrameLoader->GetMaybePendingBrowsingContext();
      networkCreated = mFrameLoader->IsNetworkCreated();

      MOZ_ASSERT_IF(aOptions.mTryUseBFCache, aOptions.mReplaceBrowsingContext);
      if (aOptions.mTryUseBFCache && bc) {
        bfcacheEntry = bc->Canonical()->GetActiveSessionHistoryEntry();
        bool useBFCache = bfcacheEntry &&
                          bfcacheEntry == aOptions.mActiveSessionHistoryEntry &&
                          !bfcacheEntry->GetFrameLoader();
        if (useBFCache) {
          MOZ_LOG(gSHIPBFCacheLog, LogLevel::Debug,
                  ("nsFrameLoaderOwner::ChangeRemotenessCommon: store the old "
                   "page in bfcache"));
          Unused << bc->SetIsInBFCache(true);
          bfcacheEntry->SetFrameLoader(mFrameLoader);
          // Session history owns now the frameloader.
          mFrameLoader = nullptr;
        }
      }

      if (mFrameLoader) {
        if (aContextType == ChangeRemotenessContextType::PRESERVE) {
          mFrameLoader->SetWillChangeProcess();
        }

        // Preserve the networkCreated status, as nsDocShells created after a
        // process swap may shouldn't change their dynamically-created status.
        mFrameLoader->Destroy(aSwitchingInProgressLoad);
        mFrameLoader = nullptr;
      }
    }

    mFrameLoader = nsFrameLoader::Recreate(
        owner, bc, aGroup, aOptions, aIsRemote, networkCreated,
        aContextType == ChangeRemotenessContextType::PRESERVE);
    if (NS_WARN_IF(!mFrameLoader)) {
      aRv.Throw(NS_ERROR_FAILURE);
      return;
    }

    // Invoke the frame loader initialization callback to perform setup on our
    // new nsFrameLoader. This may cause our ErrorResult to become errored, so
    // double-check after calling.
    aFrameLoaderInit();
    if (NS_WARN_IF(aRv.Failed())) {
      return;
    }
  }

  // Now that we have a new FrameLoader, we'll eventually need to reset
  // nsSubDocumentFrame to use the new one. We can delay doing this if we're
  // keeping our old frameloader around in the BFCache and the new frame hasn't
  // presented yet to continue painting the previous document.
  const bool retainPaint = bfcacheEntry && mFrameLoader->IsRemoteFrame();
  if (!retainPaint) {
    MOZ_LOG(
        gSHIPBFCacheLog, LogLevel::Debug,
        ("Previous frameLoader not entering BFCache - not retaining paint data"
         "(bfcacheEntry=%p, isRemoteFrame=%d)",
         bfcacheEntry.get(), mFrameLoader->IsRemoteFrame()));
  }

  ChangeFrameLoaderCommon(owner, retainPaint);
}

void nsFrameLoaderOwner::ChangeFrameLoaderCommon(Element* aOwner,
                                                 bool aRetainPaint) {
  // Now that we've got a new FrameLoader, we need to reset our
  // nsSubDocumentFrame to use the new FrameLoader.
  if (nsSubDocumentFrame* ourFrame = do_QueryFrame(aOwner->GetPrimaryFrame())) {
    auto retain = aRetainPaint ? nsSubDocumentFrame::RetainPaintData::Yes
                               : nsSubDocumentFrame::RetainPaintData::No;
    ourFrame->ResetFrameLoader(retain);
  }

  // If the element is focused, or the current mouse over target then
  // we need to update that state for the new BrowserParent too.
  if (nsFocusManager* fm = nsFocusManager::GetFocusManager()) {
    if (fm->GetFocusedElement() == aOwner) {
      fm->ActivateRemoteFrameIfNeeded(*aOwner,
                                      nsFocusManager::GenerateFocusActionId());
    }
  }

  if (aOwner->GetPrimaryFrame()) {
    EventStateManager* eventManager =
        aOwner->GetPrimaryFrame()->PresContext()->EventStateManager();
    eventManager->RecomputeMouseEnterStateForRemoteFrame(*aOwner);
  }

  if (aOwner->IsXULElement()) {
    // Assuming this element is a XULFrameElement, once we've reset our
    // FrameLoader, fire an event to act like we've recreated ourselves, similar
    // to what XULFrameElement does after rebinding to the tree.
    // ChromeOnlyDispatch is turns on to make sure this isn't fired into
    // content.
    (new mozilla::AsyncEventDispatcher(aOwner, u"XULFrameLoaderCreated"_ns,
                                       mozilla::CanBubble::eYes,
                                       mozilla::ChromeOnlyDispatch::eYes))
        ->RunDOMEventWhenSafe();
  }

  mFrameLoader->PropagateIsUnderHiddenEmbedderElement(
      !aOwner->GetPrimaryFrame() ||
      !aOwner->GetPrimaryFrame()->StyleVisibility()->IsVisible());
}

void nsFrameLoaderOwner::ChangeRemoteness(
    const mozilla::dom::RemotenessOptions& aOptions, mozilla::ErrorResult& rv) {
  bool isRemote = !aOptions.mRemoteType.IsEmpty();

  std::function<void()> frameLoaderInit = [&] {
    if (isRemote) {
      mFrameLoader->ConfigRemoteProcess(aOptions.mRemoteType, nullptr);
    }

    if (aOptions.mPendingSwitchID.WasPassed()) {
      mFrameLoader->ResumeLoad(aOptions.mPendingSwitchID.Value());
    } else {
      mFrameLoader->LoadFrame(false);
    }
  };

  auto shouldPreserve = ShouldPreserveBrowsingContext(
      isRemote, /* replaceBrowsingContext */ false);
  NavigationIsolationOptions options;
  ChangeRemotenessCommon(shouldPreserve, options,
                         aOptions.mSwitchingInProgressLoad, isRemote,
                         /* group */ nullptr, frameLoaderInit, rv);
}

void nsFrameLoaderOwner::ChangeRemotenessWithBridge(BrowserBridgeChild* aBridge,
                                                    mozilla::ErrorResult& rv) {
  MOZ_ASSERT(XRE_IsContentProcess());
  if (NS_WARN_IF(!mFrameLoader)) {
    rv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  std::function<void()> frameLoaderInit = [&] {
    MOZ_DIAGNOSTIC_ASSERT(!mFrameLoader->mInitialized);
    RefPtr<BrowserBridgeHost> host = aBridge->FinishInit(mFrameLoader);
    mFrameLoader->mPendingBrowsingContext->SetEmbedderElement(
        mFrameLoader->GetOwnerContent());
    mFrameLoader->mRemoteBrowser = host;
    mFrameLoader->mInitialized = true;
  };

  NavigationIsolationOptions options;
  ChangeRemotenessCommon(ChangeRemotenessContextType::PRESERVE, options,
                         /* inProgress */ true,
                         /* isRemote */ true, /* group */ nullptr,
                         frameLoaderInit, rv);
}

void nsFrameLoaderOwner::ChangeRemotenessToProcess(
    ContentParent* aContentParent, const NavigationIsolationOptions& aOptions,
    BrowsingContextGroup* aGroup, mozilla::ErrorResult& rv) {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT_IF(aGroup, aOptions.mReplaceBrowsingContext);
  bool isRemote = aContentParent != nullptr;

  std::function<void()> frameLoaderInit = [&] {
    if (isRemote) {
      mFrameLoader->ConfigRemoteProcess(aContentParent->GetRemoteType(),
                                        aContentParent);
    }
  };

  auto shouldPreserve =
      ShouldPreserveBrowsingContext(isRemote, aOptions.mReplaceBrowsingContext);
  ChangeRemotenessCommon(shouldPreserve, aOptions, /* inProgress */ true,
                         isRemote, aGroup, frameLoaderInit, rv);
}

void nsFrameLoaderOwner::SubframeCrashed() {
  MOZ_ASSERT(XRE_IsContentProcess());

  std::function<void()> frameLoaderInit = [&] {
    RefPtr<nsFrameLoader> frameLoader = mFrameLoader;
    nsContentUtils::AddScriptRunner(NS_NewRunnableFunction(
        "nsFrameLoaderOwner::SubframeCrashed", [frameLoader]() {
          nsCOMPtr<nsIURI> uri;
          nsresult rv = NS_NewURI(getter_AddRefs(uri), "about:blank");
          if (NS_WARN_IF(NS_FAILED(rv))) {
            return;
          }

          RefPtr<nsDocShell> docShell =
              frameLoader->GetDocShell(IgnoreErrors());
          if (NS_WARN_IF(!docShell)) {
            return;
          }
          bool displayed = false;
          docShell->DisplayLoadError(NS_ERROR_FRAME_CRASHED, uri,
                                     u"about:blank", nullptr, &displayed);
        }));
  };

  NavigationIsolationOptions options;
  ChangeRemotenessCommon(ChangeRemotenessContextType::PRESERVE, options,
                         /* inProgress */ false, /* isRemote */ false,
                         /* group */ nullptr, frameLoaderInit, IgnoreErrors());
}

void nsFrameLoaderOwner::ReplaceFrameLoader(nsFrameLoader* aNewFrameLoader) {
  MOZ_LOG(gSHIPBFCacheLog, LogLevel::Debug,
          ("nsFrameLoaderOwner::ReplaceFrameLoader: Replace frameloader"));

  mFrameLoader = aNewFrameLoader;

  if (auto* browserParent = mFrameLoader->GetBrowserParent()) {
    browserParent->AddWindowListeners();
  }

  RefPtr<Element> owner = do_QueryObject(this);
  ChangeFrameLoaderCommon(owner, /* aRetainPaint = */ false);
}

void nsFrameLoaderOwner::AttachFrameLoader(nsFrameLoader* aFrameLoader) {
  mFrameLoaderList.insertBack(aFrameLoader);
}

void nsFrameLoaderOwner::DetachFrameLoader(nsFrameLoader* aFrameLoader) {
  if (aFrameLoader->isInList()) {
    MOZ_ASSERT(mFrameLoaderList.contains(aFrameLoader));
    aFrameLoader->remove();
  }
}

void nsFrameLoaderOwner::FrameLoaderDestroying(nsFrameLoader* aFrameLoader) {
  if (aFrameLoader == mFrameLoader) {
    while (!mFrameLoaderList.isEmpty()) {
      RefPtr<nsFrameLoader> loader = mFrameLoaderList.popFirst();
      if (loader != mFrameLoader) {
        loader->Destroy();
      }
    }
  } else {
    DetachFrameLoader(aFrameLoader);
  }
}
