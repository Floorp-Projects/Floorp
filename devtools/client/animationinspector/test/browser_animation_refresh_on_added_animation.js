/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

requestLongerTimeout(2);

// Test that the panel content refreshes when new animations are added.

add_task(function* () {
  yield addTab(URL_ROOT + "doc_simple_animation.html");
  let {inspector, panel} = yield openAnimationInspector();

  info("Select a non animated node");
  yield selectNodeAndWaitForAnimations(".still", inspector);

  assertAnimationsDisplayed(panel, 0);

  info("Start an animation on the node");
  yield changeElementAndWait({
    selector: ".still",
    attributeName: "class",
    attributeValue: "ball animated"
  }, panel, inspector);

  assertAnimationsDisplayed(panel, 1);

  info("Remove the animation class on the node");
  yield changeElementAndWait({
    selector: ".ball.animated",
    attributeName: "class",
    attributeValue: "ball still"
  }, panel, inspector);

  assertAnimationsDisplayed(panel, 0);
});

function* changeElementAndWait(options, panel, inspector) {
  let onPanelUpdated = panel.once(panel.UI_UPDATED_EVENT);
  let onInspectorUpdated = inspector.once("inspector-updated");

  yield executeInContent("devtools:test:setAttribute", options);

  yield promise.all([
    onInspectorUpdated, onPanelUpdated, waitForAllAnimationTargets(panel)]);
}
