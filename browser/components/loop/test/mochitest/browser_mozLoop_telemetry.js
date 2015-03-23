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
  let oldCanRecord = Services.telemetry.canRecordExtended;
  Services.telemetry.canRecordExtended = true;
  registerCleanupFunction(function () {
    Services.telemetry.canRecordExtended = oldCanRecord;
  });
});

/**
 * Tests that enumerated bucket histograms exist and can be updated.
 */
add_task(function* test_mozLoop_telemetryAdd_buckets() {
  let histogramId = "LOOP_TWO_WAY_MEDIA_CONN_LENGTH";
  let histogram = Services.telemetry.getKeyedHistogramById(histogramId);
  let CONN_LENGTH = gMozLoopAPI.TWO_WAY_MEDIA_CONN_LENGTH;

  histogram.clear();
  for (let value of [CONN_LENGTH.SHORTER_THAN_10S,
                     CONN_LENGTH.BETWEEN_10S_AND_30S,
                     CONN_LENGTH.BETWEEN_10S_AND_30S,
                     CONN_LENGTH.BETWEEN_30S_AND_5M,
                     CONN_LENGTH.BETWEEN_30S_AND_5M,
                     CONN_LENGTH.BETWEEN_30S_AND_5M,
                     CONN_LENGTH.MORE_THAN_5M,
                     CONN_LENGTH.MORE_THAN_5M,
                     CONN_LENGTH.MORE_THAN_5M,
                     CONN_LENGTH.MORE_THAN_5M]) {
    gMozLoopAPI.telemetryAddKeyedValue(histogramId, value);
  }

  let snapshot = histogram.snapshot();
  is(snapshot["SHORTER_THAN_10S"].sum, 1, "TWO_WAY_MEDIA_CONN_LENGTH.SHORTER_THAN_10S");
  is(snapshot["BETWEEN_10S_AND_30S"].sum, 2, "TWO_WAY_MEDIA_CONN_LENGTH.BETWEEN_10S_AND_30S");
  is(snapshot["BETWEEN_30S_AND_5M"].sum, 3, "TWO_WAY_MEDIA_CONN_LENGTH.BETWEEN_30S_AND_5M");
  is(snapshot["MORE_THAN_5M"].sum, 4, "TWO_WAY_MEDIA_CONN_LENGTH.MORE_THAN_5M");
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
