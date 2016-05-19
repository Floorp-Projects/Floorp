/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URL = "data:text/html;charset=utf-8,";

function getElRect(selector, win) {
  let el = win.document.querySelector(selector);
  return el.getBoundingClientRect();
}

/**
 * Drag an element identified by 'selector' by [x,y] amount. Returns
 * the rect of the dragged element as it was before drag.
 */
function dragElementBy(selector, x, y, win) {
  let rect = getElRect(selector, win);
  let startPoint = [ rect.left + rect.width / 2, rect.top + rect.height / 2 ];
  let endPoint = [ startPoint[0] + x, startPoint[1] + y ];

  EventUtils.synthesizeMouseAtPoint(...startPoint, { type: "mousedown" }, win);
  EventUtils.synthesizeMouseAtPoint(...endPoint, { type: "mousemove" }, win);
  EventUtils.synthesizeMouseAtPoint(...endPoint, { type: "mouseup" }, win);

  return rect;
}

function* testViewportResize(ui, selector, moveBy,
                             expectedViewportSize, expectedHandleMove) {
  let win = ui.toolWindow;

  let resized = waitForViewportResizeTo(ui, ...expectedViewportSize);
  let startRect = dragElementBy(selector, ...moveBy, win);
  yield resized;

  let endRect = getElRect(selector, win);
  is(endRect.left - startRect.left, expectedHandleMove[0],
    `The x move of ${selector} is as expected`);
  is(endRect.top - startRect.top, expectedHandleMove[1],
    `The y move of ${selector} is as expected`);
}

addRDMTask(TEST_URL, function* ({ ui, manager }) {
  ok(ui, "An instance of the RDM should be attached to the tab.");
  yield setViewportSize(ui, manager, 300, 300);

  // Do horizontal + vertical resize
  yield testViewportResize(ui, ".viewport-resize-handle",
    [10, 10], [320, 310], [10, 10]);

  // Do horizontal resize
  yield testViewportResize(ui, ".viewport-horizontal-resize-handle",
    [-10, 10], [300, 310], [-10, 0]);

  // Do vertical resize
  yield testViewportResize(ui, ".viewport-vertical-resize-handle",
    [-10, -10], [300, 300], [0, -10], ui);
});
