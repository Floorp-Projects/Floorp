/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if selecting snapshots in the frontend displays the appropriate data
 * respective to their recorded animation frame.
 */

async function ifTestingSupported() {
  const { target, panel } = await initCanvasDebuggerFrontend(SIMPLE_CANVAS_URL);
  const { window, $, EVENTS, SnapshotsListView, CallsListView } = panel.panelWin;

  await reload(target);

  SnapshotsListView._onRecordButtonClick();
  const snapshotTarget = SnapshotsListView.getItemAtIndex(0).target;

  EventUtils.sendMouseEvent({ type: "mousedown" }, snapshotTarget, window);
  EventUtils.sendMouseEvent({ type: "mousedown" }, snapshotTarget, window);
  EventUtils.sendMouseEvent({ type: "mousedown" }, snapshotTarget, window);

  ok(true, "clicking in-progress snapshot does not fail");

  const finished = once(window, EVENTS.SNAPSHOT_RECORDING_FINISHED);
  SnapshotsListView._onRecordButtonClick();
  await finished;

  await teardown(panel);
  finish();
}
