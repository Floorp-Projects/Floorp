/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
  * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Cc, Ci } = require("chrome");
const Services = require("Services");

const WEBGL_CONTEXT_NAME = "experimental-webgl";

function isWebGLForceEnabled() {
  return Services.prefs.getBoolPref("webgl.force-enabled");
}

function isWebGLSupportedByGFX() {
  let supported = false;

  try {
    let gfxInfo = Cc["@mozilla.org/gfx/info;1"].getService(Ci.nsIGfxInfo);
    let angle = gfxInfo.FEATURE_WEBGL_ANGLE;
    let opengl = gfxInfo.FEATURE_WEBGL_OPENGL;

    // if either the Angle or OpenGL renderers are available, WebGL should work
    supported = gfxInfo.getFeatureStatus(angle) === gfxInfo.FEATURE_STATUS_OK ||
                gfxInfo.getFeatureStatus(opengl) === gfxInfo.FEATURE_STATUS_OK;
  } catch (e) {
    return false;
  }
  return supported;
}

function create3DContext(canvas) {
  // try to get a valid context from an existing canvas
  let context = null;
  try {
    context = canvas.getContext(WEBGL_CONTEXT_NAME, {});
  } catch (e) {
    return null;
  }
  return context;
}

function createCanvas(doc) {
  return doc.createElementNS("http://www.w3.org/1999/xhtml", "canvas");
}

function isWebGLSupported(doc) {
  let supported =
    !isWebGLForceEnabled() &&
     isWebGLSupportedByGFX() &&
     create3DContext(createCanvas(doc));

  return supported;
}
exports.isWebGLSupported = isWebGLSupported;
