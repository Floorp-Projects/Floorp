/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test that shapes are updated correctly on mouse events in transform mode.

const TEST_URL = URL_ROOT + "doc_inspector_highlighter_cssshapes.html";
const HIGHLIGHTER_TYPE = "ShapesHighlighter";
const SHAPE_IDS = ["#polygon-transform", "#circle", "#ellipse", "#inset"];

add_task(async function() {
  let inspector = await openInspectorForURL(TEST_URL);
  let helper = await getHighlighterHelperFor(HIGHLIGHTER_TYPE)(inspector);
  let {testActor} = inspector;

  await testTranslate(testActor, helper);
  await testScale(testActor, helper);

  helper.finalize();
});

async function testTranslate(testActor, helper) {
  for (let shape of SHAPE_IDS) {
    info(`Displaying ${shape}`);
    await helper.show(shape, {mode: "cssClipPath", transformMode: true});
    let { mouse } = helper;

    let { center, width, height } = await getBoundingBoxInPx(testActor, helper, shape);
    let [x, y] = center;
    let dx = width / 10;
    let dy = height / 10;

    info(`Translating ${shape}`);
    await mouse.down(x, y, shape);
    await mouse.move(x + dx, y + dy, shape);
    await mouse.up(x + dx, y + dy, shape);
    await testActor.reflow();

    let newBB = await getBoundingBoxInPx(testActor, helper);
    isnot(newBB.center[0], x, `${shape} translated on y axis`);
    isnot(newBB.center[1], y, `${shape} translated on x axis`);

    info(`Translating ${shape} back`);
    await mouse.down(x + dx, y + dy, shape);
    await mouse.move(x, y, shape);
    await mouse.up(x, y, shape);
    await testActor.reflow();

    newBB = await getBoundingBoxInPx(testActor, helper, shape);
    is(newBB.center[0], x, `${shape} translated back on x axis`);
    is(newBB.center[1], y, `${shape} translated back on y axis`);
  }
}

async function testScale(testActor, helper) {
  for (let shape of SHAPE_IDS) {
    info(`Displaying ${shape}`);
    await helper.show(shape, {mode: "cssClipPath", transformMode: true});
    let { mouse } = helper;

    let { nw, width,
          height, center } = await getBoundingBoxInPx(testActor, helper, shape);

    // if the top or left edges are not visible, move the shape so it is.
    if (nw[0] < 0 || nw[1] < 0) {
      let [x, y] = center;
      let dx = Math.max(0, -nw[0]);
      let dy = Math.max(0, -nw[1]);
      await mouse.down(x, y, shape);
      await mouse.move(x + dx, y + dy, shape);
      await mouse.up(x + dx, y + dy, shape);
      await testActor.reflow();
      nw[0] += dx;
      nw[1] += dy;
    }
    let dx = width / 10;
    let dy = height / 10;

    info("Scaling from nw");
    await mouse.down(nw[0], nw[1], shape);
    await mouse.move(nw[0] + dx, nw[1] + dy, shape);
    await mouse.up(nw[0] + dx, nw[1] + dy, shape);
    await testActor.reflow();

    let nwBB = await getBoundingBoxInPx(testActor, helper, shape);
    isnot(nwBB.nw[0], nw[0], `${shape} nw moved right after nw scale`);
    isnot(nwBB.nw[1], nw[1], `${shape} nw moved down after nw scale`);
    isnot(nwBB.width, width, `${shape} width reduced after nw scale`);
    isnot(nwBB.height, height, `${shape} height reduced after nw scale`);

    info("Scaling from ne");
    await mouse.down(nwBB.ne[0], nwBB.ne[1], shape);
    await mouse.move(nwBB.ne[0] - dx, nwBB.ne[1] + dy, shape);
    await mouse.up(nwBB.ne[0] - dx, nwBB.ne[1] + dy, shape);
    await testActor.reflow();

    let neBB = await getBoundingBoxInPx(testActor, helper, shape);
    isnot(neBB.ne[0], nwBB.ne[0], `${shape} ne moved right after ne scale`);
    isnot(neBB.ne[1], nwBB.ne[1], `${shape} ne moved down after ne scale`);
    isnot(neBB.width, nwBB.width, `${shape} width reduced after ne scale`);
    isnot(neBB.height, nwBB.height, `${shape} height reduced after ne scale`);

    info("Scaling from sw");
    await mouse.down(neBB.sw[0], neBB.sw[1], shape);
    await mouse.move(neBB.sw[0] + dx, neBB.sw[1] - dy, shape);
    await mouse.up(neBB.sw[0] + dx, neBB.sw[1] - dy, shape);
    await testActor.reflow();

    let swBB = await getBoundingBoxInPx(testActor, helper, shape);
    isnot(swBB.sw[0], neBB.sw[0], `${shape} sw moved right after sw scale`);
    isnot(swBB.sw[1], neBB.sw[1], `${shape} sw moved down after sw scale`);
    isnot(swBB.width, neBB.width, `${shape} width reduced after sw scale`);
    isnot(swBB.height, neBB.height, `${shape} height reduced after sw scale`);

    info("Scaling from se");
    await mouse.down(swBB.se[0], swBB.se[1], shape);
    await mouse.move(swBB.se[0] - dx, swBB.se[1] - dy, shape);
    await mouse.up(swBB.se[0] - dx, swBB.se[1] - dy, shape);
    await testActor.reflow();

    let seBB = await getBoundingBoxInPx(testActor, helper, shape);
    isnot(seBB.se[0], swBB.se[0], `${shape} se moved right after se scale`);
    isnot(seBB.se[1], swBB.se[1], `${shape} se moved down after se scale`);
    isnot(seBB.width, swBB.width, `${shape} width reduced after se scale`);
    isnot(seBB.height, swBB.height, `${shape} height reduced after se scale`);
  }
}

async function getBoundingBoxInPx(testActor, helper, shape = "#polygon") {
  let quads = await testActor.getAllAdjustedQuads(shape);
  let { width, height } = quads.content[0].bounds;
  let computedStyle = await helper.highlightedNode.getComputedStyle();
  let paddingTop = parseFloat(computedStyle["padding-top"].value);
  let paddingLeft = parseFloat(computedStyle["padding-left"].value);

  // path is always of form "Mx y Lx y Lx y Lx y Z", where x/y are numbers
  let path = await helper.getElementAttribute("shapes-bounding-box", "d");
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
