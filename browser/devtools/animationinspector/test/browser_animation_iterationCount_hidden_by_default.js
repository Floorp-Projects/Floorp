/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that iteration count is only shown in the UI when it's different than 1

add_task(function*() {
  yield addTab(TEST_URL_ROOT + "doc_simple_animation.html");
  let {inspector, panel} = yield openAnimationInspector();

  info("Selecting a node with an animation that doesn't repeat");
  yield selectNode(".long", inspector);
  let widget = panel.playerWidgets[0];

  ok(isNodeVisible(widget.metaDataComponent.durationValue),
    "The duration value is shown");
  ok(!isNodeVisible(widget.metaDataComponent.delayValue),
    "The delay value is hidden");
  ok(!isNodeVisible(widget.metaDataComponent.iterationValue),
    "The iteration count is hidden");

  info("Selecting a node with an animation that repeats several times");
  yield selectNode(".delayed", inspector);
  widget = panel.playerWidgets[0];

  ok(isNodeVisible(widget.metaDataComponent.durationValue),
    "The duration value is shown");
  ok(isNodeVisible(widget.metaDataComponent.delayValue),
    "The delay value is shown");
  ok(isNodeVisible(widget.metaDataComponent.iterationValue),
    "The iteration count is shown");
});
