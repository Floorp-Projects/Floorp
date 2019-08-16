/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// Testing that moving the mouse over the document with the element picker
// started highlights nodes

const NESTED_FRAME_SRC =
  "data:text/html;charset=utf-8," + "nested iframe<div>nested div</div>";

const OUTER_FRAME_SRC =
  "data:text/html;charset=utf-8," +
  "little frame<div>little div</div>" +
  "<iframe src='" +
  NESTED_FRAME_SRC +
  "' />";

const TEST_URI =
  "data:text/html;charset=utf-8," +
  "iframe tests for inspector" +
  '<iframe src="' +
  OUTER_FRAME_SRC +
  '" />';

add_task(async function() {
  const { toolbox, inspector, testActor } = await openInspectorForURL(TEST_URI);
  const outerFrameDiv = ["iframe", "div"];
  const innerFrameDiv = ["iframe", "iframe", "div"];

  info("Waiting for element picker to activate.");
  await startPicker(inspector.toolbox);

  info("Moving mouse over outerFrameDiv");
  await moveMouseOver(outerFrameDiv);
  ok(
    await testActor.assertHighlightedNode(outerFrameDiv),
    "outerFrameDiv is highlighted."
  );

  info("Moving mouse over innerFrameDiv");
  await moveMouseOver(innerFrameDiv);
  ok(
    await testActor.assertHighlightedNode(innerFrameDiv),
    "innerFrameDiv is highlighted."
  );

  info("Selecting root node");
  await selectNode(inspector.walker.rootNode, inspector);

  info("Selecting an element from the nested iframe directly");
  const innerFrameFront = await getNodeFrontInFrame(
    "iframe",
    "iframe",
    inspector
  );
  const innerFrameDivFront = await getNodeFrontInFrame(
    "div",
    innerFrameFront,
    inspector
  );
  await selectNode(innerFrameDivFront, inspector);

  is(
    inspector.breadcrumbs.nodeHierarchy.length,
    9,
    "Breadcrumbs have 9 items."
  );

  info("Waiting for element picker to deactivate.");
  await toolbox.nodePicker.stop();

  function moveMouseOver(selector) {
    info("Waiting for element " + selector + " to be highlighted");
    testActor
      .synthesizeMouse({
        selector: selector,
        options: { type: "mousemove" },
        center: true,
      })
      .then(() => toolbox.nodePicker.once("picker-node-hovered"));
  }
});
