/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Make sure that the CSS shapes highlighters have the correct attributes.

const TEST_URL = URL_ROOT + "doc_inspector_highlighter_cssshapes.html";
const HIGHLIGHTER_TYPE = "ShapesHighlighter";

add_task(async function() {
  const {inspector, testActor} = await openInspectorForURL(TEST_URL);
  const front = inspector.inspector;
  const highlighter = await front.getHighlighterByType(HIGHLIGHTER_TYPE);

  await polygonHasCorrectAttrs(testActor, inspector, highlighter);
  await circleHasCorrectAttrs(testActor, inspector, highlighter);
  await ellipseHasCorrectAttrs(testActor, inspector, highlighter);
  await insetHasCorrectAttrs(testActor, inspector, highlighter);

  await highlighter.finalize();
});

async function polygonHasCorrectAttrs(testActor, inspector, highlighterFront) {
  info("Checking polygon highlighter has correct points");

  const polygonNode = await getNodeFront("#polygon", inspector);
  await highlighterFront.show(polygonNode, {mode: "cssClipPath"});

  const points = await testActor.getHighlighterNodeAttribute(
    "shapes-polygon", "points", highlighterFront);
  const realPoints = "0,0 12.5,50 25,0 37.5,50 50,0 62.5,50 " +
                   "75,0 87.5,50 100,0 90,100 50,60 10,100";
  is(points, realPoints, "Polygon highlighter has correct points");
}

async function circleHasCorrectAttrs(testActor, inspector, highlighterFront) {
  info("Checking circle highlighter has correct attributes");

  const circleNode = await getNodeFront("#circle", inspector);
  await highlighterFront.show(circleNode, {mode: "cssClipPath"});

  const rx = await testActor.getHighlighterNodeAttribute(
    "shapes-ellipse", "rx", highlighterFront);
  const ry = await testActor.getHighlighterNodeAttribute(
    "shapes-ellipse", "ry", highlighterFront);
  const cx = await testActor.getHighlighterNodeAttribute(
    "shapes-ellipse", "cx", highlighterFront);
  const cy = await testActor.getHighlighterNodeAttribute(
    "shapes-ellipse", "cy", highlighterFront);

  is(rx, 25, "Circle highlighter has correct rx");
  is(ry, 25, "Circle highlighter has correct ry");
  is(cx, 30, "Circle highlighter has correct cx");
  is(cy, 40, "Circle highlighter has correct cy");
}

async function ellipseHasCorrectAttrs(testActor, inspector, highlighterFront) {
  info("Checking ellipse highlighter has correct attributes");

  const ellipseNode = await getNodeFront("#ellipse", inspector);
  await highlighterFront.show(ellipseNode, {mode: "cssClipPath"});

  const rx = await testActor.getHighlighterNodeAttribute(
    "shapes-ellipse", "rx", highlighterFront);
  const ry = await testActor.getHighlighterNodeAttribute(
    "shapes-ellipse", "ry", highlighterFront);
  const cx = await testActor.getHighlighterNodeAttribute(
    "shapes-ellipse", "cx", highlighterFront);
  const cy = await testActor.getHighlighterNodeAttribute(
    "shapes-ellipse", "cy", highlighterFront);

  is(rx, 40, "Ellipse highlighter has correct rx");
  is(ry, 30, "Ellipse highlighter has correct ry");
  is(cx, 25, "Ellipse highlighter has correct cx");
  is(cy, 30, "Ellipse highlighter has correct cy");
}

async function insetHasCorrectAttrs(testActor, inspector, highlighterFront) {
  info("Checking rect highlighter has correct attributes");

  const insetNode = await getNodeFront("#inset", inspector);
  await highlighterFront.show(insetNode, {mode: "cssClipPath"});

  const x = await testActor.getHighlighterNodeAttribute(
    "shapes-rect", "x", highlighterFront);
  const y = await testActor.getHighlighterNodeAttribute(
    "shapes-rect", "y", highlighterFront);
  const width = await testActor.getHighlighterNodeAttribute(
    "shapes-rect", "width", highlighterFront);
  const height = await testActor.getHighlighterNodeAttribute(
    "shapes-rect", "height", highlighterFront);

  is(x, 15, "Rect highlighter has correct x");
  is(y, 25, "Rect highlighter has correct y");
  is(width, 72.5, "Rect highlighter has correct width");
  is(height, 45, "Rect highlighter has correct height");
}
