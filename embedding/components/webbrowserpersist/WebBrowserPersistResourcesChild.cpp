/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebBrowserPersistResourcesChild.h"

#include "WebBrowserPersistDocumentChild.h"
#include "mozilla/dom/ContentChild.h"

namespace mozilla {

NS_IMPL_ISUPPORTS(WebBrowserPersistResourcesChild,
                  nsIWebBrowserPersistResourceVisitor)

WebBrowserPersistResourcesChild::WebBrowserPersistResourcesChild()
{
}

WebBrowserPersistResourcesChild::~WebBrowserPersistResourcesChild()
{
}

NS_IMETHODIMP
WebBrowserPersistResourcesChild::VisitResource(nsIWebBrowserPersistDocument *aDocument,
                                               const nsACString& aURI)
{
    nsCString copiedURI(aURI); // Yay, XPIDL/IPDL mismatch.
    SendVisitResource(copiedURI);
    return NS_OK;
}

NS_IMETHODIMP
WebBrowserPersistResourcesChild::VisitDocument(nsIWebBrowserPersistDocument* aDocument,
                                               nsIWebBrowserPersistDocument* aSubDocument)
{
    auto* subActor = new WebBrowserPersistDocumentChild();
    // As a consequence of how PWebBrowserPersistDocumentConstructor
    // can be sent by both the parent and the child, we must pass the
    // aBrowser and outerWindowID arguments here, but the values are
    // ignored by the parent.  In particular, the TabChild in which
    // persistence started does not necessarily exist at this point;
    // see bug 1203602.
    if (!Manager()->Manager()
        ->SendPWebBrowserPersistDocumentConstructor(subActor, nullptr, 0)) {
        // NOTE: subActor is freed at this point.
        return NS_ERROR_FAILURE;
    }
    // ...but here, IPC won't free subActor until after this returns
    // to the event loop.

    // The order of these two messages will be preserved, because
    // they're the same toplevel protocol and priority.
    //
    // With this ordering, it's always the transition out of START
    // state that causes a document's parent actor to be exposed to
    // XPCOM (for both parent->child and child->parent construction),
    // which simplifies the lifetime management.
    SendVisitDocument(subActor);
    subActor->Start(aSubDocument);
    return NS_OK;
}

NS_IMETHODIMP
WebBrowserPersistResourcesChild::EndVisit(nsIWebBrowserPersistDocument *aDocument,
                                          nsresult aStatus)
{
    Send__delete__(this, aStatus);
    return NS_OK;
}

} // namespace mozilla
