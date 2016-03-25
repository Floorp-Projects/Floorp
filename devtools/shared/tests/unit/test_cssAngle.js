/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test classifyAngle.

"use strict";

const {angleUtils} = require("devtools/shared/css-angle");

const CLASSIFY_TESTS = [
  { input: "180deg", output: "deg" },
  { input: "-180deg", output: "deg" },
  { input: "180DEG", output: "deg" },
  { input: "200rad", output: "rad" },
  { input: "-200rad", output: "rad" },
  { input: "200RAD", output: "rad" },
  { input: "0.5grad", output: "grad" },
  { input: "-0.5grad", output: "grad" },
  { input: "0.5GRAD", output: "grad" },
  { input: "0.33turn", output: "turn" },
  { input: "0.33TURN", output: "turn" },
  { input: "-0.33turn", output: "turn" }
];

function run_test() {
  for (let test of CLASSIFY_TESTS) {
    let result = angleUtils.classifyAngle(test.input);
    equal(result, test.output, "test classifyAngle(" + test.input + ")");
  }
}
