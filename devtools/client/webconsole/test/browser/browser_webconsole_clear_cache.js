/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Check that clearing the output also clears the console cache.

"use strict";

const TEST_URI =
  "data:text/html;charset=utf8,<!DOCTYPE html>Test clear cache<script>abcdef</script>";
const EXPECTED_REPORT = "ReferenceError: abcdef is not defined";

add_task(async function() {
  const tab = await addTab(TEST_URI);
  let hud = await openConsole(tab);

  const CACHED_MESSAGE = "CACHED_MESSAGE";
  await logTextToConsole(hud, CACHED_MESSAGE);

  info("Close and re-open the console");
  await closeToolbox();
  hud = await openConsole(tab);

  await waitFor(() => findErrorMessage(hud, EXPECTED_REPORT));
  await waitFor(() => findConsoleAPIMessage(hud, CACHED_MESSAGE));

  info(
    "Click the clear output button and wait until there's no messages in the output"
  );
  let onMessagesCacheCleared = hud.ui.once("messages-cache-cleared");
  hud.ui.window.document.querySelector(".devtools-clear-icon").click();
  await onMessagesCacheCleared;

  info("Close and re-open the console");
  await closeToolbox();
  hud = await openConsole(tab);

  info("Log a smoke message in order to know that the console is ready");
  await logTextToConsole(hud, "Smoke message");
  is(
    findConsoleAPIMessage(hud, CACHED_MESSAGE),
    undefined,
    "The cached message is not visible anymore"
  );
  is(
    findErrorMessage(hud, EXPECTED_REPORT),
    undefined,
    "The cached error message is not visible anymore as well"
  );

  // Test that we also clear the cache when calling console.clear().
  const NEW_CACHED_MESSAGE = "NEW_CACHED_MESSAGE";
  await logTextToConsole(hud, NEW_CACHED_MESSAGE);

  info("Send a console.clear() from the content page");
  onMessagesCacheCleared = hud.ui.once("messages-cache-cleared");
  const onConsoleCleared = waitForMessageByType(
    hud,
    "Console was cleared",
    ".console-api"
  );
  SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    content.wrappedJSObject.console.clear();
  });
  await Promise.all([onConsoleCleared, onMessagesCacheCleared]);

  info("Close and re-open the console");
  await closeToolbox();
  hud = await openConsole(tab);

  info("Log a smoke message in order to know that the console is ready");
  await logTextToConsole(hud, "Second smoke message");
  is(
    findConsoleAPIMessage(hud, NEW_CACHED_MESSAGE),
    undefined,
    "The new cached message is not visible anymore"
  );
});

function logTextToConsole(hud, text) {
  const onMessage = waitForMessageByType(hud, text, ".console-api");
  SpecialPowers.spawn(gBrowser.selectedBrowser, [text], function(str) {
    content.wrappedJSObject.console.log(str);
  });
  return onMessage;
}
