/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the waterfall is properly built after finishing a recording.
 */

function spawnTest () {
  let { target, panel } = yield initPerformance(SIMPLE_URL);
  let { $, $$, EVENTS, PerformanceController, OverviewView, WaterfallView } = panel.panelWin;

  yield startRecording(panel);
  ok(true, "Recording has started.");

  let updated = 0;
  OverviewView.on(EVENTS.OVERVIEW_RENDERED, () => updated++);

  ok((yield waitUntil(() => updated > 0)),
    "The overview graphs were updated a bunch of times.");
  ok((yield waitUntil(() => PerformanceController.getCurrentRecording().getMarkers().length > 0)),
    "There are some markers available.");

  yield stopRecording(panel);
  ok(true, "Recording has ended.");

  // Test the header container.

  ok($(".waterfall-header-container"),
    "A header container should have been created.");

  // Test the header sidebar (left).

  ok($(".waterfall-header-container > .waterfall-sidebar"),
    "A header sidebar node should have been created.");
  ok($(".waterfall-header-container > .waterfall-sidebar > .waterfall-header-name"),
    "A header name label should have been created inside the sidebar.");

  // Test the header ticks (right).

  ok($(".waterfall-header-ticks"),
    "A header ticks node should have been created.");
  ok($$(".waterfall-header-ticks > .waterfall-header-tick").length > 0,
    "Some header tick labels should have been created inside the tick node.");

  // Test the markers container.

  ok($(".waterfall-marker-container"),
    "A marker container should have been created.");

  // Test the markers sidebar (left).

  ok($$(".waterfall-marker-container > .waterfall-sidebar").length,
    "Some marker sidebar nodes should have been created.");
  ok($$(".waterfall-marker-container > .waterfall-sidebar:not(spacer) > .waterfall-marker-bullet").length,
    "Some marker color bullets should have been created inside the sidebar.");
  ok($$(".waterfall-marker-container > .waterfall-sidebar:not(spacer) > .waterfall-marker-name").length,
    "Some marker name labels should have been created inside the sidebar.");

  // Test the markers waterfall (right).

  ok($$(".waterfall-marker-item").length,
    "Some marker waterfall nodes should have been created.");
  ok($$(".waterfall-marker-item:not(spacer) > .waterfall-marker-bar").length,
    "Some marker color bars should have been created inside the waterfall.");
  yield teardown(panel);
  finish();
}
