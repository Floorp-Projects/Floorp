/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test that grid layouts without items don't cause grid highlighter errors.

const TEST_URL = `
  <style type='text/css'>
    .grid {
      display: grid;
      grid-template-columns: 20px 20px;
      grid-gap: 15px;
    }
  </style>
  <div class="grid"></div>
`;

const HIGHLIGHTER_TYPE = "CssGridHighlighter";

add_task(async function() {
  const {inspector, testActor} = await openInspectorForURL(
    "data:text/html;charset=utf-8," + encodeURIComponent(TEST_URL));
  const front = inspector.inspector;
  const highlighter = await front.getHighlighterByType(HIGHLIGHTER_TYPE);

  info("Try to show the highlighter on the grid container");
  const node = await getNodeFront(".grid", inspector);
  await highlighter.show(node);

  let hidden = await testActor.getHighlighterNodeAttribute(
    "css-grid-canvas", "hidden", highlighter);
  ok(!hidden, "The highlighter is visible");

  info("Hiding the highlighter");
  await highlighter.hide();

  hidden = await testActor.getHighlighterNodeAttribute(
    "css-grid-canvas", "hidden", highlighter);
  ok(hidden, "The highlighter is hidden");

  await highlighter.finalize();
});
