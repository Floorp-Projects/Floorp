/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the graph's scale and position is reset on a page reload.
 */

add_task(async function() {
  const { target, panel } = await initWebAudioEditor(SIMPLE_CONTEXT_URL);
  const { panelWin } = panel;
  const { gFront, $, $$, EVENTS, ContextView } = panelWin;

  const started = once(gFront, "start-context");

  await Promise.all([
    waitForGraphRendered(panelWin, 3, 2),
    reload(target),
  ]);

  is(ContextView.getCurrentScale(), 1, "Default graph scale is 1.");
  is(ContextView.getCurrentTranslation()[0], 20, "Default x-translation is 20.");
  is(ContextView.getCurrentTranslation()[1], 20, "Default y-translation is 20.");

  // Change both attribute and D3's internal store
  panelWin.d3.select("#graph-target").attr("transform", "translate([100, 400]) scale(10)");
  ContextView._zoomBinding.scale(10);
  ContextView._zoomBinding.translate([100, 400]);

  is(ContextView.getCurrentScale(), 10, "After zoom, scale is 10.");
  is(ContextView.getCurrentTranslation()[0], 100, "After zoom, x-translation is 100.");
  is(ContextView.getCurrentTranslation()[1], 400, "After zoom, y-translation is 400.");

  await Promise.all([
    waitForGraphRendered(panelWin, 3, 2),
    reload(target),
  ]);

  is(ContextView.getCurrentScale(), 1, "After refresh, graph scale is 1.");
  is(ContextView.getCurrentTranslation()[0], 20, "After refresh, x-translation is 20.");
  is(ContextView.getCurrentTranslation()[1], 20, "After refresh, y-translation is 20.");

  await teardown(target);
});
