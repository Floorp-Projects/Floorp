/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

requestLongerTimeout(2);

// Check that the timeline toolbar contains a pause button and that this pause button can
// be clicked. Check that when it is, the button changes state and the scrubber stops and
// resumes.

add_task(async function() {
  await addTab(URL_ROOT + "doc_simple_animation.html");

  let {panel} = await openAnimationInspector();
  let btn = panel.playTimelineButtonEl;

  ok(btn, "The play/pause button exists");
  ok(!btn.classList.contains("paused"), "The play/pause button is in its playing state");

  info("Click on the button to pause all timeline animations");
  await clickTimelinePlayPauseButton(panel);

  ok(btn.classList.contains("paused"), "The play/pause button is in its paused state");
  await assertScrubberMoving(panel, false);

  info("Click again on the button to play all timeline animations");
  await clickTimelinePlayPauseButton(panel);

  ok(!btn.classList.contains("paused"),
     "The play/pause button is in its playing state again");
  await assertScrubberMoving(panel, true);
});
