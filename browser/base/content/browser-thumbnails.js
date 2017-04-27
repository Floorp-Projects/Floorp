/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Keeps thumbnails of open web pages up-to-date.
 */
var gBrowserThumbnails = {
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
   * Top site URLs refresh timer.
   */
  _topSiteURLsRefreshTimer: null,

  /**
   * List of tab events we want to listen for.
   */
  _tabEvents: ["TabClose", "TabSelect"],

  init: function Thumbnails_init() {
    PageThumbs.addExpirationFilter(this);
    gBrowser.addTabsProgressListener(this);
    Services.prefs.addObserver(this.PREF_DISK_CACHE_SSL, this);

    this._sslDiskCacheEnabled =
      Services.prefs.getBoolPref(this.PREF_DISK_CACHE_SSL);

    this._tabEvents.forEach(function(aEvent) {
      gBrowser.tabContainer.addEventListener(aEvent, this);
    }, this);

    this._timeouts = new WeakMap();
  },

  uninit: function Thumbnails_uninit() {
    PageThumbs.removeExpirationFilter(this);
    gBrowser.removeTabsProgressListener(this);
    Services.prefs.removeObserver(this.PREF_DISK_CACHE_SSL, this);
    if (this._topSiteURLsRefreshTimer) {
      this._topSiteURLsRefreshTimer.cancel();
      this._topSiteURLsRefreshTimer = null;
    }

    this._tabEvents.forEach(function(aEvent) {
      gBrowser.tabContainer.removeEventListener(aEvent, this);
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

  clearTopSiteURLCache: function Thumbnails_clearTopSiteURLCache() {
    if (this._topSiteURLsRefreshTimer) {
      this._topSiteURLsRefreshTimer.cancel();
      this._topSiteURLsRefreshTimer = null;
    }
    // Delete the defined property
    delete this._topSiteURLs;
    XPCOMUtils.defineLazyGetter(this, "_topSiteURLs",
                                Thumbnails_getTopSiteURLs);
  },

  notify: function Thumbnails_notify(timer) {
    gBrowserThumbnails._topSiteURLsRefreshTimer = null;
    gBrowserThumbnails.clearTopSiteURLCache();
  },

  filterForThumbnailExpiration:
  function Thumbnails_filterForThumbnailExpiration(aCallback) {
    aCallback(this._topSiteURLs);
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
    // Only capture about:newtab top sites.
    if (!aBrowser.currentURI ||
        this._topSiteURLs.indexOf(aBrowser.currentURI.spec) == -1)
      return;
    this._shouldCapture(aBrowser, function(aResult) {
      if (aResult) {
        PageThumbs.captureAndStoreIfStale(aBrowser);
      }
    });
  },

  _delayedCapture: function Thumbnails_delayedCapture(aBrowser) {
    if (this._timeouts.has(aBrowser))
      clearTimeout(this._timeouts.get(aBrowser));
    else
      aBrowser.addEventListener("scroll", this, true);

    let timeout = setTimeout(() => {
      this._clearTimeout(aBrowser);
      this._capture(aBrowser);
    }, this._captureDelayMS);

    this._timeouts.set(aBrowser, timeout);
  },

  _shouldCapture: function Thumbnails_shouldCapture(aBrowser, aCallback) {
    // Capture only if it's the currently selected tab.
    if (aBrowser != gBrowser.selectedBrowser) {
      aCallback(false);
      return;
    }
    PageThumbs.shouldStoreThumbnail(aBrowser, aCallback);
  },

  _clearTimeout: function Thumbnails_clearTimeout(aBrowser) {
    if (this._timeouts.has(aBrowser)) {
      aBrowser.removeEventListener("scroll", this);
      clearTimeout(this._timeouts.get(aBrowser));
      this._timeouts.delete(aBrowser);
    }
  }
};

function Thumbnails_getTopSiteURLs() {
  // The _topSiteURLs getter can be expensive to run, but its return value can
  // change frequently on new profiles, so as a compromise we cache its return
  // value as a lazy getter for 1 minute every time it's called.
  gBrowserThumbnails._topSiteURLsRefreshTimer =
    Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
  gBrowserThumbnails._topSiteURLsRefreshTimer.initWithCallback(gBrowserThumbnails,
                                                               60 * 1000,
                                                               Ci.nsITimer.TYPE_ONE_SHOT);
  return NewTabUtils.links.getLinks().reduce((urls, link) => {
    if (link)
      urls.push(link.url);
    return urls;
  }, []);
}

XPCOMUtils.defineLazyGetter(gBrowserThumbnails, "_topSiteURLs",
                            Thumbnails_getTopSiteURLs);
