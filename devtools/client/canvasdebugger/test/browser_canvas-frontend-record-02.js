/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests whether the frontend displays a placeholder snapshot while recording.
 */

async function ifTestingSupported() {
  const { target, panel } = await initCanvasDebuggerFrontend(SIMPLE_CANVAS_URL);
  const { window, EVENTS, L10N, $, SnapshotsListView } = panel.panelWin;

  await reload(target);

  const recordingStarted = once(window, EVENTS.SNAPSHOT_RECORDING_STARTED);
  const recordingFinished = once(window, EVENTS.SNAPSHOT_RECORDING_FINISHED);
  const recordingSelected = once(window, EVENTS.SNAPSHOT_RECORDING_SELECTED);
  SnapshotsListView._onRecordButtonClick();

  await recordingStarted;
  ok(true, "Started recording a snapshot of the animation loop.");

  const item = SnapshotsListView.getItemAtIndex(0);

  is($(".snapshot-item-title", item.target).getAttribute("value"),
    L10N.getFormatStr("snapshotsList.itemLabel", 1),
    "The placeholder item's title label is correct.");

  is($(".snapshot-item-calls", item.target).getAttribute("value"),
    L10N.getStr("snapshotsList.loadingLabel"),
    "The placeholder item's calls label is correct.");

  is($(".snapshot-item-save", item.target).getAttribute("value"), "",
    "The placeholder item's save label should not have a value yet.");

  is($("#reload-notice").getAttribute("hidden"), "true",
    "The reload notice should now be hidden.");
  is($("#empty-notice").getAttribute("hidden"), "true",
    "The empty notice should now be hidden.");
  is($("#waiting-notice").hasAttribute("hidden"), false,
    "The waiting notice should now be visible.");

  is($("#screenshot-container").getAttribute("hidden"), "true",
    "The screenshot container should still be hidden.");
  is($("#snapshot-filmstrip").getAttribute("hidden"), "true",
    "The snapshot filmstrip should still be hidden.");

  is($("#debugging-pane-contents").getAttribute("hidden"), "true",
    "The rest of the UI should still be hidden.");

  await recordingFinished;
  ok(true, "Finished recording a snapshot of the animation loop.");

  await recordingSelected;
  ok(true, "Finished selecting a snapshot of the animation loop.");

  is($("#reload-notice").getAttribute("hidden"), "true",
    "The reload notice should now be hidden.");
  is($("#empty-notice").getAttribute("hidden"), "true",
    "The empty notice should now be hidden.");
  is($("#waiting-notice").getAttribute("hidden"), "true",
    "The waiting notice should now be hidden.");

  is($("#screenshot-container").hasAttribute("hidden"), false,
    "The screenshot container should now be visible.");
  is($("#snapshot-filmstrip").hasAttribute("hidden"), false,
    "The snapshot filmstrip should now be visible.");

  is($("#debugging-pane-contents").hasAttribute("hidden"), false,
    "The rest of the UI should now be visible.");

  await teardown(panel);
  finish();
}
