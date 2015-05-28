/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the profiler can handle creation and stopping of console profiles
 * after being opened.
 */

function* spawnTest() {
  loadFrameScripts();
  let { target, toolbox, panel } = yield initPerformance(SIMPLE_URL);
  let { $, EVENTS, gFront, PerformanceController, OverviewView, RecordingsView } = panel.panelWin;

  yield consoleProfile(panel.panelWin, "rust");

  let recordings = PerformanceController.getRecordings();
  is(recordings.length, 1, "a recordings found in the performance panel.");
  is(recordings[0].isConsole(), true, "recording came from console.profile.");
  is(recordings[0].getLabel(), "rust", "correct label in the recording model.");
  is(recordings[0].isRecording(), true, "recording is still recording.");

  is(RecordingsView.selectedItem.attachment, recordings[0],
    "The first console recording should be selected.");

  // Ensure overview is still rendering
  yield once(OverviewView, EVENTS.OVERVIEW_RENDERED);
  yield once(OverviewView, EVENTS.OVERVIEW_RENDERED);
  yield once(OverviewView, EVENTS.OVERVIEW_RENDERED);

  yield consoleProfileEnd(panel.panelWin, "rust");

  yield teardown(panel);
  finish();
}
