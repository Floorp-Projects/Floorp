/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * This file contains tests for the mozLoop telemetry API.
 */

"use strict";

var [, gHandlers] = LoopAPI.inspect();
var gConstants;
gHandlers.GetAllConstants({}, constants => {
  gConstants = constants;
});

function resetMauPrefs() {
  Services.prefs.clearUserPref("loop.mau.openPanel");
  Services.prefs.clearUserPref("loop.mau.openConversation");
  Services.prefs.clearUserPref("loop.mau.roomOpen");
  Services.prefs.clearUserPref("loop.mau.roomShare");
  Services.prefs.clearUserPref("loop.mau.roomDelete");
}

/**
 * Enable local telemetry recording for the duration of the tests.
 */
add_task(function* test_initialize() {
  let oldCanRecord = Services.telemetry.canRecordExtended;
  Services.telemetry.canRecordExtended = true;

  // Ensure prefs are their default values.
  resetMauPrefs();

  registerCleanupFunction(function() {
    // Clean up the prefs after use to be nice.
    resetMauPrefs();
    Services.telemetry.canRecordExtended = oldCanRecord;
  });
});

add_task(function* test_mozLoop_telemetryAdd_roomCreate_buckets() {
  let histogramId = "LOOP_ROOM_CREATE";
  let histogram = Services.telemetry.getHistogramById(histogramId);
  const ACTION_TYPES = gConstants.ROOM_CREATE;

  histogram.clear();
  for (let value of [ACTION_TYPES.CREATE_SUCCESS,
                     ACTION_TYPES.CREATE_FAIL,
                     ACTION_TYPES.CREATE_FAIL]) {
    gHandlers.TelemetryAddValue({ data: [histogramId, value] }, () => {});
  }

  let snapshot = histogram.snapshot();
  Assert.strictEqual(snapshot.counts[ACTION_TYPES.CREATE_SUCCESS], 1,
    "SHARING_ROOM_URL.CREATE_SUCCESS");
  Assert.strictEqual(snapshot.counts[ACTION_TYPES.CREATE_FAIL], 2,
    "SHARING_ROOM_URL.CREATE_FAIL");
});

add_task(function* test_mozLoop_telemetryAdd_loopMauType_buckets() {
  let histogramId = "LOOP_ACTIVITY_COUNTER";
  let histogram = Services.telemetry.getHistogramById(histogramId);
  const ACTION_TYPES = gConstants.LOOP_MAU_TYPE;

  histogram.clear();

  for (let value of [ACTION_TYPES.OPEN_PANEL,
                     ACTION_TYPES.OPEN_CONVERSATION,
                     ACTION_TYPES.ROOM_OPEN,
                     ACTION_TYPES.ROOM_SHARE,
                     ACTION_TYPES.ROOM_DELETE]) {
    gHandlers.TelemetryAddValue({ data: [histogramId, value] }, () => {});
  }

  let snapshot = histogram.snapshot();
  Assert.strictEqual(snapshot.counts[ACTION_TYPES.OPEN_PANEL], 1,
    "LOOP_MAU_TYPE.OPEN_PANEL");
  Assert.strictEqual(snapshot.counts[ACTION_TYPES.OPEN_CONVERSATION], 1,
    "LOOP_MAU_TYPE.OPEN_CONVERSATION");
  Assert.strictEqual(snapshot.counts[ACTION_TYPES.ROOM_OPEN], 1,
    "LOOP_MAU_TYPE.ROOM_OPEN");
  Assert.strictEqual(snapshot.counts[ACTION_TYPES.ROOM_SHARE], 1,
    "LOOP_MAU_TYPE.ROOM_SHARE");
  Assert.strictEqual(snapshot.counts[ACTION_TYPES.ROOM_DELETE], 1,
    "LOOP_MAU_TYPE.ROOM_DELETE");

  // Reset the prefs here, so we don't affect other tests in this file.
  resetMauPrefs();
});

/**
 * Tests that only one event is sent every 30 days
 */
add_task(function* test_mozLoop_telemetryAdd_loopMau_more_than_30_days() {
  let histogramId = "LOOP_ACTIVITY_COUNTER";
  let histogram = Services.telemetry.getHistogramById(histogramId);
  const ACTION_TYPES = gConstants.LOOP_MAU_TYPE;

  histogram.clear();
  gHandlers.TelemetryAddValue({ data: [histogramId, ACTION_TYPES.OPEN_PANEL] }, () => {});

  let snapshot = histogram.snapshot();
  Assert.strictEqual(snapshot.counts[ACTION_TYPES.OPEN_PANEL], 1,
    "LOOP_MAU_TYPE.OPEN_PANEL");

  // Let's be sure that the last event was sent a month ago
  let timestamp = (Math.floor(Date.now() / 1000)) - 2593000;
  Services.prefs.setIntPref("loop.mau.openPanel", timestamp);

  gHandlers.TelemetryAddValue({ data: [histogramId, ACTION_TYPES.OPEN_PANEL] }, () => {});
  snapshot = histogram.snapshot();
  Assert.strictEqual(snapshot.counts[ACTION_TYPES.OPEN_PANEL], 2,
    "LOOP_MAU_TYPE.OPEN_PANEL");

  // Reset the prefs here, so we don't affect other tests in this file.
  resetMauPrefs();
});

add_task(function* test_mozLoop_telemetryAdd_loopMau_less_than_30_days() {
  let histogramId = "LOOP_ACTIVITY_COUNTER";
  let histogram = Services.telemetry.getHistogramById(histogramId);
  const ACTION_TYPES = gConstants.LOOP_MAU_TYPE;

  histogram.clear();
  gHandlers.TelemetryAddValue({ data: [histogramId, ACTION_TYPES.OPEN_PANEL] }, () => {});

  let snapshot = histogram.snapshot();
  Assert.strictEqual(snapshot.counts[ACTION_TYPES.OPEN_PANEL], 1,
    "LOOP_MAU_TYPE.OPEN_PANEL");

  let timestamp = (Math.floor(Date.now() / 1000)) - 1000;
  Services.prefs.setIntPref("loop.mau.openPanel", timestamp);

  gHandlers.TelemetryAddValue({ data: [histogramId, ACTION_TYPES.OPEN_PANEL] }, () => {});
  snapshot = histogram.snapshot();
  Assert.strictEqual(snapshot.counts[ACTION_TYPES.OPEN_PANEL], 1,
    "LOOP_MAU_TYPE.OPEN_PANEL");

  // Reset the prefs here, so we don't affect other tests in this file.
  resetMauPrefs();
});
