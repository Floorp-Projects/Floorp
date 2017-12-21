/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint no-eval:0 */

"use strict";

var Cu = Components.utils;
const {require} = Cu.import("resource://devtools/shared/Loader.jsm", {});
const {findOptimalTimeInterval} = require("devtools/client/animationinspector/utils");

// This test array contains objects that are used to test the
// findOptimalTimeInterval function. Each object should have the following
// properties:
// - desc: an optional string that will be printed out
// - minTimeInterval: a number that represents the minimum time in ms
//   that should be displayed in one interval
// - expectedInterval: a number that you expect the findOptimalTimeInterval
//   function to return as a result.
//   Optionally you can pass a string where `interval` is the calculated
//   interval, this string will be eval'd and tested to be truthy.
const TEST_DATA = [{
  desc: "With no minTimeInterval, expect the interval to be 0",
  minTimeInterval: null,
  expectedInterval: 0
}, {
  desc: "With a minTimeInterval of 0 ms, expect the interval to be 0",
  minTimeInterval: 0,
  expectedInterval: 0
}, {
  desc: "With a minInterval of 1ms, expect the interval to be the 1ms too",
  minTimeInterval: 1,
  expectedInterval: 1
}, {
  desc: "With a very small minTimeInterval, expect the interval to be 1ms",
  minTimeInterval: 1e-31,
  expectedInterval: 1
}, {
  desc: "With a minInterval of 2.5ms, expect the interval to be 2.5ms too",
  minTimeInterval: 2.5,
  expectedInterval: 2.5
}, {
  desc: "With a minInterval of 5ms, expect the interval to be 5ms too",
  minTimeInterval: 5,
  expectedInterval: 5
}, {
  desc: "With a minInterval of 7ms, expect the interval to be the next " +
        "multiple of 5",
  minTimeInterval: 7,
  expectedInterval: 10
}, {
  minTimeInterval: 20,
  expectedInterval: 25
}, {
  minTimeInterval: 33,
  expectedInterval: 50
}, {
  minTimeInterval: 987,
  expectedInterval: 1000
}, {
  minTimeInterval: 1234,
  expectedInterval: 2500
}, {
  minTimeInterval: 9800,
  expectedInterval: 10000
}];

function run_test() {
  for (let {minTimeInterval, desc, expectedInterval} of TEST_DATA) {
    info(`Testing minTimeInterval: ${minTimeInterval}.
          Expecting ${expectedInterval}.`);

    let interval = findOptimalTimeInterval(minTimeInterval);
    if (typeof expectedInterval == "string") {
      ok(eval(expectedInterval), desc);
    } else {
      equal(interval, expectedInterval, desc);
    }
  }
}
