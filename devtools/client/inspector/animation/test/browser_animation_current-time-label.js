/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for following CurrentTimeLabel component:
// * element existence
// * label content at plural timing

add_task(async function () {
  await addTab(URL_ROOT + "doc_multi_timings.html");
  await removeAnimatedElementsExcept([".keyframes-easing-step"]);
  const { animationInspector, panel } = await openAnimationInspector();

  info("Checking current time label existence");
  const labelEl = panel.querySelector(".current-time-label");
  ok(labelEl, "current time label should exist");

  info("Checking current time label content");
  const duration = animationInspector.state.timeScale.getDuration();
  clickOnCurrentTimeScrubberController(animationInspector, panel, 0.5);
  await waitUntilAnimationsPlayState(animationInspector, "paused");
  await waitUntilCurrentTimeChangedAt(animationInspector, duration * 0.5);
  const targetAnimation = animationInspector.state.animations[0];
  assertLabelContent(labelEl, targetAnimation.state.currentTime);

  clickOnCurrentTimeScrubberController(animationInspector, panel, 0.2);
  await waitUntilCurrentTimeChangedAt(animationInspector, duration * 0.2);
  assertLabelContent(labelEl, targetAnimation.state.currentTime);

  info("Checking current time label content during running");
  // Resume
  clickOnPauseResumeButton(animationInspector, panel);
  const previousContent = labelEl.textContent;

  info("Wait until the time label changes");
  await waitFor(() => labelEl.textContent != previousContent);
  isnot(
    previousContent,
    labelEl.textContent,
    "Current time label should change"
  );
});

function assertLabelContent(labelEl, time) {
  const expected = formatStopwatchTime(time);
  is(labelEl.textContent, expected, `Content of label should be ${expected}`);
}

function formatStopwatchTime(time) {
  // Format falsy values as 0
  if (!time) {
    return "00:00.000";
  }

  let milliseconds = parseInt(time % 1000, 10);
  let seconds = parseInt((time / 1000) % 60, 10);
  let minutes = parseInt(time / (1000 * 60), 10);

  const pad = (nb, max) => {
    if (nb < max) {
      return new Array((max + "").length - (nb + "").length + 1).join("0") + nb;
    }

    return nb;
  };

  minutes = pad(minutes, 10);
  seconds = pad(seconds, 10);
  milliseconds = pad(milliseconds, 100);

  return `${minutes}:${seconds}.${milliseconds}`;
}
