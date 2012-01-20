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
    info("Skipping tilt_gl06 because WebGL isn't supported on this hardware.");
    return;
  }

  let canvas = createCanvas();

  let renderer = new TiltGL.Renderer(canvas, onWebGLFail, onWebGLSuccess);
  let gl = renderer.context;

  if (!isWebGLAvailable) {
    return;
  }


  let vb = new renderer.VertexBuffer([1, 2, 3, 4, 5, 6], 3);

  ok(vb instanceof TiltGL.VertexBuffer,
    "The vertex buffer object wasn't instantiated correctly.");
  ok(vb._ref,
    "The vertex buffer gl element wasn't created at initialization.");
  ok(vb.components,
    "The vertex buffer components weren't created at initialization.");
  is(vb.itemSize, 3,
    "The vertex buffer item size isn't set correctly.");
  is(vb.numItems, 2,
    "The vertex buffer number of items weren't calculated correctly.");


  let ib = new renderer.IndexBuffer([0, 1, 2, 3, 4, 5, 6, 7, 8, 9]);

  ok(ib instanceof TiltGL.IndexBuffer,
    "The index buffer object wasn't instantiated correctly.");
  ok(ib._ref,
    "The index buffer gl element wasn't created at initialization.");
  ok(ib.components,
    "The index buffer components weren't created at initialization.");
  is(ib.itemSize, 1,
    "The index buffer item size isn't set correctly.");
  is(ib.numItems, 10,
    "The index buffer number of items weren't calculated correctly.");
}
