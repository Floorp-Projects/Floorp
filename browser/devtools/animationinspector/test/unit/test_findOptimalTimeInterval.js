/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint no-eval:0 */

"use strict";

const Cu = Components.utils;
const {require} = Cu.import("resource://gre/modules/devtools/Loader.jsm", {});
const {findOptimalTimeInterval} = require("devtools/animationinspector/utils");

// This test array contains objects that are used to test the
// findOptimalTimeInterval function. Each object should have the following
// properties:
// - desc: an optional string that will be printed out
// - timeScale: a number that represents how many pixels is 1ms
// - minSpacing: an optional number that represents the minim space between 2
//   time graduations
// - expectedInterval: a number that you expect the findOptimalTimeInterval
//   function to return as a result.
//   Optionally you can pass a string where `interval` is the calculated
//   interval, this string will be eval'd and tested to be truthy.
const TEST_DATA = [{
  desc: "With 1px being 1ms and no minSpacing, expect the interval to be the " +
        "interval multiple",
  timeScale: 1,
  minSpacing: undefined,
  expectedInterval: 25
}, {
  desc: "With 1px being 1ms and a custom minSpacing being a multiple of 25 " +
        "expect the interval to be the custom min spacing",
  timeScale: 1,
  minSpacing: 50,
  expectedInterval: 50
}, {
  desc: "With 1px being 1ms and a custom minSpacing not being multiple of 25 " +
        "expect the interval to be the next multiple of 10",
  timeScale: 1,
  minSpacing: 26,
  expectedInterval: 50
}, {
  desc: "If 1ms corresponds to a distance that is greater than the min " +
        "spacing then, expect the interval to be this distance",
  timeScale: 20,
  minSpacing: undefined,
  expectedInterval: 20
}, {
  desc: "If 1ms corresponds to a distance that is greater than the min " +
        "spacing then, expect the interval to be this distance, even if it " +
        "isn't a multiple of 25",
  timeScale: 33,
  minSpacing: undefined,
  expectedInterval: 33
}, {
  desc: "If 1ms is a very small distance, then expect this distance to be " +
        "multiplied by 25, 50, 100, 200, etc... until it goes over the min " +
        "spacing",
  timeScale: 0.001,
  minSpacing: undefined,
  expectedInterval: 12.8
}, {
  desc: "If the time scale is such that we need to iterate more than the " +
        "maximum allowed number of iterations, then expect an interval lower " +
        "than the minimum one",
  timeScale: 1e-31,
  minSpacing: undefined,
  expectedInterval: "interval < 10"
}];

function run_test() {
  for (let {timeScale, desc, minSpacing, expectedInterval} of TEST_DATA) {
    do_print("Testing timeScale: " + timeScale + " and minSpacing: " +
              minSpacing + ". Expecting " + expectedInterval + ".");

    let interval = findOptimalTimeInterval(timeScale, minSpacing);
    if (typeof expectedInterval == "string") {
      ok(eval(expectedInterval), desc);
    } else {
      equal(interval, expectedInterval, desc);
    }
  }
}
