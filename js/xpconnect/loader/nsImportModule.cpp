/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsImportModule.h"

#include "mozilla/dom/ScriptSettings.h"
#include "mozJSComponentLoader.h"
#include "nsContentUtils.h"
#include "nsExceptionHandler.h"
#include "nsPrintfCString.h"
#include "xpcpublic.h"
#include "xpcprivate.h"
#include "js/PropertyAndElement.h"  // JS_GetProperty

using mozilla::dom::AutoJSAPI;

namespace mozilla {
namespace loader {

static void AnnotateCrashReportWithJSException(JSContext* aCx,
                                               const char* aURI) {
  JS::RootedValue exn(aCx);
  if (JS_GetPendingException(aCx, &exn)) {
    JS_ClearPendingException(aCx);

    JSAutoRealm ar(aCx, xpc::PrivilegedJunkScope());
    JS_WrapValue(aCx, &exn);

    nsAutoCString file;
    uint32_t line;
    uint32_t column;
    nsAutoString msg;
    nsContentUtils::ExtractErrorValues(aCx, exn, file, &line, &column, msg);

    nsPrintfCString errorString("Failed to load module \"%s\": %s:%u:%u: %s",
                                aURI, file.get(), line, column,
                                NS_ConvertUTF16toUTF8(msg).get());

    CrashReporter::AnnotateCrashReport(
        CrashReporter::Annotation::JSModuleLoadError, errorString);
  }
}

nsresult ImportModule(const char* aURI, const char* aExportName,
                      const nsIID& aIID, void** aResult, bool aInfallible) {
  AutoJSAPI jsapi;
  MOZ_ALWAYS_TRUE(jsapi.Init(xpc::PrivilegedJunkScope()));
  JSContext* cx = jsapi.cx();

  JS::RootedObject global(cx);
  JS::RootedObject exports(cx);
  nsresult rv = mozJSComponentLoader::Get()->Import(
      cx, nsDependentCString(aURI), &global, &exports);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    if (aInfallible) {
      AnnotateCrashReportWithJSException(cx, aURI);

      MOZ_CRASH_UNSAFE_PRINTF("Failed to load critical module \"%s\"", aURI);
    }
    return rv;
  }

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
