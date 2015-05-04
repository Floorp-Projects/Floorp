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

  let widget = panel.playerWidgets[0];

  // Make sure to wait for the target-retrieved event if the nodeFront hasn't
  // yet been retrieved by the TargetNodeComponent.
  if (!widget.targetNodeComponent.nodeFront) {
    yield widget.targetNodeComponent.once("target-retrieved");
  }

  let targetEl = widget.el.querySelector(".animation-target");
  ok(targetEl, "The player widget has a target element");
  is(targetEl.textContent, "<divid=\"\"class=\"ball animated\">",
    "The target element's content is correct");

  let selectorEl = targetEl.querySelector(".node-selector");
  ok(selectorEl, "The icon to select the target element in the inspector exists");
});
