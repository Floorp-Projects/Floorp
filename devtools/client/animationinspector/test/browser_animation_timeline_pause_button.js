/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

requestLongerTimeout(2);

// Check that the timeline toolbar contains a pause button and that this pause
// button can be clicked. Check that when it is, the current animations
// displayed in the timeline get their playstates changed accordingly, and check
// that the scrubber resumes/stops moving.
// Also checks that the button goes to the right state when the scrubber has
// reached the end of the timeline: continues to be in playing mode for infinite
// animations, goes to paused mode otherwise.
// And test that clicking the button once the scrubber has reached the end of
// the timeline does the right thing.

add_task(function* () {
  yield addTab(URL_ROOT + "doc_simple_animation.html");

  let {panel, inspector} = yield openAnimationInspector();
  let timeline = panel.animationsTimelineComponent;
  let btn = panel.playTimelineButtonEl;

  ok(btn, "The play/pause button exists");
  ok(!btn.classList.contains("paused"),
     "The play/pause button is in its playing state");

  info("Click on the button to pause all timeline animations");
  yield clickTimelinePlayPauseButton(panel);

  ok(btn.classList.contains("paused"),
     "The play/pause button is in its paused state");
  yield assertScrubberMoving(panel, false);

  info("Click again on the button to play all timeline animations");
  yield clickTimelinePlayPauseButton(panel);

  ok(!btn.classList.contains("paused"),
     "The play/pause button is in its playing state again");
  yield assertScrubberMoving(panel, true);

  // Some animations on the test page are infinite, so the scrubber won't stop
  // at the end of the timeline, and the button should remain in play mode.
  info("Select an infinite animation, reload the page and wait for the " +
       "animation to complete");
  yield selectNodeAndWaitForAnimations(".multi", inspector);
  yield reloadTab(inspector);
  yield waitForOutOfBoundScrubber(timeline);

  ok(!btn.classList.contains("paused"),
     "The button is in its playing state still, animations are infinite.");
  yield assertScrubberMoving(panel, true);

  info("Click on the button after the scrubber has moved out of bounds");
  yield clickTimelinePlayPauseButton(panel);

  ok(btn.classList.contains("paused"),
     "The button can be paused after the scrubber has moved out of bounds");
  yield assertScrubberMoving(panel, false);

  // For a finite animation though, once the scrubber reaches the end of the
  // timeline, it should go back to paused mode.
  info("Select a finite animation, reload the page and wait for the " +
       "animation to complete");
  yield selectNodeAndWaitForAnimations(".negative-delay", inspector);

  let onScrubberStopped = waitForScrubberStopped(timeline);
  yield reloadTab(inspector);
  yield onScrubberStopped;

  ok(btn.classList.contains("paused"),
     "The button is in paused state once finite animations are done");
  yield assertScrubberMoving(panel, false);

  info("Click again on the button to play the animation from the start again");
  yield clickTimelinePlayPauseButton(panel);

  ok(!btn.classList.contains("paused"),
     "Clicking the button once finite animations are done should restart them");
  yield assertScrubberMoving(panel, true);
});

function waitForOutOfBoundScrubber({win, scrubberEl}) {
  return new Promise(resolve => {
    function check() {
      let pos = scrubberEl.getBoxQuads()[0].bounds.right;
      let width = win.document.documentElement.offsetWidth;
      if (pos >= width) {
        setTimeout(resolve, 50);
      } else {
        setTimeout(check, 50);
      }
    }
    check();
  });
}

function waitForScrubberStopped(timeline) {
  return new Promise(resolve => {
    timeline.on("timeline-data-changed",
      function onTimelineData(e, {isMoving}) {
        if (!isMoving) {
          timeline.off("timeline-data-changed", onTimelineData);
          resolve();
        }
      });
  });
}
