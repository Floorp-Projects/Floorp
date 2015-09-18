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

  let {panel} = yield openAnimationInspectorNewUI();
  let btn = panel.playTimelineButtonEl;

  ok(btn, "The play/pause button exists");
  ok(!btn.classList.contains("paused"),
     "The play/pause button is in its playing state");

  info("Click on the button to pause all timeline animations");
  yield clickPlayPauseButton(panel);

  ok(btn.classList.contains("paused"),
     "The play/pause button is in its paused state");
  yield checkIfScrubberMoving(panel, false);

  info("Click again on the button to play all timeline animations");
  yield clickPlayPauseButton(panel);

  ok(!btn.classList.contains("paused"),
     "The play/pause button is in its playing state again");
  yield checkIfScrubberMoving(panel, true);
});

function* clickPlayPauseButton(panel) {
  let onUiUpdated = panel.once(panel.UI_UPDATED_EVENT);

  let btn = panel.playTimelineButtonEl;
  let win = btn.ownerDocument.defaultView;
  EventUtils.sendMouseEvent({type: "click"}, btn, win);

  yield onUiUpdated;
  yield waitForAllAnimationTargets(panel);
}

function* checkIfScrubberMoving(panel, isMoving) {
  let timeline = panel.animationsTimelineComponent;
  let scrubberEl = timeline.scrubberEl;

  if (isMoving) {
    // If we expect the scrubber to move, just wait for a couple of
    // timeline-data-changed events and compare times.
    let {time: time1} = yield timeline.once("timeline-data-changed");
    let {time: time2} = yield timeline.once("timeline-data-changed");
    ok(time2 > time1, "The scrubber is moving");
  } else {
    // If instead we expect the scrubber to remain at its position, just wait
    // for some time. A relatively long timeout is used because the test page
    // has long running animations, so the scrubber doesn't move that quickly.
    let startOffset = scrubberEl.offsetLeft;
    yield new Promise(r => setTimeout(r, 2000));
    let endOffset = scrubberEl.offsetLeft;
    is(startOffset, endOffset, "The scrubber is not moving");
  }
}
