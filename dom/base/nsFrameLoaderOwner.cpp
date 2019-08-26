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
#include "mozilla/StaticPrefs_fission.h"

using namespace mozilla;
using namespace mozilla::dom;

already_AddRefed<nsFrameLoader> nsFrameLoaderOwner::GetFrameLoader() {
  return do_AddRef(mFrameLoader);
}

void nsFrameLoaderOwner::SetFrameLoader(nsFrameLoader* aNewFrameLoader) {
  mFrameLoader = aNewFrameLoader;
}

already_AddRefed<mozilla::dom::BrowsingContext>
nsFrameLoaderOwner::GetBrowsingContext() {
  if (mFrameLoader) {
    return mFrameLoader->GetBrowsingContext();
  }
  return nullptr;
}

bool nsFrameLoaderOwner::UseRemoteSubframes() {
  RefPtr<Element> owner = do_QueryObject(this);
  MOZ_ASSERT(this);

  nsILoadContext* loadContext = owner->OwnerDoc()->GetLoadContext();
  MOZ_DIAGNOSTIC_ASSERT(loadContext);

  return loadContext->UseRemoteSubframes();
}

bool nsFrameLoaderOwner::ShouldPreserveBrowsingContext(
    const mozilla::dom::RemotenessOptions& aOptions) {
  if (aOptions.mReplaceBrowsingContext) {
    return false;
  }

  // Don't preserve contexts if this is a chrome (parent process) window
  // that is changing from remote to local.
  if (XRE_IsParentProcess() && (!aOptions.mRemoteType.WasPassed() ||
                                aOptions.mRemoteType.Value().IsVoid())) {
    return false;
  }

  // We will preserve our browsing context if either fission is enabled, or the
  // `preserve_browsing_contexts` pref is active.
  return UseRemoteSubframes() ||
         StaticPrefs::fission_preserve_browsing_contexts();
}

void nsFrameLoaderOwner::ChangeRemoteness(
    const mozilla::dom::RemotenessOptions& aOptions, mozilla::ErrorResult& rv) {
  RefPtr<mozilla::dom::BrowsingContext> bc;

  // If we already have a Frameloader, destroy it, possibly preserving its
  // browsing context.
  if (mFrameLoader) {
    if (ShouldPreserveBrowsingContext(aOptions)) {
      bc = mFrameLoader->GetBrowsingContext();
      mFrameLoader->SkipBrowsingContextDetach();
    }

    mFrameLoader->Destroy();
    mFrameLoader = nullptr;
  }

  // In this case, we're not reparenting a frameloader, we're just destroying
  // our current one and creating a new one, so we can use ourselves as the
  // owner.
  RefPtr<Element> owner = do_QueryObject(this);
  MOZ_ASSERT(owner);
  mFrameLoader = nsFrameLoader::Create(owner, bc, aOptions);

  if (NS_WARN_IF(!mFrameLoader)) {
    return;
  }

  if (aOptions.mError.WasPassed()) {
    nsCOMPtr<nsIURI> uri;
    rv = NS_NewURI(getter_AddRefs(uri), "about:blank");
    if (NS_WARN_IF(rv.Failed())) {
      return;
    }

    nsDocShell* docShell = mFrameLoader->GetDocShell(rv);
    if (NS_WARN_IF(rv.Failed())) {
      return;
    }
    bool displayed = false;
    docShell->DisplayLoadError(static_cast<nsresult>(aOptions.mError.Value()),
                               uri, u"about:blank", nullptr, &displayed);

  } else if (aOptions.mPendingSwitchID.WasPassed()) {
    mFrameLoader->ResumeLoad(aOptions.mPendingSwitchID.Value());
  } else {
    mFrameLoader->LoadFrame(false);
  }

  // Now that we've got a new FrameLoader, we need to reset our
  // nsSubDocumentFrame to use the new FrameLoader.
  if (nsSubDocumentFrame* ourFrame = do_QueryFrame(owner->GetPrimaryFrame())) {
    ourFrame->ResetFrameLoader();
  }

  if (nsFocusManager* fm = nsFocusManager::GetFocusManager()) {
    if (fm->GetFocusedElement() == owner) {
      fm->ActivateRemoteFrameIfNeeded(*owner);
    }
  }

  // Assuming this element is a XULFrameElement, once we've reset our
  // FrameLoader, fire an event to act like we've recreated ourselves, similar
  // to what XULFrameElement does after rebinding to the tree.
  // ChromeOnlyDispatch is turns on to make sure this isn't fired into content.
  (new mozilla::AsyncEventDispatcher(
       owner, NS_LITERAL_STRING("XULFrameLoaderCreated"),
       mozilla::CanBubble::eYes, mozilla::ChromeOnlyDispatch::eYes))
      ->RunDOMEventWhenSafe();
}
