/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test that shapes are updated correctly on mouse events.

const TEST_URL = URL_ROOT + "doc_inspector_highlighter_cssshapes.html";
const HIGHLIGHTER_TYPE = "ShapesHighlighter";

add_task(function* () {
  let inspector = yield openInspectorForURL(TEST_URL);
  let helper = yield getHighlighterHelperFor(HIGHLIGHTER_TYPE)(inspector);
  let {testActor} = inspector;

  yield testPolygonMovePoint(testActor, helper);
  yield testPolygonAddPoint(testActor, helper);
  yield testPolygonRemovePoint(testActor, helper);
  yield testCircleMoveCenter(testActor, helper);
  yield testEllipseMoveRadius(testActor, helper);
  yield testInsetMoveEdges(testActor, helper);

  helper.finalize();
});

function* testPolygonMovePoint(testActor, helper) {
  info("Displaying polygon");
  yield helper.show("#polygon", {mode: "cssClipPath"});
  let { mouse, highlightedNode } = helper;

  let points = yield helper.getElementAttribute("shapes-polygon", "points");
  let [x, y] = points.split(" ")[0].split(",");
  let quads = yield testActor.getAllAdjustedQuads("#polygon");
  let { top, left, width, height } = quads.border[0].bounds;
  x = left + width * x / 100;
  y = top + height * y / 100;
  let dx = width / 10;
  let dy = height / 10;

  info("Moving first polygon point");
  yield mouse.down(x, y);
  yield mouse.move(x + dx, y + dy);
  yield mouse.up();
  yield testActor.reflow();

  let computedStyle = yield highlightedNode.getComputedStyle();
  let definition = computedStyle["clip-path"].value;
  ok(definition.includes(`${dx}px ${dy}px`), `Point moved to ${dx}px ${dy}px`);
}

function* testPolygonAddPoint(testActor, helper) {
  yield helper.show("#polygon", {mode: "cssClipPath"});
  let { mouse, highlightedNode } = helper;

  // Move first point to have same x as second point, then double click between
  // the two points to add a new one.
  let points = yield helper.getElementAttribute("shapes-polygon", "points");
  let pointsArray = points.split(" ");
  let quads = yield testActor.getAllAdjustedQuads("#polygon");
  let { top, left, width, height } = quads.border[0].bounds;
  let [x1, y1] = pointsArray[0].split(",");
  let [x2, y2] = pointsArray[1].split(",");
  x1 = left + width * x1 / 100;
  x2 = left + width * x2 / 100;
  y1 = top + height * y1 / 100;
  y2 = top + height * y2 / 100;

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

  info("Adding new polygon point");
  yield testActor.synthesizeMouse(options);
  yield testActor.reflow();

  let computedStyle = yield highlightedNode.getComputedStyle();
  let definition = computedStyle["clip-path"].value;
  ok(definition.includes(`${newPointX * 100 / width}% ${newPointY * 100 / height}%`),
     "Point successfuly added");
}

function* testPolygonRemovePoint(testActor, helper) {
  yield helper.show("#polygon", {mode: "cssClipPath"});
  let { highlightedNode } = helper;

  let points = yield helper.getElementAttribute("shapes-polygon", "points");
  let [x, y] = points.split(" ")[0].split(",");
  let quads = yield testActor.getAllAdjustedQuads("#polygon");
  let { top, left, width, height } = quads.border[0].bounds;

  let options = {
    selector: ":root",
    x: left + width * x / 100,
    y: top + height * y / 100,
    center: false,
    options: {clickCount: 2}
  };

  info("Removing first polygon point");
  yield testActor.synthesizeMouse(options);
  yield testActor.reflow();

  let computedStyle = yield highlightedNode.getComputedStyle();
  let definition = computedStyle["clip-path"].value;
  ok(!definition.includes(`${x}% ${y}%`), "Point successfully removed");
}

function* testCircleMoveCenter(testActor, helper) {
  yield helper.show("#circle", {mode: "cssClipPath"});
  let { mouse, highlightedNode } = helper;

  let cx = parseFloat(yield helper.getElementAttribute("shapes-ellipse", "cx"));
  let cy = parseFloat(yield helper.getElementAttribute("shapes-ellipse", "cy"));
  let quads = yield testActor.getAllAdjustedQuads("#circle");
  let { width, height } = quads.border[0].bounds;
  let cxPixel = width * cx / 100;
  let cyPixel = height * cy / 100;
  let dx = width / 10;
  let dy = height / 10;

  info("Moving circle center");
  yield mouse.down(cxPixel, cyPixel, "#circle");
  yield mouse.move(cxPixel + dx, cyPixel + dy, "#circle");
  yield mouse.up(cxPixel + dx, cyPixel + dy, "#circle");
  yield testActor.reflow();

  let computedStyle = yield highlightedNode.getComputedStyle();
  let definition = computedStyle["clip-path"].value;
  ok(definition.includes(`at ${cx + 10}% ${cy + 10}%`),
     "Circle center successfully moved");
}

function* testEllipseMoveRadius(testActor, helper) {
  yield helper.show("#ellipse", {mode: "cssClipPath"});
  let { mouse, highlightedNode } = helper;

  let rx = parseFloat(yield helper.getElementAttribute("shapes-ellipse", "rx"));
  let ry = parseFloat(yield helper.getElementAttribute("shapes-ellipse", "ry"));
  let cx = parseFloat(yield helper.getElementAttribute("shapes-ellipse", "cx"));
  let cy = parseFloat(yield helper.getElementAttribute("shapes-ellipse", "cy"));
  let quads = yield testActor.getAllAdjustedQuads("#ellipse");
  let { width, height } = quads.content[0].bounds;
  let computedStyle = yield highlightedNode.getComputedStyle();
  let paddingTop = parseFloat(computedStyle["padding-top"].value);
  let paddingLeft = parseFloat(computedStyle["padding-left"].value);
  let cxPixel = paddingLeft + width * cx / 100;
  let cyPixel = paddingTop + height * cy / 100;
  let rxPixel = cxPixel + width * rx / 100;
  let ryPixel = cyPixel + height * ry / 100;
  let dx = width / 10;
  let dy = height / 10;

  info("Moving ellipse rx");
  yield mouse.down(rxPixel, cyPixel, "#ellipse");
  yield mouse.move(rxPixel + dx, cyPixel, "#ellipse");
  yield mouse.up(rxPixel + dx, cyPixel, "#ellipse");
  yield testActor.reflow();

  info("Moving ellipse ry");
  yield mouse.down(cxPixel, ryPixel, "#ellipse");
  yield mouse.move(cxPixel, ryPixel - dy, "#ellipse");
  yield mouse.up(cxPixel, ryPixel - dy, "#ellipse");
  yield testActor.reflow();

  computedStyle = yield highlightedNode.getComputedStyle();
  let definition = computedStyle["clip-path"].value;
  ok(definition.includes(`${rx + 10}% ${ry - 10}%`),
     "Ellipse radiuses successfully moved");
}

function* testInsetMoveEdges(testActor, helper) {
  yield helper.show("#inset", {mode: "cssClipPath"});
  let { mouse, highlightedNode } = helper;

  let x = parseFloat(yield helper.getElementAttribute("shapes-rect", "x"));
  let y = parseFloat(yield helper.getElementAttribute("shapes-rect", "y"));
  let width = parseFloat(yield helper.getElementAttribute("shapes-rect", "width"));
  let height = parseFloat(yield helper.getElementAttribute("shapes-rect", "height"));
  let quads = yield testActor.getAllAdjustedQuads("#inset");
  let { width: elemWidth, height: elemHeight } = quads.content[0].bounds;

  let left = elemWidth * x / 100;
  let top = elemHeight * y / 100;
  let right = left + elemWidth * width / 100;
  let bottom = top + elemHeight * height / 100;
  let xCenter = (left + right) / 2;
  let yCenter = (top + bottom) / 2;
  let dx = elemWidth / 10;
  let dy = elemHeight / 10;

  info("Moving inset top");
  yield mouse.down(xCenter, top, "#inset");
  yield mouse.move(xCenter, top + dy, "#inset");
  yield mouse.up(xCenter, top + dy, "#inset");
  yield testActor.reflow();

  info("Moving inset bottom");
  yield mouse.down(xCenter, bottom, "#inset");
  yield mouse.move(xCenter, bottom + dy, "#inset");
  yield mouse.up(xCenter, bottom + dy, "#inset");
  yield testActor.reflow();

  info("Moving inset left");
  yield mouse.down(left, yCenter, "#inset");
  yield mouse.move(left + dx, yCenter, "#inset");
  yield mouse.up(left + dx, yCenter, "#inset");
  yield testActor.reflow();

  info("Moving inset right");
  yield mouse.down(right, yCenter, "#inset");
  yield mouse.move(right + dx, yCenter, "#inset");
  yield mouse.up(right + dx, yCenter, "#inset");
  yield testActor.reflow();

  let computedStyle = yield highlightedNode.getComputedStyle();
  let definition = computedStyle["clip-path"].value;
  ok(definition.includes(
    `${top + dy}px ${elemWidth - right - dx}px ${100 - y - height - 10}% ${x + 10}%`),
     "Inset edges successfully moved");
}
