/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the profiler can correctly handle simultaneous console and manual
 * recordings (via `console.profile` and clicking the record button).
 */

var C = 1; // is console
var R = 2; // is recording
var S = 4; // is selected

function testRecordings (win, expected) {
  let recordings = win.PerformanceController.getRecordings();
  let current = win.PerformanceController.getCurrentRecording();
  is(recordings.length, expected.length, "expected number of recordings");
  recordings.forEach((recording, i) => {
    ok(recording.isConsole() == !!(expected[i] & C), `recording ${i+1} has expected console state.`);
    ok(recording.isRecording() == !!(expected[i] & R), `recording ${i+1} has expected console state.`);
    ok((recording === current) == !!(expected[i] & S), `recording ${i+1} has expected selected state.`);
  });
}

function* spawnTest() {
  PMM_loadFrameScripts(gBrowser);
  let { target, toolbox, panel } = yield initPerformance(SIMPLE_URL);
  let { EVENTS, PerformanceController, OverviewView, RecordingsView, WaterfallView } = panel.panelWin;

  info("Starting console.profile()...");
  yield consoleProfile(panel.panelWin);
  testRecordings(panel.panelWin, [C+S+R]);
  info("Starting manual recording...");
  yield startRecording(panel);
  testRecordings(panel.panelWin, [C+R, R+S]);
  info("Starting console.profile(\"3\")...");
  yield consoleProfile(panel.panelWin, "3");
  testRecordings(panel.panelWin, [C+R, R+S, C+R]);
  info("Starting console.profile(\"3\")...");
  yield consoleProfile(panel.panelWin, "4");
  testRecordings(panel.panelWin, [C+R, R+S, C+R, C+R]);

  info("Ending console.profileEnd()...");
  yield consoleProfileEnd(panel.panelWin);

  testRecordings(panel.panelWin, [C+R, R+S, C+R, C]);
  ok(OverviewView.isRendering(), "still rendering overview with manual recorded selected.");

  let onSelected = once(WaterfallView, EVENTS.WATERFALL_RENDERED);
  info("Select last recording...");
  RecordingsView.selectedIndex = 3;
  yield onSelected;
  testRecordings(panel.panelWin, [C+R, R, C+R, C+S]);
  ok(!OverviewView.isRendering(), "stop rendering overview when selected completed recording.");

  info("Manually stop manual recording...");
  yield stopRecording(panel);
  testRecordings(panel.panelWin, [C+R, S, C+R, C]);
  ok(!OverviewView.isRendering(), "stop rendering overview when selected completed recording.");

  onSelected = once(PerformanceController, EVENTS.RECORDING_SELECTED);
  info("Select first recording...");
  RecordingsView.selectedIndex = 0;
  yield onSelected;
  testRecordings(panel.panelWin, [C+R+S, 0, C+R, C]);
  yield once(OverviewView, EVENTS.OVERVIEW_RENDERED);
  ok(OverviewView.isRendering(), "should be rendering overview when selected recording in progress.");

  info("Ending console.profileEnd()...");
  yield consoleProfileEnd(panel.panelWin);
  testRecordings(panel.panelWin, [C+R+S, 0, C, C]);
  ok(OverviewView.isRendering(), "should still be rendering overview when selected recording in progress.");
  info("Start one more manual recording...");
  yield startRecording(panel);
  testRecordings(panel.panelWin, [C+R, 0, C, C, R+S]);
  ok(OverviewView.isRendering(), "should be rendering overview when selected recording in progress.");
  info("Stop manual recording...");
  yield stopRecording(panel);
  testRecordings(panel.panelWin, [C+R, 0, C, C, S]);
  ok(!OverviewView.isRendering(), "stop rendering overview when selected completed recording.");

  info("Ending console.profileEnd()...");
  yield consoleProfileEnd(panel.panelWin);
  testRecordings(panel.panelWin, [C, 0, C, C, S]);
  ok(!OverviewView.isRendering(), "stop rendering overview when selected completed recording.");

  yield teardown(panel);
  finish();
}
