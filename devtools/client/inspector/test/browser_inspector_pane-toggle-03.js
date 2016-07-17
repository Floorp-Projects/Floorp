/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that the toggle button can collapse and expand the inspector side/bottom
// panel, and that the appropriate attributes are updated in the process.

add_task(function* () {
  let {inspector} = yield openInspectorForURL("about:blank");

  let button = inspector.panelDoc.querySelector(".sidebar-toggle");
  let panel = inspector.panelDoc.querySelector("#inspector-sidebar-container");

  ok(!button.classList.contains("pane-collapsed"), "The button is in expanded state");

  info("Listen to the end of the animation on the sidebar panel");
  let onTransitionEnd = once(panel, "transitionend");

  info("Click on the toggle button");
  EventUtils.synthesizeMouseAtCenter(button, {},
    inspector.panelDoc.defaultView);

  yield onTransitionEnd;
  ok(button.classList.contains("pane-collapsed"), "The button is in collapsed state");
  ok(panel.classList.contains("pane-collapsed"), "The panel is in collapsed state");

  info("Listen again to the end of the animation on the sidebar panel");
  onTransitionEnd = once(panel, "transitionend");

  info("Click on the toggle button again");
  EventUtils.synthesizeMouseAtCenter(button, {},
    inspector.panelDoc.defaultView);

  yield onTransitionEnd;
  ok(!button.classList.contains("pane-collapsed"), "The button is in expanded state");
  ok(!panel.classList.contains("pane-collapsed"), "The panel is in expanded state");
});
