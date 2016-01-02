/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

requestLongerTimeout(2);

// Check that the scrubber in the timeline moves when animations are playing.
// The animations in the test page last for a very long time, so the test just
// measures the position of the scrubber once, then waits for some time to pass
// and measures its position again.

add_task(function*() {
  yield addTab(TEST_URL_ROOT + "doc_simple_animation.html");
  let {panel} = yield openAnimationInspector();

  let timeline = panel.animationsTimelineComponent;
  let scrubberEl = timeline.scrubberEl;
  let startPos = scrubberEl.getBoundingClientRect().left;

  info("Wait for some time to check that the scrubber moves");
  yield new Promise(r => setTimeout(r, 2000));

  let endPos = scrubberEl.getBoundingClientRect().left;

  ok(endPos > startPos, "The scrubber has moved");
});
