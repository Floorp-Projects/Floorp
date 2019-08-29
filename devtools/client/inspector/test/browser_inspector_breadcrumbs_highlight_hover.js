/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test that hovering over nodes on the breadcrumb buttons in the inspector
// shows the highlighter over those nodes
add_task(async function() {
  info("Loading the test document and opening the inspector");
  const { inspector, testActor } = await openInspectorForURL(
    "data:text/html;charset=utf-8,<h1>foo</h1><span>bar</span>"
  );
  info("Selecting the test node");
  await selectNode("span", inspector);
  const bcButtons = inspector.breadcrumbs.container;

  let onNodeHighlighted = inspector.highlighter.once("node-highlight");
  let button = bcButtons.childNodes[1];
  EventUtils.synthesizeMouseAtCenter(
    button,
    { type: "mousemove" },
    button.ownerDocument.defaultView
  );
  await onNodeHighlighted;

  let isVisible = await testActor.isHighlighting();
  ok(isVisible, "The highlighter is shown on a markup container hover");

  ok(
    await testActor.assertHighlightedNode("body"),
    "The highlighter highlights the right node"
  );

  const onNodeUnhighlighted = inspector.highlighter.once("node-unhighlight");
  // move outside of the breadcrumb trail to trigger unhighlight
  EventUtils.synthesizeMouseAtCenter(
    inspector.addNodeButton,
    { type: "mousemove" },
    inspector.addNodeButton.ownerDocument.defaultView
  );
  await onNodeUnhighlighted;

  onNodeHighlighted = inspector.highlighter.once("node-highlight");
  button = bcButtons.childNodes[2];
  EventUtils.synthesizeMouseAtCenter(
    button,
    { type: "mousemove" },
    button.ownerDocument.defaultView
  );
  await onNodeHighlighted;

  isVisible = await testActor.isHighlighting();
  ok(isVisible, "The highlighter is shown on a markup container hover");

  ok(
    await testActor.assertHighlightedNode("span"),
    "The highlighter highlights the right node"
  );
});
