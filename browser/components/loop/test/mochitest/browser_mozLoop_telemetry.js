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
  let histogram = Services.telemetry.getHistogramById(histogramId);
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
    gMozLoopAPI.telemetryAddValue(histogramId, value);
  }

  let snapshot = histogram.snapshot();
  is(snapshot.counts[CONN_LENGTH.SHORTER_THAN_10S], 1, "TWO_WAY_MEDIA_CONN_LENGTH.SHORTER_THAN_10S");
  is(snapshot.counts[CONN_LENGTH.BETWEEN_10S_AND_30S], 2, "TWO_WAY_MEDIA_CONN_LENGTH.BETWEEN_10S_AND_30S");
  is(snapshot.counts[CONN_LENGTH.BETWEEN_30S_AND_5M], 3, "TWO_WAY_MEDIA_CONN_LENGTH.BETWEEN_30S_AND_5M");
  is(snapshot.counts[CONN_LENGTH.MORE_THAN_5M], 4, "TWO_WAY_MEDIA_CONN_LENGTH.MORE_THAN_5M");
});

add_task(function* test_mozLoop_telemetryAdd_sharing_buckets() {
  let histogramId = "LOOP_SHARING_STATE_CHANGE";
  let histogram = Services.telemetry.getHistogramById(histogramId);
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
    gMozLoopAPI.telemetryAddValue(histogramId, value);
  }

  let snapshot = histogram.snapshot();
  Assert.strictEqual(snapshot.counts[SHARING_STATES.WINDOW_ENABLED], 1, "SHARING_STATE_CHANGE.WINDOW_ENABLED");
  Assert.strictEqual(snapshot.counts[SHARING_STATES.WINDOW_DISABLED], 2, "SHARING_STATE_CHANGE.WINDOW_DISABLED");
  Assert.strictEqual(snapshot.counts[SHARING_STATES.BROWSER_ENABLED], 3, "SHARING_STATE_CHANGE.BROWSER_ENABLED");
  Assert.strictEqual(snapshot.counts[SHARING_STATES.BROWSER_DISABLED], 4, "SHARING_STATE_CHANGE.BROWSER_DISABLED");
});
