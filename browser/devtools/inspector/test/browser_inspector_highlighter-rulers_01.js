/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test the creation of the geometry highlighter elements.

const TEST_URL = "data:text/html;charset=utf-8," +
                 "<div style='position:absolute;left: 0; top: 0; width: 40000px; height: 8000px'></div>"

const ID = "rulers-highlighter-";

// Maximum size, in pixel, for the horizontal ruler and vertical ruler
// used by RulersHighlighter
const RULERS_MAX_X_AXIS = 10000;
const RULERS_MAX_Y_AXIS = 15000;
// Number of steps after we add a text in RulersHighliter;
// currently the unit is in pixel.
const RULERS_TEXT_STEP = 100;

add_task(function*() {
  let { inspector, toolbox } = yield openInspectorForURL(TEST_URL);
  let front = inspector.inspector;

  let highlighter = yield front.getHighlighterByType("RulersHighlighter");

  yield isHiddenByDefault(highlighter, inspector);
  yield hasRightLabelsContent(highlighter, inspector);

  yield highlighter.finalize();
});

function* isHiddenByDefault(highlighterFront, inspector) {
  info("Checking the highlighter is hidden by default");

  let hidden = yield getHighlighterNodeAttribute(highlighterFront,
      ID + "elements", "hidden");

  is(hidden, "true", "highlighter is hidden by default");

  info("Checking the highlighter is displayed when asked");
  // the rulers doesn't need any node, but as highligher it seems mandatory
  // ones, so the body is given
  let body = yield getNodeFront("body", inspector);
  yield highlighterFront.show(body);

  hidden = yield getHighlighterNodeAttribute(highlighterFront,
      ID + "elements", "hidden");

  isnot(hidden, "true", "highlighter is visible after show");
}

function* hasRightLabelsContent(highlighterFront, inspector) {
  info("Checking the rulers have the proper text, based on rulers' size");

  let contentX = yield getHighlighterNodeTextContent(highlighterFront, `${ID}x-axis-text`);
  let contentY = yield getHighlighterNodeTextContent(highlighterFront, `${ID}y-axis-text`);

  let expectedX = "";
  for (let i = RULERS_TEXT_STEP; i < RULERS_MAX_X_AXIS; i+= RULERS_TEXT_STEP)
    expectedX += i;

  is(contentX, expectedX, "x axis text content is correct");

  let expectedY = "";
  for (let i = RULERS_TEXT_STEP; i < RULERS_MAX_Y_AXIS; i+= RULERS_TEXT_STEP)
    expectedY += i;

  is(contentY, expectedY, "y axis text content is correct");
}
