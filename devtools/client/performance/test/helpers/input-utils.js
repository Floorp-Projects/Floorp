/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

exports.HORIZONTAL_AXIS = 1;
exports.VERTICAL_AXIS = 2;

/**
 * Simulates a command event on an element.
 */
exports.command = (node) => {
  let ev = node.ownerDocument.createEvent("XULCommandEvent");
  ev.initCommandEvent("command", true, true, node.ownerDocument.defaultView, 0, false,
                      false, false, false, null, 0);
  node.dispatchEvent(ev);
};

/**
 * Simulates a click event on a devtools canvas graph.
 */
exports.clickCanvasGraph = (graph, { x, y }) => {
  x = x || 0;
  y = y || 0;
  x /= graph._window.devicePixelRatio;
  y /= graph._window.devicePixelRatio;
  graph._onMouseMove({ testX: x, testY: y });
  graph._onMouseDown({ testX: x, testY: y });
  graph._onMouseUp({ testX: x, testY: y });
};

/**
 * Simulates a drag start event on a devtools canvas graph.
 */
exports.dragStartCanvasGraph = (graph, { x, y }) => {
  x = x || 0;
  y = y || 0;
  x /= graph._window.devicePixelRatio;
  y /= graph._window.devicePixelRatio;
  graph._onMouseMove({ testX: x, testY: y });
  graph._onMouseDown({ testX: x, testY: y });
};

/**
 * Simulates a drag stop event on a devtools canvas graph.
 */
exports.dragStopCanvasGraph = (graph, { x, y }) => {
  x = x || 0;
  y = y || 0;
  x /= graph._window.devicePixelRatio;
  y /= graph._window.devicePixelRatio;
  graph._onMouseMove({ testX: x, testY: y });
  graph._onMouseUp({ testX: x, testY: y });
};

/**
 * Simulates a scroll event on a devtools canvas graph.
 */
exports.scrollCanvasGraph = (graph, { axis, wheel, x, y }) => {
  x = x || 1;
  y = y || 1;
  x /= graph._window.devicePixelRatio;
  y /= graph._window.devicePixelRatio;
  graph._onMouseMove({
    testX: x,
    testY: y
  });
  graph._onMouseWheel({
    testX: x,
    testY: y,
    axis: axis,
    detail: wheel,
    HORIZONTAL_AXIS: exports.HORIZONTAL_AXIS,
    VERTICAL_AXIS: exports.VERTICAL_AXIS
  });
};
