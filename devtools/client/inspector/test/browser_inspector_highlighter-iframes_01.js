/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// Testing that moving the mouse over the document with the element picker
// started highlights nodes

const NESTED_FRAME_SRC =
  "data:text/html;charset=utf-8," +
  "nested iframe<section>nested div</section>";

const OUTER_FRAME_SRC =
  "data:text/html;charset=utf-8," +
  "little frame<main>little div</main>" +
  "<iframe src='" +
  NESTED_FRAME_SRC +
  "' />";

const TEST_URI =
  "data:text/html;charset=utf-8," +
  "iframe tests for inspector" +
  '<iframe src="' +
  OUTER_FRAME_SRC +
  '" />';

add_task(async function () {
  const { toolbox, inspector } = await openInspectorForURL(TEST_URI);
  const outerFrameMainSelector = ["iframe", "main"];
  const innerFrameSectionSelector = ["iframe", "iframe", "section"];

  const outerFrameMainNodeFront = await getNodeFrontInFrames(
    outerFrameMainSelector,
    inspector
  );
  const outerFrameHighlighterTestFront = await getHighlighterTestFront(
    toolbox,
    { target: outerFrameMainNodeFront.targetFront }
  );

  const innerFrameSectionNodeFront = await getNodeFrontInFrames(
    innerFrameSectionSelector,
    inspector
  );
  const innerFrameHighlighterTestFront = await getHighlighterTestFront(
    toolbox,
    { target: innerFrameSectionNodeFront.targetFront }
  );

  info("Waiting for element picker to activate.");
  await startPicker(inspector.toolbox);

  info("Moving mouse over outerFrameDiv");
  await hoverElement(inspector, outerFrameMainSelector);

  ok(
    await outerFrameHighlighterTestFront.assertHighlightedNode(
      isEveryFrameTargetEnabled()
        ? outerFrameMainSelector.at(-1)
        : outerFrameMainSelector
    ),
    "outerFrameDiv is highlighted."
  );

  info("Moving mouse over innerFrameDiv");
  await hoverElement(inspector, innerFrameSectionSelector);
  ok(
    await innerFrameHighlighterTestFront.assertHighlightedNode(
      isEveryFrameTargetEnabled()
        ? innerFrameSectionSelector.at(-1)
        : innerFrameSectionSelector
    ),
    "innerFrameDiv is highlighted."
  );

  info("Selecting root node");
  await selectNode(inspector.walker.rootNode, inspector);

  info("Selecting an element from the nested iframe directly");
  await selectNodeInFrames(innerFrameSectionSelector, inspector);

  is(
    inspector.breadcrumbs.nodeHierarchy.length,
    9,
    "Breadcrumbs have 9 items."
  );

  info("Waiting for element picker to deactivate.");
  await toolbox.nodePicker.stop({ canceled: true });
});
