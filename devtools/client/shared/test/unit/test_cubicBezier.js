/* -*- Mode: Javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests the CubicBezier API in the CubicBezierWidget module

var Cu = Components.utils;
var {require} = Cu.import("resource://devtools/shared/Loader.jsm", {});
var {CubicBezier, parseTimingFunction} = require("devtools/client/shared/widgets/CubicBezierWidget");

function run_test() {
  throwsWhenMissingCoordinates();
  throwsWhenIncorrectCoordinates();
  convertsStringCoordinates();
  coordinatesToStringOutputsAString();
  pointGettersReturnPointCoordinatesArrays();
  toStringOutputsCubicBezierValue();
  toStringOutputsCssPresetValues();
  testParseTimingFunction();
}

function throwsWhenMissingCoordinates() {
  do_check_throws(() => {
    new CubicBezier();
  }, "Throws an exception when coordinates are missing");
}

function throwsWhenIncorrectCoordinates() {
  do_check_throws(() => {
    new CubicBezier([]);
  }, "Throws an exception when coordinates are incorrect (empty array)");

  do_check_throws(() => {
    new CubicBezier([0, 0]);
  }, "Throws an exception when coordinates are incorrect (incomplete array)");

  do_check_throws(() => {
    new CubicBezier(["a", "b", "c", "d"]);
  }, "Throws an exception when coordinates are incorrect (invalid type)");

  do_check_throws(() => {
    new CubicBezier([1.5, 0, 1.5, 0]);
  }, "Throws an exception when coordinates are incorrect (time range invalid)");

  do_check_throws(() => {
    new CubicBezier([-0.5, 0, -0.5, 0]);
  }, "Throws an exception when coordinates are incorrect (time range invalid)");
}

function convertsStringCoordinates() {
  do_print("Converts string coordinates to numbers");
  let c = new CubicBezier(["0", "1", ".5", "-2"]);

  Assert.equal(c.coordinates[0], 0);
  Assert.equal(c.coordinates[1], 1);
  Assert.equal(c.coordinates[2], .5);
  Assert.equal(c.coordinates[3], -2);
}

function coordinatesToStringOutputsAString() {
  do_print("coordinates.toString() outputs a string representation");

  let c = new CubicBezier(["0", "1", "0.5", "-2"]);
  let string = c.coordinates.toString();
  Assert.equal(string, "0,1,.5,-2");

  c = new CubicBezier([1, 1, 1, 1]);
  string = c.coordinates.toString();
  Assert.equal(string, "1,1,1,1");
}

function pointGettersReturnPointCoordinatesArrays() {
  do_print("Points getters return arrays of coordinates");

  let c = new CubicBezier([0, .2, .5, 1]);
  Assert.equal(c.P1[0], 0);
  Assert.equal(c.P1[1], .2);
  Assert.equal(c.P2[0], .5);
  Assert.equal(c.P2[1], 1);
}

function toStringOutputsCubicBezierValue() {
  do_print("toString() outputs the cubic-bezier() value");

  let c = new CubicBezier([0, 1, 1, 0]);
  Assert.equal(c.toString(), "cubic-bezier(0,1,1,0)");
}

function toStringOutputsCssPresetValues() {
  do_print("toString() outputs the css predefined values");

  let c = new CubicBezier([0, 0, 1, 1]);
  Assert.equal(c.toString(), "linear");

  c = new CubicBezier([0.25, 0.1, 0.25, 1]);
  Assert.equal(c.toString(), "ease");

  c = new CubicBezier([0.42, 0, 1, 1]);
  Assert.equal(c.toString(), "ease-in");

  c = new CubicBezier([0, 0, 0.58, 1]);
  Assert.equal(c.toString(), "ease-out");

  c = new CubicBezier([0.42, 0, 0.58, 1]);
  Assert.equal(c.toString(), "ease-in-out");
}

function testParseTimingFunction() {
  do_print("test parseTimingFunction");

  for (let test of ["ease", "linear", "ease-in", "ease-out", "ease-in-out"]) {
    ok(parseTimingFunction(test), test);
  }

  ok(!parseTimingFunction("something"), "non-function token");
  ok(!parseTimingFunction("something()"), "non-cubic-bezier function");
  ok(!parseTimingFunction("cubic-bezier(something)",
                           "cubic-bezier with non-numeric argument"));
  ok(!parseTimingFunction("cubic-bezier(1,2,3:7)",
                           "did not see comma"));
  ok(!parseTimingFunction("cubic-bezier(1,2,3,7:",
                          "did not see close paren"));
  ok(!parseTimingFunction("cubic-bezier(1,2", "early EOF after number"));
  ok(!parseTimingFunction("cubic-bezier(1,2,", "early EOF after comma"));
  deepEqual(parseTimingFunction("cubic-bezier(1,2,3,7)"), [1, 2, 3, 7],
            "correct invocation");
  deepEqual(parseTimingFunction("cubic-bezier(1,  /* */ 2,3,   7  )"),
            [1, 2, 3, 7],
            "correct with comments and whitespace");
}

function do_check_throws(cb, info) {
  do_print(info);

  let hasThrown = false;
  try {
    cb();
  } catch (e) {
    hasThrown = true;
  }

  Assert.ok(hasThrown);
}
