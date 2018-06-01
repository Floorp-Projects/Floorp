/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests whether the frontend behaves correctly while reording a snapshot.
 */

async function ifTestingSupported() {
  const { target, panel } = await initCanvasDebuggerFrontend(SIMPLE_CANVAS_URL);
  const { window, EVENTS, $, SnapshotsListView } = panel.panelWin;

  await reload(target);

  is($("#record-snapshot").hasAttribute("checked"), false,
    "The 'record snapshot' button should initially be unchecked.");
  is($("#record-snapshot").hasAttribute("disabled"), false,
    "The 'record snapshot' button should initially be enabled.");
  is($("#record-snapshot").hasAttribute("hidden"), false,
    "The 'record snapshot' button should now be visible.");

  is(SnapshotsListView.itemCount, 0,
    "There should be no items available in the snapshots list view.");
  is(SnapshotsListView.selectedIndex, -1,
    "There should be no selected item in the snapshots list view.");

  const recordingStarted = once(window, EVENTS.SNAPSHOT_RECORDING_STARTED);
  const recordingFinished = once(window, EVENTS.SNAPSHOT_RECORDING_FINISHED);
  SnapshotsListView._onRecordButtonClick();

  await recordingStarted;
  ok(true, "Started recording a snapshot of the animation loop.");

  is($("#record-snapshot").getAttribute("checked"), "true",
    "The 'record snapshot' button should now be checked.");
  is($("#record-snapshot").hasAttribute("hidden"), false,
    "The 'record snapshot' button should still be visible.");

  is(SnapshotsListView.itemCount, 1,
    "There should be one item available in the snapshots list view now.");
  is(SnapshotsListView.selectedIndex, -1,
    "There should be no selected item in the snapshots list view yet.");

  await recordingFinished;
  ok(true, "Finished recording a snapshot of the animation loop.");

  is($("#record-snapshot").hasAttribute("checked"), false,
    "The 'record snapshot' button should now be unchecked.");
  is($("#record-snapshot").hasAttribute("disabled"), false,
    "The 'record snapshot' button should now be re-enabled.");
  is($("#record-snapshot").hasAttribute("hidden"), false,
    "The 'record snapshot' button should still be visible.");

  is(SnapshotsListView.itemCount, 1,
    "There should still be only one item available in the snapshots list view.");
  is(SnapshotsListView.selectedIndex, 0,
    "There should be one selected item in the snapshots list view now.");

  await teardown(panel);
  finish();
}
