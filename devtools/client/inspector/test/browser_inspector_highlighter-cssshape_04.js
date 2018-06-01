/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test that shapes are updated correctly on mouse events.

const TEST_URL = URL_ROOT + "doc_inspector_highlighter_cssshapes.html";
const HIGHLIGHTER_TYPE = "ShapesHighlighter";

add_task(async function() {
  const env = await openInspectorForURL(TEST_URL);
  const helper = await getHighlighterHelperFor(HIGHLIGHTER_TYPE)(env);
  const {testActor, inspector} = env;
  const view = selectRuleView(inspector);
  const highlighters = view.highlighters;

  const config = {inspector, view, highlighters, testActor, helper};

  await testPolygonMovePoint(config);
  await testPolygonAddPoint(config);
  await testPolygonRemovePoint(config);
  await testCircleMoveCenter(config);
  await testEllipseMoveRadius(config);
  await testInsetMoveEdges(config);

  helper.finalize();
});

async function getComputedPropertyValue(selector, property, inspector) {
  const highlightedNode = await getNodeFront(selector, inspector);
  const computedStyle = await inspector.pageStyle.getComputed(highlightedNode);
  return computedStyle[property].value;
}

async function setup(config) {
  const { view, selector, property, inspector } = config;
  info(`Turn on shapes highlighter for ${selector}`);
  await selectNode(selector, inspector);
  await toggleShapesHighlighter(view, selector, property, true);
}

async function teardown(config) {
  const { view, selector, property } = config;
  info(`Turn off shapes highlighter for ${selector}`);
  await toggleShapesHighlighter(view, selector, property, false);
}

async function testPolygonMovePoint(config) {
  const {inspector, view, highlighters, testActor, helper} = config;
  const selector = "#polygon";
  const property = "clip-path";

  await setup({selector, property, ...config});

  const points = await testActor.getHighlighterNodeAttribute(
    "shapes-polygon", "points", highlighters.highlighters[HIGHLIGHTER_TYPE]);
  let [x, y] = points.split(" ")[0].split(",");
  const quads = await testActor.getAllAdjustedQuads(selector);
  const { top, left, width, height } = quads.border[0].bounds;
  x = left + width * x / 100;
  y = top + height * y / 100;
  const dx = width / 10;
  const dyPercent = 10;
  const dy = height / dyPercent;

  const onRuleViewChanged = view.once("ruleview-changed");
  info("Moving first polygon point");
  const { mouse } = helper;
  await mouse.down(x, y);
  await mouse.move(x + dx, y + dy);
  await mouse.up();
  await testActor.reflow();
  info("Waiting for rule view changed from shape change");
  await onRuleViewChanged;

  const definition = await getComputedPropertyValue(selector, property, inspector);
  ok(definition.includes(`${dx}px ${dyPercent}%`),
    `Point moved to ${dx}px ${dyPercent}%`);

  await teardown({selector, property, ...config});
}

async function testPolygonAddPoint(config) {
  const {inspector, view, highlighters, testActor, helper} = config;
  const selector = "#polygon";
  const property = "clip-path";

  await setup({selector, property, ...config});

  // Move first point to have same x as second point, then double click between
  // the two points to add a new one.
  const points = await testActor.getHighlighterNodeAttribute(
    "shapes-polygon", "points", highlighters.highlighters[HIGHLIGHTER_TYPE]);
  const pointsArray = points.split(" ");
  const quads = await testActor.getAllAdjustedQuads(selector);
  const { top, left, width, height } = quads.border[0].bounds;
  let [x1, y1] = pointsArray[0].split(",");
  let [x2, y2] = pointsArray[1].split(",");
  x1 = left + width * x1 / 100;
  x2 = left + width * x2 / 100;
  y1 = top + height * y1 / 100;
  y2 = top + height * y2 / 100;

  const { mouse } = helper;
  await mouse.down(x1, y1);
  await mouse.move(x2, y1);
  await mouse.up();
  await testActor.reflow();

  let newPointX = x2;
  let newPointY = (y1 + y2) / 2;
  const options = {
    selector: ":root",
    x: newPointX,
    y: newPointY,
    center: false,
    options: {clickCount: 2}
  };

  const onRuleViewChanged = view.once("ruleview-changed");
  info("Adding new polygon point");
  await testActor.synthesizeMouse(options);
  await testActor.reflow();
  info("Waiting for rule view changed from shape change");
  await onRuleViewChanged;

  // Decimal precision for coordinates with percentage units is 2
  const precision = 2;
  // Round to the desired decimal precision and cast to Number to remove trailing zeroes.
  newPointX = Number((newPointX * 100 / width).toFixed(precision));
  newPointY = Number((newPointY * 100 / height).toFixed(precision));
  const definition = await getComputedPropertyValue(selector, property, inspector);
  ok(definition.includes(`${newPointX}% ${newPointY}%`),
     "Point successfuly added");

  await teardown({selector, property, ...config});
}

async function testPolygonRemovePoint(config) {
  const {inspector, highlighters, testActor, helper} = config;
  const selector = "#polygon";
  const property = "clip-path";

  await setup({selector, property, ...config});

  const points = await testActor.getHighlighterNodeAttribute(
    "shapes-polygon", "points", highlighters.highlighters[HIGHLIGHTER_TYPE]);
  const [x, y] = points.split(" ")[0].split(",");
  const quads = await testActor.getAllAdjustedQuads(selector);
  const { top, left, width, height } = quads.border[0].bounds;

  const options = {
    selector: ":root",
    x: left + width * x / 100,
    y: top + height * y / 100,
    center: false,
    options: {clickCount: 2}
  };

  info("Move mouse over first point in highlighter");
  const onEventHandled = highlighters.once("highlighter-event-handled");
  const { mouse } = helper;
  await mouse.move(options.x, options.y);
  await onEventHandled;
  const markerHidden = await testActor.getHighlighterNodeAttribute(
    "shapes-marker-hover", "hidden", highlighters.highlighters[HIGHLIGHTER_TYPE]);
  ok(!markerHidden, "Marker on highlighter is visible");

  info("Double click on first point in highlighter");
  const onShapeChangeApplied = highlighters.once("shapes-highlighter-changes-applied");
  await testActor.synthesizeMouse(options);
  info("Waiting for shape changes to apply");
  await onShapeChangeApplied;
  const definition = await getComputedPropertyValue(selector, property, inspector);
  ok(!definition.includes(`${x}% ${y}%`), "Point successfully removed");

  await teardown({selector, property, ...config});
}

async function testCircleMoveCenter(config) {
  const {inspector, highlighters, testActor, helper} = config;
  const selector = "#circle";
  const property = "clip-path";

  const onShapeChangeApplied = highlighters.once("shapes-highlighter-changes-applied");
  await setup({selector, property, ...config});

  const cx = parseFloat(await testActor.getHighlighterNodeAttribute(
    "shapes-ellipse", "cx", highlighters.highlighters[HIGHLIGHTER_TYPE]));
  const cy = parseFloat(await testActor.getHighlighterNodeAttribute(
    "shapes-ellipse", "cy", highlighters.highlighters[HIGHLIGHTER_TYPE]));
  const quads = await testActor.getAllAdjustedQuads(selector);
  const { width, height } = quads.border[0].bounds;
  const cxPixel = width * cx / 100;
  const cyPixel = height * cy / 100;
  const dx = width / 10;
  const dy = height / 10;

  info("Moving circle center");
  const { mouse } = helper;
  await mouse.down(cxPixel, cyPixel, selector);
  await mouse.move(cxPixel + dx, cyPixel + dy, selector);
  await mouse.up(cxPixel + dx, cyPixel + dy, selector);
  await testActor.reflow();
  info("Waiting for shape changes to apply");
  await onShapeChangeApplied;

  const definition = await getComputedPropertyValue(selector, property, inspector);
  ok(definition.includes(`at ${cx + 10}% ${cy + 10}%`),
     "Circle center successfully moved");

  await teardown({selector, property, ...config});
}

async function testEllipseMoveRadius(config) {
  const {inspector, highlighters, testActor, helper} = config;
  const selector = "#ellipse";
  const property = "clip-path";

  await setup({selector, property, ...config});

  const rx = parseFloat(await testActor.getHighlighterNodeAttribute(
    "shapes-ellipse", "rx", highlighters.highlighters[HIGHLIGHTER_TYPE]));
  const ry = parseFloat(await testActor.getHighlighterNodeAttribute(
    "shapes-ellipse", "ry", highlighters.highlighters[HIGHLIGHTER_TYPE]));
  const cx = parseFloat(await testActor.getHighlighterNodeAttribute(
    "shapes-ellipse", "cx", highlighters.highlighters[HIGHLIGHTER_TYPE]));
  const cy = parseFloat(await testActor.getHighlighterNodeAttribute(
    "shapes-ellipse", "cy", highlighters.highlighters[HIGHLIGHTER_TYPE]));
  const quads = await testActor.getAllAdjustedQuads("#ellipse");
  const { width, height } = quads.content[0].bounds;
  const highlightedNode = await getNodeFront(selector, inspector);
  const computedStyle = await inspector.pageStyle.getComputed(highlightedNode);
  const paddingTop = parseFloat(computedStyle["padding-top"].value);
  const paddingLeft = parseFloat(computedStyle["padding-left"].value);
  const cxPixel = paddingLeft + width * cx / 100;
  const cyPixel = paddingTop + height * cy / 100;
  const rxPixel = cxPixel + width * rx / 100;
  const ryPixel = cyPixel + height * ry / 100;
  const dx = width / 10;
  const dy = height / 10;

  const { mouse } = helper;
  info("Moving ellipse rx");
  await mouse.down(rxPixel, cyPixel, selector);
  await mouse.move(rxPixel + dx, cyPixel, selector);
  await mouse.up(rxPixel + dx, cyPixel, selector);
  await testActor.reflow();

  info("Moving ellipse ry");
  const onShapeChangeApplied = highlighters.once("shapes-highlighter-changes-applied");
  await mouse.down(cxPixel, ryPixel, selector);
  await mouse.move(cxPixel, ryPixel - dy, selector);
  await mouse.up(cxPixel, ryPixel - dy, selector);
  await testActor.reflow();
  await onShapeChangeApplied;

  const definition = await getComputedPropertyValue(selector, property, inspector);
  ok(definition.includes(`${rx + 10}% ${ry - 10}%`),
     "Ellipse radiuses successfully moved");

  await teardown({selector, property, ...config});
}

async function testInsetMoveEdges(config) {
  const {inspector, highlighters, testActor, helper} = config;
  const selector = "#inset";
  const property = "clip-path";

  await setup({selector, property, ...config});

  const x = parseFloat(await testActor.getHighlighterNodeAttribute(
    "shapes-rect", "x", highlighters.highlighters[HIGHLIGHTER_TYPE]));
  const y = parseFloat(await testActor.getHighlighterNodeAttribute(
    "shapes-rect", "y", highlighters.highlighters[HIGHLIGHTER_TYPE]));
  const width = parseFloat(await testActor.getHighlighterNodeAttribute(
    "shapes-rect", "width", highlighters.highlighters[HIGHLIGHTER_TYPE]));
  const height = parseFloat(await testActor.getHighlighterNodeAttribute(
    "shapes-rect", "height", highlighters.highlighters[HIGHLIGHTER_TYPE]));
  const quads = await testActor.getAllAdjustedQuads(selector);
  const { width: elemWidth, height: elemHeight } = quads.content[0].bounds;

  const left = elemWidth * x / 100;
  const top = elemHeight * y / 100;
  const right = left + elemWidth * width / 100;
  const bottom = top + elemHeight * height / 100;
  const xCenter = (left + right) / 2;
  const yCenter = (top + bottom) / 2;
  const dx = elemWidth / 10;
  const dy = elemHeight / 10;
  const { mouse } = helper;

  info("Moving inset top");
  let onShapeChangeApplied = highlighters.once("shapes-highlighter-changes-applied");
  await mouse.down(xCenter, top, selector);
  await mouse.move(xCenter, top + dy, selector);
  await mouse.up(xCenter, top + dy, selector);
  await testActor.reflow();
  await onShapeChangeApplied;

  // TODO: Test bottom inset marker after Bug 1456777 is fixed.
  // Bug 1456777 - https://bugzilla.mozilla.org/show_bug.cgi?id=1456777
  // The test element is larger than the viewport when tests run in headless mode.
  // When moved, the bottom marker value is getting clamped to the viewport.

  info("Moving inset left");
  onShapeChangeApplied = highlighters.once("shapes-highlighter-changes-applied");
  await mouse.down(left, yCenter, selector);
  await mouse.move(left + dx, yCenter, selector);
  await mouse.up(left + dx, yCenter, selector);
  await testActor.reflow();
  await onShapeChangeApplied;

  info("Moving inset right");
  onShapeChangeApplied = highlighters.once("shapes-highlighter-changes-applied");
  await mouse.down(right, yCenter, selector);
  await mouse.move(right + dx, yCenter, selector);
  await mouse.up(right + dx, yCenter, selector);
  await testActor.reflow();
  await onShapeChangeApplied;

  const definition = await getComputedPropertyValue(selector, property, inspector);

  // NOTE: No change to bottom inset until Bug 1456777 is fixed.
  ok(definition.includes(
    `${top + dy}px ${elemWidth - right - dx}px ${100 - y - height}% ${x + 10}%`),
     "Inset edges successfully moved");

  await teardown({selector, property, ...config});
}
