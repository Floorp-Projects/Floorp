/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

requestLongerTimeout(2);

// Test that the panel content refreshes when new animations are added.

add_task(async function() {
  await addTab(URL_ROOT + "doc_simple_animation.html");
  let {inspector, panel} = await openAnimationInspector();

  info("Select a non animated node");
  await selectNodeAndWaitForAnimations(".still", inspector);

  assertAnimationsDisplayed(panel, 0);

  info("Start an animation on the node");
  let onRendered = waitForAnimationTimelineRendering(panel);
  await changeElementAndWait({
    selector: ".still",
    attributeName: "class",
    attributeValue: "ball animated"
  }, panel, inspector);

  await onRendered;
  await waitForAllAnimationTargets(panel);

  assertAnimationsDisplayed(panel, 1);

  info("Remove the animation class on the node");
  await changeElementAndWait({
    selector: ".ball.animated",
    attributeName: "class",
    attributeValue: "ball still"
  }, panel, inspector);

  assertAnimationsDisplayed(panel, 0);
});

async function changeElementAndWait(options, panel, inspector) {
  let onPanelUpdated = panel.once(panel.UI_UPDATED_EVENT);
  let onInspectorUpdated = inspector.once("inspector-updated");

  await executeInContent("devtools:test:setAttribute", options);

  await promise.all([onInspectorUpdated, onPanelUpdated]);
}
