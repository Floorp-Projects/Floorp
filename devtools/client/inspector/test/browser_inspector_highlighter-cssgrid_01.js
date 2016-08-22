/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test the creation of the canvas highlighter element of the css grid highlighter.

const TEST_URL = `
  <style type='text/css'>
    #grid {
      display: grid;
    }
    #cell1 {
      grid-column: 1;
      grid-row: 1;
    }
    #cell2 {
      grid-column: 2;
      grid-row: 1;
    }
    #cell3 {
      grid-column: 1;
      grid-row: 2;
    }
    #cell4 {
      grid-column: 2;
      grid-row: 2;
    }
  </style>
  <div id="grid">
    <div id="cell1">cell1</div>
    <div id="cell2">cell2</div>
    <div id="cell3">cell3</div>
    <div id="cell4">cell4</div>
  </div>
`;

const HIGHLIGHTER_TYPE = "CssGridHighlighter";

add_task(function* () {
  let {inspector, testActor} = yield openInspectorForURL(
    "data:text/html;charset=utf-8," + encodeURIComponent(TEST_URL));
  let front = inspector.inspector;
  let highlighter = yield front.getHighlighterByType(HIGHLIGHTER_TYPE);

  yield isHiddenByDefault(testActor, highlighter);
  yield isVisibleWhenShown(testActor, inspector, highlighter);

  yield highlighter.finalize();
});

function* isHiddenByDefault(testActor, highlighterFront) {
  info("Checking that the highlighter is hidden by default");

  let hidden = yield testActor.getHighlighterNodeAttribute(
    "css-grid-canvas", "hidden", highlighterFront);
  ok(hidden, "The highlighter is hidden by default");
}

function* isVisibleWhenShown(testActor, inspector, highlighterFront) {
  info("Asking to show the highlighter on the test node");

  let node = yield getNodeFront("#grid", inspector);
  yield highlighterFront.show(node);

  let hidden = yield testActor.getHighlighterNodeAttribute(
    "css-grid-canvas", "hidden", highlighterFront);
  ok(!hidden, "The highlighter is visible");

  info("Hiding the highlighter");
  yield highlighterFront.hide();

  hidden = yield testActor.getHighlighterNodeAttribute(
    "css-grid-canvas", "hidden", highlighterFront);
  ok(hidden, "The highlighter is hidden");
}
