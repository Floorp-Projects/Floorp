/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Check that clearing the output also clears the console cache.

"use strict";

const TEST_URI =
  "data:text/html;charset=utf8,Test clear cache<script>abcdef</script>";
const EXPECTED_REPORT = "ReferenceError: abcdef is not defined";

add_task(async function() {
  const tab = await addTab(TEST_URI);
  let hud = await openConsole(tab);

  const CACHED_MESSAGE = "CACHED_MESSAGE";
  await logTextToConsole(hud, CACHED_MESSAGE);

  info("Close and re-open the console");
  await closeToolbox();
  hud = await openConsole(tab);

  await waitFor(() => findMessage(hud, EXPECTED_REPORT));
  await waitFor(() => findMessage(hud, CACHED_MESSAGE));

  info(
    "Click the clear output button and wait until there's no messages in the output"
  );
  hud.ui.window.document.querySelector(".devtools-clear-icon").click();
  await waitFor(() => findMessages(hud, "").length === 0);

  info("Close and re-open the console");
  await closeToolbox();
  hud = await openConsole(tab);

  info("Log a smoke message in order to know that the console is ready");
  await logTextToConsole(hud, "Smoke message");
  is(
    findMessage(hud, CACHED_MESSAGE),
    null,
    "The cached message is not visible anymore"
  );
  is(
    findMessage(hud, EXPECTED_REPORT),
    null,
    "The cached error message is not visible anymore as well"
  );

  // Test that we also clear the cache when calling console.clear().
  const NEW_CACHED_MESSAGE = "NEW_CACHED_MESSAGE";
  await logTextToConsole(hud, NEW_CACHED_MESSAGE);

  info("Send a console.clear() from the content page");
  const onConsoleCleared = waitForMessage(hud, "Console was cleared");
  ContentTask.spawn(gBrowser.selectedBrowser, {}, () => {
    content.wrappedJSObject.console.clear();
  });
  await onConsoleCleared;

  info("Close and re-open the console");
  await closeToolbox();
  hud = await openConsole(tab);

  info("Log a smoke message in order to know that the console is ready");
  await logTextToConsole(hud, "Second smoke message");
  is(
    findMessage(hud, NEW_CACHED_MESSAGE),
    null,
    "The new cached message is not visible anymore"
  );
});

function logTextToConsole(hud, text) {
  const onMessage = waitForMessage(hud, text);
  ContentTask.spawn(gBrowser.selectedBrowser, text, function(str) {
    content.wrappedJSObject.console.log(str);
  });
  return onMessage;
}
