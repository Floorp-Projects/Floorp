/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the overview has an initial selection when recording has finished
 * and there is data available.
 */

add_task(function*() {
  let { target, panel } = yield initTimelinePanel(SIMPLE_URL);
  let { $, EVENTS, TimelineView, TimelineController } = panel.panelWin;
  let { OVERVIEW_INITIAL_SELECTION_RATIO: selectionRatio } = panel.panelWin;

  $("#memory-checkbox").checked = true;
  yield TimelineController.updateMemoryRecording();

  yield TimelineController.toggleRecording();
  ok(true, "Recording has started.");

  let updated = 0;
  panel.panelWin.on(EVENTS.OVERVIEW_UPDATED, () => updated++);

  ok((yield waitUntil(() => updated > 10)),
    "The overview graph was updated a bunch of times.");
  ok((yield waitUntil(() => TimelineController.getMarkers().length > 0)),
    "There are some markers available.");
  ok((yield waitUntil(() => TimelineController.getMemory().length > 0)),
    "There are some memory measurements available now.");

  yield TimelineController.toggleRecording();
  ok(true, "Recording has ended.");

  let interval = TimelineController.getInterval();
  let markers = TimelineController.getMarkers();
  let selection = TimelineView.markersOverview.getSelection();

  is((selection.start) | 0,
     (markers[0].start * TimelineView.markersOverview.dataScaleX) | 0,
    "The initial selection start is correct.");

  is((selection.end - selection.start) | 0,
     (selectionRatio * TimelineView.markersOverview.width) | 0,
    "The initial selection end is correct.");
});
