/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Check that clearing the browser console output also clears the console cache.

"use strict";

const TEST_URI = "data:text/html;charset=utf8,Test browser console clear cache";

add_task(async function() {
  await pushPref("devtools.browserconsole.contentMessages", true);
  await pushPref("devtools.browsertoolbox.fission", true);

  await addTab(TEST_URI);
  let hud = await BrowserConsoleManager.toggleBrowserConsole();
  // builtin-modules warning messages seem to be emitted late and causes the test to fail,
  // so we filter those messages out (Bug 1479876)
  await setFilterState(hud, { text: "-builtin-modules.js" });

  const CACHED_MESSAGE = "CACHED_MESSAGE";
  await logTextInContentAndWaitForMessage(hud, CACHED_MESSAGE);

  info("Click the clear output button");
  const onBrowserConsoleOutputCleared = waitFor(
    () => !findMessage(hud, CACHED_MESSAGE)
  );
  hud.ui.window.document.querySelector(".devtools-clear-icon").click();
  await onBrowserConsoleOutputCleared;

  // Check that there are no other messages logged (see Bug 1457478).
  // Log a message to make sure the console handled any prior log.
  await logTextInContentAndWaitForMessage(hud, "after clear");
  const messages = hud.ui.outputNode.querySelectorAll(".message");
  is(messages.length, 1, "There is only the new message in the output");

  info("Close and re-open the browser console");
  await safeCloseBrowserConsole();
  hud = await BrowserConsoleManager.toggleBrowserConsole();

  info("Log a smoke message in order to know that the console is ready");
  await logTextInContentAndWaitForMessage(hud, "Smoke message");
  is(
    findMessage(hud, CACHED_MESSAGE),
    undefined,
    "The cached message is not visible anymore"
  );
});

function logTextInContentAndWaitForMessage(hud, text) {
  const onMessage = waitForMessage(hud, text);
  SpecialPowers.spawn(gBrowser.selectedBrowser, [text], function(str) {
    content.wrappedJSObject.console.log(str);
  });
  return onMessage;
}
