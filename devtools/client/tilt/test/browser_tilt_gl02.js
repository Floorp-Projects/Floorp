/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

var isWebGLAvailable;

function onWebGLFail() {
  isWebGLAvailable = false;
}

function onWebGLSuccess() {
  isWebGLAvailable = true;
}

function test() {
  if (!isWebGLSupported()) {
    aborting();
    info("Skipping tilt_gl02 because WebGL isn't supported on this hardware.");
    return;
  }

  let canvas = createCanvas();

  let renderer = new TiltGL.Renderer(canvas, onWebGLFail, onWebGLSuccess);
  let gl = renderer.context;

  if (!isWebGLAvailable) {
    aborting();
    return;
  }


  renderer.fill([1, 0, 0, 1]);
  ok(isApproxVec(renderer._fillColor, [1, 0, 0, 1]),
    "The fill color wasn't set correctly.");

  renderer.stroke([0, 1, 0, 1]);
  ok(isApproxVec(renderer._strokeColor, [0, 1, 0, 1]),
    "The stroke color wasn't set correctly.");

  renderer.strokeWeight(2);
  is(renderer._strokeWeightValue, 2,
    "The stroke weight wasn't set correctly.");
  is(gl.getParameter(gl.LINE_WIDTH), 2,
    "The stroke weight wasn't applied correctly.");
}
