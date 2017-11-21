/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that WebSocket connection failure messages are displayed. See Bug 603750.

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "new-console-output/test/mochitest/test-websocket.html";
const TEST_URI2 = "data:text/html;charset=utf-8,Web Console test for " +
                  "bug 603750: Web Socket errors";

add_task(async function () {
  const hud = await openNewTabAndConsole(TEST_URI2);

  BrowserTestUtils.loadURI(gBrowser.selectedBrowser, TEST_URI);

  await waitFor(() => findMessage(hud, "ws://0.0.0.0:81"));
  await waitFor(() => findMessage(hud, "ws://0.0.0.0:82"));
  ok(true, "WebSocket error messages are displayed in the console");
});
