/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "txXMLParser.h"
#include "txURIUtils.h"
#include "txXPathTreeWalker.h"

#include "mozilla/dom/Document.h"
#include "nsSyncLoadService.h"
#include "nsNetUtil.h"
#include "nsIURI.h"

using namespace mozilla;
using namespace mozilla::dom;

Result<txXPathNode, nsresult> txParseDocumentFromURI(const nsAString& aHref,
                                                     const txXPathNode& aLoader,
                                                     nsAString& aErrMsg) {
  nsCOMPtr<nsIURI> documentURI;
  nsresult rv = NS_NewURI(getter_AddRefs(documentURI), aHref);
  NS_ENSURE_SUCCESS(rv, Err(rv));

  Document* loaderDocument = txXPathNativeNode::getDocument(aLoader);

  nsCOMPtr<nsILoadGroup> loadGroup = loaderDocument->GetDocumentLoadGroup();

  // For the system principal loaderUri will be null here, which is good
  // since that means that chrome documents can load any uri.

  // Raw pointer, we want the resulting txXPathNode to hold a reference to
  // the document.
  nsCOMPtr<Document> theDocument;
  nsAutoSyncOperation sync(loaderDocument,
                           SyncOperationBehavior::eSuspendInput);
  rv = nsSyncLoadService::LoadDocument(
      documentURI, nsIContentPolicy::TYPE_INTERNAL_XMLHTTPREQUEST_SYNC,
      loaderDocument->NodePrincipal(),
      nsILoadInfo::SEC_REQUIRE_CORS_INHERITS_SEC_CONTEXT, loadGroup,
      loaderDocument->CookieJarSettings(), true,
      loaderDocument->GetReferrerPolicy(), getter_AddRefs(theDocument));

  if (NS_FAILED(rv)) {
    aErrMsg.AppendLiteral("Document load of ");
    aErrMsg.Append(aHref);
    aErrMsg.AppendLiteral(" failed.");
    return Err(rv);
  }

  return txXPathNode(theDocument);
}
