/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=4 sw=2 sts=2 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*

  A protocol handler for ``chrome:''

*/

#include "nsChromeProtocolHandler.h"
#include "nsChromeRegistry.h"
#include "nsCOMPtr.h"
#include "nsContentUtils.h"
#include "nsThreadUtils.h"
#include "nsIChannel.h"
#include "nsIChromeRegistry.h"
#include "nsIFile.h"
#include "nsIFileChannel.h"
#include "nsIStandardURL.h"
#include "nsNetUtil.h"
#include "nsNetCID.h"
#include "nsIURL.h"
#include "nsString.h"
#include "nsStandardURL.h"

////////////////////////////////////////////////////////////////////////////////

NS_IMPL_ISUPPORTS(nsChromeProtocolHandler, nsIProtocolHandler,
                  nsISupportsWeakReference)

////////////////////////////////////////////////////////////////////////////////
// nsIProtocolHandler methods:

NS_IMETHODIMP
nsChromeProtocolHandler::GetScheme(nsACString& result) {
  result.AssignLiteral("chrome");
  return NS_OK;
}

NS_IMETHODIMP
nsChromeProtocolHandler::AllowPort(int32_t port, const char* scheme,
                                   bool* _retval) {
  // don't override anything.
  *_retval = false;
  return NS_OK;
}

/* static */ nsresult nsChromeProtocolHandler::CreateNewURI(
    const nsACString& aSpec, const char* aCharset, nsIURI* aBaseURI,
    nsIURI** result) {
  // Chrome: URLs (currently) have no additional structure beyond that provided
  // by standard URLs, so there is no "outer" given to CreateInstance
  nsresult rv;
  nsCOMPtr<nsIURI> surl;
  rv =
      NS_MutateURI(new mozilla::net::nsStandardURL::Mutator())
          .Apply(&nsIStandardURLMutator::Init, nsIStandardURL::URLTYPE_STANDARD,
                 -1, aSpec, aCharset, aBaseURI, nullptr)

          .Finalize(surl);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Canonify the "chrome:" URL; e.g., so that we collapse
  // "chrome://navigator/content/" and "chrome://navigator/content"
  // and "chrome://navigator/content/navigator.xul".

  rv = nsChromeRegistry::Canonify(surl);
  mozilla::Unused << NS_WARN_IF(NS_FAILED(rv));

  surl.forget(result);
  return NS_OK;
}

NS_IMETHODIMP
nsChromeProtocolHandler::NewChannel(nsIURI* aURI, nsILoadInfo* aLoadInfo,
                                    nsIChannel** aResult) {
  nsresult rv;

  NS_ENSURE_ARG_POINTER(aURI);
  NS_ENSURE_ARG_POINTER(aLoadInfo);

  MOZ_ASSERT(aResult, "Null out param");

  nsCOMPtr<nsIURI> debugURL = aURI;
  rv = nsChromeRegistry::Canonify(debugURL);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIChannel> result;

  if (!nsChromeRegistry::gChromeRegistry) {
    // We don't actually want this ref, we just want the service to
    // initialize if it hasn't already.
    nsCOMPtr<nsIChromeRegistry> reg = mozilla::services::GetChromeRegistry();
    NS_ENSURE_TRUE(nsChromeRegistry::gChromeRegistry, NS_ERROR_FAILURE);
  }

  nsCOMPtr<nsIURI> resolvedURI;
  rv = nsChromeRegistry::gChromeRegistry->ConvertChromeURL(
      aURI, getter_AddRefs(resolvedURI));
  if (NS_FAILED(rv)) {
#ifdef DEBUG
    printf("Couldn't convert chrome URL: %s\n", aURI->GetSpecOrDefault().get());
#endif
    return rv;
  }

  // We don't want to allow the inner protocol handler modify the result
  // principal URI since we want either |aURI| or anything pre-set by upper
  // layers to prevail.
  nsCOMPtr<nsIURI> savedResultPrincipalURI;
  rv =
      aLoadInfo->GetResultPrincipalURI(getter_AddRefs(savedResultPrincipalURI));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = NS_NewChannelInternal(getter_AddRefs(result), resolvedURI, aLoadInfo);
  NS_ENSURE_SUCCESS(rv, rv);

#ifdef DEBUG
  nsCOMPtr<nsIFileChannel> fileChan(do_QueryInterface(result));
  if (fileChan) {
    nsCOMPtr<nsIFile> file;
    fileChan->GetFile(getter_AddRefs(file));

    bool exists = false;
    file->Exists(&exists);
    if (!exists) {
      printf("Chrome file doesn't exist: %s\n",
             file->HumanReadablePath().get());
    }
  }
#endif

  // Make sure that the channel remembers where it was
  // originally loaded from.
  rv = aLoadInfo->SetResultPrincipalURI(savedResultPrincipalURI);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = result->SetOriginalURI(aURI);
  if (NS_FAILED(rv)) return rv;

  // Use a system principal for /content and /skin files.
  // See bug 1855225 for discussion about whether to extend it more generally
  // to other chrome:// URIs.
  nsAutoCString path;
  aURI->GetPathQueryRef(path);
  if (StringBeginsWith(path, "/content/"_ns) ||
      StringBeginsWith(path, "/skin/"_ns)) {
    result->SetOwner(nsContentUtils::GetSystemPrincipal());
  }

  // XXX Removed dependency-tracking code from here, because we're not
  // tracking them anyways (with fastload we checked only in DEBUG
  // and with startupcache not at all), but this is where we would start
  // if we need to re-add.
  // See bug 531886, bug 533038.
  result->SetContentCharset("UTF-8"_ns);

  result.forget(aResult);
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
