// -*- Mode: js2; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let Cc = Components.classes;
let Ci = Components.interfaces;
let Cu = Components.utils;
let Cr = Components.results;

/**
 * Handler for APZC display port and pan begin/end notifications.
 * These notifications are only sent by widget/windows/winrt code when
 * the pref: layers.async-pan-zoom.enabled is true.
 */

var APZCObserver = {
  init: function() {
    this._enabled = Services.prefs.getBoolPref(kAsyncPanZoomEnabled);
    if (!this._enabled) {
      return;
    }

    let os = Services.obs;
    os.addObserver(this, "apzc-request-content-repaint", false);
    os.addObserver(this, "apzc-handle-pan-begin", false);
    os.addObserver(this, "apzc-handle-pan-end", false);

    Elements.tabList.addEventListener("TabSelect", this, true);
    Elements.tabList.addEventListener("TabOpen", this, true);
    Elements.tabList.addEventListener("TabClose", this, true);
  },

  handleEvent: function APZC_handleEvent(aEvent) {
    switch (aEvent.type) {
      case 'pageshow':
      case 'TabSelect':
        Services.obs.notifyObservers(null, "viewport-needs-updating", null);
        break;
      case 'TabOpen': {
        let browser = aEvent.originalTarget.linkedBrowser;
        browser.addEventListener("pageshow", this, true);
        break;
      }
      case 'TabClose': {
        let browser = aEvent.originalTarget.linkedBrowser;
        browser.removeEventListener("pageshow", this);
        break;
      }
    }
  },
  shutdown: function shutdown() {
    if (!this._enabled) {
      return;
    }
    Elements.tabList.removeEventListener("TabSelect", this, true);
    Elements.tabList.removeEventListener("TabOpen", this, true);
    Elements.tabList.removeEventListener("TabClose", this, true);

    let os = Services.obs;
    os.removeObserver(this, "apzc-request-content-repaint");
    os.removeObserver(this, "apzc-handle-pan-begin");
    os.removeObserver(this, "apzc-handle-pan-end");
  },
  observe: function ao_observe(aSubject, aTopic, aData) {
    if (aTopic == "apzc-request-content-repaint") {
      let frameMetrics = JSON.parse(aData);
      let scrollTo = frameMetrics.scrollTo;
      let displayPort = frameMetrics.displayPort;
      let resolution = frameMetrics.resolution;
      let compositedRect = frameMetrics.compositedRect;

      if (StartUI.isStartPageVisible) {
        let windowUtils = Browser.windowUtils;
        Browser.selectedBrowser.contentWindow.scrollTo(scrollTo.x, scrollTo.y);
        windowUtils.setResolution(resolution, resolution);
        windowUtils.setDisplayPortForElement(displayPort.x * resolution,
                                             displayPort.y * resolution,
                                             displayPort.width * resolution,
                                             displayPort.height * resolution,
                                             Elements.startUI);
      } else {
        let windowUtils = Browser.selectedBrowser.contentWindow.
                                  QueryInterface(Ci.nsIInterfaceRequestor).
                                  getInterface(Ci.nsIDOMWindowUtils);
        windowUtils.setScrollPositionClampingScrollPortSize(compositedRect.width,
                                                            compositedRect.height);
        Browser.selectedBrowser.messageManager.sendAsyncMessage("Content:SetCacheViewport", {
          scrollX: scrollTo.x,
          scrollY: scrollTo.y,
          x: displayPort.x + scrollTo.x,
          y: displayPort.y + scrollTo.y,
          w: displayPort.width,
          h: displayPort.height,
          scale: resolution,
          id: 1
        });
      }

      Util.dumpLn("APZC scrollTo.x: " + scrollTo.x + ", scrollTo.y: " + scrollTo.y);
      Util.dumpLn("APZC setResolution: " + resolution);
      Util.dumpLn("APZC setDisplayPortForElement: displayPort.x: " +
                  displayPort.x + ", displayPort.y: " + displayPort.y +
                  ", displayPort.width: " + displayPort.width +
                  ", displayort.height: " + displayPort.height);
    } else if (aTopic == "apzc-handle-pan-begin") {
      // When we're panning, hide the main scrollbars by setting imprecise
      // input (which sets a property on the browser which hides the scrollbar
      // via CSS).  This reduces jittering from left to right. We may be able
      // to get rid of this once we implement axis locking in /gfx APZC.
      Util.dumpLn("APZC pan-begin");
      if (InputSourceHelper.isPrecise) {
        InputSourceHelper._imprecise();
      }

    } else if (aTopic == "apzc-handle-pan-end") {
      Util.dumpLn("APZC pan-end");
    }
  }
};
