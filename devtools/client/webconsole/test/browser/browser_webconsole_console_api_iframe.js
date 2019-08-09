/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that Console API works with iframes. See Bug 613013.

const TEST_URI =
  "http://example.com/browser/devtools/client/webconsole/" +
  "test/browser/test-console-api-iframe.html";

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);
  const loggedString = "iframe added";
  // Wait for the initial message to be displayed.
  await waitFor(() => findMessage(hud, loggedString));
  ok(true, "The initial message is displayed in the console");
  // Create a promise for the message logged after the reload.
  const onMessage = waitForMessage(hud, loggedString);
  BrowserReload();
  await onMessage;
  ok(true, "The message is also displayed after a page reload");
});
