/* -*- Mode: Javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests the CubicBezier API in the CubicBezierWidget module

const Cu = Components.utils;
let {devtools} = Cu.import("resource://gre/modules/devtools/Loader.jsm", {});
let require = devtools.require;
let {CubicBezier} = require("devtools/shared/widgets/CubicBezierWidget");

function run_test() {
  throwsWhenMissingCoordinates();
  throwsWhenIncorrectCoordinates();
  convertsStringCoordinates();
  coordinatesToStringOutputsAString();
  pointGettersReturnPointCoordinatesArrays();
  toStringOutputsCubicBezierValue();
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
    new CubicBezier([0,0]);
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

  do_check_eq(c.coordinates[0], 0);
  do_check_eq(c.coordinates[1], 1);
  do_check_eq(c.coordinates[2], .5);
  do_check_eq(c.coordinates[3], -2);
}

function coordinatesToStringOutputsAString() {
  do_print("coordinates.toString() outputs a string representation");

  let c = new CubicBezier(["0", "1", "0.5", "-2"]);
  let string = c.coordinates.toString();
  do_check_eq(string, "0,1,.5,-2");

  let c = new CubicBezier([1, 1, 1, 1]);
  let string = c.coordinates.toString();
  do_check_eq(string, "1,1,1,1");
}

function pointGettersReturnPointCoordinatesArrays() {
  do_print("Points getters return arrays of coordinates");

  let c = new CubicBezier([0, .2, .5, 1]);
  do_check_eq(c.P1[0], 0);
  do_check_eq(c.P1[1], .2);
  do_check_eq(c.P2[0], .5);
  do_check_eq(c.P2[1], 1);
}

function toStringOutputsCubicBezierValue() {
  do_print("toString() outputs the cubic-bezier() value");

  let c = new CubicBezier([0, 0, 1, 1]);
  do_check_eq(c.toString(), "cubic-bezier(0,0,1,1)");
}

function do_check_throws(cb, info) {
  do_print(info);

  let hasThrown = false;
  try {
    cb();
  } catch (e) {
    hasThrown = true;
  }

  do_check_true(hasThrown);
}
