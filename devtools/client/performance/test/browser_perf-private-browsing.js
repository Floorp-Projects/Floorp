/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that disables the frontend when in private browsing mode.
 */

let gPanelWinTuples = [];

function* spawnTest() {
  yield testNormalWindow();
  yield testPrivateWindow();
  yield testRecordingFailingInWindow(0);
  yield testRecordingFailingInWindow(1);
  yield teardownPerfInWindow(1);
  yield testRecordingSucceedingInWindow(0);
  yield teardownPerfInWindow(0);

  gPanelWinTuples = null;
  finish();
}

function* createPanelInWindow(options) {
  let win = yield addWindow(options);
  let tab = yield addTab(SIMPLE_URL, win);
  let target = TargetFactory.forTab(tab);
  yield target.makeRemote();

  let toolbox = yield gDevTools.showToolbox(target, "performance");
  yield toolbox.initPerformance();

  let panel = yield toolbox.getCurrentPanel().open();
  gPanelWinTuples.push({ panel, win });

  return { panel, win };
}

function* testNormalWindow() {
  let { panel } = yield createPanelInWindow({ private: false });
  let { PerformanceView } = panel.panelWin;

  is(PerformanceView.getState(), "empty",
    "The initial state of the performance panel view is correct (1).");
}

function* testPrivateWindow() {
  let { panel } = yield createPanelInWindow({ private: true });
  let { PerformanceView } = panel.panelWin;

  is(PerformanceView.getState(), "unavailable",
    "The initial state of the performance panel view is correct (2).");
}

function* testRecordingFailingInWindow(index) {
  let { panel } = gPanelWinTuples[index];
  let { EVENTS, PerformanceController } = panel.panelWin;

  let onRecordingStarted = () => {
    ok(false, "Recording should not start while a private window is present.");
  };

  PerformanceController.on(EVENTS.RECORDING_STARTED, onRecordingStarted);

  let whenFailed = once(PerformanceController, EVENTS.NEW_RECORDING_FAILED);
  PerformanceController.startRecording();
  yield whenFailed;
  ok(true, "Recording has failed.");

  PerformanceController.off(EVENTS.RECORDING_STARTED, onRecordingStarted);
}

function* testRecordingSucceedingInWindow(index) {
  let { panel } = gPanelWinTuples[index];
  let { EVENTS, PerformanceController } = panel.panelWin;

  let onRecordingFailed = () => {
    ok(false, "Recording should start while now private windows are present.");
  };

  PerformanceController.on(EVENTS.NEW_RECORDING_FAILED, onRecordingFailed);

  yield startRecording(panel);
  yield stopRecording(panel);
  ok(true, "Recording has succeeded.");

  PerformanceController.off(EVENTS.RECORDING_STARTED, onRecordingFailed);
}

function* teardownPerfInWindow(index) {
  let { panel, win } = gPanelWinTuples[index];
  yield teardown(panel, win);
  win.close();
}
