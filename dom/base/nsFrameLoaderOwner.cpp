/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsFrameLoaderOwner.h"
#include "nsFrameLoader.h"
#include "mozilla/dom/FrameLoaderBinding.h"

already_AddRefed<nsFrameLoader> nsFrameLoaderOwner::GetFrameLoader() {
  return do_AddRef(mFrameLoader);
}

void nsFrameLoaderOwner::SetFrameLoader(nsFrameLoader* aNewFrameLoader) {
  mFrameLoader = aNewFrameLoader;
}

void nsFrameLoaderOwner::ChangeRemoteness(
    const mozilla::dom::RemotenessOptions& aOptions, mozilla::ErrorResult& rv) {
  // If we already have a Frameloader, destroy it.
  if (mFrameLoader) {
    mFrameLoader->Destroy();
    mFrameLoader = nullptr;
  }

  // In this case, we're not reparenting a frameloader, we're just destroying
  // our current one and creating a new one, so we can use ourselves as the
  // owner.
  RefPtr<Element> owner = do_QueryObject(this);
  MOZ_ASSERT(owner);
  mFrameLoader = nsFrameLoader::Create(owner, aOptions);
  mFrameLoader->LoadFrame(false);

  // Now that we've got a new FrameLoader, we need to reset our
  // nsSubDocumentFrame use the new FrameLoader. We just unset the frameloader
  // here, and expect that the subdocument frame will pick up the new
  // frameloader lazily.
  nsIFrame* ourFrame = owner->GetPrimaryFrame();
  if (ourFrame) {
    nsSubDocumentFrame* ourFrameFrame = do_QueryFrame(ourFrame);
    if (ourFrameFrame) {
      ourFrameFrame->UnsetFrameLoader();
    }
  }

  // Assuming this element is a XULFrameElement, once we've reset our
  // FrameLoader, fire an event to act like we've recreated ourselves, similar
  // to what XULFrameElement does after rebinding to the tree.
  // ChromeOnlyDispatch is turns on to make sure this isn't fired into content.
  (new AsyncEventDispatcher(owner, NS_LITERAL_STRING("XULFrameLoaderCreated"),
                            CanBubble::eYes, ChromeOnlyDispatch::eYes))
    ->RunDOMEventWhenSafe();

}
