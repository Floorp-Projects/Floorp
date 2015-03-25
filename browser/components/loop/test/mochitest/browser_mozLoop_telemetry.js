/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * This file contains tests for the mozLoop telemetry API.
 */
Components.utils.import("resource://gre/modules/Promise.jsm", this);

add_task(loadLoopPanel);

/**
 * Enable local telemetry recording for the duration of the tests.
 */
add_task(function* test_initialize() {
  let oldCanRecord = Services.telemetry.canRecord;
  Services.telemetry.canRecord = true;
  registerCleanupFunction(function () {
    Services.telemetry.canRecord = oldCanRecord;
  });
});

/**
 * Tests that boolean histograms exist and can be updated.
 */
add_task(function* test_mozLoop_telemetryAdd_boolean() {
  for (let histogramId of [
    "LOOP_CLIENT_CALL_URL_REQUESTS_SUCCESS",
    "LOOP_CLIENT_CALL_URL_SHARED",
  ]) {
    let histogram = Services.telemetry.getHistogramById(histogramId);

    histogram.clear();
    for (let value of [false, false, true]) {
      gMozLoopAPI.telemetryAdd(histogramId, value);
    }

    let snapshot = histogram.snapshot();
    is(snapshot.counts[0], 2, "snapshot.counts[0] == 2");
    is(snapshot.counts[1], 1, "snapshot.counts[1] == 1");
  }
});

add_task(function* test_mozLoop_telemetryAdd_sharing_buckets() {
  let histogramId = "LOOP_SHARING_STATE_CHANGE";
  let histogram = Services.telemetry.getKeyedHistogramById(histogramId);
  const SHARING_STATES = gMozLoopAPI.SHARING_STATE_CHANGE;

  histogram.clear();
  for (let value of [SHARING_STATES.WINDOW_ENABLED,
                     SHARING_STATES.WINDOW_DISABLED,
                     SHARING_STATES.WINDOW_DISABLED,
                     SHARING_STATES.BROWSER_ENABLED,
                     SHARING_STATES.BROWSER_ENABLED,
                     SHARING_STATES.BROWSER_ENABLED,
                     SHARING_STATES.BROWSER_DISABLED,
                     SHARING_STATES.BROWSER_DISABLED,
                     SHARING_STATES.BROWSER_DISABLED,
                     SHARING_STATES.BROWSER_DISABLED]) {
    gMozLoopAPI.telemetryAddKeyedValue(histogramId, value);
  }

  let snapshot = histogram.snapshot();
  Assert.strictEqual(snapshot["WINDOW_ENABLED"].sum, 1, "SHARING_STATE_CHANGE.WINDOW_ENABLED");
  Assert.strictEqual(snapshot["WINDOW_DISABLED"].sum, 2, "SHARING_STATE_CHANGE.WINDOW_DISABLED");
  Assert.strictEqual(snapshot["BROWSER_ENABLED"].sum, 3, "SHARING_STATE_CHANGE.BROWSER_ENABLED");
  Assert.strictEqual(snapshot["BROWSER_DISABLED"].sum, 4, "SHARING_STATE_CHANGE.BROWSER_DISABLED");
});
