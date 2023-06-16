/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for following PauseResumeButton component with spacebar:
// * make animations to pause/resume by spacebar
// * combination with other UI components

add_task(async function () {
  await addTab(URL_ROOT + "doc_custom_playback_rate.html");
  const { animationInspector, panel } = await openAnimationInspector();

  info("Checking spacebar makes animations to pause");
  await testPauseAndResumeBySpacebar(animationInspector, panel);

  info(
    "Checking spacebar makes animations to pause when the button has the focus"
  );
  const pauseResumeButton = panel.querySelector(".pause-resume-button");
  await testPauseAndResumeBySpacebar(animationInspector, pauseResumeButton);

  info("Checking spacebar works with other UI components");
  // To pause
  clickOnPauseResumeButton(animationInspector, panel);
  await waitUntilAnimationsPlayState(animationInspector, "paused");
  // To resume
  sendSpaceKeyEvent(animationInspector, panel);
  await waitUntilAnimationsPlayState(animationInspector, "running");
  // To pause
  clickOnCurrentTimeScrubberController(animationInspector, panel, 0.5);
  await waitUntilAnimationsPlayState(animationInspector, "paused");
  // To resume
  clickOnPauseResumeButton(animationInspector, panel);
  await waitUntilAnimationsPlayState(animationInspector, "running");
  // To pause
  sendSpaceKeyEvent(animationInspector, panel);
  await waitUntilAnimationsPlayState(animationInspector, "paused");
  ok(true, "All components that can make animations pause/resume works fine");
});

async function testPauseAndResumeBySpacebar(animationInspector, element) {
  await sendSpaceKeyEvent(animationInspector, element);
  await waitUntilAnimationsPlayState(animationInspector, "paused");
  ok(true, "Space key can pause animations");
  await sendSpaceKeyEvent(animationInspector, element);
  await waitUntilAnimationsPlayState(animationInspector, "running");
  ok(true, "Space key can resume animations");
}
