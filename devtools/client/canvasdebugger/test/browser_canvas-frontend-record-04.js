/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Bug 1122766
 * Tests that the canvas actor correctly returns from recordAnimationFrame
 * in the scenario where a loop starts with rAF and has rAF in the beginning
 * of its loop, when the recording starts before the rAFs start.
 */

async function ifTestingSupported() {
  const { target, panel } = await initCanvasDebuggerFrontend(RAF_BEGIN_URL);
  const { window, EVENTS, gFront, SnapshotsListView } = panel.panelWin;
  loadFrameScriptUtils();

  await reload(target);

  const recordingFinished = once(window, EVENTS.SNAPSHOT_RECORDING_FINISHED);
  SnapshotsListView._onRecordButtonClick();

  // Wait until after the recording started to trigger the content.
  // Use the gFront method rather than the SNAPSHOT_RECORDING_STARTED event
  // which triggers before the underlying actor call
  await waitUntil(async function() {
    return !(await gFront.isRecording());
  });

  // Start animation in content
  evalInDebuggee("start();");

  await recordingFinished;
  ok(true, "Finished recording a snapshot of the animation loop.");

  await removeTab(target.tab);
  finish();
}
