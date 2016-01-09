/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test that the right arrows/labels are shown even when the css properties are
// in several different css rules.

const TEST_URL = URL_ROOT + "doc_inspector_highlighter-geometry_01.html";
const ID = "geometry-editor-";
const PROPS = ["left", "right", "top", "bottom"];

add_task(function*() {
  let {inspector, testActor} = yield openInspectorForURL(TEST_URL);
  let front = inspector.inspector;

  let highlighter = yield front.getHighlighterByType("GeometryEditorHighlighter");

  yield checkArrowsLabels("#node1", ["size"],
                          highlighter, inspector, testActor);

  yield checkArrowsLabels("#node2", ["top", "left", "bottom", "right"],
                          highlighter, inspector, testActor);

  yield checkArrowsLabels("#node3", ["top", "left", "size"],
                          highlighter, inspector, testActor);

  yield highlighter.finalize();
});

function* checkArrowsLabels(selector, expectedProperties, highlighterFront, inspector, testActor) {
  info("Getting node " + selector + " from the page");
  let node = yield getNodeFront(selector, inspector);

  info("Highlighting the node");
  yield highlighterFront.show(node);

  for (let name of expectedProperties) {
    let hidden;
    if (name === "size") {
      hidden = yield testActor.getHighlighterNodeAttribute(ID + "label-size", "hidden", highlighterFront);
    } else {
      hidden = yield testActor.getHighlighterNodeAttribute(ID + "arrow-" + name, "hidden", highlighterFront);
    }
    ok(!hidden, "The " + name + " arrow/label is visible for node " + selector);
  }

  // Testing that the other arrows are hidden
  for (let name of PROPS) {
    if (expectedProperties.indexOf(name) !== -1) {
      continue;
    }
    let hidden = yield testActor.getHighlighterNodeAttribute(ID + "arrow-" + name, "hidden", highlighterFront);
    is(hidden, "true", "The " + name + " arrow is hidden for node " + selector);
  }

  info("Hiding the highlighter");
  yield highlighterFront.hide();
}
