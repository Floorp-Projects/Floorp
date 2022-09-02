/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that (cached and live) logs and errors are displayed in the expected order
// in the console output. See Bug 1483662.

"use strict";

const TEST_URI =
  "https://example.com/browser/devtools/client/webconsole/test/browser/test-console-logs-exceptions-order.html";

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);
  await checkConsoleOutput(hud);

  info("Reload the content window");
  await reloadBrowser();
  await checkConsoleOutput(hud);
});

async function checkConsoleOutput(hud) {
  await waitFor(
    () =>
      findConsoleAPIMessage(hud, "First") &&
      findErrorMessage(hud, "Second") &&
      findConsoleAPIMessage(hud, "Third") &&
      findErrorMessage(hud, "Fourth")
  );

  const messagesText = Array.from(
    hud.ui.outputNode.querySelectorAll(".message .message-body")
  ).map(n => n.textContent);

  Assert.deepEqual(
    messagesText,
    ["First", "Uncaught Second", "Third", "Uncaught Fourth"],
    "Errors are displayed in the expected order"
  );
}
