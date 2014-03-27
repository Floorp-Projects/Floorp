/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the stepping buttons in the call list toolbar work as advertised.
 */

function ifTestingSupported() {
  let [target, debuggee, panel] = yield initCanavsDebuggerFrontend(SIMPLE_CANVAS_URL);
  let { window, $, EVENTS, SnapshotsListView, CallsListView } = panel.panelWin;

  yield reload(target);

  let recordingFinished = once(window, EVENTS.SNAPSHOT_RECORDING_FINISHED);
  let callListPopulated = once(window, EVENTS.CALL_LIST_POPULATED);
  SnapshotsListView._onRecordButtonClick();
  yield promise.all([recordingFinished, callListPopulated]);

  checkSteppingButtons(1, 1, 1, 1);
  is(CallsListView.selectedIndex, -1,
    "There should be no selected item in the calls list view initially.");

  CallsListView._onResume();
  checkSteppingButtons(1, 1, 1, 1);
  is(CallsListView.selectedIndex, 0,
    "The first draw call should now be selected.");

  CallsListView._onResume();
  checkSteppingButtons(1, 1, 1, 1);
  is(CallsListView.selectedIndex, 2,
    "The second draw call should now be selected.");

  CallsListView._onStepOver();
  checkSteppingButtons(1, 1, 1, 1);
  is(CallsListView.selectedIndex, 3,
    "The next context call should now be selected.");

  CallsListView._onStepOut();
  checkSteppingButtons(0, 0, 1, 0);
  is(CallsListView.selectedIndex, 7,
    "The last context call should now be selected.");

  function checkSteppingButtons(resume, stepOver, stepIn, stepOut) {
    if (!resume) {
      is($("#resume").getAttribute("disabled"), "true",
        "The resume button doesn't have the expected disabled state.");
    } else {
      is($("#resume").hasAttribute("disabled"), false,
        "The resume button doesn't have the expected enabled state.");
    }
    if (!stepOver) {
      is($("#step-over").getAttribute("disabled"), "true",
        "The stepOver button doesn't have the expected disabled state.");
    } else {
      is($("#step-over").hasAttribute("disabled"), false,
        "The stepOver button doesn't have the expected enabled state.");
    }
    if (!stepIn) {
      is($("#step-in").getAttribute("disabled"), "true",
        "The stepIn button doesn't have the expected disabled state.");
    } else {
      is($("#step-in").hasAttribute("disabled"), false,
        "The stepIn button doesn't have the expected enabled state.");
    }
    if (!stepOut) {
      is($("#step-out").getAttribute("disabled"), "true",
        "The stepOut button doesn't have the expected disabled state.");
    } else {
      is($("#step-out").hasAttribute("disabled"), false,
        "The stepOut button doesn't have the expected enabled state.");
    }
  }

  yield teardown(panel);
  finish();
}
