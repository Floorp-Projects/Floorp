/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that the inspector toggled panel is visible by default, is hidden after
// clicking on the toggle button and remains expanded/collapsed when switching
// hosts.

add_task(function* () {
  info("Open the inspector in a side toolbox host");
  let {toolbox, inspector} = yield openInspectorForURL("about:blank", "side");

  let panel = inspector.panelDoc.querySelector("#inspector-splitter-box .controlled");

  let button = inspector.panelDoc.querySelector(".sidebar-toggle");
  ok(!panel.classList.contains("pane-collapsed"), "The panel is in expanded state");

  info("Listen to the end of the animation on the sidebar panel");
  let onTransitionEnd = once(panel, "transitionend");

  info("Click on the toggle button");
  EventUtils.synthesizeMouseAtCenter(button, {},
    inspector.panelDoc.defaultView);

  yield onTransitionEnd;
  ok(panel.classList.contains("pane-collapsed"), "The panel is in collapsed state");
  ok(!panel.hasAttribute("animated"),
    "The collapsed panel will not perform unwanted animations");

  info("Switch the host to bottom type");
  yield toolbox.switchHost("bottom");
  ok(panel.classList.contains("pane-collapsed"), "The panel is in collapsed state");

  info("Click on the toggle button to expand the panel again");

  onTransitionEnd = once(panel, "transitionend");
  EventUtils.synthesizeMouseAtCenter(button, {},
    inspector.panelDoc.defaultView);
  yield onTransitionEnd;

  ok(!panel.classList.contains("pane-collapsed"), "The panel is in expanded state");
});
