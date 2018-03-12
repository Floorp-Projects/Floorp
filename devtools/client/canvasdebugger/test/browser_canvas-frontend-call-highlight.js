/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if certain function calls are properly highlighted in the UI.
 */

function* ifTestingSupported() {
  let { target, panel } = yield initCanvasDebuggerFrontend(SIMPLE_CANVAS_URL);
  let { window, $, EVENTS, SnapshotsListView, CallsListView } = panel.panelWin;

  yield reload(target);

  let recordingFinished = once(window, EVENTS.SNAPSHOT_RECORDING_FINISHED);
  let callListPopulated = once(window, EVENTS.CALL_LIST_POPULATED);
  SnapshotsListView._onRecordButtonClick();
  yield promise.all([recordingFinished, callListPopulated]);

  is(CallsListView.itemCount, 8,
    "All the function calls should now be displayed in the UI.");

  is($(".call-item-view", CallsListView.getItemAtIndex(0).target).hasAttribute("draw-call"), true,
    "The first item's node should have a draw-call attribute.");
  is($(".call-item-view", CallsListView.getItemAtIndex(1).target).hasAttribute("draw-call"), false,
    "The second item's node should not have a draw-call attribute.");
  is($(".call-item-view", CallsListView.getItemAtIndex(2).target).hasAttribute("draw-call"), true,
    "The third item's node should have a draw-call attribute.");
  is($(".call-item-view", CallsListView.getItemAtIndex(3).target).hasAttribute("draw-call"), false,
    "The fourth item's node should not have a draw-call attribute.");
  is($(".call-item-view", CallsListView.getItemAtIndex(4).target).hasAttribute("draw-call"), true,
    "The fifth item's node should have a draw-call attribute.");
  is($(".call-item-view", CallsListView.getItemAtIndex(5).target).hasAttribute("draw-call"), false,
    "The sixth item's node should not have a draw-call attribute.");
  is($(".call-item-view", CallsListView.getItemAtIndex(6).target).hasAttribute("draw-call"), true,
    "The seventh item's node should have a draw-call attribute.");
  is($(".call-item-view", CallsListView.getItemAtIndex(7).target).hasAttribute("draw-call"), false,
    "The eigth item's node should not have a draw-call attribute.");

  yield teardown(panel);
  finish();
}
