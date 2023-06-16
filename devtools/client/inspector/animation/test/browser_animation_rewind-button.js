/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

requestLongerTimeout(2);

// Test for following RewindButton component:
// * element existence
// * make animations to rewind to zero
// * the state should be always paused after rewinding

add_task(async function () {
  await addTab(URL_ROOT + "doc_multi_timings.html");
  await removeAnimatedElementsExcept([".delay-negative", ".delay-positive"]);
  const { animationInspector, panel } = await openAnimationInspector();

  info("Checking button existence");
  ok(panel.querySelector(".rewind-button"), "Rewind button should exist");

  info("Checking rewind button makes animations to rewind to zero");
  clickOnRewindButton(animationInspector, panel);
  await waitUntilAnimationsPlayState(animationInspector, "paused");
  await waitUntilCurrentTimeChangedAt(animationInspector, 0);
  ok(true, "Rewind button make current time 0");

  info("Checking rewind button makes animations after clicking scrubber");
  clickOnCurrentTimeScrubberController(animationInspector, panel, 0.5);
  clickOnRewindButton(animationInspector, panel);
  await waitUntilCurrentTimeChangedAt(animationInspector, 0);
  await waitUntilAnimationsPlayState(animationInspector, "paused");
  ok(true, "Rewind button make current time 0 even after clicking scrubber");
});
