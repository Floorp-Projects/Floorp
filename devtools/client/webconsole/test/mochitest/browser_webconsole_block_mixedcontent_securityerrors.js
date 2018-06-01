/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// The test loads a web page with mixed active and display content
// on it while the "block mixed content" settings are _on_.
// It then checks that the blocked mixed content warning messages
// are logged to the console and have the correct "Learn More"
// url appended to them. After the first test finishes, it invokes
// a second test that overrides the mixed content blocker settings
// by clicking on the doorhanger shield and validates that the
// appropriate messages are logged to console.
// Bug 875456 - Log mixed content messages from the Mixed Content
// Blocker to the Security Pane in the Web Console.

"use strict";

const TEST_URI = "https://example.com/browser/devtools/client/webconsole/" +
                 "test/mochitest/" +
                 "test-mixedcontent-securityerrors.html";
const LEARN_MORE_URI = "https://developer.mozilla.org/docs/Web/Security/" +
                       "Mixed_content" + DOCS_GA_PARAMS;

const blockedActiveContentText = "Blocked loading mixed active content " +
  "\u201chttp://example.com/\u201d";
const blockedDisplayContentText = "Blocked loading mixed display content " +
  "\u201chttp://example.com/tests/image/test/mochitest/blue.png\u201d";
const activeContentText = "Loading mixed (insecure) active content " +
  "\u201chttp://example.com/\u201d on a secure page";
const displayContentText = "Loading mixed (insecure) display content " +
  "\u201chttp://example.com/tests/image/test/mochitest/blue.png\u201d on a " +
  "secure page";

add_task(async function() {
  await pushPrefEnv();

  const hud = await openNewTabAndConsole(TEST_URI);

  const waitForErrorMessage = text =>
    waitFor(() => findMessage(hud, text, ".message.error"), undefined, 100);

  const onBlockedIframe = waitForErrorMessage(blockedActiveContentText);
  const onBlockedImage = waitForErrorMessage(blockedDisplayContentText);

  await onBlockedImage;
  ok(true, "Blocked mixed display content error message is visible");

  const blockedMixedActiveContentMessage = await onBlockedIframe;
  ok(true, "Blocked mixed active content error message is visible");

  info("Clicking on the Learn More link");
  let learnMoreLink = blockedMixedActiveContentMessage.querySelector(".learn-more-link");
  let response = await simulateLinkClick(learnMoreLink);
  is(response.link, LEARN_MORE_URI, `Clicking the provided link opens ${response.link}`);

  info("Test disabling mixed content protection");

  const {gIdentityHandler} = gBrowser.ownerGlobal;
  ok(gIdentityHandler._identityBox.classList.contains("mixedActiveBlocked"),
    "Mixed Active Content state appeared on identity box");
  // Disabe mixed content protection.
  gIdentityHandler.disableMixedContentProtection();

  const waitForWarningMessage = text =>
    waitFor(() => findMessage(hud, text, ".message.warn"), undefined, 100);

  const onMixedActiveContent = waitForWarningMessage(activeContentText);
  const onMixedDisplayContent = waitForWarningMessage(displayContentText);

  await onMixedDisplayContent;
  ok(true, "Mixed display content warning message is visible");

  const mixedActiveContentMessage = await onMixedActiveContent;
  ok(true, "Mixed active content warning message is visible");

  info("Clicking on the Learn More link");
  learnMoreLink = mixedActiveContentMessage.querySelector(".learn-more-link");
  response = await simulateLinkClick(learnMoreLink);
  is(response.link, LEARN_MORE_URI, `Clicking the provided link opens ${response.link}`);
});

function pushPrefEnv() {
  const prefs = [
    ["security.mixed_content.block_active_content", true],
    ["security.mixed_content.block_display_content", true],
    ["security.mixed_content.upgrade_display_content", false],
  ];

  return Promise.all(prefs.map(([pref, value]) => pushPref(pref, value)));
}
