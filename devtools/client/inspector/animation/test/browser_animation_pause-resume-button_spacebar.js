/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for following PauseResumeButton component with spacebar:
// * make animations to pause/resume by spacebar
// * combination with other UI components

add_task(async function() {
  await addTab(URL_ROOT + "doc_custom_playback_rate.html");
  const { animationInspector, panel } = await openAnimationInspector();

  info("Checking spacebar makes animations to pause");
  await testPauseAndResumeBySpacebar(animationInspector, panel);

  info("Checking spacebar makes animations to pause when the button has the focus");
  const pauseResumeButton = panel.querySelector(".pause-resume-button");
  await testPauseAndResumeBySpacebar(animationInspector, pauseResumeButton);

  info("Checking spacebar works with other UI components");
  // To pause
  await clickOnPauseResumeButton(animationInspector, panel);
  // To resume
  await sendSpaceKeyEvent(animationInspector, panel);
  assertAnimationsRunning(animationInspector);
  // To pause
  await clickOnCurrentTimeScrubberController(animationInspector, panel, 0.5);
  // To resume
  await clickOnPauseResumeButton(animationInspector, panel);
  // To pause
  await sendSpaceKeyEvent(animationInspector, panel);
  assertAnimationsPausing(animationInspector);
});

async function testPauseAndResumeBySpacebar(animationInspector, element) {
  await sendSpaceKeyEvent(animationInspector, element);
  assertAnimationsPausing(animationInspector);
  await sendSpaceKeyEvent(animationInspector, element);
  assertAnimationsRunning(animationInspector);
}
