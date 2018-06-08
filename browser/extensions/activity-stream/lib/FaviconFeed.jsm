/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyGlobalGetters(this, ["fetch"]);

const {actionTypes: at} = ChromeUtils.import("resource://activity-stream/common/Actions.jsm", {});
const {PersistentCache} = ChromeUtils.import("resource://activity-stream/lib/PersistentCache.jsm", {});
const {getDomain} = ChromeUtils.import("resource://activity-stream/lib/TippyTopProvider.jsm", {});

ChromeUtils.defineModuleGetter(this, "PlacesUtils",
  "resource://gre/modules/PlacesUtils.jsm");
ChromeUtils.defineModuleGetter(this, "Services",
  "resource://gre/modules/Services.jsm");
ChromeUtils.defineModuleGetter(this, "NewTabUtils",
  "resource://gre/modules/NewTabUtils.jsm");

const FIVE_MINUTES = 5 * 60 * 1000;
const ONE_DAY = 24 * 60 * 60 * 1000;
const TIPPYTOP_UPDATE_TIME = ONE_DAY;
const TIPPYTOP_RETRY_DELAY = FIVE_MINUTES;
const MIN_FAVICON_SIZE = 96;

/**
 * Get favicon info (uri and size) for a uri from Places.
 *
 * @param uri {nsIURI} Page to check for favicon data
 * @returns A promise of an object (possibly null) containing the data
 */
function getFaviconInfo(uri) {
  // Use 0 to get the biggest width available
  const preferredWidth = 0;
  return new Promise(resolve => PlacesUtils.favicons.getFaviconDataForPage(
    uri,
    // Package up the icon data in an object if we have it; otherwise null
    (iconUri, faviconLength, favicon, mimeType, faviconSize) =>
      resolve(iconUri ? {iconUri, faviconSize} : null),
    preferredWidth));
}

/**
 * Fetches visit paths for a given URL from its most recent visit in Places.
 *
 * Note that this includes the URL itself as well as all the following
 * permenent&temporary redirected URLs if any.
 *
 * @param {String} a URL string
 *
 * @returns {Array} Returns an array containing objects as
 *   {int}    visit_id: ID of the visit in moz_historyvisits.
 *   {String} url: URL of the redirected URL.
 */
async function fetchVisitPaths(url) {
  const query = `
    WITH RECURSIVE path(visit_id)
    AS (
      SELECT v.id
      FROM moz_places h
      JOIN moz_historyvisits v
        ON v.place_id = h.id
      WHERE h.url_hash = hash(:url) AND h.url = :url
        AND v.visit_date = h.last_visit_date

      UNION

      SELECT id
      FROM moz_historyvisits
      JOIN path
        ON visit_id = from_visit
      WHERE visit_type IN
        (${PlacesUtils.history.TRANSITIONS.REDIRECT_PERMANENT},
         ${PlacesUtils.history.TRANSITIONS.REDIRECT_TEMPORARY})
    )
    SELECT visit_id, (
      SELECT (
        SELECT url
        FROM moz_places
        WHERE id = place_id)
      FROM moz_historyvisits
      WHERE id = visit_id) AS url
    FROM path
  `;

  const visits = await NewTabUtils.activityStreamProvider.executePlacesQuery(query, {
    columns: ["visit_id", "url"],
    params: {url}
  });
  return visits;
}

/**
 * Fetch favicon for a url by following its redirects in Places.
 *
 * This can improve the rich icon coverage for Top Sites since Places only
 * associates the favicon to the final url if the original one gets redirected.
 * Note this is not an urgent request, hence it is dispatched to the main
 * thread idle handler to avoid any possible performance impact.
 */
async function fetchIconFromRedirects(url) {
  const visitPaths = await fetchVisitPaths(url);
  if (visitPaths.length > 1) {
    const lastVisit = visitPaths.pop();
    const redirectedUri = Services.io.newURI(lastVisit.url);
    const iconInfo = await getFaviconInfo(redirectedUri);
    if (iconInfo && iconInfo.faviconSize >= MIN_FAVICON_SIZE) {
      PlacesUtils.favicons.setAndFetchFaviconForPage(
        Services.io.newURI(url),
        iconInfo.iconUri,
        false,
        PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE,
        null,
        Services.scriptSecurityManager.getSystemPrincipal()
      );
    }
  }
}

this.FaviconFeed = class FaviconFeed {
  constructor() {
    this.tippyTopNextUpdate = 0;
    this.cache = new PersistentCache("tippytop", true);
    this._sitesByDomain = null;
    this.numRetries = 0;
    this._queryForRedirects = new Set();
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

  /**
  * fetchIcon attempts to fetch a rich icon for the given url from two sources.
  * First, it looks up the tippy top feed, if it's still missing, then it queries
  * the places for rich icon with its most recent visit in order to deal with
  * the redirected visit. See Bug 1421428 for more details.
  */
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
      iconUri = iconUri.mutate().setRef("tippytop").finalize();
      PlacesUtils.favicons.setAndFetchFaviconForPage(
        Services.io.newURI(url),
        iconUri,
        false,
        PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE,
        null,
        Services.scriptSecurityManager.getSystemPrincipal()
      );
      return;
    }

    if (!this._queryForRedirects.has(url)) {
      this._queryForRedirects.add(url);
      Services.tm.idleDispatchToMainThread(() => fetchIconFromRedirects(url));
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

const EXPORTED_SYMBOLS = ["FaviconFeed", "fetchIconFromRedirects"];
