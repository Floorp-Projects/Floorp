/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Animation inspector makes the current time to stop
// after end of animation duration except iterations infinity.
// Test followings:
// * state of animations and UI components after end of animation duration
// * state of animations and UI components after end of animation duration
//   but iteration count is infinity

add_task(async function() {
  await addTab(URL_ROOT + "doc_simple_animation.html");
  await removeAnimatedElementsExcept([".compositor-all", ".long"]);
  const {
    animationInspector,
    inspector,
    panel,
  } = await openAnimationInspector();

  info("Checking state after end of animation duration");
  await selectNodeAndWaitForAnimations(".long", inspector);
  const pixelsData = getDurationAndRate(animationInspector, panel, 5);
  await clickOnCurrentTimeScrubberController(
    animationInspector,
    panel,
    1 - pixelsData.rate
  );
  await clickOnPauseResumeButton(animationInspector, panel);
  // Must be able to catch rendering event after stopping the animation.
  await waitForSummaryAndDetail(animationInspector);
  await assertStates(animationInspector, panel, false);

  info(
    "Checking state after end of animation duration and infinity iterations"
  );
  await clickOnPauseResumeButton(animationInspector, panel);
  await selectNodeAndWaitForAnimations(".compositor-all", inspector);
  await clickOnCurrentTimeScrubberController(animationInspector, panel, 1);
  await clickOnPauseResumeButton(animationInspector, panel);
  await assertStates(animationInspector, panel, true);
});

async function assertStates(animationInspector, panel, shouldRunning) {
  const buttonEl = panel.querySelector(".pause-resume-button");
  const labelEl = panel.querySelector(".current-time-label");
  const scrubberEl = panel.querySelector(".current-time-scrubber");

  const previousLabelContent = labelEl.textContent;
  const previousScrubberX = scrubberEl.getBoundingClientRect().x;
  await wait(100);
  const currentLabelContent = labelEl.textContent;
  const currentScrubberX = scrubberEl.getBoundingClientRect().x;

  if (shouldRunning) {
    isnot(
      previousLabelContent,
      currentLabelContent,
      "Current time label content should change"
    );
    isnot(
      previousScrubberX,
      currentScrubberX,
      "Current time scrubber position should change"
    );
    ok(
      !buttonEl.classList.contains("paused"),
      "State of button should be running"
    );
    assertAnimationsRunning(animationInspector);
  } else {
    is(
      previousLabelContent,
      currentLabelContent,
      "Current time label Content should not change"
    );
    is(
      previousScrubberX,
      currentScrubberX,
      "Current time scrubber position should not change"
    );
    ok(
      buttonEl.classList.contains("paused"),
      "State of button should be paused"
    );
    assertAnimationsPausing(animationInspector);
  }
}
