/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests whether the frontend displays the correct info for a snapshot
 * after finishing recording.
 */

function* ifTestingSupported() {
  let { target, panel } = yield initCanvasDebuggerFrontend(SIMPLE_CANVAS_URL);
  let { window, EVENTS, $, SnapshotsListView } = panel.panelWin;

  yield reload(target);

  let recordingFinished = once(window, EVENTS.SNAPSHOT_RECORDING_FINISHED);
  SnapshotsListView._onRecordButtonClick();

  yield recordingFinished;
  ok(true, "Finished recording a snapshot of the animation loop.");

  let item = SnapshotsListView.getItemAtIndex(0);

  is(SnapshotsListView.selectedItem, item,
    "The first item should now be selected in the snapshots list view (1).");
  is(SnapshotsListView.selectedIndex, 0,
    "The first item should now be selected in the snapshots list view (2).");

  is($(".snapshot-item-calls", item.target).getAttribute("value"), "4 draws, 8 calls",
    "The placeholder item's calls label is correct.");
  is($(".snapshot-item-save", item.target).getAttribute("value"), "Save",
    "The placeholder item's save label is correct.");
  is($(".snapshot-item-save", item.target).getAttribute("disabled"), "false",
    "The placeholder item's save label should be clickable.");

  yield teardown(panel);
  finish();
}
