/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if selecting snapshots in the frontend displays the appropriate data
 * respective to their recorded animation frame.
 */

function ifTestingSupported() {
  let [target, debuggee, panel] = yield initCanavsDebuggerFrontend(SIMPLE_CANVAS_URL);
  let { window, $, EVENTS, SnapshotsListView, CallsListView } = panel.panelWin;

  yield reload(target);

  yield recordAndWaitForFirstSnapshot();
  info("First snapshot recorded.")

  is(SnapshotsListView.selectedIndex, 0,
    "A snapshot should be automatically selected after first recording.");
  is(CallsListView.selectedIndex, -1,
    "There should be no call item automatically selected in the snapshot.");

  yield recordAndWaitForAnotherSnapshot();
  info("Second snapshot recorded.")

  is(SnapshotsListView.selectedIndex, 0,
    "A snapshot should not be automatically selected after another recording.");
  is(CallsListView.selectedIndex, -1,
    "There should still be no call item automatically selected in the snapshot.");

  let secondSnapshotTarget = SnapshotsListView.getItemAtIndex(1).target;
  let snapshotSelected = waitForSnapshotSelection();
  EventUtils.sendMouseEvent({ type: "mousedown" }, secondSnapshotTarget, window);

  yield snapshotSelected;
  info("Second snapshot selected.");

  is(SnapshotsListView.selectedIndex, 1,
    "The second snapshot should now be selected.");
  is(CallsListView.selectedIndex, -1,
    "There should still be no call item automatically selected in the snapshot.");

  let firstDrawCallContents = $(".call-item-contents", CallsListView.getItemAtIndex(2).target);
  let screenshotDisplayed = once(window, EVENTS.CALL_SCREENSHOT_DISPLAYED);
  EventUtils.sendMouseEvent({ type: "mousedown" }, firstDrawCallContents, window);

  yield screenshotDisplayed;
  info("First draw call in the second snapshot selected.");

  is(SnapshotsListView.selectedIndex, 1,
    "The second snapshot should still be selected.");
  is(CallsListView.selectedIndex, 2,
    "The first draw call should now be selected in the snapshot.");

  let firstSnapshotTarget = SnapshotsListView.getItemAtIndex(0).target;
  let snapshotSelected = waitForSnapshotSelection();
  EventUtils.sendMouseEvent({ type: "mousedown" }, firstSnapshotTarget, window);

  yield snapshotSelected;
  info("First snapshot re-selected.");

  is(SnapshotsListView.selectedIndex, 0,
    "The first snapshot should now be re-selected.");
  is(CallsListView.selectedIndex, -1,
    "There should still be no call item automatically selected in the snapshot.");

  function recordAndWaitForFirstSnapshot() {
    let recordingFinished = once(window, EVENTS.SNAPSHOT_RECORDING_FINISHED);
    let snapshotSelected = waitForSnapshotSelection();
    SnapshotsListView._onRecordButtonClick();
    return promise.all([recordingFinished, snapshotSelected]);
  }

  function recordAndWaitForAnotherSnapshot() {
    let recordingFinished = once(window, EVENTS.SNAPSHOT_RECORDING_FINISHED);
    SnapshotsListView._onRecordButtonClick();
    return recordingFinished;
  }

  function waitForSnapshotSelection() {
    let callListPopulated = once(window, EVENTS.CALL_LIST_POPULATED);
    let thumbnailsDisplayed = once(window, EVENTS.THUMBNAILS_DISPLAYED);
    let screenshotDisplayed = once(window, EVENTS.CALL_SCREENSHOT_DISPLAYED);
    return promise.all([
      callListPopulated,
      thumbnailsDisplayed,
      screenshotDisplayed
    ]);
  }

  yield teardown(panel);
  finish();
}
