/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that player widgets display information about target nodes

add_task(function*() {
  yield addTab(TEST_URL_ROOT + "doc_simple_animation.html");
  let {inspector, panel} = yield openAnimationInspector();

  info("Select the simple animated node");
  yield selectNode(".animated", inspector);

  let targetNodeComponent = panel.animationsTimelineComponent.targetNodes[0];
  // Make sure to wait for the target-retrieved event if the nodeFront hasn't
  // yet been retrieved by the TargetNodeComponent.
  if (!targetNodeComponent.nodeFront) {
    yield targetNodeComponent.once("target-retrieved");
  }

  is(targetNodeComponent.el.textContent, "div#.ball.animated",
    "The target element's content is correct");

  let highlighterEl = targetNodeComponent.el.querySelector(".node-highlighter");
  ok(highlighterEl,
    "The icon to highlight the target element in the page exists");
});
