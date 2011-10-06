/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Boris Zbarsky <bzbarsky@mit.edu>.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

NS_IMETHODIMP
nsDataDocumentContentPolicy::ShouldLoad(PRUint32 aContentType,
                                        nsIURI *aContentLocation,
                                        nsIURI *aRequestingLocation,
                                        nsISupports *aRequestingContext,
                                        const nsACString &aMimeGuess,
                                        nsISupports *aExtra,
                                        PRInt16 *aDecision)
{
  *aDecision = nsIContentPolicy::ACCEPT;
  // Look for the document.  In most cases, aRequestingContext is a node.
  nsCOMPtr<nsIDocument> doc;
  nsCOMPtr<nsINode> node = do_QueryInterface(aRequestingContext);
  if (node) {
    doc = node->GetOwnerDoc();
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
    // Allow local resources for SVG-as-an-image documents, but disallow
    // everything else, to prevent data leakage
    bool hasFlags;
    nsresult rv = NS_URIChainHasFlags(aContentLocation,
                                      nsIProtocolHandler::URI_IS_LOCAL_RESOURCE,
                                      &hasFlags);
    if (NS_FAILED(rv) || !hasFlags) {
      // resource is not local (or we couldn't tell) - reject!
      *aDecision = nsIContentPolicy::REJECT_TYPE;

      // report error, if we can.
      if (node) {
        nsIPrincipal* requestingPrincipal = node->NodePrincipal();
        nsRefPtr<nsIURI> principalURI;
        rv = requestingPrincipal->GetURI(getter_AddRefs(principalURI));
        if (NS_SUCCEEDED(rv) && principalURI) {
          nsScriptSecurityManager::ReportError(
            nsnull, NS_LITERAL_STRING("CheckSameOriginError"), principalURI,
            aContentLocation);
        }
      }
    } else if (aContentType == nsIContentPolicy::TYPE_IMAGE &&
               doc->GetDocumentURI()) {
      // Check for (& disallow) recursive image-loads
      bool isRecursiveLoad;
      rv = aContentLocation->EqualsExceptRef(doc->GetDocumentURI(),
                                             &isRecursiveLoad);
      if (NS_FAILED(rv) || isRecursiveLoad) {
        NS_WARNING("Refusing to recursively load image");
        *aDecision = nsIContentPolicy::REJECT_TYPE;
      }
    }
    return NS_OK;
  }

  // Allow all loads for non-external-resource documents
  if (!doc->GetDisplayDocument()) {
    return NS_OK;
  }

  // For external resources, blacklist some load types
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
                                           PRInt16 *aDecision)
{
  return ShouldLoad(aContentType, aContentLocation, aRequestingLocation,
                    aRequestingContext, aMimeGuess, aExtra, aDecision);
}
