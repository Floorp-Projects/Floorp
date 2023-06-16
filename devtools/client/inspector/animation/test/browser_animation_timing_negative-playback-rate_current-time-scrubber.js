/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test whether the scrubber was working in case of negative playback rate.

add_task(async function () {
  await addTab(URL_ROOT + "doc_negative_playback_rate.html");
  await removeAnimatedElementsExcept([".normal"]);
  const { animationInspector, panel } = await openAnimationInspector();

  info("Set initial state");
  clickOnCurrentTimeScrubberController(animationInspector, panel, 0);
  await waitUntilAnimationsPlayState(animationInspector, "paused");
  const initialCurrentTime =
    animationInspector.state.animations[0].state.currentTime;
  const initialProgressBarX = getProgressBarX(panel);

  info("Check whether the animation currentTime was decreased");
  clickOnCurrentTimeScrubberController(animationInspector, panel, 0.5);
  await waitUntilCurrentTimeChangedAt(
    animationInspector,
    animationInspector.state.timeScale.getDuration() * 0.5
  );
  ok(
    initialCurrentTime >
      animationInspector.state.animations[0].state.currentTime,
    "currentTime should be decreased"
  );

  info("Check whether the progress bar was moved to left");
  ok(
    initialProgressBarX > getProgressBarX(panel),
    "Progress bar should be moved to left"
  );
});

function getProgressBarX(panel) {
  const areaEl = panel.querySelector(".keyframes-progress-bar-area");
  const barEl = areaEl.querySelector(".keyframes-progress-bar");
  const controllerBounds = areaEl.getBoundingClientRect();
  const barBounds = barEl.getBoundingClientRect();
  const barX = barBounds.x + barBounds.width / 2 - controllerBounds.x;
  return barX;
}
