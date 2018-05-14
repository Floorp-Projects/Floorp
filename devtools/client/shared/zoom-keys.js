/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Ci } = require("chrome");
const Services = require("Services");
const KeyShortcuts = require("devtools/client/shared/key-shortcuts");

const ZOOM_PREF = "devtools.toolbox.zoomValue";
const MIN_ZOOM = 0.5;
const MAX_ZOOM = 2;

const {LocalizationHelper} = require("devtools/shared/l10n");
const L10N = new LocalizationHelper("devtools/client/locales/toolbox.properties");

/**
 * Register generic keys to control zoom level of the given document.
 * Used by both the toolboxes and the browser console.
 *
 * @param {DOMWindow} The window on which we should listent to key strokes and
 *                    modify the zoom factor.
 */
exports.register = function(window) {
  let shortcuts = new KeyShortcuts({
    window
  });
  let docShell = window.QueryInterface(Ci.nsIInterfaceRequestor)
    .getInterface(Ci.nsIWebNavigation)
    .QueryInterface(Ci.nsIDocShell);
  let contViewer = docShell.contentViewer;
  let zoomValue = parseFloat(Services.prefs.getCharPref(ZOOM_PREF));
  let zoomIn = function(event) {
    setZoom(zoomValue + 0.1);
    event.preventDefault();
  };

  let zoomOut = function(event) {
    setZoom(zoomValue - 0.1);
    event.preventDefault();
  };

  let zoomReset = function(event) {
    setZoom(1);
    event.preventDefault();
  };

  let setZoom = function(newValue) {
    // cap zoom value
    zoomValue = Math.max(newValue, MIN_ZOOM);
    zoomValue = Math.min(zoomValue, MAX_ZOOM);
    // Prevent the floating-point error. (e.g. 1.1 + 0.1 = 1.2000000000000002)
    zoomValue = Math.round(zoomValue * 10) / 10;

    contViewer.fullZoom = zoomValue;

    Services.prefs.setCharPref(ZOOM_PREF, zoomValue);
  };

  // Set zoom to whatever the last setting was.
  setZoom(zoomValue);

  shortcuts.on(L10N.getStr("toolbox.zoomIn.key"), zoomIn);
  let zoomIn2 = L10N.getStr("toolbox.zoomIn2.key");
  if (zoomIn2) {
    shortcuts.on(zoomIn2, zoomIn);
  }
  let zoomIn3 = L10N.getStr("toolbox.zoomIn3.key");
  if (zoomIn3) {
    shortcuts.on(zoomIn3, zoomIn);
  }

  shortcuts.on(L10N.getStr("toolbox.zoomOut.key"),
               zoomOut);
  let zoomOut2 = L10N.getStr("toolbox.zoomOut2.key");
  if (zoomOut2) {
    shortcuts.on(zoomOut2, zoomOut);
  }

  shortcuts.on(L10N.getStr("toolbox.zoomReset.key"),
               zoomReset);
  let zoomReset2 = L10N.getStr("toolbox.zoomReset2.key");
  if (zoomReset2) {
    shortcuts.on(zoomReset2, zoomReset);
  }
};
