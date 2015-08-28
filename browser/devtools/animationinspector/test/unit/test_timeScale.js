/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const Cu = Components.utils;
const {require} = Cu.import("resource://gre/modules/devtools/Loader.jsm", {});
const {TimeScale} = require("devtools/animationinspector/components");

const TEST_ANIMATIONS = [{
  startTime: 500,
  delay: 0,
  duration: 1000,
  iterationCount: 1,
  playbackRate: 1
}, {
  startTime: 400,
  delay: 100,
  duration: 10,
  iterationCount: 100,
  playbackRate: 1
}, {
  startTime: 50,
  delay: 1000,
  duration: 100,
  iterationCount: 20,
  playbackRate: 1
}];
const EXPECTED_MIN_START = 50;
const EXPECTED_MAX_END = 3050;

const TEST_STARTTIME_TO_DISTANCE = [{
  time: 50,
  width: 100,
  expectedDistance: 0
}, {
  time: 50,
  width: 0,
  expectedDistance: 0
}, {
  time: 3050,
  width: 200,
  expectedDistance: 200
}, {
  time: 1550,
  width: 200,
  expectedDistance: 100
}];

const TEST_DURATION_TO_DISTANCE = [{
  time: 3000,
  width: 100,
  expectedDistance: 100
}, {
  time: 0,
  width: 100,
  expectedDistance: 0
}];

const TEST_DISTANCE_TO_TIME = [{
  distance: 100,
  width: 100,
  expectedTime: 3050
}, {
  distance: 0,
  width: 100,
  expectedTime: 50
}, {
  distance: 25,
  width: 200,
  expectedTime: 425
}];

const TEST_DISTANCE_TO_RELATIVE_TIME = [{
  distance: 100,
  width: 100,
  expectedTime: 3000
}, {
  distance: 0,
  width: 100,
  expectedTime: 0
}, {
  distance: 25,
  width: 200,
  expectedTime: 375
}];

const TEST_FORMAT_TIME_MS = [{
  time: 0,
  expectedFormattedTime: "0ms"
}, {
  time: 3540.341,
  expectedFormattedTime: "3540ms"
}, {
  time: 1.99,
  expectedFormattedTime: "2ms"
}, {
  time: 4000,
  expectedFormattedTime: "4000ms"
}];

const TEST_FORMAT_TIME_S = [{
  time: 0,
  expectedFormattedTime: "0.0s"
}, {
  time: 3540.341,
  expectedFormattedTime: "3.5s"
}, {
  time: 1.99,
  expectedFormattedTime: "0.0s"
}, {
  time: 4000,
  expectedFormattedTime: "4.0s"
}, {
  time: 102540,
  expectedFormattedTime: "102.5s"
}, {
  time: 102940,
  expectedFormattedTime: "102.9s"
}];

function run_test() {
  do_print("Check the default min/max range values");
  equal(TimeScale.minStartTime, Infinity);
  equal(TimeScale.maxEndTime, 0);

  do_print("Test adding a few animations");
  for (let state of TEST_ANIMATIONS) {
    TimeScale.addAnimation(state);
  }
  equal(TimeScale.minStartTime, EXPECTED_MIN_START);
  equal(TimeScale.maxEndTime, EXPECTED_MAX_END);

  do_print("Test reseting the animations");
  TimeScale.reset();
  equal(TimeScale.minStartTime, Infinity);
  equal(TimeScale.maxEndTime, 0);

  do_print("Test adding the animations again");
  for (let state of TEST_ANIMATIONS) {
    TimeScale.addAnimation(state);
  }
  equal(TimeScale.minStartTime, EXPECTED_MIN_START);
  equal(TimeScale.maxEndTime, EXPECTED_MAX_END);

  do_print("Test converting start times to distances");
  for (let {time, width, expectedDistance} of TEST_STARTTIME_TO_DISTANCE) {
    let distance = TimeScale.startTimeToDistance(time, width);
    equal(distance, expectedDistance);
  }

  do_print("Test converting durations to distances");
  for (let {time, width, expectedDistance} of TEST_DURATION_TO_DISTANCE) {
    let distance = TimeScale.durationToDistance(time, width);
    equal(distance, expectedDistance);
  }

  do_print("Test converting distances to times");
  for (let {distance, width, expectedTime} of TEST_DISTANCE_TO_TIME) {
    let time = TimeScale.distanceToTime(distance, width);
    equal(time, expectedTime);
  }

  do_print("Test converting distances to relative times");
  for (let {distance, width, expectedTime} of TEST_DISTANCE_TO_RELATIVE_TIME) {
    let time = TimeScale.distanceToRelativeTime(distance, width);
    equal(time, expectedTime);
  }

  do_print("Test formatting times (millis)");
  for (let {time, expectedFormattedTime} of TEST_FORMAT_TIME_MS) {
    let formattedTime = TimeScale.formatTime(time);
    equal(formattedTime, expectedFormattedTime);
  }

  // Add 1 more animation to increase the range and test more time formatting
  // cases.
  TimeScale.addAnimation({
    startTime: 3000,
    duration: 5000,
    delay: 0,
    iterationCount: 1
  });

  do_print("Test formatting times (seconds)");
  for (let {time, expectedFormattedTime} of TEST_FORMAT_TIME_S) {
    let formattedTime = TimeScale.formatTime(time);
    equal(formattedTime, expectedFormattedTime);
  }
}
