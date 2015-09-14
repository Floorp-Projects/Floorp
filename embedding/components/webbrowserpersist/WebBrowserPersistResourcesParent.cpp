/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebBrowserPersistResourcesParent.h"

#include "nsThreadUtils.h"

namespace mozilla {

NS_IMPL_ISUPPORTS(WebBrowserPersistResourcesParent,
                  nsIWebBrowserPersistDocumentReceiver)

WebBrowserPersistResourcesParent::WebBrowserPersistResourcesParent(
        nsIWebBrowserPersistDocument* aDocument,
        nsIWebBrowserPersistResourceVisitor* aVisitor)
: mDocument(aDocument)
, mVisitor(aVisitor)
{
    MOZ_ASSERT(aDocument);
    MOZ_ASSERT(aVisitor);
}

WebBrowserPersistResourcesParent::~WebBrowserPersistResourcesParent()
{
}

void
WebBrowserPersistResourcesParent::ActorDestroy(ActorDestroyReason aWhy)
{
    if (aWhy != Deletion && mVisitor) {
        // See comment in WebBrowserPersistDocumentParent::ActorDestroy
        // (or bug 1202887) for why this is deferred.
        nsCOMPtr<nsIRunnable> errorLater = NS_NewRunnableMethodWithArgs
            <nsCOMPtr<nsIWebBrowserPersistDocument>, nsresult>
            (mVisitor, &nsIWebBrowserPersistResourceVisitor::EndVisit,
             mDocument, NS_ERROR_FAILURE);
        NS_DispatchToCurrentThread(errorLater);
    }
    mVisitor = nullptr;
}

bool
WebBrowserPersistResourcesParent::Recv__delete__(const nsresult& aStatus)
{
    mVisitor->EndVisit(mDocument, aStatus);
    mVisitor = nullptr;
    return true;
}

bool
WebBrowserPersistResourcesParent::RecvVisitResource(const nsCString& aURI)
{
    mVisitor->VisitResource(mDocument, aURI);
    return true;
}

bool
WebBrowserPersistResourcesParent::RecvVisitDocument(PWebBrowserPersistDocumentParent* aSubDocument)
{
    // Don't expose the subdocument to the visitor until it's ready
    // (until the actor isn't in START state).
    static_cast<WebBrowserPersistDocumentParent*>(aSubDocument)
        ->SetOnReady(this);
    return true;
}

NS_IMETHODIMP
WebBrowserPersistResourcesParent::OnDocumentReady(nsIWebBrowserPersistDocument* aSubDocument)
{
    if (!mVisitor) {
        return NS_ERROR_FAILURE;
    }
    mVisitor->VisitDocument(mDocument, aSubDocument);
    return NS_OK;
}

NS_IMETHODIMP
WebBrowserPersistResourcesParent::OnError(nsresult aFailure)
{
    // Nothing useful to do but ignore the failed document.
    return NS_OK;
}

} // namespace mozilla
