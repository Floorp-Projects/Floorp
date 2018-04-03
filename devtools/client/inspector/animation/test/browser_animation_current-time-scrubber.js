/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for following CurrentTimeScrubber and CurrentTimeScrubberController components:
// * element existence
// * scrubber position validity
// * make animations currentTime to change by click on the controller
// * mouse drag on the scrubber

add_task(async function() {
  await addTab(URL_ROOT + "doc_simple_animation.html");
  await removeAnimatedElementsExcept([".long"]);
  const { animationInspector, panel } = await openAnimationInspector();

  info("Checking scrubber controller existence");
  const controllerEl = panel.querySelector(".current-time-scrubber-controller");
  ok(controllerEl, "scrubber controller should exist");

  info("Checking scrubber existence");
  const scrubberEl = controllerEl.querySelector(".current-time-scrubber");
  ok(scrubberEl, "scrubber should exist");

  info("Checking scrubber changes current time of animation and the position");
  const duration = animationInspector.state.timeScale.getDuration();
  await clickOnCurrentTimeScrubberController(animationInspector, panel, 0);
  assertAnimationsCurrentTime(animationInspector, 0);
  assertPosition(scrubberEl, controllerEl, 0, animationInspector);

  await clickOnCurrentTimeScrubberController(animationInspector, panel, 1);
  assertAnimationsCurrentTime(animationInspector, duration);
  assertPosition(scrubberEl, controllerEl, duration, animationInspector);

  await clickOnCurrentTimeScrubberController(animationInspector, panel, 0.5);
  assertAnimationsCurrentTime(animationInspector, duration * 0.5);
  assertPosition(scrubberEl, controllerEl, duration * 0.5, animationInspector);

  info("Checking current time scrubber position during running");
  // Running again
  await clickOnPauseResumeButton(animationInspector, panel);
  let previousX = scrubberEl.getBoundingClientRect().x;
  await wait(100);
  let currentX = scrubberEl.getBoundingClientRect().x;
  isnot(previousX, currentX, "Scrubber should be moved");

  info("Checking draggable on scrubber over animation list");
  await clickOnPauseResumeButton(animationInspector, panel);
  previousX = scrubberEl.getBoundingClientRect().x;
  await dragOnCurrentTimeScrubber(animationInspector, panel, 0.5, 2, 30);
  currentX = scrubberEl.getBoundingClientRect().x;
  isnot(previousX, currentX, "Scrubber should be draggable");

  info("Checking a behavior which mouse out from animation inspector area " +
       "during dragging from controller");
  await dragOnCurrentTimeScrubberController(animationInspector, panel, 0.5, 2);
  ok(!panel.querySelector(".animation-list-container")
           .classList.contains("active-scrubber"), "Click and DnD should be inactive");
});

function assertPosition(scrubberEl, controllerEl, time, animationInspector) {
  const controllerBounds = controllerEl.getBoundingClientRect();
  const scrubberBounds = scrubberEl.getBoundingClientRect();
  const scrubberX = scrubberBounds.x + scrubberBounds.width / 2 - controllerBounds.x;
  const timeScale = animationInspector.state.timeScale;
  const expected = Math.round(time / timeScale.getDuration() * controllerBounds.width);
  is(scrubberX, expected, `Position should be ${ expected } at ${ time }ms`);
}
