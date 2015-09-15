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
    info("Skipping tilt_gl03 because WebGL isn't supported on this hardware.");
    return;
  }

  let canvas = createCanvas();

  let renderer = new TiltGL.Renderer(canvas, onWebGLFail, onWebGLSuccess);
  let gl = renderer.context;

  if (!isWebGLAvailable) {
    aborting();
    return;
  }


  renderer.defaults();
  is(gl.getParameter(gl.DEPTH_TEST), true,
    "The depth test wasn't set to the correct default value.");
  is(gl.getParameter(gl.STENCIL_TEST), false,
    "The stencil test wasn't set to the correct default value.");
  is(gl.getParameter(gl.CULL_FACE), false,
    "The cull face wasn't set to the correct default value.");
  is(gl.getParameter(gl.FRONT_FACE), gl.CCW,
    "The front face wasn't set to the correct default value.");
  is(gl.getParameter(gl.BLEND), true,
    "The blend mode wasn't set to the correct default value.");
  is(gl.getParameter(gl.BLEND_SRC_ALPHA), gl.SRC_ALPHA,
    "The source blend func wasn't set to the correct default value.");
  is(gl.getParameter(gl.BLEND_DST_ALPHA), gl.ONE_MINUS_SRC_ALPHA,
    "The destination blend func wasn't set to the correct default value.");


  ok(isApproxVec(renderer._fillColor, [1, 1, 1, 1]),
    "The fill color wasn't set to the correct default value.");
  ok(isApproxVec(renderer._strokeColor, [0, 0, 0, 1]),
    "The stroke color wasn't set to the correct default value.");
  is(renderer._strokeWeightValue, 1,
    "The stroke weight wasn't set to the correct default value.");
  is(gl.getParameter(gl.LINE_WIDTH), 1,
    "The stroke weight wasn't applied with the correct default value.");


  ok(isApproxVec(renderer.projMatrix, [
    1.2071068286895752, 0, 0, 0, 0, -2.4142136573791504, 0, 0, 0, 0,
    -1.0202020406723022, -1, -181.06602478027344, 181.06602478027344,
    148.14492797851562, 181.06602478027344
  ]), "The default perspective projection matrix wasn't set correctly.");

  ok(isApproxVec(renderer.mvMatrix, [
    1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1
  ]), "The default model view matrix wasn't set correctly.");
}
