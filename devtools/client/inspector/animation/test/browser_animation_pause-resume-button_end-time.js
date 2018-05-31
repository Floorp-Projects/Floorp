/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test whether the animation can rewind if the current time is over end time when
// the resume button clicked.

add_task(async function() {
  await addTab(URL_ROOT + "doc_simple_animation.html");
  await removeAnimatedElementsExcept([".animated", ".end-delay", ".long"]);
  const { animationInspector, panel } = await openAnimationInspector();

  info("Check animations state after resuming with infinite animation");
  info("Make the current time of animation to be over its end time");
  await clickOnCurrentTimeScrubberController(animationInspector, panel, 1);
  info("Resume animations");
  await clickOnPauseResumeButton(animationInspector, panel);
  await wait(1000);
  assertPlayState(animationInspector.state.animations,
                  ["running", "finished", "finished"]);
  await clickOnCurrentTimeScrubberController(animationInspector, panel, 0);

  info("Check animations state after resuming without infinite animation");
  info("Remove infinite animation");
  await setClassAttribute(animationInspector, ".animated", "ball still");
  info("Make the current time of animation to be over its end time");
  await clickOnCurrentTimeScrubberController(animationInspector, panel, 1);
  await clickOnPlaybackRateSelector(animationInspector, panel, 10);
  info("Resume animations");
  await clickOnPauseResumeButton(animationInspector, panel);
  assertPlayState(animationInspector.state.animations, ["running", "running"]);
  assertCurrentTimeLessThanDuration(animationInspector.state.animations);
  assertScrubberPosition(panel);
});

function assertPlayState(animations, expectedState) {
  animations.forEach((animation, index) => {
    is(animation.state.playState, expectedState[index],
       `The playState of animation [${ index }] should be ${ expectedState[index] }`);
  });
}

function assertCurrentTimeLessThanDuration(animations) {
  animations.forEach((animation, index) => {
    ok(animation.state.currentTime < animation.state.duration,
       `The current time of animation[${ index }] should be less than its duration`);
  });
}

function assertScrubberPosition(panel) {
  const scrubberEl = panel.querySelector(".current-time-scrubber");
  const translateX = parseFloat(scrubberEl.style.transform.match(/-?\d+(\.\d+)?/)[0]);
  ok(translateX >= 0, "The translateX of scrubber position should be zero or more");
}
