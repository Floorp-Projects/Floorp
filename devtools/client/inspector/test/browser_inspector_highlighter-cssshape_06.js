/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test that shapes are updated correctly on mouse events in transform mode.

const TEST_URL = URL_ROOT + "doc_inspector_highlighter_cssshapes.html";
const HIGHLIGHTER_TYPE = "ShapesHighlighter";
const SHAPE_IDS = ["#polygon-transform", "#circle", "#ellipse", "#inset"];

add_task(function* () {
  let inspector = yield openInspectorForURL(TEST_URL);
  let helper = yield getHighlighterHelperFor(HIGHLIGHTER_TYPE)(inspector);
  let {testActor} = inspector;

  yield testTranslate(testActor, helper);
  yield testScale(testActor, helper);

  helper.finalize();
});

function* testTranslate(testActor, helper) {
  for (let shape of SHAPE_IDS) {
    info(`Displaying ${shape}`);
    yield helper.show(shape, {mode: "cssClipPath", transformMode: true});
    let { mouse } = helper;

    let { top, left, width, height } = yield getBoundingBoxInPx(testActor, helper, shape);
    let x = left + width / 2;
    let y = top + height / 2;
    let dx = width / 10;
    let dy = height / 10;

    info(`Translating ${shape}`);
    yield mouse.down(x, y, shape);
    yield mouse.move(x + dx, y + dy, shape);
    yield mouse.up(x + dx, y + dy, shape);
    yield testActor.reflow();

    let newBB = yield getBoundingBoxInPx(testActor, helper);
    isnot(newBB.top, top, `${shape} translated on y axis`);
    isnot(newBB.left, left, `${shape} translated on x axis`);

    info(`Translating ${shape} back`);
    yield mouse.down(x + dx, y + dy, shape);
    yield mouse.move(x, y, shape);
    yield mouse.up(x, y, shape);
    yield testActor.reflow();

    newBB = yield getBoundingBoxInPx(testActor, helper, shape);
    is(newBB.top, top, `${shape} translated back on y axis`);
    is(newBB.left, left, `${shape} translated back on x axis`);
  }
}

function* testScale(testActor, helper) {
  for (let shape of SHAPE_IDS) {
    info(`Displaying ${shape}`);
    yield helper.show(shape, {mode: "cssClipPath", transformMode: true});
    let { mouse } = helper;

    let { top, left, width, height } = yield getBoundingBoxInPx(testActor, helper, shape);

    // if the top or left edges are not visible, move the shape so it is.
    if (top < 0 || left < 0) {
      let x = left + width / 2;
      let y = top + height / 2;
      let dx = Math.max(0, -left);
      let dy = Math.max(0, -top);
      yield mouse.down(x, y, shape);
      yield mouse.move(x + dx, y + dy, shape);
      yield mouse.up(x + dx, y + dy, shape);
      yield testActor.reflow();
      left += dx;
      top += dy;
    }
    let dx = width / 10;
    let dy = height / 10;

    info("Scaling from nw");
    yield mouse.down(left, top, shape);
    yield mouse.move(left + dx, top + dy, shape);
    yield mouse.up(left + dx, top + dy, shape);
    yield testActor.reflow();

    let nwBB = yield getBoundingBoxInPx(testActor, helper, shape);
    isnot(nwBB.top, top, `${shape} top moved down after nw scale`);
    isnot(nwBB.left, left, `${shape} left moved right after nw scale`);
    isnot(nwBB.width, width, `${shape} width reduced after nw scale`);
    isnot(nwBB.height, height, `${shape} height reduced after nw scale`);

    info("Scaling from ne");
    yield mouse.down(nwBB.left + nwBB.width, nwBB.top, shape);
    yield mouse.move(nwBB.left + nwBB.width - dx, nwBB.top + dy, shape);
    yield mouse.up(nwBB.left + nwBB.width - dx, nwBB.top + dy, shape);
    yield testActor.reflow();

    let neBB = yield getBoundingBoxInPx(testActor, helper, shape);
    isnot(neBB.top, nwBB.top, `${shape} top moved down after ne scale`);
    is(neBB.left, nwBB.left, `${shape} left not moved right after ne scale`);
    isnot(neBB.width, nwBB.width, `${shape} width reduced after ne scale`);
    isnot(neBB.height, nwBB.height, `${shape} height reduced after ne scale`);

    info("Scaling from sw");
    yield mouse.down(neBB.left, neBB.top + neBB.height, shape);
    yield mouse.move(neBB.left + dx, neBB.top + neBB.height - dy, shape);
    yield mouse.up(neBB.left + dx, neBB.top + neBB.height - dy, shape);
    yield testActor.reflow();

    let swBB = yield getBoundingBoxInPx(testActor, helper, shape);
    is(swBB.top, neBB.top, `${shape} top not moved down after sw scale`);
    isnot(swBB.left, neBB.left, `${shape} left moved right after sw scale`);
    isnot(swBB.width, neBB.width, `${shape} width reduced after sw scale`);
    isnot(swBB.height, neBB.height, `${shape} height reduced after sw scale`);

    info("Scaling from se");
    yield mouse.down(swBB.left + swBB.width, swBB.top + swBB.height, shape);
    yield mouse.move(swBB.left + swBB.width - dx, swBB.top + swBB.height - dy, shape);
    yield mouse.up(swBB.left + swBB.width - dx, swBB.top + swBB.height - dy, shape);
    yield testActor.reflow();

    let seBB = yield getBoundingBoxInPx(testActor, helper, shape);
    is(seBB.top, swBB.top, `${shape} top not moved down after se scale`);
    is(seBB.left, swBB.left, `${shape} left not moved right after se scale`);
    isnot(seBB.width, swBB.width, `${shape} width reduced after se scale`);
    isnot(seBB.height, swBB.height, `${shape} height reduced after se scale`);
  }
}

function* getBoundingBoxInPx(testActor, helper, shape = "#polygon") {
  let bbTop = parseFloat(yield helper.getElementAttribute("shapes-bounding-box", "y"));
  let bbLeft = parseFloat(yield helper.getElementAttribute("shapes-bounding-box", "x"));
  let bbWidth = parseFloat(yield helper.getElementAttribute("shapes-bounding-box",
    "width"));
  let bbHeight = parseFloat(yield helper.getElementAttribute("shapes-bounding-box",
    "height"));

  let quads = yield testActor.getAllAdjustedQuads(shape);
  let { width, height } = quads.content[0].bounds;
  let computedStyle = yield helper.highlightedNode.getComputedStyle();
  let paddingTop = parseFloat(computedStyle["padding-top"].value);
  let paddingLeft = parseFloat(computedStyle["padding-left"].value);

  return {
    top: paddingTop + height * bbTop / 100,
    left: paddingLeft + width * bbLeft / 100,
    width: width * bbWidth / 100,
    height: height * bbHeight / 100
  };
}
