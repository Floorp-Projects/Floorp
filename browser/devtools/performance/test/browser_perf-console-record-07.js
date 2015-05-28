/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that a call to console.profileEnd() with no label ends the
 * most recent console recording, and console.profileEnd() with a label that does not
 * match any pending recordings does nothing.
 */

function* spawnTest() {
  loadFrameScripts();
  let { target, toolbox, panel } = yield initPerformance(SIMPLE_URL);
  let { $, EVENTS, gFront, PerformanceController, OverviewView, RecordingsView, WaterfallView } = panel.panelWin;

  yield consoleProfile(panel.panelWin);
  yield consoleProfile(panel.panelWin, "1");
  yield consoleProfile(panel.panelWin, "2");

  let recordings = PerformanceController.getRecordings();
  is(recordings.length, 3, "3 recordings found");
  is(RecordingsView.selectedItem.attachment, recordings[0],
    "The first console recording should be selected.");

  yield consoleProfileEnd(panel.panelWin);

  // First off a label-less profileEnd to make sure no other recordings close
  consoleProfileEnd(panel.panelWin, "fxos");
  yield idleWait(500);

  recordings = PerformanceController.getRecordings();
  is(recordings.length, 3, "3 recordings found");

  is(recordings[0].getLabel(), "", "Checking label of recording 1");
  is(recordings[1].getLabel(), "1", "Checking label of recording 2");
  is(recordings[2].getLabel(), "2", "Checking label of recording 3");
  is(recordings[0].isRecording(), true,
    "The not most recent recording should not stop when calling console.profileEnd with no args.");
  is(recordings[1].isRecording(), true,
    "The not most recent recording should not stop when calling console.profileEnd with no args.");
  is(recordings[2].isRecording(), false,
    "Only thw most recent recording should stop when calling console.profileEnd with no args.");

  let detailsRendered = once(WaterfallView, EVENTS.WATERFALL_RENDERED);
  yield consoleProfileEnd(panel.panelWin);
  yield consoleProfileEnd(panel.panelWin);

  is(recordings[0].isRecording(), false,
    "All recordings should now be ended. (1)");
  is(recordings[1].isRecording(), false,
    "All recordings should now be ended. (2)");
  is(recordings[2].isRecording(), false,
    "All recordings should now be ended. (3)");

  yield detailsRendered;

  consoleProfileEnd(panel.panelWin);
  yield idleWait(500);
  ok(true, "Calling additional console.profileEnd() with no argument and no pending recordings does not throw.");

  yield teardown(panel);
  finish();
}
