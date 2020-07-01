/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsFrameLoaderOwner.h"
#include "nsFrameLoader.h"
#include "nsFocusManager.h"
#include "nsSubDocumentFrame.h"
#include "nsQueryObject.h"
#include "mozilla/AsyncEventDispatcher.h"
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
    bool aSwitchingInProgressLoad, bool aIsRemote,
    std::function<void()>& aFrameLoaderInit, mozilla::ErrorResult& aRv) {
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
      if (aContextType == ChangeRemotenessContextType::PRESERVE) {
        mFrameLoader->SetWillChangeProcess();
      }

      // Preserve the networkCreated status, as nsDocShells created after a
      // process swap may shouldn't change their dynamically-created status.
      networkCreated = mFrameLoader->IsNetworkCreated();
      mFrameLoader->Destroy(aSwitchingInProgressLoad);
      mFrameLoader = nullptr;
    }

    mFrameLoader = nsFrameLoader::Recreate(
        owner, bc, aIsRemote, networkCreated,
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

  // If we're switching process for an in progress load, then suppress
  // progress events from the new BrowserParent to prevent duplicate
  // events for the new initial about:blank and the new 'start' event.
  if (aSwitchingInProgressLoad && mFrameLoader->GetBrowserParent()) {
    mFrameLoader->GetBrowserParent()
        ->SuspendProgressEventsUntilAfterNextLoadStarts();
  }

  // Now that we've got a new FrameLoader, we need to reset our
  // nsSubDocumentFrame to use the new FrameLoader.
  if (nsSubDocumentFrame* ourFrame = do_QueryFrame(owner->GetPrimaryFrame())) {
    ourFrame->ResetFrameLoader();
  }

  // If the element is focused, or the current mouse over target then
  // we need to update that state for the new BrowserParent too.
  if (nsFocusManager* fm = nsFocusManager::GetFocusManager()) {
    if (fm->GetFocusedElement() == owner) {
      fm->ActivateRemoteFrameIfNeeded(*owner);
    }
  }

  if (owner->GetPrimaryFrame()) {
    EventStateManager* eventManager =
        owner->GetPrimaryFrame()->PresContext()->EventStateManager();
    eventManager->RecomputeMouseEnterStateForRemoteFrame(*owner);
  }

  if (owner->IsXULElement()) {
    // Assuming this element is a XULFrameElement, once we've reset our
    // FrameLoader, fire an event to act like we've recreated ourselves, similar
    // to what XULFrameElement does after rebinding to the tree.
    // ChromeOnlyDispatch is turns on to make sure this isn't fired into
    // content.
    (new mozilla::AsyncEventDispatcher(owner, u"XULFrameLoaderCreated"_ns,
                                       mozilla::CanBubble::eYes,
                                       mozilla::ChromeOnlyDispatch::eYes))
        ->RunDOMEventWhenSafe();
  }
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
  ChangeRemotenessCommon(shouldPreserve, aOptions.mSwitchingInProgressLoad,
                         isRemote, frameLoaderInit, rv);
}

void nsFrameLoaderOwner::ChangeRemotenessWithBridge(BrowserBridgeChild* aBridge,
                                                    mozilla::ErrorResult& rv) {
  MOZ_ASSERT(XRE_IsContentProcess());
  if (NS_WARN_IF(!mFrameLoader)) {
    rv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  std::function<void()> frameLoaderInit = [&] {
    RefPtr<BrowserBridgeHost> host = aBridge->FinishInit(mFrameLoader);
    mFrameLoader->mPendingBrowsingContext->SetEmbedderElement(
        mFrameLoader->GetOwnerContent());
    mFrameLoader->mRemoteBrowser = host;
  };

  ChangeRemotenessCommon(ChangeRemotenessContextType::PRESERVE,
                         /* inProgress */ true,
                         /* isRemote */ true, frameLoaderInit, rv);
}

void nsFrameLoaderOwner::ChangeRemotenessToProcess(
    ContentParent* aContentParent, bool aReplaceBrowsingContext,
    mozilla::ErrorResult& rv) {
  MOZ_ASSERT(XRE_IsParentProcess());
  bool isRemote = aContentParent != nullptr;

  std::function<void()> frameLoaderInit = [&] {
    if (isRemote) {
      mFrameLoader->ConfigRemoteProcess(aContentParent->GetRemoteType(),
                                        aContentParent);
    }

    // FIXME(bug 1644779): We'd like to stop triggering a load here, as this
    // reads the attributes, such as `src`, on the <browser> element, and could
    // start another load which will be clobbered shortly.
    //
    // This is OK for now, as we're mimicing the existing process switching
    // behaviour, and <browser> elements created by tabbrowser don't have the
    // `src` attribute specified.
    mFrameLoader->LoadFrame(false);
  };

  auto shouldPreserve =
      ShouldPreserveBrowsingContext(isRemote, aReplaceBrowsingContext);
  ChangeRemotenessCommon(shouldPreserve, /* inProgress */ true, isRemote,
                         frameLoaderInit, rv);
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

  ChangeRemotenessCommon(ChangeRemotenessContextType::PRESERVE,
                         /* inProgress */ false,
                         /* isRemote */ false, frameLoaderInit, IgnoreErrors());
}
