/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Check that clearing the browser console output also clears the console cache.

"use strict";

const TEST_URI = "data:text/html;charset=utf8,Test browser console clear cache";

add_task(async function() {
  await pushPref("devtools.browserconsole.contentMessages", true);
  await addTab(TEST_URI);
  let hud = await BrowserConsoleManager.toggleBrowserConsole();
  const CACHED_MESSAGE = "CACHED_MESSAGE";
  await logTextToConsole(hud, CACHED_MESSAGE);

  info("Click the clear output button");
  const onBrowserConsoleOutputCleared = waitFor(
    () => !findMessage(hud, CACHED_MESSAGE)
  );
  hud.ui.window.document.querySelector(".devtools-clear-icon").click();
  await onBrowserConsoleOutputCleared;

  // Check that there are no other messages logged (see Bug 1457478).
  // Log a message to make sure the console handled any prior log.
  await logTextToConsole(hud, "after clear");
  const messages = hud.ui.outputNode.querySelectorAll(".message");
  is(messages.length, 1, "There is only the new message in the output");

  info("Close and re-open the browser console");
  await BrowserConsoleManager.toggleBrowserConsole();
  hud = await BrowserConsoleManager.toggleBrowserConsole();

  info("Log a smoke message in order to know that the console is ready");
  await logTextToConsole(hud, "Smoke message");
  is(
    findMessage(hud, CACHED_MESSAGE),
    null,
    "The cached message is not visible anymore"
  );
});

function logTextToConsole(hud, text) {
  const onMessage = waitForMessage(hud, text);
  ContentTask.spawn(gBrowser.selectedBrowser, text, function(str) {
    content.wrappedJSObject.console.log(str);
  });
  return onMessage;
}
