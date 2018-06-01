/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test the creation of the CSS shapes highlighter.

const TEST_URL = URL_ROOT + "doc_inspector_highlighter_cssshapes.html";
const HIGHLIGHTER_TYPE = "ShapesHighlighter";
const SHAPE_IDS = ["polygon", "ellipse", "rect"];
const SHAPE_TYPES = [
  {
    shapeName: "polygon",
    highlighter: "polygon"
  },
  {
    shapeName: "circle",
    highlighter: "ellipse"
  },
  {
    shapeName: "ellipse",
    highlighter: "ellipse"
  },
  {
    shapeName: "inset",
    highlighter: "rect"
  }
];

add_task(async function() {
  const {inspector, testActor} = await openInspectorForURL(TEST_URL);
  const front = inspector.inspector;
  const highlighter = await front.getHighlighterByType(HIGHLIGHTER_TYPE);

  await isHiddenByDefault(testActor, highlighter);
  await isVisibleWhenShown(testActor, inspector, highlighter);

  await highlighter.finalize();
});

async function getShapeHidden(testActor, highlighterFront) {
  const hidden = {};
  for (const shape of SHAPE_IDS) {
    hidden[shape] = await testActor.getHighlighterNodeAttribute(
      "shapes-" + shape, "hidden", highlighterFront);
  }
  return hidden;
}

async function isHiddenByDefault(testActor, highlighterFront) {
  info("Checking that highlighter is hidden by default");

  const polygonHidden = await testActor.getHighlighterNodeAttribute(
    "shapes-polygon", "hidden", highlighterFront);
  const ellipseHidden = await testActor.getHighlighterNodeAttribute(
    "shapes-ellipse", "hidden", highlighterFront);
  ok(polygonHidden && ellipseHidden, "The highlighter is hidden by default");
}

async function isVisibleWhenShown(testActor, inspector, highlighterFront) {
  for (const { shapeName, highlighter } of SHAPE_TYPES) {
    info(`Asking to show the highlighter on the ${shapeName} node`);

    const node = await getNodeFront(`#${shapeName}`, inspector);
    await highlighterFront.show(node, {mode: "cssClipPath"});

    const hidden = await getShapeHidden(testActor, highlighterFront);
    ok(!hidden[highlighter], `The ${shapeName} highlighter is visible`);
  }

  info("Hiding the highlighter");
  await highlighterFront.hide();

  const hidden = await getShapeHidden(testActor, highlighterFront);
  ok(hidden.polygon && hidden.ellipse && hidden.rect, "The highlighter is hidden");
}
