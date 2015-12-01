/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that the scrubber in the timeline can be moved by clicking & dragging
// in the header area.
// Also check that doing so changes the timeline's play/pause button to paused
// state.
// Finally, also check that the scrubber can be moved using the scrubber handle.

add_task(function*() {
  yield addTab(TEST_URL_ROOT + "doc_simple_animation.html");

  let {panel} = yield openAnimationInspector();
  let timeline = panel.animationsTimelineComponent;
  let {win, timeHeaderEl, scrubberEl, scrubberHandleEl} = timeline;
  let playTimelineButtonEl = panel.playTimelineButtonEl;

  ok(!playTimelineButtonEl.classList.contains("paused"),
     "The timeline play button is in its playing state by default");

  info("Mousedown in the header to move the scrubber");
  yield synthesizeInHeaderAndWaitForChange(timeline, 50, 1, "mousedown");
  let newPos = parseInt(scrubberEl.style.left, 10);
  is(newPos, 50, "The scrubber moved on mousedown");

  ok(playTimelineButtonEl.classList.contains("paused"),
     "The timeline play button is in its paused state after mousedown");

  info("Continue moving the mouse and verify that the scrubber tracks it");
  yield synthesizeInHeaderAndWaitForChange(timeline, 100, 1, "mousemove");
  newPos = parseInt(scrubberEl.style.left, 10);
  is(newPos, 100, "The scrubber followed the mouse");

  ok(playTimelineButtonEl.classList.contains("paused"),
     "The timeline play button is in its paused state after mousemove");

  info("Release the mouse and move again and verify that the scrubber stays");
  EventUtils.synthesizeMouse(timeHeaderEl, 100, 1, {type: "mouseup"}, win);
  EventUtils.synthesizeMouse(timeHeaderEl, 200, 1, {type: "mousemove"}, win);
  newPos = parseInt(scrubberEl.style.left, 10);
  is(newPos, 100, "The scrubber stopped following the mouse");

  info("Try to drag the scrubber handle and check that the scrubber moves");
  let onDataChanged = timeline.once("timeline-data-changed");
  EventUtils.synthesizeMouse(scrubberHandleEl, 1, 20, {type: "mousedown"}, win);
  EventUtils.synthesizeMouse(timeHeaderEl, 0, 0, {type: "mousemove"}, win);
  EventUtils.synthesizeMouse(timeHeaderEl, 0, 0, {type: "mouseup"}, win);
  yield onDataChanged;

  newPos = parseInt(scrubberEl.style.left, 10);
  is(newPos, 0, "The scrubber stopped following the mouse");
});

function* synthesizeInHeaderAndWaitForChange(timeline, x, y, type) {
  let onDataChanged = timeline.once("timeline-data-changed");
  EventUtils.synthesizeMouse(timeline.timeHeaderEl, x, y, {type}, timeline.win);
  yield onDataChanged;
}
