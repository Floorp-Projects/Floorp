/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the js call tree views get rerendered when toggling `show-jit-optimizations`
 */
function* spawnTest() {
  let { panel } = yield initPerformance(SIMPLE_URL);
  let { EVENTS, $, DetailsView, JsCallTreeView } = panel.panelWin;

  Services.prefs.setBoolPref(JIT_PREF, true);

  yield startRecording(panel);
  yield stopRecording(panel);

  let rendered = once(JsCallTreeView, EVENTS.JS_CALL_TREE_RENDERED);
  yield DetailsView.selectView("js-calltree");
  yield rendered;

  ok(!$("#jit-optimizations-view").classList.contains("hidden"), "JIT Optimizations shown");

  rendered = once(JsCallTreeView, EVENTS.JS_CALL_TREE_RENDERED);
  Services.prefs.setBoolPref(JIT_PREF, false);
  yield rendered;
  ok(true, "JsCallTreeView rerendered when toggling off show-jit-optimizations.");
  ok($("#jit-optimizations-view").classList.contains("hidden"), "JIT Optimizations hidden");

  rendered = once(JsCallTreeView, EVENTS.JS_CALL_TREE_RENDERED);
  Services.prefs.setBoolPref(JIT_PREF, true);
  yield rendered;
  ok(true, "JsCallTreeView rerendered when toggling off show-jit-optimizations.");
  ok(!$("#jit-optimizations-view").classList.contains("hidden"), "JIT Optimizations shown");

  yield teardown(panel);
  finish();
}
