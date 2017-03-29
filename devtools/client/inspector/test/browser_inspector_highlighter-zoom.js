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

  info("Highlighting the test node");

  yield hoverElement("div", inspector);
  let isVisible = yield testActor.isHighlighting();
  ok(isVisible, "The highlighter is visible");

  for (let level of TEST_LEVELS) {
    info("Zoom to level " + level +
         " and check that the highlighter is correct");

    yield testActor.zoomPageTo(level);
    isVisible = yield testActor.isHighlighting();
    ok(isVisible, "The highlighter is still visible at zoom level " + level);

    yield testActor.isNodeCorrectlyHighlighted("div", is);

    info("Check that the highlighter root wrapper node was scaled down");

    let style = yield getElementsNodeStyle(testActor);
    let { width, height } = yield testActor.getWindowDimensions();
    is(style, expectedStyle(width, height, level),
      "The style attribute of the root element is correct");
  }
});

function* hoverElement(selector, inspector) {
  info("Hovering node " + selector + " in the markup view");
  let container = yield getContainerForSelector(selector, inspector);
  yield hoverContainer(container, inspector);
}

function* hoverContainer(container, inspector) {
  let onHighlight = inspector.toolbox.once("node-highlight");
  EventUtils.synthesizeMouse(container.tagLine, 2, 2, {type: "mousemove"},
      inspector.markup.doc.defaultView);
  yield onHighlight;
}

function* getElementsNodeStyle(testActor) {
  let value = yield testActor.getHighlighterNodeAttribute(
    "box-model-elements", "style");
  return value;
}
