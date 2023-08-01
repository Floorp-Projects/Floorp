/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file is loaded into the browser window scope.
/* eslint-env mozilla/browser-window */

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
    gBrowser.addTabsProgressListener(this);
    Services.prefs.addObserver(this.PREF_DISK_CACHE_SSL, this);

    this._sslDiskCacheEnabled = Services.prefs.getBoolPref(
      this.PREF_DISK_CACHE_SSL
    );

    this._tabEvents.forEach(function (aEvent) {
      gBrowser.tabContainer.addEventListener(aEvent, this);
    }, this);

    this._timeouts = new WeakMap();
  },

  uninit: function Thumbnails_uninit() {
    gBrowser.removeTabsProgressListener(this);
    Services.prefs.removeObserver(this.PREF_DISK_CACHE_SSL, this);

    if (this._topSiteURLsRefreshTimer) {
      this._topSiteURLsRefreshTimer.cancel();
      this._topSiteURLsRefreshTimer = null;
    }

    this._tabEvents.forEach(function (aEvent) {
      gBrowser.tabContainer.removeEventListener(aEvent, this);
    }, this);
  },

  handleEvent: function Thumbnails_handleEvent(aEvent) {
    switch (aEvent.type) {
      case "scroll":
        let browser = aEvent.currentTarget;
        if (this._timeouts.has(browser)) {
          this._delayedCapture(browser);
        }
        break;
      case "TabSelect":
        this._delayedCapture(aEvent.target.linkedBrowser);
        break;
      case "TabClose": {
        this._cancelDelayedCapture(aEvent.target.linkedBrowser);
        break;
      }
    }
  },

  observe: function Thumbnails_observe(subject, topic, data) {
    switch (data) {
      case this.PREF_DISK_CACHE_SSL:
        this._sslDiskCacheEnabled = Services.prefs.getBoolPref(
          this.PREF_DISK_CACHE_SSL
        );
        break;
    }
  },

  clearTopSiteURLCache: function Thumbnails_clearTopSiteURLCache() {
    if (this._topSiteURLsRefreshTimer) {
      this._topSiteURLsRefreshTimer.cancel();
      this._topSiteURLsRefreshTimer = null;
    }
    // Delete the defined property
    delete this._topSiteURLs;
    ChromeUtils.defineLazyGetter(this, "_topSiteURLs", getTopSiteURLs);
  },

  notify: function Thumbnails_notify(timer) {
    gBrowserThumbnails._topSiteURLsRefreshTimer = null;
    gBrowserThumbnails.clearTopSiteURLCache();
  },

  /**
   * State change progress listener for all tabs.
   */
  onStateChange: function Thumbnails_onStateChange(
    aBrowser,
    aWebProgress,
    aRequest,
    aStateFlags,
    aStatus
  ) {
    if (
      aStateFlags & Ci.nsIWebProgressListener.STATE_STOP &&
      aStateFlags & Ci.nsIWebProgressListener.STATE_IS_NETWORK
    ) {
      this._delayedCapture(aBrowser);
    }
  },

  async _capture(aBrowser) {
    // Only capture about:newtab top sites.
    const topSites = await this._topSiteURLs;
    if (!aBrowser.currentURI || !topSites.includes(aBrowser.currentURI.spec)) {
      return;
    }
    if (await this._shouldCapture(aBrowser)) {
      await PageThumbs.captureAndStoreIfStale(aBrowser);
    }
  },

  _delayedCapture: function Thumbnails_delayedCapture(aBrowser) {
    if (this._timeouts.has(aBrowser)) {
      this._cancelDelayedCallbacks(aBrowser);
    } else {
      aBrowser.addEventListener("scroll", this, true);
    }

    let idleCallback = () => {
      this._cancelDelayedCapture(aBrowser);
      this._capture(aBrowser);
    };

    // setTimeout to set a guarantee lower bound for the requestIdleCallback
    // (and therefore the delayed capture)
    let timeoutId = setTimeout(() => {
      let idleCallbackId = requestIdleCallback(idleCallback, {
        timeout: this._captureDelayMS * 30,
      });
      this._timeouts.set(aBrowser, { isTimeout: false, id: idleCallbackId });
    }, this._captureDelayMS);

    this._timeouts.set(aBrowser, { isTimeout: true, id: timeoutId });
  },

  _shouldCapture: async function Thumbnails_shouldCapture(aBrowser) {
    // Capture only if it's the currently selected tab and not an about: page.
    if (
      aBrowser != gBrowser.selectedBrowser ||
      gBrowser.currentURI.schemeIs("about")
    ) {
      return false;
    }
    return PageThumbs.shouldStoreThumbnail(aBrowser);
  },

  _cancelDelayedCapture: function Thumbnails_cancelDelayedCapture(aBrowser) {
    if (this._timeouts.has(aBrowser)) {
      aBrowser.removeEventListener("scroll", this);
      this._cancelDelayedCallbacks(aBrowser);
      this._timeouts.delete(aBrowser);
    }
  },

  _cancelDelayedCallbacks: function Thumbnails_cancelDelayedCallbacks(
    aBrowser
  ) {
    let timeoutData = this._timeouts.get(aBrowser);

    if (timeoutData.isTimeout) {
      clearTimeout(timeoutData.id);
    } else {
      // idle callback dispatched
      window.cancelIdleCallback(timeoutData.id);
    }
  },
};

async function getTopSiteURLs() {
  // The _topSiteURLs getter can be expensive to run, but its return value can
  // change frequently on new profiles, so as a compromise we cache its return
  // value as a lazy getter for 1 minute every time it's called.
  gBrowserThumbnails._topSiteURLsRefreshTimer = Cc[
    "@mozilla.org/timer;1"
  ].createInstance(Ci.nsITimer);
  gBrowserThumbnails._topSiteURLsRefreshTimer.initWithCallback(
    gBrowserThumbnails,
    60 * 1000,
    Ci.nsITimer.TYPE_ONE_SHOT
  );
  let sites = [];
  // Get both the top sites returned by the query, and also any pinned sites
  // that the user might have added manually that also need a screenshot.
  // Also include top sites that don't have rich icons
  let topSites = await NewTabUtils.activityStreamLinks.getTopSites();
  sites.push(...topSites.filter(link => !(link.faviconSize >= 96)));
  sites.push(...NewTabUtils.pinnedLinks.links);
  return sites.reduce((urls, link) => {
    if (link) {
      urls.push(link.url);
    }
    return urls;
  }, []);
}

ChromeUtils.defineLazyGetter(
  gBrowserThumbnails,
  "_topSiteURLs",
  getTopSiteURLs
);
