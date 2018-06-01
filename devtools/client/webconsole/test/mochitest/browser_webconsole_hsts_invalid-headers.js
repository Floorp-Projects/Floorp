/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that errors about invalid HSTS security headers are logged to the web console.

"use strict";

const TEST_URI = "data:text/html;charset=utf-8,Web Console HSTS invalid header test";
const SJS_URL = "https://example.com/browser/devtools/client/webconsole/" +
                "/test/mochitest/test_hsts-invalid-headers.sjs";
const LEARN_MORE_URI = "https://developer.mozilla.org/docs/Web/HTTP/Headers/" +
                       "Strict-Transport-Security" + DOCS_GA_PARAMS;

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);

  await navigateAndCheckWarningMessage({
    url: SJS_URL + "?badSyntax",
    name: "Could not parse header error displayed successfully",
    text: "Strict-Transport-Security: The site specified a header that could " +
          "not be parsed successfully."
  }, hud);

  await navigateAndCheckWarningMessage({
    url: SJS_URL + "?noMaxAge",
    name: "No max-age error displayed successfully",
    text: "Strict-Transport-Security: The site specified a header that did " +
          "not include a \u2018max-age\u2019 directive."
  }, hud);

  await navigateAndCheckWarningMessage({
    url: SJS_URL + "?invalidIncludeSubDomains",
    name: "Invalid includeSubDomains error displayed successfully",
    text: "Strict-Transport-Security: The site specified a header that " +
          "included an invalid \u2018includeSubDomains\u2019 directive."
  }, hud);

  await navigateAndCheckWarningMessage({
    url: SJS_URL + "?invalidMaxAge",
    name: "Invalid max-age error displayed successfully",
    text: "Strict-Transport-Security: The site specified a header that " +
          "included an invalid \u2018max-age\u2019 directive."
  }, hud);

  await navigateAndCheckWarningMessage({
    url: SJS_URL + "?multipleIncludeSubDomains",
    name: "Multiple includeSubDomains error displayed successfully",
    text: "Strict-Transport-Security: The site specified a header that " +
          "included multiple \u2018includeSubDomains\u2019 directives."
  }, hud);

  await navigateAndCheckWarningMessage({
    url: SJS_URL + "?multipleMaxAge",
    name: "Multiple max-age error displayed successfully",
    text: "Strict-Transport-Security: The site specified a header that " +
          "included multiple \u2018max-age\u2019 directives."
  }, hud);
});

async function navigateAndCheckWarningMessage({url, name, text}, hud) {
  hud.jsterm.clearOutput(true);

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
