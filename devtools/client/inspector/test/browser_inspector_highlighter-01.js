/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test that hovering over nodes in the markup-view shows the highlighter over
// those nodes
add_task(async function() {
  info("Loading the test document and opening the inspector");
  const { inspector, highlighterTestFront } = await openInspectorForURL(
    "data:text/html;charset=utf-8,<h1>foo</h1><span>bar</span>"
  );
  const { waitForHighlighterTypeShown } = getHighlighterTestHelpers(inspector);

  let isVisible = !!inspector.highlighters.getActiveHighlighter(
    inspector.highlighters.TYPES.BOXMODEL
  );
  ok(!isVisible, "The highlighter is hidden by default");

  info("Selecting the test node");
  await selectNode("span", inspector);
  const container = await getContainerForSelector("h1", inspector);

  const onHighlight = waitForHighlighterTypeShown(
    inspector.highlighters.TYPES.BOXMODEL
  );

  EventUtils.synthesizeMouseAtCenter(
    container.tagLine,
    { type: "mousemove" },
    inspector.markup.doc.defaultView
  );
  await onHighlight;

  isVisible = await highlighterTestFront.isHighlighting();
  ok(isVisible, "The highlighter is shown on a markup container hover");

  ok(
    await highlighterTestFront.assertHighlightedNode("h1"),
    "The highlighter highlights the right node"
  );
});
