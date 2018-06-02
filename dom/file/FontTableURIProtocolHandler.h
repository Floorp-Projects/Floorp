/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef FontTableURIProtocolHandler_h
#define FontTableURIProtocolHandler_h

#include "nsIProtocolHandler.h"
#include "nsIURI.h"
#include "nsWeakReference.h"

#define FONTTABLEURI_SCHEME "moz-fonttable"

namespace mozilla {
namespace dom {

class FontTableURIProtocolHandler final : public nsIProtocolHandler
                                        , public nsIProtocolHandlerWithDynamicFlags
                                        , public nsSupportsWeakReference
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPROTOCOLHANDLER
  NS_DECL_NSIPROTOCOLHANDLERWITHDYNAMICFLAGS

  FontTableURIProtocolHandler();

  static nsresult GenerateURIString(nsACString &aUri);

protected:
  virtual ~FontTableURIProtocolHandler();
};

inline bool IsFontTableURI(nsIURI* aUri)
{
  bool isFont;
  return NS_SUCCEEDED(aUri->SchemeIs(FONTTABLEURI_SCHEME, &isFont)) && isFont;
}

} // namespace dom
} // namespace mozilla

#endif /* FontTableURIProtocolHandler_h */
