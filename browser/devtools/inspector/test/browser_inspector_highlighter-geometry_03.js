/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test that the right arrows/labels are shown even when the css properties are
// in several different css rules.

const TEST_URL = TEST_URL_ROOT + "doc_inspector_highlighter-geometry_01.html";
const ID = "geometry-editor-";
const PROPS = ["left", "right", "top", "bottom"];

add_task(function*() {
  let {inspector, toolbox} = yield openInspectorForURL(TEST_URL);
  let front = inspector.inspector;

  let highlighter = yield front.getHighlighterByType("GeometryEditorHighlighter");

  yield checkArrowsLabels("#node1", ["size"],
                          highlighter, inspector);

  yield checkArrowsLabels("#node2", ["top", "left", "bottom", "right"],
                          highlighter, inspector);

  yield checkArrowsLabels("#node3", ["top", "left", "size"],
                          highlighter, inspector);

  yield highlighter.finalize();
});

function* checkArrowsLabels(selector, expectedProperties, highlighterFront, inspector) {
  info("Getting node " + selector + " from the page");
  let node = yield getNodeFront(selector, inspector);

  info("Highlighting the node");
  yield highlighterFront.show(node);

  for (let name of expectedProperties) {
    let hidden;
    if (name === "size") {
      hidden = yield getHighlighterNodeAttribute(highlighterFront,
        ID + "label-size", "hidden");
    } else {
      hidden = yield getHighlighterNodeAttribute(highlighterFront,
        ID + "arrow-" + name, "hidden");
    }
    ok(!hidden, "The " + name + " arrow/label is visible for node " + selector);
  }

  // Testing that the other arrows are hidden
  for (let name of PROPS) {
    if (expectedProperties.indexOf(name) !== -1) {
      continue;
    }
    let hidden = yield getHighlighterNodeAttribute(highlighterFront,
      ID + "arrow-" + name, "hidden");
    is(hidden, "true", "The " + name + " arrow is hidden for node " + selector);
  }

  info("Hiding the highlighter");
  yield highlighterFront.hide();
}
