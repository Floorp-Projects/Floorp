/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that errors about invalid HPKP security headers are logged to the web console.

"use strict";

const TEST_URI = "data:text/html;charset=utf-8,Web Console HPKP invalid header test";
const SJS_URL = "https://example.com/browser/devtools/client/webconsole/" +
                "test/mochitest/test_hpkp-invalid-headers.sjs";
const LEARN_MORE_URI = "https://developer.mozilla.org/docs/Web/HTTP/" +
                       "Public_Key_Pinning" + DOCS_GA_PARAMS;
const NON_BUILTIN_ROOT_PREF =
  "security.cert_pinning.process_headers_from_non_builtin_roots";

add_task(async function() {
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref(NON_BUILTIN_ROOT_PREF);
  });

  const hud = await openNewTabAndConsole(TEST_URI);

  await navigateAndCheckForWarningMessage({
    url: SJS_URL + "?badSyntax",
    name: "Could not parse header error displayed successfully",
    text: "Public-Key-Pins: The site specified a header that could not be " +
          "parsed successfully."
  }, hud);

  await navigateAndCheckForWarningMessage({
    url: SJS_URL + "?noMaxAge",
    name: "No max-age error displayed successfully",
    text: "Public-Key-Pins: The site specified a header that did not include " +
          "a \u2018max-age\u2019 directive."
  }, hud);

  await navigateAndCheckForWarningMessage({
    url: SJS_URL + "?invalidIncludeSubDomains",
    name: "Invalid includeSubDomains error displayed successfully",
    text: "Public-Key-Pins: The site specified a header that included an " +
          "invalid \u2018includeSubDomains\u2019 directive."
  }, hud);

  await navigateAndCheckForWarningMessage({
    url: SJS_URL + "?invalidMaxAge",
    name: "Invalid max-age error displayed successfully",
    text: "Public-Key-Pins: The site specified a header that included an " +
          "invalid \u2018max-age\u2019 directive."
  }, hud);

  await navigateAndCheckForWarningMessage({
    url: SJS_URL + "?multipleIncludeSubDomains",
    name: "Multiple includeSubDomains error displayed successfully",
    text: "Public-Key-Pins: The site specified a header that included " +
          "multiple \u2018includeSubDomains\u2019 directives."
  }, hud);

  await navigateAndCheckForWarningMessage({
    url: SJS_URL + "?multipleMaxAge",
    name: "Multiple max-age error displayed successfully",
    text: "Public-Key-Pins: The site specified a header that included " +
          "multiple \u2018max-age\u2019 directives."
  }, hud);

  await navigateAndCheckForWarningMessage({
    url: SJS_URL + "?multipleReportURIs",
    name: "Multiple report-uri error displayed successfully",
    text: "Public-Key-Pins: The site specified a header that included " +
          "multiple \u2018report-uri\u2019 directives."
  }, hud);

  // The root used for mochitests is not built-in, so set the relevant pref to
  // true to have the PKP implementation return more specific errors.
  Services.prefs.setBoolPref(NON_BUILTIN_ROOT_PREF, true);

  await navigateAndCheckForWarningMessage({
    url: SJS_URL + "?pinsetDoesNotMatch",
    name: "Non-matching pinset error displayed successfully",
    text: "Public-Key-Pins: The site specified a header that did not include " +
          "a matching pin."
  }, hud);

  Services.prefs.setBoolPref(NON_BUILTIN_ROOT_PREF, false);

  await navigateAndCheckForWarningMessage({
    url: SJS_URL + "?pinsetDoesNotMatch",
    name: "Non-built-in root error displayed successfully",
    text: "Public-Key-Pins: The certificate used by the site was not issued " +
          "by a certificate in the default root certificate store. To " +
          "prevent accidental breakage, the specified header was ignored."
  }, hud);
});

async function navigateAndCheckForWarningMessage({name, text, url}, hud) {
  hud.ui.clearOutput(true);

  const onMessage = waitForMessage(hud, text, ".message.warning");
  BrowserTestUtils.loadURI(gBrowser.selectedBrowser, url);
  const {node} = await onMessage;
  ok(node, name);

  const learnMoreNode = node.querySelector(".learn-more-link");
  ok(learnMoreNode, `There is a "Learn more" link`);
  const navigationResponse = await simulateLinkClick(learnMoreNode);
  is(navigationResponse.link, LEARN_MORE_URI,
    "Click on the learn more link navigates the user to the expected url");
}
