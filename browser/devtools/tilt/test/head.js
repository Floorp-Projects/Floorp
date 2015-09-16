/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

let {require} = Components.utils.import("resource://gre/modules/devtools/Loader.jsm", {});
let TiltManager = require("devtools/tilt/tilt").TiltManager;
let TiltGL = require("devtools/tilt/tilt-gl");
let {EPSILON, TiltMath, vec3, mat3, mat4, quat4} = require("devtools/tilt/tilt-math");
let TiltUtils = require("devtools/tilt/tilt-utils");
let {TiltVisualizer} = require("devtools/tilt/tilt-visualizer");
let DevToolsUtils = require("devtools/toolkit/DevToolsUtils");
let {getRect, getIframeContentOffset} = require("devtools/toolkit/layout/utils");


const DEFAULT_HTML = "data:text/html," +
  "<DOCTYPE html>" +
  "<html>" +
    "<head>" +
      "<meta charset='utf-8'/>" +
      "<title>Three Laws</title>" +
    "</head>" +
    "<body>" +
      "<div id='first-law'>" +
        "A robot may not injure a human being or, through inaction, allow a " +
        "human being to come to harm." +
      "</div>" +
      "<div>" +
        "A robot must obey the orders given to it by human beings, except " +
        "where such orders would conflict with the First Law." +
      "</div>" +
      "<div>" +
        "A robot must protect its own existence as long as such protection " +
        "does not conflict with the First or Second Laws." +
      "</div>" +
      "<div id='far-far-away' style='position: absolute; top: 250%;'>" +
        "I like bacon." +
      "</div>" +
    "<body>" +
  "</html>";

let Tilt = TiltManager.getTiltForBrowser(window);

const STARTUP = Tilt.NOTIFICATIONS.STARTUP;
const INITIALIZING = Tilt.NOTIFICATIONS.INITIALIZING;
const INITIALIZED = Tilt.NOTIFICATIONS.INITIALIZED;
const DESTROYING = Tilt.NOTIFICATIONS.DESTROYING;
const BEFORE_DESTROYED = Tilt.NOTIFICATIONS.BEFORE_DESTROYED;
const DESTROYED = Tilt.NOTIFICATIONS.DESTROYED;
const SHOWN = Tilt.NOTIFICATIONS.SHOWN;
const HIDDEN = Tilt.NOTIFICATIONS.HIDDEN;
const HIGHLIGHTING = Tilt.NOTIFICATIONS.HIGHLIGHTING;
const UNHIGHLIGHTING = Tilt.NOTIFICATIONS.UNHIGHLIGHTING;
const NODE_REMOVED = Tilt.NOTIFICATIONS.NODE_REMOVED;

const TILT_ENABLED = Services.prefs.getBoolPref("devtools.tilt.enabled");

DevToolsUtils.testing = true;
SimpleTest.registerCleanupFunction(() => {
  DevToolsUtils.testing = false;
});

function isTiltEnabled() {
  info("Apparently, Tilt is" + (TILT_ENABLED ? "" : " not") + " enabled.");
  return TILT_ENABLED;
}

function isWebGLSupported() {
  let supported = !TiltGL.isWebGLForceEnabled() &&
                   TiltGL.isWebGLSupported() &&
                   TiltGL.create3DContext(createCanvas());

  info("Apparently, WebGL is" + (supported ? "" : " not") + " supported.");
  return supported;
}

function isApprox(num1, num2, delta) {
  if (Math.abs(num1 - num2) > (delta || EPSILON)) {
    info("isApprox expected " + num1 + ", got " + num2 + " instead.");
    return false;
  }
  return true;
}

function isApproxVec(vec1, vec2, delta) {
  vec1 = Array.prototype.slice.call(vec1);
  vec2 = Array.prototype.slice.call(vec2);

  if (vec1.length !== vec2.length) {
    return false;
  }
  for (let i = 0, len = vec1.length; i < len; i++) {
    if (!isApprox(vec1[i], vec2[i], delta)) {
      info("isApproxVec expected [" + vec1 + "], got [" + vec2 + "] instead.");
      return false;
    }
  }
  return true;
}

function isEqualVec(vec1, vec2) {
  vec1 = Array.prototype.slice.call(vec1);
  vec2 = Array.prototype.slice.call(vec2);

  if (vec1.length !== vec2.length) {
    return false;
  }
  for (let i = 0, len = vec1.length; i < len; i++) {
    if (vec1[i] !== vec2[i]) {
      info("isEqualVec expected [" + vec1 + "], got [" + vec2 + "] instead.");
      return false;
    }
  }
  return true;
}

function createCanvas() {
  return document.createElementNS("http://www.w3.org/1999/xhtml", "canvas");
}


function createTab(callback, location) {
  info("Creating a tab, with callback " + typeof callback +
                      ", and location " + location + ".");

  let tab = gBrowser.selectedTab = gBrowser.addTab();

  gBrowser.selectedBrowser.addEventListener("load", function onLoad() {
    gBrowser.selectedBrowser.removeEventListener("load", onLoad, true);
    callback(tab);
  }, true);

  gBrowser.selectedBrowser.contentWindow.location = location || DEFAULT_HTML;
  return tab;
}


function createTilt(callbacks, close, suddenDeath) {
  info("Creating Tilt, with callbacks {" + Object.keys(callbacks) + "}" +
                   ", autoclose param " + close +
          ", and sudden death handler " + typeof suddenDeath + ".");

  handleFailure(suddenDeath);

  Services.prefs.setBoolPref("webgl.verbose", true);
  TiltUtils.Output.suppressAlerts = true;

  info("Attempting to start Tilt.");
  Services.obs.addObserver(onTiltOpen, INITIALIZING, false);
  Tilt.toggle();

  function onTiltOpen() {
    info("Tilt was opened.");
    Services.obs.removeObserver(onTiltOpen, INITIALIZING);

    executeSoon(function() {
      if ("function" === typeof callbacks.onTiltOpen) {
        info("Calling 'onTiltOpen'.");
        callbacks.onTiltOpen(Tilt.visualizers[Tilt.currentWindowId]);
      }
      if (close) {
        executeSoon(function() {
          info("Attempting to close Tilt.");
          Services.obs.addObserver(onTiltClose, DESTROYED, false);
          Tilt.destroy(Tilt.currentWindowId);
        });
      }
    });
  }

  function onTiltClose() {
    info("Tilt was closed.");
    Services.obs.removeObserver(onTiltClose, DESTROYED);

    executeSoon(function() {
      if ("function" === typeof callbacks.onTiltClose) {
        info("Calling 'onTiltClose'.");
        callbacks.onTiltClose();
      }
      if ("function" === typeof callbacks.onEnd) {
        info("Calling 'onEnd'.");
        callbacks.onEnd();
      }
    });
  }

  function handleFailure(suddenDeath) {
    Tilt.failureCallback = function() {
      info("Tilt FAIL.");
      Services.obs.removeObserver(onTiltOpen, INITIALIZING);

      info("Now relying on sudden death handler " + typeof suddenDeath + ".");
      suddenDeath && suddenDeath();
    }
  }
}

function getPickablePoint(presenter) {
  let vertices = presenter._meshStacks[0].vertices.components;

  let topLeft = vec3.create([vertices[0], vertices[1], vertices[2]]);
  let bottomRight = vec3.create([vertices[6], vertices[7], vertices[8]]);
  let center = vec3.lerp(topLeft, bottomRight, 0.5, []);

  let renderer = presenter._renderer;
  let viewport = [0, 0, renderer.width, renderer.height];

  return vec3.project(center, viewport, renderer.mvMatrix, renderer.projMatrix);
}

function aborting() {
  // Tilt aborting and we need at least one pass, fail or todo so let's add a
  // dummy pass.
  ok(true, "Test aborted early.");
}
