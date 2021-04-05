/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsWebNavigationInfo.h"

#include "mozilla/dom/BrowsingContext.h"
#include "nsIWebNavigation.h"
#include "nsServiceManagerUtils.h"
#include "nsIDocumentLoaderFactory.h"
#include "nsIDocShell.h"
#include "nsContentUtils.h"
#include "imgLoader.h"
#include "nsPluginHost.h"

NS_IMPL_ISUPPORTS(nsWebNavigationInfo, nsIWebNavigationInfo)

NS_IMETHODIMP
nsWebNavigationInfo::IsTypeSupported(const nsACString& aType,
                                     nsIWebNavigation* aWebNav,
                                     uint32_t* aIsTypeSupported) {
  MOZ_ASSERT(aIsTypeSupported, "null out param?");

  *aIsTypeSupported = IsTypeSupported(aType, aWebNav);
  return NS_OK;
}

uint32_t nsWebNavigationInfo::IsTypeSupported(const nsACString& aType,
                                              nsIWebNavigation* aWebNav) {
  // Note to self: aWebNav could be an nsWebBrowser or an nsDocShell here (or
  // an nsSHistory, but not much we can do with that).  So if we start using
  // it here, we need to be careful to get to the docshell correctly.
  nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(aWebNav));
  auto* browsingContext = docShell ? docShell->GetBrowsingContext() : nullptr;
  bool pluginsAllowed =
      browsingContext ? browsingContext->GetAllowPlugins() : true;

  return IsTypeSupported(aType, pluginsAllowed);
}

uint32_t nsWebNavigationInfo::IsTypeSupported(const nsACString& aType,
                                              bool aPluginsAllowed) {
  // We want to claim that the type for PDF documents is unsupported,
  // so that the internal PDF viewer's stream converted will get used.
  if (aType.LowerCaseEqualsLiteral("application/pdf") &&
      nsContentUtils::IsPDFJSEnabled()) {
    return nsIWebNavigationInfo::UNSUPPORTED;
    ;
  }

  const nsCString& flatType = PromiseFlatCString(aType);
  return IsTypeSupportedInternal(flatType);
}

uint32_t nsWebNavigationInfo::IsTypeSupportedInternal(const nsCString& aType) {
  nsContentUtils::ContentViewerType vtype = nsContentUtils::TYPE_UNSUPPORTED;

  nsCOMPtr<nsIDocumentLoaderFactory> docLoaderFactory =
      nsContentUtils::FindInternalContentViewer(aType, &vtype);

  switch (vtype) {
    case nsContentUtils::TYPE_UNSUPPORTED:
      return nsIWebNavigationInfo::UNSUPPORTED;

    case nsContentUtils::TYPE_FALLBACK:
      return nsIWebNavigationInfo::FALLBACK;

    case nsContentUtils::TYPE_UNKNOWN:
      return nsIWebNavigationInfo::OTHER;

    case nsContentUtils::TYPE_CONTENT:
      // XXXbz we only need this because images register for the same
      // contractid as documents, so we can't tell them apart based on
      // contractid.
      if (imgLoader::SupportImageWithMimeType(aType.get())) {
        return nsIWebNavigationInfo::IMAGE;
      } else {
        return nsIWebNavigationInfo::OTHER;
      }
  }

  return nsIWebNavigationInfo::UNSUPPORTED;
}
