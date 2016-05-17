/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test the drag and zooming behavior

"use strict";

const CanvasUtils = require("devtools/client/memory/components/tree-map/canvas-utils");
const DragZoom = require("devtools/client/memory/components/tree-map/drag-zoom");

const TEST_URL = "data:text/html,<html><body></body></html>";
const PIXEL_SCROLL_MODE = 0;
const PIXEL_DELTA = 10;
const MAX_RAF_LOOP = 1000;

this.test = makeMemoryTest(TEST_URL, function* ({ tab, panel }) {
  const panelWin = panel.panelWin;
  const panelDoc = panelWin.document;
  const div = panelDoc.createElement("div");

  Object.assign(div.style, {
    width: "100px",
    height: "200px",
    position: "absolute",
    left:0,
    top:0
  });

  let rafMock = createRAFMock();

  panelDoc.body.appendChild(div);

  let canvases = new CanvasUtils(div, 0);
  let dragZoom = new DragZoom(canvases.container, 0, rafMock.raf);
  let style = canvases.container.style;

  info("Check initial state of dragZoom");
  {
    is(dragZoom.zoom, 0, "Zooming starts at 0");
    is(dragZoom.smoothZoom, 0, "Smoothed zooming starts at 0");
    is(rafMock.timesCalled, 0, "No RAFs have been queued");
    is(style.transform, "translate(0px, 0px) scale(1)",
       "No transforms have been done.");

    canvases.container.dispatchEvent(new WheelEvent("wheel", {
      deltaY: -PIXEL_DELTA,
      deltaMode: PIXEL_SCROLL_MODE
    }));

    is(style.transform, "translate(0px, 0px) scale(1.05)",
       "The div has been slightly scaled.");
    is(dragZoom.zoom, PIXEL_DELTA * dragZoom.ZOOM_SPEED,
      "The zoom was increased");
    ok(floatEquality(dragZoom.smoothZoom, 0.05),
      "The smooth zoom is between the initial value and the target");
    is(rafMock.timesCalled, 1, "A RAF has been queued");
  }

  info("RAF will eventually stop once the smooth values approach the target");
  {
    let i;
    let lastCallCount;
    for (i = 0; i < MAX_RAF_LOOP; i++) {
      if (lastCallCount === rafMock.timesCalled) {
        break;
      }
      lastCallCount = rafMock.timesCalled;
      rafMock.nextFrame();
    }
    is(style.transform, "translate(0px, 0px) scale(1.1)",
       "The scale has been fully applied");
    is(dragZoom.zoom, dragZoom.smoothZoom,
      "The smooth and target zoom values match");
    isnot(MAX_RAF_LOOP, i,
      "The RAF loop correctly stopped");
  }

  info("Dragging correctly translates the div");
  {
    let initialX = dragZoom.translateX;
    let initialY = dragZoom.translateY;
    div.dispatchEvent(new MouseEvent("mousemove", {
      clientX: 10,
      clientY: 10,
    }));
    div.dispatchEvent(new MouseEvent("mousedown"));
    div.dispatchEvent(new MouseEvent("mousemove", {
      clientX: 20,
      clientY: 20,
    }));
    div.dispatchEvent(new MouseEvent("mouseup"));

    is(style.transform, "translate(2.5px, 5px) scale(1.1)",
      "The style is correctly translated");
    ok(floatEquality(dragZoom.translateX, 5),
      "Translate X moved by some pixel amount");
    ok(floatEquality(dragZoom.translateY, 10),
      "Translate Y moved by some pixel amount");
  }

  info("Zooming centers around the mouse");
  {
    canvases.container.dispatchEvent(new WheelEvent("wheel", {
      deltaY: -PIXEL_DELTA,
      deltaMode: PIXEL_SCROLL_MODE
    }));
    // Run through the RAF loop to zoom in towards that value.
    let lastCallCount;
    for (let i = 0; i < MAX_RAF_LOOP; i++) {
      if (lastCallCount === rafMock.timesCalled) {
        break;
      }
      lastCallCount = rafMock.timesCalled;
      rafMock.nextFrame();
    }
    is(style.transform, "translate(8.18182px, 18.1818px) scale(1.2)",
       "Zooming affects the translation to keep the mouse centered");
    ok(floatEquality(dragZoom.translateX, 8.181818181818185),
       "Translate X was affected by the mouse position");
    ok(floatEquality(dragZoom.translateY, 18.18181818181817),
       "Translate Y was affected by the mouse position");
    is(dragZoom.zoom, 0.2, "Zooming starts at 0");
  }

  dragZoom.destroy();

  info("Scroll isn't tracked after destruction");
  {
    let previousZoom = dragZoom.zoom;
    let previousSmoothZoom = dragZoom.smoothZoom;

    canvases.container.dispatchEvent(new WheelEvent("wheel", {
      deltaY: -PIXEL_DELTA,
      deltaMode: PIXEL_SCROLL_MODE
    }));

    is(dragZoom.zoom, previousZoom,
      "The zoom stayed the same");
    is(dragZoom.smoothZoom, previousSmoothZoom,
      "The smooth zoom stayed the same");
  }

  info("Translation isn't tracked after destruction");
  {
    let initialX = dragZoom.translateX;
    let initialY = dragZoom.translateY;

    div.dispatchEvent(new MouseEvent("mousedown"));
    div.dispatchEvent(new MouseEvent("mousemove"), {
      clientX: 40,
      clientY: 40,
    });
    div.dispatchEvent(new MouseEvent("mouseup"));
    is(dragZoom.translateX, initialX,
      "The translationX didn't change");
    is(dragZoom.translateY, initialY,
      "The translationY didn't change");
  }
  panelDoc.body.removeChild(div);
});
