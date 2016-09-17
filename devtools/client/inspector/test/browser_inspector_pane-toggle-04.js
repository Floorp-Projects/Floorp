/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

let { Toolbox } = require("devtools/client/framework/toolbox");

// Test that the dimensions of the collapsed inspector panel are not modified
// when switching from horizontal to vertical layout, which is mandatory to make
// sure the panel remains visually hidden (using negative margins).

add_task(function* () {
  info("Set temporary preferences to ensure a small sidebar width.");
  yield new Promise(resolve => {
    let options = {"set": [
      ["devtools.toolsidebar-width.inspector", 200]
    ]};
    SpecialPowers.pushPrefEnv(options, resolve);
  });

  let { inspector, toolbox } = yield openInspectorForURL("about:blank");
  let button = inspector.panelDoc.querySelector(".sidebar-toggle");
  let panel = inspector.panelDoc.querySelector("#inspector-splitter-box .controlled");

  info("Changing toolbox host to a window.");
  yield toolbox.switchHost(Toolbox.HostType.WINDOW);

  let hostWindow = toolbox._host._window;
  let originalWidth = hostWindow.outerWidth;
  let originalHeight = hostWindow.outerHeight;

  info("Resizing window to switch to the horizontal layout.");
  hostWindow.resizeTo(800, 300);

  // Check the sidebar is expanded when the test starts.
  ok(!panel.classList.contains("pane-collapsed"), "The panel is in expanded state");

  info("Collapse the inspector sidebar.");
  let onTransitionEnd = once(panel, "transitionend");
  EventUtils.synthesizeMouseAtCenter(button, {},
    inspector.panelDoc.defaultView);
  yield onTransitionEnd;

  ok(panel.classList.contains("pane-collapsed"), "The panel is in collapsed state");
  let currentPanelHeight = panel.getBoundingClientRect().height;
  let currentPanelMarginBottom = panel.style.marginBottom;

  info("Resizing window to switch to the vertical layout.");
  hostWindow.resizeTo(300, 800);

  // Check the panel is collapsed, and still has the same dimensions.
  ok(panel.classList.contains("pane-collapsed"), "The panel is still collapsed");
  is(panel.getBoundingClientRect().height, currentPanelHeight,
    "The panel height has not been modified when changing the layout.");
  is(panel.style.marginBottom, currentPanelMarginBottom,
    "The panel margin-bottom has not been modified when changing the layout.");

  info("Restoring window original size.");
  hostWindow.resizeTo(originalWidth, originalHeight);
});

registerCleanupFunction(function () {
  // Restore the host type for other tests.
  Services.prefs.clearUserPref("devtools.toolbox.host");
});
