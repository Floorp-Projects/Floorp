/* -*- Mode: Javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests the BezierCanvas API in the CubicBezierWidget module

const Cu = Components.utils;
var {require} = Cu.import("resource://gre/modules/devtools/Loader.jsm", {});
var {CubicBezier, BezierCanvas} = require("devtools/shared/widgets/CubicBezierWidget");

function run_test() {
  offsetsGetterReturnsData();
  convertsOffsetsToCoordinates();
  plotsCanvas();
}

function offsetsGetterReturnsData() {
  do_print("offsets getter returns an array of 2 offset objects");

  let b = new BezierCanvas(getCanvasMock(), getCubicBezier(), [.25, 0]);
  let offsets = b.offsets;

  do_check_eq(offsets.length, 2);

  do_check_true("top" in offsets[0]);
  do_check_true("left" in offsets[0]);
  do_check_true("top" in offsets[1]);
  do_check_true("left" in offsets[1]);

  do_check_eq(offsets[0].top, "300px");
  do_check_eq(offsets[0].left, "0px");
  do_check_eq(offsets[1].top, "100px");
  do_check_eq(offsets[1].left, "200px");

  do_print("offsets getter returns data according to current padding");

  b = new BezierCanvas(getCanvasMock(), getCubicBezier(), [0, 0]);
  offsets = b.offsets;

  do_check_eq(offsets[0].top, "400px");
  do_check_eq(offsets[0].left, "0px");
  do_check_eq(offsets[1].top, "0px");
  do_check_eq(offsets[1].left, "200px");
}

function convertsOffsetsToCoordinates() {
  do_print("Converts offsets to coordinates");

  let b = new BezierCanvas(getCanvasMock(), getCubicBezier(), [.25, 0]);

  let coordinates = b.offsetsToCoordinates({style: {
    left: "0px",
    top: "0px"
  }});
  do_check_eq(coordinates.length, 2);
  do_check_eq(coordinates[0], 0);
  do_check_eq(coordinates[1], 1.5);

  coordinates = b.offsetsToCoordinates({style: {
    left: "0px",
    top: "300px"
  }});
  do_check_eq(coordinates[0], 0);
  do_check_eq(coordinates[1], 0);

  coordinates = b.offsetsToCoordinates({style: {
    left: "200px",
    top: "100px"
  }});
  do_check_eq(coordinates[0], 1);
  do_check_eq(coordinates[1], 1);
}

function plotsCanvas() {
  do_print("Plots the curve to the canvas");

  let hasDrawnCurve = false;
  let b = new BezierCanvas(getCanvasMock(), getCubicBezier(), [.25, 0]);
  b.ctx.bezierCurveTo = () => hasDrawnCurve = true;
  b.plot();

  do_check_true(hasDrawnCurve);
}

function getCubicBezier() {
  return new CubicBezier([0,0,1,1]);
}

function getCanvasMock(w=200, h=400) {
  return {
    getContext: function() {
      return {
        scale: () => {},
        translate: () => {},
        clearRect: () => {},
        beginPath: () => {},
        closePath: () => {},
        moveTo: () => {},
        lineTo: () => {},
        stroke: () => {},
        arc: () => {},
        fill: () => {},
        bezierCurveTo: () => {},
        save: () => {},
        restore: () => {},
        setTransform: () => {}
      };
    },
    width: w,
    height: h
  };
}
