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

bool nsFrameLoaderOwner::UseRemoteSubframes() {
  RefPtr<Element> owner = do_QueryObject(this);

  nsILoadContext* loadContext = owner->OwnerDoc()->GetLoadContext();
  MOZ_DIAGNOSTIC_ASSERT(loadContext);

  return loadContext->UseRemoteSubframes();
}

bool nsFrameLoaderOwner::ShouldPreserveBrowsingContext(
    const mozilla::dom::RemotenessOptions& aOptions) {
  if (aOptions.mReplaceBrowsingContext) {
    return false;
  }

  if (XRE_IsParentProcess()) {
    // Don't preserve for remote => parent loads.
    if (aOptions.mRemoteType.IsVoid()) {
      return false;
    }

    // Don't preserve for parent => remote loads.
    if (mFrameLoader && !mFrameLoader->IsRemoteFrame()) {
      return false;
    }
  }

  // We will preserve our browsing context if either fission is enabled, or the
  // `preserve_browsing_contexts` pref is active.
  return UseRemoteSubframes() ||
         StaticPrefs::fission_preserve_browsing_contexts();
}

void nsFrameLoaderOwner::ChangeRemotenessCommon(
    const ChangeRemotenessContextType& aContextType,
    bool aSwitchingInProgressLoad, const nsAString& aRemoteType,
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
      if (aContextType != ChangeRemotenessContextType::DONT_PRESERVE) {
        bc = mFrameLoader->GetBrowsingContext();
        if (aContextType == ChangeRemotenessContextType::PRESERVE) {
          mFrameLoader->SetWillChangeProcess();
        }
      }

      // Preserve the networkCreated status, as nsDocShells created after a
      // process swap may shouldn't change their dynamically-created status.
      networkCreated = mFrameLoader->IsNetworkCreated();
      mFrameLoader->Destroy(aSwitchingInProgressLoad);
      mFrameLoader = nullptr;
    }

    mFrameLoader = nsFrameLoader::Recreate(
        owner, bc, aRemoteType, networkCreated,
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
    (new mozilla::AsyncEventDispatcher(
         owner, NS_LITERAL_STRING("XULFrameLoaderCreated"),
         mozilla::CanBubble::eYes, mozilla::ChromeOnlyDispatch::eYes))
        ->RunDOMEventWhenSafe();
  }
}

void nsFrameLoaderOwner::ChangeRemoteness(
    const mozilla::dom::RemotenessOptions& aOptions, mozilla::ErrorResult& rv) {
  std::function<void()> frameLoaderInit = [&] {
    if (aOptions.mError.WasPassed()) {
      RefPtr<nsFrameLoader> frameLoader = mFrameLoader;
      nsresult error = static_cast<nsresult>(aOptions.mError.Value());
      nsContentUtils::AddScriptRunner(NS_NewRunnableFunction(
          "nsFrameLoaderOwner::DisplayLoadError", [frameLoader, error]() {
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
            docShell->DisplayLoadError(error, uri, u"about:blank", nullptr,
                                       &displayed);
          }));
    } else if (aOptions.mPendingSwitchID.WasPassed()) {
      mFrameLoader->ResumeLoad(aOptions.mPendingSwitchID.Value());
    } else {
      mFrameLoader->LoadFrame(false);
    }
  };

  ChangeRemotenessContextType preserveType =
      ChangeRemotenessContextType::DONT_PRESERVE;
  if (ShouldPreserveBrowsingContext(aOptions)) {
    preserveType = ChangeRemotenessContextType::PRESERVE;
  } else if (aOptions.mReplaceBrowsingContext) {
    preserveType = ChangeRemotenessContextType::DONT_PRESERVE_BUT_PROPAGATE;
  }

  ChangeRemotenessCommon(preserveType, aOptions.mSwitchingInProgressLoad,
                         aOptions.mRemoteType, frameLoaderInit, rv);
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

  // NOTE: We always use the DEFAULT_REMOTE_TYPE here, because we don't actually
  // know the real remote type, and don't need to, as we're a content process.
  ChangeRemotenessCommon(
      /* preserve */ ChangeRemotenessContextType::PRESERVE,
      /* switching in progress load */ true,
      NS_LITERAL_STRING(DEFAULT_REMOTE_TYPE), frameLoaderInit, rv);
}
