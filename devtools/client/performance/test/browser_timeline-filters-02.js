/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";
/* eslint-disable */
/**
 * Tests markers filtering mechanism.
 */

const URL = EXAMPLE_URL + "doc_innerHTML.html";

async function spawnTest() {
  let { panel } = await initPerformance(URL);
  let { $, $$, EVENTS, PerformanceController, OverviewView, WaterfallView } = panel.panelWin;

  await startRecording(panel);
  ok(true, "Recording has started.");

  await waitUntil(() => {
    let markers = PerformanceController.getCurrentRecording().getMarkers();
    return markers.some(m => m.name == "Parse HTML") &&
           markers.some(m => m.name == "Javascript");
  });

  let waterfallRendered = WaterfallView.once(EVENTS.UI_WATERFALL_RENDERED);
  await stopRecording(panel);

  $("#filter-button").click();
  let filterJS = $("menuitem[marker-type=Javascript]");

  await waterfallRendered;

  ok($(".waterfall-marker-bar[type=Javascript]"), "Found at least one 'Javascript' marker");
  ok(!$(".waterfall-marker-bar[type='Parse HTML']"), "Found no Parse HTML markers as they are nested still");

  EventUtils.synthesizeMouseAtCenter(filterJS, {type: "mouseup"}, panel.panelWin);
  await Promise.all([
    WaterfallView.once(EVENTS.UI_WATERFALL_RENDERED),
    once(filterJS, "command")
  ]);

  ok(!$(".waterfall-marker-bar[type=Javascript]"), "Javascript markers are all hidden.");
  ok($(".waterfall-marker-bar[type='Parse HTML']"),
    "Found at least one 'Parse HTML' marker still visible after hiding JS markers");

  await teardown(panel);
  finish();
}
/* eslint-enable */
