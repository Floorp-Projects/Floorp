/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests the CubicBezier API in the CubicBezierWidget module

var { require } = ChromeUtils.import("resource://devtools/shared/Loader.jsm");
var {
  CubicBezier,
  parseTimingFunction,
} = require("devtools/client/shared/widgets/CubicBezierWidget");

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
  info("Converts string coordinates to numbers");
  const c = new CubicBezier(["0", "1", ".5", "-2"]);

  Assert.equal(c.coordinates[0], 0);
  Assert.equal(c.coordinates[1], 1);
  Assert.equal(c.coordinates[2], 0.5);
  Assert.equal(c.coordinates[3], -2);
}

function coordinatesToStringOutputsAString() {
  info("coordinates.toString() outputs a string representation");

  let c = new CubicBezier(["0", "1", "0.5", "-2"]);
  let string = c.coordinates.toString();
  Assert.equal(string, "0,1,.5,-2");

  c = new CubicBezier([1, 1, 1, 1]);
  string = c.coordinates.toString();
  Assert.equal(string, "1,1,1,1");
}

function pointGettersReturnPointCoordinatesArrays() {
  info("Points getters return arrays of coordinates");

  const c = new CubicBezier([0, 0.2, 0.5, 1]);
  Assert.equal(c.P1[0], 0);
  Assert.equal(c.P1[1], 0.2);
  Assert.equal(c.P2[0], 0.5);
  Assert.equal(c.P2[1], 1);
}

function toStringOutputsCubicBezierValue() {
  info("toString() outputs the cubic-bezier() value");

  const c = new CubicBezier([0, 1, 1, 0]);
  Assert.equal(c.toString(), "cubic-bezier(0,1,1,0)");
}

function toStringOutputsCssPresetValues() {
  info("toString() outputs the css predefined values");

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
  info("test parseTimingFunction");

  for (const test of ["ease", "linear", "ease-in", "ease-out", "ease-in-out"]) {
    ok(parseTimingFunction(test), test);
  }

  ok(!parseTimingFunction("something"), "non-function token");
  ok(!parseTimingFunction("something()"), "non-cubic-bezier function");
  ok(
    !parseTimingFunction(
      "cubic-bezier(something)",
      "cubic-bezier with non-numeric argument"
    )
  );
  ok(!parseTimingFunction("cubic-bezier(1,2,3:7)", "did not see comma"));
  ok(!parseTimingFunction("cubic-bezier(1,2,3,7:", "did not see close paren"));
  ok(!parseTimingFunction("cubic-bezier(1,2", "early EOF after number"));
  ok(!parseTimingFunction("cubic-bezier(1,2,", "early EOF after comma"));
  deepEqual(
    parseTimingFunction("cubic-bezier(1,2,3,7)"),
    [1, 2, 3, 7],
    "correct invocation"
  );
  deepEqual(
    parseTimingFunction("cubic-bezier(1,  /* */ 2,3,   7  )"),
    [1, 2, 3, 7],
    "correct with comments and whitespace"
  );
}

function do_check_throws(cb, details) {
  info(details);

  let hasThrown = false;
  try {
    cb();
  } catch (e) {
    hasThrown = true;
  }

  Assert.ok(hasThrown);
}
