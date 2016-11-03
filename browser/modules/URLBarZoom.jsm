// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [ "URLBarZoom" ];

Components.utils.import("resource://gre/modules/Services.jsm");

var URLBarZoom = {

  init(aWindow) {
    // Register ourselves with the service so we know when the zoom prefs change.
    Services.obs.addObserver(this, "browser-fullZoom:zoomChange", false);
    Services.obs.addObserver(this, "browser-fullZoom:zoomReset", false);
    Services.obs.addObserver(this, "browser-fullZoom:location-change", false);
  },

  observe(aSubject, aTopic) {
    this.updateZoomButton(aSubject, aTopic);
  },

  updateZoomButton(aSubject, aTopic) {
    // aSubject.ownerGlobal may no longer exist if a tab has been dragged to a
    // new window. In this case, aSubject.ownerGlobal will be supplied by
    // updateZoomButton() called in XULBrowserWindow.onLocationChange().
    if (!aSubject.ownerGlobal) {
      return;
    }

    let win = aSubject.ownerGlobal;
    let customizableZoomControls = win.document.getElementById("zoom-controls");
    let zoomResetButton = win.document.getElementById("urlbar-zoom-button");
    let zoomFactor = Math.round(win.ZoomManager.zoom * 100);

    // Ensure that zoom controls haven't already been added to browser in Customize Mode
    if (customizableZoomControls &&
        customizableZoomControls.getAttribute("cui-areatype") == "toolbar") {
      zoomResetButton.hidden = true;
      return;
    }
    if (zoomFactor != 100) {
      // Check if zoom button is visible and update label if it is
      if (zoomResetButton.hidden) {
        zoomResetButton.hidden = false;
      }
      // Only allow pulse animation for zoom changes, not tab switching
      if (aTopic != "browser-fullZoom:location-change") {
        zoomResetButton.setAttribute("animate", "true");
      } else {
        zoomResetButton.removeAttribute("animate");
      }
      zoomResetButton.setAttribute("label",
        win.gNavigatorBundle.getFormattedString("urlbar-zoom-button.label", [zoomFactor]));
    } else {
      // Hide button if zoom is at 100%
      zoomResetButton.hidden = true;
    }
  },
};
