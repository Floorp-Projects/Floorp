/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that if an animation has had its playbackRate changed via the DOM, then
// the timeline UI shows the right delay and duration.
// Indeed, the header in the timeline UI always shows the unaltered time,
// because there might be multiple animations displayed at the same time, some
// of which may have a different rate than others. Those that have had their
// rate changed have a delay = delay/rate and a duration = duration/rate.

add_task(function*() {
  yield addTab(TEST_URL_ROOT + "doc_modify_playbackRate.html");

  let {panel} = yield openAnimationInspectorNewUI();
  yield waitForAllAnimationTargets(panel);

  let timelineEl = panel.animationsTimelineComponent.rootWrapperEl;

  let timeBlocks = timelineEl.querySelectorAll(".time-block");
  is(timeBlocks.length, 2, "2 animations are displayed");

  info("The first animation has its rate set to 1, let's measure it");

  let el = timeBlocks[0];
  let duration = parseInt(el.querySelector(".iterations").style.width, 10);
  let delay = parseInt(el.querySelector(".delay").style.width, 10);

  info("The second animation has its rate set to 2, so should be shorter");

  let el2 = timeBlocks[1];
  let duration2 = parseInt(el2.querySelector(".iterations").style.width, 10);
  let delay2 = parseInt(el2.querySelector(".delay").style.width, 10);

  // The width are calculated by the animation-inspector dynamically depending
  // on the size of the panel, and therefore depends on the test machine/OS.
  // Let's not try to be too precise here and compare numbers.
  let durationDelta = (2 * duration2) - duration;
  ok(durationDelta <= 1, "The duration width is correct");
  let delayDelta = (2 * delay2) - delay;
  ok(delayDelta <= 1, "The delay width is correct");
});
