/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the slider in the calls list view works as advertised.
 */

function ifTestingSupported() {
  let [target, debuggee, panel] = yield initCanavsDebuggerFrontend(SIMPLE_CANVAS_URL);
  let { window, $, EVENTS, SnapshotsListView, CallsListView } = panel.panelWin;

  yield reload(target);

  let recordingFinished = once(window, EVENTS.SNAPSHOT_RECORDING_FINISHED);
  let callListPopulated = once(window, EVENTS.CALL_LIST_POPULATED);
  SnapshotsListView._onRecordButtonClick();
  yield promise.all([recordingFinished, callListPopulated]);

  is(CallsListView.selectedIndex, -1,
    "No item in the function calls list should be initially selected.");

  is($("#calls-slider").value, 0,
    "The slider should be moved all the way to the start.");
  is($("#calls-slider").min, 0,
    "The slider minimum value should be 0.");
  is($("#calls-slider").max, 7,
    "The slider maximum value should be 7.");

  CallsListView.selectedIndex = 1;
  is($("#calls-slider").value, 1,
    "The slider should be changed according to the current selection.");

  $("#calls-slider").value = 2;
  is(CallsListView.selectedIndex, 2,
    "The calls selection should be changed according to the current slider value.");

  yield teardown(panel);
  finish();
}
