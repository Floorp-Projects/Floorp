/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { actionTypes: at } = ChromeUtils.import(
  "resource://activity-stream/common/Actions.jsm"
);
const { getDomain } = ChromeUtils.import(
  "resource://activity-stream/lib/TippyTopProvider.jsm"
);
const { RemoteSettings } = ChromeUtils.import(
  "resource://services-settings/remote-settings.js"
);

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  PlacesUtils: "resource://gre/modules/PlacesUtils.sys.mjs",
});
ChromeUtils.defineModuleGetter(
  lazy,
  "NewTabUtils",
  "resource://gre/modules/NewTabUtils.jsm"
);

const MIN_FAVICON_SIZE = 96;

/**
 * Get favicon info (uri and size) for a uri from Places.
 *
 * @param uri {nsIURI} Page to check for favicon data
 * @returns A promise of an object (possibly null) containing the data
 */
function getFaviconInfo(uri) {
  return new Promise(resolve =>
    lazy.PlacesUtils.favicons.getFaviconDataForPage(
      uri,
      // Package up the icon data in an object if we have it; otherwise null
      (iconUri, faviconLength, favicon, mimeType, faviconSize) =>
        resolve(iconUri ? { iconUri, faviconSize } : null),
      lazy.NewTabUtils.activityStreamProvider.THUMB_FAVICON_SIZE
    )
  );
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
        (${lazy.PlacesUtils.history.TRANSITIONS.REDIRECT_PERMANENT},
         ${lazy.PlacesUtils.history.TRANSITIONS.REDIRECT_TEMPORARY})
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

  const visits = await lazy.NewTabUtils.activityStreamProvider.executePlacesQuery(
    query,
    {
      columns: ["visit_id", "url"],
      params: { url },
    }
  );
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
      lazy.PlacesUtils.favicons.setAndFetchFaviconForPage(
        Services.io.newURI(url),
        iconInfo.iconUri,
        false,
        lazy.PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE,
        null,
        Services.scriptSecurityManager.getSystemPrincipal()
      );
    }
  }
}

class FaviconFeed {
  constructor() {
    this._queryForRedirects = new Set();
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

    const site = await this.getSite(getDomain(url));
    if (!site) {
      if (!this._queryForRedirects.has(url)) {
        this._queryForRedirects.add(url);
        Services.tm.idleDispatchToMainThread(() => fetchIconFromRedirects(url));
      }
      return;
    }

    let iconUri = Services.io.newURI(site.image_url);
    // The #tippytop is to be able to identify them for telemetry.
    iconUri = iconUri
      .mutate()
      .setRef("tippytop")
      .finalize();
    lazy.PlacesUtils.favicons.setAndFetchFaviconForPage(
      Services.io.newURI(url),
      iconUri,
      false,
      lazy.PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE,
      null,
      Services.scriptSecurityManager.getSystemPrincipal()
    );
  }

  /**
   * Get the site tippy top data from Remote Settings.
   */
  async getSite(domain) {
    const sites = await this.tippyTop.get({
      filters: { domain },
      syncIfEmpty: false,
    });
    return sites.length ? sites[0] : null;
  }

  /**
   * Get the tippy top collection from Remote Settings.
   */
  get tippyTop() {
    if (!this._tippyTop) {
      this._tippyTop = RemoteSettings("tippytop");
    }
    return this._tippyTop;
  }

  /**
   * Determine if we should be fetching and saving icons.
   */
  get shouldFetchIcons() {
    return Services.prefs.getBoolPref("browser.chrome.site_icons");
  }

  onAction(action) {
    switch (action.type) {
      case at.RICH_ICON_MISSING:
        this.fetchIcon(action.data.url);
        break;
    }
  }
}

const EXPORTED_SYMBOLS = ["FaviconFeed", "fetchIconFromRedirects"];
