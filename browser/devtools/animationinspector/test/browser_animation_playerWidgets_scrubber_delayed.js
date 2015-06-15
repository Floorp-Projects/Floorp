/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the currentTime timeline doesn't move if the animation is currently
// waiting for an animation-delay.

add_task(function*() {
  yield addTab(TEST_URL_ROOT + "doc_simple_animation.html");
  let {inspector, panel} = yield openAnimationInspector();

  info("Select the delayed animation node");
  yield selectNode(".delayed", inspector);

  let widget = panel.playerWidgets[0];

  let timeline = widget.currentTimeEl;
  is(timeline.value, 0, "The timeline is at 0 since the animation hasn't started");

  let timeLabel = widget.timeDisplayEl;
  is(timeLabel.textContent, "0s", "The current time is 0");
});
