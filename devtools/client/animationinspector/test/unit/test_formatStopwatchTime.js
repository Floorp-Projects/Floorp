/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var Cu = Components.utils;
const {require} = Cu.import("resource://devtools/shared/Loader.jsm", {});
const {formatStopwatchTime} = require("devtools/client/animationinspector/utils");

const TEST_DATA = [{
  desc: "Formatting 0",
  time: 0,
  expected: "00:00.000"
}, {
  desc: "Formatting null",
  time: null,
  expected: "00:00.000"
}, {
  desc: "Formatting undefined",
  time: undefined,
  expected: "00:00.000"
}, {
  desc: "Formatting a small number of ms",
  time: 13,
  expected: "00:00.013"
}, {
  desc: "Formatting a slightly larger number of ms",
  time: 500,
  expected: "00:00.500"
}, {
  desc: "Formatting 1 second",
  time: 1000,
  expected: "00:01.000"
}, {
  desc: "Formatting a number of seconds",
  time: 1532,
  expected: "00:01.532"
}, {
  desc: "Formatting a big number of seconds",
  time: 58450,
  expected: "00:58.450"
}, {
  desc: "Formatting 1 minute",
  time: 60000,
  expected: "01:00.000"
}, {
  desc: "Formatting a number of minutes",
  time: 263567,
  expected: "04:23.567"
}, {
  desc: "Formatting a large number of minutes",
  time: 1000 * 60 * 60 * 3,
  expected: "180:00.000"
}];

function run_test() {
  for (let {desc, time, expected} of TEST_DATA) {
    equal(formatStopwatchTime(time), expected, desc);
  }
}
