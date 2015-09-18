/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the panel content refreshes when new animations are added.

add_task(function*() {
  yield addTab(TEST_URL_ROOT + "doc_simple_animation.html");

  let {inspector, panel} = yield openAnimationInspector();
  yield testRefreshOnNewAnimation(inspector, panel);
});

function* testRefreshOnNewAnimation(inspector, panel) {
  info("Select a non animated node");
  yield selectNode(".still", inspector);

  assertAnimationsDisplayed(panel, 0);

  info("Listen to the next UI update event");
  let onPanelUpdated = panel.once(panel.UI_UPDATED_EVENT);

  info("Start an animation on the node");
  yield executeInContent("devtools:test:setAttribute", {
    selector: ".still",
    attributeName: "class",
    attributeValue: "ball animated"
  });

  yield onPanelUpdated;
  ok(true, "The panel update event was fired");

  assertAnimationsDisplayed(panel, 1);

  info("Remove the animation class on the node");
  onPanelUpdated = panel.once(panel.UI_UPDATED_EVENT);
  yield executeInContent("devtools:test:setAttribute", {
    selector: ".ball.animated",
    attributeName: "class",
    attributeValue: "ball still"
  });
  yield onPanelUpdated;
}
