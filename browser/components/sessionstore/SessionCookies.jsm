/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["SessionCookies"];

const Cu = Components.utils;
const Ci = Components.interfaces;

Cu.import("resource://gre/modules/Services.jsm", this);
Cu.import("resource://gre/modules/XPCOMUtils.jsm", this);

XPCOMUtils.defineLazyModuleGetter(this, "Utils",
  "resource://gre/modules/sessionstore/Utils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PrivacyLevel",
  "resource://gre/modules/sessionstore/PrivacyLevel.jsm");

// MAX_EXPIRY should be 2^63-1, but JavaScript can't handle that precision.
const MAX_EXPIRY = Math.pow(2, 62);

/**
 * The external API implemented by the SessionCookies module.
 */
this.SessionCookies = Object.freeze({
  update(windows) {
    SessionCookiesInternal.update(windows);
  },

  getHostsForWindow(window) {
    return SessionCookiesInternal.getHostsForWindow(window);
  },

  restore(cookies) {
    SessionCookiesInternal.restore(cookies);
  }
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
   * Retrieve the list of all hosts contained in the given windows' session
   * history entries (per window) and collect the associated cookies for those
   * hosts, if any. The given state object is being modified.
   *
   * @param windows
   *        Array of window state objects.
   *        [{ tabs: [...], cookies: [...] }, ...]
   */
  update(windows) {
    this._ensureInitialized();

    // Check whether we're allowed to store cookies.
    let storeAnyCookies = PrivacyLevel.canSave(false);
    let storeSecureCookies = PrivacyLevel.canSave(true);

    for (let window of windows) {
      let cookies = [];

      if (storeAnyCookies) {
        // Collect all cookies for the current window.
        for (let host of this.getHostsForWindow(window)) {
          for (let cookie of CookieStore.getCookiesForHost(host)) {
            if (!cookie.secure || storeSecureCookies) {
              cookies.push(cookie);
            }
          }
        }
      }

      // Don't include/keep empty cookie sections.
      if (cookies.length) {
        window.cookies = cookies;
      } else if ("cookies" in window) {
        delete window.cookies;
      }
    }
  },

  /**
   * Returns a map of all hosts for a given window that we might want to
   * collect cookies for.
   *
   * @param window
   *        A window state object containing tabs with history entries.
   * @return {set} A set of hosts for a given window state object.
   */
  getHostsForWindow(window) {
    let hosts = new Set();

    for (let tab of window.tabs) {
      for (let entry of tab.entries) {
        this._extractHostsFromEntry(entry, hosts);
      }
    }

    return hosts;
  },

  /**
   * Restores a given list of session cookies.
   */
  restore(cookies) {

    for (let cookie of cookies) {
      let expiry = "expiry" in cookie ? cookie.expiry : MAX_EXPIRY;
      let cookieObj = {
        host: cookie.host,
        path: cookie.path || "",
        name: cookie.name || ""
      };
      if (!Services.cookies.cookieExists(cookieObj, cookie.originAttributes || {})) {
        Services.cookies.add(cookie.host, cookie.path || "", cookie.name || "",
                             cookie.value, !!cookie.secure, !!cookie.httponly,
                             /* isSession = */ true, expiry, cookie.originAttributes || {});
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
  _ensureInitialized() {
    if (!this._initialized) {
      this._reloadCookies();
      this._initialized = true;
      Services.obs.addObserver(this, "cookie-changed", false);
    }
  },

  /**
   * Fill a given map with hosts found in the given entry's session history and
   * any child entries.
   *
   * @param entry
   *        the history entry, serialized
   * @param hosts
   *        the set that will be used to store hosts
   */
  _extractHostsFromEntry(entry, hosts) {
    try {
      // It's alright if this throws for about: URIs.
      let {host, scheme} = Utils.makeURI(entry.url);
      if (/^(file|https?)$/.test(scheme)) {
        hosts.add(host);
      }
    } catch (ex) { }

    if (entry.children) {
      for (let child of entry.children) {
        this._extractHostsFromEntry(child, hosts);
      }
    }
  },

  /**
   * Adds a given cookie to the store.
   */
  _addCookie(cookie) {
    cookie.QueryInterface(Ci.nsICookie2);

    if (cookie.isSession) {
      CookieStore.set(cookie);
    }
  },

  /**
   * Updates a given cookie.
   */
  _updateCookie(cookie) {
    cookie.QueryInterface(Ci.nsICookie2);

    if (cookie.isSession) {
      CookieStore.set(cookie);
    } else {
      CookieStore.delete(cookie);
    }
  },

  /**
   * Removes a given cookie from the store.
   */
  _removeCookie(cookie) {
    cookie.QueryInterface(Ci.nsICookie2);

    if (cookie.isSession) {
      CookieStore.delete(cookie);
    }
  },

  /**
   * Removes a given list of cookies from the store.
   */
  _removeCookies(cookies) {
    for (let i = 0; i < cookies.length; i++) {
      this._removeCookie(cookies.queryElementAt(i, Ci.nsICookie2));
    }
  },

  /**
   * Iterates all cookies in the cookies service and puts them into the store
   * if they're session cookies.
   */
  _reloadCookies() {
    let iter = Services.cookies.enumerator;
    while (iter.hasMoreElements()) {
      this._addCookie(iter.getNext());
    }
  }
};

/**
 * Generates all possible subdomains for a given host and prepends a leading
 * dot to all variants.
 *
 * See http://tools.ietf.org/html/rfc6265#section-5.1.3
 *     http://en.wikipedia.org/wiki/HTTP_cookie#Domain_and_Path
 *
 * All cookies belonging to a web page will be internally represented by a
 * nsICookie object. nsICookie.host will be the request host if no domain
 * parameter was given when setting the cookie. If a specific domain was given
 * then nsICookie.host will contain that specific domain and prepend a leading
 * dot to it.
 *
 * We thus generate all possible subdomains for a given domain and prepend a
 * leading dot to them as that is the value that was used as the map key when
 * the cookie was set.
 */
function* getPossibleSubdomainVariants(host) {
  // Try given domain with a leading dot (.www.example.com).
  yield "." + host;

  // Stop if there are only two parts left (e.g. example.com was given).
  let parts = host.split(".");
  if (parts.length < 3) {
    return;
  }

  // Remove the first subdomain (www.example.com -> example.com).
  let rest = parts.slice(1).join(".");

  // Try possible parent subdomains.
  yield* getPossibleSubdomainVariants(rest);
}

/**
 * The internal cookie storage that keeps track of every active session cookie.
 * These are stored using maps per host, path, and cookie name.
 */
var CookieStore = {
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
   *   },
   *   ".example.com": {
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
   * @param mainHost
   *        A string containing the host name we want to get cookies for.
   */
  getCookiesForHost(mainHost) {
    let cookies = [];

    let appendCookiesForHost = host => {
      if (!this._hosts.has(host)) {
        return;
      }

      for (let pathToNamesMap of this._hosts.get(host).values()) {
        for (let nameToCookiesMap of pathToNamesMap.values()) {
          cookies.push(...nameToCookiesMap.values());
        }
      }
    }

    // Try to find cookies for the given host, e.g. <www.example.com>.
    // The full hostname will be in the map if the Set-Cookie header did not
    // have a domain= attribute, i.e. the cookie will only be stored for the
    // request domain. Also, try to find cookies for subdomains, e.g.
    // <.example.com>. We will find those variants with a leading dot in the
    // map if the Set-Cookie header had a domain= attribute, i.e. the cookie
    // will be stored for a parent domain and we send it for any subdomain.
    for (let variant of [mainHost, ...getPossibleSubdomainVariants(mainHost)]) {
      appendCookiesForHost(variant);
    }

    return cookies;
  },

  /**
   * Stores a given cookie.
   *
   * @param cookie
   *        The nsICookie2 object to add to the storage.
   */
  set(cookie) {
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

    if (cookie.originAttributes) {
      jscookie.originAttributes = cookie.originAttributes;
    }

    this._ensureMap(cookie).set(cookie.name, jscookie);
  },

  /**
   * Removes a given cookie.
   *
   * @param cookie
   *        The nsICookie2 object to be removed from storage.
   */
  delete(cookie) {
    // Deletion shouldn't create new maps.
    // Bail out whenever we find that an entry or a map doesn't exist.
    let map = this._hosts.get(cookie.host);
    if (!map) {
      return;
    }

    map = map.get(ChromeUtils.originAttributesToSuffix(cookie.originAttributes));
    if (!map) {
      return;
    }

    map = map.get(cookie.path);
    if (!map) {
      return;
    }

    map.delete(cookie.name);
  },

  /**
   * Removes all cookies.
   */
  clear() {
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
  _ensureMap(cookie) {
    if (!this._hosts.has(cookie.host)) {
      this._hosts.set(cookie.host, new Map());
    }

    let originAttributesMap = this._hosts.get(cookie.host);
    // If cookie.originAttributes is null, originAttributes will be an empty string.
    let originAttributes = ChromeUtils.originAttributesToSuffix(cookie.originAttributes);
    if (!originAttributesMap.has(originAttributes)) {
      originAttributesMap.set(originAttributes, new Map());
    }

    let pathToNamesMap = originAttributesMap.get(originAttributes);

    if (!pathToNamesMap.has(cookie.path)) {
      pathToNamesMap.set(cookie.path, new Map());
    }

    return pathToNamesMap.get(cookie.path);
  }
};
