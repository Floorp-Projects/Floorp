/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test that shapes are updated correctly on mouse events.

const TEST_URL = URL_ROOT + "doc_inspector_highlighter_cssshapes.html";
const HIGHLIGHTER_TYPE = "ShapesHighlighter";

add_task(function* () {
  let env = yield openInspectorForURL(TEST_URL);
  let helper = yield getHighlighterHelperFor(HIGHLIGHTER_TYPE)(env);
  let {testActor, inspector} = env;
  let view = selectRuleView(inspector);
  let highlighters = view.highlighters;

  let config = {inspector, view, highlighters, testActor, helper};

  yield testPolygonMovePoint(config);
  yield testPolygonAddPoint(config);
  yield testPolygonRemovePoint(config);
  yield testCircleMoveCenter(config);
  yield testEllipseMoveRadius(config);
  yield testInsetMoveEdges(config);

  helper.finalize();
});

function* getComputedPropertyValue(selector, property, inspector) {
  let highlightedNode = yield getNodeFront(selector, inspector);
  let computedStyle = yield inspector.pageStyle.getComputed(highlightedNode);
  return computedStyle[property].value;
}

function* setup(config) {
  const { view, selector, property, inspector } = config;
  info(`Turn on shapes highlighter for ${selector}`);
  yield selectNode(selector, inspector);
  return yield toggleShapesHighlighter(view, selector, property, true);
}

function* teardown(config) {
  const { view, selector, property } = config;
  info(`Turn off shapes highlighter for ${selector}`);
  return yield toggleShapesHighlighter(view, selector, property, false);
}

function* testPolygonMovePoint(config) {
  const {inspector, view, highlighters, testActor, helper} = config;
  const selector = "#polygon";
  const property = "clip-path";

  yield setup({selector, property, ...config});

  let points = yield testActor.getHighlighterNodeAttribute(
    "shapes-polygon", "points", highlighters.highlighters[HIGHLIGHTER_TYPE]);
  let [x, y] = points.split(" ")[0].split(",");
  let quads = yield testActor.getAllAdjustedQuads(selector);
  let { top, left, width, height } = quads.border[0].bounds;
  x = left + width * x / 100;
  y = top + height * y / 100;
  let dx = width / 10;
  let dy = height / 10;

  let onRuleViewChanged = view.once("ruleview-changed");
  info("Moving first polygon point");
  let { mouse } = helper;
  yield mouse.down(x, y);
  yield mouse.move(x + dx, y + dy);
  yield mouse.up();
  yield testActor.reflow();
  info("Waiting for rule view changed from shape change");
  yield onRuleViewChanged;

  let definition = yield getComputedPropertyValue(selector, property, inspector);
  ok(definition.includes(`${dx}px ${dy}px`), `Point moved to ${dx}px ${dy}px`);

  yield teardown({selector, property, ...config});
}

function* testPolygonAddPoint(config) {
  const {inspector, view, highlighters, testActor, helper} = config;
  const selector = "#polygon";
  const property = "clip-path";

  yield setup({selector, property, ...config});

  // Move first point to have same x as second point, then double click between
  // the two points to add a new one.
  let points = yield testActor.getHighlighterNodeAttribute(
    "shapes-polygon", "points", highlighters.highlighters[HIGHLIGHTER_TYPE]);
  let pointsArray = points.split(" ");
  let quads = yield testActor.getAllAdjustedQuads(selector);
  let { top, left, width, height } = quads.border[0].bounds;
  let [x1, y1] = pointsArray[0].split(",");
  let [x2, y2] = pointsArray[1].split(",");
  x1 = left + width * x1 / 100;
  x2 = left + width * x2 / 100;
  y1 = top + height * y1 / 100;
  y2 = top + height * y2 / 100;

  let { mouse } = helper;
  yield mouse.down(x1, y1);
  yield mouse.move(x2, y1);
  yield mouse.up();
  yield testActor.reflow();

  let newPointX = x2;
  let newPointY = (y1 + y2) / 2;
  let options = {
    selector: ":root",
    x: newPointX,
    y: newPointY,
    center: false,
    options: {clickCount: 2}
  };

  let onRuleViewChanged = view.once("ruleview-changed");
  info("Adding new polygon point");
  yield testActor.synthesizeMouse(options);
  yield testActor.reflow();
  info("Waiting for rule view changed from shape change");
  yield onRuleViewChanged;

  // Decimal precision for coordinates with percentage units is 2
  let precision = 2;
  // Round to the desired decimal precision and cast to Number to remove trailing zeroes.
  newPointX = Number((newPointX * 100 / width).toFixed(precision));
  newPointY = Number((newPointY * 100 / height).toFixed(precision));
  let definition = yield getComputedPropertyValue(selector, property, inspector);
  ok(definition.includes(`${newPointX}% ${newPointY}%`),
     "Point successfuly added");

  yield teardown({selector, property, ...config});
}

function* testPolygonRemovePoint(config) {
  const {inspector, highlighters, testActor, helper} = config;
  const selector = "#polygon";
  const property = "clip-path";

  yield setup({selector, property, ...config});

  let points = yield testActor.getHighlighterNodeAttribute(
    "shapes-polygon", "points", highlighters.highlighters[HIGHLIGHTER_TYPE]);
  let [x, y] = points.split(" ")[0].split(",");
  let quads = yield testActor.getAllAdjustedQuads(selector);
  let { top, left, width, height } = quads.border[0].bounds;

  let options = {
    selector: ":root",
    x: left + width * x / 100,
    y: top + height * y / 100,
    center: false,
    options: {clickCount: 2}
  };

  info("Move mouse over first point in highlighter");
  let onEventHandled = highlighters.once("highlighter-event-handled");
  let { mouse } = helper;
  yield mouse.move(options.x, options.y);
  yield onEventHandled;
  let markerHidden = yield testActor.getHighlighterNodeAttribute(
    "shapes-marker-hover", "hidden", highlighters.highlighters[HIGHLIGHTER_TYPE]);
  ok(!markerHidden, "Marker on highlighter is visible");

  info("Double click on first point in highlighter");
  let onShapeChangeApplied = highlighters.once("shapes-highlighter-changes-applied");
  yield testActor.synthesizeMouse(options);
  info("Waiting for shape changes to apply");
  yield onShapeChangeApplied;
  let definition = yield getComputedPropertyValue(selector, property, inspector);
  ok(!definition.includes(`${x}% ${y}%`), "Point successfully removed");

  yield teardown({selector, property, ...config});
}

function* testCircleMoveCenter(config) {
  const {inspector, highlighters, testActor, helper} = config;
  const selector = "#circle";
  const property = "clip-path";

  yield setup({selector, property, ...config});

  let cx = parseFloat(yield testActor.getHighlighterNodeAttribute(
    "shapes-ellipse", "cx", highlighters.highlighters[HIGHLIGHTER_TYPE]));
  let cy = parseFloat(yield testActor.getHighlighterNodeAttribute(
    "shapes-ellipse", "cy", highlighters.highlighters[HIGHLIGHTER_TYPE]));
  let quads = yield testActor.getAllAdjustedQuads(selector);
  let { width, height } = quads.border[0].bounds;
  let cxPixel = width * cx / 100;
  let cyPixel = height * cy / 100;
  let dx = width / 10;
  let dy = height / 10;

  let onShapeChangeApplied = highlighters.once("shapes-highlighter-changes-applied");
  info("Moving circle center");
  let { mouse } = helper;
  yield mouse.down(cxPixel, cyPixel, selector);
  yield mouse.move(cxPixel + dx, cyPixel + dy, selector);
  yield mouse.up(cxPixel + dx, cyPixel + dy, selector);
  yield onShapeChangeApplied;

  let definition = yield getComputedPropertyValue(selector, property, inspector);
  ok(definition.includes(`at ${cx + 10}% ${cy + 10}%`),
     "Circle center successfully moved");

  yield teardown({selector, property, ...config});
}

function* testEllipseMoveRadius(config) {
  const {inspector, highlighters, testActor, helper} = config;
  const selector = "#ellipse";
  const property = "clip-path";

  yield setup({selector, property, ...config});

  let rx = parseFloat(yield testActor.getHighlighterNodeAttribute(
    "shapes-ellipse", "rx", highlighters.highlighters[HIGHLIGHTER_TYPE]));
  let ry = parseFloat(yield testActor.getHighlighterNodeAttribute(
    "shapes-ellipse", "ry", highlighters.highlighters[HIGHLIGHTER_TYPE]));
  let cx = parseFloat(yield testActor.getHighlighterNodeAttribute(
    "shapes-ellipse", "cx", highlighters.highlighters[HIGHLIGHTER_TYPE]));
  let cy = parseFloat(yield testActor.getHighlighterNodeAttribute(
    "shapes-ellipse", "cy", highlighters.highlighters[HIGHLIGHTER_TYPE]));
  let quads = yield testActor.getAllAdjustedQuads("#ellipse");
  let { width, height } = quads.content[0].bounds;
  let highlightedNode = yield getNodeFront(selector, inspector);
  let computedStyle = yield inspector.pageStyle.getComputed(highlightedNode);
  let paddingTop = parseFloat(computedStyle["padding-top"].value);
  let paddingLeft = parseFloat(computedStyle["padding-left"].value);
  let cxPixel = paddingLeft + width * cx / 100;
  let cyPixel = paddingTop + height * cy / 100;
  let rxPixel = cxPixel + width * rx / 100;
  let ryPixel = cyPixel + height * ry / 100;
  let dx = width / 10;
  let dy = height / 10;

  let { mouse } = helper;
  info("Moving ellipse rx");
  yield mouse.down(rxPixel, cyPixel, selector);
  yield mouse.move(rxPixel + dx, cyPixel, selector);
  yield mouse.up(rxPixel + dx, cyPixel, selector);
  yield testActor.reflow();

  info("Moving ellipse ry");
  let onShapeChangeApplied = highlighters.once("shapes-highlighter-changes-applied");
  yield mouse.down(cxPixel, ryPixel, selector);
  yield mouse.move(cxPixel, ryPixel - dy, selector);
  yield mouse.up(cxPixel, ryPixel - dy, selector);
  yield testActor.reflow();
  yield onShapeChangeApplied;

  let definition = yield getComputedPropertyValue(selector, property, inspector);
  ok(definition.includes(`${rx + 10}% ${ry - 10}%`),
     "Ellipse radiuses successfully moved");

  yield teardown({selector, property, ...config});
}

function* testInsetMoveEdges(config) {
  const {inspector, highlighters, testActor, helper} = config;
  const selector = "#inset";
  const property = "clip-path";

  yield setup({selector, property, ...config});

  let x = parseFloat(yield testActor.getHighlighterNodeAttribute(
    "shapes-rect", "x", highlighters.highlighters[HIGHLIGHTER_TYPE]));
  let y = parseFloat(yield testActor.getHighlighterNodeAttribute(
    "shapes-rect", "y", highlighters.highlighters[HIGHLIGHTER_TYPE]));
  let width = parseFloat(yield testActor.getHighlighterNodeAttribute(
    "shapes-rect", "width", highlighters.highlighters[HIGHLIGHTER_TYPE]));
  let height = parseFloat(yield testActor.getHighlighterNodeAttribute(
    "shapes-rect", "height", highlighters.highlighters[HIGHLIGHTER_TYPE]));
  let quads = yield testActor.getAllAdjustedQuads(selector);
  let { width: elemWidth, height: elemHeight } = quads.content[0].bounds;

  let left = elemWidth * x / 100;
  let top = elemHeight * y / 100;
  let right = left + elemWidth * width / 100;
  let bottom = top + elemHeight * height / 100;
  let xCenter = (left + right) / 2;
  let yCenter = (top + bottom) / 2;
  let dx = elemWidth / 10;
  let dy = elemHeight / 10;
  let { mouse } = helper;

  info("Moving inset top");
  yield mouse.down(xCenter, top, selector);
  yield mouse.move(xCenter, top + dy, selector);
  yield mouse.up(xCenter, top + dy, selector);
  yield testActor.reflow();

  info("Moving inset bottom");
  yield mouse.down(xCenter, bottom, selector);
  yield mouse.move(xCenter, bottom + dy, selector);
  yield mouse.up(xCenter, bottom + dy, selector);
  yield testActor.reflow();

  info("Moving inset left");
  yield mouse.down(left, yCenter, selector);
  yield mouse.move(left + dx, yCenter, selector);
  yield mouse.up(left + dx, yCenter, selector);
  yield testActor.reflow();

  info("Moving inset right");
  let onShapeChangeApplied = highlighters.once("shapes-highlighter-changes-applied");
  yield mouse.down(right, yCenter, selector);
  yield mouse.move(right + dx, yCenter, selector);
  yield mouse.up(right + dx, yCenter, selector);
  yield onShapeChangeApplied;

  let definition = yield getComputedPropertyValue(selector, property, inspector);
  ok(definition.includes(
    `${top + dy}px ${elemWidth - right - dx}px ${100 - y - height - 10}% ${x + 10}%`),
     "Inset edges successfully moved");

  yield teardown({selector, property, ...config});
}
