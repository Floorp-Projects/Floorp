/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the js flamegraphs get rerendered when toggling `flatten-tree-recursion`
 */
function spawnTest () {
  let { panel } = yield initPerformance(SIMPLE_URL);
  let { EVENTS, PerformanceController, DetailsView, JsFlameGraphView, FlameGraphUtils } = panel.panelWin;

  Services.prefs.setBoolPref(FLATTEN_PREF, true);

  yield DetailsView.selectView("js-flamegraph");

  yield startRecording(panel);
  yield busyWait(100);

  let rendered = once(JsFlameGraphView, EVENTS.JS_FLAMEGRAPH_RENDERED);
  yield stopRecording(panel);
  yield rendered;

  let samples1 = PerformanceController.getCurrentRecording().getProfile().threads[0].samples;
  let rendering1 = FlameGraphUtils._cache.get(samples1);

  ok(samples1,
    "The samples were retrieved from the controller.");
  ok(rendering1,
    "The rendering data was cached.");

  rendered = once(JsFlameGraphView, EVENTS.JS_FLAMEGRAPH_RENDERED);
  Services.prefs.setBoolPref(FLATTEN_PREF, false);
  yield rendered;

  ok(true, "JsFlameGraphView rerendered when toggling flatten-tree-recursion.");

  let samples2 = PerformanceController.getCurrentRecording().getProfile().threads[0].samples;
  let rendering2 = FlameGraphUtils._cache.get(samples2);

  is(samples1, samples2,
    "The same samples data should be retrieved from the controller (1).");
  isnot(rendering1, rendering2,
    "The rendering data should be different because other options were used (1).");

  rendered = once(JsFlameGraphView, EVENTS.JS_FLAMEGRAPH_RENDERED);
  Services.prefs.setBoolPref(FLATTEN_PREF, true);
  yield rendered;

  ok(true, "JsFlameGraphView rerendered when toggling back flatten-tree-recursion.");

  let samples3 = PerformanceController.getCurrentRecording().getProfile().threads[0].samples;
  let rendering3 = FlameGraphUtils._cache.get(samples3);

  is(samples2, samples3,
    "The same samples data should be retrieved from the controller (2).");
  isnot(rendering2, rendering3,
    "The rendering data should be different because other options were used (2).");

  yield teardown(panel);
  finish();
}
