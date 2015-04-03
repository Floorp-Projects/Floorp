/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test the creation of the geometry highlighter elements.

const TEST_URL = "data:text/html;charset=utf-8," +
                 "<div style='position:absolute;left: 0; top: 0; width: 40000px; height: 8000px'></div>"

const ID = "rulers-highlighter-";

add_task(function*() {
  let { inspector, toolbox } = yield openInspectorForURL(TEST_URL);
  let front = inspector.inspector;

  let highlighter = yield front.getHighlighterByType("RulersHighlighter");

  // the rulers doesn't need any node, but as highligher it seems mandatory
  // ones, so the body is given
  let body = yield getNodeFront("body", inspector);
  yield highlighter.show(body);

  yield isUpdatedAfterScroll(highlighter, inspector);

  yield highlighter.finalize();
});

function* isUpdatedAfterScroll(highlighterFront, inspector) {
  info("Checking the rulers' position by default");

  let xAxisRulerTransform = yield getHighlighterNodeAttribute(highlighterFront,
    `${ID}x-axis-ruler`, "transform");
  let xAxisTextTransform = yield getHighlighterNodeAttribute(highlighterFront,
    `${ID}x-axis-text`, "transform");
  let yAxisRulerTransform = yield getHighlighterNodeAttribute(highlighterFront,
    `${ID}y-axis-ruler`, "transform");
  let yAxisTextTransform = yield getHighlighterNodeAttribute(highlighterFront,
    `${ID}y-axis-text`, "transform");

  is(xAxisRulerTransform, null, "x axis ruler is positioned properly");
  is(xAxisTextTransform, null, "x axis text are positioned properly");
  is(yAxisRulerTransform, null, "y axis ruler is positioned properly");
  is(yAxisRulerTransform, null, "y axis text are positioned properly");

  info("Asking the content window to scroll to specific coords");

  let x = 200, y = 300;

  let { data } = yield executeInContent("Test:ScrollWindow", { x, y });

  is(data.x, x, "window scrolled properly horizontally");
  is(data.y, y, "window scrolled properly vertically");

  info("Checking the rulers are properly positioned after the scrolling");

  xAxisRulerTransform = yield getHighlighterNodeAttribute(highlighterFront,
    `${ID}x-axis-ruler`, "transform");
  xAxisTextTransform = yield getHighlighterNodeAttribute(highlighterFront,
    `${ID}x-axis-text`, "transform");
  yAxisRulerTransform = yield getHighlighterNodeAttribute(highlighterFront,
    `${ID}y-axis-ruler`, "transform");
  yAxisTextTransform = yield getHighlighterNodeAttribute(highlighterFront,
    `${ID}y-axis-text`, "transform");

  is(xAxisRulerTransform, `translate(-${x})`, "x axis ruler is positioned properly");
  is(xAxisTextTransform, `translate(-${x})`, "x axis text are positioned properly");
  is(yAxisRulerTransform, `translate(0, -${y})`, "y axis ruler is positioned properly");
  is(yAxisRulerTransform, `translate(0, -${y})`, "y axis text are positioned properly");

  info("Asking the content window to scroll relative to the current position");

  ({ data }) = yield executeInContent("Test:ScrollWindow", {
    x: -50,
    y: -60,
    relative: true
  });

  is(data.x, x - 50, "window scrolled properly horizontally");
  is(data.y, y - 60, "window scrolled properly vertically");

  info("Checking the rulers are properly positioned after the relative scrolling");

  xAxisRulerTransform = yield getHighlighterNodeAttribute(highlighterFront,
    `${ID}x-axis-ruler`, "transform");
  xAxisTextTransform = yield getHighlighterNodeAttribute(highlighterFront,
    `${ID}x-axis-text`, "transform");
  yAxisRulerTransform = yield getHighlighterNodeAttribute(highlighterFront,
    `${ID}y-axis-ruler`, "transform");
  yAxisTextTransform = yield getHighlighterNodeAttribute(highlighterFront,
    `${ID}y-axis-text`, "transform");

  is(xAxisRulerTransform, `translate(-${x - 50})`, "x axis ruler is positioned properly");
  is(xAxisTextTransform, `translate(-${x - 50})`, "x axis text are positioned properly");
  is(yAxisRulerTransform, `translate(0, -${y - 60})`, "y axis ruler is positioned properly");
  is(yAxisRulerTransform, `translate(0, -${y - 60})`, "y axis text are positioned properly");
}
