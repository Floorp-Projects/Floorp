/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the graph's scale and position is reset on a page reload.
 */

function spawnTest() {
  let [target, debuggee, panel] = yield initWebAudioEditor(SIMPLE_CONTEXT_URL);
  let { panelWin } = panel;
  let { gFront, $, $$, EVENTS, WebAudioGraphView } = panelWin;

  let started = once(gFront, "start-context");

  yield Promise.all([
    reload(target),
    waitForGraphRendered(panelWin, 3, 2)
  ]);

  is(WebAudioGraphView.getCurrentScale(), 1, "Default graph scale is 1.");
  is(WebAudioGraphView.getCurrentTranslation()[0], 20, "Default x-translation is 20.");
  is(WebAudioGraphView.getCurrentTranslation()[1], 20, "Default y-translation is 20.");

  // Change both attribute and D3's internal store
  panelWin.d3.select("#graph-target").attr("transform", "translate([100, 400]) scale(10)");
  WebAudioGraphView._zoomBinding.scale(10);
  WebAudioGraphView._zoomBinding.translate([100, 400]);

  is(WebAudioGraphView.getCurrentScale(), 10, "After zoom, scale is 10.");
  is(WebAudioGraphView.getCurrentTranslation()[0], 100, "After zoom, x-translation is 100.");
  is(WebAudioGraphView.getCurrentTranslation()[1], 400, "After zoom, y-translation is 400.");

  yield Promise.all([
    reload(target),
    waitForGraphRendered(panelWin, 3, 2)
  ]);

  is(WebAudioGraphView.getCurrentScale(), 1, "After refresh, graph scale is 1.");
  is(WebAudioGraphView.getCurrentTranslation()[0], 20, "After refresh, x-translation is 20.");
  is(WebAudioGraphView.getCurrentTranslation()[1], 20, "After refresh, y-translation is 20.");

  yield teardown(panel);
  finish();
}

