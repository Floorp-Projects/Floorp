/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ImportMap.h"

#include "js/Array.h"                 // IsArrayObject
#include "js/friend/ErrorMessages.h"  // js::GetErrorMessage, JSMSG_*
#include "js/JSON.h"                  // JS_ParseJSON
#include "LoadedScript.h"
#include "ModuleLoaderBase.h"  // ScriptLoaderInterface
#include "nsContentUtils.h"
#include "nsIScriptElement.h"
#include "nsIScriptError.h"
#include "nsJSUtils.h"  // nsAutoJSString
#include "nsNetUtil.h"  // NS_NewURI
#include "ScriptLoadRequest.h"

using JS::SourceText;
using mozilla::Err;
using mozilla::LazyLogModule;
using mozilla::MakeUnique;
using mozilla::UniquePtr;
using mozilla::WrapNotNull;

namespace JS::loader {

LazyLogModule ImportMap::gImportMapLog("ImportMap");

#undef LOG
#define LOG(args) \
  MOZ_LOG(ImportMap::gImportMapLog, mozilla::LogLevel::Debug, args)

#define LOG_ENABLED() \
  MOZ_LOG_TEST(ImportMap::gImportMapLog, mozilla::LogLevel::Debug)

void ReportWarningHelper::Report(const char* aMessageName,
                                 const nsTArray<nsString>& aParams) const {
  mLoader->ReportWarningToConsole(mRequest, aMessageName, aParams);
}

// https://wicg.github.io/import-maps/#parse-a-url-like-import-specifier
static ResolveResult ParseURLLikeImportSpecifier(const nsAString& aSpecifier,
                                                 nsIURI* aBaseURL) {
  nsCOMPtr<nsIURI> uri;
  nsresult rv;

  // Step 1. If specifier starts with "/", "./", or "../", then:
  if (StringBeginsWith(aSpecifier, u"/"_ns) ||
      StringBeginsWith(aSpecifier, u"./"_ns) ||
      StringBeginsWith(aSpecifier, u"../"_ns)) {
    // Step 1.1. Let url be the result of parsing specifier with baseURL as the
    // base URL.
    rv = NS_NewURI(getter_AddRefs(uri), aSpecifier, nullptr, aBaseURL);
    // Step 1.2. If url is failure, then return null.
    if (NS_FAILED(rv)) {
      return Err(ResolveError::Failure);
    }

    // Step 1.3. Return url.
    return WrapNotNull(uri);
  }

  // Step 2. Let url be the result of parsing specifier (with no base URL).
  rv = NS_NewURI(getter_AddRefs(uri), aSpecifier);
  // Step 3. If url is failure, then return null.
  if (NS_FAILED(rv)) {
    return Err(ResolveError::FailureMayBeBare);
  }

  // Step 4. Return url.
  return WrapNotNull(uri);
}

// https://wicg.github.io/import-maps/#normalize-a-specifier-key
static void NormalizeSpecifierKey(const nsAString& aSpecifierKey,
                                  nsIURI* aBaseURL,
                                  const ReportWarningHelper& aWarning,
                                  nsAString& aRetVal) {
  // Step 1. If specifierKey is the empty string, then:
  if (aSpecifierKey.IsEmpty()) {
    // Step 1.1. Report a warning to the console that specifier keys cannot be
    // the empty string.
    aWarning.Report("ImportMapEmptySpecifierKeys");

    // Step 1.2. Return null.
    aRetVal = EmptyString();
    return;
  }

  // Step 2. Let url be the result of parsing a URL-like import specifier, given
  // specifierKey and baseURL.
  auto parseResult = ParseURLLikeImportSpecifier(aSpecifierKey, aBaseURL);

  // Step 3. If url is not null, then return the serialization of url.
  if (parseResult.isOk()) {
    nsCOMPtr<nsIURI> url = parseResult.unwrap();
    aRetVal = NS_ConvertUTF8toUTF16(url->GetSpecOrDefault());
    return;
  }

  // Step 4. Return specifierKey.
  aRetVal = aSpecifierKey;
}

// https://wicg.github.io/import-maps/#sort-and-normalize-a-specifier-map
static UniquePtr<SpecifierMap> SortAndNormalizeSpecifierMap(
    JSContext* aCx, JS::HandleObject aOriginalMap, nsIURI* aBaseURL,
    const ReportWarningHelper& aWarning) {
  // Step 1. Let normalized be an empty map.
  UniquePtr<SpecifierMap> normalized = MakeUnique<SpecifierMap>();

  JS::Rooted<JS::IdVector> specifierKeys(aCx, JS::IdVector(aCx));
  if (!JS_Enumerate(aCx, aOriginalMap, &specifierKeys)) {
    return nullptr;
  }

  // Step 2. For each specifierKey → value of originalMap,
  for (size_t i = 0; i < specifierKeys.length(); i++) {
    const JS::RootedId specifierId(aCx, specifierKeys[i]);
    nsAutoJSString specifierKey;
    NS_ENSURE_TRUE(specifierKey.init(aCx, specifierId), nullptr);

    // Step 2.1. Let normalizedSpecifierKey be the result of normalizing a
    // specifier key given specifierKey and baseURL.
    nsString normalizedSpecifierKey;
    NormalizeSpecifierKey(specifierKey, aBaseURL, aWarning,
                          normalizedSpecifierKey);

    // Step 2.2. If normalizedSpecifierKey is null, then continue.
    if (normalizedSpecifierKey.IsEmpty()) {
      continue;
    }

    JS::RootedValue idVal(aCx);
    NS_ENSURE_TRUE(JS_GetPropertyById(aCx, aOriginalMap, specifierId, &idVal),
                   nullptr);
    // Step 2.3. If value is not a string, then:
    if (!idVal.isString()) {
      // Step 2.3.1. Report a warning to the console that addresses need to
      // be strings.
      aWarning.Report("ImportMapAddressesNotStrings");

      // Step 2.3.2. Set normalized[normalizedSpecifierKey] to null.
      normalized->insert_or_assign(normalizedSpecifierKey, nullptr);

      // Step 2.3.3. Continue.
      continue;
    }

    nsAutoJSString value;
    NS_ENSURE_TRUE(value.init(aCx, idVal), nullptr);

    // Step 2.4. Let addressURL be the result of parsing a URL-like import
    // specifier given value and baseURL.
    auto parseResult = ParseURLLikeImportSpecifier(value, aBaseURL);

    // Step 2.5. If addressURL is null, then:
    if (parseResult.isErr()) {
      // Step 2.5.1. Report a warning to the console that the address was
      // invalid.
      AutoTArray<nsString, 1> params;
      params.AppendElement(value);
      aWarning.Report("ImportMapInvalidAddress", params);

      // Step 2.5.2. Set normalized[normalizedSpecifierKey] to null.
      normalized->insert_or_assign(normalizedSpecifierKey, nullptr);

      // Step 2.5.3. Continue.
      continue;
    }

    nsCOMPtr<nsIURI> addressURL = parseResult.unwrap();
    nsCString address = addressURL->GetSpecOrDefault();
    // Step 2.6. If specifierKey ends with U+002F (/), and the serialization
    // of addressURL does not end with U+002F (/), then:
    if (StringEndsWith(specifierKey, u"/"_ns) &&
        !StringEndsWith(address, "/"_ns)) {
      // Step 2.6.1. Report a warning to the console that an invalid address
      // was given for the specifier key specifierKey; since specifierKey
      // ended in a slash, the address needs to as well.
      AutoTArray<nsString, 2> params;
      params.AppendElement(specifierKey);
      params.AppendElement(NS_ConvertUTF8toUTF16(address));
      aWarning.Report("ImportMapAddressNotEndsWithSlash", params);

      // Step 2.6.2. Set normalized[normalizedSpecifierKey] to null.
      normalized->insert_or_assign(normalizedSpecifierKey, nullptr);

      // Step 2.6.3. Continue.
      continue;
    }

    LOG(("ImportMap::SortAndNormalizeSpecifierMap {%s, %s}",
         NS_ConvertUTF16toUTF8(normalizedSpecifierKey).get(),
         addressURL->GetSpecOrDefault().get()));

    // Step 2.7. Set normalized[normalizedSpecifierKey] to addressURL.
    normalized->insert_or_assign(normalizedSpecifierKey, addressURL);
  }

  // Step 3: Return the result of sorting normalized, with an entry a being
  // less than an entry b if b’s key is code unit less than a’s key.
  //
  // Impl note: The sorting is done when inserting the entry.
  return normalized;
}

// Check if it's a map defined in
// https://infra.spec.whatwg.org/#ordered-map
//
// If it is, *aIsMap will be set to true.
static bool IsMapObject(JSContext* aCx, JS::HandleValue aMapVal, bool* aIsMap) {
  MOZ_ASSERT(aIsMap);

  *aIsMap = false;
  if (!aMapVal.isObject()) {
    return true;
  }

  bool isArray;
  if (!IsArrayObject(aCx, aMapVal, &isArray)) {
    return false;
  }

  *aIsMap = !isArray;
  return true;
}

// https://wicg.github.io/import-maps/#sort-and-normalize-scopes
static UniquePtr<ScopeMap> SortAndNormalizeScopes(
    JSContext* aCx, JS::HandleObject aOriginalMap, nsIURI* aBaseURL,
    const ReportWarningHelper& aWarning) {
  JS::Rooted<JS::IdVector> scopeKeys(aCx, JS::IdVector(aCx));
  if (!JS_Enumerate(aCx, aOriginalMap, &scopeKeys)) {
    return nullptr;
  }

  // Step 1. Let normalized be an empty map.
  UniquePtr<ScopeMap> normalized = MakeUnique<ScopeMap>();

  // Step 2. For each scopePrefix → potentialSpecifierMap of originalMap,
  for (size_t i = 0; i < scopeKeys.length(); i++) {
    const JS::RootedId scopeKey(aCx, scopeKeys[i]);
    nsAutoJSString scopePrefix;
    NS_ENSURE_TRUE(scopePrefix.init(aCx, scopeKey), nullptr);

    // Step 2.1. If potentialSpecifierMap is not a map, then throw a TypeError
    // indicating that the value of the scope with prefix scopePrefix needs to
    // be a JSON object.
    JS::RootedValue mapVal(aCx);
    NS_ENSURE_TRUE(JS_GetPropertyById(aCx, aOriginalMap, scopeKey, &mapVal),
                   nullptr);

    bool isMap;
    if (!IsMapObject(aCx, mapVal, &isMap)) {
      return nullptr;
    }
    if (!isMap) {
      const char16_t* scope = scopePrefix.get();
      JS_ReportErrorNumberUC(aCx, js::GetErrorMessage, nullptr,
                             JSMSG_IMPORT_MAPS_SCOPE_VALUE_NOT_A_MAP, scope);
      return nullptr;
    }

    // Step 2.2. Let scopePrefixURL be the result of parsing scopePrefix with
    // baseURL as the base URL.
    nsCOMPtr<nsIURI> scopePrefixURL;
    nsresult rv = NS_NewURI(getter_AddRefs(scopePrefixURL), scopePrefix,
                            nullptr, aBaseURL);

    // Step 2.3. If scopePrefixURL is failure, then:
    if (NS_FAILED(rv)) {
      // Step 2.3.1. Report a warning to the console that the scope prefix URL
      // was not parseable.
      AutoTArray<nsString, 1> params;
      params.AppendElement(scopePrefix);
      aWarning.Report("ImportMapScopePrefixNotParseable", params);

      // Step 2.3.2. Continue.
      continue;
    }

    // Step 2.4. Let normalizedScopePrefix be the serialization of
    // scopePrefixURL.
    nsCString normalizedScopePrefix = scopePrefixURL->GetSpecOrDefault();

    // Step 2.5. Set normalized[normalizedScopePrefix] to the result of sorting
    // and normalizing a specifier map given potentialSpecifierMap and baseURL.
    JS::RootedObject potentialSpecifierMap(aCx, &mapVal.toObject());
    UniquePtr<SpecifierMap> specifierMap = SortAndNormalizeSpecifierMap(
        aCx, potentialSpecifierMap, aBaseURL, aWarning);
    if (!specifierMap) {
      return nullptr;
    }

    normalized->insert_or_assign(normalizedScopePrefix,
                                 std::move(specifierMap));
  }

  // Step 3. Return the result of sorting normalized, with an entry a being less
  // than an entry b if b’s key is code unit less than a’s key.
  //
  // Impl note: The sorting is done when inserting the entry.
  return normalized;
}

// https://wicg.github.io/import-maps/#parse-an-import-map-string
// static
UniquePtr<ImportMap> ImportMap::ParseString(
    JSContext* aCx, SourceText<char16_t>& aInput, nsIURI* aBaseURL,
    const ReportWarningHelper& aWarning) {
  // Step 1. Let parsed be the result of parsing JSON into Infra values given
  // input.
  JS::Rooted<JS::Value> parsedVal(aCx);
  if (!JS_ParseJSON(aCx, aInput.get(), aInput.length(), &parsedVal)) {
    // If JS_ParseJSON fail it will throw SyntaxError.
    NS_WARNING("Parsing Import map string failed");
    return nullptr;
  }

  // Step 2. If parsed is not a map, then throw a TypeError indicating that
  // the top-level value needs to be a JSON object.
  bool isMap;
  if (!IsMapObject(aCx, parsedVal, &isMap)) {
    return nullptr;
  }
  if (!isMap) {
    JS_ReportErrorNumberASCII(aCx, js::GetErrorMessage, nullptr,
                              JSMSG_IMPORT_MAPS_NOT_A_MAP);
    return nullptr;
  }

  JS::RootedObject parsedObj(aCx, &parsedVal.toObject());
  JS::RootedValue importsVal(aCx);
  if (!JS_GetProperty(aCx, parsedObj, "imports", &importsVal)) {
    return nullptr;
  }

  // Step 3. Let sortedAndNormalizedImports be an empty map.
  //
  // Impl note: If parsed["imports"] doesn't exist, we will allocate
  // sortedAndNormalizedImports to an empty map in Step 8 below.
  UniquePtr<SpecifierMap> sortedAndNormalizedImports = nullptr;

  // Step 4. If parsed["imports"] exists, then:
  if (!importsVal.isUndefined()) {
    // Step 4.1. If parsed["imports"] is not a map, then throw a TypeError
    // indicating that the "imports" top-level key needs to be a JSON object.
    bool isMap;
    if (!IsMapObject(aCx, importsVal, &isMap)) {
      return nullptr;
    }
    if (!isMap) {
      JS_ReportErrorNumberASCII(aCx, js::GetErrorMessage, nullptr,
                                JSMSG_IMPORT_MAPS_IMPORTS_NOT_A_MAP);
      return nullptr;
    }

    // Step 4.2. Set sortedAndNormalizedImports to the result of sorting and
    // normalizing a specifier map given parsed["imports"] and baseURL.
    JS::RootedObject importsObj(aCx, &importsVal.toObject());
    sortedAndNormalizedImports =
        SortAndNormalizeSpecifierMap(aCx, importsObj, aBaseURL, aWarning);
    if (!sortedAndNormalizedImports) {
      return nullptr;
    }
  }

  JS::RootedValue scopesVal(aCx);
  if (!JS_GetProperty(aCx, parsedObj, "scopes", &scopesVal)) {
    return nullptr;
  }

  // Step 5. Let sortedAndNormalizedScopes be an empty map.
  //
  // Impl note: If parsed["scopes"] doesn't exist, we will allocate
  // sortedAndNormalizedScopes to an empty map in Step 8 below.
  UniquePtr<ScopeMap> sortedAndNormalizedScopes = nullptr;

  // Step 6. If parsed["scopes"] exists, then:
  if (!scopesVal.isUndefined()) {
    // Step 6.1. If parsed["scopes"] is not a map, then throw a TypeError
    // indicating that the "scopes" top-level key needs to be a JSON object.
    bool isMap;
    if (!IsMapObject(aCx, scopesVal, &isMap)) {
      return nullptr;
    }
    if (!isMap) {
      JS_ReportErrorNumberASCII(aCx, js::GetErrorMessage, nullptr,
                                JSMSG_IMPORT_MAPS_SCOPES_NOT_A_MAP);
      return nullptr;
    }

    // Step 6.2. Set sortedAndNormalizedScopes to the result of sorting and
    // normalizing scopes given parsed["scopes"] and baseURL.
    JS::RootedObject scopesObj(aCx, &scopesVal.toObject());
    sortedAndNormalizedScopes =
        SortAndNormalizeScopes(aCx, scopesObj, aBaseURL, aWarning);
    if (!sortedAndNormalizedScopes) {
      return nullptr;
    }
  }

  // Step 7. If parsed’s keys contains any items besides "imports" or
  // "scopes", report a warning to the console that an invalid top-level key
  // was present in the import map.
  JS::Rooted<JS::IdVector> keys(aCx, JS::IdVector(aCx));
  if (!JS_Enumerate(aCx, parsedObj, &keys)) {
    return nullptr;
  }

  for (size_t i = 0; i < keys.length(); i++) {
    const JS::RootedId key(aCx, keys[i]);
    nsAutoJSString val;
    NS_ENSURE_TRUE(val.init(aCx, key), nullptr);
    if (val.EqualsLiteral("imports") || val.EqualsLiteral("scopes")) {
      continue;
    }

    AutoTArray<nsString, 1> params;
    params.AppendElement(val);
    aWarning.Report("ImportMapInvalidTopLevelKey", params);
  }

  // Impl note: Create empty maps for sortedAndNormalizedImports and
  // sortedAndNormalizedImports if they aren't allocated.
  if (!sortedAndNormalizedImports) {
    sortedAndNormalizedImports = MakeUnique<SpecifierMap>();
  }
  if (!sortedAndNormalizedScopes) {
    sortedAndNormalizedScopes = MakeUnique<ScopeMap>();
  }

  // Step 8. Return the import map whose imports are
  // sortedAndNormalizedImports and whose scopes scopes are
  // sortedAndNormalizedScopes.
  return MakeUnique<ImportMap>(std::move(sortedAndNormalizedImports),
                               std::move(sortedAndNormalizedScopes));
}

// https://url.spec.whatwg.org/#is-special
static bool IsSpecialScheme(nsIURI* aURI) {
  nsAutoCString scheme;
  aURI->GetScheme(scheme);
  return scheme.EqualsLiteral("ftp") || scheme.EqualsLiteral("file") ||
         scheme.EqualsLiteral("http") || scheme.EqualsLiteral("https") ||
         scheme.EqualsLiteral("ws") || scheme.EqualsLiteral("wss");
}

// https://wicg.github.io/import-maps/#resolve-an-imports-match
static mozilla::Result<nsCOMPtr<nsIURI>, ResolveError> ResolveImportsMatch(
    nsString& aNormalizedSpecifier, nsIURI* aAsURL,
    const SpecifierMap* aSpecifierMap) {
  // Step 1. For each specifierKey → resolutionResult of specifierMap,
  for (auto&& [specifierKey, resolutionResult] : *aSpecifierMap) {
    nsAutoString specifier{aNormalizedSpecifier};
    nsCString asURL = aAsURL ? aAsURL->GetSpecOrDefault() : EmptyCString();

    // Step 1.1. If specifierKey is normalizedSpecifier, then:
    if (specifierKey.Equals(aNormalizedSpecifier)) {
      // Step 1.1.1. If resolutionResult is null, then throw a TypeError
      // indicating that resolution of specifierKey was blocked by a null entry.
      // This will terminate the entire resolve a module specifier algorithm,
      // without any further fallbacks.
      if (!resolutionResult) {
        LOG(
            ("ImportMap::ResolveImportsMatch normalizedSpecifier: %s, "
             "specifierKey: %s, but resolution is null.",
             NS_ConvertUTF16toUTF8(aNormalizedSpecifier).get(),
             NS_ConvertUTF16toUTF8(specifierKey).get()));
        return Err(ResolveError::BlockedByNullEntry);
      }

      // Step 1.1.2. Assert: resolutionResult is a URL.
      MOZ_ASSERT(resolutionResult);

      // Step 1.1.3. Return resolutionResult.
      return resolutionResult;
    }

    // Step 1.2. If all of the following are true:
    // specifierKey ends with U+002F (/),
    // normalizedSpecifier starts with specifierKey, and
    // either asURL is null, or asURL is special
    if (StringEndsWith(specifierKey, u"/"_ns) &&
        StringBeginsWith(aNormalizedSpecifier, specifierKey) &&
        (!aAsURL || IsSpecialScheme(aAsURL))) {
      // Step 1.2.1. If resolutionResult is null, then throw a TypeError
      // indicating that resolution of specifierKey was blocked by a null entry.
      // This will terminate the entire resolve a module specifier algorithm,
      // without any further fallbacks.
      if (!resolutionResult) {
        LOG(
            ("ImportMap::ResolveImportsMatch normalizedSpecifier: %s, "
             "specifierKey: %s, but resolution is null.",
             NS_ConvertUTF16toUTF8(aNormalizedSpecifier).get(),
             NS_ConvertUTF16toUTF8(specifierKey).get()));
        return Err(ResolveError::BlockedByNullEntry);
      }

      // Step 1.2.2. Assert: resolutionResult is a URL.
      MOZ_ASSERT(resolutionResult);

      // Step 1.2.3. Let afterPrefix be the portion of normalizedSpecifier after
      // the initial specifierKey prefix.
      nsAutoString afterPrefix(
          Substring(aNormalizedSpecifier, specifierKey.Length()));

      // Step 1.2.4. Assert: resolutionResult, serialized, ends with "/", as
      // enforced during parsing.
      MOZ_ASSERT(StringEndsWith(resolutionResult->GetSpecOrDefault(), "/"_ns));

      // Step 1.2.5. Let url be the result of parsing afterPrefix relative to
      // the base URL resolutionResult.
      nsCOMPtr<nsIURI> url;
      nsresult rv = NS_NewURI(getter_AddRefs(url), afterPrefix, nullptr,
                              resolutionResult);

      // Step 1.2.6. If url is failure, then throw a TypeError indicating that
      // resolution of normalizedSpecifier was blocked since the afterPrefix
      // portion could not be URL-parsed relative to the resolutionResult mapped
      // to by the specifierKey prefix.
      //
      // This will terminate the entire resolve a module specifier algorithm,
      // without any further fallbacks.
      if (NS_FAILED(rv)) {
        LOG(
            ("ImportMap::ResolveImportsMatch normalizedSpecifier: %s, "
             "specifierKey: %s, resolutionResult: %s, afterPrefix: %s, "
             "but URL is not parsable.",
             NS_ConvertUTF16toUTF8(aNormalizedSpecifier).get(),
             NS_ConvertUTF16toUTF8(specifierKey).get(),
             resolutionResult->GetSpecOrDefault().get(),
             NS_ConvertUTF16toUTF8(afterPrefix).get()));
        return Err(ResolveError::BlockedByAfterPrefix);
      }

      // Step 1.2.7. Assert: url is a URL.
      MOZ_ASSERT(url);

      // Step 1.2.8. If the serialization of url does not start with the
      // serialization of resolutionResult, then throw a TypeError indicating
      // that resolution of normalizedSpecifier was blocked due to it
      // backtracking above its prefix specifierKey.
      //
      // This will terminate the entire resolve a module specifier algorithm,
      // without any further fallbacks.
      if (!StringBeginsWith(url->GetSpecOrDefault(),
                            resolutionResult->GetSpecOrDefault())) {
        LOG(
            ("ImportMap::ResolveImportsMatch normalizedSpecifier: %s, "
             "specifierKey: %s, "
             "url %s does not start with resolutionResult %s.",
             NS_ConvertUTF16toUTF8(aNormalizedSpecifier).get(),
             NS_ConvertUTF16toUTF8(specifierKey).get(),
             url->GetSpecOrDefault().get(),
             resolutionResult->GetSpecOrDefault().get()));
        return Err(ResolveError::BlockedByBacktrackingPrefix);
      }

      // Step 1.2.9. Return url.
      return std::move(url);
    }
  }

  // Step 2. Return null.
  return nsCOMPtr<nsIURI>(nullptr);
}

// https://wicg.github.io/import-maps/#resolve-a-module-specifier
// static
ResolveResult ImportMap::ResolveModuleSpecifier(ImportMap* aImportMap,
                                                ScriptLoaderInterface* aLoader,
                                                LoadedScript* aScript,
                                                const nsAString& aSpecifier) {
  LOG(("ImportMap::ResolveModuleSpecifier specifier: %s",
       NS_ConvertUTF16toUTF8(aSpecifier).get()));
  nsCOMPtr<nsIURI> baseURL;
  if (aScript) {
    baseURL = aScript->BaseURL();
  } else {
    baseURL = aLoader->GetBaseURI();
  }

  // Step 6. Let asURL be the result of parsing a URL-like import specifier
  // given specifier and baseURL.
  //
  // Impl note: Step 5 is done below if aImportMap exists.
  auto parseResult = ParseURLLikeImportSpecifier(aSpecifier, baseURL);
  nsCOMPtr<nsIURI> asURL;
  if (parseResult.isOk()) {
    asURL = parseResult.unwrap();
  }

  if (aImportMap) {
    // Step 5. Let baseURLString be baseURL, serialized.
    nsCString baseURLString = baseURL->GetSpecOrDefault();

    // Step 7. Let normalizedSpecifier be the serialization of asURL, if asURL
    // is non-null; otherwise, specifier.
    nsAutoString normalizedSpecifier =
        asURL ? NS_ConvertUTF8toUTF16(asURL->GetSpecOrDefault())
              : nsAutoString{aSpecifier};

    // Step 8. For each scopePrefix → scopeImports of importMap’s scopes,
    for (auto&& [scopePrefix, scopeImports] : *aImportMap->mScopes) {
      // Step 8.1. If scopePrefix is baseURLString, or if scopePrefix ends with
      // U+002F (/) and baseURLString starts with scopePrefix, then:
      if (scopePrefix.Equals(baseURLString) ||
          (StringEndsWith(scopePrefix, "/"_ns) &&
           StringBeginsWith(baseURLString, scopePrefix))) {
        // Step 8.1.1. Let scopeImportsMatch be the result of resolving an
        // imports match given normalizedSpecifier, asURL, and scopeImports.
        auto result =
            ResolveImportsMatch(normalizedSpecifier, asURL, scopeImports.get());
        if (result.isErr()) {
          return result.propagateErr();
        }

        nsCOMPtr<nsIURI> scopeImportsMatch = result.unwrap();
        // Step 8.1.2. If scopeImportsMatch is not null, then return
        // scopeImportsMatch.
        if (scopeImportsMatch) {
          LOG((
              "ImportMap::ResolveModuleSpecifier returns scopeImportsMatch: %s",
              scopeImportsMatch->GetSpecOrDefault().get()));
          return WrapNotNull(scopeImportsMatch);
        }
      }
    }

    // Step 9. Let topLevelImportsMatch be the result of resolving an imports
    // match given normalizedSpecifier, asURL, and importMap’s imports.
    auto result = ResolveImportsMatch(normalizedSpecifier, asURL,
                                      aImportMap->mImports.get());
    if (result.isErr()) {
      return result.propagateErr();
    }
    nsCOMPtr<nsIURI> topLevelImportsMatch = result.unwrap();

    // Step 10. If topLevelImportsMatch is not null, then return
    // topLevelImportsMatch.
    if (topLevelImportsMatch) {
      LOG(("ImportMap::ResolveModuleSpecifier returns topLevelImportsMatch: %s",
           topLevelImportsMatch->GetSpecOrDefault().get()));
      return WrapNotNull(topLevelImportsMatch);
    }
  }

  // Step 11. At this point, the specifier was able to be turned in to a URL,
  // but it wasn’t remapped to anything by importMap. If asURL is not null, then
  // return asURL.
  if (asURL) {
    LOG(("ImportMap::ResolveModuleSpecifier returns asURL: %s",
         asURL->GetSpecOrDefault().get()));
    return WrapNotNull(asURL);
  }

  // Step 12. Throw a TypeError indicating that specifier was a bare specifier,
  // but was not remapped to anything by importMap.
  if (parseResult.unwrapErr() != ResolveError::FailureMayBeBare) {
    // We may have failed to parse a non-bare specifier for another reason.
    return Err(ResolveError::Failure);
  }

  return Err(ResolveError::InvalidBareSpecifier);
}

#undef LOG
#undef LOG_ENABLED
}  // namespace JS::loader
