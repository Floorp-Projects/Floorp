/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const IDLE_PREF = "devtools.performance.ui.show-idle-blocks";

/**
 * Tests that the js Flamegraphs gets rerendered when toggling `show-idle-blocks`
 */
function spawnTest () {
  let { panel } = yield initPerformance(SIMPLE_URL);
  let { EVENTS, DetailsView, JsFlameGraphView } = panel.panelWin;

  DetailsView.selectView("js-flamegraph");

  Services.prefs.setBoolPref(IDLE_PREF, true);

  yield startRecording(panel);
  yield busyWait(100);

  let rendered = once(JsFlameGraphView, EVENTS.JS_FLAMEGRAPH_RENDERED);
  yield stopRecording(panel);
  yield rendered;

  rendered = once(JsFlameGraphView, EVENTS.JS_FLAMEGRAPH_RENDERED);
  Services.prefs.setBoolPref(IDLE_PREF, false);
  yield rendered;

  ok(true, "JsFlameGraphView rerendered when toggling show-idle-blocks.");

  rendered = once(JsFlameGraphView, EVENTS.JS_FLAMEGRAPH_RENDERED);
  Services.prefs.setBoolPref(IDLE_PREF, true);
  yield rendered;

  ok(true, "JsFlameGraphView rerendered when toggling back show-idle-blocks.");

  yield teardown(panel);
  finish();
}
