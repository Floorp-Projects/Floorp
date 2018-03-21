/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

requestLongerTimeout(2);

// Check that the timeline is displays as many iteration elements as there are
// iterations in an animation.

add_task(async function() {
  await addTab(URL_ROOT + "doc_simple_animation.html");
  let {inspector, panel} = await openAnimationInspector();

  info("Selecting the test node");
  await selectNodeAndWaitForAnimations(".delayed", inspector);

  info("Getting the animation element from the panel");
  const timelineComponent = panel.animationsTimelineComponent;
  const timelineEl = timelineComponent.rootWrapperEl;
  let animation = timelineEl.querySelector(".time-block");

  // Get iteration count from summary graph path.
  let iterationCount = getIterationCount(animation);

  is(animation.querySelectorAll("svg g").length, 1,
     "The animation timeline contains one g element");
  is(iterationCount, 10,
     "The animation timeline contains the right number of iterations");
  ok(!animation.querySelector(".infinity"),
     "The summary graph does not have any elements "
     + " that have infinity class");

  info("Selecting another test node with an infinite animation");
  await selectNodeAndWaitForAnimations(".animated", inspector);

  info("Getting the animation element from the panel again");
  animation = timelineEl.querySelector(".time-block");
  iterationCount = getIterationCount(animation);

  is(animation.querySelectorAll("svg g").length, 1,
     "The animation timeline contains one g element");
  is(iterationCount, 1,
     "The animation timeline contains one iteration");
  ok(animation.querySelector(".infinity"),
     "The summary graph has an element that has infinity class");
});

function getIterationCount(timeblockEl) {
  return timeblockEl.querySelectorAll("svg g .iteration-path").length;
}
