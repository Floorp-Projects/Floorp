/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["SessionCookies"];

const Cu = Components.utils;
const Ci = Components.interfaces;

Cu.import("resource://gre/modules/Services.jsm", this);

// MAX_EXPIRY should be 2^63-1, but JavaScript can't handle that precision.
const MAX_EXPIRY = Math.pow(2, 62);


/**
 * The external API implemented by the SessionCookies module.
 */
this.SessionCookies = Object.freeze({
  getCookiesForHost: function (host) {
    return SessionCookiesInternal.getCookiesForHost(host);
  }
});

/**
 * The internal API.
 */
let SessionCookiesInternal = {
  /**
   * Stores whether we're initialized, yet.
   */
  _initialized: false,

  /**
   * Returns the list of active session cookies for a given host.
   */
  getCookiesForHost: function (host) {
    this._ensureInitialized();
    return CookieStore.getCookiesForHost(host);
  },

  /**
   * Handles observers notifications that are sent whenever cookies are added,
   * changed, or removed. Ensures that the storage is updated accordingly.
   */
  observe: function (subject, topic, data) {
    switch (data) {
      case "added":
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
      case "reload":
        CookieStore.clear();
        this._reloadCookies();
        break;
      default:
        throw new Error("Unhandled cookie-changed notification.");
    }
  },

  /**
   * If called for the first time in a session, iterates all cookies in the
   * cookies service and puts them into the store if they're session cookies.
   */
  _ensureInitialized: function () {
    if (!this._initialized) {
      this._reloadCookies();
      this._initialized = true;
      Services.obs.addObserver(this, "cookie-changed", false);
    }
  },

  /**
   * Updates or adds a given cookie to the store.
   */
  _updateCookie: function (cookie) {
    cookie.QueryInterface(Ci.nsICookie2);

    if (cookie.isSession) {
      CookieStore.set(cookie);
    }
  },

  /**
   * Removes a given cookie from the store.
   */
  _removeCookie: function (cookie) {
    cookie.QueryInterface(Ci.nsICookie2);

    if (cookie.isSession) {
      CookieStore.delete(cookie);
    }
  },

  /**
   * Removes a given list of cookies from the store.
   */
  _removeCookies: function (cookies) {
    for (let i = 0; i < cookies.length; i++) {
      this._removeCookie(cookies.queryElementAt(i, Ci.nsICookie2));
    }
  },

  /**
   * Iterates all cookies in the cookies service and puts them into the store
   * if they're session cookies.
   */
  _reloadCookies: function () {
    let iter = Services.cookies.enumerator;
    while (iter.hasMoreElements()) {
      this._updateCookie(iter.getNext());
    }
  }
};

/**
 * The internal cookie storage that keeps track of every active session cookie.
 * These are stored using maps per host, path, and cookie name.
 */
let CookieStore = {
  /**
   * The internal structure holding all known cookies.
   *
   * Host =>
   *  Path =>
   *    Name => {path: "/", name: "sessionid", secure: true}
   *
   * Maps are used for storage but the data structure is equivalent to this:
   *
   * this._hosts = {
   *   "www.mozilla.org": {
   *     "/": {
   *       "username": {name: "username", value: "my_name_is", etc...},
   *       "sessionid": {name: "sessionid", value: "1fdb3a", etc...}
   *     }
   *   },
   *   "tbpl.mozilla.org": {
   *     "/path": {
   *       "cookiename": {name: "cookiename", value: "value", etc...}
   *     }
   *   }
   * };
   */
  _hosts: new Map(),

  /**
   * Returns the list of stored session cookies for a given host.
   *
   * @param host
   *        A string containing the host name we want to get cookies for.
   */
  getCookiesForHost: function (host) {
    if (!this._hosts.has(host)) {
      return [];
    }

    let cookies = [];

    for (let pathToNamesMap of this._hosts.get(host).values()) {
      cookies = cookies.concat([cookie for (cookie of pathToNamesMap.values())]);
    }

    return cookies;
  },

  /**
   * Stores a given cookie.
   *
   * @param cookie
   *        The nsICookie2 object to add to the storage.
   */
  set: function (cookie) {
    let jscookie = {host: cookie.host, value: cookie.value};

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

    this._ensureMap(cookie).set(cookie.name, jscookie);
  },

  /**
   * Removes a given cookie.
   *
   * @param cookie
   *        The nsICookie2 object to be removed from storage.
   */
  delete: function (cookie) {
    this._ensureMap(cookie).delete(cookie.name);
  },

  /**
   * Removes all cookies.
   */
  clear: function () {
    this._hosts.clear();
  },

  /**
   * Creates all maps necessary to store a given cookie.
   *
   * @param cookie
   *        The nsICookie2 object to create maps for.
   *
   * @return The newly created Map instance mapping cookie names to
   *         internal jscookies, in the given path of the given host.
   */
  _ensureMap: function (cookie) {
    if (!this._hosts.has(cookie.host)) {
      this._hosts.set(cookie.host, new Map());
    }

    let pathToNamesMap = this._hosts.get(cookie.host);

    if (!pathToNamesMap.has(cookie.path)) {
      pathToNamesMap.set(cookie.path, new Map());
    }

    return pathToNamesMap.get(cookie.path);
  }
};
