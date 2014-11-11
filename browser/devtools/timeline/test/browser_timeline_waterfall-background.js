/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the waterfall background is a 1px high canvas stretching across
 * the container bounds.
 */

let test = Task.async(function*() {
  let { target, panel } = yield initTimelinePanel(SIMPLE_URL);
  let { $, EVENTS, TimelineView, TimelineController } = panel.panelWin;

  yield TimelineController.toggleRecording();
  ok(true, "Recording has started.");

  let updated = 0;
  panel.panelWin.on(EVENTS.OVERVIEW_UPDATED, () => updated++);

  ok((yield waitUntil(() => updated > 0)),
    "The overview graph was updated a bunch of times.");
  ok((yield waitUntil(() => TimelineController.getMarkers().length > 0)),
    "There are some markers available.");

  yield TimelineController.toggleRecording();
  ok(true, "Recording has ended.");

  // Test the waterfall background.

  let parentWidth = $("#timeline-waterfall").getBoundingClientRect().width;
  let waterfallWidth = TimelineView.waterfall._waterfallWidth;
  let sidebarWidth = 150; // px
  is(waterfallWidth, parentWidth - sidebarWidth,
    "The waterfall width is correct.")

  ok(TimelineView.waterfall._canvas,
    "A canvas should be created after the recording ended.");
  ok(TimelineView.waterfall._ctx,
    "A 2d context should be created after the recording ended.");

  is(TimelineView.waterfall._canvas.width, waterfallWidth,
    "The canvas width is correct.");
  is(TimelineView.waterfall._canvas.height, 1,
    "The canvas height is correct.");

  yield teardown(panel);
  finish();
});
