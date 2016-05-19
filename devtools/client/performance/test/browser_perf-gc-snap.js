/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the marker details on GC markers displays allocation
 * buttons and snaps to the correct range
 */
function* spawnTest() {
  let { panel } = yield initPerformance(ALLOCS_URL);
  let { $, $$, EVENTS, PerformanceController, OverviewView, DetailsView, WaterfallView, MemoryCallTreeView } = panel.panelWin;
  let EPSILON = 0.00001;

  Services.prefs.setBoolPref(ALLOCATIONS_PREF, true);

  yield startRecording(panel);
  yield idleWait(1000);
  yield stopRecording(panel);

  injectGCMarkers(PerformanceController, WaterfallView);

  // Select everything
  let rendered = WaterfallView.once(EVENTS.UI_WATERFALL_RENDERED);
  OverviewView.setTimeInterval({ startTime: 0, endTime: Number.MAX_VALUE });
  yield rendered;

  let bars = $$(".waterfall-marker-bar");
  let gcMarkers = PerformanceController.getCurrentRecording().getMarkers();
  ok(gcMarkers.length === 9, "should have 9 GC markers");
  ok(bars.length === 9, "should have 9 GC markers rendered");

  /**
   * Check when it's the second marker of the first GC cycle.
   */

  let targetMarker = gcMarkers[1];
  let targetBar = bars[1];
  info(`Clicking GC Marker of type ${targetMarker.causeName} ${targetMarker.start}:${targetMarker.end}`);
  EventUtils.sendMouseEvent({ type: "mousedown" }, targetBar);
  let showAllocsButton;
  // On slower machines this can not be found immediately?
  yield waitUntil(() => showAllocsButton = $("#waterfall-details .custom-button[type='show-allocations']"));
  ok(showAllocsButton, "GC buttons when allocations are enabled");

  rendered = once(MemoryCallTreeView, EVENTS.UI_MEMORY_CALL_TREE_RENDERED);
  EventUtils.sendMouseEvent({ type: "click" }, showAllocsButton);
  yield rendered;

  is(OverviewView.getTimeInterval().startTime, 0, "When clicking first GC, should use 0 as start time");
  within(OverviewView.getTimeInterval().endTime, targetMarker.start, EPSILON, "Correct end time range");

  let duration = PerformanceController.getCurrentRecording().getDuration();
  rendered = once(WaterfallView, EVENTS.UI_WATERFALL_RENDERED);
  OverviewView.setTimeInterval({ startTime: 0, endTime: duration });
  yield DetailsView.selectView("waterfall");
  yield rendered;

  /**
   * Check when there is a previous GC cycle
   */

  bars = $$(".waterfall-marker-bar");
  targetMarker = gcMarkers[4];
  targetBar = bars[4];

  info(`Clicking GC Marker of type ${targetMarker.causeName} ${targetMarker.start}:${targetMarker.end}`);
  EventUtils.sendMouseEvent({ type: "mousedown" }, targetBar);
  // On slower machines this can not be found immediately?
  yield waitUntil(() => showAllocsButton = $("#waterfall-details .custom-button[type='show-allocations']"));
  ok(showAllocsButton, "GC buttons when allocations are enabled");

  rendered = once(MemoryCallTreeView, EVENTS.UI_MEMORY_CALL_TREE_RENDERED);
  EventUtils.sendMouseEvent({ type: "click" }, showAllocsButton);
  yield rendered;

  within(OverviewView.getTimeInterval().startTime, gcMarkers[2].end, EPSILON,
    "selection start range is last marker from previous GC cycle.");
  within(OverviewView.getTimeInterval().endTime, targetMarker.start, EPSILON,
    "selection end range is current GC marker's start time");

  /**
   * Now with allocations disabled
   */

  // Reselect the entire recording -- due to bug 1196945, the new recording
  // won't reset the selection
  duration = PerformanceController.getCurrentRecording().getDuration();
  rendered = once(WaterfallView, EVENTS.UI_WATERFALL_RENDERED);
  OverviewView.setTimeInterval({ startTime: 0, endTime: duration });
  yield rendered;

  Services.prefs.setBoolPref(ALLOCATIONS_PREF, false);
  yield startRecording(panel);
  rendered = once(WaterfallView, EVENTS.UI_WATERFALL_RENDERED);
  yield stopRecording(panel);
  yield rendered;

  injectGCMarkers(PerformanceController, WaterfallView);

  // Select everything
  rendered = WaterfallView.once(EVENTS.UI_WATERFALL_RENDERED);
  OverviewView.setTimeInterval({ startTime: 0, endTime: Number.MAX_VALUE });
  yield rendered;

  ok(true, "WaterfallView rendered after recording is stopped.");

  bars = $$(".waterfall-marker-bar");
  gcMarkers = PerformanceController.getCurrentRecording().getMarkers();

  EventUtils.sendMouseEvent({ type: "mousedown" }, bars[0]);
  showAllocsButton = $("#waterfall-details .custom-button[type='show-allocations']");
  ok(!showAllocsButton, "No GC buttons when allocations are disabled");


  yield teardown(panel);
  finish();
}

function injectGCMarkers(controller, waterfall) {
  // Push some fake GC markers into the recording
  let realMarkers = controller.getCurrentRecording().getMarkers();
  // Invalidate marker cache
  waterfall._cache.delete(realMarkers);
  realMarkers.length = 0;
  for (let gcMarker of GC_MARKERS) {
    realMarkers.push(gcMarker);
  }
}

var GC_MARKERS = [
  { causeName: "TOO_MUCH_MALLOC", cycle: 1 },
  { causeName: "TOO_MUCH_MALLOC", cycle: 1 },
  { causeName: "TOO_MUCH_MALLOC", cycle: 1 },
  { causeName: "ALLOC_TRIGGER", cycle: 2 },
  { causeName: "ALLOC_TRIGGER", cycle: 2 },
  { causeName: "ALLOC_TRIGGER", cycle: 2 },
  { causeName: "SET_NEW_DOCUMENT", cycle: 3 },
  { causeName: "SET_NEW_DOCUMENT", cycle: 3 },
  { causeName: "SET_NEW_DOCUMENT", cycle: 3 },
].map((marker, i) => {
  marker.name = "GarbageCollection";
  marker.start = 50 + (i * 10);
  marker.end = marker.start + 9;
  return marker;
});
