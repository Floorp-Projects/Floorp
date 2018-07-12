/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for following RewindButton component:
// * element existence
// * make animations to rewind to zero
// * the state should be always paused after rewinding

add_task(async function() {
  await addTab(URL_ROOT + "doc_multi_timings.html");
  const { animationInspector, panel } = await openAnimationInspector();
  await removeAnimatedElementsExcept([".animated",
                                      ".negative-delay"]);

  info("Checking button existence");
  ok(panel.querySelector(".rewind-button"), "Rewind button should exist");

  info("Checking rewind button makes animations to rewind to zero");
  await clickOnRewindButton(animationInspector, panel);
  assertAnimationsCurrentTime(animationInspector, 0);
  assertAnimationsPausing(animationInspector, panel);

  info("Checking rewind button makes animations after clicking scrubber");
  await clickOnCurrentTimeScrubberController(animationInspector, panel, 0.5);
  await clickOnRewindButton(animationInspector, panel);
  assertAnimationsCurrentTime(animationInspector, 0);
  assertAnimationsPausing(animationInspector, panel);
});
