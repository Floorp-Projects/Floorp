/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test that shapes are updated correctly on mouse events in transform mode.

const TEST_URL = URL_ROOT + "doc_inspector_highlighter_cssshapes.html";
const HIGHLIGHTER_TYPE = "ShapesHighlighter";
const SHAPE_SELECTORS = ["#polygon-transform", "#circle", "#ellipse", "#inset"];

add_task(function* () {
  let env = yield openInspectorForURL(TEST_URL);
  let helper = yield getHighlighterHelperFor(HIGHLIGHTER_TYPE)(env);
  let {testActor, inspector} = env;
  let view = selectRuleView(inspector);
  let highlighters = view.highlighters;
  let config = { inspector, view, highlighters, testActor, helper };

  yield testTranslate(config);
  yield testScale(config);
});

function* setup(config) {
  const { inspector, view, selector, property, options } = config;
  yield selectNode(selector, inspector);
  return yield toggleShapesHighlighter(view, selector, property, true, options);
}

function* testTranslate(config) {
  const { testActor, helper, highlighters } = config;
  const options = { transformMode: true };
  const property = "clip-path";

  for (let selector of SHAPE_SELECTORS) {
    yield setup({selector, property, options, ...config});
    let { mouse } = helper;

    let { center, width, height } = yield getBoundingBoxInPx({selector, ...config});
    let [x, y] = center;
    let dx = width / 10;
    let dy = height / 10;
    let onShapeChangeApplied;

    info(`Translating ${selector}`);
    onShapeChangeApplied = highlighters.once("shapes-highlighter-changes-applied");
    yield mouse.down(x, y, selector);
    yield mouse.move(x + dx, y + dy, selector);
    yield mouse.up(x + dx, y + dy, selector);
    yield onShapeChangeApplied;

    let newBB = yield getBoundingBoxInPx({selector, ...config});
    isnot(newBB.center[0], x, `${selector} translated on y axis`);
    isnot(newBB.center[1], y, `${selector} translated on x axis`);

    info(`Translating ${selector} back`);
    onShapeChangeApplied = highlighters.once("shapes-highlighter-changes-applied");
    yield mouse.down(x + dx, y + dy, selector);
    yield mouse.move(x, y, selector);
    yield mouse.up(x, y, selector);
    yield testActor.reflow();
    yield onShapeChangeApplied;

    newBB = yield getBoundingBoxInPx({selector, ...config});
    is(newBB.center[0], x, `${selector} translated back on x axis`);
    is(newBB.center[1], y, `${selector} translated back on y axis`);
  }
}

function* testScale(config) {
  const { testActor, helper, highlighters } = config;
  const options = { transformMode: true };
  const property = "clip-path";

  for (let selector of SHAPE_SELECTORS) {
    yield setup({selector, property, options, ...config});
    let { mouse } = helper;

    let { nw, width,
          height, center } = yield getBoundingBoxInPx({selector, ...config});

    // if the top or left edges are not visible, move the shape so it is.
    if (nw[0] < 0 || nw[1] < 0) {
      let [x, y] = center;
      let dx = Math.max(0, -nw[0]);
      let dy = Math.max(0, -nw[1]);
      yield mouse.down(x, y, selector);
      yield mouse.move(x + dx, y + dy, selector);
      yield mouse.up(x + dx, y + dy, selector);
      yield testActor.reflow();
      nw[0] += dx;
      nw[1] += dy;
    }
    let dx = width / 10;
    let dy = height / 10;
    let onShapeChangeApplied;

    info("Scaling from nw");
    onShapeChangeApplied = highlighters.once("shapes-highlighter-changes-applied");
    yield mouse.down(nw[0], nw[1], selector);
    yield mouse.move(nw[0] + dx, nw[1] + dy, selector);
    yield mouse.up(nw[0] + dx, nw[1] + dy, selector);
    yield testActor.reflow();
    yield onShapeChangeApplied;

    let nwBB = yield getBoundingBoxInPx({selector, ...config});
    isnot(nwBB.nw[0], nw[0], `${selector} nw moved right after nw scale`);
    isnot(nwBB.nw[1], nw[1], `${selector} nw moved down after nw scale`);
    isnot(nwBB.width, width, `${selector} width reduced after nw scale`);
    isnot(nwBB.height, height, `${selector} height reduced after nw scale`);

    info("Scaling from ne");
    onShapeChangeApplied = highlighters.once("shapes-highlighter-changes-applied");
    yield mouse.down(nwBB.ne[0], nwBB.ne[1], selector);
    yield mouse.move(nwBB.ne[0] - dx, nwBB.ne[1] + dy, selector);
    yield mouse.up(nwBB.ne[0] - dx, nwBB.ne[1] + dy, selector);
    yield testActor.reflow();
    yield onShapeChangeApplied;

    let neBB = yield getBoundingBoxInPx({selector, ...config});
    isnot(neBB.ne[0], nwBB.ne[0], `${selector} ne moved right after ne scale`);
    isnot(neBB.ne[1], nwBB.ne[1], `${selector} ne moved down after ne scale`);
    isnot(neBB.width, nwBB.width, `${selector} width reduced after ne scale`);
    isnot(neBB.height, nwBB.height, `${selector} height reduced after ne scale`);

    info("Scaling from sw");
    onShapeChangeApplied = highlighters.once("shapes-highlighter-changes-applied");
    yield mouse.down(neBB.sw[0], neBB.sw[1], selector);
    yield mouse.move(neBB.sw[0] + dx, neBB.sw[1] - dy, selector);
    yield mouse.up(neBB.sw[0] + dx, neBB.sw[1] - dy, selector);
    yield testActor.reflow();
    yield onShapeChangeApplied;

    let swBB = yield getBoundingBoxInPx({selector, ...config});
    isnot(swBB.sw[0], neBB.sw[0], `${selector} sw moved right after sw scale`);
    isnot(swBB.sw[1], neBB.sw[1], `${selector} sw moved down after sw scale`);
    isnot(swBB.width, neBB.width, `${selector} width reduced after sw scale`);
    isnot(swBB.height, neBB.height, `${selector} height reduced after sw scale`);

    info("Scaling from se");
    onShapeChangeApplied = highlighters.once("shapes-highlighter-changes-applied");
    yield mouse.down(swBB.se[0], swBB.se[1], selector);
    yield mouse.move(swBB.se[0] - dx, swBB.se[1] - dy, selector);
    yield mouse.up(swBB.se[0] - dx, swBB.se[1] - dy, selector);
    yield testActor.reflow();
    yield onShapeChangeApplied;

    let seBB = yield getBoundingBoxInPx({selector, ...config});
    isnot(seBB.se[0], swBB.se[0], `${selector} se moved right after se scale`);
    isnot(seBB.se[1], swBB.se[1], `${selector} se moved down after se scale`);
    isnot(seBB.width, swBB.width, `${selector} width reduced after se scale`);
    isnot(seBB.height, swBB.height, `${selector} height reduced after se scale`);
  }
}

function* getBoundingBoxInPx(config) {
  const { testActor, selector, inspector, highlighters } = config;
  let quads = yield testActor.getAllAdjustedQuads(selector);
  let { width, height } = quads.content[0].bounds;
  let highlightedNode = yield getNodeFront(selector, inspector);
  let computedStyle = yield inspector.pageStyle.getComputed(highlightedNode);
  let paddingTop = parseFloat(computedStyle["padding-top"].value);
  let paddingLeft = parseFloat(computedStyle["padding-left"].value);
  // path is always of form "Mx y Lx y Lx y Lx y Z", where x/y are numbers
  let path = yield testActor.getHighlighterNodeAttribute(
    "shapes-bounding-box", "d", highlighters.highlighters[HIGHLIGHTER_TYPE]);
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
