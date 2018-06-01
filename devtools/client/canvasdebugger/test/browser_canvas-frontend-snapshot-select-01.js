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

  await recordAndWaitForFirstSnapshot();
  info("First snapshot recorded.");

  is(SnapshotsListView.selectedIndex, 0,
    "A snapshot should be automatically selected after first recording.");
  is(CallsListView.selectedIndex, -1,
    "There should be no call item automatically selected in the snapshot.");

  await recordAndWaitForAnotherSnapshot();
  info("Second snapshot recorded.");

  is(SnapshotsListView.selectedIndex, 0,
    "A snapshot should not be automatically selected after another recording.");
  is(CallsListView.selectedIndex, -1,
    "There should still be no call item automatically selected in the snapshot.");

  const secondSnapshotTarget = SnapshotsListView.getItemAtIndex(1).target;
  let snapshotSelected = waitForSnapshotSelection();
  EventUtils.sendMouseEvent({ type: "mousedown" }, secondSnapshotTarget, window);

  await snapshotSelected;
  info("Second snapshot selected.");

  is(SnapshotsListView.selectedIndex, 1,
    "The second snapshot should now be selected.");
  is(CallsListView.selectedIndex, -1,
    "There should still be no call item automatically selected in the snapshot.");

  const firstDrawCallContents = $(".call-item-contents", CallsListView.getItemAtIndex(2).target);
  const screenshotDisplayed = once(window, EVENTS.CALL_SCREENSHOT_DISPLAYED);
  EventUtils.sendMouseEvent({ type: "mousedown" }, firstDrawCallContents, window);

  await screenshotDisplayed;
  info("First draw call in the second snapshot selected.");

  is(SnapshotsListView.selectedIndex, 1,
    "The second snapshot should still be selected.");
  is(CallsListView.selectedIndex, 2,
    "The first draw call should now be selected in the snapshot.");

  const firstSnapshotTarget = SnapshotsListView.getItemAtIndex(0).target;
  snapshotSelected = waitForSnapshotSelection();
  EventUtils.sendMouseEvent({ type: "mousedown" }, firstSnapshotTarget, window);

  await snapshotSelected;
  info("First snapshot re-selected.");

  is(SnapshotsListView.selectedIndex, 0,
    "The first snapshot should now be re-selected.");
  is(CallsListView.selectedIndex, -1,
    "There should still be no call item automatically selected in the snapshot.");

  function recordAndWaitForFirstSnapshot() {
    const recordingFinished = once(window, EVENTS.SNAPSHOT_RECORDING_FINISHED);
    const snapshotSelected = waitForSnapshotSelection();
    SnapshotsListView._onRecordButtonClick();
    return Promise.all([recordingFinished, snapshotSelected]);
  }

  function recordAndWaitForAnotherSnapshot() {
    const recordingFinished = once(window, EVENTS.SNAPSHOT_RECORDING_FINISHED);
    SnapshotsListView._onRecordButtonClick();
    return recordingFinished;
  }

  function waitForSnapshotSelection() {
    const callListPopulated = once(window, EVENTS.CALL_LIST_POPULATED);
    const thumbnailsDisplayed = once(window, EVENTS.THUMBNAILS_DISPLAYED);
    const screenshotDisplayed = once(window, EVENTS.CALL_SCREENSHOT_DISPLAYED);
    return Promise.all([
      callListPopulated,
      thumbnailsDisplayed,
      screenshotDisplayed
    ]);
  }

  await teardown(panel);
  finish();
}
