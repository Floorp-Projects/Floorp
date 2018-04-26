/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable mozilla/no-arbitrary-setTimeout */

"use strict";

requestLongerTimeout(2);

// Checks that the play/pause button goes to the right state when the scrubber has reached
// the end of the timeline but there are infinite animations playing.

add_task(async function() {
  // TODO see if this is needed?
  // let timerPrecision = Preferences.get("privacy.reduceTimerPrecision");
  // Preferences.set("privacy.reduceTimerPrecision", false);

  // registerCleanupFunction(function () {
  //   Preferences.set("privacy.reduceTimerPrecision", timerPrecision);
  // });

  await addTab(URL_ROOT + "doc_simple_animation.html");

  let {panel, inspector} = await openAnimationInspector();
  let timeline = panel.animationsTimelineComponent;
  let btn = panel.playTimelineButtonEl;

  info("Select an infinite animation and wait for the scrubber to reach the end");
  await selectNodeAndWaitForAnimations(".multi", inspector);
  await waitForOutOfBoundScrubber(timeline);

  ok(!btn.classList.contains("paused"),
     "The button is in its playing state still, animations are infinite.");
  await assertScrubberMoving(panel, true);

  info("Click on the button after the scrubber has moved out of bounds");
  await clickTimelinePlayPauseButton(panel);

  ok(btn.classList.contains("paused"),
     "The button can be paused after the scrubber has moved out of bounds");
  await assertScrubberMoving(panel, false);
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
