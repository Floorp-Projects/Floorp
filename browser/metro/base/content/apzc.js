// -*- Mode: js2; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let Cc = Components.classes;
let Ci = Components.interfaces;
let Cu = Components.utils;
let Cr = Components.results;

/**
 * Misc. front end utilities for apzc management.
 * the pref: layers.async-pan-zoom.enabled is true.
 */

var APZCObserver = {
  _debugEvents: false,
  _enabled: false,

  get enabled() {
    return this._enabled;
  },

  init: function() {
    this._enabled = Services.prefs.getBoolPref(kAsyncPanZoomEnabled);
    if (!this._enabled) {
      return;
    }

    let os = Services.obs;
    os.addObserver(this, "apzc-handle-pan-begin", false);
    os.addObserver(this, "apzc-handle-pan-end", false);

    // Fired by ContentAreaObserver
    window.addEventListener("SizeChanged", this, true);

    Elements.tabList.addEventListener("TabSelect", this, true);
    Elements.tabList.addEventListener("TabOpen", this, true);
    Elements.tabList.addEventListener("TabClose", this, true);
  },

  shutdown: function shutdown() {
    if (!this._enabled) {
      return;
    }

    let os = Services.obs;
    os.removeObserver(this, "apzc-handle-pan-begin");
    os.removeObserver(this, "apzc-handle-pan-end");

    window.removeEventListener("SizeChanged", this, true);

    Elements.tabList.removeEventListener("TabSelect", this, true);
    Elements.tabList.removeEventListener("TabOpen", this, true);
    Elements.tabList.removeEventListener("TabClose", this, true);
  },

  handleEvent: function APZC_handleEvent(aEvent) {
    switch (aEvent.type) {
      case "SizeChanged":
      case 'TabSelect':
        this._resetDisplayPort();
        break;

      case 'pageshow':
        if (aEvent.target != Browser.selectedBrowser.contentDocument) {
          break;
        }
        this._resetDisplayPort();
        break;

      case 'TabOpen': {
        let browser = aEvent.originalTarget.linkedBrowser;
        browser.addEventListener("pageshow", this, true);
        // Register for notifications from content about scroll actions.
        browser.messageManager.addMessageListener("Browser:ContentScroll", this);
        break;
      }
      case 'TabClose': {
        let browser = aEvent.originalTarget.linkedBrowser;
        browser.removeEventListener("pageshow", this, true);
        browser.messageManager.removeMessageListener("Browser:ContentScroll", this);
        break;
      }
    }
  },

  observe: function ao_observe(aSubject, aTopic, aData) {
    if (aTopic == "apzc-handle-pan-begin") {
      // When we're panning, hide the main scrollbars by setting imprecise
      // input (which sets a property on the browser which hides the scrollbar
      // via CSS).  This reduces jittering from left to right. We may be able
      // to get rid of this once we implement axis locking in /gfx APZC.
      if (InputSourceHelper.isPrecise) {
        InputSourceHelper._imprecise();
      }
    }
  },

  receiveMessage: function(aMessage) {
    let json = aMessage.json;
    switch (aMessage.name) {
       // Content notifies us here (syncronously) if it has scrolled
       // independent of the apz. This can happen in a lot of
       // cases: keyboard shortcuts, scroll wheel, or content script.
       // Let the apz know about this change so that it can update
       // its scroll offset data.
      case "Browser:ContentScroll": {
        let data = json.viewId + " " + json.presShellId + " (" + json.scrollOffset.x + ", " + json.scrollOffset.y + ")";
        Services.obs.notifyObservers(null, "apzc-scroll-offset-changed", data);
        break;
      }
    }
  },

  _resetDisplayPort: function () {
    // Start off with something reasonable. The apzc will handle these
    // calculations once scrolling starts.
    let doc = Browser.selectedBrowser.contentDocument.documentElement;
    // While running tests, sometimes this can be null. If we don't have a
    // root document, there's no point in setting a scrollable display port.
    if (!doc) {
      return;
    }
    let win = Browser.selectedBrowser.contentWindow;
    let factor = 0.2;
    let portX = 0;
    let portY = 0;
    let portWidth = ContentAreaObserver.width;
    let portHeight = ContentAreaObserver.height;

    if (portWidth < doc.scrollWidth) {
      portWidth += ContentAreaObserver.width * factor;
      if (portWidth > doc.scrollWidth) {
        portWidth = doc.scrollWidth;
      }
    }
    if (portHeight < doc.scrollHeight) {
      portHeight += ContentAreaObserver.height * factor;
      if (portHeight > doc.scrollHeight) {
        portHeight = doc.scrollHeight;
      }
    }
    if (win.scrollX > 0) {
      portX -= ContentAreaObserver.width * factor;
    }
    if (win.scrollY > 0) {
      portY -= ContentAreaObserver.height * factor;
    }
    let cwu = Browser.selectedBrowser.contentWindow
                      .QueryInterface(Ci.nsIInterfaceRequestor)
                      .getInterface(Ci.nsIDOMWindowUtils);
    cwu.setDisplayPortForElement(portX, portY,
                                 portWidth, portHeight,
                                 Browser.selectedBrowser.contentDocument.documentElement);
  }
};
