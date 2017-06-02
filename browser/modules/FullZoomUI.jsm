// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [ "FullZoomUI" ];

Components.utils.import("resource://gre/modules/Services.jsm");

var FullZoomUI = {
  init(aWindow) {
    aWindow.addEventListener("EndSwapDocShells", onEndSwapDocShells, true);
    aWindow.addEventListener("FullZoomChange", onFullZoomChange);
    aWindow.addEventListener("unload", () => {
      aWindow.removeEventListener("EndSwapDocShells", onEndSwapDocShells, true);
      aWindow.removeEventListener("FullZoomChange", onFullZoomChange);
    }, {once: true});
  },
}

function fullZoomLocationChangeObserver(aSubject, aTopic) {
  // If the tab was the last one in its window and has been dragged to another
  // window, the original browser's window will be unavailable here. Since that
  // window is closing, we can just ignore this notification.
  if (!aSubject.ownerGlobal) {
    return;
  }
  updateZoomUI(aSubject, false);
}
Services.obs.addObserver(fullZoomLocationChangeObserver, "browser-fullZoom:location-change");

function onEndSwapDocShells(event) {
  updateZoomUI(event.originalTarget);
}

function onFullZoomChange(event) {
  let browser;
  if (event.target.nodeType == event.target.DOCUMENT_NODE) {
    // In non-e10s, the event is dispatched on the contentDocument
    // so we need to jump through some hoops to get to the <xul:browser>.
    let gBrowser = event.currentTarget.gBrowser;
    let topDoc = event.target.defaultView.top.document;
    browser = gBrowser.getBrowserForDocument(topDoc);
  } else {
    browser = event.originalTarget;
  }
  updateZoomUI(browser, true);
}

/**
 * Updates zoom controls.
 *
 * @param {object} aBrowser The browser that the zoomed content resides in.
 * @param {boolean} aAnimate Should be True for all cases unless the zoom
 *   change is related to tab switching. Optional
 */
function updateZoomUI(aBrowser, aAnimate = false) {
  let win = aBrowser.ownerGlobal;
  if (aBrowser != win.gBrowser.selectedBrowser) {
    return;
  }

  let appMenuZoomReset = win.document.getElementById("appMenu-zoomReset-button");
  let customizableZoomControls = win.document.getElementById("zoom-controls");
  let customizableZoomReset = win.document.getElementById("zoom-reset-button");
  let urlbarZoomButton = win.document.getElementById("urlbar-zoom-button");
  let zoomFactor = Math.round(win.ZoomManager.zoom * 100);

  // Hide urlbar zoom button if zoom is at 100% or the customizable control is
  // in the toolbar.
  urlbarZoomButton.hidden =
    (zoomFactor == 100 ||
     (customizableZoomControls &&
      customizableZoomControls.getAttribute("cui-areatype") == "toolbar"));

  let label = win.gNavigatorBundle.getFormattedString("zoom-button.label", [zoomFactor]);
  if (appMenuZoomReset) {
    appMenuZoomReset.setAttribute("label", label);
  }
  if (customizableZoomReset) {
    customizableZoomReset.setAttribute("label", label);
  }
  if (!urlbarZoomButton.hidden) {
    if (aAnimate) {
      urlbarZoomButton.setAttribute("animate", "true");
    } else {
      urlbarZoomButton.removeAttribute("animate");
    }
    urlbarZoomButton.setAttribute("label", label);
  }
}

Components.utils.import("resource:///modules/CustomizableUI.jsm");
let customizationListener = {
  onAreaNodeRegistered(aAreaType, aAreaNode) {
    if (aAreaType == CustomizableUI.AREA_PANEL) {
      updateZoomUI(aAreaNode.ownerGlobal.gBrowser.selectedBrowser);
    }
  }
};
customizationListener.onWidgetAdded =
customizationListener.onWidgetRemoved =
customizationListener.onWidgetMoved = function(aWidgetId) {
  if (aWidgetId == "zoom-controls") {
    for (let window of CustomizableUI.windows) {
      updateZoomUI(window.gBrowser.selectedBrowser);
    }
  }
};
customizationListener.onWidgetReset =
customizationListener.onWidgetUndoMove = function(aWidgetNode) {
  if (aWidgetNode.id == "zoom-controls") {
    updateZoomUI(aWidgetNode.ownerGlobal.gBrowser.selectedBrowser);
  }
};
CustomizableUI.addListener(customizationListener);

