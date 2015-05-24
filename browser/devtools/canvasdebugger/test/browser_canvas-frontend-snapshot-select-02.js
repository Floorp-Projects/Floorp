/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if selecting snapshots in the frontend displays the appropriate data
 * respective to their recorded animation frame.
 */

function* ifTestingSupported() {
  let { target, panel } = yield initCanvasDebuggerFrontend(SIMPLE_CANVAS_URL);
  let { window, $, EVENTS, SnapshotsListView, CallsListView } = panel.panelWin;

  yield reload(target);

  SnapshotsListView._onRecordButtonClick();
  let snapshotTarget = SnapshotsListView.getItemAtIndex(0).target;

  EventUtils.sendMouseEvent({ type: "mousedown" }, snapshotTarget, window);
  EventUtils.sendMouseEvent({ type: "mousedown" }, snapshotTarget, window);
  EventUtils.sendMouseEvent({ type: "mousedown" }, snapshotTarget, window);

  ok(true, "clicking in-progress snapshot does not fail");

  let finished = once(window, EVENTS.SNAPSHOT_RECORDING_FINISHED);
  SnapshotsListView._onRecordButtonClick();
  yield finished;

  yield teardown(panel);
  finish();
}
