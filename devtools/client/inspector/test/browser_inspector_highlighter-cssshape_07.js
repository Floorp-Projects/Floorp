/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test that shapes are updated correctly for scaling on one axis in transform mode.

const TEST_URL = URL_ROOT + "doc_inspector_highlighter_cssshapes.html";
const HIGHLIGHTER_TYPE = "ShapesHighlighter";
const SHAPE_IDS = ["#polygon-transform", "#ellipse"];

add_task(function* () {
  let inspector = yield openInspectorForURL(TEST_URL);
  let helper = yield getHighlighterHelperFor(HIGHLIGHTER_TYPE)(inspector);
  let {testActor} = inspector;

  yield testOneDimScale(testActor, helper);

  helper.finalize();
});

function* testOneDimScale(testActor, helper) {
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

    info("Scaling from w");
    yield mouse.down(left, top + height / 2, shape);
    yield mouse.move(left + dx, top + height / 2, shape);
    yield mouse.up(left + dx, top + height / 2, shape);
    yield testActor.reflow();

    let wBB = yield getBoundingBoxInPx(testActor, helper, shape);
    is(wBB.top, top, `${shape} top not moved down after w scale`);
    isnot(wBB.left, left, `${shape} left moved right after w scale`);
    isnot(wBB.width, width, `${shape} width reduced after w scale`);
    is(wBB.height, height, `${shape} height not reduced after w scale`);

    info("Scaling from e");
    yield mouse.down(wBB.left + wBB.width, wBB.top + wBB.height / 2, shape);
    yield mouse.move(wBB.left + wBB.width - dx, wBB.top + wBB.height / 2, shape);
    yield mouse.up(wBB.left + wBB.width - dx, wBB.top + wBB.height / 2, shape);
    yield testActor.reflow();

    let eBB = yield getBoundingBoxInPx(testActor, helper, shape);
    is(eBB.top, wBB.top, `${shape} top not moved down after e scale`);
    is(eBB.left, wBB.left, `${shape} left not moved right after e scale`);
    isnot(eBB.width, wBB.width, `${shape} width reduced after e scale`);
    is(eBB.height, wBB.height, `${shape} height not reduced after e scale`);

    info("Scaling from s");
    yield mouse.down(eBB.left + eBB.width / 2, eBB.top + eBB.height, shape);
    yield mouse.move(eBB.left + eBB.width / 2, eBB.top + eBB.height - dy, shape);
    yield mouse.up(eBB.left + eBB.width / 2, eBB.top + eBB.height - dy, shape);
    yield testActor.reflow();

    let sBB = yield getBoundingBoxInPx(testActor, helper, shape);
    is(sBB.top, eBB.top, `${shape} top not moved down after w scale`);
    is(sBB.left, eBB.left, `${shape} left not moved right after w scale`);
    is(sBB.width, eBB.width, `${shape} width not reduced after w scale`);
    isnot(sBB.height, eBB.height, `${shape} height reduced after w scale`);

    info("Scaling from n");
    yield mouse.down(sBB.left + sBB.width / 2, sBB.top, shape);
    yield mouse.move(sBB.left + sBB.width / 2, sBB.top + dy, shape);
    yield mouse.up(sBB.left + sBB.width / 2, sBB.top + dy, shape);
    yield testActor.reflow();

    let nBB = yield getBoundingBoxInPx(testActor, helper, shape);
    isnot(nBB.top, sBB.top, `${shape} top moved down after n scale`);
    is(nBB.left, sBB.left, `${shape} left not moved right after n scale`);
    is(nBB.width, sBB.width, `${shape} width reduced after n scale`);
    isnot(nBB.height, sBB.height, `${shape} height not reduced after n scale`);
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
