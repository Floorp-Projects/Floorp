/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * This file contains tests for the mozLoop telemetry API.
 */

"use strict";

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
  let histogramId = "LOOP_TWO_WAY_MEDIA_CONN_LENGTH_1";
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
  let histogramId = "LOOP_SHARING_STATE_CHANGE_1";
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

add_task(function* test_mozLoop_telemetryAdd_sharingURL_buckets() {
  let histogramId = "LOOP_SHARING_ROOM_URL";
  let histogram = Services.telemetry.getHistogramById(histogramId);
  const SHARING_TYPES = gMozLoopAPI.SHARING_ROOM_URL;

  histogram.clear();
  for (let value of [SHARING_TYPES.COPY_FROM_PANEL,
                     SHARING_TYPES.COPY_FROM_CONVERSATION,
                     SHARING_TYPES.COPY_FROM_CONVERSATION,
                     SHARING_TYPES.EMAIL_FROM_CALLFAILED,
                     SHARING_TYPES.EMAIL_FROM_CALLFAILED,
                     SHARING_TYPES.EMAIL_FROM_CALLFAILED,
                     SHARING_TYPES.EMAIL_FROM_CONVERSATION,
                     SHARING_TYPES.EMAIL_FROM_CONVERSATION,
                     SHARING_TYPES.EMAIL_FROM_CONVERSATION,
                     SHARING_TYPES.EMAIL_FROM_CONVERSATION]) {
    gMozLoopAPI.telemetryAddValue(histogramId, value);
  }

  let snapshot = histogram.snapshot();
  Assert.strictEqual(snapshot.counts[SHARING_TYPES.COPY_FROM_PANEL], 1,
    "SHARING_ROOM_URL.COPY_FROM_PANEL");
  Assert.strictEqual(snapshot.counts[SHARING_TYPES.COPY_FROM_CONVERSATION], 2,
    "SHARING_ROOM_URL.COPY_FROM_CONVERSATION");
  Assert.strictEqual(snapshot.counts[SHARING_TYPES.EMAIL_FROM_CALLFAILED], 3,
    "SHARING_ROOM_URL.EMAIL_FROM_CALLFAILED");
  Assert.strictEqual(snapshot.counts[SHARING_TYPES.EMAIL_FROM_CONVERSATION], 4,
    "SHARING_ROOM_URL.EMAIL_FROM_CONVERSATION");
});

add_task(function* test_mozLoop_telemetryAdd_roomCreate_buckets() {
  let histogramId = "LOOP_ROOM_CREATE";
  let histogram = Services.telemetry.getHistogramById(histogramId);
  const ACTION_TYPES = gMozLoopAPI.ROOM_CREATE;

  histogram.clear();
  for (let value of [ACTION_TYPES.CREATE_SUCCESS,
                     ACTION_TYPES.CREATE_FAIL,
                     ACTION_TYPES.CREATE_FAIL]) {
    gMozLoopAPI.telemetryAddValue(histogramId, value);
  }

  let snapshot = histogram.snapshot();
  Assert.strictEqual(snapshot.counts[ACTION_TYPES.CREATE_SUCCESS], 1,
    "SHARING_ROOM_URL.CREATE_SUCCESS");
  Assert.strictEqual(snapshot.counts[ACTION_TYPES.CREATE_FAIL], 2,
    "SHARING_ROOM_URL.CREATE_FAIL");
});

add_task(function* test_mozLoop_telemetryAdd_roomDelete_buckets() {
  let histogramId = "LOOP_ROOM_DELETE";
  let histogram = Services.telemetry.getHistogramById(histogramId);
  const ACTION_TYPES = gMozLoopAPI.ROOM_DELETE;

  histogram.clear();
  for (let value of [ACTION_TYPES.DELETE_SUCCESS,
                     ACTION_TYPES.DELETE_FAIL,
                     ACTION_TYPES.DELETE_FAIL]) {
    gMozLoopAPI.telemetryAddValue(histogramId, value);
  }

  let snapshot = histogram.snapshot();
  Assert.strictEqual(snapshot.counts[ACTION_TYPES.DELETE_SUCCESS], 1,
    "SHARING_ROOM_URL.DELETE_SUCCESS");
  Assert.strictEqual(snapshot.counts[ACTION_TYPES.DELETE_FAIL], 2,
    "SHARING_ROOM_URL.DELETE_FAIL");
});

add_task(function* test_mozLoop_telemetryAdd_roomContextAdd_buckets() {
  let histogramId = "LOOP_ROOM_CONTEXT_ADD";
  let histogram = Services.telemetry.getHistogramById(histogramId);
  const ACTION_TYPES = gMozLoopAPI.ROOM_CONTEXT_ADD;

  histogram.clear();
  for (let value of [ACTION_TYPES.ADD_FROM_PANEL,
                     ACTION_TYPES.ADD_FROM_CONVERSATION,
                     ACTION_TYPES.ADD_FROM_CONVERSATION]) {
    gMozLoopAPI.telemetryAddValue(histogramId, value);
  }

  let snapshot = histogram.snapshot();
  Assert.strictEqual(snapshot.counts[ACTION_TYPES.ADD_FROM_PANEL], 1,
    "SHARING_ROOM_URL.CREATE_SUCCESS");
  Assert.strictEqual(snapshot.counts[ACTION_TYPES.ADD_FROM_CONVERSATION], 2,
    "SHARING_ROOM_URL.ADD_FROM_CONVERSATION");
});

add_task(function* test_mozLoop_telemetryAdd_roomContextClick() {
  let histogramId = "LOOP_ROOM_CONTEXT_CLICK";
  let histogram = Services.telemetry.getHistogramById(histogramId);

  histogram.clear();

  let snapshot;
  for (let i = 1; i < 4; ++i) {
    gMozLoopAPI.telemetryAddValue(histogramId, 1);
    snapshot = histogram.snapshot();
    Assert.strictEqual(snapshot.counts[0], i);
  }
});

add_task(function* test_mozLoop_telemetryAdd_roomSessionWithChat() {
  let histogramId = "LOOP_ROOM_SESSION_WITHCHAT";
  let histogram = Services.telemetry.getHistogramById(histogramId);

  histogram.clear();

  let snapshot;
  for (let i = 1; i < 4; ++i) {
    gMozLoopAPI.telemetryAddValue(histogramId, 1);
    snapshot = histogram.snapshot();
    Assert.strictEqual(snapshot.counts[0], i);
  }
});
