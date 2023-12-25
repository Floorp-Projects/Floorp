/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef js_loader_ScriptFecthOptions_h
#define js_loader_ScriptFecthOptions_h

#include "mozilla/CORSMode.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/ReferrerPolicyBinding.h"
#include "mozilla/dom/RequestBinding.h"  // RequestPriority
#include "nsCOMPtr.h"
#include "nsIPrincipal.h"

namespace JS::loader {

// https://fetch.spec.whatwg.org/#concept-request-parser-metadata
// All scripts are either "parser-inserted" or "not-parser-inserted", so
// the empty string is not necessary.
enum class ParserMetadata {
  NotParserInserted,
  ParserInserted,
};

/*
 * ScriptFetchOptions loosely corresponds to HTML's "script fetch options",
 * https://html.spec.whatwg.org/multipage/webappapis.html#script-fetch-options
 * with the exception of the following properties:
 *   integrity metadata
 *      The integrity metadata used for the initial fetch. This is
 *      implemented in ScriptLoadRequest, as it changes for every
 *      ScriptLoadRequest.
 *
 *   referrerPolicy
 *     For a module script, its referrerPolicy will be updated if there is a
 *     HTTP Response 'REFERRER-POLICY' header, given this value may be different
 *     for every ScriptLoadRequest, so we store it directly in
 *     ScriptLoadRequest.
 *
 * In the case of classic scripts without dynamic import, this object is
 * used once. For modules, this object is propogated throughout the module
 * tree. If there is a dynamically imported module in any type of script,
 * the ScriptFetchOptions object will be propogated from its importer.
 */

class ScriptFetchOptions {
  ~ScriptFetchOptions();

 public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(ScriptFetchOptions)
  NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS(ScriptFetchOptions)

  ScriptFetchOptions(mozilla::CORSMode aCORSMode, const nsAString& aNonce,
                     mozilla::dom::RequestPriority aFetchPriority,
                     const ParserMetadata aParserMetadata,
                     nsIPrincipal* aTriggeringPrincipal,
                     mozilla::dom::Element* aElement = nullptr);

  /*
   *  The credentials mode used for the initial fetch (for module scripts)
   *  and for fetching any imported modules (for both module scripts and
   *  classic scripts)
   */
  const mozilla::CORSMode mCORSMode;

  /*
   * The cryptographic nonce metadata used for the initial fetch and for
   * fetching any imported modules.
   */
  const nsString mNonce;

  /*
   * <https://html.spec.whatwg.org/multipage/webappapis.html#script-fetch-options>.
   */
  const mozilla::dom::RequestPriority mFetchPriority;

  /*
   * The parser metadata used for the initial fetch and for fetching any
   * imported modules
   */
  const ParserMetadata mParserMetadata;

  /*
   *  Used to determine CSP and if we are on the About page.
   *  Only used in DOM content scripts.
   *  TODO: Move to ScriptLoadContext
   */
  nsCOMPtr<nsIPrincipal> mTriggeringPrincipal;
  /*
   *  Represents fields populated by DOM elements (nonce, parser metadata)
   *  Leave this field as a nullptr for any fetch that requires the
   *  default classic script options.
   *  (https://html.spec.whatwg.org/multipage/webappapis.html#default-classic-script-fetch-options)
   *  TODO: extract necessary fields rather than passing this object
   */
  nsCOMPtr<mozilla::dom::Element> mElement;
};

}  // namespace JS::loader

#endif  // js_loader_ScriptFetchOptions_h
