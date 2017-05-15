/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test the creation of the CSS shapes highlighter.

const TEST_URL = URL_ROOT + "doc_inspector_highlighter_cssshapes.html";
const HIGHLIGHTER_TYPE = "ShapesHighlighter";

add_task(function* () {
  let {inspector, testActor} = yield openInspectorForURL(TEST_URL);
  let front = inspector.inspector;
  let highlighter = yield front.getHighlighterByType(HIGHLIGHTER_TYPE);

  yield isHiddenByDefault(testActor, highlighter);
  yield isVisibleWhenShown(testActor, inspector, highlighter);

  yield highlighter.finalize();
});

function* isHiddenByDefault(testActor, highlighterFront) {
  info("Checking that highlighter is hidden by default");

  let polygonHidden = yield testActor.getHighlighterNodeAttribute(
    "shapes-polygon", "hidden", highlighterFront);
  let ellipseHidden = yield testActor.getHighlighterNodeAttribute(
    "shapes-ellipse", "hidden", highlighterFront);
  ok(polygonHidden && ellipseHidden, "The highlighter is hidden by default");
}

function* isVisibleWhenShown(testActor, inspector, highlighterFront) {
  info("Asking to show the highlighter on the polygon node");

  let polygonNode = yield getNodeFront("#polygon", inspector);
  yield highlighterFront.show(polygonNode, {mode: "cssClipPath"});

  let polygonHidden = yield testActor.getHighlighterNodeAttribute(
    "shapes-polygon", "hidden", highlighterFront);
  ok(!polygonHidden, "The polygon highlighter is visible");

  info("Asking to show the highlighter on the circle node");
  let circleNode = yield getNodeFront("#circle", inspector);
  yield highlighterFront.show(circleNode, {mode: "cssClipPath"});

  let ellipseHidden = yield testActor.getHighlighterNodeAttribute(
    "shapes-ellipse", "hidden", highlighterFront);
  polygonHidden = yield testActor.getHighlighterNodeAttribute(
    "shapes-polygon", "hidden", highlighterFront);
  ok(!ellipseHidden, "The circle highlighter is visible");
  ok(polygonHidden, "The polygon highlighter is no longer visible");

  info("Hiding the highlighter");
  yield highlighterFront.hide();

  polygonHidden = yield testActor.getHighlighterNodeAttribute(
    "shapes-polygon", "hidden", highlighterFront);
  ok(polygonHidden, "The highlighter is hidden");
}
