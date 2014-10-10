/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * This file contains tests for the mozLoop telemetry API.
 */

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
