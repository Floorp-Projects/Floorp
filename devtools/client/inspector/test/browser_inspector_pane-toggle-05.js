/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
* Test the keyboard navigation for the pane toggle using
* space and enter
*/

add_task(function* () {
  let {inspector} = yield openInspectorForURL("about:blank", "side");
  let panel = inspector.panelDoc.querySelector("#inspector-sidebar-container");
  let button = inspector.panelDoc.querySelector(".sidebar-toggle");

  ok(!panel.classList.contains("pane-collapsed"), "The panel is in expanded state");

  yield togglePane(button, "Press on the toggle button", panel, "VK_RETURN");
  ok(panel.classList.contains("pane-collapsed"), "The panel is in collapsed state");

  yield togglePane(button, "Press on the toggle button to expand the panel again",
    panel, "VK_SPACE");
  ok(!panel.classList.contains("pane-collapsed"), "The panel is in expanded state");
});

function* togglePane(button, message, panel, keycode) {
  let onTransitionEnd = once(panel, "transitionend");
  info(message);
  button.focus();
  EventUtils.synthesizeKey(keycode, {});
  yield onTransitionEnd;
}
