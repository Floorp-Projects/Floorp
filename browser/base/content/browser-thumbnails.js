#ifdef 0
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#endif

/**
 * Keeps thumbnails of open web pages up-to-date.
 */
let gBrowserThumbnails = {
  _captureDelayMS: 2000,

  /**
   * Map of capture() timeouts assigned to their browsers.
   */
  _timeouts: null,

  /**
   * Cache for the PageThumbs module.
   */
  _pageThumbs: null,

  /**
   * List of tab events we want to listen for.
   */
  _tabEvents: ["TabClose", "TabSelect"],

  init: function Thumbnails_init() {
    gBrowser.addTabsProgressListener(this);

    this._tabEvents.forEach(function (aEvent) {
      gBrowser.tabContainer.addEventListener(aEvent, this, false);
    }, this);

    this._timeouts = new WeakMap();

    XPCOMUtils.defineLazyModuleGetter(this, "_pageThumbs",
      "resource:///modules/PageThumbs.jsm", "PageThumbs");
  },

  uninit: function Thumbnails_uninit() {
    gBrowser.removeTabsProgressListener(this);

    this._tabEvents.forEach(function (aEvent) {
      gBrowser.tabContainer.removeEventListener(aEvent, this, false);
    }, this);
  },

  handleEvent: function Thumbnails_handleEvent(aEvent) {
    switch (aEvent.type) {
      case "scroll":
        let browser = aEvent.currentTarget;
        if (this._timeouts.has(browser))
          this._delayedCapture(browser);
        break;
      case "TabSelect":
        this._delayedCapture(aEvent.target.linkedBrowser);
        break;
      case "TabClose": {
        this._clearTimeout(aEvent.target.linkedBrowser);
        break;
      }
    }
  },

  /**
   * State change progress listener for all tabs.
   */
  onStateChange: function Thumbnails_onStateChange(aBrowser, aWebProgress,
                                                   aRequest, aStateFlags, aStatus) {
    if (aStateFlags & Ci.nsIWebProgressListener.STATE_STOP &&
        aStateFlags & Ci.nsIWebProgressListener.STATE_IS_NETWORK)
      this._delayedCapture(aBrowser);
  },

  _capture: function Thumbnails_capture(aBrowser) {
    if (this._shouldCapture(aBrowser))
      this._pageThumbs.captureAndStore(aBrowser);
  },

  _delayedCapture: function Thumbnails_delayedCapture(aBrowser) {
    if (this._timeouts.has(aBrowser))
      clearTimeout(this._timeouts.get(aBrowser));
    else
      aBrowser.addEventListener("scroll", this, true);

    let timeout = setTimeout(function () {
      this._clearTimeout(aBrowser);
      this._capture(aBrowser);
    }.bind(this), this._captureDelayMS);

    this._timeouts.set(aBrowser, timeout);
  },

  _shouldCapture: function Thumbnails_shouldCapture(aBrowser) {
    let doc = aBrowser.contentDocument;

    // FIXME Bug 720575 - Don't capture thumbnails for SVG or XML documents as
    //       that currently regresses Talos SVG tests.
    if (doc instanceof SVGDocument || doc instanceof XMLDocument)
      return false;

    // There's no point in taking screenshot of loading pages.
    if (aBrowser.docShell.busyFlags != Ci.nsIDocShell.BUSY_FLAGS_NONE)
      return false;

    // Don't take screenshots of about: pages.
    if (aBrowser.currentURI.schemeIs("about"))
      return false;

    let channel = aBrowser.docShell.currentDocumentChannel;

    // No valid document channel. We shouldn't take a screenshot.
    if (!channel)
      return false;

    // Don't take screenshots of internally redirecting about: pages.
    // This includes error pages.
    if (channel.originalURI.schemeIs("about"))
      return false;

    try {
      // If the channel is a nsIHttpChannel get its http status code.
      let httpChannel = channel.QueryInterface(Ci.nsIHttpChannel);

      // Continue only if we have a 2xx status code.
      return Math.floor(httpChannel.responseStatus / 100) == 2;
    } catch (e) {
      // Not a http channel, we just assume a success status code.
      return true;
    }
  },

  _clearTimeout: function Thumbnails_clearTimeout(aBrowser) {
    if (this._timeouts.has(aBrowser)) {
      aBrowser.removeEventListener("scroll", this, false);
      clearTimeout(this._timeouts.get(aBrowser));
      this._timeouts.delete(aBrowser);
    }
  }
};
