/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Content policy implementation that prevents all loads of images,
 * subframes, etc from documents loaded as data (eg documents loaded
 * via XMLHttpRequest).
 */

#include "nsDataDocumentContentPolicy.h"
#include "nsNetUtil.h"
#include "nsScriptSecurityManager.h"
#include "nsIDocument.h"
#include "nsINode.h"
#include "nsIDOMWindow.h"
#include "nsIDOMDocument.h"

NS_IMPL_ISUPPORTS1(nsDataDocumentContentPolicy, nsIContentPolicy)

// Helper method for ShouldLoad()
// Checks a URI for the given flags.  Returns true if the URI has the flags,
// and false if not (or if we weren't able to tell).
static bool
HasFlags(nsIURI* aURI, PRUint32 aURIFlags)
{
  bool hasFlags;
  nsresult rv = NS_URIChainHasFlags(aURI, aURIFlags, &hasFlags);
  return NS_SUCCEEDED(rv) && hasFlags;
}

NS_IMETHODIMP
nsDataDocumentContentPolicy::ShouldLoad(PRUint32 aContentType,
                                        nsIURI *aContentLocation,
                                        nsIURI *aRequestingLocation,
                                        nsISupports *aRequestingContext,
                                        const nsACString &aMimeGuess,
                                        nsISupports *aExtra,
                                        nsIPrincipal *aRequestPrincipal,
                                        PRInt16 *aDecision)
{
  *aDecision = nsIContentPolicy::ACCEPT;
  // Look for the document.  In most cases, aRequestingContext is a node.
  nsCOMPtr<nsIDocument> doc;
  nsCOMPtr<nsINode> node = do_QueryInterface(aRequestingContext);
  if (node) {
    doc = node->OwnerDoc();
  } else {
    nsCOMPtr<nsIDOMWindow> window = do_QueryInterface(aRequestingContext);
    if (window) {
      nsCOMPtr<nsIDOMDocument> domDoc;
      window->GetDocument(getter_AddRefs(domDoc));
      doc = do_QueryInterface(domDoc);
    }
  }

  // DTDs are always OK to load
  if (!doc || aContentType == nsIContentPolicy::TYPE_DTD) {
    return NS_OK;
  }

  // Nothing else is OK to load for data documents
  if (doc->IsLoadedAsData()) {
    *aDecision = nsIContentPolicy::REJECT_TYPE;
    return NS_OK;
  }

  if (doc->IsBeingUsedAsImage()) {
    // We only allow SVG images to load content from URIs that are local and
    // also satisfy one of the following conditions:
    //  - URI inherits security context, e.g. data URIs
    //   OR
    //  - URI loadable by subsumers, e.g. blob URIs
    // Any URI that doesn't meet these requirements will be rejected below.
    if (!HasFlags(aContentLocation,
                  nsIProtocolHandler::URI_IS_LOCAL_RESOURCE) ||
        (!HasFlags(aContentLocation,
                   nsIProtocolHandler::URI_INHERITS_SECURITY_CONTEXT) &&
         !HasFlags(aContentLocation,
                   nsIProtocolHandler::URI_LOADABLE_BY_SUBSUMERS))) {
      *aDecision = nsIContentPolicy::REJECT_TYPE;

      // Report error, if we can.
      if (node) {
        nsIPrincipal* requestingPrincipal = node->NodePrincipal();
        nsRefPtr<nsIURI> principalURI;
        nsresult rv =
          requestingPrincipal->GetURI(getter_AddRefs(principalURI));
        if (NS_SUCCEEDED(rv) && principalURI) {
          nsScriptSecurityManager::ReportError(
            nullptr, NS_LITERAL_STRING("CheckSameOriginError"), principalURI,
            aContentLocation);
        }
      }
    } else if (aContentType == nsIContentPolicy::TYPE_IMAGE &&
               doc->GetDocumentURI()) {
      // Check for (& disallow) recursive image-loads
      bool isRecursiveLoad;
      nsresult rv = aContentLocation->EqualsExceptRef(doc->GetDocumentURI(),
                                                      &isRecursiveLoad);
      if (NS_FAILED(rv) || isRecursiveLoad) {
        NS_WARNING("Refusing to recursively load image");
        *aDecision = nsIContentPolicy::REJECT_TYPE;
      }
    }
    return NS_OK;
  }

  // Allow all loads for non-resource documents
  if (!doc->IsResourceDoc()) {
    return NS_OK;
  }

  // For resource documents, blacklist some load types
  if (aContentType == nsIContentPolicy::TYPE_OBJECT ||
      aContentType == nsIContentPolicy::TYPE_DOCUMENT ||
      aContentType == nsIContentPolicy::TYPE_SUBDOCUMENT ||
      aContentType == nsIContentPolicy::TYPE_SCRIPT) {
    *aDecision = nsIContentPolicy::REJECT_TYPE;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDataDocumentContentPolicy::ShouldProcess(PRUint32 aContentType,
                                           nsIURI *aContentLocation,
                                           nsIURI *aRequestingLocation,
                                           nsISupports *aRequestingContext,
                                           const nsACString &aMimeGuess,
                                           nsISupports *aExtra,
                                           nsIPrincipal *aRequestPrincipal,
                                           PRInt16 *aDecision)
{
  return ShouldLoad(aContentType, aContentLocation, aRequestingLocation,
                    aRequestingContext, aMimeGuess, aExtra, aRequestPrincipal,
                    aDecision);
}
