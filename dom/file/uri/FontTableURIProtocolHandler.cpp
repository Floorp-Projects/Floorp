/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FontTableURIProtocolHandler.h"
#include "nsIDUtils.h"
#include "nsIUUIDGenerator.h"
#include "nsNetUtil.h"
#include "nsSimpleURI.h"

using namespace mozilla;
using namespace mozilla::dom;

/* static */
nsresult FontTableURIProtocolHandler::GenerateURIString(nsACString& aUri) {
  nsresult rv;
  nsCOMPtr<nsIUUIDGenerator> uuidgen =
      do_GetService("@mozilla.org/uuid-generator;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsID id;
  rv = uuidgen->GenerateUUIDInPlace(&id);
  NS_ENSURE_SUCCESS(rv, rv);

  aUri = FONTTABLEURI_SCHEME;
  aUri.Append(':');

  aUri += NSID_TrimBracketsASCII(id);

  return NS_OK;
}

FontTableURIProtocolHandler::FontTableURIProtocolHandler() = default;
FontTableURIProtocolHandler::~FontTableURIProtocolHandler() = default;

NS_IMPL_ISUPPORTS(FontTableURIProtocolHandler, nsIProtocolHandler,
                  nsISupportsWeakReference)

NS_IMETHODIMP
FontTableURIProtocolHandler::GetDefaultPort(int32_t* result) {
  *result = -1;
  return NS_OK;
}

NS_IMETHODIMP
FontTableURIProtocolHandler::GetProtocolFlags(uint32_t* result) {
  *result = URI_NORELATIVE | URI_NOAUTH | URI_LOADABLE_BY_SUBSUMERS |
            URI_NON_PERSISTABLE | URI_IS_LOCAL_RESOURCE;
  return NS_OK;
}

NS_IMETHODIMP
FontTableURIProtocolHandler::GetFlagsForURI(nsIURI* aURI, uint32_t* aResult) {
  return FontTableURIProtocolHandler::GetProtocolFlags(aResult);
}

NS_IMETHODIMP
FontTableURIProtocolHandler::NewChannel(nsIURI* uri, nsILoadInfo* aLoadInfo,
                                        nsIChannel** result) {
  return NS_ERROR_DOM_BAD_URI;
}

NS_IMETHODIMP
FontTableURIProtocolHandler::AllowPort(int32_t port, const char* scheme,
                                       bool* _retval) {
  *_retval = false;
  return NS_OK;
}

NS_IMETHODIMP
FontTableURIProtocolHandler::GetScheme(nsACString& result) {
  result.AssignLiteral(FONTTABLEURI_SCHEME);
  return NS_OK;
}
