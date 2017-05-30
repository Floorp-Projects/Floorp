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

  /**
   * Pref that controls whether activity stream is enabled
   */
  PREF_ACTIVITY_STREAM_ENABLED: "browser.newtabpage.activity-stream.enabled",

  _activityStreamEnabled: null,

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
    Services.prefs.addObserver(this.PREF_ACTIVITY_STREAM_ENABLED, this);

    this._sslDiskCacheEnabled =
      Services.prefs.getBoolPref(this.PREF_DISK_CACHE_SSL);
    this._activityStreamEnabled =
      Services.prefs.getBoolPref(this.PREF_ACTIVITY_STREAM_ENABLED);

    this._tabEvents.forEach(function(aEvent) {
      gBrowser.tabContainer.addEventListener(aEvent, this);
    }, this);

    this._timeouts = new WeakMap();
  },

  uninit: function Thumbnails_uninit() {
    PageThumbs.removeExpirationFilter(this);
    gBrowser.removeTabsProgressListener(this);
    Services.prefs.removeObserver(this.PREF_DISK_CACHE_SSL, this);
    Services.prefs.removeObserver(this.PREF_ACTIVITY_STREAM_ENABLED, this);

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

  observe: function Thumbnails_observe(subject, topic, data) {
    switch (data) {
      case this.PREF_DISK_CACHE_SSL:
        this._sslDiskCacheEnabled =
          Services.prefs.getBoolPref(this.PREF_DISK_CACHE_SSL);
        break;
      case this.PREF_ACTIVITY_STREAM_ENABLED:
        this._activityStreamEnabled =
          Services.prefs.getBoolPref(this.PREF_ACTIVITY_STREAM_ENABLED);
        // Get the new top sites
        XPCOMUtils.defineLazyGetter(this, "_topSiteURLs", getTopSiteURLs);
        break;
    }
  },

  /**
   * clearTopSiteURLCache is only ever called if we've created an nsITimer,
   * which only happens if we've loaded the tiles top sites. Therefore we only
   * need to clear the tiles top sites (and not activity stream's top sites)
   */
  clearTopSiteURLCache: function Thumbnails_clearTopSiteURLCache() {
    if (this._topSiteURLsRefreshTimer) {
      this._topSiteURLsRefreshTimer.cancel();
      this._topSiteURLsRefreshTimer = null;
    }
    // Delete the defined property
    delete this._topSiteURLs;
    XPCOMUtils.defineLazyGetter(this, "_topSiteURLs", getTopSiteURLs);
  },

  notify: function Thumbnails_notify(timer) {
    gBrowserThumbnails._topSiteURLsRefreshTimer = null;
    gBrowserThumbnails.clearTopSiteURLCache();
  },

  async filterForThumbnailExpiration(aCallback) {
    const topSites = await this._topSiteURLs;
    aCallback(topSites);
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

  async _capture(aBrowser) {
    // Only capture about:newtab top sites.
    const topSites = await this._topSiteURLs;
    if (!aBrowser.currentURI ||
        topSites.indexOf(aBrowser.currentURI.spec) == -1)
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

async function getTopSiteURLs() {
  let sites = [];
  if (gBrowserThumbnails._activityStreamEnabled) {
    sites = await NewTabUtils.activityStreamLinks.getTopSites();
  } else {
    // The _topSiteURLs getter can be expensive to run, but its return value can
    // change frequently on new profiles, so as a compromise we cache its return
    // value as a lazy getter for 1 minute every time it's called.
    gBrowserThumbnails._topSiteURLsRefreshTimer =
      Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    gBrowserThumbnails._topSiteURLsRefreshTimer.initWithCallback(gBrowserThumbnails,
                                                                 60 * 1000,
                                                                 Ci.nsITimer.TYPE_ONE_SHOT);
    sites = NewTabUtils.links.getLinks();
  }
  return sites.reduce((urls, link) => {
    if (link) urls.push(link.url);
    return urls;
  }, []);
}

XPCOMUtils.defineLazyGetter(gBrowserThumbnails, "_topSiteURLs", getTopSiteURLs);
