/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Make sure that the CSS shapes highlighters have the correct attributes.

const TEST_URL = URL_ROOT + "doc_inspector_highlighter_cssshapes.html";
const HIGHLIGHTER_TYPE = "ShapesHighlighter";

add_task(function* () {
  let {inspector, testActor} = yield openInspectorForURL(TEST_URL);
  let front = inspector.inspector;
  let highlighter = yield front.getHighlighterByType(HIGHLIGHTER_TYPE);

  yield polygonHasCorrectAttrs(testActor, inspector, highlighter);
  yield circleHasCorrectAttrs(testActor, inspector, highlighter);

  yield highlighter.finalize();
});

function* polygonHasCorrectAttrs(testActor, inspector, highlighterFront) {
  info("Checking polygon highlighter has correct points");

  let polygonNode = yield getNodeFront("#polygon", inspector);
  yield highlighterFront.show(polygonNode, {mode: "cssClipPath"});

  let points = yield testActor.getHighlighterNodeAttribute(
    "shapes-polygon", "points", highlighterFront);
  let realPoints = "0,0 12.5,50 25,0 37.5,50 50,0 62.5,50 " +
                   "75,0 87.5,50 100,0 90,100 50,60 10,100";
  is(points, realPoints, "Polygon highlighter has correct points");
}

function* circleHasCorrectAttrs(testActor, inspector, highlighterFront) {
  info("Checking circle highlighter has correct attributes");

  let circleNode = yield getNodeFront("#circle", inspector);
  yield highlighterFront.show(circleNode, {mode: "cssClipPath"});

  let rx = yield testActor.getHighlighterNodeAttribute(
    "shapes-ellipse", "rx", highlighterFront);
  let ry = yield testActor.getHighlighterNodeAttribute(
    "shapes-ellipse", "ry", highlighterFront);
  let cx = yield testActor.getHighlighterNodeAttribute(
    "shapes-ellipse", "cx", highlighterFront);
  let cy = yield testActor.getHighlighterNodeAttribute(
    "shapes-ellipse", "cy", highlighterFront);

  is(rx, 25, "Circle highlighter has correct rx");
  is(ry, 25, "Circle highlighter has correct ry");
  is(cx, 30, "Circle highlighter has correct cx");
  is(cy, 40, "Circle highlighter has correct cy");
}
