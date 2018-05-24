/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that we can log warning message to the web console from the toolbox.

const TEST_URI = "data:text/html;charset=utf-8,<p>test logErrorInPage";

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);
  const toolbox = hud.ui.consoleOutput.toolbox;

  toolbox.target.logWarningInPage("beware the octopus", "content javascript");

  const node = await waitFor(() => findMessage(hud, "octopus"));
  ok(node, "text is displayed in web console");
  ok(node.classList.contains("warn"), "the log represents a warning");
});
