#ifdef 0
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#endif

Cu.import("resource://gre/modules/NewTabUtils.jsm");

/**
 * Keeps thumbnails of open web pages up-to-date.
 */
let gBrowserThumbnails = {
  /**
   * Pref that controls whether we can store SSL content on disk
   */
  PREF_DISK_CACHE_SSL: "browser.cache.disk_cache_ssl",

  _captureDelayMS: 1000,

  /**
   * Used to keep track of disk_cache_ssl preference
   */
  _sslDiskCacheEnabled: null,

  /**
   * Map of capture() timeouts assigned to their browsers.
   */
  _timeouts: null,

  /**
   * List of tab events we want to listen for.
   */
  _tabEvents: ["TabClose", "TabSelect"],

  init: function Thumbnails_init() {
    // Bug 863512 - Make page thumbnails work in electrolysis
    if (gMultiProcessBrowser)
      return;

    PageThumbs.addExpirationFilter(this);
    gBrowser.addTabsProgressListener(this);
    Services.prefs.addObserver(this.PREF_DISK_CACHE_SSL, this, false);

    this._sslDiskCacheEnabled =
      Services.prefs.getBoolPref(this.PREF_DISK_CACHE_SSL);

    this._tabEvents.forEach(function (aEvent) {
      gBrowser.tabContainer.addEventListener(aEvent, this, false);
    }, this);

    this._timeouts = new WeakMap();
  },

  uninit: function Thumbnails_uninit() {
    // Bug 863512 - Make page thumbnails work in electrolysis
    if (gMultiProcessBrowser)
      return;

    PageThumbs.removeExpirationFilter(this);
    gBrowser.removeTabsProgressListener(this);
    Services.prefs.removeObserver(this.PREF_DISK_CACHE_SSL, this);

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

  observe: function Thumbnails_observe() {
    this._sslDiskCacheEnabled =
      Services.prefs.getBoolPref(this.PREF_DISK_CACHE_SSL);
  },

  filterForThumbnailExpiration:
  function Thumbnails_filterForThumbnailExpiration(aCallback) {
    aCallback([browser.currentURI.spec for (browser of gBrowser.browsers)]);
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
      PageThumbs.captureAndStoreIfStale(aBrowser);
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
    // Capture only if it's a top site in about:newtab.
    if (NewTabUtils.links.getLinks().indexOf(aBrowser.currentURI.spec) < 0)
      return false;

    // Capture only if it's the currently selected tab.
    if (aBrowser != gBrowser.selectedBrowser)
      return false;

    // Don't capture in per-window private browsing mode.
    if (PrivateBrowsingUtils.isWindowPrivate(window))
      return false;

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
    let uri = channel.originalURI;
    if (uri.schemeIs("about"))
      return false;

    let httpChannel;
    try {
      httpChannel = channel.QueryInterface(Ci.nsIHttpChannel);
    } catch (e) { /* Not an HTTP channel. */ }

    if (httpChannel) {
      // Continue only if we have a 2xx status code.
      try {
        if (Math.floor(httpChannel.responseStatus / 100) != 2)
          return false;
      } catch (e) {
        // Can't get response information from the httpChannel
        // because mResponseHead is not available.
        return false;
      }

      // Cache-Control: no-store.
      if (httpChannel.isNoStoreResponse())
        return false;

      // Don't capture HTTPS pages unless the user explicitly enabled it.
      if (uri.schemeIs("https") && !this._sslDiskCacheEnabled)
        return false;
    }

    return true;
  },

  _clearTimeout: function Thumbnails_clearTimeout(aBrowser) {
    if (this._timeouts.has(aBrowser)) {
      aBrowser.removeEventListener("scroll", this, false);
      clearTimeout(this._timeouts.get(aBrowser));
      this._timeouts.delete(aBrowser);
    }
  }
};
