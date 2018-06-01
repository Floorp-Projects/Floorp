/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test the creation of the geometry highlighter elements.

const TEST_URL = "data:text/html;charset=utf-8," +
                 "<div style='position:absolute;left: 0; top: 0; " +
                 "width: 40000px; height: 8000px'></div>";

const ID = "rulers-highlighter-";

// Maximum size, in pixel, for the horizontal ruler and vertical ruler
// used by RulersHighlighter
const RULERS_MAX_X_AXIS = 10000;
const RULERS_MAX_Y_AXIS = 15000;
// Number of steps after we add a text in RulersHighliter;
// currently the unit is in pixel.
const RULERS_TEXT_STEP = 100;

add_task(async function() {
  const { inspector, testActor } = await openInspectorForURL(TEST_URL);
  const front = inspector.inspector;

  const highlighter = await front.getHighlighterByType("RulersHighlighter");

  await isHiddenByDefault(highlighter, inspector, testActor);
  await hasRightLabelsContent(highlighter, inspector, testActor);

  await highlighter.finalize();
});

async function isHiddenByDefault(highlighterFront, inspector, testActor) {
  info("Checking the highlighter is hidden by default");

  let hidden = await testActor.getHighlighterNodeAttribute(
      ID + "elements", "hidden", highlighterFront);

  is(hidden, "true", "highlighter is hidden by default");

  info("Checking the highlighter is displayed when asked");
  // the rulers doesn't need any node, but as highligher it seems mandatory
  // ones, so the body is given
  const body = await getNodeFront("body", inspector);
  await highlighterFront.show(body);

  hidden = await testActor.getHighlighterNodeAttribute(
      ID + "elements", "hidden", highlighterFront);

  isnot(hidden, "true", "highlighter is visible after show");
}

async function hasRightLabelsContent(highlighterFront, inspector, testActor) {
  info("Checking the rulers have the proper text, based on rulers' size");

  const contentX = await testActor.getHighlighterNodeTextContent(
    `${ID}x-axis-text`, highlighterFront);
  const contentY = await testActor.getHighlighterNodeTextContent(
    `${ID}y-axis-text`, highlighterFront);

  let expectedX = "";
  for (let i = RULERS_TEXT_STEP; i < RULERS_MAX_X_AXIS; i += RULERS_TEXT_STEP) {
    expectedX += i;
  }

  is(contentX, expectedX, "x axis text content is correct");

  let expectedY = "";
  for (let i = RULERS_TEXT_STEP; i < RULERS_MAX_Y_AXIS; i += RULERS_TEXT_STEP) {
    expectedY += i;
  }

  is(contentY, expectedY, "y axis text content is correct");
}
