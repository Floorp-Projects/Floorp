/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { setupDraw } = require("./draw");
const DragZoom = require("./drag-zoom");
const CanvasUtils = require("./canvas-utils");

/**
 * Start the tree map visualization
 *
 * @param  {HTMLDivElement} container
 * @param  {Object} report
 *                  the report from a census
 * @param  {Number} debounceRate
 */
module.exports = function startVisualization(parentEl, report,
                                              debounceRate = 60) {
  let window = parentEl.ownerDocument.defaultView;
  let canvases = new CanvasUtils(parentEl, debounceRate);
  let dragZoom = new DragZoom(canvases.container, debounceRate,
                              window.requestAnimationFrame);

  setupDraw(report, canvases, dragZoom);

  return function stopVisualization() {
    canvases.destroy();
    dragZoom.destroy();
  };
};
