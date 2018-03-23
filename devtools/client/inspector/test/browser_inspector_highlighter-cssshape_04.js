/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test that shapes are updated correctly on mouse events.

const TEST_URL = URL_ROOT + "doc_inspector_highlighter_cssshapes.html";
const HIGHLIGHTER_TYPE = "ShapesHighlighter";

add_task(async function() {
  let inspector = await openInspectorForURL(TEST_URL);
  let helper = await getHighlighterHelperFor(HIGHLIGHTER_TYPE)(inspector);
  let {testActor} = inspector;

  await testPolygonMovePoint(testActor, helper);
  await testPolygonAddPoint(testActor, helper);
  await testPolygonRemovePoint(testActor, helper);
  await testCircleMoveCenter(testActor, helper);
  await testEllipseMoveRadius(testActor, helper);
  await testInsetMoveEdges(testActor, helper);

  helper.finalize();
});

async function testPolygonMovePoint(testActor, helper) {
  info("Displaying polygon");
  await helper.show("#polygon", {mode: "cssClipPath"});
  let { mouse, highlightedNode } = helper;

  let points = await helper.getElementAttribute("shapes-polygon", "points");
  let [x, y] = points.split(" ")[0].split(",");
  let quads = await testActor.getAllAdjustedQuads("#polygon");
  let { top, left, width, height } = quads.border[0].bounds;
  x = left + width * x / 100;
  y = top + height * y / 100;
  let dx = width / 10;
  let dy = height / 10;

  info("Moving first polygon point");
  await mouse.down(x, y);
  await mouse.move(x + dx, y + dy);
  await mouse.up();
  await testActor.reflow();

  let computedStyle = await highlightedNode.getComputedStyle();
  let definition = computedStyle["clip-path"].value;
  ok(definition.includes(`${dx}px ${dy}px`), `Point moved to ${dx}px ${dy}px`);
}

async function testPolygonAddPoint(testActor, helper) {
  await helper.show("#polygon", {mode: "cssClipPath"});
  let { mouse, highlightedNode } = helper;

  // Move first point to have same x as second point, then double click between
  // the two points to add a new one.
  let points = await helper.getElementAttribute("shapes-polygon", "points");
  let pointsArray = points.split(" ");
  let quads = await testActor.getAllAdjustedQuads("#polygon");
  let { top, left, width, height } = quads.border[0].bounds;
  let [x1, y1] = pointsArray[0].split(",");
  let [x2, y2] = pointsArray[1].split(",");
  x1 = left + width * x1 / 100;
  x2 = left + width * x2 / 100;
  y1 = top + height * y1 / 100;
  y2 = top + height * y2 / 100;

  await mouse.down(x1, y1);
  await mouse.move(x2, y1);
  await mouse.up();
  await testActor.reflow();

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
  await testActor.synthesizeMouse(options);
  await testActor.reflow();

  let computedStyle = await highlightedNode.getComputedStyle();
  let definition = computedStyle["clip-path"].value;
  // Decimal precision for coordinates with percentage units is 2
  let precision = 2;
  // Round to the desired decimal precision and cast to Number to remove trailing zeroes.
  newPointX = Number((newPointX * 100 / width).toFixed(precision));
  newPointY = Number((newPointY * 100 / height).toFixed(precision));
  ok(definition.includes(`${newPointX}% ${newPointY}%`),
     "Point successfuly added");
}

async function testPolygonRemovePoint(testActor, helper) {
  await helper.show("#polygon", {mode: "cssClipPath"});
  let { highlightedNode } = helper;

  let points = await helper.getElementAttribute("shapes-polygon", "points");
  let [x, y] = points.split(" ")[0].split(",");
  let quads = await testActor.getAllAdjustedQuads("#polygon");
  let { top, left, width, height } = quads.border[0].bounds;

  let options = {
    selector: ":root",
    x: left + width * x / 100,
    y: top + height * y / 100,
    center: false,
    options: {clickCount: 2}
  };

  info("Removing first polygon point");
  await testActor.synthesizeMouse(options);
  await testActor.reflow();

  let computedStyle = await highlightedNode.getComputedStyle();
  let definition = computedStyle["clip-path"].value;
  ok(!definition.includes(`${x}% ${y}%`), "Point successfully removed");
}

async function testCircleMoveCenter(testActor, helper) {
  await helper.show("#circle", {mode: "cssClipPath"});
  let { mouse, highlightedNode } = helper;

  let cx = parseFloat(await helper.getElementAttribute("shapes-ellipse", "cx"));
  let cy = parseFloat(await helper.getElementAttribute("shapes-ellipse", "cy"));
  let quads = await testActor.getAllAdjustedQuads("#circle");
  let { width, height } = quads.border[0].bounds;
  let cxPixel = width * cx / 100;
  let cyPixel = height * cy / 100;
  let dx = width / 10;
  let dy = height / 10;

  info("Moving circle center");
  await mouse.down(cxPixel, cyPixel, "#circle");
  await mouse.move(cxPixel + dx, cyPixel + dy, "#circle");
  await mouse.up(cxPixel + dx, cyPixel + dy, "#circle");
  await testActor.reflow();

  let computedStyle = await highlightedNode.getComputedStyle();
  let definition = computedStyle["clip-path"].value;
  ok(definition.includes(`at ${cx + 10}% ${cy + 10}%`),
     "Circle center successfully moved");
}

async function testEllipseMoveRadius(testActor, helper) {
  await helper.show("#ellipse", {mode: "cssClipPath"});
  let { mouse, highlightedNode } = helper;

  let rx = parseFloat(await helper.getElementAttribute("shapes-ellipse", "rx"));
  let ry = parseFloat(await helper.getElementAttribute("shapes-ellipse", "ry"));
  let cx = parseFloat(await helper.getElementAttribute("shapes-ellipse", "cx"));
  let cy = parseFloat(await helper.getElementAttribute("shapes-ellipse", "cy"));
  let quads = await testActor.getAllAdjustedQuads("#ellipse");
  let { width, height } = quads.content[0].bounds;
  let computedStyle = await highlightedNode.getComputedStyle();
  let paddingTop = parseFloat(computedStyle["padding-top"].value);
  let paddingLeft = parseFloat(computedStyle["padding-left"].value);
  let cxPixel = paddingLeft + width * cx / 100;
  let cyPixel = paddingTop + height * cy / 100;
  let rxPixel = cxPixel + width * rx / 100;
  let ryPixel = cyPixel + height * ry / 100;
  let dx = width / 10;
  let dy = height / 10;

  info("Moving ellipse rx");
  await mouse.down(rxPixel, cyPixel, "#ellipse");
  await mouse.move(rxPixel + dx, cyPixel, "#ellipse");
  await mouse.up(rxPixel + dx, cyPixel, "#ellipse");
  await testActor.reflow();

  info("Moving ellipse ry");
  await mouse.down(cxPixel, ryPixel, "#ellipse");
  await mouse.move(cxPixel, ryPixel - dy, "#ellipse");
  await mouse.up(cxPixel, ryPixel - dy, "#ellipse");
  await testActor.reflow();

  computedStyle = await highlightedNode.getComputedStyle();
  let definition = computedStyle["clip-path"].value;
  ok(definition.includes(`${rx + 10}% ${ry - 10}%`),
     "Ellipse radiuses successfully moved");
}

async function testInsetMoveEdges(testActor, helper) {
  await helper.show("#inset", {mode: "cssClipPath"});
  let { mouse, highlightedNode } = helper;

  let x = parseFloat(await helper.getElementAttribute("shapes-rect", "x"));
  let y = parseFloat(await helper.getElementAttribute("shapes-rect", "y"));
  let width = parseFloat(await helper.getElementAttribute("shapes-rect", "width"));
  let height = parseFloat(await helper.getElementAttribute("shapes-rect", "height"));
  let quads = await testActor.getAllAdjustedQuads("#inset");
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
  await mouse.down(xCenter, top, "#inset");
  await mouse.move(xCenter, top + dy, "#inset");
  await mouse.up(xCenter, top + dy, "#inset");
  await testActor.reflow();

  info("Moving inset bottom");
  await mouse.down(xCenter, bottom, "#inset");
  await mouse.move(xCenter, bottom + dy, "#inset");
  await mouse.up(xCenter, bottom + dy, "#inset");
  await testActor.reflow();

  info("Moving inset left");
  await mouse.down(left, yCenter, "#inset");
  await mouse.move(left + dx, yCenter, "#inset");
  await mouse.up(left + dx, yCenter, "#inset");
  await testActor.reflow();

  info("Moving inset right");
  await mouse.down(right, yCenter, "#inset");
  await mouse.move(right + dx, yCenter, "#inset");
  await mouse.up(right + dx, yCenter, "#inset");
  await testActor.reflow();

  let computedStyle = await highlightedNode.getComputedStyle();
  let definition = computedStyle["clip-path"].value;
  ok(definition.includes(
    `${top + dy}px ${elemWidth - right - dx}px ${100 - y - height - 10}% ${x + 10}%`),
     "Inset edges successfully moved");
}
