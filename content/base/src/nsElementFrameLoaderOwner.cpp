/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set tw=80 expandtab softtabstop=2 ts=2 sw=2: */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsElementFrameLoaderOwner.h"

#include "nsIDOMDocument.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsContentUtils.h"
#include "mozilla/Preferences.h"
#include "mozilla/ErrorResult.h"
#include "nsIAppsService.h"
#include "nsServiceManagerUtils.h"
#include "mozIApplication.h"
#include "nsIPermissionManager.h"
#include "GeckoProfiler.h"
#include "nsIDocument.h"
#include "nsPIDOMWindow.h"

using namespace mozilla;
using namespace mozilla::dom;

nsElementFrameLoaderOwner::~nsElementFrameLoaderOwner()
{
  if (mFrameLoader) {
    mFrameLoader->Destroy();
  }
}

nsresult
nsElementFrameLoaderOwner::GetContentDocument(nsIDOMDocument** aContentDocument)
{
  NS_PRECONDITION(aContentDocument, "Null out param");
  nsCOMPtr<nsIDOMDocument> document = do_QueryInterface(GetContentDocument());
  document.forget(aContentDocument);
  return NS_OK;
}

nsIDocument*
nsElementFrameLoaderOwner::GetContentDocument()
{
  nsCOMPtr<nsPIDOMWindow> win = GetContentWindow();
  if (!win) {
    return nullptr;
  }

  nsIDocument *doc = win->GetDoc();

  // Return null for cross-origin contentDocument.
  if (!nsContentUtils::SubjectPrincipal()->
        SubsumesConsideringDomain(doc->NodePrincipal())) {
    return nullptr;
  }
  return doc;
}

nsresult
nsElementFrameLoaderOwner::GetContentWindow(nsIDOMWindow** aContentWindow)
{
  NS_PRECONDITION(aContentWindow, "Null out param");
  nsCOMPtr<nsPIDOMWindow> window = GetContentWindow();
  window.forget(aContentWindow);
  return NS_OK;
}

already_AddRefed<nsPIDOMWindow>
nsElementFrameLoaderOwner::GetContentWindow()
{
  EnsureFrameLoader();

  if (!mFrameLoader) {
    return nullptr;
  }

  bool depthTooGreat = false;
  mFrameLoader->GetDepthTooGreat(&depthTooGreat);
  if (depthTooGreat) {
    // Claim to have no contentWindow
    return nullptr;
  }

  nsCOMPtr<nsIDocShell> doc_shell;
  mFrameLoader->GetDocShell(getter_AddRefs(doc_shell));

  nsCOMPtr<nsPIDOMWindow> win = do_GetInterface(doc_shell);

  if (!win) {
    return nullptr;
  }

  NS_ASSERTION(win->IsOuterWindow(),
               "Uh, this window should always be an outer window!");

  return win.forget();
}

void
nsElementFrameLoaderOwner::EnsureFrameLoader()
{
  Element* thisElement = ThisFrameElement();
  if (!thisElement->GetParent() ||
      !thisElement->IsInDoc() ||
      mFrameLoader ||
      mFrameLoaderCreationDisallowed) {
    // If frame loader is there, we just keep it around, cached
    return;
  }

  // Strangely enough, this method doesn't actually ensure that the
  // frameloader exists.  It's more of a best-effort kind of thing.
  mFrameLoader = nsFrameLoader::Create(thisElement, mNetworkCreated);
}

NS_IMETHODIMP
nsElementFrameLoaderOwner::GetFrameLoader(nsIFrameLoader **aFrameLoader)
{
  NS_IF_ADDREF(*aFrameLoader = mFrameLoader);
  return NS_OK;
}

NS_IMETHODIMP_(already_AddRefed<nsFrameLoader>)
nsElementFrameLoaderOwner::GetFrameLoader()
{
  nsRefPtr<nsFrameLoader> loader = mFrameLoader;
  return loader.forget();
}

NS_IMETHODIMP
nsElementFrameLoaderOwner::SwapFrameLoaders(nsIFrameLoaderOwner* aOtherOwner)
{
  // We don't support this yet
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
nsElementFrameLoaderOwner::LoadSrc()
{
  EnsureFrameLoader();

  if (!mFrameLoader) {
    return NS_OK;
  }

  nsresult rv = mFrameLoader->LoadFrame();
#ifdef DEBUG
  if (NS_FAILED(rv)) {
    NS_WARNING("failed to load URL");
  }
#endif

  return rv;
}

void
nsElementFrameLoaderOwner::SwapFrameLoaders(nsXULElement& aOtherOwner,
                                            ErrorResult& aError)
{
  aError.Throw(NS_ERROR_NOT_IMPLEMENTED);
}
