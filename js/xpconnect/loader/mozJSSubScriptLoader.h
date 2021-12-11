/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMPtr.h"
#include "mozIJSSubScriptLoader.h"

#include "js/experimental/JSStencil.h"
#include "js/CompileOptions.h"  // JS::ReadOnlyCompileOptions

class nsIPrincipal;
class nsIURI;
class LoadSubScriptOptions;

#define MOZ_JSSUBSCRIPTLOADER_CID                    \
  { /* 829814d6-1dd2-11b2-8e08-82fa0a339b00 */       \
    0x929814d6, 0x1dd2, 0x11b2, {                    \
      0x8e, 0x08, 0x82, 0xfa, 0x0a, 0x33, 0x9b, 0x00 \
    }                                                \
  }

class nsIIOService;

class mozJSSubScriptLoader : public mozIJSSubScriptLoader {
 public:
  mozJSSubScriptLoader();

  // all the interface method declarations...
  NS_DECL_ISUPPORTS
  NS_DECL_MOZIJSSUBSCRIPTLOADER

 private:
  virtual ~mozJSSubScriptLoader();

  bool ReadStencil(JS::Stencil** stencilOut, nsIURI* uri, JSContext* cx,
                   const JS::ReadOnlyCompileOptions& options,
                   nsIIOService* serv, bool useCompilationScope);

  nsresult ReadScriptAsync(nsIURI* uri, JS::HandleObject targetObj,
                           JS::HandleObject loadScope, nsIIOService* serv,
                           bool wantReturnValue, bool cache,
                           JS::MutableHandleValue retval);

  nsresult DoLoadSubScriptWithOptions(const nsAString& url,
                                      LoadSubScriptOptions& options,
                                      JSContext* cx,
                                      JS::MutableHandleValue retval);
};
