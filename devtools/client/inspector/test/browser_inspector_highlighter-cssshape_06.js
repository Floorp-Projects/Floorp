/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test that shapes are updated correctly on mouse events in transform mode.

const TEST_URL = URL_ROOT + "doc_inspector_highlighter_cssshapes.html";
const HIGHLIGHTER_TYPE = "ShapesHighlighter";
const SHAPE_SELECTORS = ["#polygon-transform", "#circle", "#ellipse", "#inset"];

add_task(async function() {
  const env = await openInspectorForURL(TEST_URL);
  const helper = await getHighlighterHelperFor(HIGHLIGHTER_TYPE)(env);
  const {testActor, inspector} = env;
  const view = selectRuleView(inspector);
  const highlighters = view.highlighters;
  const config = { inspector, view, highlighters, testActor, helper };

  await testTranslate(config);
  await testScale(config);
});

async function setup(config) {
  const { inspector, view, selector, property, options } = config;
  await selectNode(selector, inspector);
  await toggleShapesHighlighter(view, selector, property, true, options);
}

async function teardown(config) {
  const { view, selector, property } = config;
  info(`Turn off shapes highlighter for ${selector}`);
  await toggleShapesHighlighter(view, selector, property, false);
}

async function testTranslate(config) {
  const { testActor, helper, highlighters } = config;
  const options = { transformMode: true };
  const property = "clip-path";

  for (const selector of SHAPE_SELECTORS) {
    await setup({selector, property, options, ...config});
    const { mouse } = helper;

    const { center, width, height } = await getBoundingBoxInPx({selector, ...config});
    const [x, y] = center;
    const dx = width / 10;
    const dy = height / 10;
    let onShapeChangeApplied;

    info(`Translating ${selector}`);
    onShapeChangeApplied = highlighters.once("shapes-highlighter-changes-applied");
    await mouse.down(x, y, selector);
    await mouse.move(x + dx, y + dy, selector);
    await mouse.up(x + dx, y + dy, selector);
    await onShapeChangeApplied;

    let newBB = await getBoundingBoxInPx({selector, ...config});
    isnot(newBB.center[0], x, `${selector} translated on y axis`);
    isnot(newBB.center[1], y, `${selector} translated on x axis`);

    info(`Translating ${selector} back`);
    onShapeChangeApplied = highlighters.once("shapes-highlighter-changes-applied");
    await mouse.down(x + dx, y + dy, selector);
    await mouse.move(x, y, selector);
    await mouse.up(x, y, selector);
    await testActor.reflow();
    await onShapeChangeApplied;

    newBB = await getBoundingBoxInPx({selector, ...config});
    is(newBB.center[0], x, `${selector} translated back on x axis`);
    is(newBB.center[1], y, `${selector} translated back on y axis`);

    await teardown({selector, property, ...config});
  }
}

async function testScale(config) {
  const { testActor, helper, highlighters } = config;
  const options = { transformMode: true };
  const property = "clip-path";

  for (const selector of SHAPE_SELECTORS) {
    await setup({selector, property, options, ...config});
    const { mouse } = helper;

    const { nw, width,
          height, center } = await getBoundingBoxInPx({selector, ...config});

    // if the top or left edges are not visible, move the shape so it is.
    if (nw[0] < 0 || nw[1] < 0) {
      const [x, y] = center;
      const dx = Math.max(0, -nw[0]);
      const dy = Math.max(0, -nw[1]);
      await mouse.down(x, y, selector);
      await mouse.move(x + dx, y + dy, selector);
      await mouse.up(x + dx, y + dy, selector);
      await testActor.reflow();
      nw[0] += dx;
      nw[1] += dy;
    }
    const dx = width / 10;
    const dy = height / 10;
    let onShapeChangeApplied;

    info("Scaling from nw");
    onShapeChangeApplied = highlighters.once("shapes-highlighter-changes-applied");
    await mouse.down(nw[0], nw[1], selector);
    await mouse.move(nw[0] + dx, nw[1] + dy, selector);
    await mouse.up(nw[0] + dx, nw[1] + dy, selector);
    await testActor.reflow();
    await onShapeChangeApplied;

    const nwBB = await getBoundingBoxInPx({selector, ...config});
    isnot(nwBB.nw[0], nw[0], `${selector} nw moved right after nw scale`);
    isnot(nwBB.nw[1], nw[1], `${selector} nw moved down after nw scale`);
    isnot(nwBB.width, width, `${selector} width reduced after nw scale`);
    isnot(nwBB.height, height, `${selector} height reduced after nw scale`);

    info("Scaling from ne");
    onShapeChangeApplied = highlighters.once("shapes-highlighter-changes-applied");
    await mouse.down(nwBB.ne[0], nwBB.ne[1], selector);
    await mouse.move(nwBB.ne[0] - dx, nwBB.ne[1] + dy, selector);
    await mouse.up(nwBB.ne[0] - dx, nwBB.ne[1] + dy, selector);
    await testActor.reflow();
    await onShapeChangeApplied;

    const neBB = await getBoundingBoxInPx({selector, ...config});
    isnot(neBB.ne[0], nwBB.ne[0], `${selector} ne moved right after ne scale`);
    isnot(neBB.ne[1], nwBB.ne[1], `${selector} ne moved down after ne scale`);
    isnot(neBB.width, nwBB.width, `${selector} width reduced after ne scale`);
    isnot(neBB.height, nwBB.height, `${selector} height reduced after ne scale`);

    info("Scaling from sw");
    onShapeChangeApplied = highlighters.once("shapes-highlighter-changes-applied");
    await mouse.down(neBB.sw[0], neBB.sw[1], selector);
    await mouse.move(neBB.sw[0] + dx, neBB.sw[1] - dy, selector);
    await mouse.up(neBB.sw[0] + dx, neBB.sw[1] - dy, selector);
    await testActor.reflow();
    await onShapeChangeApplied;

    const swBB = await getBoundingBoxInPx({selector, ...config});
    isnot(swBB.sw[0], neBB.sw[0], `${selector} sw moved right after sw scale`);
    isnot(swBB.sw[1], neBB.sw[1], `${selector} sw moved down after sw scale`);
    isnot(swBB.width, neBB.width, `${selector} width reduced after sw scale`);
    isnot(swBB.height, neBB.height, `${selector} height reduced after sw scale`);

    info("Scaling from se");
    onShapeChangeApplied = highlighters.once("shapes-highlighter-changes-applied");
    await mouse.down(swBB.se[0], swBB.se[1], selector);
    await mouse.move(swBB.se[0] - dx, swBB.se[1] - dy, selector);
    await mouse.up(swBB.se[0] - dx, swBB.se[1] - dy, selector);
    await testActor.reflow();
    await onShapeChangeApplied;

    const seBB = await getBoundingBoxInPx({selector, ...config});
    isnot(seBB.se[0], swBB.se[0], `${selector} se moved right after se scale`);
    isnot(seBB.se[1], swBB.se[1], `${selector} se moved down after se scale`);
    isnot(seBB.width, swBB.width, `${selector} width reduced after se scale`);
    isnot(seBB.height, swBB.height, `${selector} height reduced after se scale`);

    await teardown({selector, property, ...config});
  }
}

async function getBoundingBoxInPx(config) {
  const { testActor, selector, inspector, highlighters } = config;
  const quads = await testActor.getAllAdjustedQuads(selector);
  const { width, height } = quads.content[0].bounds;
  const highlightedNode = await getNodeFront(selector, inspector);
  const computedStyle = await inspector.pageStyle.getComputed(highlightedNode);
  const paddingTop = parseFloat(computedStyle["padding-top"].value);
  const paddingLeft = parseFloat(computedStyle["padding-left"].value);
  // path is always of form "Mx y Lx y Lx y Lx y Z", where x/y are numbers
  const path = await testActor.getHighlighterNodeAttribute(
    "shapes-bounding-box", "d", highlighters.highlighters[HIGHLIGHTER_TYPE]);
  const coords = path.replace(/[MLZ]/g, "").split(" ").map((n, i) => {
    return i % 2 === 0 ? paddingLeft + width * n / 100 : paddingTop + height * n / 100;
  });

  const nw = [coords[0], coords[1]];
  const ne = [coords[2], coords[3]];
  const se = [coords[4], coords[5]];
  const sw = [coords[6], coords[7]];
  const center = [(nw[0] + se[0]) / 2, (nw[1] + se[1]) / 2];
  const shapeWidth = Math.sqrt((ne[0] - nw[0]) ** 2 + (ne[1] - nw[1]) ** 2);
  const shapeHeight = Math.sqrt((sw[0] - nw[0]) ** 2 + (sw[1] - nw[1]) ** 2);

  return { nw, ne, se, sw, center, width: shapeWidth, height: shapeHeight };
}
