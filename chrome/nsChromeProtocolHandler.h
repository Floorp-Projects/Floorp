/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsChromeProtocolHandler_h___
#define nsChromeProtocolHandler_h___

#include "nsIProtocolHandler.h"
#include "nsWeakReference.h"
#include "mozilla/Attributes.h"

#define NS_CHROMEPROTOCOLHANDLER_CID                 \
  { /* 61ba33c0-3031-11d3-8cd0-0060b0fc14a3 */       \
    0x61ba33c0, 0x3031, 0x11d3, {                    \
      0x8c, 0xd0, 0x00, 0x60, 0xb0, 0xfc, 0x14, 0xa3 \
    }                                                \
  }

class nsChromeProtocolHandler final : public nsIProtocolHandler,
                                      public nsSupportsWeakReference {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS

  // nsIProtocolHandler methods:
  NS_DECL_NSIPROTOCOLHANDLER

  // nsChromeProtocolHandler methods:
  nsChromeProtocolHandler() {}
  static nsresult CreateNewURI(const nsACString& aSpec, const char* aCharset,
                               nsIURI* aBaseURI, nsIURI** result);

 private:
  ~nsChromeProtocolHandler() {}
};

#endif /* nsChromeProtocolHandler_h___ */
