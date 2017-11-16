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

    let { nw, width, height, center } = yield getBoundingBoxInPx(testActor, helper, shape);

    // if the top or left edges are not visible, move the shape so it is.
    if (nw[0] < 0 || nw[1] < 0) {
      let [x, y] = center;
      let dx = Math.max(0, -nw[0]);
      let dy = Math.max(0, -nw[1]);
      yield mouse.down(x, y, shape);
      yield mouse.move(x + dx, y + dy, shape);
      yield mouse.up(x + dx, y + dy, shape);
      yield testActor.reflow();
      nw[0] += dx;
      nw[1] += dy;
    }
    let dx = width / 10;
    let dy = height / 10;

    info("Scaling from w");
    yield mouse.down(nw[0], center[1], shape);
    yield mouse.move(nw[0] + dx, center[1], shape);
    yield mouse.up(nw[0] + dx, center[1], shape);
    yield testActor.reflow();

    let wBB = yield getBoundingBoxInPx(testActor, helper, shape);
    isnot(wBB.nw[0], nw[0], `${shape} nw moved right after w scale`);
    is(wBB.nw[1], nw[1], `${shape} nw not moved down after w scale`);
    isnot(wBB.width, width, `${shape} width reduced after w scale`);
    is(wBB.height, height, `${shape} height not reduced after w scale`);

    info("Scaling from e");
    yield mouse.down(wBB.ne[0], center[1], shape);
    yield mouse.move(wBB.ne[0] - dx, center[1], shape);
    yield mouse.up(wBB.ne[0] - dx, center[1], shape);
    yield testActor.reflow();

    let eBB = yield getBoundingBoxInPx(testActor, helper, shape);
    isnot(eBB.ne[0], wBB.ne[0], `${shape} ne moved left after e scale`);
    is(eBB.ne[1], wBB.ne[1], `${shape} ne not moved down after e scale`);
    isnot(eBB.width, wBB.width, `${shape} width reduced after e scale`);
    is(eBB.height, wBB.height, `${shape} height not reduced after e scale`);

    info("Scaling from s");
    yield mouse.down(eBB.center[0], eBB.sw[1], shape);
    yield mouse.move(eBB.center[0], eBB.sw[1] - dy, shape);
    yield mouse.up(eBB.center[0], eBB.sw[1] - dy, shape);
    yield testActor.reflow();

    let sBB = yield getBoundingBoxInPx(testActor, helper, shape);
    is(sBB.sw[0], eBB.sw[0], `${shape} sw not moved right after w scale`);
    isnot(sBB.sw[1], eBB.sw[1], `${shape} sw moved down after w scale`);
    is(sBB.width, eBB.width, `${shape} width not reduced after w scale`);
    isnot(sBB.height, eBB.height, `${shape} height reduced after w scale`);

    info("Scaling from n");
    yield mouse.down(sBB.center[0], sBB.nw[1], shape);
    yield mouse.move(sBB.center[0], sBB.nw[1] + dy, shape);
    yield mouse.up(sBB.center[0], sBB.nw[1] + dy, shape);
    yield testActor.reflow();

    let nBB = yield getBoundingBoxInPx(testActor, helper, shape);
    is(nBB.nw[0], sBB.nw[0], `${shape} nw not moved right after n scale`);
    isnot(nBB.nw[1], sBB.nw[1], `${shape} nw moved down after n scale`);
    is(nBB.width, sBB.width, `${shape} width reduced after n scale`);
    isnot(nBB.height, sBB.height, `${shape} height not reduced after n scale`);
  }
}

function* getBoundingBoxInPx(testActor, helper, shape = "#polygon") {
  let quads = yield testActor.getAllAdjustedQuads(shape);
  let { width, height } = quads.content[0].bounds;
  let computedStyle = yield helper.highlightedNode.getComputedStyle();
  let paddingTop = parseFloat(computedStyle["padding-top"].value);
  let paddingLeft = parseFloat(computedStyle["padding-left"].value);

  // path is always of form "Mx y Lx y Lx y Lx y Z", where x/y are numbers
  let path = yield helper.getElementAttribute("shapes-bounding-box", "d");
  let coords = path.replace(/[MLZ]/g, "").split(" ").map((n, i) => {
    return i % 2 === 0 ? paddingLeft + width * n / 100 : paddingTop + height * n / 100;
  });

  let nw = [coords[0], coords[1]];
  let ne = [coords[2], coords[3]];
  let se = [coords[4], coords[5]];
  let sw = [coords[6], coords[7]];
  let center = [(nw[0] + se[0]) / 2, (nw[1] + se[1]) / 2];
  let shapeWidth = Math.sqrt((ne[0] - nw[0]) ** 2 + (ne[1] - nw[1]) ** 2);
  let shapeHeight = Math.sqrt((sw[0] - nw[0]) ** 2 + (sw[1] - nw[1]) ** 2);

  return { nw, ne, se, sw, center, width: shapeWidth, height: shapeHeight };
}
