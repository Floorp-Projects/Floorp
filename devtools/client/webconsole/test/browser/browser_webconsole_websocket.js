/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that WebSocket connection failure messages are displayed. See Bug 603750.

const TEST_URI =
  "https://example.com/browser/devtools/client/webconsole/test/browser/test-websocket.html";

add_task(async function () {
  const hud = await openNewTabAndConsole(TEST_URI);
  await waitFor(
    () => findErrorMessage(hud, "wss://0.0.0.0:81"),
    "Did not find error message for wss://0.0.0.0:81 connection",
    500
  );
  await waitFor(
    () => findErrorMessage(hud, "wss://0.0.0.0:82"),
    "Did not find error message for wss://0.0.0.0:82 connection",
    500
  );
  ok(true, "WebSocket error messages are displayed in the console");
});
