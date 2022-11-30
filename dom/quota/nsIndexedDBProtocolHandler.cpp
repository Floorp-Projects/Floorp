/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim:set ts=2 sts=2 sw=2 et cin:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIndexedDBProtocolHandler.h"

#include <cstdint>
#include "ErrorList.h"
#include "mozilla/Assertions.h"
#include "mozilla/MacroForEach.h"
#include "nsIWeakReference.h"
#include "nsStandardURL.h"
#include "nsStringFwd.h"
#include "nscore.h"

using namespace mozilla::net;

nsIndexedDBProtocolHandler::nsIndexedDBProtocolHandler() = default;

nsIndexedDBProtocolHandler::~nsIndexedDBProtocolHandler() = default;

NS_IMPL_ISUPPORTS(nsIndexedDBProtocolHandler, nsIProtocolHandler,
                  nsISupportsWeakReference)

NS_IMETHODIMP nsIndexedDBProtocolHandler::GetScheme(nsACString& aScheme) {
  aScheme.AssignLiteral("indexeddb");
  return NS_OK;
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
