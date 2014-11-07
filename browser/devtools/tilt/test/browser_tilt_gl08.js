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
    info("Skipping tilt_gl08 because WebGL isn't supported on this hardware.");
    return;
  }

  let canvas = createCanvas();

  let renderer = new TiltGL.Renderer(canvas, onWebGLFail, onWebGLSuccess);
  let gl = renderer.context;

  if (!isWebGLAvailable) {
    aborting();
    return;
  }


  let t = new renderer.Texture({
    source: canvas,
    format: "RGB"
  });

  ok(t instanceof TiltGL.Texture,
    "The texture object wasn't instantiated correctly.");

  ok(t._ref,
    "The texture WebGL object wasn't created properly.");
  isnot(t._id, -1,
    "The texture id wasn't set properly.");
  isnot(t.width, -1,
    "The texture width wasn't set properly.");
  isnot(t.height, -1,
    "The texture height wasn't set properly.");
  ok(t.loaded,
    "The texture loaded flag wasn't set to true as it should.");
}
