/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that only js-calltree view is on, default, and many things are hidden
 * when in retro mode.
 */
const HIDDEN_OPTIONS = ["option-enable-memory", "option-invert-flame-graph", "option-show-jit-optimizations", "option-flatten-tree-recursion"];

Services.prefs.setBoolPref("devtools.performance.ui.retro-mode", true);

function spawnTest () {
  let { panel } = yield initPerformance(SIMPLE_URL);
  let { EVENTS, DetailsView, PerformanceController, $, $$, JsCallTreeView } = panel.panelWin;

  yield startRecording(panel);
  yield stopRecording(panel);

  let model = PerformanceController.getCurrentRecording();

  ok(model.getMemory().length === 0, "model did not record memory.");
  ok(model.getTicks().length !== 0, "model did get ticks.");
  ok(model.getAllocations().sites.length === 0, "model did get allocation data.");
  ok(model.getAllocations().timestamps.length === 0, "model did get allocation data.");
  ok(model.getAllocations().frames.length === 0, "model did get allocation data.");
  ok(model.getAllocations().counts.length === 0, "model did get allocation data.");

  ok(DetailsView.isViewSelected(JsCallTreeView),
    "The jscalltree view is selected by default");

  for (let option of $$("#performance-options-menupopup > menuitem")) {
    if (HIDDEN_OPTIONS.indexOf(option.id) !== -1) {
      ok(option.hidden === true, `${option.id} should be hidden.`);
    } else {
      ok(option.hidden === false, `${option.id} should be visible.`);
    }
  }

  for (let viewbutton of $$("#performance-toolbar-controls-detail-views > toolbarbutton")) {
    ok (viewbutton.hidden === true, `${viewbutton.id} should be hidden.`);
  }

  ok($("#markers-overview").hidden, "markers overview should be hidden.");
  ok($("#memory-overview").hidden, "memory overview should be hidden.");
  ok(!$("#time-framerate").hidden, "framerate should be shown.");

  yield teardown(panel);
  finish();
}
