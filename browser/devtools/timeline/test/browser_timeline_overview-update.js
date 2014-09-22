/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the overview graph is continuously updated.
 */

let test = Task.async(function*() {
  let { target, panel } = yield initTimelinePanel("about:blank");
  let { EVENTS, TimelineView, TimelineController } = panel.panelWin;

  yield DevToolsUtils.waitForTime(1000);
  yield TimelineController.toggleRecording();
  ok(true, "Recording has started.");

  ok("selectionEnabled" in TimelineView.overview,
    "The selection should not be enabled for the overview graph (1).");
  is(TimelineView.overview.selectionEnabled, false,
    "The selection should not be enabled for the overview graph (2).");
  is(TimelineView.overview.hasSelection(), false,
    "The overview graph shouldn't have a selection before recording.");

  let updated = 0;
  panel.panelWin.on(EVENTS.OVERVIEW_UPDATED, () => updated++);

  ok((yield waitUntil(() => updated > 10)),
    "The overview graph was updated a bunch of times.");

  ok("selectionEnabled" in TimelineView.overview,
    "The selection should still not be enabled for the overview graph (3).");
  is(TimelineView.overview.selectionEnabled, false,
    "The selection should still not be enabled for the overview graph (4).");
  is(TimelineView.overview.hasSelection(), false,
    "The overview graph should not have a selection while recording.");

  yield TimelineController.toggleRecording();
  ok(true, "Recording has ended.");

  is(TimelineController.getMarkers().length, 0,
    "There are no markers available.");
  is(TimelineView.overview.selectionEnabled, true,
    "The selection should now be enabled for the overview graph.");
  is(TimelineView.overview.hasSelection(), false,
    "The overview graph should not have a selection after recording.");

  yield teardown(panel);
  finish();
});
