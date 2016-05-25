/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// Testing that moving the mouse over the document with the element picker
// started highlights nodes

const NESTED_FRAME_SRC = "data:text/html;charset=utf-8," +
  "nested iframe<div>nested div</div>";

const OUTER_FRAME_SRC = "data:text/html;charset=utf-8," +
  "little frame<div>little div</div>" +
  "<iframe src='" + NESTED_FRAME_SRC + "' />";

const TEST_URI = "data:text/html;charset=utf-8," +
  "iframe tests for inspector" +
  "<iframe src=\"" + OUTER_FRAME_SRC + "\" />";

add_task(function* () {
  let {inspector, testActor} = yield openInspectorForURL(TEST_URI);
  let outerFrameDiv = ["iframe", "div"];
  let innerFrameDiv = ["iframe", "iframe", "div"];

  info("Waiting for element picker to activate.");
  yield startPicker(inspector.toolbox);

  info("Moving mouse over outerFrameDiv");
  yield moveMouseOver(outerFrameDiv);
  ok((yield testActor.assertHighlightedNode(outerFrameDiv)),
     "outerFrameDiv is highlighted.");

  info("Moving mouse over innerFrameDiv");
  yield moveMouseOver(innerFrameDiv);
  ok((yield testActor.assertHighlightedNode(innerFrameDiv)),
     "innerFrameDiv is highlighted.");

  info("Selecting root node");
  yield selectNode(inspector.walker.rootNode, inspector);

  info("Selecting an element from the nested iframe directly");
  let innerFrameFront = yield getNodeFrontInFrame("iframe", "iframe",
                                                  inspector);
  let innerFrameDivFront = yield getNodeFrontInFrame("div", innerFrameFront,
                                                     inspector);
  yield selectNode(innerFrameDivFront, inspector);

  is(inspector.breadcrumbs.nodeHierarchy.length, 9,
     "Breadcrumbs have 9 items.");

  info("Waiting for element picker to deactivate.");
  yield inspector.toolbox.highlighterUtils.stopPicker();

  function moveMouseOver(selector) {
    info("Waiting for element " + selector + " to be highlighted");
    testActor.synthesizeMouse({
      selector: selector,
      options: {type: "mousemove"},
      center: true
    }).then(() => inspector.toolbox.once("picker-node-hovered"));
  }
});
