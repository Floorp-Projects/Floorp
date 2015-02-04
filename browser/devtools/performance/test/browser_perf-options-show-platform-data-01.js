/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const PLATFORM_DATA_PREF = "devtools.performance.ui.show-platform-data";

/**
 * Tests that the JsCallTree get rerendered when toggling `show-platform-data`
 */
function spawnTest () {
  let { panel } = yield initPerformance(SIMPLE_URL);
  let { EVENTS, DetailsView, JsFlameGraphView, JsCallTreeView } = panel.panelWin;

  DetailsView.selectView("js-calltree");

  Services.prefs.setBoolPref(PLATFORM_DATA_PREF, true);

  yield startRecording(panel);
  yield busyWait(100);

  let rendered = once(JsCallTreeView, EVENTS.JS_CALL_TREE_RENDERED);
  yield stopRecording(panel);
  yield rendered;

  rendered = once(JsCallTreeView, EVENTS.JS_CALL_TREE_RENDERED);
  Services.prefs.setBoolPref(PLATFORM_DATA_PREF, false);
  yield rendered;
  ok(true, "JsCallTreeView rerendered when toggling off show-platform-data.");

  rendered = once(JsCallTreeView, EVENTS.JS_CALL_TREE_RENDERED);
  Services.prefs.setBoolPref(PLATFORM_DATA_PREF, true);
  yield rendered;
  ok(true, "JsCallTreeView rerendered when toggling on show-platform-data.");

  yield teardown(panel);
  finish();
}
