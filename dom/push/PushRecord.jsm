/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://gre/modules/AppConstants.jsm");
Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Messaging",
                                  "resource://gre/modules/Messaging.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
                                  "resource://gre/modules/PlacesUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Task",
                                  "resource://gre/modules/Task.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PrivateBrowsingUtils",
                                  "resource://gre/modules/PrivateBrowsingUtils.jsm");


this.EXPORTED_SYMBOLS = ["PushRecord"];

const prefs = new Preferences("dom.push.");

/**
 * The push subscription record, stored in IndexedDB.
 */
function PushRecord(props) {
  this.pushEndpoint = props.pushEndpoint;
  this.scope = props.scope;
  this.originAttributes = props.originAttributes;
  this.pushCount = props.pushCount || 0;
  this.lastPush = props.lastPush || 0;
  this.p256dhPublicKey = props.p256dhPublicKey;
  this.p256dhPrivateKey = props.p256dhPrivateKey;
  this.authenticationSecret = props.authenticationSecret;
  this.systemRecord = !!props.systemRecord;
  this.appServerKey = props.appServerKey;
  this.setQuota(props.quota);
  this.ctime = (typeof props.ctime === "number") ? props.ctime : 0;
}

PushRecord.prototype = {
  setQuota(suggestedQuota) {
    if (this.quotaApplies()) {
      let quota = +suggestedQuota;
      this.quota = quota >= 0 ? quota : prefs.get("maxQuotaPerSubscription");
    } else {
      this.quota = Infinity;
    }
  },

  resetQuota() {
    this.quota = this.quotaApplies() ?
                 prefs.get("maxQuotaPerSubscription") : Infinity;
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
    if (lastVisit > this.lastPush) {
      // If the user visited the site since the last time we received a
      // notification, reset the quota. `Math.max(0, ...)` ensures the
      // last visit date isn't in the future.
      let daysElapsed =
        Math.max(0, (Date.now() - lastVisit) / 24 / 60 / 60 / 1000);
      this.quota = Math.min(
        Math.round(8 * Math.pow(daysElapsed, -0.8)),
        prefs.get("maxQuotaPerSubscription")
      );
      Services.telemetry.getHistogramById("PUSH_API_QUOTA_RESET_TO").add(this.quota);
    }
  },

  receivedPush(lastVisit) {
    this.updateQuota(lastVisit);
    this.pushCount++;
    this.lastPush = Date.now();
  },

  reduceQuota() {
    if (!this.quotaApplies()) {
      return;
    }
    this.quota = Math.max(this.quota - 1, 0);
    // We check for ctime > 0 to skip older records that did not have ctime.
    if (this.isExpired() && this.ctime > 0) {
      let duration = Date.now() - this.ctime;
      Services.telemetry.getHistogramById("PUSH_API_QUOTA_EXPIRATION_TIME").add(duration / 1000);
    }
  },

  /**
   * Queries the Places database for the last time a user visited the site
   * associated with a push registration.
   *
   * @returns {Promise} A promise resolved with either the last time the user
   *  visited the site, or `-Infinity` if the site is not in the user's history.
   *  The time is expressed in milliseconds since Epoch.
   */
  getLastVisit: Task.async(function* () {
    if (!this.quotaApplies() || this.isTabOpen()) {
      // If the registration isn't subject to quota, or the user already
      // has the site open, skip expensive database queries.
      return Date.now();
    }

    if (AppConstants.MOZ_ANDROID_HISTORY) {
      let result = yield Messaging.sendRequestForResult({
        type: "History:GetPrePathLastVisitedTimeMilliseconds",
        prePath: this.uri.prePath,
      });
      return result == 0 ? -Infinity : result;
    }

    // Places History transition types that can fire a
    // `pushsubscriptionchange` event when the user visits a site with expired push
    // registrations. Visits only count if the user sees the origin in the address
    // bar. This excludes embedded resources, downloads, and framed links.
    const QUOTA_REFRESH_TRANSITIONS_SQL = [
      Ci.nsINavHistoryService.TRANSITION_LINK,
      Ci.nsINavHistoryService.TRANSITION_TYPED,
      Ci.nsINavHistoryService.TRANSITION_BOOKMARK,
      Ci.nsINavHistoryService.TRANSITION_REDIRECT_PERMANENT,
      Ci.nsINavHistoryService.TRANSITION_REDIRECT_TEMPORARY
    ].join(",");

    let db =  yield PlacesUtils.promiseDBConnection();
    // We're using a custom query instead of `nsINavHistoryQueryOptions`
    // because the latter doesn't expose a way to filter by transition type:
    // `setTransitions` performs a logical "and," but we want an "or." We
    // also avoid an unneeded left join on `moz_favicons`, and an `ORDER BY`
    // clause that emits a suboptimal index warning.
    let rows = yield db.executeCached(
      `SELECT MAX(visit_date) AS lastVisit
       FROM moz_places p
       JOIN moz_historyvisits ON p.id = place_id
       WHERE rev_host = get_unreversed_host(:host || '.') || '.'
         AND url BETWEEN :prePath AND :prePath || X'FFFF'
         AND visit_type IN (${QUOTA_REFRESH_TRANSITIONS_SQL})
      `,
      {
        // Restrict the query to all pages for this origin.
        host: this.uri.host,
        prePath: this.uri.prePath,
      }
    );

    if (!rows.length) {
      return -Infinity;
    }
    // Places records times in microseconds.
    let lastVisit = rows[0].getResultByName("lastVisit");

    return lastVisit / 1000;
  }),

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
        if (tabURI.prePath == this.uri.prePath) {
          return true;
        }
      }
    }
    return false;
  },

  /**
   * Indicates whether the registration can deliver push messages to its
   * associated service worker. System subscriptions are exempt from the
   * permission check.
   */
  hasPermission() {
    if (this.systemRecord || prefs.get("testing.ignorePermission")) {
      return true;
    }
    let permission = Services.perms.testExactPermissionFromPrincipal(
      this.principal, "desktop-notification");
    return permission == Ci.nsIPermissionManager.ALLOW_ACTION;
  },

  quotaChanged() {
    if (!this.hasPermission()) {
      return Promise.resolve(false);
    }
    return this.getLastVisit()
      .then(lastVisit => lastVisit > this.lastPush);
  },

  quotaApplies() {
    return !this.systemRecord;
  },

  isExpired() {
    return this.quota === 0;
  },

  matchesOriginAttributes(pattern) {
    if (this.systemRecord) {
      return false;
    }
    return ChromeUtils.originAttributesMatchPattern(
      this.principal.originAttributes, pattern);
  },

  hasAuthenticationSecret() {
    return !!this.authenticationSecret &&
           this.authenticationSecret.byteLength == 16;
  },

  matchesAppServerKey(key) {
    if (!this.appServerKey) {
      return !key;
    }
    if (!key) {
      return false;
    }
    return this.appServerKey.length === key.length &&
           this.appServerKey.every((value, index) => value === key[index]);
  },

  toSubscription() {
    return {
      endpoint: this.pushEndpoint,
      lastPush: this.lastPush,
      pushCount: this.pushCount,
      p256dhKey: this.p256dhPublicKey,
      authenticationSecret: this.authenticationSecret,
      appServerKey: this.appServerKey,
      quota: this.quotaApplies() ? this.quota : -1,
      systemRecord: this.systemRecord,
    };
  },
};

// Define lazy getters for the principal and scope URI. IndexedDB can't store
// `nsIPrincipal` objects, so we keep them in a private weak map.
var principals = new WeakMap();
Object.defineProperties(PushRecord.prototype, {
  principal: {
    get() {
      if (this.systemRecord) {
        return Services.scriptSecurityManager.getSystemPrincipal();
      }
      let principal = principals.get(this);
      if (!principal) {
        let url = this.scope;
        if (this.originAttributes) {
          // Allow tests to omit origin attributes.
          url += this.originAttributes;
        }
        principal = Services.scriptSecurityManager.createCodebasePrincipalFromOrigin(url);
        principals.set(this, principal);
      }
      return principal;
    },
    configurable: true,
  },

  uri: {
    get() {
      return this.principal.URI;
    },
    configurable: true,
  },
});
