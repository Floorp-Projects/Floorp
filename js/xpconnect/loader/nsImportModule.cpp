/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsImportModule.h"

#include "mozilla/ResultExtensions.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozJSComponentLoader.h"
#include "xpcpublic.h"
#include "xpcprivate.h"

using mozilla::dom::AutoJSAPI;

namespace mozilla {
namespace loader {

nsresult ImportModule(const char* aURI, const char* aExportName,
                      const nsIID& aIID, void** aResult) {
  AutoJSAPI jsapi;
  MOZ_ALWAYS_TRUE(jsapi.Init(xpc::PrivilegedJunkScope()));
  JSContext* cx = jsapi.cx();

  JS::RootedObject global(cx);
  JS::RootedObject exports(cx);
  MOZ_TRY(mozJSComponentLoader::Get()->Import(cx, nsDependentCString(aURI),
                                              &global, &exports));

  if (aExportName) {
    JS::RootedValue namedExport(cx);
    if (!JS_GetProperty(cx, exports, aExportName, &namedExport)) {
      return NS_ERROR_FAILURE;
    }
    if (!namedExport.isObject()) {
      return NS_ERROR_XPC_BAD_CONVERT_JS;
    }
    exports.set(&namedExport.toObject());
  }

  return nsXPConnect::XPConnect()->WrapJS(cx, exports, aIID, aResult);
}

}  // namespace loader
}  // namespace mozilla
