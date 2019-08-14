/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* A namespace class for static content security utilities. */

#include "nsContentSecurityUtils.h"

#include "nsIContentSecurityPolicy.h"
#include "nsIURI.h"

#include "mozilla/dom/Document.h"

#if defined(DEBUG) && !defined(ANDROID)
/* static */
void nsContentSecurityUtils::AssertAboutPageHasCSP(Document* aDocument) {
  // We want to get to a point where all about: pages ship with a CSP. This
  // assertion ensures that we can not deploy new about: pages without a CSP.
  // Initially we will whitelist legacy about: pages which not yet have a CSP
  // attached, but ultimately that whitelist should disappear.
  // Please note that any about: page should not use inline JS or inline CSS,
  // and instead should load JS and CSS from an external file (*.js, *.css)
  // which allows us to apply a strong CSP omitting 'unsafe-inline'. Ideally,
  // the CSP allows precisely the resources that need to be loaded; but it
  // should at least be as strong as:
  // <meta http-equiv="Content-Security-Policy" content="default-src chrome:"/>

  // Check if we are loading an about: URI at all
  nsCOMPtr<nsIURI> documentURI = aDocument->GetDocumentURI();
  if (!documentURI->SchemeIs("about") ||
      Preferences::GetBool("csp.skip_about_page_has_csp_assert")) {
    return;
  }

  // Potentially init the legacy whitelist of about URIs without a CSP.
  static StaticAutoPtr<nsTArray<nsCString>> sLegacyAboutPagesWithNoCSP;
  if (!sLegacyAboutPagesWithNoCSP ||
      Preferences::GetBool("csp.overrule_about_uris_without_csp_whitelist")) {
    sLegacyAboutPagesWithNoCSP = new nsTArray<nsCString>();
    nsAutoCString legacyAboutPages;
    Preferences::GetCString("csp.about_uris_without_csp", legacyAboutPages);
    for (const nsACString& hostString : legacyAboutPages.Split(',')) {
      // please note that for the actual whitelist we only store the path of
      // about: URI. Let's reassemble the full about URI here so we don't
      // have to remove query arguments later.
      nsCString aboutURI;
      aboutURI.AppendLiteral("about:");
      aboutURI.Append(hostString);
      sLegacyAboutPagesWithNoCSP->AppendElement(aboutURI);
    }
    ClearOnShutdown(&sLegacyAboutPagesWithNoCSP);
  }

  // Check if the about URI is whitelisted
  nsAutoCString aboutSpec;
  documentURI->GetSpec(aboutSpec);
  ToLowerCase(aboutSpec);
  for (auto& legacyPageEntry : *sLegacyAboutPagesWithNoCSP) {
    // please note that we perform a substring match here on purpose,
    // so we don't have to deal and parse out all the query arguments
    // the various about pages rely on.
    if (aboutSpec.Find(legacyPageEntry) == 0) {
      return;
    }
  }

  nsCOMPtr<nsIContentSecurityPolicy> csp = aDocument->GetCsp();
  bool foundDefaultSrc = false;
  if (csp) {
    uint32_t policyCount = 0;
    csp->GetPolicyCount(&policyCount);
    nsAutoString parsedPolicyStr;
    for (uint32_t i = 0; i < policyCount; ++i) {
      csp->GetPolicyString(i, parsedPolicyStr);
      if (parsedPolicyStr.Find("default-src") >= 0) {
        foundDefaultSrc = true;
        break;
      }
    }
  }
  if (Preferences::GetBool("csp.overrule_about_uris_without_csp_whitelist")) {
    NS_ASSERTION(foundDefaultSrc, "about: page must have a CSP");
    return;
  }
  MOZ_ASSERT(foundDefaultSrc,
             "about: page must contain a CSP including default-src");
}
#endif
