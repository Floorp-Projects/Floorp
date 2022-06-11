// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["ZoomUI"];
const gLoadContext = Cu.createLoadContext();
const gContentPrefs = Cc["@mozilla.org/content-pref/service;1"].getService(
  Ci.nsIContentPrefService2
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const gZoomPropertyName = "browser.content.full-zoom";

const lazy = {};

ChromeUtils.defineModuleGetter(
  lazy,
  "PanelMultiView",
  "resource:///modules/PanelMultiView.jsm"
);

var ZoomUI = {
  init(aWindow) {
    aWindow.addEventListener("EndSwapDocShells", onEndSwapDocShells, true);
    aWindow.addEventListener("FullZoomChange", onZoomChange);
    aWindow.addEventListener("TextZoomChange", onZoomChange);
    aWindow.addEventListener(
      "unload",
      () => {
        aWindow.removeEventListener(
          "EndSwapDocShells",
          onEndSwapDocShells,
          true
        );
        aWindow.removeEventListener("FullZoomChange", onZoomChange);
        aWindow.removeEventListener("TextZoomChange", onZoomChange);
      },
      { once: true }
    );
  },

  /**
   * Gets the global browser.content.full-zoom content preference.
   *
   * @returns Promise<prefValue>
   *                  Resolves to the preference value (float) when done.
   */
  getGlobalValue() {
    return new Promise(resolve => {
      let cachedVal = gContentPrefs.getCachedGlobal(
        gZoomPropertyName,
        gLoadContext
      );
      if (cachedVal) {
        // We've got cached information, though it may be we've cached
        // an undefined value, or the cached info is invalid. To ensure
        // a valid return, we opt to return the default 1.0 in the
        // undefined and invalid cases.
        resolve(parseFloat(cachedVal.value) || 1.0);
        return;
      }
      // Otherwise, nothing is cached, so we must do a full lookup
      // with  `gContentPrefs.getGlobal()`.
      let value = 1.0;
      gContentPrefs.getGlobal(gZoomPropertyName, gLoadContext, {
        handleResult(pref) {
          if (pref.value) {
            value = parseFloat(pref.value);
          }
        },
        handleCompletion(reason) {
          resolve(value);
        },
        handleError(error) {
          Cu.reportError(error);
        },
      });
    });
  },
};

function fullZoomLocationChangeObserver(aSubject, aTopic) {
  // If the tab was the last one in its window and has been dragged to another
  // window, the original browser's window will be unavailable here. Since that
  // window is closing, we can just ignore this notification.
  if (!aSubject.ownerGlobal) {
    return;
  }
  updateZoomUI(aSubject, false);
}
Services.obs.addObserver(
  fullZoomLocationChangeObserver,
  "browser-fullZoom:location-change"
);

function onEndSwapDocShells(event) {
  updateZoomUI(event.originalTarget);
}

function onZoomChange(event) {
  let browser;
  if (event.target.nodeType == event.target.DOCUMENT_NODE) {
    // In non-e10s, the event is dispatched on the contentDocument
    // so we need to jump through some hoops to get to the <xul:browser>.
    let topDoc = event.target.defaultView.top.document;
    if (!topDoc.documentElement) {
      // In some events, such as loading synthetic documents, the
      // documentElement will be null and we won't be able to find
      // an associated browser.
      return;
    }
    browser = topDoc.ownerGlobal.docShell.chromeEventHandler;
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
async function updateZoomUI(aBrowser, aAnimate = false) {
  let win = aBrowser.ownerGlobal;
  if (!win.gBrowser || win.gBrowser.selectedBrowser != aBrowser) {
    return;
  }

  let appMenuZoomReset = lazy.PanelMultiView.getViewNode(
    win.document,
    "appMenu-zoomReset-button2"
  );

  // Exit early if UI elements aren't present.
  if (!appMenuZoomReset) {
    return;
  }

  let customizableZoomControls = win.document.getElementById("zoom-controls");
  let customizableZoomReset = win.document.getElementById("zoom-reset-button");
  let urlbarZoomButton = win.document.getElementById("urlbar-zoom-button");
  let zoomFactor = Math.round(win.ZoomManager.zoom * 100);

  let defaultZoom = Math.round((await ZoomUI.getGlobalValue()) * 100);

  if (!win.gBrowser || win.gBrowser.selectedBrowser != aBrowser) {
    // Because the CPS call is async, at this point the selected browser
    // may have changed. We should re-check whether the browser for which we've
    // been notified is still the selected browser and bail out if not.
    // If the selected browser changed (again), we will have been called again
    // with the "right" browser, and that'll update the zoom level.
    return;
  }

  // Hide urlbar zoom button if zoom is at the default zoom level,
  // if we're viewing an about:blank page with an empty/null
  // principal, if the PDF viewer is currently open,
  // or if the customizable control is in the toolbar.

  urlbarZoomButton.hidden =
    defaultZoom == zoomFactor ||
    (aBrowser.currentURI.spec == "about:blank" &&
      (!aBrowser.contentPrincipal ||
        aBrowser.contentPrincipal.isNullPrincipal)) ||
    (aBrowser.contentPrincipal &&
      aBrowser.contentPrincipal.spec == "resource://pdf.js/web/viewer.html") ||
    (customizableZoomControls &&
      customizableZoomControls.getAttribute("cui-areatype") == "toolbar");

  let label = win.gNavigatorBundle.getFormattedString("zoom-button.label", [
    zoomFactor,
  ]);
  if (appMenuZoomReset) {
    appMenuZoomReset.setAttribute("label", label);
  }
  if (customizableZoomReset) {
    customizableZoomReset.setAttribute("label", label);
  }
  if (!urlbarZoomButton.hidden) {
    if (aAnimate && !win.gReduceMotion) {
      urlbarZoomButton.setAttribute("animate", "true");
    } else {
      urlbarZoomButton.removeAttribute("animate");
    }
    urlbarZoomButton.setAttribute("label", label);
  }

  win.FullZoom.updateCommands();
}

const { CustomizableUI } = ChromeUtils.import(
  "resource:///modules/CustomizableUI.jsm"
);
let customizationListener = {};
customizationListener.onWidgetAdded = customizationListener.onWidgetRemoved = customizationListener.onWidgetMoved = function(
  aWidgetId
) {
  if (aWidgetId == "zoom-controls") {
    for (let window of CustomizableUI.windows) {
      updateZoomUI(window.gBrowser.selectedBrowser);
    }
  }
};
customizationListener.onWidgetReset = customizationListener.onWidgetUndoMove = function(
  aWidgetNode
) {
  if (aWidgetNode.id == "zoom-controls") {
    updateZoomUI(aWidgetNode.ownerGlobal.gBrowser.selectedBrowser);
  }
};
CustomizableUI.addListener(customizationListener);
