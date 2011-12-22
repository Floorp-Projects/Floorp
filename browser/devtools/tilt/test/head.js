/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*global Services, Components, gBrowser, executeSoon */
/*global InspectorUI, Tilt, TiltGL, EPSILON */
"use strict";

Components.utils.import("resource:///modules/devtools/TiltGL.jsm");
Components.utils.import("resource:///modules/devtools/TiltMath.jsm");
Components.utils.import("resource:///modules/devtools/TiltUtils.jsm");
Components.utils.import("resource:///modules/devtools/TiltVisualizer.jsm");


const DEFAULT_HTML = "data:text/html," +
  "<DOCTYPE html>" +
  "<html>" +
    "<head>" +
      "<title>Three Laws</title>" +
    "</head>" +
    "<body>" +
      "<div>" +
        "A robot may not injure a human being or, through inaction, allow a" +
        "human being to come to harm." +
      "</div>" +
      "<div>" +
        "A robot must obey the orders given to it by human beings, except" +
        "where such orders would conflict with the First Law." +
      "</div>" +
      "<div>" +
        "A robot must protect its own existence as long as such protection" +
        "does not conflict with the First or Second Laws." +
      "</div>" +
    "<body>" +
  "</html>";

const INSPECTOR_OPENED = InspectorUI.INSPECTOR_NOTIFICATIONS.OPENED;
const INSPECTOR_CLOSED = InspectorUI.INSPECTOR_NOTIFICATIONS.CLOSED;

const TILT_INITIALIZED = Tilt.NOTIFICATIONS.INITIALIZED;
const TILT_DESTROYED = Tilt.NOTIFICATIONS.DESTROYED;
const TILT_SHOWN = Tilt.NOTIFICATIONS.SHOWN;
const TILT_HIDDEN = Tilt.NOTIFICATIONS.HIDDEN;

const TILT_ENABLED = Services.prefs.getBoolPref("devtools.tilt.enabled");
const INSP_ENABLED = Services.prefs.getBoolPref("devtools.inspector.enabled");


function isTiltEnabled() {
  return TILT_ENABLED && INSP_ENABLED;
}

function isWebGLSupported() {
  return TiltGL.isWebGLSupported() && TiltGL.create3DContext(createCanvas());
}

function isApprox(num1, num2) {
  return Math.abs(num1 - num2) < EPSILON;
}

function isApproxVec(vec1, vec2) {
  if (vec1.length !== vec2.length) {
    return false;
  }
  for (let i = 0, len = vec1.length; i < len; i++) {
    if (!isApprox(vec1[i], vec2[i])) {
      return false;
    }
  }
  return true;
}


function createCanvas() {
  return document.createElementNS("http://www.w3.org/1999/xhtml", "canvas");
}


function createTab(callback, location) {
  let tab = gBrowser.selectedTab = gBrowser.addTab();

  gBrowser.selectedBrowser.addEventListener("load", function onLoad() {
    gBrowser.selectedBrowser.removeEventListener("load", onLoad, true);
    callback(tab);
  }, true);

  gBrowser.selectedBrowser.contentWindow.location = location || DEFAULT_HTML;
  return tab;
}


function createTilt(callbacks, close) {
  Services.obs.addObserver(onInspectorOpen, INSPECTOR_OPENED, false);
  InspectorUI.toggleInspectorUI();

  function onInspectorOpen() {
    Services.obs.removeObserver(onInspectorOpen, INSPECTOR_OPENED);

    executeSoon(function() {
      if ("function" === typeof callbacks.onInspectorOpen) {
        callbacks.onInspectorOpen();
      }
      Services.obs.addObserver(onTiltOpen, TILT_INITIALIZED, false);
      Tilt.initialize();
    });
  }

  function onTiltOpen() {
    Services.obs.removeObserver(onTiltOpen, TILT_INITIALIZED);

    executeSoon(function() {
      if ("function" === typeof callbacks.onTiltOpen) {
        callbacks.onTiltOpen(Tilt.visualizers[Tilt.currentWindowId]);
      }
      if (close) {
        Services.obs.addObserver(onTiltClose, TILT_DESTROYED, false);
        Tilt.destroy(Tilt.currentWindowId);
      }
    });
  }

  function onTiltClose() {
    Services.obs.removeObserver(onTiltClose, TILT_DESTROYED);

    executeSoon(function() {
      if ("function" === typeof callbacks.onTiltClose) {
        callbacks.onTiltClose();
      }
      if (close) {
        Services.obs.addObserver(onInspectorClose, INSPECTOR_CLOSED, false);
        InspectorUI.closeInspectorUI();
      }
    });
  }

  function onInspectorClose() {
    Services.obs.removeObserver(onInspectorClose, INSPECTOR_CLOSED);

    executeSoon(function() {
      if ("function" === typeof callbacks.onInspectorClose) {
        callbacks.onInspectorClose();
      }
      if ("function" === typeof callbacks.onEnd) {
        callbacks.onEnd();
      }
    });
  }
}
