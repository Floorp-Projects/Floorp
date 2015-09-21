/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that the sidebar panel toggle button actually works.

add_task(function* () {
  let {inspector} = yield openInspectorForURL("about:blank");

  let button = inspector.panelDoc.getElementById("inspector-pane-toggle");
  let panel = inspector.panelDoc.querySelector("#inspector-sidebar");

  ok(!button.hasAttribute("pane-collapsed"), "The button is in expanded state");

  info("Listen to the end of the animation on the sidebar panel");
  let onTransitionEnd = once(panel, "transitionend");

  info("Click on the toggle button");
  EventUtils.synthesizeMouseAtCenter(button, {type: "mousedown"},
    inspector.panelDoc.defaultView);

  yield onTransitionEnd;
  ok(button.hasAttribute("pane-collapsed"), "The button is in collapsed state");
  ok(panel.hasAttribute("pane-collapsed"), "The panel is in collapsed state");

  info("Listen again to the end of the animation on the sidebar panel");
  onTransitionEnd = once(panel, "transitionend");

  info("Click on the toggle button again");
  EventUtils.synthesizeMouseAtCenter(button, {type: "mousedown"},
    inspector.panelDoc.defaultView);

  yield onTransitionEnd;
  ok(!button.hasAttribute("pane-collapsed"), "The button is in expanded state");
  ok(!panel.hasAttribute("pane-collapsed"), "The panel is in expanded state");
});
