/* -*- Mode: Javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests the BezierCanvas API in the CubicBezierWidget module

var Cu = Components.utils;
var {require} = Cu.import("resource://devtools/shared/Loader.jsm", {});
var {CubicBezier, BezierCanvas} = require("devtools/client/shared/widgets/CubicBezierWidget");

function run_test() {
  offsetsGetterReturnsData();
  convertsOffsetsToCoordinates();
  plotsCanvas();
}

function offsetsGetterReturnsData() {
  info("offsets getter returns an array of 2 offset objects");

  let b = new BezierCanvas(getCanvasMock(), getCubicBezier(), [.25, 0]);
  let offsets = b.offsets;

  Assert.equal(offsets.length, 2);

  Assert.ok("top" in offsets[0]);
  Assert.ok("left" in offsets[0]);
  Assert.ok("top" in offsets[1]);
  Assert.ok("left" in offsets[1]);

  Assert.equal(offsets[0].top, "300px");
  Assert.equal(offsets[0].left, "0px");
  Assert.equal(offsets[1].top, "100px");
  Assert.equal(offsets[1].left, "200px");

  info("offsets getter returns data according to current padding");

  b = new BezierCanvas(getCanvasMock(), getCubicBezier(), [0, 0]);
  offsets = b.offsets;

  Assert.equal(offsets[0].top, "400px");
  Assert.equal(offsets[0].left, "0px");
  Assert.equal(offsets[1].top, "0px");
  Assert.equal(offsets[1].left, "200px");
}

function convertsOffsetsToCoordinates() {
  info("Converts offsets to coordinates");

  let b = new BezierCanvas(getCanvasMock(), getCubicBezier(), [.25, 0]);

  let coordinates = b.offsetsToCoordinates({style: {
    left: "0px",
    top: "0px"
  }});
  Assert.equal(coordinates.length, 2);
  Assert.equal(coordinates[0], 0);
  Assert.equal(coordinates[1], 1.5);

  coordinates = b.offsetsToCoordinates({style: {
    left: "0px",
    top: "300px"
  }});
  Assert.equal(coordinates[0], 0);
  Assert.equal(coordinates[1], 0);

  coordinates = b.offsetsToCoordinates({style: {
    left: "200px",
    top: "100px"
  }});
  Assert.equal(coordinates[0], 1);
  Assert.equal(coordinates[1], 1);
}

function plotsCanvas() {
  info("Plots the curve to the canvas");

  let hasDrawnCurve = false;
  let b = new BezierCanvas(getCanvasMock(), getCubicBezier(), [.25, 0]);
  b.ctx.bezierCurveTo = () => {
    hasDrawnCurve = true;
  };
  b.plot();

  Assert.ok(hasDrawnCurve);
}

function getCubicBezier() {
  return new CubicBezier([0, 0, 1, 1]);
}

function getCanvasMock(w = 200, h = 400) {
  return {
    getContext: function () {
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
