/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that console recordings can overlap (not completely nested).
 */

function spawnTest () {
  loadFrameScripts();
  let { target, toolbox, panel } = yield initPerformance(SIMPLE_URL);
  let { $, EVENTS, gFront, PerformanceController, OverviewView, RecordingsView, WaterfallView } = panel.panelWin;

  yield consoleProfile(panel.panelWin, "rust");

  let recordings = PerformanceController.getRecordings();
  is(recordings.length, 1, "a recording found in the performance panel.");
  is(RecordingsView.selectedItem.attachment, recordings[0],
    "The first console recording should be selected.");

  yield consoleProfile(panel.panelWin, "golang");

  recordings = PerformanceController.getRecordings();
  is(recordings.length, 2, "two recordings found in the performance panel.");
  is(RecordingsView.selectedItem.attachment, recordings[0],
    "The first console recording should still be selected.");

  // Ensure overview is still rendering
  yield once(OverviewView, EVENTS.OVERVIEW_RENDERED);
  yield once(OverviewView, EVENTS.OVERVIEW_RENDERED);
  yield once(OverviewView, EVENTS.OVERVIEW_RENDERED);

  yield consoleProfileEnd(panel.panelWin, "rust");

  recordings = PerformanceController.getRecordings();
  is(recordings.length, 2, "two recordings found in the performance panel.");
  is(RecordingsView.selectedItem.attachment, recordings[0],
    "The first console recording should still be selected.");
  is(RecordingsView.selectedItem.attachment.isRecording(), false,
    "The first console recording should no longer be recording.");

  let detailsRendered = once(WaterfallView, EVENTS.WATERFALL_RENDERED);
  yield consoleProfileEnd(panel.panelWin, "golang");
  yield detailsRendered;

  recordings = PerformanceController.getRecordings();
  is(recordings.length, 2, "two recordings found in the performance panel.");
  is(RecordingsView.selectedItem.attachment, recordings[0],
    "The first console recording should still be selected.");
  is(recordings[1].isRecording(), false,
    "The second console recording should no longer be recording.");

  yield teardown(panel);
  finish();
}
