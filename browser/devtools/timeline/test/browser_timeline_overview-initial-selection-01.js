/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the overview has an initial selection when recording has finished
 * and there is data available.
 */

let test = Task.async(function*() {
  let { target, panel } = yield initTimelinePanel(SIMPLE_URL);
  let { EVENTS, TimelineView, TimelineController } = panel.panelWin;
  let { OVERVIEW_INITIAL_SELECTION_RATIO: selectionRatio } = panel.panelWin;

  yield TimelineController.toggleRecording();
  ok(true, "Recording has started.");

  let updated = 0;
  panel.panelWin.on(EVENTS.OVERVIEW_UPDATED, () => updated++);

  ok((yield waitUntil(() => updated > 10)),
    "The overview graph was updated a bunch of times.");
  ok((yield waitUntil(() => TimelineController.getMarkers().length > 0)),
    "There are some markers available.");

  yield TimelineController.toggleRecording();
  ok(true, "Recording has ended.");

  let markers = TimelineController.getMarkers();
  let selection = TimelineView.overview.getSelection();

  is((selection.start) | 0,
     ((markers[0].start - markers.startTime) * TimelineView.overview.dataScaleX) | 0,
    "The initial selection start is correct.");

  is((selection.end - selection.start) | 0,
     (selectionRatio * TimelineView.overview.width) | 0,
    "The initial selection end is correct.");

  yield teardown(panel);
  finish();
});
