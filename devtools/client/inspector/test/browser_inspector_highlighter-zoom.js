/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test that the highlighter stays correctly positioned and has the right aspect
// ratio even when the page is zoomed in or out.

const TEST_URL = "data:text/html;charset=utf-8,<div>zoom me</div>";

// TEST_LEVELS entries should contain the zoom level to test.
const TEST_LEVELS = [2, 1, .5];

// Returns the expected style attribute value to check for on the highlighter's elements
// node, for the values given.
const expectedStyle = (w, h, z) =>
        (z !== 1 ? `transform-origin:top left; transform:scale(${1 / z}); ` : "") +
        `position:absolute; width:${w * z}px;height:${h * z}px; ` +
        "overflow:hidden";

add_task(function* () {
  let {inspector, testActor} = yield openInspectorForURL(TEST_URL);
  let highlighterUtils = inspector.toolbox.highlighterUtils;

  let div = yield getNodeFront("div", inspector);

  for (let level of TEST_LEVELS) {
    info(`Zoom to level ${level}`);
    yield testActor.zoomPageTo(level, false);

    info("Highlight the test node");
    yield highlighterUtils.highlightNodeFront(div);

    let isVisible = yield testActor.isHighlighting();
    ok(isVisible, `The highlighter is visible at zoom level ${level}`);

    yield testActor.isNodeCorrectlyHighlighted("div", is);

    info("Check that the highlighter root wrapper node was scaled down");

    let style = yield getElementsNodeStyle(testActor);
    let { width, height } = yield testActor.getWindowDimensions();
    is(style, expectedStyle(width, height, level),
      "The style attribute of the root element is correct");

    info("Unhighlight the node");
    yield highlighterUtils.unhighlight();
  }
});

function* getElementsNodeStyle(testActor) {
  let value = yield testActor.getHighlighterNodeAttribute("box-model-elements", "style");
  return value;
}
