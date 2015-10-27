/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "txXMLParser.h"
#include "txURIUtils.h"
#include "txXPathTreeWalker.h"

#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsSyncLoadService.h"
#include "nsNetUtil.h"
#include "nsIURI.h"
#include "nsIPrincipal.h"

nsresult
txParseDocumentFromURI(const nsAString& aHref,
                       const txXPathNode& aLoader,
                       nsAString& aErrMsg,
                       txXPathNode** aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);
    *aResult = nullptr;
    nsCOMPtr<nsIURI> documentURI;
    nsresult rv = NS_NewURI(getter_AddRefs(documentURI), aHref);
    NS_ENSURE_SUCCESS(rv, rv);

    nsIDocument* loaderDocument = txXPathNativeNode::getDocument(aLoader);

    nsCOMPtr<nsILoadGroup> loadGroup = loaderDocument->GetDocumentLoadGroup();

    // For the system principal loaderUri will be null here, which is good
    // since that means that chrome documents can load any uri.

    // Raw pointer, we want the resulting txXPathNode to hold a reference to
    // the document.
    nsIDOMDocument* theDocument = nullptr;
    nsAutoSyncOperation sync(loaderDocument);
    rv = nsSyncLoadService::LoadDocument(documentURI,
                                         nsIContentPolicy::TYPE_INTERNAL_XMLHTTPREQUEST,
                                         loaderDocument->NodePrincipal(),
                                         nsILoadInfo::SEC_REQUIRE_CORS_DATA_INHERITS,
                                         loadGroup, true,
                                         loaderDocument->GetReferrerPolicy(),
                                         &theDocument);

    if (NS_FAILED(rv)) {
        aErrMsg.AppendLiteral("Document load of ");
        aErrMsg.Append(aHref);
        aErrMsg.AppendLiteral(" failed.");
        return NS_FAILED(rv) ? rv : NS_ERROR_FAILURE;
    }

    *aResult = txXPathNativeNode::createXPathNode(theDocument);
    if (!*aResult) {
        NS_RELEASE(theDocument);
        return NS_ERROR_FAILURE;
    }

    return NS_OK;
}
