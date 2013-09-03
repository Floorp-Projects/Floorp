/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsWebNavigationInfo.h"
#include "nsIWebNavigation.h"
#include "nsServiceManagerUtils.h"
#include "nsIDocumentLoaderFactory.h"
#include "nsIPluginHost.h"
#include "nsIDocShell.h"
#include "nsContentUtils.h"
#include "imgLoader.h"

NS_IMPL_ISUPPORTS1(nsWebNavigationInfo, nsIWebNavigationInfo)

#define CONTENT_DLF_CONTRACT "@mozilla.org/content/document-loader-factory;1"
#define PLUGIN_DLF_CONTRACT \
    "@mozilla.org/content/plugin/document-loader-factory;1"

nsresult
nsWebNavigationInfo::Init()
{
  nsresult rv;
  mCategoryManager = do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
nsWebNavigationInfo::IsTypeSupported(const nsACString& aType,
                                     nsIWebNavigation* aWebNav,
                                     uint32_t* aIsTypeSupported)
{
  NS_PRECONDITION(aIsTypeSupported, "null out param?");

  // Note to self: aWebNav could be an nsWebBrowser or an nsDocShell here (or
  // an nsSHistory, but not much we can do with that).  So if we start using
  // it here, we need to be careful to get to the docshell correctly.
  
  // For now just report what the Gecko-Content-Viewers category has
  // to say for itself.
  *aIsTypeSupported = nsIWebNavigationInfo::UNSUPPORTED;

  const nsCString& flatType = PromiseFlatCString(aType);
  nsresult rv = IsTypeSupportedInternal(flatType, aIsTypeSupported);
  NS_ENSURE_SUCCESS(rv, rv);

  if (*aIsTypeSupported) {
    return rv;
  }

  // If this request is for a docShell that isn't going to allow plugins,
  // there's no need to try and find a plugin to handle it.
  nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(aWebNav));
  bool allowed;
  if (docShell && NS_SUCCEEDED(docShell->GetAllowPlugins(&allowed)) && !allowed) {
    return NS_OK;
  }

  // Try reloading plugins in case they've changed.
  nsCOMPtr<nsIPluginHost> pluginHost =
    do_GetService(MOZ_PLUGIN_HOST_CONTRACTID);
  if (pluginHost) {
    // false will ensure that currently running plugins will not
    // be shut down
    rv = pluginHost->ReloadPlugins();
    if (NS_SUCCEEDED(rv)) {
      // OK, we reloaded plugins and there were new ones
      // (otherwise NS_ERROR_PLUGINS_PLUGINSNOTCHANGED would have
      // been returned).  Try checking whether we can handle the
      // content now.
      return IsTypeSupportedInternal(flatType, aIsTypeSupported);
    }
  }

  return NS_OK;
}

nsresult
nsWebNavigationInfo::IsTypeSupportedInternal(const nsCString& aType,
                                             uint32_t* aIsSupported)
{
  NS_PRECONDITION(aIsSupported, "Null out param?");


  nsContentUtils::ContentViewerType vtype = nsContentUtils::TYPE_UNSUPPORTED;

  nsCOMPtr<nsIDocumentLoaderFactory> docLoaderFactory =
    nsContentUtils::FindInternalContentViewer(aType.get(), &vtype);

  switch (vtype) {
  case nsContentUtils::TYPE_UNSUPPORTED:
    *aIsSupported = nsIWebNavigationInfo::UNSUPPORTED;
    break;

  case nsContentUtils::TYPE_PLUGIN:
    *aIsSupported = nsIWebNavigationInfo::PLUGIN;
    break;

  case nsContentUtils::TYPE_UNKNOWN:
    *aIsSupported = nsIWebNavigationInfo::OTHER;
    break;

  case nsContentUtils::TYPE_CONTENT:
    // XXXbz we only need this because images register for the same
    // contractid as documents, so we can't tell them apart based on
    // contractid.
    if (imgLoader::SupportImageWithMimeType(aType.get())) {
      *aIsSupported = nsIWebNavigationInfo::IMAGE;
    }
    else {
      *aIsSupported = nsIWebNavigationInfo::OTHER;
    }
    break;
  }

  return NS_OK;
}
