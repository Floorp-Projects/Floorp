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
    info("Skipping tilt_gl05 because WebGL isn't supported on this hardware.");
    return;
  }

  let canvas = createCanvas();

  let renderer = new TiltGL.Renderer(canvas, onWebGLFail, onWebGLSuccess);
  let gl = renderer.context;

  if (!isWebGLAvailable) {
    return;
  }


  let mesh = {
    vertices: new renderer.VertexBuffer([1, 2, 3], 3),
    indices: new renderer.IndexBuffer([1]),
  };

  ok(mesh.vertices instanceof TiltGL.VertexBuffer,
    "The mesh vertices weren't saved at initialization.");
  ok(mesh.indices instanceof TiltGL.IndexBuffer,
    "The mesh indices weren't saved at initialization.");
}
