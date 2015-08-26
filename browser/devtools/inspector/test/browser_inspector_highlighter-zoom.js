/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test that the highlighter stays correctly positioned and has the right aspect
// ratio even when the page is zoomed in or out.

const TEST_URL = "data:text/html;charset=utf-8,<div>zoom me</div>";

// TEST_LEVELS entries should contain the following properties:
// - level: the zoom level to test
// - expected: the style attribute value to check for on the root highlighter
//   element.
const TEST_LEVELS = [{
  level: 2,
  expected: "position:absolute;transform-origin:top left;transform:scale(0.5);width:200%;height:200%;"
}, {
  level: 1,
  expected: "position:absolute;width:100%;height:100%;"
}, {
  level: .5,
  expected: "position:absolute;transform-origin:top left;transform:scale(2);width:50%;height:50%;"
}];

add_task(function*() {
  let {inspector, testActor} = yield openInspectorForURL(TEST_URL);

  info("Highlighting the test node");

  yield hoverElement("div", inspector);
  let isVisible = yield testActor.isHighlighting();
  ok(isVisible, "The highlighter is visible");

  for (let {level, expected} of TEST_LEVELS) {
    info("Zoom to level " + level + " and check that the highlighter is correct");

    yield testActor.zoomPageTo(level);
    isVisible = yield testActor.isHighlighting();
    ok(isVisible, "The highlighter is still visible at zoom level " + level);

    yield testActor.isNodeCorrectlyHighlighted("div", is);

    info("Check that the highlighter root wrapper node was scaled down");

    let style = yield getRootNodeStyle(testActor);
    is(style, expected, "The style attribute of the root element is correct");
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

function* getRootNodeStyle(testActor) {
  let value = yield testActor.getHighlighterNodeAttribute("box-model-root", "style");
  return value;
}
