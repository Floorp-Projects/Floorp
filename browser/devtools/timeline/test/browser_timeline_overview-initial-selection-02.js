/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the overview has no initial selection when recording has finished
 * and there is no data available.
 */

let test = Task.async(function*() {
  let { target, panel } = yield initTimelinePanel(SIMPLE_URL);
  let { $, EVENTS, TimelineView, TimelineController } = panel.panelWin;
  let { OVERVIEW_INITIAL_SELECTION_RATIO: selectionRatio } = panel.panelWin;

  $("#memory-checkbox").checked = true;
  yield TimelineController.updateMemoryRecording();

  yield TimelineController.toggleRecording();
  ok(true, "Recording has started.");

  yield TimelineController._stopRecordingAndDiscardData();
  ok(true, "Recording has ended.");

  let markers = TimelineController.getMarkers();
  let memory = TimelineController.getMemory();
  let selection = TimelineView.markersOverview.getSelection();

  is(markers.length, 0,
    "There are no markers available.");
  is(memory.length, 0,
    "There are no memory measurements available.");
  is(selection.start, null,
    "The initial selection start is correct.");
  is(selection.end, null,
    "The initial selection end is correct.");

  yield teardown(panel);
  finish();
});
