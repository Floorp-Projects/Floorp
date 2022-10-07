/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef js_loader_ImportMap_h
#define js_loader_ImportMap_h

#include <functional>
#include <map>

#include "js/SourceText.h"
#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/Logging.h"
#include "mozilla/RefPtr.h"
#include "mozilla/UniquePtr.h"
#include "nsStringFwd.h"
#include "nsTArray.h"
#include "ResolveResult.h"

struct JSContext;
class nsIScriptElement;
class nsIURI;

namespace JS::loader {
class LoadedScript;
class ScriptLoaderInterface;
class ScriptLoadRequest;

/**
 * A helper class to report warning to ScriptLoaderInterface.
 */
class ReportWarningHelper {
 public:
  ReportWarningHelper(ScriptLoaderInterface* aLoader,
                      ScriptLoadRequest* aRequest)
      : mLoader(aLoader), mRequest(aRequest) {}

  void Report(const char* aMessageName,
              const nsTArray<nsString>& aParams = nsTArray<nsString>()) const;

 private:
  RefPtr<ScriptLoaderInterface> mLoader;
  ScriptLoadRequest* mRequest;
};

// Specifier map from import maps.
// https://html.spec.whatwg.org/multipage/webappapis.html#module-specifier-map
using SpecifierMap =
    std::map<nsString, nsCOMPtr<nsIURI>, std::greater<nsString>>;

// Scope map from import maps.
// https://html.spec.whatwg.org/multipage/webappapis.html#concept-import-map-scopes
using ScopeMap = std::map<nsCString, mozilla::UniquePtr<SpecifierMap>,
                          std::greater<nsCString>>;

/**
 * Implementation of Import maps.
 * https://html.spec.whatwg.org/multipage/webappapis.html#import-maps
 */
class ImportMap {
 public:
  ImportMap(mozilla::UniquePtr<SpecifierMap> aImports,
            mozilla::UniquePtr<ScopeMap> aScopes)
      : mImports(std::move(aImports)), mScopes(std::move(aScopes)) {}

  /**
   * Parse the JSON string from the Import map script.
   * This function will throw a TypeError if there's any invalid key or value in
   * the JSON text according to the spec.
   *
   * https://html.spec.whatwg.org/multipage/webappapis.html#parse-an-import-map-string
   */
  static mozilla::UniquePtr<ImportMap> ParseString(
      JSContext* aCx, JS::SourceText<char16_t>& aInput, nsIURI* aBaseURL,
      const ReportWarningHelper& aWarning);

  /**
   * This implements "Resolve a module specifier" algorithm defined in the
   * Import maps spec.
   *
   * See
   * https://html.spec.whatwg.org/multipage/webappapis.html#resolve-a-module-specifier
   *
   * Impl note: According to the spec, if the specifier cannot be resolved, this
   * method will throw a TypeError(Step 13). But the tricky part is when
   * creating a module script,
   * see
   * https://html.spec.whatwg.org/multipage/webappapis.html#validate-requested-module-specifiers
   * If the resolving failed, it shall catch the exception and set to the
   * script's parse error.
   * For implementation we return a ResolveResult here, and the callers will
   * need to convert the result to a TypeError if it fails.
   */
  static ResolveResult ResolveModuleSpecifier(ImportMap* aImportMap,
                                              ScriptLoaderInterface* aLoader,
                                              LoadedScript* aScript,
                                              const nsAString& aSpecifier);

  // Logging
  static mozilla::LazyLogModule gImportMapLog;

 private:
  /**
   * https://html.spec.whatwg.org/multipage/webappapis.html#import-map-processing-model
   *
   * Formally, an import map is a struct with two items:
   * 1. imports, a module specifier map, and
   * 2. scopes, an ordered map of URLs to module specifier maps.
   */
  mozilla::UniquePtr<SpecifierMap> mImports;
  mozilla::UniquePtr<ScopeMap> mScopes;
};

}  // namespace JS::loader

#endif  // js_loader_ImportMap_h
