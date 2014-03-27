/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if filtering the items in the call list works properly.
 */

function ifTestingSupported() {
  let [target, debuggee, panel] = yield initCanavsDebuggerFrontend(SIMPLE_CANVAS_URL);
  let { window, $, EVENTS, SnapshotsListView, CallsListView } = panel.panelWin;
  let searchbox = $("#calls-searchbox");

  yield reload(target);

  let firstRecordingFinished = once(window, EVENTS.SNAPSHOT_RECORDING_FINISHED);
  let callListPopulated = once(window, EVENTS.CALL_LIST_POPULATED);
  SnapshotsListView._onRecordButtonClick();
  yield promise.all([firstRecordingFinished, callListPopulated]);

  is(searchbox.value, "",
    "The searchbox should be initially empty.");
  is(CallsListView.visibleItems.length, 8,
    "All the items should be initially visible in the calls list.");

  searchbox.focus();
  EventUtils.sendString("clear", window);

  is(searchbox.value, "clear",
    "The searchbox should now contain the 'clear' string.");
  is(CallsListView.visibleItems.length, 1,
    "Only one item should now be visible in the calls list.");

  is(CallsListView.visibleItems[0].attachment.actor.type, CallWatcherFront.METHOD_FUNCTION,
    "The visible item's type has the expected value.");
  is(CallsListView.visibleItems[0].attachment.actor.name, "clearRect",
    "The visible item's name has the expected value.");
  is(CallsListView.visibleItems[0].attachment.actor.file, SIMPLE_CANVAS_URL,
    "The visible item's file has the expected value.");
  is(CallsListView.visibleItems[0].attachment.actor.line, 25,
    "The visible item's line has the expected value.");
  is(CallsListView.visibleItems[0].attachment.actor.argsPreview, "0, 0, 128, 128",
    "The visible item's args have the expected value.");
  is(CallsListView.visibleItems[0].attachment.actor.callerPreview, "ctx",
    "The visible item's caller has the expected value.");

  let secondRecordingFinished = once(window, EVENTS.SNAPSHOT_RECORDING_FINISHED);
  let callListPopulated = once(window, EVENTS.CALL_LIST_POPULATED);

  SnapshotsListView._onRecordButtonClick();
  yield secondRecordingFinished;

  SnapshotsListView.selectedIndex = 1;
  yield callListPopulated;

  is(searchbox.value, "clear",
    "The searchbox should still contain the 'clear' string.");
  is(CallsListView.visibleItems.length, 1,
    "Only one item should still be visible in the calls list.");

  for (let i = 0; i < 5; i++) {
    searchbox.focus();
    EventUtils.sendKey("BACK_SPACE", window);
  }

  is(searchbox.value, "",
    "The searchbox should now be emptied.");
  is(CallsListView.visibleItems.length, 8,
    "All the items should be initially visible again in the calls list.");

  yield teardown(panel);
  finish();
}
