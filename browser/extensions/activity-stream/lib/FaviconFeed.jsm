/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {utils: Cu} = Components;
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

Cu.importGlobalProperties(["fetch", "URL"]);

const {actionTypes: at} = Cu.import("resource://activity-stream/common/Actions.jsm", {});
const {PersistentCache} = Cu.import("resource://activity-stream/lib/PersistentCache.jsm", {});
const {getDomain} = Cu.import("resource://activity-stream/lib/TippyTopProvider.jsm", {});

XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
  "resource://gre/modules/PlacesUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Services",
  "resource://gre/modules/Services.jsm");

const FIVE_MINUTES = 5 * 60 * 1000;
const ONE_DAY = 24 * 60 * 60 * 1000;
const TIPPYTOP_UPDATE_TIME = ONE_DAY;
const TIPPYTOP_RETRY_DELAY = FIVE_MINUTES;

this.FaviconFeed = class FaviconFeed {
  constructor() {
    this.tippyTopNextUpdate = 0;
    this.cache = new PersistentCache("tippytop", true);
    this._sitesByDomain = null;
    this.numRetries = 0;
  }

  get endpoint() {
    return this.store.getState().Prefs.values["tippyTop.service.endpoint"];
  }

  async loadCachedData() {
    const data = await this.cache.get("sites");
    if (data && "_timestamp" in data) {
      this._sitesByDomain = data;
      this.tippyTopNextUpdate = data._timestamp + TIPPYTOP_UPDATE_TIME;
    }
  }

  async maybeRefresh() {
    if (Date.now() >= this.tippyTopNextUpdate) {
      await this.refresh();
    }
  }

  async refresh() {
    let headers = new Headers();
    if (this._sitesByDomain && this._sitesByDomain._etag) {
      headers.set("If-None-Match", this._sitesByDomain._etag);
    }
    let {data, etag, status} = await this.loadFromURL(this.endpoint, headers);
    let failedUpdate = false;
    if (status === 200) {
      this._sitesByDomain = this._sitesArrayToObjectByDomain(data);
      this._sitesByDomain._etag = etag;
    } else if (status !== 304) {
      failedUpdate = true;
    }
    let delay = TIPPYTOP_UPDATE_TIME;
    if (failedUpdate) {
      delay = Math.min(TIPPYTOP_UPDATE_TIME, TIPPYTOP_RETRY_DELAY * Math.pow(2, this.numRetries++));
    } else {
      this._sitesByDomain._timestamp = Date.now();
      this.cache.set("sites", this._sitesByDomain);
      this.numRetries = 0;
    }
    this.tippyTopNextUpdate = Date.now() + delay;
  }

  async loadFromURL(url, headers) {
    let data = [];
    let etag;
    let status;
    try {
      let response = await fetch(url, {headers});
      status = response.status;
      if (status === 200) {
        data = await response.json();
        etag = response.headers.get("ETag");
      }
    } catch (error) {
      Cu.reportError(`Failed to load tippy top manifest from ${url}`);
    }
    return {data, etag, status};
  }

  _sitesArrayToObjectByDomain(sites) {
    let sitesByDomain = {};
    for (const site of sites) {
      // The tippy top manifest can have a url property (string) or a
      // urls property (array of strings)
      for (const domain of site.domains || []) {
        sitesByDomain[domain] = {image_url: site.image_url};
      }
    }
    return sitesByDomain;
  }

  getSitesByDomain() {
    // return an already loaded object or a promise for that object
    return this._sitesByDomain || (this._sitesByDomain = new Promise(async resolve => {
      await this.loadCachedData();
      await this.maybeRefresh();
      if (this._sitesByDomain instanceof Promise) {
        // If _sitesByDomain is still a Promise, no data was loaded from cache or fetch.
        this._sitesByDomain = {};
      }
      resolve(this._sitesByDomain);
    }));
  }

  async fetchIcon(url) {
    // Avoid initializing and fetching icons if prefs are turned off
    if (!this.shouldFetchIcons) {
      return;
    }

    const sitesByDomain = await this.getSitesByDomain();
    const domain = getDomain(url);
    if (domain in sitesByDomain) {
      let iconUri = Services.io.newURI(sitesByDomain[domain].image_url);
      // The #tippytop is to be able to identify them for telemetry.
      iconUri.ref = "tippytop";
      PlacesUtils.favicons.setAndFetchFaviconForPage(
        Services.io.newURI(url),
        iconUri,
        false,
        PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE,
        null,
        Services.scriptSecurityManager.getSystemPrincipal()
      );
    }
  }

  /**
   * Determine if we should be fetching and saving icons.
   */
  get shouldFetchIcons() {
    return this.endpoint && Services.prefs.getBoolPref("browser.chrome.site_icons");
  }

  onAction(action) {
    switch (action.type) {
      case at.SYSTEM_TICK:
        if (this._sitesByDomain) {
          // No need to refresh if we haven't been initialized.
          this.maybeRefresh();
        }
        break;
      case at.RICH_ICON_MISSING:
        this.fetchIcon(action.data.url);
        break;
    }
  }
};

this.EXPORTED_SYMBOLS = ["FaviconFeed"];
