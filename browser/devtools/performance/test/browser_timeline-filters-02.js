/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests markers filtering mechanism.
 */

const URL = EXAMPLE_URL + "doc_innerHTML.html";

function* spawnTest() {
  let { panel } = yield initPerformance(URL);
  let { $, $$, EVENTS, PerformanceController, OverviewView, WaterfallView } = panel.panelWin;

  yield startRecording(panel);
  ok(true, "Recording has started.");

  yield waitUntil(() => {
    let markers = PerformanceController.getCurrentRecording().getMarkers();
    return markers.some(m => m.name == "Parse HTML") &&
           markers.some(m => m.name == "Javascript");
  });

  let waterfallRendered = WaterfallView.once(EVENTS.WATERFALL_RENDERED);
  yield stopRecording(panel);

  $("#filter-button").click();
  let filterJS = $("menuitem[marker-type=Javascript]");

  yield waterfallRendered;

  ok($(".waterfall-marker-bar[type=Javascript]"), "Found at least one 'Javascript' marker");
  ok(!$(".waterfall-marker-bar[type='Parse HTML']"), "Found no Parse HTML markers as they are nested still");

  EventUtils.synthesizeMouseAtCenter(filterJS, {type: "mouseup"}, panel.panelWin);
  yield Promise.all([
    WaterfallView.once(EVENTS.WATERFALL_RENDERED),
    once(filterJS, "command")
  ]);

  ok(!$(".waterfall-marker-bar[type=Javascript]"), "Javascript markers are all hidden.");
  ok($(".waterfall-marker-bar[type='Parse HTML']"),
    "Found at least one 'Parse HTML' marker still visible after hiding JS markers");

  yield teardown(panel);
  finish();
}
