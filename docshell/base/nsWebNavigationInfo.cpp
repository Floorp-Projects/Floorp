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
#include "nsIPluginHost.h"
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
  uint32_t result = IsTypeSupportedInternal(flatType);
  if (result != nsIWebNavigationInfo::UNSUPPORTED) {
    return result;
  }

  // As of FF 52, we only support flash and test plugins, so if the mime types
  // don't match for that, exit before we start loading plugins.
  if (!nsPluginHost::CanUsePluginForMIMEType(aType)) {
    return nsIWebNavigationInfo::UNSUPPORTED;
  }

  // If this request is for a docShell that isn't going to allow plugins,
  // there's no need to try and find a plugin to handle it.
  if (!aPluginsAllowed) {
    return nsIWebNavigationInfo::UNSUPPORTED;
  }

  // Try reloading plugins in case they've changed.
  nsCOMPtr<nsIPluginHost> pluginHost =
      do_GetService(MOZ_PLUGIN_HOST_CONTRACTID);
  if (pluginHost) {
    // false will ensure that currently running plugins will not
    // be shut down
    nsresult rv = pluginHost->ReloadPlugins();
    if (NS_SUCCEEDED(rv)) {
      // OK, we reloaded plugins and there were new ones
      // (otherwise NS_ERROR_PLUGINS_PLUGINSNOTCHANGED would have
      // been returned).  Try checking whether we can handle the
      // content now.
      return IsTypeSupportedInternal(flatType);
    }
  }

  return nsIWebNavigationInfo::UNSUPPORTED;
}

uint32_t nsWebNavigationInfo::IsTypeSupportedInternal(const nsCString& aType) {
  nsContentUtils::ContentViewerType vtype = nsContentUtils::TYPE_UNSUPPORTED;

  nsCOMPtr<nsIDocumentLoaderFactory> docLoaderFactory =
      nsContentUtils::FindInternalContentViewer(aType, &vtype);

  switch (vtype) {
    case nsContentUtils::TYPE_UNSUPPORTED:
      return nsIWebNavigationInfo::UNSUPPORTED;

    case nsContentUtils::TYPE_PLUGIN:
      return nsIWebNavigationInfo::PLUGIN;

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
