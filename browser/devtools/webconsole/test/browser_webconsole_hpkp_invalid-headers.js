 /* Any copyright is dedicated to the Public Domain.
  * http://creativecommons.org/publicdomain/zero/1.0/ */
/* Tests that errors about invalid HPKP security headers are logged
 *  to the web console */

"use strict";

const TEST_URI = "data:text/html;charset=utf-8,Web Console HPKP invalid " +
                 "header test";
const SJS_URL = "https://example.com/browser/browser/devtools/webconsole/" +
                "test/test_hpkp-invalid-headers.sjs";
const LEARN_MORE_URI = "https://developer.mozilla.org/docs/Web/Security/" +
                       "Public_Key_Pinning";
const NON_BUILTIN_ROOT_PREF = "security.cert_pinning.process_headers_from_" +
                              "non_builtin_roots";

let test = asyncTest(function* () {
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref(NON_BUILTIN_ROOT_PREF);
  });
  // The root used for mochitests is not built-in, so set the relevant pref to
  // true to force all pinning error messages to appear.
  Services.prefs.setBoolPref(NON_BUILTIN_ROOT_PREF, true);

  yield loadTab(TEST_URI);

  let hud = yield openConsole();

  yield* checkForMessage({
    url: SJS_URL + "?badSyntax",
    name: "Could not parse header error displayed successfully",
    text: "Public-Key-Pins: The site specified a header that could not be " +
          "parsed successfully."
  }, hud);

  yield* checkForMessage({
    url: SJS_URL + "?noMaxAge",
    name: "No max-age error displayed successfully",
    text: "Public-Key-Pins: The site specified a header that did not include " +
          "a 'max-age' directive."
  }, hud);

  yield* checkForMessage({
    url: SJS_URL + "?invalidIncludeSubDomains",
    name: "Invalid includeSubDomains error displayed successfully",
    text: "Public-Key-Pins: The site specified a header that included an " +
          "invalid 'includeSubDomains' directive."
  }, hud);

  yield* checkForMessage({
    url: SJS_URL + "?invalidMaxAge",
    name: "Invalid max-age error displayed successfully",
    text: "Public-Key-Pins: The site specified a header that included an " +
          "invalid 'max-age' directive."
  }, hud);

  yield* checkForMessage({
    url: SJS_URL + "?multipleIncludeSubDomains",
    name: "Multiple includeSubDomains error displayed successfully",
    text: "Public-Key-Pins: The site specified a header that included " +
          "multiple 'includeSubDomains' directives."
  }, hud);

  yield* checkForMessage({
    url: SJS_URL + "?multipleMaxAge",
    name: "Multiple max-age error displayed successfully",
    text: "Public-Key-Pins: The site specified a header that included " +
          "multiple 'max-age' directives."
  }, hud);

  yield* checkForMessage({
    url: SJS_URL + "?multipleReportURIs",
    name: "Multiple report-uri error displayed successfully",
    text: "Public-Key-Pins: The site specified a header that included " +
          "multiple 'report-uri' directives."
  }, hud);

  yield* checkForMessage({
    url: SJS_URL + "?pinsetDoesNotMatch",
    name: "Non-matching pinset error displayed successfully",
    text: "Public-Key-Pins: The site specified a header that did not include " +
          "a matching pin."
  }, hud);
});

function* checkForMessage(curTest, hud) {
  hud.jsterm.clearOutput();

  content.location = curTest.url;

  let results = yield waitForMessages({
    webconsole: hud,
    messages: [
      {
        name: curTest.name,
        text: curTest.text,
        category: CATEGORY_SECURITY,
        severity: SEVERITY_WARNING,
        objects: true,
      },
    ],
  });

  yield testClickOpenNewTab(hud, results);
}

function testClickOpenNewTab(hud, results) {
  let warningNode = results[0].clickableElements[0];
  ok(warningNode, "link element");
  ok(warningNode.classList.contains("learn-more-link"), "link class name");
  return simulateMessageLinkClick(warningNode, LEARN_MORE_URI);
}
