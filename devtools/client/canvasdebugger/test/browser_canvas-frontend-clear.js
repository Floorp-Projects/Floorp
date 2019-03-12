/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if clearing the snapshots list works as expected.
 */

async function ifTestingSupported() {
  const { target, panel } = await initCanvasDebuggerFrontend(SIMPLE_CANVAS_URL);
  const { window, EVENTS, SnapshotsListView } = panel.panelWin;

  await reload(target);

  const firstRecordingFinished = once(window, EVENTS.SNAPSHOT_RECORDING_FINISHED);
  SnapshotsListView._onRecordButtonClick();

  await firstRecordingFinished;
  ok(true, "Finished recording a snapshot of the animation loop.");

  is(SnapshotsListView.itemCount, 1,
    "There should be one item available in the snapshots list.");

  const secondRecordingFinished = once(window, EVENTS.SNAPSHOT_RECORDING_FINISHED);
  SnapshotsListView._onRecordButtonClick();

  await secondRecordingFinished;
  ok(true, "Finished recording another snapshot of the animation loop.");

  is(SnapshotsListView.itemCount, 2,
    "There should be two items available in the snapshots list.");

  const clearingFinished = once(window, EVENTS.SNAPSHOTS_LIST_CLEARED);
  SnapshotsListView._onClearButtonClick();

  await clearingFinished;
  ok(true, "Finished recording all snapshots.");

  is(SnapshotsListView.itemCount, 0,
    "There should be no items available in the snapshots list.");

  await teardown(panel);
  finish();
}
