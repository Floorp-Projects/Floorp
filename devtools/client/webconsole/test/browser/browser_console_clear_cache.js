/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Check that clearing the browser console output also clears the console cache.

"use strict";

const TEST_URI =
  "data:text/html;charset=utf8,<!DOCTYPE html>Test browser console clear cache";

add_task(async function () {
  await pushPref("devtools.browsertoolbox.scope", "everything");

  await addTab(TEST_URI);
  let hud = await BrowserConsoleManager.toggleBrowserConsole();

  const CACHED_MESSAGE = "CACHED_MESSAGE";
  await logTextInContentAndWaitForMessage(hud, CACHED_MESSAGE);

  info("Click the clear output button");
  const onBrowserConsoleOutputCleared = waitFor(
    () => !findConsoleAPIMessage(hud, CACHED_MESSAGE)
  );
  hud.ui.window.document.querySelector(".devtools-clear-icon").click();
  await onBrowserConsoleOutputCleared;
  ok(true, "Message was cleared");

  info("Close and re-open the browser console");
  await safeCloseBrowserConsole();
  hud = await BrowserConsoleManager.toggleBrowserConsole();

  info("Log a smoke message in order to know that the console is ready");
  await logTextInContentAndWaitForMessage(hud, "Smoke message");
  is(
    findConsoleAPIMessage(hud, CACHED_MESSAGE),
    undefined,
    "The cached message is not visible anymore"
  );
});

function logTextInContentAndWaitForMessage(hud, text) {
  const onMessage = waitForMessageByType(hud, text, ".console-api");
  SpecialPowers.spawn(gBrowser.selectedBrowser, [text], function (str) {
    content.wrappedJSObject.console.log(str);
  });
  return onMessage;
}
