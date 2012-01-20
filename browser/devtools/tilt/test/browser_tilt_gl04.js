/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

let isWebGLAvailable;

function onWebGLFail() {
  isWebGLAvailable = false;
}

function onWebGLSuccess() {
  isWebGLAvailable = true;
}

function test() {
  if (!isWebGLSupported()) {
    info("Skipping tilt_gl04 because WebGL isn't supported on this hardware.");
    return;
  }

  let canvas = createCanvas();

  let renderer = new TiltGL.Renderer(canvas, onWebGLFail, onWebGLSuccess);
  let gl = renderer.context;

  if (!isWebGLAvailable) {
    return;
  }


  renderer.perspective();
  ok(isApproxVec(renderer.projMatrix, [
    1.2071068286895752, 0, 0, 0, 0, -2.4142136573791504, 0, 0, 0, 0,
    -1.0202020406723022, -1, -181.06602478027344, 181.06602478027344,
    148.14492797851562, 181.06602478027344
  ]), "The default perspective proj. matrix wasn't calculated correctly.");

  ok(isApproxVec(renderer.mvMatrix, [
    1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1
  ]), "Changing the perpective matrix should reset the modelview by default.");


  renderer.ortho();
  ok(isApproxVec(renderer.projMatrix, [
    0.006666666828095913, 0, 0, 0, 0, -0.013333333656191826, 0, 0, 0, 0, -1,
    0, -1, 1, 0, 1
  ]), "The default ortho proj. matrix wasn't calculated correctly.");
  ok(isApproxVec(renderer.mvMatrix, [
    1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1
  ]), "Changing the ortho matrix should reset the modelview by default.");


  renderer.projection(mat4.perspective(45, 1, 0.1, 100));
  ok(isApproxVec(renderer.projMatrix, [
    2.4142136573791504, 0, 0, 0, 0, 2.4142136573791504, 0, 0, 0, 0,
    -1.0020020008087158, -1, 0, 0, -0.20020020008087158, 0
  ]), "A custom proj. matrix couldn't be set correctly.");
  ok(isApproxVec(renderer.mvMatrix, [
    1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1
  ]), "Setting a custom proj. matrix should reset the model view by default.");


  renderer.translate(1, 1, 1);
  ok(isApproxVec(renderer.mvMatrix, [
    1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 1, 1, 1, 1
  ]), "The translation transformation wasn't applied correctly.");

  renderer.rotate(0.5, 1, 1, 1);
  ok(isApproxVec(renderer.mvMatrix, [
    0.9183883666992188, 0.317602276802063, -0.23599065840244293, 0,
    -0.23599065840244293, 0.9183883666992188, 0.317602276802063, 0,
    0.317602276802063, -0.23599065840244293, 0.9183883666992188, 0, 1, 1, 1, 1
  ]), "The rotation transformation wasn't applied correctly.");

  renderer.rotateX(0.5);
  ok(isApproxVec(renderer.mvMatrix, [
    0.9183883666992188, 0.317602276802063, -0.23599065840244293, 0,
    -0.05483464524149895, 0.6928216814994812, 0.7190210819244385, 0,
    0.391862154006958, -0.6474001407623291, 0.6536949872970581, 0, 1, 1, 1, 1
  ]), "The X rotation transformation wasn't applied correctly.");

  renderer.rotateY(0.5);
  ok(isApproxVec(renderer.mvMatrix, [
    0.6180928945541382, 0.5891023874282837, -0.5204993486404419, 0,
    -0.05483464524149895, 0.6928216814994812, 0.7190210819244385, 0,
    0.7841902375221252, -0.4158804416656494, 0.4605313837528229, 0, 1, 1, 1, 1
  ]), "The Y rotation transformation wasn't applied correctly.");

  renderer.rotateZ(0.5);
  ok(isApproxVec(renderer.mvMatrix, [
    0.5161384344100952, 0.8491423726081848, -0.11206408590078354, 0,
    -0.3444514572620392, 0.3255774974822998, 0.8805410265922546, 0,
    0.7841902375221252, -0.4158804416656494, 0.4605313837528229, 0, 1, 1, 1, 1
  ]), "The Z rotation transformation wasn't applied correctly.");

  renderer.scale(2, 2, 2);
  ok(isApproxVec(renderer.mvMatrix, [
    1.0322768688201904, 1.6982847452163696, -0.22412817180156708, 0,
    -0.6889029145240784, 0.6511549949645996, 1.7610820531845093, 0,
    1.5683804750442505, -0.8317608833312988, 0.9210627675056458, 0, 1, 1, 1, 1
  ]), "The Z rotation transformation wasn't applied correctly.");

  renderer.transform(mat4.create());
  ok(isApproxVec(renderer.mvMatrix, [
    1.0322768688201904, 1.6982847452163696, -0.22412817180156708, 0,
    -0.6889029145240784, 0.6511549949645996, 1.7610820531845093, 0,
    1.5683804750442505, -0.8317608833312988, 0.9210627675056458, 0, 1, 1, 1, 1
  ]), "The identity matrix transformation wasn't applied correctly.");

  renderer.origin(1, 1, 1);
  ok(isApproxVec(renderer.mvMatrix, [
    1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1
  ]), "The origin wasn't reset to identity correctly.");

  renderer.translate(1, 2);
  ok(isApproxVec(renderer.mvMatrix, [
    1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 1, 2, 0, 1
  ]), "The second translation transformation wasn't applied correctly.");

  renderer.scale(3, 4);
  ok(isApproxVec(renderer.mvMatrix, [
    3, 0, 0, 0, 0, 4, 0, 0, 0, 0, 1, 0, 1, 2, 0, 1
  ]), "The second scale transformation wasn't applied correctly.");
}
