/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the waterfall view renders content after recording.
 */
function* spawnTest() {
  let { panel } = yield initPerformance(SIMPLE_URL);
  let { $, $$, EVENTS, PerformanceController, OverviewView, DetailsView, WaterfallView, MemoryCallTreeView } = panel.panelWin;
  let EPSILON = 0.00001;

  // Add the Cu.forceGC option to display allocation types for tests
  let triggerTypes = Services.prefs.getCharPref("devtools.performance.ui.show-triggers-for-gc-types");
  Services.prefs.setCharPref("devtools.performance.ui.show-triggers-for-gc-types", `${triggerTypes} COMPONENT_UTILS`);

  // Disable non GC markers so we don't have nested markers and marker index
  // matches DOM index.
  PerformanceController.setPref("hidden-markers", Array.reduce($$("menuitem"), (disabled, item) => {
    let type = item.getAttribute("marker-type");
    if (type && type !== "GarbageCollection") {
      disabled.push(type);
    }
    return disabled;
  }, []));

  Services.prefs.setBoolPref(ALLOCATIONS_PREF, true);

  yield startRecording(panel);
  yield waitUntil(hasGCMarkers(PerformanceController));
  let rendered = once(WaterfallView, EVENTS.WATERFALL_RENDERED);
  yield stopRecording(panel);
  ok(DetailsView.isViewSelected(WaterfallView),
    "The waterfall view is selected by default in the details view.");
  yield rendered;

  let bars = $$(".waterfall-marker-bar");
  let markers = PerformanceController.getCurrentRecording().getMarkers();
  let gcMarkers = markers.filter(isForceGCMarker);

  ok(gcMarkers.length >= 2, "should have atleast 2 GC markers");
  ok(bars.length >= 2, "should have atleast 2 GC markers rendered");

  info("Received markers:");
  for (let marker of gcMarkers) {
    info(`${marker.causeName} ${marker.start}:${marker.end}`);
  }

  /**
   * Check when it's the first GC marker
   */

  info(`Will show allocation trigger button for ${Services.prefs.getCharPref("devtools.performance.ui.show-triggers-for-gc-types")}`);
  info(`Clicking GC Marker of type ${gcMarkers[0].causeName} ${gcMarkers[0].start}:${gcMarkers[0].end}`);
  EventUtils.sendMouseEvent({ type: "mousedown" }, bars[0]);
  let showAllocsButton;
  // On slower machines this can not be found immediately?
  yield waitUntil(() => showAllocsButton = $("#waterfall-details .custom-button[type='show-allocations']"));
  ok(showAllocsButton, "GC buttons when allocations are enabled");

  rendered = once(MemoryCallTreeView, EVENTS.MEMORY_CALL_TREE_RENDERED);
  EventUtils.sendMouseEvent({ type: "click" }, showAllocsButton);
  yield rendered;

  is(OverviewView.getTimeInterval().startTime, 0, "When clicking first GC, should use 0 as start time");
  within(OverviewView.getTimeInterval().endTime, gcMarkers[0].start, EPSILON, "Correct end time range");

  let duration = PerformanceController.getCurrentRecording().getDuration();
  rendered = once(WaterfallView, EVENTS.WATERFALL_RENDERED);
  OverviewView.setTimeInterval({ startTime: 0, endTime: duration });
  yield DetailsView.selectView("waterfall");
  yield rendered;

  /**
   * Check when there is a previous GC marker
   */

  bars = $$(".waterfall-marker-bar");
  info(`Clicking GC Marker of type ${gcMarkers[1].causeName} ${gcMarkers[1].start}:${gcMarkers[1].end}`);
  EventUtils.sendMouseEvent({ type: "mousedown" }, bars[1]);
  // On slower machines this can not be found immediately?
  yield waitUntil(() => showAllocsButton = $("#waterfall-details .custom-button[type='show-allocations']"));
  ok(showAllocsButton, "GC buttons when allocations are enabled");

  rendered = once(MemoryCallTreeView, EVENTS.MEMORY_CALL_TREE_RENDERED);
  EventUtils.sendMouseEvent({ type: "click" }, showAllocsButton);
  yield rendered;

  within(OverviewView.getTimeInterval().startTime, gcMarkers[0].end, EPSILON,
    "selection start range is previous GC marker's end time");
  within(OverviewView.getTimeInterval().endTime, gcMarkers[1].start, EPSILON,
    "selection end range is current GC marker's start time");

  /**
   * Now with allocations disabled
   */

  // Reselect the entire recording -- due to bug 1196945, the new recording
  // won't reset the selection
  duration = PerformanceController.getCurrentRecording().getDuration();
  rendered = once(WaterfallView, EVENTS.WATERFALL_RENDERED);
  OverviewView.setTimeInterval({ startTime: 0, endTime: duration });
  yield rendered;

  Services.prefs.setBoolPref(ALLOCATIONS_PREF, false);
  yield startRecording(panel);
  yield waitUntil(hasGCMarkers(PerformanceController));

  rendered = once(WaterfallView, EVENTS.WATERFALL_RENDERED);
  yield stopRecording(panel);
  ok(DetailsView.isViewSelected(WaterfallView),
    "The waterfall view is selected by default in the details view.");
  yield rendered;

  ok(true, "WaterfallView rendered after recording is stopped.");

  bars = $$(".waterfall-marker-bar");
  markers = PerformanceController.getCurrentRecording().getMarkers();
  gcMarkers = markers.filter(isForceGCMarker);

  EventUtils.sendMouseEvent({ type: "mousedown" }, bars[0]);
  showAllocsButton = $("#waterfall-details .custom-button[type='show-allocations']");
  ok(!showAllocsButton, "No GC buttons when allocations are disabled");


  yield teardown(panel);
  finish();
}

function isForceGCMarker (m) {
  return m.name === "GarbageCollection" && m.causeName === "COMPONENT_UTILS" && m.start >= 0;
}

function hasGCMarkers (controller) {
  return function () {
    Cu.forceGC();
    return controller.getCurrentRecording().getMarkers().filter(isForceGCMarker).length >= 2;
  };
}
