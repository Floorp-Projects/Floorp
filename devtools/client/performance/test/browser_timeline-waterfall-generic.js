/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the waterfall is properly built after finishing a recording.
 */

function* spawnTest() {
  // This test seems to take a long time to cleanup on Ubuntu VMs.
  requestLongerTimeout(2);

  let { target, panel } = yield initPerformance(SIMPLE_URL);
  let { $, $$, EVENTS, PerformanceController, OverviewView, WaterfallView, DetailsView } = panel.panelWin;
  let { WATERFALL_MARKER_SIDEBAR_SAFE_BOUNDS } = require("devtools/client/performance/modules/widgets/marker-view");

  yield startRecording(panel);
  ok(true, "Recording has started.");

  let updated = 0;
  OverviewView.on(EVENTS.OVERVIEW_RENDERED, () => updated++);

  ok((yield waitUntil(() => updated > 0)),
    "The overview graphs were updated a bunch of times.");
  ok((yield waitUntil(() => PerformanceController.getCurrentRecording().getMarkers().length > 0)),
    "There are some markers available.");

  let rendered = Promise.all([
    DetailsView.selectView("waterfall"),
    once(WaterfallView, EVENTS.WATERFALL_RENDERED)
  ]);

  yield stopRecording(panel);
  ok(true, "Recording has ended.");

  yield rendered;
  ok(true, "Recording has rendered.");

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

  // Test the markers sidebar (left).

  ok($$(".waterfall-tree-item > .waterfall-sidebar").length,
    "Some marker sidebar nodes should have been created.");
  ok($$(".waterfall-tree-item > .waterfall-sidebar > .waterfall-marker-bullet").length,
    "Some marker color bullets should have been created inside the sidebar.");
  ok($$(".waterfall-tree-item > .waterfall-sidebar > .waterfall-marker-name").length,
    "Some marker name labels should have been created inside the sidebar.");

  // Test the markers waterfall (right).

  ok($$(".waterfall-tree-item > .waterfall-marker").length,
    "Some marker waterfall nodes should have been created.");
  ok($$(".waterfall-tree-item > .waterfall-marker > .waterfall-marker-bar").length,
    "Some marker color bars should have been created inside the waterfall.");

  // Test the sidebar

  let detailsView = WaterfallView.details;
  let markersRoot = WaterfallView._markersRoot;

  let parentWidthBefore = $("#waterfall-view").getBoundingClientRect().width;
  let sidebarWidthBefore = $(".waterfall-sidebar").getBoundingClientRect().width;
  let detailsWidthBefore = $("#waterfall-details").getBoundingClientRect().width;

  ok(detailsView.hidden,
    "The details view in the waterfall view is hidden by default.");
  is(detailsWidthBefore, 0,
    "The details view width should be 0 when hidden.");
  is(markersRoot._waterfallWidth, parentWidthBefore - sidebarWidthBefore - WATERFALL_MARKER_SIDEBAR_SAFE_BOUNDS,
    "The waterfall width is correct.")

  let receivedFocusEvent = markersRoot.once("focus");
  let waterfallRerendered = once(WaterfallView, EVENTS.WATERFALL_RENDERED);
  WaterfallView._markersRoot.getChild(0).focus();
  yield receivedFocusEvent;
  yield waterfallRerendered;

  let parentWidthAfter = $("#waterfall-view").getBoundingClientRect().width;
  let sidebarWidthAfter = $(".waterfall-sidebar").getBoundingClientRect().width;
  let detailsWidthAfter = $("#waterfall-details").getBoundingClientRect().width;

  ok(!detailsView.hidden,
    "The details view in the waterfall view is now visible.");
  is(parentWidthBefore, parentWidthAfter,
    "The parent view's width should not have changed.");
  is(sidebarWidthBefore, sidebarWidthAfter,
    "The sidebar view's width should not have changed.");
  isnot(detailsWidthAfter, 0,
    "The details view width should not be 0 when visible.");
  is(markersRoot._waterfallWidth, parentWidthAfter - sidebarWidthAfter - detailsWidthAfter - WATERFALL_MARKER_SIDEBAR_SAFE_BOUNDS,
    "The waterfall width is correct (2).")

  yield teardown(panel);
  finish();
}
