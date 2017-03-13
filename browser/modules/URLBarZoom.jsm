// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [ "URLBarZoom" ];

Components.utils.import("resource://gre/modules/Services.jsm");

var URLBarZoom = {
  init(aWindow) {
    aWindow.addEventListener("EndSwapDocShells", onEndSwapDocShells, true);
    aWindow.addEventListener("unload", () => {
      aWindow.removeEventListener("EndSwapDocShells", onEndSwapDocShells, true);
    }, {once: true});
  },
}

function fullZoomObserver(aSubject, aTopic) {
  // If the tab was the last one in its window and has been dragged to another
  // window, the original browser's window will be unavailable here. Since that
  // window is closing, we can just ignore this notification.
  if (!aSubject.ownerGlobal) {
    return;
  }

  let animate = (aTopic != "browser-fullZoom:location-change");
  updateZoomButton(aSubject, animate);
}

function onEndSwapDocShells(event) {
  updateZoomButton(event.originalTarget);
}

  /**
   * Updates the zoom button in the location bar.
   *
   * @param {object} aBrowser The browser that the zoomed content resides in.
   * @param {boolean} aAnimate Should be True for all cases unless the zoom
   *   change is related to tab switching. Optional
   * @param {number} aValue The value that should be used for the zoom control.
   *   If not provided then the value will be read from the window. Useful
   *   if the data on the window may be stale.
   */
function updateZoomButton(aBrowser, aAnimate = false, aValue = undefined) {
  let win = aBrowser.ownerGlobal;
  if (aBrowser != win.gBrowser.selectedBrowser) {
    return;
  }

  let customizableZoomControls = win.document.getElementById("zoom-controls");
  let zoomResetButton = win.document.getElementById("urlbar-zoom-button");

  // Ensure that zoom controls haven't already been added to browser in Customize Mode
  if (customizableZoomControls &&
      customizableZoomControls.getAttribute("cui-areatype") == "toolbar") {
    zoomResetButton.hidden = true;
    return;
  }

  let zoomFactor = Math.round((aValue || win.ZoomManager.zoom) * 100);
  if (zoomFactor != 100) {
    // Check if zoom button is visible and update label if it is
    if (zoomResetButton.hidden) {
      zoomResetButton.hidden = false;
    }
    if (aAnimate) {
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
}

Services.obs.addObserver(fullZoomObserver, "browser-fullZoom:zoomChange", false);
Services.obs.addObserver(fullZoomObserver, "browser-fullZoom:zoomReset", false);
Services.obs.addObserver(fullZoomObserver, "browser-fullZoom:location-change", false);
Services.mm.addMessageListener("SyntheticDocument:ZoomChange", function(aMessage) {
  updateZoomButton(aMessage.target, true, aMessage.data.value);
});
