/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test the helper functions of the shapes highlighter.
 */

"use strict";

const {
  splitCoords,
  coordToPercent,
  evalCalcExpression,
  shapeModeToCssPropertyName,
  getCirclePath
} = require("devtools/server/actors/highlighters/shapes");

function run_test() {
  test_split_coords();
  test_coord_to_percent();
  test_eval_calc_expression();
  test_shape_mode_to_css_property_name();
  test_get_circle_path();
  run_next_test();
}

function test_split_coords() {
  const tests = [{
    desc: "splitCoords for basic coordinate pair",
    expr: "30% 20%",
    expected: ["30%", "20%"]
  }, {
    desc: "splitCoords for coord pair with calc()",
    expr: "calc(50px + 20%) 30%",
    expected: ["calc(50px+20%)", "30%"]
  }];

  for (let { desc, expr, expected } of tests) {
    deepEqual(splitCoords(expr), expected, desc);
  }
}

function test_coord_to_percent() {
  const size = 1000;
  const tests = [{
    desc: "coordToPercent for percent value",
    expr: "50%",
    expected: 50
  }, {
    desc: "coordToPercent for px value",
    expr: "500px",
    expected: 50
  }, {
    desc: "coordToPercent for zero value",
    expr: "0",
    expected: 0
  }];

  for (let { desc, expr, expected } of tests) {
    equal(coordToPercent(expr, size), expected, desc);
  }
}

function test_eval_calc_expression() {
  const size = 1000;
  const tests = [{
    desc: "evalCalcExpression with one value",
    expr: "50%",
    expected: 50
  }, {
    desc: "evalCalcExpression with percent and px values",
    expr: "50% + 100px",
    expected: 60
  }, {
    desc: "evalCalcExpression with a zero value",
    expr: "0 + 100px",
    expected: 10
  }, {
    desc: "evalCalcExpression with a negative value",
    expr: "-200px+50%",
    expected: 30
  }];

  for (let { desc, expr, expected } of tests) {
    equal(evalCalcExpression(expr, size), expected, desc);
  }
}

function test_shape_mode_to_css_property_name() {
  const tests = [{
    desc: "shapeModeToCssPropertyName for clip-path",
    expr: "cssClipPath",
    expected: "clipPath"
  }, {
    desc: "shapeModeToCssPropertyName for shape-outside",
    expr: "cssShapeOutside",
    expected: "shapeOutside"
  }];

  for (let { desc, expr, expected } of tests) {
    equal(shapeModeToCssPropertyName(expr), expected, desc);
  }
}

function test_get_circle_path() {
  const tests = [{
    desc: "getCirclePath with no resizing, no zoom, 1:1 ratio",
    cx: 0, cy: 0, width: 100, height: 100, zoom: 1,
    expected: "M-10,0a10,10 0 1,0 20,0a10,10 0 1,0 -20,0"
  }, {
    desc: "getCirclePath with resizing, no zoom, 1:1 ratio",
    cx: 0, cy: 0, width: 200, height: 200, zoom: 1,
    expected: "M-5,0a5,5 0 1,0 10,0a5,5 0 1,0 -10,0"
  }, {
    desc: "getCirclePath with resizing, zoom, 1:1 ratio",
    cx: 0, cy: 0, width: 200, height: 200, zoom: 2,
    expected: "M-2.5,0a2.5,2.5 0 1,0 5,0a2.5,2.5 0 1,0 -5,0"
  }, {
    desc: "getCirclePath with resizing, zoom, non-square ratio",
    cx: 0, cy: 0, width: 100, height: 200, zoom: 2,
    expected: "M-5,0a5,2.5 0 1,0 10,0a5,2.5 0 1,0 -10,0"
  }];

  for (let { desc, cx, cy, width, height, zoom, expected } of tests) {
    equal(getCirclePath(cx, cy, width, height, zoom), expected, desc);
  }
}
