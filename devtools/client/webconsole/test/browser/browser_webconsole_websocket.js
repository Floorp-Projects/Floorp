/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that WebSocket connection failure messages are displayed. See Bug 603750.

const TEST_URI =
  "http://example.com/browser/devtools/client/webconsole/test/browser/test-websocket.html";

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);
  await waitFor(
    () => findMessage(hud, "ws://0.0.0.0:81", ".error"),
    "Did not find error message for ws://0.0.0.0:81 connection",
    500
  );
  await waitFor(
    () => findMessage(hud, "ws://0.0.0.0:82", ".error"),
    "Did not find error message for ws://0.0.0.0:82 connection",
    500
  );
  ok(true, "WebSocket error messages are displayed in the console");
});
