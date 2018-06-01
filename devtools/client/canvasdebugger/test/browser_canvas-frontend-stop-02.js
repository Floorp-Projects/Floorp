/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that a recording that does not have a rAF cycle fails after timeout.
 */

async function ifTestingSupported() {
  const { target, panel } = await initCanvasDebuggerFrontend(NO_CANVAS_URL);
  const { window, EVENTS, $, SnapshotsListView } = panel.panelWin;

  await reload(target);

  const recordingStarted = once(window, EVENTS.SNAPSHOT_RECORDING_STARTED);
  SnapshotsListView._onRecordButtonClick();

  await recordingStarted;

  is($("#empty-notice").hidden, true, "Empty notice not shown");
  is($("#waiting-notice").hidden, false, "Waiting notice shown");

  const recordingFinished = once(window, EVENTS.SNAPSHOT_RECORDING_FINISHED);
  const recordingCancelled = once(window, EVENTS.SNAPSHOT_RECORDING_CANCELLED);

  await Promise.all([recordingFinished, recordingCancelled]);

  ok(true, "Recording stopped and was considered failed.");

  is(SnapshotsListView.itemCount, 0, "No snapshots in the list.");
  is($("#empty-notice").hidden, false, "Empty notice shown");
  is($("#waiting-notice").hidden, true, "Waiting notice not shown");

  await teardown(panel);
  finish();
}
