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
    info("Skipping tilt_gl01 because WebGL isn't supported on this hardware.");
    return;
  }

  let canvas = createCanvas();

  let renderer = new TiltGL.Renderer(canvas, onWebGLFail, onWebGLSuccess);
  let gl = renderer.context;

  if (!isWebGLAvailable) {
    aborting();
    return;
  }


  ok(renderer,
    "The TiltGL.Renderer constructor should have initialized a new object.");

  ok(gl instanceof WebGLRenderingContext,
    "The renderer context wasn't created correctly from the passed canvas.");


  let clearColor = gl.getParameter(gl.COLOR_CLEAR_VALUE),
      clearDepth = gl.getParameter(gl.DEPTH_CLEAR_VALUE);

  is(clearColor[0], 0,
    "The default red clear color wasn't set correctly at initialization.");
  is(clearColor[1], 0,
    "The default green clear color wasn't set correctly at initialization.");
  is(clearColor[2], 0,
    "The default blue clear color wasn't set correctly at initialization.");
  is(clearColor[3], 0,
    "The default alpha clear color wasn't set correctly at initialization.");
  is(clearDepth, 1,
    "The default clear depth wasn't set correctly at initialization.");

  is(renderer.width, canvas.width,
    "The renderer width wasn't set correctly from the passed canvas.");
  is(renderer.height, canvas.height,
    "The renderer height wasn't set correctly from the passed canvas.");

  ok(renderer.mvMatrix,
    "The model view matrix wasn't initialized properly.");
  ok(renderer.projMatrix,
    "The model view matrix wasn't initialized properly.");

  ok(isApproxVec(renderer._fillColor, [1, 1, 1, 1]),
    "The default fill color wasn't set correctly.");
  ok(isApproxVec(renderer._strokeColor, [0, 0, 0, 1]),
    "The default stroke color wasn't set correctly.");
  is(renderer._strokeWeightValue, 1,
    "The default stroke weight wasn't set correctly.");

  ok(renderer._colorShader,
    "A default color shader should have been created.");

  ok(typeof renderer.Program, "function",
    "At init, the renderer should have created a Program constructor.");
  ok(typeof renderer.VertexBuffer, "function",
    "At init, the renderer should have created a VertexBuffer constructor.");
  ok(typeof renderer.IndexBuffer, "function",
    "At init, the renderer should have created a IndexBuffer constructor.");
  ok(typeof renderer.Texture, "function",
    "At init, the renderer should have created a Texture constructor.");

  renderer.depthTest(true);
  is(gl.getParameter(gl.DEPTH_TEST), true,
    "The depth test wasn't enabled when requested.");

  renderer.depthTest(false);
  is(gl.getParameter(gl.DEPTH_TEST), false,
    "The depth test wasn't disabled when requested.");

  renderer.stencilTest(true);
  is(gl.getParameter(gl.STENCIL_TEST), true,
    "The stencil test wasn't enabled when requested.");

  renderer.stencilTest(false);
  is(gl.getParameter(gl.STENCIL_TEST), false,
    "The stencil test wasn't disabled when requested.");

  renderer.cullFace("front");
  is(gl.getParameter(gl.CULL_FACE), true,
    "The cull face wasn't enabled when requested.");
  is(gl.getParameter(gl.CULL_FACE_MODE), gl.FRONT,
    "The cull face front mode wasn't set correctly.");

  renderer.cullFace("back");
  is(gl.getParameter(gl.CULL_FACE), true,
    "The cull face wasn't enabled when requested.");
  is(gl.getParameter(gl.CULL_FACE_MODE), gl.BACK,
    "The cull face back mode wasn't set correctly.");

  renderer.cullFace("both");
  is(gl.getParameter(gl.CULL_FACE), true,
    "The cull face wasn't enabled when requested.");
  is(gl.getParameter(gl.CULL_FACE_MODE), gl.FRONT_AND_BACK,
    "The cull face back mode wasn't set correctly.");

  renderer.cullFace(false);
  is(gl.getParameter(gl.CULL_FACE), false,
    "The cull face wasn't disabled when requested.");

  renderer.frontFace("cw");
  is(gl.getParameter(gl.FRONT_FACE), gl.CW,
    "The front face cw mode wasn't set correctly.");

  renderer.frontFace("ccw");
  is(gl.getParameter(gl.FRONT_FACE), gl.CCW,
    "The front face ccw mode wasn't set correctly.");

  renderer.blendMode("alpha");
  is(gl.getParameter(gl.BLEND), true,
    "The blend mode wasn't enabled when requested.");
  is(gl.getParameter(gl.BLEND_SRC_ALPHA), gl.SRC_ALPHA,
    "The source blend func wasn't set correctly.");
  is(gl.getParameter(gl.BLEND_DST_ALPHA), gl.ONE_MINUS_SRC_ALPHA,
    "The destination blend func wasn't set correctly.");

  renderer.blendMode("add");
  is(gl.getParameter(gl.BLEND), true,
    "The blend mode wasn't enabled when requested.");
  is(gl.getParameter(gl.BLEND_SRC_ALPHA), gl.SRC_ALPHA,
    "The source blend func wasn't set correctly.");
  is(gl.getParameter(gl.BLEND_DST_ALPHA), gl.ONE,
    "The destination blend func wasn't set correctly.");

  renderer.blendMode(false);
  is(gl.getParameter(gl.CULL_FACE), false,
    "The blend mode wasn't disabled when requested.");


  is(gl.getParameter(gl.CURRENT_PROGRAM), null,
    "No program should be initially set in the WebGL context.");

  renderer.useColorShader(new renderer.VertexBuffer([1, 2, 3], 3));

  ok(gl.getParameter(gl.CURRENT_PROGRAM) instanceof WebGLProgram,
    "The correct program hasn't been set in the WebGL context.");
}
