/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "txURIUtils.h"
#include "nsNetUtil.h"
#include "mozilla/dom/Document.h"
#include "nsIHttpChannelInternal.h"
#include "nsIPrincipal.h"
#include "mozilla/LoadInfo.h"

using mozilla::dom::Document;
using mozilla::net::LoadInfo;

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
}  //-- resolveHref

// static
void URIUtils::ResetWithSource(Document* aNewDoc, nsINode* aSourceNode) {
  nsCOMPtr<Document> sourceDoc = aSourceNode->OwnerDoc();
  nsIPrincipal* sourcePrincipal = sourceDoc->NodePrincipal();
  nsIPrincipal* sourceStoragePrincipal = sourceDoc->EffectiveStoragePrincipal();

  // Copy the channel and loadgroup from the source document.
  nsCOMPtr<nsILoadGroup> loadGroup = sourceDoc->GetDocumentLoadGroup();
  nsCOMPtr<nsIChannel> channel = sourceDoc->GetChannel();
  if (!channel) {
    // Need to synthesize one
    nsresult rv = NS_NewChannel(
        getter_AddRefs(channel), sourceDoc->GetDocumentURI(), sourceDoc,
        nsILoadInfo::SEC_FORCE_INHERIT_PRINCIPAL, nsIContentPolicy::TYPE_OTHER,
        nullptr,  // aPerformanceStorage
        loadGroup,
        nullptr,  // aCallbacks
        nsIChannel::LOAD_BYPASS_SERVICE_WORKER);

    if (NS_FAILED(rv)) {
      return;
    }
  }

  aNewDoc->Reset(channel, loadGroup);
  aNewDoc->SetPrincipals(sourcePrincipal, sourceStoragePrincipal);
  aNewDoc->SetBaseURI(sourceDoc->GetDocBaseURI());

  // Copy charset
  aNewDoc->SetDocumentCharacterSetSource(
      sourceDoc->GetDocumentCharacterSetSource());
  aNewDoc->SetDocumentCharacterSet(sourceDoc->GetDocumentCharacterSet());
}
