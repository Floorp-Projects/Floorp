/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that the scrubber in the timeline-based UI can be moved by clicking &
// dragging in the header area.

add_task(function*() {
  yield addTab(TEST_URL_ROOT + "doc_simple_animation.html");

  let {panel} = yield openAnimationInspectorNewUI();
  yield waitForAllAnimationTargets(panel);

  let timeline = panel.animationsTimelineComponent;
  let win = timeline.win;
  let timeHeaderEl = timeline.timeHeaderEl;
  let scrubberEl = timeline.scrubberEl;

  info("Mousedown in the header to move the scrubber");
  EventUtils.synthesizeMouse(timeHeaderEl, 50, 1, {type: "mousedown"}, win);
  let newPos = parseInt(scrubberEl.style.left);
  is(newPos, 50, "The scrubber moved on mousedown");

  info("Continue moving the mouse and verify that the scrubber tracks it");
  EventUtils.synthesizeMouse(timeHeaderEl, 100, 1, {type: "mousemove"}, win);
  newPos = parseInt(scrubberEl.style.left);
  is(newPos, 100, "The scrubber followed the mouse");

  info("Release the mouse and move again and verify that the scrubber stays");
  EventUtils.synthesizeMouse(timeHeaderEl, 100, 1, {type: "mouseup"}, win);
  EventUtils.synthesizeMouse(timeHeaderEl, 200, 1, {type: "mousemove"}, win);
  newPos = parseInt(scrubberEl.style.left);
  is(newPos, 100, "The scrubber stopped following the mouse");
});
