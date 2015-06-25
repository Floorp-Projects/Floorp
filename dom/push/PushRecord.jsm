/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
                                  "resource://gre/modules/PlacesUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "PrivateBrowsingUtils",
                                  "resource://gre/modules/PrivateBrowsingUtils.jsm");

this.EXPORTED_SYMBOLS = ["PushRecord"];

const prefs = new Preferences("dom.push.");

// History transition types that can fire an `pushsubscriptionchange` event
// when the user visits a site with expired push registrations. Visits only
// count if the user sees the origin in the address bar. This excludes embedded
// resources, downloads, and framed links.
const QUOTA_REFRESH_TRANSITIONS_SQL = [
  Ci.nsINavHistoryService.TRANSITION_LINK,
  Ci.nsINavHistoryService.TRANSITION_TYPED,
  Ci.nsINavHistoryService.TRANSITION_BOOKMARK,
  Ci.nsINavHistoryService.TRANSITION_REDIRECT_PERMANENT,
  Ci.nsINavHistoryService.TRANSITION_REDIRECT_TEMPORARY
].join(",");

function PushRecord(props) {
  this.pushEndpoint = props.pushEndpoint;
  this.scope = props.scope;
  this.origin = Services.io.newURI(this.scope, null, null).prePath;
  this.originAttributes = props.originAttributes;
  this.pushCount = props.pushCount || 0;
  this.lastPush = props.lastPush || 0;
  this.setQuota(props.quota);
}

PushRecord.prototype = {
  setQuota(suggestedQuota) {
    this.quota = (!isNaN(suggestedQuota) && suggestedQuota >= 0) ?
                 suggestedQuota : prefs.get("maxQuotaPerSubscription");
  },

  updateQuota(lastVisit) {
    if (this.isExpired() || !this.quotaApplies()) {
      // Ignore updates if the registration is already expired, or isn't
      // subject to quota.
      return;
    }
    if (lastVisit < 0) {
      // If the user cleared their history, but retained the push permission,
      // mark the registration as expired.
      this.quota = 0;
      return;
    }
    let currentQuota;
    if (lastVisit > this.lastPush) {
      // If the user visited the site since the last time we received a
      // notification, reset the quota.
      let daysElapsed = (Date.now() - lastVisit) / 24 / 60 / 60 / 1000;
      currentQuota = Math.min(
        Math.round(8 * Math.pow(daysElapsed, -0.8)),
        prefs.get("maxQuotaPerSubscription")
      );
    } else {
      // The user hasn't visited the site since the last notification.
      currentQuota = this.quota;
    }
    this.quota = Math.max(currentQuota - 1, 0);
  },

  receivedPush(lastVisit) {
    this.updateQuota(lastVisit);
    this.pushCount++;
    this.lastPush = Date.now();
  },

  /**
   * Queries the Places database for the last time a user visited the site
   * associated with a push registration.
   *
   * @returns {Promise} A promise resolved with either the last time the user
   *  visited the site, or `-Infinity` if the site is not in the user's history.
   *  The time is expressed in milliseconds since Epoch.
   */
  getLastVisit() {
    if (!this.quotaApplies() || this.isTabOpen()) {
      // If the registration isn't subject to quota, or the user already
      // has the site open, skip the Places query.
      return Promise.resolve(Date.now());
    }
    return PlacesUtils.withConnectionWrapper("PushRecord.getLastVisit", db => {
      // We're using a custom query instead of `nsINavHistoryQueryOptions`
      // because the latter doesn't expose a way to filter by transition type:
      // `setTransitions` performs a logical "and," but we want an "or." We
      // also avoid an unneeded left join on `moz_favicons`, and an `ORDER BY`
      // clause that emits a suboptimal index warning.
      return db.executeCached(
        `SELECT MAX(p.last_visit_date)
         FROM moz_places p
         INNER JOIN moz_historyvisits h ON p.id = h.place_id
         WHERE (
           p.url >= :urlLowerBound AND p.url <= :urlUpperBound AND
           h.visit_type IN (${QUOTA_REFRESH_TRANSITIONS_SQL})
         )
        `,
        {
          // Restrict the query to all pages for this origin.
          urlLowerBound: this.origin,
          urlUpperBound: this.origin + "\x7f"
        }
      );
    }).then(rows => {
      if (!rows.length) {
        return -Infinity;
      }
      // Places records times in microseconds.
      let lastVisit = rows[0].getResultByIndex(0);
      return lastVisit / 1000;
    });
  },

  isTabOpen() {
    let windows = Services.wm.getEnumerator("navigator:browser");
    while (windows.hasMoreElements()) {
      let window = windows.getNext();
      if (window.closed || PrivateBrowsingUtils.isWindowPrivate(window)) {
        continue;
      }
      // `gBrowser` on Desktop; `BrowserApp` on Fennec.
      let tabs = window.gBrowser ? window.gBrowser.tabContainer.children :
                 window.BrowserApp.tabs;
      for (let tab of tabs) {
        // `linkedBrowser` on Desktop; `browser` on Fennec.
        let tabURI = (tab.linkedBrowser || tab.browser).currentURI;
        if (tabURI.prePath == this.origin) {
          return true;
        }
      }
    }
    return false;
  },

  quotaApplies() {
    return Number.isFinite(this.quota);
  },

  isExpired() {
    return this.quota === 0;
  },

  toRegistration() {
    return {
      pushEndpoint: this.pushEndpoint,
      lastPush: this.lastPush,
      pushCount: this.pushCount,
    };
  },

  toRegister() {
    return {
      pushEndpoint: this.pushEndpoint,
    };
  },
};

// Mark the `origin` property as non-enumerable to avoid storing the
// registration origin in IndexedDB.
Object.defineProperty(PushRecord.prototype, "origin", {
  configurable: true,
  enumerable: false,
  writable: true,
});
