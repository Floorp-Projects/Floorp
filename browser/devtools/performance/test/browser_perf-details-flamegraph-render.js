/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the flamegraph view renders content after recording.
 */
function spawnTest () {
  let { panel } = yield initPerformance(SIMPLE_URL);
  let { EVENTS, DetailsView, JsFlameGraphView } = panel.panelWin;

  yield startRecording(panel);
  yield busyWait(100);
  yield stopRecording(panel);

  let rendered = once(JsFlameGraphView, EVENTS.JS_FLAMEGRAPH_RENDERED);
  yield DetailsView.selectView("js-flamegraph");
  ok(DetailsView.isViewSelected(JsFlameGraphView), "The flamegraph is now selected.");
  yield rendered;

  ok(true, "JsFlameGraphView rendered after recording is stopped.");

  yield startRecording(panel);
  yield busyWait(100);

  rendered = once(JsFlameGraphView, EVENTS.JS_FLAMEGRAPH_RENDERED);
  yield stopRecording(panel);
  yield rendered;

  ok(true, "JsFlameGraphView rendered again after recording completed a second time.");

  yield teardown(panel);
  finish();
}
