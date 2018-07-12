/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test whether the most left position means negative current time.

add_task(async function() {
  await addTab(URL_ROOT + "doc_multi_timings.html");
  await removeAnimatedElementsExcept([".cssanimation-normal", ".delay-negative"]);
  const { animationInspector, panel, inspector } = await openAnimationInspector();

  info("Checking scrubber controller existence");
  const controllerEl = panel.querySelector(".current-time-scrubber-area");
  ok(controllerEl, "scrubber controller should exist");

  info("Checking the current time of most left scrubber position");
  const timeScale = animationInspector.state.timeScale;
  await clickOnCurrentTimeScrubberController(animationInspector, panel, 0);
  assertAnimationsCurrentTime(animationInspector, -1 * timeScale.zeroPositionTime);

  info("Select negative current time animation");
  await selectNodeAndWaitForAnimations(".cssanimation-normal", inspector);
  assertAnimationsCurrentTime(animationInspector, -1 * timeScale.zeroPositionTime);

  info("Back to 'body' and rewind the animation");
  await selectNodeAndWaitForAnimations("body", inspector);
  await clickOnRewindButton(animationInspector, panel);
  assertAnimationsCurrentTime(animationInspector, 0);
});
