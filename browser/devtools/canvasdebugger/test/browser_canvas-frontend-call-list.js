/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if all the function calls associated with an animation frame snapshot
 * are properly displayed in the UI.
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

  testItem(CallsListView.getItemAtIndex(0),
    "1", "ctx", "clearRect", "(0, 0, 128, 128)", "doc_simple-canvas.html:25");

  testItem(CallsListView.getItemAtIndex(1),
    "2", "ctx", "fillStyle", " = rgb(192, 192, 192)", "doc_simple-canvas.html:20");
  testItem(CallsListView.getItemAtIndex(2),
    "3", "ctx", "fillRect", "(0, 0, 128, 128)", "doc_simple-canvas.html:21");

  testItem(CallsListView.getItemAtIndex(3),
    "4", "ctx", "fillStyle", " = rgba(0, 0, 192, 0.5)", "doc_simple-canvas.html:20");
  testItem(CallsListView.getItemAtIndex(4),
    "5", "ctx", "fillRect", "(30, 30, 55, 50)", "doc_simple-canvas.html:21");

  testItem(CallsListView.getItemAtIndex(5),
    "6", "ctx", "fillStyle", " = rgba(192, 0, 0, 0.5)", "doc_simple-canvas.html:20");
  testItem(CallsListView.getItemAtIndex(6),
    "7", "ctx", "fillRect", "(10, 10, 55, 50)", "doc_simple-canvas.html:21");

  testItem(CallsListView.getItemAtIndex(7),
    "8", "", "requestAnimationFrame", "(Function)", "doc_simple-canvas.html:30");

  function testItem(item, index, context, name, args, location) {
    let i = CallsListView.indexOfItem(item);
    is(i, index - 1,
      "The item at index " + index + " is correctly displayed in the UI.");

    is($(".call-item-index", item.target).getAttribute("value"), index,
      "The item's gutter label has the correct text.");

    if (context) {
      is($(".call-item-context", item.target).getAttribute("value"), context,
        "The item's context label has the correct text.");
    } else {
      is($(".call-item-context", item.target), null,
        "The item's context label should not be available.");
    }

    is($(".call-item-name", item.target).getAttribute("value"), name,
      "The item's name label has the correct text.");
    is($(".call-item-args", item.target).getAttribute("value"), args,
      "The item's args label has the correct text.");
    is($(".call-item-location", item.target).getAttribute("value"), location,
      "The item's location label has the correct text.");
  }

  yield teardown(panel);
  finish();
}
