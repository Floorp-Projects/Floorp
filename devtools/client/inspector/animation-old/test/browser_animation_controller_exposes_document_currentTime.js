/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the controller provides the document.timeline currentTime (at least
// the last known version since new animations were added).

add_task(async function() {
  await addTab(URL_ROOT + "doc_simple_animation.html");
  const {panel, controller} = await openAnimationInspector();

  ok(controller.documentCurrentTime, "The documentCurrentTime getter exists");
  checkDocumentTimeIsCorrect(controller);
  const time1 = controller.documentCurrentTime;

  await startNewAnimation(controller, panel);
  checkDocumentTimeIsCorrect(controller);
  const time2 = controller.documentCurrentTime;
  ok(time2 > time1, "The new documentCurrentTime is higher than the old one");
});

function checkDocumentTimeIsCorrect(controller) {
  let time = 0;
  for (const {state} of controller.animationPlayers) {
    time = Math.max(time, state.documentCurrentTime);
  }
  is(controller.documentCurrentTime, time,
     "The documentCurrentTime is correct");
}

async function startNewAnimation(controller, panel) {
  info("Add a new animation to the page and check the time again");
  const onPlayerAdded = controller.once(controller.PLAYERS_UPDATED_EVENT);
  const onRendered = waitForAnimationTimelineRendering(panel);

  await executeInContent("devtools:test:setAttribute", {
    selector: ".still",
    attributeName: "class",
    attributeValue: "ball still short"
  });

  await onPlayerAdded;
  await onRendered;
  await waitForAllAnimationTargets(panel);
}
