/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["SessionCookies"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

ChromeUtils.defineModuleGetter(
  this,
  "PrivacyLevel",
  "resource://gre/modules/sessionstore/PrivacyLevel.jsm"
);

const MAX_EXPIRY = Number.MAX_SAFE_INTEGER;

/**
 * The external API implemented by the SessionCookies module.
 */
var SessionCookies = Object.freeze({
  collect() {
    return SessionCookiesInternal.collect();
  },

  restore(cookies) {
    SessionCookiesInternal.restore(cookies);
  },
});

/**
 * The internal API.
 */
var SessionCookiesInternal = {
  /**
   * Stores whether we're initialized, yet.
   */
  _initialized: false,

  /**
   * Retrieve an array of all stored session cookies.
   */
  collect() {
    this._ensureInitialized();
    return CookieStore.toArray();
  },

  /**
   * Restores a given list of session cookies.
   */
  restore(cookies) {
    for (let cookie of cookies) {
      let expiry = "expiry" in cookie ? cookie.expiry : MAX_EXPIRY;
      let exists = false;
      try {
        exists = Services.cookies.cookieExists(
          cookie.host,
          cookie.path || "",
          cookie.name || "",
          cookie.originAttributes || {}
        );
      } catch (ex) {
        Cu.reportError(
          `CookieService::CookieExists failed with error '${ex}' for '${JSON.stringify(
            cookie
          )}'.`
        );
      }
      if (!exists) {
        try {
          Services.cookies.add(
            cookie.host,
            cookie.path || "",
            cookie.name || "",
            cookie.value,
            !!cookie.secure,
            !!cookie.httponly,
            /* isSession = */ true,
            expiry,
            cookie.originAttributes || {},
            cookie.sameSite || Ci.nsICookie.SAMESITE_NONE,
            cookie.schemeMap || Ci.nsICookie.SCHEME_HTTPS
          );
        } catch (ex) {
          Cu.reportError(
            `CookieService::Add failed with error '${ex}' for cookie ${JSON.stringify(
              cookie
            )}.`
          );
        }
      }
    }
  },

  /**
   * Handles observers notifications that are sent whenever cookies are added,
   * changed, or removed. Ensures that the storage is updated accordingly.
   */
  observe(subject, topic, data) {
    switch (data) {
      case "added":
        this._addCookie(subject);
        break;
      case "changed":
        this._updateCookie(subject);
        break;
      case "deleted":
        this._removeCookie(subject);
        break;
      case "cleared":
        CookieStore.clear();
        break;
      case "batch-deleted":
        this._removeCookies(subject);
        break;
      default:
        throw new Error("Unhandled session-cookie-changed notification.");
    }
  },

  /**
   * If called for the first time in a session, iterates all cookies in the
   * cookies service and puts them into the store if they're session cookies.
   */
  _ensureInitialized() {
    if (this._initialized) {
      return;
    }
    this._reloadCookies();
    this._initialized = true;
    Services.obs.addObserver(this, "session-cookie-changed");

    // Listen for privacy level changes to reload cookies when needed.
    Services.prefs.addObserver("browser.sessionstore.privacy_level", () => {
      this._reloadCookies();
    });
  },

  /**
   * Adds a given cookie to the store.
   */
  _addCookie(cookie) {
    cookie.QueryInterface(Ci.nsICookie);

    // Store only session cookies, obey the privacy level.
    if (cookie.isSession && PrivacyLevel.canSave(cookie.isSecure)) {
      CookieStore.add(cookie);
    }
  },

  /**
   * Updates a given cookie.
   */
  _updateCookie(cookie) {
    cookie.QueryInterface(Ci.nsICookie);

    // Store only session cookies, obey the privacy level.
    if (cookie.isSession && PrivacyLevel.canSave(cookie.isSecure)) {
      CookieStore.add(cookie);
    } else {
      CookieStore.delete(cookie);
    }
  },

  /**
   * Removes a given cookie from the store.
   */
  _removeCookie(cookie) {
    cookie.QueryInterface(Ci.nsICookie);

    if (cookie.isSession) {
      CookieStore.delete(cookie);
    }
  },

  /**
   * Removes a given list of cookies from the store.
   */
  _removeCookies(cookies) {
    for (let i = 0; i < cookies.length; i++) {
      this._removeCookie(cookies.queryElementAt(i, Ci.nsICookie));
    }
  },

  /**
   * Iterates all cookies in the cookies service and puts them into the store
   * if they're session cookies. Obeys the user's chosen privacy level.
   */
  _reloadCookies() {
    CookieStore.clear();

    // Bail out if we're not supposed to store cookies at all.
    if (!PrivacyLevel.canSave(false)) {
      return;
    }

    for (let cookie of Services.cookies.sessionCookies) {
      this._addCookie(cookie);
    }
  },
};

/**
 * The internal storage that keeps track of session cookies.
 */
var CookieStore = {
  /**
   * The internal map holding all known session cookies.
   */
  _entries: new Map(),

  /**
   * Stores a given cookie.
   *
   * @param cookie
   *        The nsICookie object to add to the storage.
   */
  add(cookie) {
    let jscookie = { host: cookie.host, value: cookie.value };

    // Only add properties with non-default values to save a few bytes.
    if (cookie.path) {
      jscookie.path = cookie.path;
    }

    if (cookie.name) {
      jscookie.name = cookie.name;
    }

    if (cookie.isSecure) {
      jscookie.secure = true;
    }

    if (cookie.isHttpOnly) {
      jscookie.httponly = true;
    }

    if (cookie.expiry < MAX_EXPIRY) {
      jscookie.expiry = cookie.expiry;
    }

    if (cookie.originAttributes) {
      jscookie.originAttributes = cookie.originAttributes;
    }

    if (cookie.sameSite) {
      jscookie.sameSite = cookie.sameSite;
    }

    if (cookie.schemeMap) {
      jscookie.schemeMap = cookie.schemeMap;
    }

    this._entries.set(this._getKeyForCookie(cookie), jscookie);
  },

  /**
   * Removes a given cookie.
   *
   * @param cookie
   *        The nsICookie object to be removed from storage.
   */
  delete(cookie) {
    this._entries.delete(this._getKeyForCookie(cookie));
  },

  /**
   * Removes all cookies.
   */
  clear() {
    this._entries.clear();
  },

  /**
   * Return all cookies as an array.
   */
  toArray() {
    return [...this._entries.values()];
  },

  /**
   * Returns the key needed to properly store and identify a given cookie.
   * A cookie is uniquely identified by the combination of its host, name,
   * path, and originAttributes properties.
   *
   * @param cookie
   *        The nsICookie object to compute a key for.
   * @return string
   */
  _getKeyForCookie(cookie) {
    return JSON.stringify({
      host: cookie.host,
      name: cookie.name,
      path: cookie.path,
      attr: ChromeUtils.originAttributesToSuffix(cookie.originAttributes),
    });
  },
};
