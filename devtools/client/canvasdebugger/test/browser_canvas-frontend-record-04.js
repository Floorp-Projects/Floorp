/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Bug 1122766
 * Tests that the canvas actor correctly returns from recordAnimationFrame
 * in the scenario where a loop starts with rAF and has rAF in the beginning
 * of its loop, when the recording starts before the rAFs start.
 */

function* ifTestingSupported() {
  let { target, panel } = yield initCanvasDebuggerFrontend(RAF_BEGIN_URL);
  let { window, EVENTS, gFront, SnapshotsListView } = panel.panelWin;
  loadFrameScripts();

  yield reload(target);

  let recordingFinished = once(window, EVENTS.SNAPSHOT_RECORDING_FINISHED);
  SnapshotsListView._onRecordButtonClick();

  // Wait until after the recording started to trigger the content.
  // Use the gFront method rather than the SNAPSHOT_RECORDING_STARTED event
  // which triggers before the underlying actor call
  yield waitUntil(function* () { return !(yield gFront.isRecording()); });

  // Start animation in content
  evalInDebuggee("start();");

  yield recordingFinished;
  ok(true, "Finished recording a snapshot of the animation loop.");

  yield removeTab(target.tab);
  finish();
}
