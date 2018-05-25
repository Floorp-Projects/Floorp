/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the inspector panel has a 3 pane toggle button, and that
// this button is visible both in BOTTOM and SIDE hosts.

add_task(async function() {
  info("Switch to 2 pane inspector to test the 3 pane toggle button behavior");
  await pushPref("devtools.inspector.three-pane-enabled", false);

  info("Open the inspector in a bottom toolbox host");
  const { inspector, toolbox } = await openInspectorForURL("about:blank", "bottom");

  const button = inspector.panelDoc.querySelector(".sidebar-toggle");
  ok(button, "The toggle button exists in the DOM");
  ok(button.getAttribute("title"), "The title tooltip has initial state");
  ok(button.classList.contains("pane-collapsed"), "The button is in collapsed state");
  ok(!!button.getClientRects().length, "The button is visible");

  info("Switch the host to side type");
  await toolbox.switchHost("side");

  ok(!!button.getClientRects().length, "The button is still visible");
  ok(button.classList.contains("pane-collapsed"),
    "The button is still in collapsed state");
});
