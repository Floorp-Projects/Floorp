/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "txURIUtils.h"
#include "nsNetUtil.h"
#include "nsIDocument.h"
#include "nsIPrincipal.h"

/**
 * URIUtils
 * A set of utilities for handling URIs
**/

/**
 * Resolves the given href argument, using the given documentBase
 * if necessary.
 * The new resolved href will be appended to the given dest String
**/
void URIUtils::resolveHref(const nsAString& href, const nsAString& base,
                           nsAString& dest) {
    if (base.IsEmpty()) {
        dest.Append(href);
        return;
    }
    if (href.IsEmpty()) {
        dest.Append(base);
        return;
    }
    nsCOMPtr<nsIURI> pURL;
    nsAutoString resultHref;
    nsresult result = NS_NewURI(getter_AddRefs(pURL), base);
    if (NS_SUCCEEDED(result)) {
        NS_MakeAbsoluteURI(resultHref, href, pURL);
        dest.Append(resultHref);
    }
} //-- resolveHref

// static
void
URIUtils::ResetWithSource(nsIDocument *aNewDoc, nsIDOMNode *aSourceNode)
{
    nsCOMPtr<nsINode> node = do_QueryInterface(aSourceNode);
    if (!node) {
        // XXXbz passing nsnull as the first arg to Reset is illegal
        aNewDoc->Reset(nsnull, nsnull);
        return;
    }

    nsCOMPtr<nsIDocument> sourceDoc = node->OwnerDoc();
    nsIPrincipal* sourcePrincipal = sourceDoc->NodePrincipal();

    // Copy the channel and loadgroup from the source document.
    nsCOMPtr<nsILoadGroup> loadGroup = sourceDoc->GetDocumentLoadGroup();
    nsCOMPtr<nsIChannel> channel = sourceDoc->GetChannel();
    if (!channel) {
        // Need to synthesize one
        if (NS_FAILED(NS_NewChannel(getter_AddRefs(channel),
                                    sourceDoc->GetDocumentURI(),
                                    nsnull,
                                    loadGroup))) {
            return;
        }
        channel->SetOwner(sourcePrincipal);
    }
    aNewDoc->Reset(channel, loadGroup);
    aNewDoc->SetPrincipal(sourcePrincipal);
    aNewDoc->SetBaseURI(sourceDoc->GetDocBaseURI());

    // Copy charset
    aNewDoc->SetDocumentCharacterSetSource(
          sourceDoc->GetDocumentCharacterSetSource());
    aNewDoc->SetDocumentCharacterSet(sourceDoc->GetDocumentCharacterSet());
}
