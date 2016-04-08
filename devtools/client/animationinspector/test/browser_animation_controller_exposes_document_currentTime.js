/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the controller provides the document.timeline currentTime (at least
// the last known version since new animations were added).

add_task(function* () {
  yield addTab(URL_ROOT + "doc_simple_animation.html");
  let {panel, controller} = yield openAnimationInspector();

  ok(controller.documentCurrentTime, "The documentCurrentTime getter exists");
  checkDocumentTimeIsCorrect(controller);
  let time1 = controller.documentCurrentTime;

  yield startNewAnimation(controller, panel);
  checkDocumentTimeIsCorrect(controller);
  let time2 = controller.documentCurrentTime;
  ok(time2 > time1, "The new documentCurrentTime is higher than the old one");
});

function checkDocumentTimeIsCorrect(controller) {
  let time = 0;
  for (let {state} of controller.animationPlayers) {
    time = Math.max(time, state.documentCurrentTime);
  }
  is(controller.documentCurrentTime, time,
     "The documentCurrentTime is correct");
}

function* startNewAnimation(controller, panel) {
  info("Add a new animation to the page and check the time again");
  let onPlayerAdded = controller.once(controller.PLAYERS_UPDATED_EVENT);
  yield executeInContent("devtools:test:setAttribute", {
    selector: ".still",
    attributeName: "class",
    attributeValue: "ball still short"
  });
  yield onPlayerAdded;
  yield waitForAllAnimationTargets(panel);
}
