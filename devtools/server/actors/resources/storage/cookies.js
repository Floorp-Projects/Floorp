/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  BaseStorageActor,
  DEFAULT_VALUE,
  SEPARATOR_GUID,
} = require("resource://devtools/server/actors/resources/storage/index.js");
const {
  LongStringActor,
} = require("resource://devtools/server/actors/string.js");

// "Lax", "Strict" and "None" are special values of the SameSite property
// that should not be translated.
const COOKIE_SAMESITE = {
  LAX: "Lax",
  STRICT: "Strict",
  NONE: "None",
};

// MAX_COOKIE_EXPIRY should be 2^63-1, but JavaScript can't handle that
// precision.
const MAX_COOKIE_EXPIRY = Math.pow(2, 62);

/**
 * General helpers
 */
function trimHttpHttpsPort(url) {
  const match = url.match(/(.+):\d+$/);

  if (match) {
    url = match[1];
  }
  if (url.startsWith("http://")) {
    return url.substr(7);
  }
  if (url.startsWith("https://")) {
    return url.substr(8);
  }
  return url;
}

class CookiesStorageActor extends BaseStorageActor {
  constructor(storageActor) {
    super(storageActor, "cookies");

    Services.obs.addObserver(this, "cookie-changed");
    Services.obs.addObserver(this, "private-cookie-changed");
  }

  destroy() {
    Services.obs.removeObserver(this, "cookie-changed");
    Services.obs.removeObserver(this, "private-cookie-changed");

    super.destroy();
  }

  populateStoresForHost(host) {
    this.hostVsStores.set(host, new Map());

    const originAttributes = this.getOriginAttributesFromHost(host);
    const cookies = this.getCookiesFromHost(host, originAttributes);

    for (const cookie of cookies) {
      if (this.isCookieAtHost(cookie, host)) {
        const uniqueKey =
          `${cookie.name}${SEPARATOR_GUID}${cookie.host}` +
          `${SEPARATOR_GUID}${cookie.path}`;

        this.hostVsStores.get(host).set(uniqueKey, cookie);
      }
    }
  }

  getOriginAttributesFromHost(host) {
    const win = this.storageActor.getWindowFromHost(host);
    let originAttributes;
    if (win) {
      originAttributes =
        win.document.effectiveStoragePrincipal.originAttributes;
    } else {
      // If we can't find the window by host, fallback to the top window
      // origin attributes.
      originAttributes =
        this.storageActor.document?.effectiveStoragePrincipal.originAttributes;
    }

    return originAttributes;
  }

  getCookiesFromHost(host, originAttributes) {
    // Local files have no host.
    if (host.startsWith("file:///")) {
      host = "";
    }

    host = trimHttpHttpsPort(host);

    return Services.cookies.getCookiesFromHost(host, originAttributes);
  }

  /**
   * Given a cookie object, figure out all the matching hosts from the page that
   * the cookie belong to.
   */
  getMatchingHosts(cookies) {
    if (!cookies) {
      return [];
    }
    if (!cookies.length) {
      cookies = [cookies];
    }
    const hosts = new Set();
    for (const host of this.hosts) {
      for (const cookie of cookies) {
        if (this.isCookieAtHost(cookie, host)) {
          hosts.add(host);
        }
      }
    }
    return [...hosts];
  }

  /**
   * Given a cookie object and a host, figure out if the cookie is valid for
   * that host.
   */
  isCookieAtHost(cookie, host) {
    if (cookie.host == null) {
      return host == null;
    }

    host = trimHttpHttpsPort(host);

    if (cookie.host.startsWith(".")) {
      return ("." + host).endsWith(cookie.host);
    }
    if (cookie.host === "") {
      return host.startsWith("file://" + cookie.path);
    }

    return cookie.host == host;
  }

  toStoreObject(cookie) {
    if (!cookie) {
      return null;
    }

    return {
      uniqueKey:
        `${cookie.name}${SEPARATOR_GUID}${cookie.host}` +
        `${SEPARATOR_GUID}${cookie.path}`,
      name: cookie.name,
      host: cookie.host || "",
      path: cookie.path || "",

      // because expires is in seconds
      expires: (cookie.expires || 0) * 1000,

      // because creationTime is in micro seconds
      creationTime: cookie.creationTime / 1000,

      size: cookie.name.length + (cookie.value || "").length,

      // - do -
      lastAccessed: cookie.lastAccessed / 1000,
      value: new LongStringActor(this.conn, cookie.value || ""),
      hostOnly: !cookie.isDomain,
      isSecure: cookie.isSecure,
      isHttpOnly: cookie.isHttpOnly,
      sameSite: this.getSameSiteStringFromCookie(cookie),
    };
  }

  getSameSiteStringFromCookie(cookie) {
    switch (cookie.sameSite) {
      case cookie.SAMESITE_LAX:
        return COOKIE_SAMESITE.LAX;
      case cookie.SAMESITE_STRICT:
        return COOKIE_SAMESITE.STRICT;
    }
    // cookie.SAMESITE_NONE
    return COOKIE_SAMESITE.NONE;
  }

  /**
   * Notification observer for "cookie-change".
   *
   * @param {(nsICookie|nsICookie[])} cookie - Cookie/s changed. Depending on the action
   * this is either null, a single cookie or an array of cookies.
   * @param {nsICookieNotification_Action} action - The cookie operation, see
   * nsICookieNotification for details.
   **/
  onCookieChanged(cookie, action) {
    const {
      COOKIE_ADDED,
      COOKIE_CHANGED,
      COOKIE_DELETED,
      COOKIES_BATCH_DELETED,
      ALL_COOKIES_CLEARED,
    } = Ci.nsICookieNotification;

    const hosts = this.getMatchingHosts(cookie);
    if (!hosts.length) {
      return;
    }

    const data = {};

    switch (action) {
      case COOKIE_ADDED:
      case COOKIE_CHANGED:
        if (hosts.length) {
          for (const host of hosts) {
            const uniqueKey =
              `${cookie.name}${SEPARATOR_GUID}${cookie.host}` +
              `${SEPARATOR_GUID}${cookie.path}`;

            this.hostVsStores.get(host).set(uniqueKey, cookie);
            data[host] = [uniqueKey];
          }
          const actionStr = action == COOKIE_ADDED ? "added" : "changed";
          this.storageActor.update(actionStr, "cookies", data);
        }
        break;

      case COOKIE_DELETED:
        if (hosts.length) {
          for (const host of hosts) {
            const uniqueKey =
              `${cookie.name}${SEPARATOR_GUID}${cookie.host}` +
              `${SEPARATOR_GUID}${cookie.path}`;

            this.hostVsStores.get(host).delete(uniqueKey);
            data[host] = [uniqueKey];
          }
          this.storageActor.update("deleted", "cookies", data);
        }
        break;

      case COOKIES_BATCH_DELETED:
        if (hosts.length) {
          for (const host of hosts) {
            const stores = [];
            // For COOKIES_BATCH_DELETED cookie is an array.
            for (const batchCookie of cookie) {
              const uniqueKey =
                `${batchCookie.name}${SEPARATOR_GUID}${batchCookie.host}` +
                `${SEPARATOR_GUID}${batchCookie.path}`;

              this.hostVsStores.get(host).delete(uniqueKey);
              stores.push(uniqueKey);
            }
            data[host] = stores;
          }
          this.storageActor.update("deleted", "cookies", data);
        }
        break;

      case ALL_COOKIES_CLEARED:
        if (hosts.length) {
          for (const host of hosts) {
            data[host] = [];
          }
          this.storageActor.update("cleared", "cookies", data);
        }
        break;
    }
  }

  async getFields() {
    return [
      { name: "uniqueKey", editable: false, private: true },
      { name: "name", editable: true, hidden: false },
      { name: "value", editable: true, hidden: false },
      { name: "host", editable: true, hidden: false },
      { name: "path", editable: true, hidden: false },
      { name: "expires", editable: true, hidden: false },
      { name: "size", editable: false, hidden: false },
      { name: "isHttpOnly", editable: true, hidden: false },
      { name: "isSecure", editable: true, hidden: false },
      { name: "sameSite", editable: false, hidden: false },
      { name: "lastAccessed", editable: false, hidden: false },
      { name: "creationTime", editable: false, hidden: true },
      { name: "hostOnly", editable: false, hidden: true },
    ];
  }

  /**
   * Pass the editItem command from the content to the chrome process.
   *
   * @param {Object} data
   *        See editCookie() for format details.
   */
  async editItem(data) {
    data.originAttributes = this.getOriginAttributesFromHost(data.host);
    this.editCookie(data);
  }

  async addItem(guid, host) {
    const window = this.storageActor.getWindowFromHost(host);
    const principal = window.document.effectiveStoragePrincipal;
    this.addCookie(guid, principal);
  }

  async removeItem(host, name) {
    const originAttributes = this.getOriginAttributesFromHost(host);
    this.removeCookie(host, name, originAttributes);
  }

  async removeAll(host, domain) {
    const originAttributes = this.getOriginAttributesFromHost(host);
    this.removeAllCookies(host, domain, originAttributes);
  }

  async removeAllSessionCookies(host, domain) {
    const originAttributes = this.getOriginAttributesFromHost(host);
    this._removeCookies(host, { domain, originAttributes, session: true });
  }

  addCookie(guid, principal) {
    // Set expiry time for cookie 1 day into the future
    // NOTE: Services.cookies.add expects the time in seconds.
    const ONE_DAY_IN_SECONDS = 60 * 60 * 24;
    const time = Math.floor(Date.now() / 1000);
    const expiry = time + ONE_DAY_IN_SECONDS;

    // principal throws an error when we try to access principal.host if it
    // does not exist (which happens at about: pages).
    // We check for asciiHost instead, which is always present, and has a
    // value of "" when the host is not available.
    const domain = principal.asciiHost ? principal.host : principal.baseDomain;

    Services.cookies.add(
      domain,
      "/",
      guid, // name
      DEFAULT_VALUE, // value
      false, // isSecure
      false, // isHttpOnly,
      false, // isSession,
      expiry, // expires,
      principal.originAttributes, // originAttributes
      Ci.nsICookie.SAMESITE_LAX, // sameSite
      principal.scheme === "https" // schemeMap
        ? Ci.nsICookie.SCHEME_HTTPS
        : Ci.nsICookie.SCHEME_HTTP
    );
  }

  /**
   * Apply the results of a cookie edit.
   *
   * @param {Object} data
   *        An object in the following format:
   *        {
   *          host: "http://www.mozilla.org",
   *          field: "value",
   *          editCookie: "name",
   *          oldValue: "%7BHello%7D",
   *          newValue: "%7BHelloo%7D",
   *          items: {
   *            name: "optimizelyBuckets",
   *            path: "/",
   *            host: ".mozilla.org",
   *            expires: "Mon, 02 Jun 2025 12:37:37 GMT",
   *            creationTime: "Tue, 18 Nov 2014 16:21:18 GMT",
   *            lastAccessed: "Wed, 17 Feb 2016 10:06:23 GMT",
   *            value: "%7BHelloo%7D",
   *            isDomain: "true",
   *            isSecure: "false",
   *            isHttpOnly: "false"
   *          }
   *        }
   */
  // eslint-disable-next-line complexity
  editCookie(data) {
    let { field, oldValue, newValue } = data;
    const origName = field === "name" ? oldValue : data.items.name;
    const origHost = field === "host" ? oldValue : data.items.host;
    const origPath = field === "path" ? oldValue : data.items.path;
    let cookie = null;

    const cookies = Services.cookies.getCookiesFromHost(
      origHost,
      data.originAttributes || {}
    );
    for (const nsiCookie of cookies) {
      if (
        nsiCookie.name === origName &&
        nsiCookie.host === origHost &&
        nsiCookie.path === origPath
      ) {
        cookie = {
          host: nsiCookie.host,
          path: nsiCookie.path,
          name: nsiCookie.name,
          value: nsiCookie.value,
          isSecure: nsiCookie.isSecure,
          isHttpOnly: nsiCookie.isHttpOnly,
          isSession: nsiCookie.isSession,
          expires: nsiCookie.expires,
          originAttributes: nsiCookie.originAttributes,
          schemeMap: nsiCookie.schemeMap,
        };
        break;
      }
    }

    if (!cookie) {
      return;
    }

    // If the date is expired set it for 10 seconds in the future.
    const now = new Date();
    if (!cookie.isSession && cookie.expires * 1000 <= now) {
      const tenSecondsFromNow = (now.getTime() + 10 * 1000) / 1000;

      cookie.expires = tenSecondsFromNow;
    }

    switch (field) {
      case "isSecure":
      case "isHttpOnly":
      case "isSession":
        newValue = newValue === "true";
        break;

      case "expires":
        newValue = Date.parse(newValue) / 1000;

        if (isNaN(newValue)) {
          newValue = MAX_COOKIE_EXPIRY;
        }
        break;

      case "host":
      case "name":
      case "path":
        // Remove the edited cookie.
        Services.cookies.remove(
          origHost,
          origName,
          origPath,
          cookie.originAttributes
        );
        break;
    }

    // Apply changes.
    cookie[field] = newValue;

    // cookie.isSession is not always set correctly on session cookies so we
    // need to trust cookie.expires instead.
    cookie.isSession = !cookie.expires;

    // Add the edited cookie.
    Services.cookies.add(
      cookie.host,
      cookie.path,
      cookie.name,
      cookie.value,
      cookie.isSecure,
      cookie.isHttpOnly,
      cookie.isSession,
      cookie.isSession ? MAX_COOKIE_EXPIRY : cookie.expires,
      cookie.originAttributes,
      cookie.sameSite,
      cookie.schemeMap
    );
  }

  _removeCookies(host, opts = {}) {
    // We use a uniqueId to emulate compound keys for cookies. We need to
    // extract the cookie name to remove the correct cookie.
    if (opts.name) {
      const split = opts.name.split(SEPARATOR_GUID);

      opts.name = split[0];
      opts.path = split[2];
    }

    host = trimHttpHttpsPort(host);

    function hostMatches(cookieHost, matchHost) {
      if (cookieHost == null) {
        return matchHost == null;
      }
      if (cookieHost.startsWith(".")) {
        return ("." + matchHost).endsWith(cookieHost);
      }
      return cookieHost == host;
    }

    const cookies = Services.cookies.getCookiesFromHost(
      host,
      opts.originAttributes || {}
    );
    for (const cookie of cookies) {
      if (
        hostMatches(cookie.host, host) &&
        (!opts.name || cookie.name === opts.name) &&
        (!opts.domain || cookie.host === opts.domain) &&
        (!opts.path || cookie.path === opts.path) &&
        (!opts.session || (!cookie.expires && !cookie.maxAge))
      ) {
        Services.cookies.remove(
          cookie.host,
          cookie.name,
          cookie.path,
          cookie.originAttributes
        );
      }
    }
  }

  removeCookie(host, name, originAttributes) {
    if (name !== undefined) {
      this._removeCookies(host, { name, originAttributes });
    }
  }

  removeAllCookies(host, domain, originAttributes) {
    this._removeCookies(host, { domain, originAttributes });
  }

  observe(subject, topic) {
    if (
      !subject ||
      (topic != "cookie-changed" && topic != "private-cookie-changed") ||
      !this.storageActor ||
      !this.storageActor.windows
    ) {
      return;
    }

    const notification = subject.QueryInterface(Ci.nsICookieNotification);
    let cookie;
    if (notification.action == Ci.nsICookieNotification.COOKIES_BATCH_DELETED) {
      // Extract the batch deleted cookies from nsIArray.
      const cookiesNoInterface =
        notification.batchDeletedCookies.QueryInterface(Ci.nsIArray);
      cookie = [];
      for (let i = 0; i < cookiesNoInterface.length; i++) {
        cookie.push(cookiesNoInterface.queryElementAt(i, Ci.nsICookie));
      }
    } else if (notification.cookie) {
      // Otherwise, get the single cookie affected by the operation.
      cookie = notification.cookie.QueryInterface(Ci.nsICookie);
    }

    this.onCookieChanged(cookie, notification.action);
  }
}
exports.CookiesStorageActor = CookiesStorageActor;
