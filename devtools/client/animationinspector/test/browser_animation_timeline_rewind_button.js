/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

requestLongerTimeout(2);

// Check that the timeline toolbar contains a rewind button and that it can be
// clicked. Check that when it is, the current animations displayed in the
// timeline get their playstates changed to paused, and their currentTimes
// reset to 0, and that the scrubber stops moving and is positioned to the
// start.

add_task(function* () {
  yield addTab(URL_ROOT + "doc_simple_animation.html");

  let {panel, controller} = yield openAnimationInspector();
  let players = controller.animationPlayers;
  let btn = panel.rewindTimelineButtonEl;

  ok(btn, "The rewind button exists");

  info("Click on the button to rewind all timeline animations");
  yield clickTimelineRewindButton(panel);

  info("Check that the scrubber has stopped moving");
  yield assertScrubberMoving(panel, false);

  ok(players.every(({state}) => state.currentTime === 0),
     "All animations' currentTimes have been set to 0");
  ok(players.every(({state}) => state.playState === "paused"),
     "All animations have been paused");

  info("Play the animations again");
  yield clickTimelinePlayPauseButton(panel);

  info("And pause them after a short while");
  yield new Promise(r => setTimeout(r, 200));

  info("Check that rewinding when animations are paused works too");
  yield clickTimelineRewindButton(panel);

  info("Check that the scrubber has stopped moving");
  yield assertScrubberMoving(panel, false);

  ok(players.every(({state}) => state.currentTime === 0),
     "All animations' currentTimes have been set to 0");
  ok(players.every(({state}) => state.playState === "paused"),
     "All animations have been paused");
});
