/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FontPreloader.h"

#include "gfxUserFontSet.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "nsContentSecurityManager.h"
#include "nsIClassOfService.h"
#include "nsIHttpChannel.h"
#include "nsISupportsPriority.h"
#include "nsNetUtil.h"

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

/* static */ void FontPreloader::BuildChannelFlags(
    nsIURI* aURI, bool aIsPreload,
    nsContentSecurityManager::CORSSecurityMapping& aCorsMapping,
    nsSecurityFlags& aSecurityFlags, nsContentPolicyType& aContentPolicyType) {
  // aCORSMode is ignored.  We always load as crossorigin=anonymous, but a
  // preload started with anything other then "anonymous" will never be found.
  aCorsMapping =
      aURI->SchemeIs("file")
          ? nsContentSecurityManager::CORSSecurityMapping::
                CORS_NONE_MAPS_TO_INHERITED_CONTEXT
          : nsContentSecurityManager::CORSSecurityMapping::REQUIRE_CORS_CHECKS;

  aSecurityFlags = nsContentSecurityManager::ComputeSecurityFlags(
      CORSMode::CORS_NONE, aCorsMapping);

  aContentPolicyType = aIsPreload ? nsIContentPolicy::TYPE_INTERNAL_FONT_PRELOAD
                                  : nsIContentPolicy::TYPE_FONT;
}

/* static */ nsresult FontPreloader::BuildChannelSetup(
    nsIChannel* aChannel, nsIHttpChannel* aHttpChannel,
    nsIReferrerInfo* aReferrerInfo, const gfxFontFaceSrc* aFontFaceSrc) {
  if (aHttpChannel) {
    nsresult rv = aHttpChannel->SetRequestHeader(
        "Accept"_ns,
        "application/font-woff2;q=1.0,application/font-woff;q=0.9,*/*;q=0.8"_ns,
        false);
    NS_ENSURE_SUCCESS(rv, rv);

    if (aReferrerInfo) {
      rv = aHttpChannel->SetReferrerInfoWithoutClone(aReferrerInfo);
      MOZ_ASSERT(NS_SUCCEEDED(rv));
    } else {
      MOZ_ASSERT(aFontFaceSrc);

      rv = aHttpChannel->SetReferrerInfo(aFontFaceSrc->mReferrerInfo);
      Unused << NS_WARN_IF(NS_FAILED(rv));

      // For WOFF and WOFF2, we should tell servers/proxies/etc NOT to try
      // and apply additional compression at the content-encoding layer
      if (aFontFaceSrc->mFormatHint == StyleFontFaceSourceFormatKeyword::Woff ||
          aFontFaceSrc->mFormatHint ==
              StyleFontFaceSourceFormatKeyword::Woff2) {
        rv = aHttpChannel->SetRequestHeader("Accept-Encoding"_ns, "identity"_ns,
                                            false);
        NS_ENSURE_SUCCESS(rv, rv);
      }
    }
  }

  nsCOMPtr<nsISupportsPriority> priorityChannel(do_QueryInterface(aChannel));
  if (priorityChannel) {
    priorityChannel->AdjustPriority(nsISupportsPriority::PRIORITY_HIGH);
  }
  nsCOMPtr<nsIClassOfService> cos(do_QueryInterface(aChannel));
  if (cos) {
    cos->AddClassFlags(nsIClassOfService::TailForbidden);
  }

  return NS_OK;
}

// static
nsresult FontPreloader::BuildChannel(
    nsIChannel** aChannel, nsIURI* aURI, const CORSMode aCORSMode,
    const dom::ReferrerPolicy& aReferrerPolicy,
    gfxUserFontEntry* aUserFontEntry, const gfxFontFaceSrc* aFontFaceSrc,
    dom::Document* aDocument, nsILoadGroup* aLoadGroup,
    nsIInterfaceRequestor* aCallbacks, bool aIsPreload) {
  nsresult rv;

  nsIPrincipal* principal =
      aUserFontEntry ? (aUserFontEntry->GetPrincipal()
                            ? aUserFontEntry->GetPrincipal()->NodePrincipal()
                            : nullptr)
                     : aDocument->NodePrincipal();

  nsContentSecurityManager::CORSSecurityMapping corsMapping;
  nsSecurityFlags securityFlags;
  nsContentPolicyType contentPolicyType;
  BuildChannelFlags(aURI, aIsPreload, corsMapping, securityFlags,
                    contentPolicyType);

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
  nsCOMPtr<nsIReferrerInfo> referrerInfo;
  if (httpChannel && !aFontFaceSrc) {
    referrerInfo = new dom::ReferrerInfo(aDocument->GetDocumentURIAsReferrer(),
                                         aReferrerPolicy);
    rv = httpChannel->SetReferrerInfoWithoutClone(referrerInfo);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
  }

  rv = BuildChannelSetup(channel, httpChannel, referrerInfo, aFontFaceSrc);
  NS_ENSURE_SUCCESS(rv, rv);

  channel.forget(aChannel);
  return NS_OK;
}

// static
nsresult FontPreloader::BuildChannel(
    nsIChannel** aChannel, nsIURI* aURI, const CORSMode aCORSMode,
    const dom::ReferrerPolicy& aReferrerPolicy,
    gfxUserFontEntry* aUserFontEntry, const gfxFontFaceSrc* aFontFaceSrc,
    dom::WorkerPrivate* aWorkerPrivate, nsILoadGroup* aLoadGroup,
    nsIInterfaceRequestor* aCallbacks, bool aIsPreload) {
  nsresult rv;

  nsIPrincipal* principal =
      aUserFontEntry ? (aUserFontEntry->GetPrincipal()
                            ? aUserFontEntry->GetPrincipal()->NodePrincipal()
                            : nullptr)
                     : aWorkerPrivate->GetPrincipal();

  nsContentSecurityManager::CORSSecurityMapping corsMapping;
  nsSecurityFlags securityFlags;
  nsContentPolicyType contentPolicyType;
  BuildChannelFlags(aURI, aIsPreload, corsMapping, securityFlags,
                    contentPolicyType);

  nsCOMPtr<nsIChannel> channel;
  rv = NS_NewChannelWithTriggeringPrincipal(
      getter_AddRefs(channel), aURI, aWorkerPrivate->GetLoadingPrincipal(),
      principal, securityFlags, contentPolicyType, nullptr, nullptr,
      aLoadGroup);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(channel));

  nsCOMPtr<nsIReferrerInfo> referrerInfo;
  if (httpChannel && !aFontFaceSrc) {
    referrerInfo =
        static_cast<dom::ReferrerInfo*>(aWorkerPrivate->GetReferrerInfo())
            ->CloneWithNewPolicy(aReferrerPolicy);
  }

  rv = BuildChannelSetup(channel, httpChannel, referrerInfo, aFontFaceSrc);
  NS_ENSURE_SUCCESS(rv, rv);

  channel.forget(aChannel);
  return NS_OK;
}

}  // namespace mozilla
