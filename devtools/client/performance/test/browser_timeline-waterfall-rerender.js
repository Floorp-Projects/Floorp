/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the waterfall remembers the selection when rerendering.
 */

function* spawnTest() {
  let { target, panel } = yield initPerformance(SIMPLE_URL);
  let { $, $$, EVENTS, PerformanceController, OverviewView, WaterfallView } = panel.panelWin;

  const MIN_MARKERS_COUNT = 50;
  const MAX_MARKERS_SELECT = 20;

  yield startRecording(panel);
  ok(true, "Recording has started.");

  let updated = 0;
  OverviewView.on(EVENTS.OVERVIEW_RENDERED, () => updated++);

  ok((yield waitUntil(() => updated > 0)),
    "The overview graphs were updated a bunch of times.");
  ok((yield waitUntil(() => PerformanceController.getCurrentRecording().getMarkers().length > MIN_MARKERS_COUNT)),
    "There are some markers available.");

  yield stopRecording(panel);
  ok(true, "Recording has ended.");

  let currentMarkers = PerformanceController.getCurrentRecording().getMarkers();
  info("Gathered markers: " + JSON.stringify(currentMarkers, null, 2));

  let initialBarsCount = $$(".waterfall-marker-bar").length;
  info("Initial bars count: " + initialBarsCount);

  // Select a portion of the overview.
  let rerendered = WaterfallView.once(EVENTS.WATERFALL_RENDERED);
  OverviewView.setTimeInterval({ startTime: 0, endTime: currentMarkers[MAX_MARKERS_SELECT].end });
  yield rerendered;

  ok(!$(".waterfall-tree-item:focus"),
    "There is no item focused in the waterfall yet.");
  ok($("#waterfall-details").hidden,
    "The waterfall sidebar is initially hidden.");

  // Focus the second item in the tree.
  WaterfallView._markersRoot.getChild(1).focus();

  let beforeResizeBarsCount = $$(".waterfall-marker-bar").length;
  info("Before resize bars count: " + beforeResizeBarsCount);
  ok(beforeResizeBarsCount < initialBarsCount,
    "A subset of the total markers was selected.");

  is(Array.indexOf($$(".waterfall-tree-item"), $(".waterfall-tree-item:focus")), 2,
    "The correct item was focused in the tree.");
  ok(!$("#waterfall-details").hidden,
    "The waterfall sidebar is now visible.");

  // Simulate a resize on the marker details.
  rerendered = WaterfallView.once(EVENTS.WATERFALL_RENDERED);
  EventUtils.sendMouseEvent({ type: "mouseup" }, WaterfallView.detailsSplitter);
  yield rerendered;

  let afterResizeBarsCount = $$(".waterfall-marker-bar").length;
  info("After resize bars count: " + afterResizeBarsCount);
  is(afterResizeBarsCount, beforeResizeBarsCount,
    "The same subset of the total markers remained visible.");

  is(Array.indexOf($$(".waterfall-tree-item"), $(".waterfall-tree-item:focus")), 2,
    "The correct item is still focused in the tree.");
  ok(!$("#waterfall-details").hidden,
    "The waterfall sidebar is still visible.");

  yield teardown(panel);
  finish();
}
