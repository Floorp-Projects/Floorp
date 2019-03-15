/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim:set ts=2 sts=2 sw=2 et cin:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIndexedDBProtocolHandler.h"

#include "nsIURIMutator.h"
#include "nsStandardURL.h"

using namespace mozilla::net;

nsIndexedDBProtocolHandler::nsIndexedDBProtocolHandler() {}

nsIndexedDBProtocolHandler::~nsIndexedDBProtocolHandler() {}

NS_IMPL_ISUPPORTS(nsIndexedDBProtocolHandler, nsIProtocolHandler,
                  nsISupportsWeakReference)

NS_IMETHODIMP nsIndexedDBProtocolHandler::GetScheme(nsACString& aScheme) {
  aScheme.AssignLiteral("indexeddb");
  return NS_OK;
}

NS_IMETHODIMP nsIndexedDBProtocolHandler::GetDefaultPort(
    int32_t* aDefaultPort) {
  *aDefaultPort = -1;
  return NS_OK;
}

NS_IMETHODIMP nsIndexedDBProtocolHandler::GetProtocolFlags(
    uint32_t* aProtocolFlags) {
  *aProtocolFlags = URI_STD | URI_DANGEROUS_TO_LOAD | URI_DOES_NOT_RETURN_DATA |
                    URI_NON_PERSISTABLE;
  return NS_OK;
}

NS_IMETHODIMP nsIndexedDBProtocolHandler::NewURI(const nsACString& aSpec,
                                                 const char* aOriginCharset,
                                                 nsIURI* aBaseURI,
                                                 nsIURI** _retval) {
  nsCOMPtr<nsIURI> baseURI(aBaseURI);
  return NS_MutateURI(new nsStandardURL::Mutator())
      .Apply(NS_MutatorMethod(
          &nsIStandardURLMutator::Init, nsIStandardURL::URLTYPE_AUTHORITY, 0,
          nsCString(aSpec), aOriginCharset, baseURI, nullptr))
      .Finalize(_retval);
}

NS_IMETHODIMP
nsIndexedDBProtocolHandler::NewChannel(nsIURI* aURI, nsILoadInfo* aLoadInfo,
                                       nsIChannel** _retval) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsIndexedDBProtocolHandler::AllowPort(int32_t aPort, const char* aScheme,
                                      bool* _retval) {
  *_retval = false;
  return NS_OK;
}
