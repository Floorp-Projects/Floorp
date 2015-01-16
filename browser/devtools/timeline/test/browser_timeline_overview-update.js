/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the markers and memory overviews are continuously updated.
 */

add_task(function*() {
  let { target, panel } = yield initTimelinePanel("about:blank");
  let { $, EVENTS, TimelineView, TimelineController } = panel.panelWin;

  $("#memory-checkbox").checked = true;
  yield TimelineController.updateMemoryRecording();

  yield TimelineController.toggleRecording();
  ok(true, "Recording has started.");

  ok("selectionEnabled" in TimelineView.markersOverview,
    "The selection should not be enabled for the markers overview (1).");
  is(TimelineView.markersOverview.selectionEnabled, false,
    "The selection should not be enabled for the markers overview (2).");
  is(TimelineView.markersOverview.hasSelection(), false,
    "The markers overview shouldn't have a selection before recording.");

  ok("selectionEnabled" in TimelineView.memoryOverview,
    "The selection should not be enabled for the memory overview (1).");
  is(TimelineView.memoryOverview.selectionEnabled, false,
    "The selection should not be enabled for the memory overview (2).");
  is(TimelineView.memoryOverview.hasSelection(), false,
    "The memory overview shouldn't have a selection before recording.");

  let updated = 0;
  panel.panelWin.on(EVENTS.OVERVIEW_UPDATED, () => updated++);

  ok((yield waitUntil(() => updated > 10)),
    "The overviews were updated a bunch of times.");
  ok((yield waitUntil(() => TimelineController.getMemory().length > 10)),
    "There are some memory measurements available now.");

  ok("selectionEnabled" in TimelineView.markersOverview,
    "The selection should still not be enabled for the markers overview (3).");
  is(TimelineView.markersOverview.selectionEnabled, false,
    "The selection should still not be enabled for the markers overview (4).");
  is(TimelineView.markersOverview.hasSelection(), false,
    "The markers overview should not have a selection while recording.");

  ok("selectionEnabled" in TimelineView.memoryOverview,
    "The selection should still not be enabled for the memory overview (3).");
  is(TimelineView.memoryOverview.selectionEnabled, false,
    "The selection should still not be enabled for the memory overview (4).");
  is(TimelineView.memoryOverview.hasSelection(), false,
    "The memory overview should not have a selection while recording.");

  yield TimelineController.toggleRecording();
  ok(true, "Recording has ended.");

  // TODO: Re-enable this assertion as part of bug 1120830
  // is(TimelineController.getMarkers().length, 0,
  //  "There are no markers available.");
  isnot(TimelineController.getMemory().length, 0,
    "There are some memory measurements available.");

  is(TimelineView.markersOverview.selectionEnabled, true,
    "The selection should now be enabled for the markers overview.");
  // TODO: Re-enable this assertion as part of bug 1120830
  // is(TimelineView.markersOverview.hasSelection(), false,
  //  "The markers overview should not have a selection after recording.");

  is(TimelineView.memoryOverview.selectionEnabled, true,
    "The selection should now be enabled for the memory overview.");
  // TODO: Re-enable this assertion as part of bug 1120830
  // is(TimelineView.memoryOverview.hasSelection(), false,
  //  "The memory overview should not have a selection after recording.");
});
