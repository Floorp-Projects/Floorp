/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FontPreloader.h"

#include "gfxUserFontSet.h"
#include "nsIClassOfService.h"
#include "nsISupportsPriority.h"

namespace mozilla {

FontPreloader::FontPreloader()
    : FetchPreloader(nsIContentPolicy::TYPE_INTERNAL_FONT_PRELOAD) {}

void FontPreloader::PrioritizeAsPreload() { PrioritizeAsPreload(Channel()); }

nsresult FontPreloader::CreateChannel(
    nsIChannel** aChannel, nsIURI* aURI, const CORSMode aCORSMode,
    const dom::ReferrerPolicy& aReferrerPolicy, dom::Document* aDocument,
    nsILoadGroup* aLoadGroup, nsIInterfaceRequestor* aCallbacks) {
  return BuildChannel(aChannel, aURI, aCORSMode, aReferrerPolicy, nullptr,
                      nullptr, aDocument, aLoadGroup, aCallbacks, true);
}

// static
void FontPreloader::PrioritizeAsPreload(nsIChannel* aChannel) {
  nsCOMPtr<nsIClassOfService> cos(do_QueryInterface(aChannel));
  if (cos) {
    cos->AddClassFlags(nsIClassOfService::Unblocked);
  }
}

// static
nsresult FontPreloader::BuildChannel(
    nsIChannel** aChannel, nsIURI* aURI, const CORSMode aCORSMode,
    const dom::ReferrerPolicy& aReferrerPolicy,
    gfxUserFontEntry* aUserFontEntry, const gfxFontFaceSrc* aFontFaceSrc,
    dom::Document* aDocument, nsILoadGroup* aLoadGroup,
    nsIInterfaceRequestor* aCallbacks, bool aIsPreload) {
  nsresult rv;

  nsIPrincipal* principal = aUserFontEntry
                                ? (aUserFontEntry->GetPrincipal()
                                       ? aUserFontEntry->GetPrincipal()->get()
                                       : nullptr)
                                : aDocument->NodePrincipal();

  // aCORSMode is ignored.  We always load as crossorigin=anonymous, but a
  // preload started with anything other then "anonymous" will never be found.

  uint32_t securityFlags = 0;
  if (aURI->SchemeIs("file")) {
    securityFlags = nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_INHERITS;
  } else {
    securityFlags = nsILoadInfo::SEC_REQUIRE_CORS_DATA_INHERITS;
  }

  nsContentPolicyType contentPolicyType =
      aIsPreload ? nsIContentPolicy::TYPE_INTERNAL_FONT_PRELOAD
                 : nsIContentPolicy::TYPE_FONT;

  nsCOMPtr<nsIChannel> channel;
  // Note we are calling NS_NewChannelWithTriggeringPrincipal() with both a
  // node and a principal.  This is because the document where the font is
  // being loaded might have a different origin from the principal of the
  // stylesheet that initiated the font load.
  rv = NS_NewChannelWithTriggeringPrincipal(getter_AddRefs(channel), aURI,
                                            aDocument, principal, securityFlags,
                                            contentPolicyType,
                                            nullptr,  // PerformanceStorage
                                            aLoadGroup);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(channel));
  if (httpChannel) {
    rv = httpChannel->SetRequestHeader(
        NS_LITERAL_CSTRING("Accept"),
        NS_LITERAL_CSTRING("application/font-woff2;q=1.0,application/"
                           "font-woff;q=0.9,*/*;q=0.8"),
        false);
    NS_ENSURE_SUCCESS(rv, rv);

    if (aFontFaceSrc) {
      rv = httpChannel->SetReferrerInfo(aFontFaceSrc->mReferrerInfo);
      Unused << NS_WARN_IF(NS_FAILED(rv));

      // For WOFF and WOFF2, we should tell servers/proxies/etc NOT to try
      // and apply additional compression at the content-encoding layer
      if (aFontFaceSrc->mFormatFlags & (gfxUserFontSet::FLAG_FORMAT_WOFF |
                                        gfxUserFontSet::FLAG_FORMAT_WOFF2)) {
        rv = httpChannel->SetRequestHeader(
            NS_LITERAL_CSTRING("Accept-Encoding"),
            NS_LITERAL_CSTRING("identity"), false);
        NS_ENSURE_SUCCESS(rv, rv);
      }
    } else {
      nsCOMPtr<nsIReferrerInfo> referrerInfo = new dom::ReferrerInfo(
          aDocument->GetDocumentURIAsReferrer(), aReferrerPolicy);
      rv = httpChannel->SetReferrerInfoWithoutClone(referrerInfo);
      MOZ_ASSERT(NS_SUCCEEDED(rv));
    }
  }

  nsCOMPtr<nsISupportsPriority> priorityChannel(do_QueryInterface(channel));
  if (priorityChannel) {
    priorityChannel->AdjustPriority(nsISupportsPriority::PRIORITY_HIGH);
  }
  nsCOMPtr<nsIClassOfService> cos(do_QueryInterface(channel));
  if (cos) {
    cos->AddClassFlags(nsIClassOfService::TailForbidden);
  }

  channel.forget(aChannel);
  return NS_OK;
}

}  // namespace mozilla
