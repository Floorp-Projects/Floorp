/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URL = "about:logo";

function dragElementBy(selector, x, y, win) {
  let el = win.document.querySelector(selector);
  let rect = el.getBoundingClientRect();
  let startPoint = [ rect.left + rect.width / 2, rect.top + rect.height / 2 ];
  let endPoint = [ startPoint[0] + x, startPoint[1] + y ];

  EventUtils.synthesizeMouseAtPoint(...startPoint, { type: "mousedown" }, win);
  EventUtils.synthesizeMouseAtPoint(...endPoint, { type: "mousemove" }, win);
  EventUtils.synthesizeMouseAtPoint(...endPoint, { type: "mouseup" }, win);
}

addRDMTask(TEST_URL, function* ({ ui, manager }) {
  ok(ui, "An instance of the RDM should be attached to the tab.");
  yield setViewportSize(ui, manager, 300, 300);
  let win = ui.toolWindow;

  // Do horizontal + vertical resize
  let resized = waitForViewportResizeTo(ui, 320, 320);
  dragElementBy(".viewport-resize-handle", 10, 10, win);
  yield resized;

  // Do horizontal resize
  let hResized = waitForViewportResizeTo(ui, 300, 320);
  dragElementBy(".viewport-horizontal-resize-handle", -10, -10, win);
  yield hResized;

  // Do vertical resize
  let vResized = waitForViewportResizeTo(ui, 300, 300);
  dragElementBy(".viewport-vertical-resize-handle", -10, -10, win);
  yield vResized;
});
