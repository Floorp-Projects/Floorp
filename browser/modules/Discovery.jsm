/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["Discovery"];

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};

ChromeUtils.defineModuleGetter(
  lazy,
  "ClientID",
  "resource://gre/modules/ClientID.jsm"
);
ChromeUtils.defineModuleGetter(
  lazy,
  "ContextualIdentityService",
  "resource://gre/modules/ContextualIdentityService.jsm"
);

const RECOMMENDATION_ENABLED = "browser.discovery.enabled";
const TELEMETRY_ENABLED = "datareporting.healthreport.uploadEnabled";
const TAAR_COOKIE_NAME = "taarId";

const Discovery = {
  set enabled(val) {
    val = !!val;
    if (val && !lazy.gTelemetryEnabled) {
      throw Error("unable to turn on recommendations");
    }
    Services.prefs.setBoolPref(RECOMMENDATION_ENABLED, val);
  },

  get enabled() {
    return lazy.gTelemetryEnabled && lazy.gRecommendationEnabled;
  },

  reset() {
    return DiscoveryInternal.update(true);
  },

  update() {
    return DiscoveryInternal.update();
  },
};

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "gRecommendationEnabled",
  RECOMMENDATION_ENABLED,
  false,
  Discovery.update
);
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "gTelemetryEnabled",
  TELEMETRY_ENABLED,
  false,
  Discovery.update
);
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "gCachedClientID",
  "toolkit.telemetry.cachedClientID",
  "",
  Discovery.reset
);
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "gContainersEnabled",
  "browser.discovery.containers.enabled",
  false,
  Discovery.reset
);

Services.obs.addObserver(Discovery.update, "contextual-identity-created");

const DiscoveryInternal = {
  get sites() {
    delete this.sites;
    this.sites = Services.prefs
      .getCharPref("browser.discovery.sites", "")
      .split(",");
    return this.sites;
  },

  getContextualIDs() {
    // There is never a zero id, this is just for use in update.
    let IDs = [0];
    if (lazy.gContainersEnabled) {
      lazy.ContextualIdentityService.getPublicIdentities().forEach(identity => {
        IDs.push(identity.userContextId);
      });
    }
    return IDs;
  },

  async update(reset = false) {
    if (reset || !Discovery.enabled) {
      for (let site of this.sites) {
        Services.cookies.remove(site, TAAR_COOKIE_NAME, "/", {});
        lazy.ContextualIdentityService.getPublicIdentities().forEach(
          identity => {
            let { userContextId } = identity;
            Services.cookies.remove(site, TAAR_COOKIE_NAME, "/", {
              userContextId,
            });
          }
        );
      }
    }

    if (Discovery.enabled) {
      // If the client id is not cached, wait for the notification that it is
      // cached.  This will happen shortly after startup in TelemetryController.jsm.
      // When that happens, we'll get a pref notification for the cached id,
      // which will call update again.
      if (!lazy.gCachedClientID) {
        return;
      }
      let id = await lazy.ClientID.getClientIdHash();
      for (let site of this.sites) {
        // This cookie gets tied down as much as possible.  Specifically,
        // SameSite, Secure, HttpOnly and non-PrivateBrowsing.
        for (let userContextId of this.getContextualIDs()) {
          let originAttributes = { privateBrowsingId: 0 };
          if (userContextId > 0) {
            originAttributes.userContextId = userContextId;
          }
          if (
            Services.cookies.cookieExists(
              site,
              "/",
              TAAR_COOKIE_NAME,
              originAttributes
            )
          ) {
            continue;
          }
          Services.cookies.add(
            site,
            "/",
            TAAR_COOKIE_NAME,
            id,
            true, // secure
            true, // httpOnly
            true, // session
            Date.now(),
            originAttributes,
            Ci.nsICookie.SAMESITE_LAX,
            Ci.nsICookie.SCHEME_HTTPS
          );
        }
      }
    }
  },
};
