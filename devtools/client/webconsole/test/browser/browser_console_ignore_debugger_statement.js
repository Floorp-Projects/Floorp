/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test that debugger statements are ignored in the browser console.

"use strict";

const URI_WITH_DEBUGGER_STATEMENT = `data:text/html,<!DOCTYPE html>
  <meta charset=utf8>
  browser console ignore debugger statement
  <script>
    debugger;
    console.log("after debugger statement");
  </script>`;

add_task(async function() {
  // Show the content messages
  await pushPref("devtools.browserconsole.contentMessages", true);
  await pushPref("devtools.browsertoolbox.fission", true);

  info("Open the Browser Console");
  const hud = await BrowserConsoleManager.toggleBrowserConsole();

  info("Add tab with a script containing debugger statement");
  const onMessage = waitForMessageByType(
    hud,
    `after debugger statement`,
    ".console-api"
  );
  await addTab(URI_WITH_DEBUGGER_STATEMENT);
  await onMessage;

  ok(true, "The debugger statement was ignored");
});
