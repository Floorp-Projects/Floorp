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

add_task(async function() {
  const {inspector, testActor} = await openInspectorForURL(
    "data:text/html;charset=utf-8," + encodeURIComponent(TEST_URL));
  const front = inspector.inspector;
  const highlighter = await front.getHighlighterByType(HIGHLIGHTER_TYPE);

  await isHiddenByDefault(testActor, highlighter);
  await isVisibleWhenShown(testActor, inspector, highlighter);

  await highlighter.finalize();
});

async function isHiddenByDefault(testActor, highlighterFront) {
  info("Checking that the highlighter is hidden by default");

  const hidden = await testActor.getHighlighterNodeAttribute(
    "css-grid-canvas", "hidden", highlighterFront);
  ok(hidden, "The highlighter is hidden by default");
}

async function isVisibleWhenShown(testActor, inspector, highlighterFront) {
  info("Asking to show the highlighter on the test node");

  const node = await getNodeFront("#grid", inspector);
  await highlighterFront.show(node);

  let hidden = await testActor.getHighlighterNodeAttribute(
    "css-grid-canvas", "hidden", highlighterFront);
  ok(!hidden, "The highlighter is visible");

  info("Hiding the highlighter");
  await highlighterFront.hide();

  hidden = await testActor.getHighlighterNodeAttribute(
    "css-grid-canvas", "hidden", highlighterFront);
  ok(hidden, "The highlighter is hidden");
}
