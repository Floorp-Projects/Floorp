/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for following PauseResumeButton component:
// * element existence
// * state during running animations
// * state during pausing animations
// * make animations to pause by push button
// * make animations to resume by push button

add_task(async function () {
  await addTab(URL_ROOT + "doc_custom_playback_rate.html");
  const { animationInspector, panel } = await openAnimationInspector();

  info("Checking pause/resume button existence");
  const buttonEl = panel.querySelector(".pause-resume-button");
  ok(buttonEl, "pause/resume button should exist");

  info("Checking state during running animations");
  ok(
    !buttonEl.classList.contains("paused"),
    "State of button should be running"
  );

  info("Checking button makes animations to pause");
  clickOnPauseResumeButton(animationInspector, panel);
  await waitUntilAnimationsPlayState(animationInspector, "paused");
  ok(true, "All of animtion are paused");
  ok(buttonEl.classList.contains("paused"), "State of button should be paused");

  info("Checking button makes animations to resume");
  clickOnPauseResumeButton(animationInspector, panel);
  await waitUntilAnimationsPlayState(animationInspector, "running");
  ok(true, "All of animtion are running");
  ok(
    !buttonEl.classList.contains("paused"),
    "State of button should be resumed"
  );
});
