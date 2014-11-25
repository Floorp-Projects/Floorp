/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the call tree view renders after recording.
 */
function spawnTest () {
  let { panel } = yield initPerformance(SIMPLE_URL);
  let { EVENTS, CallTreeView, OverviewView } = panel.panelWin;

  let updated = 0;
  CallTreeView.on(EVENTS.CALL_TREE_RENDERED, () => updated++);

  let rendered = once(CallTreeView, EVENTS.CALL_TREE_RENDERED);

  yield startRecording(panel);
  yield busyWait(100);
  yield stopRecording(panel);
  yield rendered;

  rendered = once(CallTreeView, EVENTS.CALL_TREE_RENDERED);
  OverviewView.emit(EVENTS.OVERVIEW_RANGE_SELECTED, { beginAt: 0, endAt: 10 });
  yield rendered;

  ok(true, "Call tree rerenders when a range in the overview graph is selected.");

  rendered = once(CallTreeView, EVENTS.CALL_TREE_RENDERED);
  OverviewView.emit(EVENTS.OVERVIEW_RANGE_CLEARED);
  yield rendered;

  ok(true, "Call tree rerenders when a range in the overview graph is removed.");

  is(updated, 3, "CallTreeView rerendered 3 times.");

  yield teardown(panel);
  finish();
}
