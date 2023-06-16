/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test that hovering over nodes on the breadcrumb buttons in the inspector
// shows the highlighter over those nodes
add_task(async function () {
  info("Loading the test document and opening the inspector");
  const { inspector, highlighterTestFront } = await openInspectorForURL(
    "data:text/html;charset=utf-8,<h1>foo</h1><span>bar</span>"
  );
  const { waitForHighlighterTypeShown, waitForHighlighterTypeHidden } =
    getHighlighterTestHelpers(inspector);

  info("Selecting the test node");
  await selectNode("span", inspector);
  const bcButtons = inspector.breadcrumbs.container;

  let onNodeHighlighted = waitForHighlighterTypeShown(
    inspector.highlighters.TYPES.BOXMODEL
  );
  let button = bcButtons.childNodes[1];
  EventUtils.synthesizeMouseAtCenter(
    button,
    { type: "mousemove" },
    button.ownerDocument.defaultView
  );
  await onNodeHighlighted;

  let isVisible = await highlighterTestFront.isHighlighting();
  ok(isVisible, "The highlighter is shown on a markup container hover");

  ok(
    await highlighterTestFront.assertHighlightedNode("body"),
    "The highlighter highlights the right node"
  );

  const onNodeUnhighlighted = waitForHighlighterTypeHidden(
    inspector.highlighters.TYPES.BOXMODEL
  );
  // move outside of the breadcrumb trail to trigger unhighlight
  EventUtils.synthesizeMouseAtCenter(
    inspector.addNodeButton,
    { type: "mousemove" },
    inspector.addNodeButton.ownerDocument.defaultView
  );
  await onNodeUnhighlighted;

  onNodeHighlighted = waitForHighlighterTypeShown(
    inspector.highlighters.TYPES.BOXMODEL
  );
  button = bcButtons.childNodes[2];
  EventUtils.synthesizeMouseAtCenter(
    button,
    { type: "mousemove" },
    button.ownerDocument.defaultView
  );
  await onNodeHighlighted;

  isVisible = await highlighterTestFront.isHighlighting();
  ok(isVisible, "The highlighter is shown on a markup container hover");

  ok(
    await highlighterTestFront.assertHighlightedNode("span"),
    "The highlighter highlights the right node"
  );
});
