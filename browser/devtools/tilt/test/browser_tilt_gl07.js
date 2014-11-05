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
    aborting();
    info("Skipping tilt_gl07 because WebGL isn't supported on this hardware.");
    return;
  }

  let canvas = createCanvas();

  let renderer = new TiltGL.Renderer(canvas, onWebGLFail, onWebGLSuccess);
  let gl = renderer.context;

  if (!isWebGLAvailable) {
    aborting();
    return;
  }


  let p = new renderer.Program({
    vs: TiltGL.ColorShader.vs,
    fs: TiltGL.ColorShader.fs,
    attributes: ["vertexPosition"],
    uniforms: ["mvMatrix", "projMatrix", "fill"]
  });

  ok(p instanceof TiltGL.Program,
    "The program object wasn't instantiated correctly.");

  ok(p._ref,
    "The program WebGL object wasn't created properly.");
  isnot(p._id, -1,
    "The program id wasn't set properly.");
  ok(p._attributes,
    "The program attributes cache wasn't created properly.");
  ok(p._uniforms,
    "The program uniforms cache wasn't created properly.");

  is(typeof p._attributes.vertexPosition, "number",
    "The vertexPosition attribute wasn't cached as it should.");
  is(typeof p._uniforms.mvMatrix, "object",
    "The mvMatrix uniform wasn't cached as it should.");
  is(typeof p._uniforms.projMatrix, "object",
    "The projMatrix uniform wasn't cached as it should.");
  is(typeof p._uniforms.fill, "object",
    "The fill uniform wasn't cached as it should.");
}
