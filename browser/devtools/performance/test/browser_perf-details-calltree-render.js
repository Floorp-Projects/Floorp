/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the call tree view renders content after recording.
 */
function* spawnTest() {
  let { panel } = yield initPerformance(SIMPLE_URL);
  let { EVENTS, DetailsView, JsCallTreeView } = panel.panelWin;

  yield startRecording(panel);
  yield busyWait(100);
  yield stopRecording(panel);

  let rendered = once(JsCallTreeView, EVENTS.JS_CALL_TREE_RENDERED);
  yield DetailsView.selectView("js-calltree");
  ok(DetailsView.isViewSelected(JsCallTreeView), "The call tree is now selected.");
  yield rendered;

  ok(true, "JsCallTreeView rendered after recording is stopped.");

  yield startRecording(panel);
  yield busyWait(100);

  rendered = once(JsCallTreeView, EVENTS.JS_CALL_TREE_RENDERED);
  yield stopRecording(panel);
  yield rendered;

  ok(true, "JsCallTreeView rendered again after recording completed a second time.");

  yield teardown(panel);
  finish();
}
