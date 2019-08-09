/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that errors about insecure passwords are logged to the web console.
// See Bug 762593.

"use strict";

const INSECURE_IFRAME_URI =
  "http://example.com/browser/devtools/client/webconsole/" +
  "test/browser/test-insecure-passwords-web-console-warning.html";
const INSECURE_PASSWORD_URI =
  "http://example.com/browser/devtools/client/webconsole/" +
  "test/browser/test-iframe-insecure-form-action.html";
const INSECURE_FORM_ACTION_URI =
  "https://example.com/browser/devtools/client/" +
  "webconsole/test/browser/test-iframe-insecure-form-action.html";

const STOLEN =
  "This is a security risk that allows user login credentials to be stolen.";
const INSECURE_PASSWORD_MSG =
  "Password fields present on an insecure (http://) page. " + STOLEN;
const INSECURE_FORM_ACTION_MSG =
  "Password fields present in a form with an insecure (http://) form action. " +
  STOLEN;
const INSECURE_IFRAME_MSG =
  "Password fields present on an insecure (http://) iframe. " + STOLEN;
const INSECURE_PASSWORDS_URI =
  "https://developer.mozilla.org/docs/Web/Security/Insecure_passwords" +
  DOCS_GA_PARAMS;

add_task(async function() {
  await testUriWarningMessage(INSECURE_IFRAME_URI, INSECURE_IFRAME_MSG);
  await testUriWarningMessage(INSECURE_PASSWORD_URI, INSECURE_PASSWORD_MSG);
  await testUriWarningMessage(
    INSECURE_FORM_ACTION_URI,
    INSECURE_FORM_ACTION_MSG
  );
});

async function testUriWarningMessage(uri, warningMessage) {
  const hud = await openNewTabAndConsole(uri);
  const message = await waitFor(() =>
    findMessage(hud, warningMessage, ".message.warn")
  );
  ok(message, "Warning message displayed successfully");
  await testLearnMoreLinkClick(message, INSECURE_PASSWORDS_URI);
}

async function testLearnMoreLinkClick(message, expectedUri) {
  const learnMoreLink = message.querySelector(".learn-more-link");
  ok(learnMoreLink, "There is a [Learn More] link");
  const { link } = await simulateLinkClick(learnMoreLink);
  is(
    link,
    expectedUri,
    "Click on [Learn More] link navigates user to " + expectedUri
  );
}
