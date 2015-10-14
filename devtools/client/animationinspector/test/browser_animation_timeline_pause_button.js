/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that the timeline toolbar contains a pause button and that this pause
// button can be clicked. Check that when it is, the current animations
// displayed in the timeline get their playstates changed accordingly, and check
// that the scrubber resumes/stops moving.

add_task(function*() {
  yield addTab(TEST_URL_ROOT + "doc_simple_animation.html");

  let {panel} = yield openAnimationInspector();
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
});
