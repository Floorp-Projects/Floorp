"use strict";

function parseArgs() {
  var query = window.location.search.substring(1);

  var split = query.split("&");

  var args = {}
  for (var i = 0; i < split.length; i++) {
    var pair = split[i].split("=");

    var key = pair[0];
    var value = true;
    if (pair.length >= 2) {
      eval("value = " + decodeURIComponent(pair[1]) + ";");
    }

    args[key] = value;
  }

  return args;
}

var gArgs = null;
function arg(key, defaultVal) {
  if (gArgs === null) {
    gArgs = parseArgs();
  }

  if (!(key in gArgs))
    return defaultVal;

  return gArgs[key];
}

function initGL(canvas) {
  if (arg("nogl"))
    return null;

  var gl = null;

  var withAA = arg("aa", false);
  var withAlpha = arg("alpha", false);
  var withDepth = arg("depth", false);
  var withPremult = arg("premult", false);
  var withPreserve = arg("preserve", false);
  var withStencil = arg("stencil", false);

  try {
    var argDict = {
      alpha: withAlpha,
      depth: withDepth,
      stencil: withStencil,
      antialias: withAA,
      premultipliedAlpha: withPremult,
      preserveDrawingBuffer: withPreserve,
    };
    gl = canvas.getContext("experimental-webgl", argDict);
  } catch(e) {}

  return gl;
}

function rAF(func) {
  var raf = window.requestAnimationFrame;
  raf(func);
}

var MAX_WAIT_FOR_COMPOSITE_DELAY_MS = 500;

function waitForComposite(func) {
  var isDone = false;
  var doneFunc = function () {
    if (isDone)
      return;
    isDone = true;
    func();
  };

  rAF(doneFunc);
  setTimeout(doneFunc, MAX_WAIT_FOR_COMPOSITE_DELAY_MS);
}
