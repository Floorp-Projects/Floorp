/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test whether the scrubber was working for even the animation of infinity duration.

add_task(async function() {
  await addTab(URL_ROOT + "doc_infinity_duration.html");
  await removeAnimatedElementsExcept([".infinity-delay-iteration-start"]);
  const { animationInspector, panel } = await openAnimationInspector();

  info("Set initial state");
  await clickOnCurrentTimeScrubberController(animationInspector, panel, 0);
  const initialCurrentTime = animationInspector.state.animations[0].state.currentTime;

  info("Check whether the animation currentTime was increased");
  await clickOnCurrentTimeScrubberController(animationInspector, panel, 1);
  ok(initialCurrentTime <= animationInspector.state.animations[0].state.currentTime,
     "currentTime should be increased");

  info("Check whether the progress bar was moved");
  const areaEl = panel.querySelector(".keyframes-progress-bar-area");
  const barEl = areaEl.querySelector(".keyframes-progress-bar");
  const controllerBounds = areaEl.getBoundingClientRect();
  const barBounds = barEl.getBoundingClientRect();
  const barX = barBounds.x + barBounds.width / 2 - controllerBounds.x;
  const expectedBarX = controllerBounds.width * 0.5;
  ok(Math.abs(barX - expectedBarX) < 1,
     "Progress bar should indicate at progress of 0.5");
});
