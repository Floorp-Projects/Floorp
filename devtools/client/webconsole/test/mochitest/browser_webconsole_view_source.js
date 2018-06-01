/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that source URLs in the Web Console can be clicked to display the
// standard View Source window. As JS exceptions and console.log() messages always
// have their locations opened in Debugger, we need to test a security message in
// order to have it opened in the standard View Source window.

"use strict";

const TEST_URI = "https://example.com/browser/devtools/client/webconsole/" +
                 "test/mochitest/" +
                 "test-mixedcontent-securityerrors.html";

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);
  info("console opened");

  const msg =
    await waitFor(() => findMessage(hud, "Blocked loading mixed active content"));
  ok(msg, "error message");
  const locationNode = msg.querySelector(".message-location .frame-link-filename");
  ok(locationNode, "location node");

  const onTabOpen = BrowserTestUtils.waitForNewTab(gBrowser, null, true);

  locationNode.click();
  await onTabOpen;
  ok(true, "the view source tab was opened in response to clicking the location node");
});
