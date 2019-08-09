/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Check that Dead Objects do not break the Web/Browser Consoles.
//
// This test:
// - Opens the Browser Console.
// - Creates a sandbox.
// - Stores a reference to the sandbox on the chrome window object.
// - Nukes the sandbox
// - Tries to use the sandbox. This is the dead object.

"use strict";

add_task(async function() {
  const hud = await BrowserConsoleManager.toggleBrowserConsole();
  ok(hud, "browser console opened");

  // Add the reference to the nuked sandbox.
  execute(
    hud,
    "window.nukedSandbox = Cu.Sandbox(null); Cu.nukeSandbox(nukedSandbox);"
  );

  await executeAndWaitForMessage(hud, "nukedSandbox", "DeadObject");
  const msg = await executeAndWaitForMessage(
    hud,
    "nukedSandbox.hello",
    "can't access dead object"
  );

  // Check that the link contains an anchor. We can't click on the link because
  // clicking links from tests attempts to access an external URL and crashes Firefox.
  const anchor = msg.node.querySelector("a");
  is(anchor.textContent, "[Learn More]", "Link text is correct");

  await executeAndWaitForMessage(hud, "delete window.nukedSandbox; 1 + 1", "2");
});
