/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the js flamegraphs get rerendered when toggling `flatten-tree-recursion`
 */
function spawnTest () {
  let { panel } = yield initPerformance(SIMPLE_URL);
  let { EVENTS, PerformanceController, DetailsView, JsFlameGraphView, FlameGraphUtils } = panel.panelWin;

  Services.prefs.setBoolPref(FLATTEN_PREF, true);

  yield startRecording(panel);
  yield busyWait(100);

  yield stopRecording(panel);
  let rendered = once(JsFlameGraphView, EVENTS.JS_FLAMEGRAPH_RENDERED);
  yield DetailsView.selectView("js-flamegraph");
  yield rendered;

  let thread1 = PerformanceController.getCurrentRecording().getProfile().threads[0];
  let rendering1 = FlameGraphUtils._cache.get(thread1);

  ok(thread1,
    "The samples were retrieved from the controller.");
  ok(rendering1,
    "The rendering data was cached.");

  rendered = once(JsFlameGraphView, EVENTS.JS_FLAMEGRAPH_RENDERED);
  Services.prefs.setBoolPref(FLATTEN_PREF, false);
  yield rendered;

  ok(true, "JsFlameGraphView rerendered when toggling flatten-tree-recursion.");

  let thread2 = PerformanceController.getCurrentRecording().getProfile().threads[0];
  let rendering2 = FlameGraphUtils._cache.get(thread2);

  is(thread1, thread2,
    "The same samples data should be retrieved from the controller (1).");
  isnot(rendering1, rendering2,
    "The rendering data should be different because other options were used (1).");

  rendered = once(JsFlameGraphView, EVENTS.JS_FLAMEGRAPH_RENDERED);
  Services.prefs.setBoolPref(FLATTEN_PREF, true);
  yield rendered;

  ok(true, "JsFlameGraphView rerendered when toggling back flatten-tree-recursion.");

  let thread3 = PerformanceController.getCurrentRecording().getProfile().threads[0];
  let rendering3 = FlameGraphUtils._cache.get(thread3);

  is(thread2, thread3,
    "The same samples data should be retrieved from the controller (2).");
  isnot(rendering2, rendering3,
    "The rendering data should be different because other options were used (2).");

  yield teardown(panel);
  finish();
}
