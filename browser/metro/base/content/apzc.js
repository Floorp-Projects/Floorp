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
    os.addObserver(this, "apzc-transform-begin", false);

    Elements.tabList.addEventListener("TabSelect", this, true);
    Elements.browsers.addEventListener("pageshow", this, true);
    messageManager.addMessageListener("Content:ZoomToRect", this);
  },

  shutdown: function shutdown() {
    if (!this._enabled) {
      return;
    }

    let os = Services.obs;
    os.removeObserver(this, "apzc-transform-begin");

    Elements.tabList.removeEventListener("TabSelect", this, true);
    Elements.browsers.removeEventListener("pageshow", this, true);
    messageManager.removeMessageListener("Content:ZoomToRect", this);
  },

  handleEvent: function APZC_handleEvent(aEvent) {
    switch (aEvent.type) {
      case 'TabSelect':
        this._resetDisplayPort();
        break;

      case 'pageshow':
        if (aEvent.target != Browser.selectedBrowser.contentDocument) {
          break;
        }
        this._resetDisplayPort();
        break;
    }
  },

  observe: function ao_observe(aSubject, aTopic, aData) {
    if (aTopic == "apzc-transform-begin") {
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
    let browser = aMessage.target;
    switch (aMessage.name) {
      case "Content:ZoomToRect": {
        let { presShellId, viewId } = json;
        let rect = Rect.fromRect(json.rect);
        if (this.isRectZoomedIn(rect, browser.contentViewportBounds)) {
          // If we're already zoomed in, zoom out instead.
          rect = new Rect(0,0,0,0);
        }
        let data = [rect.x, rect.y, rect.width, rect.height, presShellId, viewId].join(",");
        Services.obs.notifyObservers(null, "apzc-zoom-to-rect", data);
      }
    }
  },

  /**
   * Check to see if the area of the rect visible in the viewport is
   * approximately the max area of the rect we can show.
   * Based on code from BrowserElementPanning.js
   */
  isRectZoomedIn: function (aRect, aViewport) {
    let overlap = aViewport.intersect(aRect);
    let overlapArea = overlap.width * overlap.height;
    let availHeight = Math.min(aRect.width * aViewport.height / aViewport.width, aRect.height);
    let showing = overlapArea / (aRect.width * availHeight);
    let ratioW = (aRect.width / aViewport.width);
    let ratioH = (aRect.height / aViewport.height);

    return (showing > 0.9 && (ratioW > 0.9 || ratioH > 0.9));
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
                                 Browser.selectedBrowser.contentDocument.documentElement,
                                 0);
  }
};
