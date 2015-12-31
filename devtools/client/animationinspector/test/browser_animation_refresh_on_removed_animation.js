/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

requestLongerTimeout(2);

// Test that the panel content refreshes when animations are removed.

add_task(function*() {
  yield addTab(TEST_URL_ROOT + "doc_simple_animation.html");

  let {inspector, panel} = yield openAnimationInspector();
  yield testRefreshOnRemove(inspector, panel);
});

function* testRefreshOnRemove(inspector, panel) {
  info("Select a animated node");
  yield selectNode(".animated", inspector);

  assertAnimationsDisplayed(panel, 1);

  info("Listen to the next UI update event");
  let onPanelUpdated = panel.once(panel.UI_UPDATED_EVENT);

  info("Remove the animation on the node by removing the class");
  yield executeInContent("devtools:test:setAttribute", {
    selector: ".animated",
    attributeName: "class",
    attributeValue: "ball still test-node"
  });

  yield onPanelUpdated;
  ok(true, "The panel update event was fired");

  assertAnimationsDisplayed(panel, 0);

  info("Add an finite animation on the node again, and wait for it to appear");
  onPanelUpdated = panel.once(panel.UI_UPDATED_EVENT);
  yield executeInContent("devtools:test:setAttribute", {
    selector: ".test-node",
    attributeName: "class",
    attributeValue: "ball short test-node"
  });
  yield onPanelUpdated;
  yield waitForAllAnimationTargets(panel);

  assertAnimationsDisplayed(panel, 1);
}
