/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if clearing the snapshots list works as expected.
 */

function* ifTestingSupported() {
  let { target, panel } = yield initCanvasDebuggerFrontend(SIMPLE_CANVAS_URL);
  let { window, EVENTS, SnapshotsListView } = panel.panelWin;

  yield reload(target);

  let firstRecordingFinished = once(window, EVENTS.SNAPSHOT_RECORDING_FINISHED);
  SnapshotsListView._onRecordButtonClick();

  yield firstRecordingFinished;
  ok(true, "Finished recording a snapshot of the animation loop.");

  is(SnapshotsListView.itemCount, 1,
    "There should be one item available in the snapshots list.");

  let secondRecordingFinished = once(window, EVENTS.SNAPSHOT_RECORDING_FINISHED);
  SnapshotsListView._onRecordButtonClick();

  yield secondRecordingFinished;
  ok(true, "Finished recording another snapshot of the animation loop.");

  is(SnapshotsListView.itemCount, 2,
    "There should be two items available in the snapshots list.");

  let clearingFinished = once(window, EVENTS.SNAPSHOTS_LIST_CLEARED);
  SnapshotsListView._onClearButtonClick();

  yield clearingFinished;
  ok(true, "Finished recording all snapshots.");

  is(SnapshotsListView.itemCount, 0,
    "There should be no items available in the snapshots list.");

  yield teardown(panel);
  finish();
}
