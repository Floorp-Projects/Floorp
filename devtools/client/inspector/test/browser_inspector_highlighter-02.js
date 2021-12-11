/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test that the highlighter is correctly displayed over a variety of elements

const TEST_URI = URL_ROOT + "doc_inspector_highlighter.html";

add_task(async function() {
  const { inspector, highlighterTestFront } = await openInspectorForURL(
    TEST_URI
  );

  info("Selecting the simple, non-transformed DIV");
  await selectAndHighlightNode("#simple-div", inspector);

  let isVisible = await highlighterTestFront.isHighlighting();
  ok(isVisible, "The highlighter is shown");
  ok(
    await highlighterTestFront.assertHighlightedNode("#simple-div"),
    "The highlighter's outline corresponds to the simple div"
  );
  await isNodeCorrectlyHighlighted(highlighterTestFront, "#simple-div");

  info("Selecting the rotated DIV");
  await selectAndHighlightNode("#rotated-div", inspector);

  isVisible = await highlighterTestFront.isHighlighting();
  ok(isVisible, "The highlighter is shown");
  info(
    "Check that the highlighter is displayed at the expected position for rotated div"
  );
  await isNodeCorrectlyHighlighted(highlighterTestFront, "#rotated-div");

  info("Selecting the zero width height DIV");
  await selectAndHighlightNode("#widthHeightZero-div", inspector);

  isVisible = await highlighterTestFront.isHighlighting();
  ok(isVisible, "The highlighter is shown");
  info(
    "Check that the highlighter is displayed at the expected position for a zero width height div"
  );
  await isNodeCorrectlyHighlighted(
    highlighterTestFront,
    "#widthHeightZero-div"
  );
});
