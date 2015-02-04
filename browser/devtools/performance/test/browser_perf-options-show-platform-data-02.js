/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const PLATFORM_DATA_PREF = "devtools.performance.ui.show-platform-data";

/**
 * Tests that the JsFlamegraphs get rerendered when toggling `show-platform-data`
 */
function spawnTest () {
  let { panel } = yield initPerformance(SIMPLE_URL);
  let { EVENTS, DetailsView, JsFlameGraphView } = panel.panelWin;

  Services.prefs.setBoolPref(PLATFORM_DATA_PREF, false);
  DetailsView.selectView("js-flamegraph");

  yield startRecording(panel);
  yield busyWait(100);

  let rendered = once(JsFlameGraphView, EVENTS.JS_FLAMEGRAPH_RENDERED);
  yield stopRecording(panel);
  yield rendered;

  rendered = once(JsFlameGraphView, EVENTS.JS_FLAMEGRAPH_RENDERED);
  Services.prefs.setBoolPref(PLATFORM_DATA_PREF, true);
  yield rendered;
  ok(true, "JsFlameGraphView rerendered when toggling on show-platform-data.");

  rendered = once(JsFlameGraphView, EVENTS.JS_FLAMEGRAPH_RENDERED);
  Services.prefs.setBoolPref(PLATFORM_DATA_PREF, false);
  yield rendered;
  ok(true, "JsFlameGraphView rerendered when toggling off show-platform-data.");

  yield teardown(panel);
  finish();
}
