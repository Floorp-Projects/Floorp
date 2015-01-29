/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const INVERT_PREF = "devtools.performance.ui.invert-call-tree";

/**
 * Tests that the call tree view renders after recording.
 */
function spawnTest () {
  let { panel } = yield initPerformance(SIMPLE_URL);
  let { EVENTS, DetailsView, CallTreeView } = panel.panelWin;

  Services.prefs.setBoolPref(INVERT_PREF, true);

  DetailsView.selectView("calltree");
  ok(DetailsView.isViewSelected(CallTreeView), "The call tree is now selected.");

  yield startRecording(panel);
  yield busyWait(100);

  let rendered = once(CallTreeView, EVENTS.CALL_TREE_RENDERED);
  yield stopRecording(panel);
  yield rendered;

  rendered = once(CallTreeView, EVENTS.CALL_TREE_RENDERED);
  Services.prefs.setBoolPref(INVERT_PREF, false);
  yield rendered;

  ok(true, "CallTreeView rerendered when toggling invert-call-tree.");

  rendered = once(CallTreeView, EVENTS.CALL_TREE_RENDERED);
  Services.prefs.setBoolPref(INVERT_PREF, true);
  yield rendered;

  ok(true, "CallTreeView rerendered when toggling back invert-call-tree.");

  yield teardown(panel);
  finish();
}
