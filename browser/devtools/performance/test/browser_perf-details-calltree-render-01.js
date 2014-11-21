/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the call tree view renders after recording.
 */
function spawnTest () {
  let { panel } = yield initPerformance(SIMPLE_URL);
  let { EVENTS, CallTreeView } = panel.panelWin;

  let updated = 0;
  CallTreeView.on(EVENTS.CALL_TREE_RENDERED, () => updated++);

  yield startRecording(panel);
  yield busyWait(100);

  let rendered = once(CallTreeView, EVENTS.CALL_TREE_RENDERED);
  yield stopRecording(panel);
  yield rendered;

  ok(true, "CallTreeView rendered on recording completed.");

  yield startRecording(panel);
  yield busyWait(100);

  rendered = once(CallTreeView, EVENTS.CALL_TREE_RENDERED);
  yield stopRecording(panel);
  yield rendered;

  ok(true, "CallTreeView rendered again after recording completed a second time.");

  yield teardown(panel);
  finish();
}
