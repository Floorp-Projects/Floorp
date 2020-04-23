/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebBrowserPersistResourcesParent.h"

#include "nsThreadUtils.h"

#include "mozilla/dom/BrowserParent.h"
#include "mozilla/dom/CanonicalBrowsingContext.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/WindowGlobalParent.h"

namespace mozilla {

NS_IMPL_ISUPPORTS(WebBrowserPersistResourcesParent,
                  nsIWebBrowserPersistDocumentReceiver)

WebBrowserPersistResourcesParent::WebBrowserPersistResourcesParent(
    nsIWebBrowserPersistDocument* aDocument,
    nsIWebBrowserPersistResourceVisitor* aVisitor)
    : mDocument(aDocument), mVisitor(aVisitor) {
  MOZ_ASSERT(aDocument);
  MOZ_ASSERT(aVisitor);
}

WebBrowserPersistResourcesParent::~WebBrowserPersistResourcesParent() = default;

void WebBrowserPersistResourcesParent::ActorDestroy(ActorDestroyReason aWhy) {
  if (aWhy != Deletion && mVisitor) {
    // See comment in WebBrowserPersistDocumentParent::ActorDestroy
    // (or bug 1202887) for why this is deferred.
    nsCOMPtr<nsIRunnable> errorLater =
        NewRunnableMethod<nsCOMPtr<nsIWebBrowserPersistDocument>, nsresult>(
            "nsIWebBrowserPersistResourceVisitor::EndVisit", mVisitor,
            &nsIWebBrowserPersistResourceVisitor::EndVisit, mDocument,
            NS_ERROR_FAILURE);
    NS_DispatchToCurrentThread(errorLater);
  }
  mVisitor = nullptr;
}

mozilla::ipc::IPCResult WebBrowserPersistResourcesParent::Recv__delete__(
    const nsresult& aStatus) {
  mVisitor->EndVisit(mDocument, aStatus);
  mVisitor = nullptr;
  return IPC_OK();
}

mozilla::ipc::IPCResult WebBrowserPersistResourcesParent::RecvVisitResource(
    const nsCString& aURI, const nsContentPolicyType& aContentPolicyType) {
  mVisitor->VisitResource(mDocument, aURI, aContentPolicyType);
  return IPC_OK();
}

mozilla::ipc::IPCResult WebBrowserPersistResourcesParent::RecvVisitDocument(
    PWebBrowserPersistDocumentParent* aSubDocument) {
  // Don't expose the subdocument to the visitor until it's ready
  // (until the actor isn't in START state).
  static_cast<WebBrowserPersistDocumentParent*>(aSubDocument)->SetOnReady(this);
  return IPC_OK();
}

mozilla::ipc::IPCResult
WebBrowserPersistResourcesParent::RecvVisitBrowsingContext(
    const dom::MaybeDiscarded<dom::BrowsingContext>& aContext) {
  if (aContext.IsNullOrDiscarded()) {
    // Nothing useful to do but ignore the discarded context.
    return IPC_OK();
  }

  mVisitor->VisitBrowsingContext(mDocument, aContext.get());
  return IPC_OK();
}

NS_IMETHODIMP
WebBrowserPersistResourcesParent::OnDocumentReady(
    nsIWebBrowserPersistDocument* aSubDocument) {
  if (!mVisitor) {
    return NS_ERROR_FAILURE;
  }
  mVisitor->VisitDocument(mDocument, aSubDocument);
  return NS_OK;
}

NS_IMETHODIMP
WebBrowserPersistResourcesParent::OnError(nsresult aFailure) {
  // Nothing useful to do but ignore the failed document.
  return NS_OK;
}

}  // namespace mozilla
