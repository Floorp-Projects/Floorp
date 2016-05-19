/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Make sure the canvases are created correctly

"use strict";

const CanvasUtils = require("devtools/client/memory/components/tree-map/canvas-utils");
const D3_SCRIPT = '<script type="application/javascript" ' +
                  'src="chrome://devtools/content/shared/vendor/d3.js>';
const TEST_URL = `data:text/html,<html><body>${D3_SCRIPT}</body></html>`;

this.test = makeMemoryTest(TEST_URL, function* ({ tab, panel }) {
  const document = panel.panelWin.document;
  const window = panel.panelWin;
  const div = document.createElement("div");

  Object.assign(div.style, {
    width: "100px",
    height: "200px",
    position: "absolute"
  });

  document.body.appendChild(div);

  info("Create the canvases");

  let canvases = new CanvasUtils(div, 0);

  info("Test the shape of the returned object");

  is(typeof canvases, "object", "Canvases create an object");
  is(typeof canvases.emit, "function", "Decorated with an EventEmitter");
  is(typeof canvases.on, "function", "Decorated with an EventEmitter");
  is(div.children[0], canvases.container, "Div has the container");
  ok(canvases.main.canvas instanceof window.HTMLCanvasElement,
    "Creates the main canvas");
  ok(canvases.zoom.canvas instanceof window.HTMLCanvasElement,
    "Creates the zoom canvas");
  ok(canvases.main.ctx instanceof window.CanvasRenderingContext2D,
    "Creates the main canvas context");
  ok(canvases.zoom.ctx instanceof window.CanvasRenderingContext2D,
    "Creates the zoom canvas context");

  info("Test resizing");

  let timesResizeCalled = 0;
  canvases.on("resize", function () {
    timesResizeCalled++;
  });

  let main = canvases.main.canvas;
  let zoom = canvases.zoom.canvas;
  let ratio = window.devicePixelRatio;

  is(main.width, 100 * ratio,
    "Main canvas width is the same as the parent div");
  is(main.height, 200 * ratio,
    "Main canvas height is the same as the parent div");
  is(zoom.width, 100 * ratio,
    "Zoom canvas width is the same as the parent div");
  is(zoom.height, 200 * ratio,
    "Zoom canvas height is the same as the parent div");
  is(timesResizeCalled, 0,
    "Resize was not emitted");

  div.style.width = "500px";
  div.style.height = "700px";

  window.dispatchEvent(new Event("resize"));

  is(main.width, 500 * ratio,
    "Main canvas width is resized to be the same as the parent div");
  is(main.height, 700 * ratio,
    "Main canvas height is resized to be the same as the parent div");
  is(zoom.width, 500 * ratio,
    "Zoom canvas width is resized to be the same as the parent div");
  is(zoom.height, 700 * ratio,
    "Zoom canvas height is resized to be the same as the parent div");
  is(timesResizeCalled, 1,
    "'resize' was emitted was emitted");

  div.style.width = "1100px";
  div.style.height = "1300px";

  canvases.destroy();
  window.dispatchEvent(new Event("resize"));

  is(main.width, 500 * ratio,
    "Main canvas width is not resized after destroy");
  is(main.height, 700 * ratio,
    "Main canvas height is not resized after destroy");
  is(zoom.width, 500 * ratio,
    "Zoom canvas width is not resized after destroy");
  is(zoom.height, 700 * ratio,
    "Zoom canvas height is not resized after destroy");
  is(timesResizeCalled, 1,
    "onResize was not called again");

  document.body.removeChild(div);
});
