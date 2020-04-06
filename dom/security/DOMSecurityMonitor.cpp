/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DOMSecurityMonitor.h"
#include "nsContentUtils.h"

#include "nsIChannel.h"
#include "nsILoadInfo.h"
#include "nsIPrincipal.h"
#include "nsIURI.h"

#include "mozilla/StaticPrefs_dom.h"

/* static */
void DOMSecurityMonitor::AuditParsingOfHTMLXMLFragments(
    nsIPrincipal* aPrincipal, const nsAString& aFragment) {
  // if the fragment parser (e.g. innerHTML()) is not called in chrome: code
  // or any of our about: pages, then there is nothing to do here.
  if (!aPrincipal->IsSystemPrincipal() && !aPrincipal->SchemeIs("about")) {
    return;
  }

  // check if the fragment is empty, if so, we can return early.
  if (aFragment.IsEmpty()) {
    return;
  }

  // check if there is a JS caller, if not, then we can can return early here
  // because we only care about calls to the fragment parser (e.g. innerHTML)
  // originating from JS code.
  nsAutoString filename;
  uint32_t lineNum = 0;
  uint32_t columnNum = 0;
  JSContext* cx = nsContentUtils::GetCurrentJSContext();
  if (!nsJSUtils::GetCallingLocation(cx, filename, &lineNum, &columnNum)) {
    return;
  }

  // check if we should skip assertion. Please only ever set this pref to
  // true if really needed for testing purposes.
  if (StaticPrefs::dom_security_skip_html_fragment_assertion()) {
    return;
  }

  /*
   * WARNING: Do not add any new entries to the htmlFragmentAllowlist
   * withiout proper review from a dom:security peer!
   */
  static nsLiteralCString htmlFragmentAllowlist[] = {
      NS_LITERAL_CSTRING("chrome://global/content/elements/marquee.js"),
      NS_LITERAL_CSTRING(
          "chrome://pocket/content/panels/js/vendor/jquery-2.1.1.min.js"),
      NS_LITERAL_CSTRING("chrome://browser/content/aboutNetError.js"),
      NS_LITERAL_CSTRING("chrome://devtools/content/shared/sourceeditor/"
                         "codemirror/codemirror.bundle.js"),
      NS_LITERAL_CSTRING(
          "chrome://devtools-startup/content/aboutdevtools/aboutdevtools.js"),
      NS_LITERAL_CSTRING(
          "resource://activity-stream/data/content/activity-stream.bundle.js"),
      NS_LITERAL_CSTRING("resource://devtools/client/debugger/src/components/"
                         "Editor/Breakpoint.js"),
      NS_LITERAL_CSTRING("resource://devtools/client/debugger/src/components/"
                         "Editor/ColumnBreakpoint.js"),
      NS_LITERAL_CSTRING(
          "resource://devtools/client/shared/vendor/fluent-react.js"),
      NS_LITERAL_CSTRING(
          "resource://devtools/client/shared/vendor/react-dom.js"),
      NS_LITERAL_CSTRING(
          "resource://devtools/client/shared/widgets/FilterWidget.js"),
      NS_LITERAL_CSTRING("resource://devtools/client/shared/widgets/tooltip/"
                         "inactive-css-tooltip-helper.js"),
      NS_LITERAL_CSTRING(
          "resource://devtools/client/shared/widgets/Spectrum.js"),
      NS_LITERAL_CSTRING("resource://gre/modules/narrate/VoiceSelect.jsm"),
      NS_LITERAL_CSTRING("resource://normandy-vendor/ReactDOM.js"),
      // ------------------------------------------------------------------
      // test pages
      // ------------------------------------------------------------------
      NS_LITERAL_CSTRING("chrome://mochikit/content/harness.xhtml"),
      NS_LITERAL_CSTRING("chrome://mochikit/content/tests/"),
      NS_LITERAL_CSTRING("chrome://mochitests/content/"),
      NS_LITERAL_CSTRING("chrome://reftest/content/"),
  };

  for (const nsLiteralCString& allowlistEntry : htmlFragmentAllowlist) {
    if (StringBeginsWith(NS_ConvertUTF16toUTF8(filename), allowlistEntry)) {
      return;
    }
  }

  nsAutoCString uriSpec;
  aPrincipal->GetAsciiSpec(uriSpec);

  // Ideally we should not call the fragment parser (e.g. innerHTML()) in
  // chrome: code or any of our about: pages. If you hit that assertion,
  // please do *not* add your filename to the allowlist above, but rather
  // refactor your code.
  fprintf(stderr,
          "Do not call the fragment parser (e.g innerHTML()) in chrome code "
          "or in about: pages, (uri: %s), (caller: %s, line: %d, col: %d), "
          "(fragment: %s)",
          uriSpec.get(), NS_ConvertUTF16toUTF8(filename).get(), lineNum,
          columnNum, NS_ConvertUTF16toUTF8(aFragment).get());
  MOZ_ASSERT(false);
}

/* static */
void DOMSecurityMonitor::AuditUseOfJavaScriptURI(nsIChannel* aChannel) {
  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();
  nsCOMPtr<nsIPrincipal> loadingPrincipal = loadInfo->GetLoadingPrincipal();

  // We only ever have no loadingPrincipal in case of a new top-level load.
  // The purpose of this assertion is to make sure we do not allow loading
  // javascript: URIs in system privileged contexts. Hence there is nothing
  // to do here in case there is no loadingPrincipal.
  if (!loadingPrincipal) {
    return;
  }

  // if the javascript: URI is not loaded by a system privileged context
  // or an about: page, there there is nothing to do here.
  if (!loadingPrincipal->IsSystemPrincipal() &&
      !loadingPrincipal->SchemeIs("about")) {
    return;
  }

  MOZ_ASSERT(false,
             "Do not use javascript: URIs in chrome code or in about: pages");
}
