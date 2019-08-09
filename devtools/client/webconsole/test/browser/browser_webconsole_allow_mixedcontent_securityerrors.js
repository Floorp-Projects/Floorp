/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// The test loads a web page with mixed active and display content
// on it while the "block mixed content" settings are _off_.
// It then checks that the loading mixed content warning messages
// are logged to the console and have the correct "Learn More"
// url appended to them.
// Bug 875456 - Log mixed content messages from the Mixed Content
// Blocker to the Security Pane in the Web Console

"use strict";

const TEST_URI =
  "https://example.com/browser/devtools/client/webconsole/" +
  "test/browser/test-mixedcontent-securityerrors.html";
const LEARN_MORE_URI =
  "https://developer.mozilla.org/docs/Web/Security/" +
  "Mixed_content" +
  DOCS_GA_PARAMS;

add_task(async function() {
  await Promise.all([
    pushPref("security.mixed_content.block_active_content", false),
    pushPref("security.mixed_content.block_display_content", false),
    pushPref("security.mixed_content.upgrade_display_content", false),
  ]);

  const hud = await openNewTabAndConsole(TEST_URI);

  const activeContentText =
    "Loading mixed (insecure) active content " +
    "\u201chttp://example.com/\u201d on a secure page";
  const displayContentText =
    "Loading mixed (insecure) display content " +
    "\u201chttp://example.com/tests/image/test/mochitest/blue.png\u201d on a secure page";

  const waitUntilWarningMessage = text =>
    waitFor(() => findMessage(hud, text, ".message.warn"), undefined, 100);

  const onMixedActiveContent = waitUntilWarningMessage(activeContentText);
  const onMixedDisplayContent = waitUntilWarningMessage(displayContentText);

  await onMixedDisplayContent;
  ok(true, "Mixed display content warning message is visible");

  const mixedActiveContentMessage = await onMixedActiveContent;
  ok(true, "Mixed active content warning message is visible");

  const checkLink = ({ link, where, expectedLink, expectedTab }) => {
    is(link, expectedLink, `Clicking the provided link opens ${link}`);
    is(where, expectedTab, `Clicking the provided link opens in expected tab`);
  };

  info("Clicking on the Learn More link");
  const learnMoreLink = mixedActiveContentMessage.querySelector(
    ".learn-more-link"
  );
  const linkSimulation = await simulateLinkClick(learnMoreLink);
  checkLink({
    ...linkSimulation,
    expectedLink: LEARN_MORE_URI,
    expectedTab: "tab",
  });
});
