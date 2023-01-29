/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const protocol = require("resource://devtools/shared/protocol.js");
const {
  LongStringActor,
} = require("resource://devtools/server/actors/string.js");
const {
  DevToolsServer,
} = require("resource://devtools/server/devtools-server.js");
const specs = require("resource://devtools/shared/specs/storage.js");
const {
  parseItemValue,
} = require("resource://devtools/shared/storage/utils.js");
loader.lazyGetter(this, "ExtensionProcessScript", () => {
  return ChromeUtils.import("resource://gre/modules/ExtensionProcessScript.jsm")
    .ExtensionProcessScript;
});
loader.lazyGetter(this, "ExtensionStorageIDB", () => {
  return ChromeUtils.import("resource://gre/modules/ExtensionStorageIDB.jsm")
    .ExtensionStorageIDB;
});
const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  Sqlite: "resource://gre/modules/Sqlite.sys.mjs",
});

const DEFAULT_VALUE = "value";

loader.lazyRequireGetter(
  this,
  "naturalSortCaseInsensitive",
  "resource://devtools/shared/natural-sort.js",
  true
);

// "Lax", "Strict" and "None" are special values of the SameSite property
// that should not be translated.
const COOKIE_SAMESITE = {
  LAX: "Lax",
  STRICT: "Strict",
  NONE: "None",
};

const SAFE_HOSTS_PREFIXES_REGEX = /^(about\+|https?\+|file\+|moz-extension\+)/;

// GUID to be used as a separator in compound keys. This must match the same
// constant in devtools/client/storage/ui.js,
// devtools/client/storage/test/head.js and
// devtools/server/tests/browser/head.js
const SEPARATOR_GUID = "{9d414cc5-8319-0a04-0586-c0a6ae01670a}";

// We give this a funny name to avoid confusion with the global
// indexedDB.
loader.lazyGetter(this, "indexedDBForStorage", () => {
  // On xpcshell, we can't instantiate indexedDB without crashing
  try {
    const sandbox = Cu.Sandbox(
      Components.Constructor(
        "@mozilla.org/systemprincipal;1",
        "nsIPrincipal"
      )(),
      { wantGlobalProperties: ["indexedDB"] }
    );
    return sandbox.indexedDB;
  } catch (e) {
    return {};
  }
});

// Maximum number of cookies/local storage key-value-pairs that can be sent
// over the wire to the client in one request.
const MAX_STORE_OBJECT_COUNT = 50;

// MAX_COOKIE_EXPIRY should be 2^63-1, but JavaScript can't handle that
// precision.
const MAX_COOKIE_EXPIRY = Math.pow(2, 62);

// A RegExp for characters that cannot appear in a file/directory name. This is
// used to sanitize the host name for indexed db to lookup whether the file is
// present in <profileDir>/storage/default/ location
var illegalFileNameCharacters = [
  "[",
  // Control characters \001 to \036
  "\\x00-\\x24",
  // Special characters
  '/:*?\\"<>|\\\\',
  "]",
].join("");
var ILLEGAL_CHAR_REGEX = new RegExp(illegalFileNameCharacters, "g");

// Holder for all the registered storage actors.
var storageTypePool = new Map();
exports.storageTypePool = storageTypePool;

/**
 * An async method equivalent to setTimeout but using Promises
 *
 * @param {number} time
 *        The wait time in milliseconds.
 */
function sleep(time) {
  return new Promise(resolve => {
    setTimeout(() => {
      resolve(null);
    }, time);
  });
}

// Helper methods to create a storage actor.
var StorageActors = {};

/**
 * Creates a default object with the common methods required by all storage
 * actors.
 *
 * This default object is missing a couple of required methods that should be
 * implemented seperately for each actor. They are namely:
 *   - observe : Method which gets triggered on the notificaiton of the watched
 *               topic.
 *   - getNamesForHost : Given a host, get list of all known store names.
 *   - getValuesForHost : Given a host (and optionally a name) get all known
 *                        store objects.
 *   - toStoreObject : Given a store object, convert it to the required format
 *                     so that it can be transferred over wire.
 *   - populateStoresForHost : Given a host, populate the map of all store
 *                             objects for it
 *   - getFields: Given a subType(optional), get an array of objects containing
 *                column field info. The info includes,
 *                "name" is name of colume key.
 *                "editable" is 1 means editable field; 0 means uneditable.
 *
 * @param {string} typeName
 *        The typeName of the actor.
 * @param {array} observationTopics
 *        An array of topics which this actor listens to via Notification Observers.
 */
StorageActors.defaults = function(typeName, observationTopics) {
  return {
    typeName,

    get conn() {
      return this.storageActor.conn;
    },

    /**
     * Returns a list of currently known hosts for the target window. This list
     * contains unique hosts from the window + all inner windows. If
     * this._internalHosts is defined then these will also be added to the list.
     */
    get hosts() {
      const hosts = new Set();
      for (const { location } of this.storageActor.windows) {
        const host = this.getHostName(location);

        if (host) {
          hosts.add(host);
        }
      }
      if (this._internalHosts) {
        for (const host of this._internalHosts) {
          hosts.add(host);
        }
      }
      return hosts;
    },

    /**
     * Returns all the windows present on the page. Includes main window + inner
     * iframe windows.
     */
    get windows() {
      return this.storageActor.windows;
    },

    /**
     * Converts the window.location object into a URL (e.g. http://domain.com).
     */
    getHostName(location) {
      if (!location) {
        // Debugging a legacy Firefox extension... no hostname available and no
        // storage possible.
        return null;
      }

      if (this.storageActor.getHostName) {
        return this.storageActor.getHostName(location);
      }

      switch (location.protocol) {
        case "about:":
          return `${location.protocol}${location.pathname}`;
        case "chrome:":
          // chrome: URLs do not support storage of any type.
          return null;
        case "data:":
          // data: URLs do not support storage of any type.
          return null;
        case "file:":
          return `${location.protocol}//${location.pathname}`;
        case "javascript:":
          return location.href;
        case "moz-extension:":
          return location.origin;
        case "resource:":
          return `${location.origin}${location.pathname}`;
        default:
          // http: or unknown protocol.
          return `${location.protocol}//${location.host}`;
      }
    },

    initialize(storageActor) {
      protocol.Actor.prototype.initialize.call(this, null);

      this.storageActor = storageActor;

      this.populateStoresForHosts();
      if (observationTopics) {
        observationTopics.forEach(observationTopic => {
          Services.obs.addObserver(this, observationTopic);
        });
      }
      this.onWindowReady = this.onWindowReady.bind(this);
      this.onWindowDestroyed = this.onWindowDestroyed.bind(this);
      this.storageActor.on("window-ready", this.onWindowReady);
      this.storageActor.on("window-destroyed", this.onWindowDestroyed);
    },

    destroy() {
      if (!this.storageActor) {
        return;
      }

      if (observationTopics) {
        observationTopics.forEach(observationTopic => {
          Services.obs.removeObserver(this, observationTopic);
        });
      }
      this.storageActor.off("window-ready", this.onWindowReady);
      this.storageActor.off("window-destroyed", this.onWindowDestroyed);

      this.hostVsStores.clear();

      protocol.Actor.prototype.destroy.call(this);

      this.storageActor = null;
    },

    getNamesForHost(host) {
      return [...this.hostVsStores.get(host).keys()];
    },

    getValuesForHost(host, name) {
      if (name) {
        return [this.hostVsStores.get(host).get(name)];
      }
      return [...this.hostVsStores.get(host).values()];
    },

    getObjectsSize(host, names) {
      return names.length;
    },

    /**
     * When a new window is added to the page. This generally means that a new
     * iframe is created, or the current window is completely reloaded.
     *
     * @param {window} window
     *        The window which was added.
     */
    async onWindowReady(window) {
      if (!this.hostVsStores) {
        return;
      }
      const host = this.getHostName(window.location);
      if (host && !this.hostVsStores.has(host)) {
        await this.populateStoresForHost(host, window);
        if (!this.storageActor) {
          // The actor might be destroyed during populateStoresForHost.
          return;
        }

        const data = {};
        data[host] = this.getNamesForHost(host);
        this.storageActor.update("added", typeName, data);
      }
    },

    /**
     * When a window is removed from the page. This generally means that an
     * iframe was removed, or the current window reload is triggered.
     *
     * @param {window} window
     *        The window which was removed.
     * @param {Object} options
     * @param {Boolean} options.dontCheckHost
     *        If set to true, the function won't check if the host still is in this.hosts.
     *        This is helpful in the case of the StorageActorMock, as the `hosts` getter
     *        uses its `windows` getter, and at this point in time the window which is
     *        going to be destroyed still exists.
     */
    onWindowDestroyed(window, { dontCheckHost } = {}) {
      if (!this.hostVsStores) {
        return;
      }
      if (!window.location) {
        // Nothing can be done if location object is null
        return;
      }
      const host = this.getHostName(window.location);
      if (host && (!this.hosts.has(host) || dontCheckHost)) {
        this.hostVsStores.delete(host);
        const data = {};
        data[host] = [];
        this.storageActor.update("deleted", typeName, data);
      }
    },

    form() {
      const hosts = {};
      for (const host of this.hosts) {
        hosts[host] = [];
      }

      return {
        actor: this.actorID,
        hosts,
        traits: this._getTraits(),
      };
    },

    // Share getTraits for child classes overriding form()
    _getTraits() {
      return {
        // The supportsXXX traits are not related to backward compatibility
        // Different storage actor types implement different APIs, the traits
        // help the client to know what is supported or not.
        supportsAddItem: typeof this.addItem === "function",
        // Note: supportsRemoveItem and supportsRemoveAll are always defined
        // for all actors. See Bug 1655001.
        supportsRemoveItem: typeof this.removeItem === "function",
        supportsRemoveAll: typeof this.removeAll === "function",
        supportsRemoveAllSessionCookies:
          typeof this.removeAllSessionCookies === "function",
      };
    },

    /**
     * Populates a map of known hosts vs a map of stores vs value.
     */
    populateStoresForHosts() {
      this.hostVsStores = new Map();
      for (const host of this.hosts) {
        this.populateStoresForHost(host);
      }
    },

    /**
     * Returns a list of requested store objects. Maximum values returned are
     * MAX_STORE_OBJECT_COUNT. This method returns paginated values whose
     * starting index and total size can be controlled via the options object
     *
     * @param {string} host
     *        The host name for which the store values are required.
     * @param {array:string} names
     *        Array containing the names of required store objects. Empty if all
     *        items are required.
     * @param {object} options
     *        Additional options for the request containing following
     *        properties:
     *         - offset {number} : The begin index of the returned array amongst
     *                  the total values
     *         - size {number} : The number of values required.
     *         - sortOn {string} : The values should be sorted on this property.
     *         - index {string} : In case of indexed db, the IDBIndex to be used
     *                 for fetching the values.
     *         - sessionString {string} : Client-side value of storage-expires-session
     *                         l10n string. Since this function can be called from both
     *                         the client and the server, and given that client and
     *                         server might have different locales, we can't compute
     *                         the localized string directly from here.
     * @return {object} An object containing following properties:
     *          - offset - The actual offset of the returned array. This might
     *                     be different from the requested offset if that was
     *                     invalid
     *          - total - The total number of entries possible.
     *          - data - The requested values.
     */
    async getStoreObjects(host, names, options = {}) {
      const offset = options.offset || 0;
      let size = options.size || MAX_STORE_OBJECT_COUNT;
      if (size > MAX_STORE_OBJECT_COUNT) {
        size = MAX_STORE_OBJECT_COUNT;
      }
      const sortOn = options.sortOn || "name";

      const toReturn = {
        offset,
        total: 0,
        data: [],
      };

      let principal = null;
      if (this.typeName === "indexedDB") {
        // We only acquire principal when the type of the storage is indexedDB
        // because the principal only matters the indexedDB.
        const win = this.storageActor.getWindowFromHost(host);
        principal = this.getPrincipal(win);
      }

      if (names) {
        for (const name of names) {
          const values = await this.getValuesForHost(
            host,
            name,
            options,
            this.hostVsStores,
            principal
          );

          const { result, objectStores } = values;

          if (result && typeof result.objectsSize !== "undefined") {
            for (const { key, count } of result.objectsSize) {
              this.objectsSize[key] = count;
            }
          }

          if (result) {
            toReturn.data.push(...result.data);
          } else if (objectStores) {
            toReturn.data.push(...objectStores);
          } else {
            toReturn.data.push(...values);
          }
        }

        if (this.typeName === "Cache") {
          // Cache storage contains several items per name but misses a custom
          // `getObjectsSize` implementation, as implemented for IndexedDB.
          // See Bug 1745242.
          toReturn.total = toReturn.data.length;
        } else {
          toReturn.total = this.getObjectsSize(host, names, options);
        }
      } else {
        let obj = await this.getValuesForHost(
          host,
          undefined,
          undefined,
          this.hostVsStores,
          principal
        );
        if (obj.dbs) {
          obj = obj.dbs;
        }

        toReturn.total = obj.length;
        toReturn.data = obj;
      }

      if (offset > toReturn.total) {
        // In this case, toReturn.data is an empty array.
        toReturn.offset = toReturn.total;
        toReturn.data = [];
      } else {
        // We need to use natural sort before slicing.
        const sorted = toReturn.data.sort((a, b) => {
          return naturalSortCaseInsensitive(
            a[sortOn],
            b[sortOn],
            options.sessionString
          );
        });
        let sliced;
        if (this.typeName === "indexedDB") {
          // indexedDB's getValuesForHost never returns *all* values available but only
          // a slice, starting at the expected offset. Therefore the result is already
          // sliced as expected.
          sliced = sorted;
        } else {
          sliced = sorted.slice(offset, offset + size);
        }
        toReturn.data = sliced.map(a => this.toStoreObject(a));
      }

      return toReturn;
    },

    getPrincipal(win) {
      if (win) {
        return win.document.effectiveStoragePrincipal;
      }
      // We are running in the browser toolbox and viewing system DBs so we
      // need to use system principal.
      return Cc["@mozilla.org/systemprincipal;1"].createInstance(
        Ci.nsIPrincipal
      );
    },
  };
};

/**
 * Creates an actor and its corresponding front and registers it to the Storage
 * Actor.
 *
 * @See StorageActors.defaults()
 *
 * @param {object} options
 *        Options required by StorageActors.defaults method which are :
 *         - typeName {string}
 *                    The typeName of the actor.
 *         - observationTopics {array}
 *                            The topics which this actor listens to via
 *                            Notification Observers.
 * @param {object} overrides
 *        All the methods which you want to be different from the ones in
 *        StorageActors.defaults method plus the required ones described there.
 */
StorageActors.createActor = function(options = {}, overrides = {}) {
  const actorObject = StorageActors.defaults(
    options.typeName,
    options.observationTopics || null
  );
  for (const key in overrides) {
    actorObject[key] = overrides[key];
  }

  const actorSpec = specs.childSpecs[options.typeName];
  const actor = protocol.ActorClassWithSpec(actorSpec, actorObject);
  storageTypePool.set(actorObject.typeName, actor);
};

/**
 * The Cookies actor and front.
 */
StorageActors.createActor(
  {
    typeName: "cookies",
  },
  {
    initialize(storageActor) {
      protocol.Actor.prototype.initialize.call(this, null);

      this.storageActor = storageActor;

      this.populateStoresForHosts();
      this.addCookieObservers();

      this.onWindowReady = this.onWindowReady.bind(this);
      this.onWindowDestroyed = this.onWindowDestroyed.bind(this);
      this.storageActor.on("window-ready", this.onWindowReady);
      this.storageActor.on("window-destroyed", this.onWindowDestroyed);
    },

    destroy() {
      this.hostVsStores.clear();

      // We need to remove the cookie listeners early in E10S mode so we need to
      // use a conditional here to ensure that we only attempt to remove them in
      // single process mode.
      if (!DevToolsServer.isInChildProcess) {
        this.removeCookieObservers();
      }

      this.storageActor.off("window-ready", this.onWindowReady);
      this.storageActor.off("window-destroyed", this.onWindowDestroyed);

      this._pendingResponse = null;

      protocol.Actor.prototype.destroy.call(this);

      this.storageActor = null;
    },

    /**
     * Given a cookie object, figure out all the matching hosts from the page that
     * the cookie belong to.
     */
    getMatchingHosts(cookies) {
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
    },

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
    },

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
    },

    getSameSiteStringFromCookie(cookie) {
      switch (cookie.sameSite) {
        case cookie.SAMESITE_LAX:
          return COOKIE_SAMESITE.LAX;
        case cookie.SAMESITE_STRICT:
          return COOKIE_SAMESITE.STRICT;
      }
      // cookie.SAMESITE_NONE
      return COOKIE_SAMESITE.NONE;
    },

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
    },

    getOriginAttributesFromHost(host) {
      const win = this.storageActor.getWindowFromHost(host);
      let originAttributes;
      if (win) {
        originAttributes =
          win.document.effectiveStoragePrincipal.originAttributes;
      } else {
        // If we can't find the window by host, fallback to the top window
        // origin attributes.
        originAttributes = this.storageActor.document?.effectiveStoragePrincipal
          .originAttributes;
      }

      return originAttributes;
    },

    /**
     * Notification observer for "cookie-change".
     *
     * @param subject
     *        {Cookie|[Array]} A JSON parsed object containing either a single
     *        cookie representation or an array. Array is only in case of
     *        a "batch-deleted" action.
     * @param {string} topic
     *        The topic of the notification.
     * @param {string} action
     *        Additional data associated with the notification. Its the type of
     *        cookie change in the "cookie-change" topic.
     */
    onCookieChanged(subject, topic, action) {
      if (
        (topic !== "cookie-changed" && topic !== "private-cookie-changed") ||
        !this.storageActor ||
        !this.storageActor.windows
      ) {
        return null;
      }

      const hosts = this.getMatchingHosts(subject);
      const data = {};

      switch (action) {
        case "added":
        case "changed":
          if (hosts.length) {
            for (const host of hosts) {
              const uniqueKey =
                `${subject.name}${SEPARATOR_GUID}${subject.host}` +
                `${SEPARATOR_GUID}${subject.path}`;

              this.hostVsStores.get(host).set(uniqueKey, subject);
              data[host] = [uniqueKey];
            }
            this.storageActor.update(action, "cookies", data);
          }
          break;

        case "deleted":
          if (hosts.length) {
            for (const host of hosts) {
              const uniqueKey =
                `${subject.name}${SEPARATOR_GUID}${subject.host}` +
                `${SEPARATOR_GUID}${subject.path}`;

              this.hostVsStores.get(host).delete(uniqueKey);
              data[host] = [uniqueKey];
            }
            this.storageActor.update("deleted", "cookies", data);
          }
          break;

        case "batch-deleted":
          if (hosts.length) {
            for (const host of hosts) {
              const stores = [];
              for (const cookie of subject) {
                const uniqueKey =
                  `${cookie.name}${SEPARATOR_GUID}${cookie.host}` +
                  `${SEPARATOR_GUID}${cookie.path}`;

                this.hostVsStores.get(host).delete(uniqueKey);
                stores.push(uniqueKey);
              }
              data[host] = stores;
            }
            this.storageActor.update("deleted", "cookies", data);
          }
          break;

        case "cleared":
          if (hosts.length) {
            for (const host of hosts) {
              data[host] = [];
            }
            this.storageActor.update("cleared", "cookies", data);
          }
          break;
      }
      return null;
    },

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
    },

    /**
     * Pass the editItem command from the content to the chrome process.
     *
     * @param {Object} data
     *        See editCookie() for format details.
     */
    async editItem(data) {
      data.originAttributes = this.getOriginAttributesFromHost(data.host);
      this.editCookie(data);
    },

    async addItem(guid, host) {
      const window = this.storageActor.getWindowFromHost(host);
      const principal = window.document.effectiveStoragePrincipal;
      this.addCookie(guid, principal);
    },

    async removeItem(host, name) {
      const originAttributes = this.getOriginAttributesFromHost(host);
      this.removeCookie(host, name, originAttributes);
    },

    async removeAll(host, domain) {
      const originAttributes = this.getOriginAttributesFromHost(host);
      this.removeAllCookies(host, domain, originAttributes);
    },

    async removeAllSessionCookies(host, domain) {
      const originAttributes = this.getOriginAttributesFromHost(host);
      this._removeCookies(host, { domain, originAttributes, session: true });
    },

    getCookiesFromHost(host, originAttributes) {
      // Local files have no host.
      if (host.startsWith("file:///")) {
        host = "";
      }

      host = trimHttpHttpsPort(host);

      return Services.cookies.getCookiesFromHost(host, originAttributes);
    },

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
      const domain = principal.asciiHost
        ? principal.host
        : principal.baseDomain;
      const path = principal.filePath.startsWith("/")
        ? principal.filePath
        : "/";

      Services.cookies.add(
        domain,
        path,
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
    },

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
    },

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
    },

    removeCookie(host, name, originAttributes) {
      if (name !== undefined) {
        this._removeCookies(host, { name, originAttributes });
      }
    },

    removeAllCookies(host, domain, originAttributes) {
      this._removeCookies(host, { domain, originAttributes });
    },

    addCookieObservers() {
      Services.obs.addObserver(this, "cookie-changed");
      Services.obs.addObserver(this, "private-cookie-changed");
      return null;
    },

    removeCookieObservers() {
      Services.obs.removeObserver(this, "cookie-changed");
      Services.obs.removeObserver(this, "private-cookie-changed");
      return null;
    },

    observe(subject, topic, data) {
      if (!subject) {
        return;
      }

      switch (topic) {
        case "cookie-changed":
        case "private-cookie-changed":
          if (data === "batch-deleted") {
            const cookiesNoInterface = subject.QueryInterface(Ci.nsIArray);
            const cookies = [];

            for (let i = 0; i < cookiesNoInterface.length; i++) {
              const cookie = cookiesNoInterface.queryElementAt(i, Ci.nsICookie);
              cookies.push(cookie);
            }
            this.onCookieChanged(cookies, topic, data);

            return;
          }

          const cookie = subject.QueryInterface(Ci.nsICookie);
          this.onCookieChanged(cookie, topic, data);
          break;
      }
    },
  }
);

/**
 * Helper method to create the overriden object required in
 * StorageActors.createActor for Local Storage and Session Storage.
 * This method exists as both Local Storage and Session Storage have almost
 * identical actors.
 */
function getObjectForLocalOrSessionStorage(type) {
  return {
    getNamesForHost(host) {
      const storage = this.hostVsStores.get(host);
      return storage ? Object.keys(storage) : [];
    },

    getValuesForHost(host, name) {
      const storage = this.hostVsStores.get(host);
      if (!storage) {
        return [];
      }
      if (name) {
        const value = storage ? storage.getItem(name) : null;
        return [{ name, value }];
      }
      if (!storage) {
        return [];
      }

      // local and session storage cannot be iterated over using Object.keys()
      // because it skips keys that are duplicated on the prototype
      // e.g. "key", "getKeys" so we need to gather the real keys using the
      // storage.key() function.
      const storageArray = [];
      for (let i = 0; i < storage.length; i++) {
        const key = storage.key(i);
        storageArray.push({
          name: key,
          value: storage.getItem(key),
        });
      }
      return storageArray;
    },

    populateStoresForHost(host, window) {
      try {
        this.hostVsStores.set(host, window[type]);
      } catch (ex) {
        console.warn(`Failed to enumerate ${type} for host ${host}: ${ex}`);
      }
    },

    populateStoresForHosts() {
      this.hostVsStores = new Map();
      for (const window of this.windows) {
        const host = this.getHostName(window.location);
        if (host) {
          this.populateStoresForHost(host, window);
        }
      }
    },

    async getFields() {
      return [
        { name: "name", editable: true },
        { name: "value", editable: true },
      ];
    },

    async addItem(guid, host) {
      const storage = this.hostVsStores.get(host);
      if (!storage) {
        return;
      }
      storage.setItem(guid, DEFAULT_VALUE);
    },

    /**
     * Edit localStorage or sessionStorage fields.
     *
     * @param {Object} data
     *        See editCookie() for format details.
     */
    async editItem({ host, field, oldValue, items }) {
      const storage = this.hostVsStores.get(host);
      if (!storage) {
        return;
      }

      if (field === "name") {
        storage.removeItem(oldValue);
      }

      storage.setItem(items.name, items.value);
    },

    async removeItem(host, name) {
      const storage = this.hostVsStores.get(host);
      if (!storage) {
        return;
      }
      storage.removeItem(name);
    },

    async removeAll(host) {
      const storage = this.hostVsStores.get(host);
      if (!storage) {
        return;
      }
      storage.clear();
    },

    observe(subject, topic, data) {
      if (
        (topic != "dom-storage2-changed" &&
          topic != "dom-private-storage2-changed") ||
        data != type
      ) {
        return null;
      }

      const host = this.getSchemaAndHost(subject.url);

      if (!this.hostVsStores.has(host)) {
        return null;
      }

      let action = "changed";
      if (subject.key == null) {
        return this.storageActor.update("cleared", type, [host]);
      } else if (subject.oldValue == null) {
        action = "added";
      } else if (subject.newValue == null) {
        action = "deleted";
      }
      const updateData = {};
      updateData[host] = [subject.key];
      return this.storageActor.update(action, type, updateData);
    },

    /**
     * Given a url, correctly determine its protocol + hostname part.
     */
    getSchemaAndHost(url) {
      const uri = Services.io.newURI(url);
      if (!uri.host) {
        return uri.spec;
      }
      return uri.scheme + "://" + uri.hostPort;
    },

    toStoreObject(item) {
      if (!item) {
        return null;
      }

      return {
        name: item.name,
        value: new LongStringActor(this.conn, item.value || ""),
      };
    },
  };
}

/**
 * The Local Storage actor and front.
 */
StorageActors.createActor(
  {
    typeName: "localStorage",
    observationTopics: ["dom-storage2-changed", "dom-private-storage2-changed"],
  },
  getObjectForLocalOrSessionStorage("localStorage")
);

/**
 * The Session Storage actor and front.
 */
StorageActors.createActor(
  {
    typeName: "sessionStorage",
    observationTopics: ["dom-storage2-changed", "dom-private-storage2-changed"],
  },
  getObjectForLocalOrSessionStorage("sessionStorage")
);

const extensionStorageHelpers = {
  /**
   * Editing is supported only for serializable types. Examples of unserializable
   * types include Map, Set and ArrayBuffer.
   */
  isEditable(value) {
    // Bug 1542038: the managed storage area is never editable
    for (const { test } of Object.values(this.supportedTypes)) {
      if (test(value)) {
        return true;
      }
    }
    return false;
  },
  isPrimitive(value) {
    const primitiveValueTypes = ["string", "number", "boolean"];
    return primitiveValueTypes.includes(typeof value) || value === null;
  },
  isObjectLiteral(value) {
    return (
      value &&
      typeof value === "object" &&
      Cu.getClassName(value, true) === "Object"
    );
  },
  // Nested arrays or object literals are only editable 2 levels deep
  isArrayOrObjectLiteralEditable(obj) {
    const topLevelValuesArr = Array.isArray(obj) ? obj : Object.values(obj);
    if (
      topLevelValuesArr.some(
        value =>
          !this.isPrimitive(value) &&
          !Array.isArray(value) &&
          !this.isObjectLiteral(value)
      )
    ) {
      // At least one value is too complex to parse
      return false;
    }
    const arrayOrObjects = topLevelValuesArr.filter(
      value => Array.isArray(value) || this.isObjectLiteral(value)
    );
    if (arrayOrObjects.length === 0) {
      // All top level values are primitives
      return true;
    }

    // One or more top level values was an array or object literal.
    // All of these top level values must themselves have only primitive values
    // for the object to be editable
    for (const nestedObj of arrayOrObjects) {
      const secondLevelValuesArr = Array.isArray(nestedObj)
        ? nestedObj
        : Object.values(nestedObj);
      if (secondLevelValuesArr.some(value => !this.isPrimitive(value))) {
        return false;
      }
    }
    return true;
  },
  typesFromString: {
    // Helper methods to parse string values in editItem
    jsonifiable: {
      test(str) {
        try {
          JSON.parse(str);
        } catch (e) {
          return false;
        }
        return true;
      },
      parse(str) {
        return JSON.parse(str);
      },
    },
  },
  supportedTypes: {
    // Helper methods to determine the value type of an item in isEditable
    array: {
      test(value) {
        if (Array.isArray(value)) {
          return extensionStorageHelpers.isArrayOrObjectLiteralEditable(value);
        }
        return false;
      },
    },
    boolean: {
      test(value) {
        return typeof value === "boolean";
      },
    },
    null: {
      test(value) {
        return value === null;
      },
    },
    number: {
      test(value) {
        return typeof value === "number";
      },
    },
    object: {
      test(value) {
        if (extensionStorageHelpers.isObjectLiteral(value)) {
          return extensionStorageHelpers.isArrayOrObjectLiteralEditable(value);
        }
        return false;
      },
    },
    string: {
      test(value) {
        return typeof value === "string";
      },
    },
  },
};

/**
 * The Extension Storage actor.
 */
StorageActors.createActor(
  {
    typeName: "extensionStorage",
  },
  {
    initialize(storageActor) {
      protocol.Actor.prototype.initialize.call(this, null);

      this.storageActor = storageActor;

      this.addonId = this.storageActor.parentActor.addonId;

      // Retrieve the base moz-extension url for the extension
      // (and also remove the final '/' from it).
      this.extensionHostURL = this.getExtensionPolicy()
        .getURL()
        .slice(0, -1);

      // Map<host, ExtensionStorageIDB db connection>
      // Bug 1542038, 1542039: Each storage area will need its own
      // dbConnectionForHost, as they each have different storage backends.
      // Anywhere dbConnectionForHost is used, we need to know the storage
      // area to access the correct database.
      this.dbConnectionForHost = new Map();

      // Bug 1542038, 1542039: Each storage area will need its own
      // this.hostVsStores or this actor will need to deviate from how
      // this.hostVsStores is defined in the framework to associate each
      // storage item with a storage area. Any methods that use it will also
      // need to be updated (e.g. getNamesForHost).
      this.hostVsStores = new Map();

      this.onStorageChange = this.onStorageChange.bind(this);

      this.onWindowReady = this.onWindowReady.bind(this);
      this.onWindowDestroyed = this.onWindowDestroyed.bind(this);
      this.storageActor.on("window-ready", this.onWindowReady);
      this.storageActor.on("window-destroyed", this.onWindowDestroyed);
    },

    getExtensionPolicy() {
      return WebExtensionPolicy.getByID(this.addonId);
    },

    destroy() {
      ExtensionStorageIDB.removeOnChangedListener(
        this.addonId,
        this.onStorageChange
      );

      this.storageActor.off("window-ready", this.onWindowReady);
      this.storageActor.off("window-destroyed", this.onWindowDestroyed);

      this.hostVsStores.clear();
      protocol.Actor.prototype.destroy.call(this);

      this.storageActor = null;
    },

    /**
     * This fires when the extension changes storage data while the storage
     * inspector is open. Ensures this.hostVsStores stays up-to-date and
     * passes the changes on to update the client.
     */
    onStorageChange(changes) {
      const host = this.extensionHostURL;
      const storeMap = this.hostVsStores.get(host);

      function isStructuredCloneHolder(value) {
        return (
          value &&
          typeof value === "object" &&
          Cu.getClassName(value, true) === "StructuredCloneHolder"
        );
      }

      for (const key in changes) {
        const storageChange = changes[key];
        let { newValue, oldValue } = storageChange;
        if (isStructuredCloneHolder(newValue)) {
          newValue = newValue.deserialize(this);
        }
        if (isStructuredCloneHolder(oldValue)) {
          oldValue = oldValue.deserialize(this);
        }

        let action;
        if (typeof newValue === "undefined") {
          action = "deleted";
          storeMap.delete(key);
        } else if (typeof oldValue === "undefined") {
          action = "added";
          storeMap.set(key, newValue);
        } else {
          action = "changed";
          storeMap.set(key, newValue);
        }

        this.storageActor.update(action, this.typeName, { [host]: [key] });
      }
    },

    /**
     * Purpose of this method is same as populateStoresForHosts but this is async.
     * This exact same operation cannot be performed in populateStoresForHosts
     * method, as that method is called in initialize method of the actor, which
     * cannot be asynchronous.
     */
    async preListStores() {
      // Ensure the actor's target is an extension and it is enabled
      if (!this.addonId || !this.getExtensionPolicy()) {
        return;
      }

      // Subscribe a listener for event notifications from the WE storage API when
      // storage local data has been changed by the extension, and keep track of the
      // listener to remove it when the debugger is being disconnected.
      ExtensionStorageIDB.addOnChangedListener(
        this.addonId,
        this.onStorageChange
      );
      try {
        // Make sure the extension storage APIs have been loaded,
        // otherwise the DevTools storage panel would not be updated
        // automatically when the extension storage data is being changed
        // if the parent ext-storage.js module wasn't already loaded
        // (See Bug 1802929).
        const { extension } = WebExtensionPolicy.getByID(this.addonId);
        await extension.apiManager.asyncGetAPI("storage", extension);
      } catch (e) {
        console.error(
          "Exception while trying to initialize webext storage API",
          e
        );
      }

      await this.populateStoresForHost(this.extensionHostURL);
    },

    /**
     * This method is overriden and left blank as for extensionStorage, this operation
     * cannot be performed synchronously. Thus, the preListStores method exists to
     * do the same task asynchronously.
     */
    populateStoresForHosts() {},

    /**
     * This method asynchronously reads the storage data for the target extension
     * and caches this data into this.hostVsStores.
     * @param {String} host - the hostname for the extension
     */
    async populateStoresForHost(host) {
      if (host !== this.extensionHostURL) {
        return;
      }

      const extension = ExtensionProcessScript.getExtensionChild(this.addonId);
      if (!extension || !extension.hasPermission("storage")) {
        return;
      }

      // Make sure storeMap is defined and set in this.hostVsStores before subscribing
      // a storage onChanged listener in the parent process
      const storeMap = new Map();
      this.hostVsStores.set(host, storeMap);

      const storagePrincipal = await this.getStoragePrincipal();

      if (!storagePrincipal) {
        // This could happen if the extension fails to be migrated to the
        // IndexedDB backend
        return;
      }

      const db = await ExtensionStorageIDB.open(storagePrincipal);
      this.dbConnectionForHost.set(host, db);
      const data = await db.get();

      for (const [key, value] of Object.entries(data)) {
        storeMap.set(key, value);
      }

      if (this.storageActor.parentActor.fallbackWindow) {
        // Show the storage actor in the add-on storage inspector even when there
        // is no extension page currently open
        // This strategy may need to change depending on the outcome of Bug 1597900
        const storageData = {};
        storageData[host] = this.getNamesForHost(host);
        this.storageActor.update("added", this.typeName, storageData);
      }
    },

    async getStoragePrincipal() {
      const { extension } = this.getExtensionPolicy();
      const {
        backendEnabled,
        storagePrincipal,
      } = await ExtensionStorageIDB.selectBackend({ extension });

      if (!backendEnabled) {
        // IDB backend disabled; give up.
        return null;
      }

      // Received as a StructuredCloneHolder, so we need to deserialize
      return storagePrincipal.deserialize(this, true);
    },

    getValuesForHost(host, name) {
      const result = [];

      if (!this.hostVsStores.has(host)) {
        return result;
      }

      if (name) {
        return [{ name, value: this.hostVsStores.get(host).get(name) }];
      }

      for (const [key, value] of Array.from(
        this.hostVsStores.get(host).entries()
      )) {
        result.push({ name: key, value });
      }
      return result;
    },

    /**
     * Converts a storage item to an "extensionobject" as defined in
     * devtools/shared/specs/storage.js. Behavior largely mirrors the "indexedDB" storage actor,
     * except where it would throw an unhandled error (i.e. for a `BigInt` or `undefined`
     * `item.value`).
     * @param {Object} item - The storage item to convert
     * @param {String} item.name - The storage item key
     * @param {*} item.value - The storage item value
     * @return {extensionobject}
     */
    toStoreObject(item) {
      if (!item) {
        return null;
      }

      let { name, value } = item;
      const isValueEditable = extensionStorageHelpers.isEditable(value);

      // `JSON.stringify()` throws for `BigInt`, adds extra quotes to strings and `Date` strings,
      // and doesn't modify `undefined`.
      switch (typeof value) {
        case "bigint":
          value = `${value.toString()}n`;
          break;
        case "string":
          break;
        case "undefined":
          value = "undefined";
          break;
        default:
          value = JSON.stringify(value);
          if (
            // can't use `instanceof` across frame boundaries
            Object.prototype.toString.call(item.value) === "[object Date]"
          ) {
            value = JSON.parse(value);
          }
      }

      return {
        name,
        value: new LongStringActor(this.conn, value),
        area: "local", // Bug 1542038, 1542039: set the correct storage area
        isValueEditable,
      };
    },

    getFields() {
      return [
        { name: "name", editable: false },
        { name: "value", editable: true },
        { name: "area", editable: false },
        { name: "isValueEditable", editable: false, private: true },
      ];
    },

    onItemUpdated(action, host, names) {
      this.storageActor.update(action, this.typeName, {
        [host]: names,
      });
    },

    async editItem({ host, field, items, oldValue }) {
      const db = this.dbConnectionForHost.get(host);
      if (!db) {
        return;
      }

      const { name, value } = items;

      let parsedValue = parseItemValue(value);
      if (parsedValue === value) {
        const { typesFromString } = extensionStorageHelpers;
        for (const { test, parse } of Object.values(typesFromString)) {
          if (test(value)) {
            parsedValue = parse(value);
            break;
          }
        }
      }
      const changes = await db.set({ [name]: parsedValue });
      this.fireOnChangedExtensionEvent(host, changes);

      this.onItemUpdated("changed", host, [name]);
    },

    async removeItem(host, name) {
      const db = this.dbConnectionForHost.get(host);
      if (!db) {
        return;
      }

      const changes = await db.remove(name);
      this.fireOnChangedExtensionEvent(host, changes);

      this.onItemUpdated("deleted", host, [name]);
    },

    async removeAll(host) {
      const db = this.dbConnectionForHost.get(host);
      if (!db) {
        return;
      }

      const changes = await db.clear();
      this.fireOnChangedExtensionEvent(host, changes);

      this.onItemUpdated("cleared", host, []);
    },

    /**
     * Let the extension know that storage data has been changed by the user from
     * the storage inspector.
     */
    fireOnChangedExtensionEvent(host, changes) {
      // Bug 1542038, 1542039: Which message to send depends on the storage area
      const uuid = new URL(host).host;
      Services.cpmm.sendAsyncMessage(
        `Extension:StorageLocalOnChanged:${uuid}`,
        changes
      );
    },
  }
);

StorageActors.createActor(
  {
    typeName: "Cache",
  },
  {
    async getCachesForHost(host) {
      const win = this.storageActor.getWindowFromHost(host);
      if (!win) {
        return null;
      }

      const principal = win.document.effectiveStoragePrincipal;

      // The first argument tells if you want to get |content| cache or |chrome|
      // cache.
      // The |content| cache is the cache explicitely named by the web content
      // (service worker or web page).
      // The |chrome| cache is the cache implicitely cached by the platform,
      // hosting the source file of the service worker.
      const { CacheStorage } = win;

      if (!CacheStorage) {
        return null;
      }

      const cache = new CacheStorage("content", principal);
      return cache;
    },

    async preListStores() {
      for (const host of this.hosts) {
        await this.populateStoresForHost(host);
      }
    },

    form() {
      const hosts = {};
      for (const host of this.hosts) {
        hosts[host] = this.getNamesForHost(host);
      }

      return {
        actor: this.actorID,
        hosts,
        traits: this._getTraits(),
      };
    },

    getNamesForHost(host) {
      // UI code expect each name to be a JSON string of an array :/
      return [...this.hostVsStores.get(host).keys()].map(a => {
        return JSON.stringify([a]);
      });
    },

    async getValuesForHost(host, name) {
      if (!name) {
        // if we get here, we most likely clicked on the refresh button
        // which called getStoreObjects, itself calling this method,
        // all that, without having selected any particular cache name.
        //
        // Try to detect if a new cache has been added and notify the client
        // asynchronously, via a RDP event.
        const previousCaches = [...this.hostVsStores.get(host).keys()];
        await this.preListStores();
        const updatedCaches = [...this.hostVsStores.get(host).keys()];
        const newCaches = updatedCaches.filter(
          cacheName => !previousCaches.includes(cacheName)
        );
        newCaches.forEach(cacheName =>
          this.onItemUpdated("added", host, [cacheName])
        );
        const removedCaches = previousCaches.filter(
          cacheName => !updatedCaches.includes(cacheName)
        );
        removedCaches.forEach(cacheName =>
          this.onItemUpdated("deleted", host, [cacheName])
        );
        return [];
      }
      // UI is weird and expect a JSON stringified array... and pass it back :/
      name = JSON.parse(name)[0];

      const cache = this.hostVsStores.get(host).get(name);
      const requests = await cache.keys();
      const results = [];
      for (const request of requests) {
        let response = await cache.match(request);
        // Unwrap the response to get access to all its properties if the
        // response happen to be 'opaque', when it is a Cross Origin Request.
        response = response.cloneUnfiltered();
        results.push(await this.processEntry(request, response));
      }
      return results;
    },

    async processEntry(request, response) {
      return {
        url: String(request.url),
        status: String(response.statusText),
      };
    },

    async getFields() {
      return [
        { name: "url", editable: false },
        { name: "status", editable: false },
      ];
    },

    async populateStoresForHost(host) {
      const storeMap = new Map();
      const caches = await this.getCachesForHost(host);
      try {
        for (const name of await caches.keys()) {
          storeMap.set(name, await caches.open(name));
        }
      } catch (ex) {
        console.warn(
          `Failed to enumerate CacheStorage for host ${host}: ${ex}`
        );
      }
      this.hostVsStores.set(host, storeMap);
    },

    /**
     * This method is overriden and left blank as for Cache Storage, this
     * operation cannot be performed synchronously. Thus, the preListStores
     * method exists to do the same task asynchronously.
     */
    populateStoresForHosts() {
      this.hostVsStores = new Map();
    },

    /**
     * Given a url, correctly determine its protocol + hostname part.
     */
    getSchemaAndHost(url) {
      const uri = Services.io.newURI(url);
      return uri.scheme + "://" + uri.hostPort;
    },

    toStoreObject(item) {
      return item;
    },

    async removeItem(host, name) {
      const cacheMap = this.hostVsStores.get(host);
      if (!cacheMap) {
        return;
      }

      const parsedName = JSON.parse(name);

      if (parsedName.length == 1) {
        // Delete the whole Cache object
        const [cacheName] = parsedName;
        cacheMap.delete(cacheName);
        const cacheStorage = await this.getCachesForHost(host);
        await cacheStorage.delete(cacheName);
        this.onItemUpdated("deleted", host, [cacheName]);
      } else if (parsedName.length == 2) {
        // Delete one cached request
        const [cacheName, url] = parsedName;
        const cache = cacheMap.get(cacheName);
        if (cache) {
          await cache.delete(url);
          this.onItemUpdated("deleted", host, [cacheName, url]);
        }
      }
    },

    async removeAll(host, name) {
      const cacheMap = this.hostVsStores.get(host);
      if (!cacheMap) {
        return;
      }

      const parsedName = JSON.parse(name);

      // Only a Cache object is a valid object to clear
      if (parsedName.length == 1) {
        const [cacheName] = parsedName;
        const cache = cacheMap.get(cacheName);
        if (cache) {
          const keys = await cache.keys();
          await Promise.all(keys.map(key => cache.delete(key)));
          this.onItemUpdated("cleared", host, [cacheName]);
        }
      }
    },

    /**
     * CacheStorage API doesn't support any notifications, we must fake them
     */
    onItemUpdated(action, host, path) {
      this.storageActor.update(action, "Cache", {
        [host]: [JSON.stringify(path)],
      });
    },
  }
);

/**
 * Code related to the Indexed DB actor and front
 */

// Metadata holder objects for various components of Indexed DB

/**
 * Meta data object for a particular index in an object store
 *
 * @param {IDBIndex} index
 *        The particular index from the object store.
 */
function IndexMetadata(index) {
  this._name = index.name;
  this._keyPath = index.keyPath;
  this._unique = index.unique;
  this._multiEntry = index.multiEntry;
}
IndexMetadata.prototype = {
  toObject() {
    return {
      name: this._name,
      keyPath: this._keyPath,
      unique: this._unique,
      multiEntry: this._multiEntry,
    };
  },
};

/**
 * Meta data object for a particular object store in a db
 *
 * @param {IDBObjectStore} objectStore
 *        The particular object store from the db.
 */
function ObjectStoreMetadata(objectStore) {
  this._name = objectStore.name;
  this._keyPath = objectStore.keyPath;
  this._autoIncrement = objectStore.autoIncrement;
  this._indexes = [];

  for (let i = 0; i < objectStore.indexNames.length; i++) {
    const index = objectStore.index(objectStore.indexNames[i]);

    const newIndex = {
      keypath: index.keyPath,
      multiEntry: index.multiEntry,
      name: index.name,
      objectStore: {
        autoIncrement: index.objectStore.autoIncrement,
        indexNames: [...index.objectStore.indexNames],
        keyPath: index.objectStore.keyPath,
        name: index.objectStore.name,
      },
    };

    this._indexes.push([newIndex, new IndexMetadata(index)]);
  }
}
ObjectStoreMetadata.prototype = {
  toObject() {
    return {
      name: this._name,
      keyPath: this._keyPath,
      autoIncrement: this._autoIncrement,
      indexes: JSON.stringify(
        [...this._indexes.values()].map(index => index.toObject())
      ),
    };
  },
};

/**
 * Meta data object for a particular indexed db in a host.
 *
 * @param {string} origin
 *        The host associated with this indexed db.
 * @param {IDBDatabase} db
 *        The particular indexed db.
 * @param {String} storage
 *        Storage type, either "temporary", "default" or "persistent".
 */
function DatabaseMetadata(origin, db, storage) {
  this._origin = origin;
  this._name = db.name;
  this._version = db.version;
  this._objectStores = [];
  this.storage = storage;

  if (db.objectStoreNames.length) {
    const transaction = db.transaction(db.objectStoreNames, "readonly");

    for (let i = 0; i < transaction.objectStoreNames.length; i++) {
      const objectStore = transaction.objectStore(
        transaction.objectStoreNames[i]
      );
      this._objectStores.push([
        transaction.objectStoreNames[i],
        new ObjectStoreMetadata(objectStore),
      ]);
    }
  }
}
DatabaseMetadata.prototype = {
  get objectStores() {
    return this._objectStores;
  },

  toObject() {
    return {
      uniqueKey: `${this._name}${SEPARATOR_GUID}${this.storage}`,
      name: this._name,
      storage: this.storage,
      origin: this._origin,
      version: this._version,
      objectStores: this._objectStores.size,
    };
  },
};

StorageActors.createActor(
  {
    typeName: "indexedDB",
  },
  {
    initialize(storageActor) {
      protocol.Actor.prototype.initialize.call(this, null);

      this.storageActor = storageActor;

      this.objectsSize = {};
      this.storageActor = storageActor;
      this.onWindowReady = this.onWindowReady.bind(this);
      this.onWindowDestroyed = this.onWindowDestroyed.bind(this);

      this.storageActor.on("window-ready", this.onWindowReady);
      this.storageActor.on("window-destroyed", this.onWindowDestroyed);
    },

    destroy() {
      this.hostVsStores.clear();
      this.objectsSize = null;

      this.storageActor.off("window-ready", this.onWindowReady);
      this.storageActor.off("window-destroyed", this.onWindowDestroyed);

      protocol.Actor.prototype.destroy.call(this);

      this.storageActor = null;
    },

    /**
     * Returns a list of currently known hosts for the target window. This list
     * contains unique hosts from the window, all inner windows and all permanent
     * indexedDB hosts defined inside the browser.
     */
    async getHosts() {
      // Add internal hosts to this._internalHosts, which will be picked up by
      // the this.hosts getter. Because this.hosts is a property on the default
      // storage actor and inherited by all storage actors we have to do it this
      // way.
      // Only look up internal hosts if we are in the browser toolbox
      const isBrowserToolbox = this.storageActor.parentActor.isRootActor;

      this._internalHosts = isBrowserToolbox
        ? await this.getInternalHosts()
        : [];

      return this.hosts;
    },

    /**
     * Remove an indexedDB database from given host with a given name.
     */
    async removeDatabase(host, name) {
      const win = this.storageActor.getWindowFromHost(host);
      if (!win) {
        return { error: `Window for host ${host} not found` };
      }

      const principal = win.document.effectiveStoragePrincipal;
      return this.removeDB(host, principal, name);
    },

    async removeAll(host, name) {
      const [db, store] = JSON.parse(name);

      const win = this.storageActor.getWindowFromHost(host);
      if (!win) {
        return;
      }

      const principal = win.document.effectiveStoragePrincipal;
      this.clearDBStore(host, principal, db, store);
    },

    async removeItem(host, name) {
      const [db, store, id] = JSON.parse(name);

      const win = this.storageActor.getWindowFromHost(host);
      if (!win) {
        return;
      }

      const principal = win.document.effectiveStoragePrincipal;
      this.removeDBRecord(host, principal, db, store, id);
    },

    /**
     * This method is overriden and left blank as for indexedDB, this operation
     * cannot be performed synchronously. Thus, the preListStores method exists to
     * do the same task asynchronously.
     */
    populateStoresForHosts() {},

    getNamesForHost(host) {
      const storesForHost = this.hostVsStores.get(host);
      if (!storesForHost) {
        return [];
      }

      const names = [];

      for (const [dbName, { objectStores }] of storesForHost) {
        if (objectStores.size) {
          for (const objectStore of objectStores.keys()) {
            names.push(JSON.stringify([dbName, objectStore]));
          }
        } else {
          names.push(JSON.stringify([dbName]));
        }
      }

      return names;
    },

    /**
     * Returns the total number of entries for various types of requests to
     * getStoreObjects for Indexed DB actor.
     *
     * @param {string} host
     *        The host for the request.
     * @param {array:string} names
     *        Array of stringified name objects for indexed db actor.
     *        The request type depends on the length of any parsed entry from this
     *        array. 0 length refers to request for the whole host. 1 length
     *        refers to request for a particular db in the host. 2 length refers
     *        to a particular object store in a db in a host. 3 length refers to
     *        particular items of an object store in a db in a host.
     * @param {object} options
     *        An options object containing following properties:
     *         - index {string} The IDBIndex for the object store in the db.
     */
    getObjectsSize(host, names, options) {
      // In Indexed DB, we are interested in only the first name, as the pattern
      // should follow in all entries.
      const name = names[0];
      const parsedName = JSON.parse(name);

      if (parsedName.length == 3) {
        // This is the case where specific entries from an object store were
        // requested
        return names.length;
      } else if (parsedName.length == 2) {
        // This is the case where all entries from an object store are requested.
        const index = options.index;
        const [db, objectStore] = parsedName;
        if (this.objectsSize[host + db + objectStore + index]) {
          return this.objectsSize[host + db + objectStore + index];
        }
      } else if (parsedName.length == 1) {
        // This is the case where details of all object stores in a db are
        // requested.
        if (
          this.hostVsStores.has(host) &&
          this.hostVsStores.get(host).has(parsedName[0])
        ) {
          return this.hostVsStores.get(host).get(parsedName[0]).objectStores
            .size;
        }
      } else if (!parsedName || !parsedName.length) {
        // This is the case were details of all dbs in a host are requested.
        if (this.hostVsStores.has(host)) {
          return this.hostVsStores.get(host).size;
        }
      }
      return 0;
    },

    /**
     * Purpose of this method is same as populateStoresForHosts but this is async.
     * This exact same operation cannot be performed in populateStoresForHosts
     * method, as that method is called in initialize method of the actor, which
     * cannot be asynchronous.
     */
    async preListStores() {
      this.hostVsStores = new Map();
      for (const host of await this.getHosts()) {
        await this.populateStoresForHost(host);
      }
    },

    async populateStoresForHost(host) {
      const storeMap = new Map();

      const win = this.storageActor.getWindowFromHost(host);
      const principal = this.getPrincipal(win);

      const { names } = await this.getDBNamesForHost(host, principal);

      for (const { name, storage } of names) {
        let metadata = await this.getDBMetaData(host, principal, name, storage);

        metadata = this.patchMetadataMapsAndProtos(metadata);

        storeMap.set(`${name} (${storage})`, metadata);
      }

      this.hostVsStores.set(host, storeMap);
    },

    /**
     * Returns the over-the-wire implementation of the indexed db entity.
     */
    toStoreObject(item) {
      if (!item) {
        return null;
      }

      if ("indexes" in item) {
        // Object store meta data
        return {
          objectStore: item.name,
          keyPath: item.keyPath,
          autoIncrement: item.autoIncrement,
          indexes: item.indexes,
        };
      }
      if ("objectStores" in item) {
        // DB meta data
        return {
          uniqueKey: `${item.name} (${item.storage})`,
          db: item.name,
          storage: item.storage,
          origin: item.origin,
          version: item.version,
          objectStores: item.objectStores,
        };
      }

      const value = JSON.stringify(item.value);

      // Indexed db entry
      return {
        name: item.name,
        value: new LongStringActor(this.conn, value),
      };
    },

    form() {
      const hosts = {};
      for (const host of this.hosts) {
        hosts[host] = this.getNamesForHost(host);
      }

      return {
        actor: this.actorID,
        hosts,
        traits: this._getTraits(),
      };
    },

    onItemUpdated(action, host, path) {
      // Database was removed, remove it from stores map
      if (action === "deleted" && path.length === 1) {
        if (this.hostVsStores.has(host)) {
          this.hostVsStores.get(host).delete(path[0]);
        }
      }

      this.storageActor.update(action, "indexedDB", {
        [host]: [JSON.stringify(path)],
      });
    },

    async getFields(subType) {
      switch (subType) {
        // Detail of database
        case "database":
          return [
            { name: "objectStore", editable: false },
            { name: "keyPath", editable: false },
            { name: "autoIncrement", editable: false },
            { name: "indexes", editable: false },
          ];

        // Detail of object store
        case "object store":
          return [
            { name: "name", editable: false },
            { name: "value", editable: false },
          ];

        // Detail of indexedDB for one origin
        default:
          return [
            { name: "uniqueKey", editable: false, private: true },
            { name: "db", editable: false },
            { name: "storage", editable: false },
            { name: "origin", editable: false },
            { name: "version", editable: false },
            { name: "objectStores", editable: false },
          ];
      }
    },

    /**
     * Fetches and stores all the metadata information for the given database
     * `name` for the given `host` with its `principal`. The stored metadata
     * information is of `DatabaseMetadata` type.
     */
    async getDBMetaData(host, principal, name, storage) {
      const request = this.openWithPrincipal(principal, name, storage);
      return new Promise(resolve => {
        request.onsuccess = event => {
          const db = event.target.result;
          const dbData = new DatabaseMetadata(host, db, storage);
          db.close();

          resolve(dbData);
        };
        request.onerror = ({ target }) => {
          console.error(
            `Error opening indexeddb database ${name} for host ${host}`,
            target.error
          );
          resolve(null);
        };
      });
    },

    splitNameAndStorage(name) {
      const lastOpenBracketIndex = name.lastIndexOf("(");
      const lastCloseBracketIndex = name.lastIndexOf(")");
      const delta = lastCloseBracketIndex - lastOpenBracketIndex - 1;

      const storage = name.substr(lastOpenBracketIndex + 1, delta);

      name = name.substr(0, lastOpenBracketIndex - 1);

      return { storage, name };
    },

    /**
     * Get all "internal" hosts. Internal hosts are database namespaces used by
     * the browser.
     */
    async getInternalHosts() {
      const profileDir = PathUtils.profileDir;
      const storagePath = PathUtils.join(profileDir, "storage", "permanent");
      const children = await IOUtils.getChildren(storagePath);
      const hosts = [];

      for (const path of children) {
        const exists = await IOUtils.exists(path);
        if (!exists) {
          continue;
        }

        const stats = await IOUtils.stat(path);
        if (
          stats.type === "directory" &&
          !SAFE_HOSTS_PREFIXES_REGEX.test(stats.path)
        ) {
          const basename = PathUtils.filename(path);
          hosts.push(basename);
        }
      }

      return hosts;
    },

    /**
     * Opens an indexed db connection for the given `principal` and
     * database `name`.
     */
    openWithPrincipal(principal, name, storage) {
      return indexedDBForStorage.openForPrincipal(principal, name, {
        storage,
      });
    },

    async removeDB(host, principal, dbName) {
      const result = new Promise(resolve => {
        const { name, storage } = this.splitNameAndStorage(dbName);
        const request = indexedDBForStorage.deleteForPrincipal(
          principal,
          name,
          {
            storage,
          }
        );

        request.onsuccess = () => {
          resolve({});
          this.onItemUpdated("deleted", host, [dbName]);
        };

        request.onblocked = () => {
          console.warn(
            `Deleting indexedDB database ${name} for host ${host} is blocked`
          );
          resolve({ blocked: true });
        };

        request.onerror = () => {
          const { error } = request;
          console.warn(
            `Error deleting indexedDB database ${name} for host ${host}: ${error}`
          );
          resolve({ error: error.message });
        };

        // If the database is blocked repeatedly, the onblocked event will not
        // be fired again. To avoid waiting forever, report as blocked if nothing
        // else happens after 3 seconds.
        setTimeout(() => resolve({ blocked: true }), 3000);
      });

      return result;
    },

    async removeDBRecord(host, principal, dbName, storeName, id) {
      let db;
      const { name, storage } = this.splitNameAndStorage(dbName);

      try {
        db = await new Promise((resolve, reject) => {
          const request = this.openWithPrincipal(principal, name, storage);
          request.onsuccess = ev => resolve(ev.target.result);
          request.onerror = ev => reject(ev.target.error);
        });

        const transaction = db.transaction(storeName, "readwrite");
        const store = transaction.objectStore(storeName);

        await new Promise((resolve, reject) => {
          const request = store.delete(id);
          request.onsuccess = () => resolve();
          request.onerror = ev => reject(ev.target.error);
        });

        this.onItemUpdated("deleted", host, [dbName, storeName, id]);
      } catch (error) {
        const recordPath = [dbName, storeName, id].join("/");
        console.error(
          `Failed to delete indexedDB record: ${recordPath}: ${error}`
        );
      }

      if (db) {
        db.close();
      }

      return null;
    },

    async clearDBStore(host, principal, dbName, storeName) {
      let db;
      const { name, storage } = this.splitNameAndStorage(dbName);

      try {
        db = await new Promise((resolve, reject) => {
          const request = this.openWithPrincipal(principal, name, storage);
          request.onsuccess = ev => resolve(ev.target.result);
          request.onerror = ev => reject(ev.target.error);
        });

        const transaction = db.transaction(storeName, "readwrite");
        const store = transaction.objectStore(storeName);

        await new Promise((resolve, reject) => {
          const request = store.clear();
          request.onsuccess = () => resolve();
          request.onerror = ev => reject(ev.target.error);
        });

        this.onItemUpdated("cleared", host, [dbName, storeName]);
      } catch (error) {
        const storePath = [dbName, storeName].join("/");
        console.error(
          `Failed to clear indexedDB store: ${storePath}: ${error}`
        );
      }

      if (db) {
        db.close();
      }

      return null;
    },

    /**
     * Fetches all the databases and their metadata for the given `host`.
     */
    async getDBNamesForHost(host, principal) {
      const sanitizedHost =
        this.getSanitizedHost(host) + principal.originSuffix;
      const profileDir = PathUtils.profileDir;
      const storagePath = PathUtils.join(profileDir, "storage");
      const files = [];
      const names = [];

      // We expect sqlite DB paths to look something like this:
      // - PathToProfileDir/storage/default/http+++www.example.com/
      //   idb/1556056096MeysDaabta.sqlite
      // - PathToProfileDir/storage/permanent/http+++www.example.com/
      //   idb/1556056096MeysDaabta.sqlite
      // - PathToProfileDir/storage/temporary/http+++www.example.com/
      //   idb/1556056096MeysDaabta.sqlite
      // The subdirectory inside the storage folder is determined by the storage
      // type:
      // - default:   { storage: "default" } or not specified.
      // - permanent: { storage: "persistent" }.
      // - temporary: { storage: "temporary" }.
      const sqliteFiles = await this.findSqlitePathsForHost(
        storagePath,
        sanitizedHost
      );

      for (const file of sqliteFiles) {
        const splitPath = PathUtils.split(file);
        const idbIndex = splitPath.indexOf("idb");
        const storage = splitPath[idbIndex - 2];
        const relative = file.substr(profileDir.length + 1);

        files.push({
          file: relative,
          storage: storage === "permanent" ? "persistent" : storage,
        });
      }

      if (files.length) {
        for (const { file, storage } of files) {
          const name = await this.getNameFromDatabaseFile(file);
          if (name) {
            names.push({
              name,
              storage,
            });
          }
        }
      }

      return { names };
    },

    /**
     * Find all SQLite files that hold IndexedDB data for a host, such as:
     *   storage/temporary/http+++www.example.com/idb/1556056096MeysDaabta.sqlite
     */
    async findSqlitePathsForHost(storagePath, sanitizedHost) {
      const sqlitePaths = [];
      const idbPaths = await this.findIDBPathsForHost(
        storagePath,
        sanitizedHost
      );
      for (const idbPath of idbPaths) {
        const children = await IOUtils.getChildren(idbPath);

        for (const path of children) {
          const exists = await IOUtils.exists(path);
          if (!exists) {
            continue;
          }

          const stats = await IOUtils.stat(path);
          if (stats.type !== "directory" && stats.path.endsWith(".sqlite")) {
            sqlitePaths.push(path);
          }
        }
      }
      return sqlitePaths;
    },

    /**
     * Find all paths that hold IndexedDB data for a host, such as:
     *   storage/temporary/http+++www.example.com/idb
     */
    async findIDBPathsForHost(storagePath, sanitizedHost) {
      const idbPaths = [];
      const typePaths = await this.findStorageTypePaths(storagePath);
      for (const typePath of typePaths) {
        const idbPath = PathUtils.join(typePath, sanitizedHost, "idb");
        if (await IOUtils.exists(idbPath)) {
          idbPaths.push(idbPath);
        }
      }
      return idbPaths;
    },

    /**
     * Find all the storage types, such as "default", "permanent", or "temporary".
     * These names have changed over time, so it seems simpler to look through all
     * types that currently exist in the profile.
     */
    async findStorageTypePaths(storagePath) {
      const children = await IOUtils.getChildren(storagePath);
      const typePaths = [];

      for (const path of children) {
        const exists = await IOUtils.exists(path);
        if (!exists) {
          continue;
        }

        const stats = await IOUtils.stat(path);
        if (stats.type === "directory") {
          typePaths.push(path);
        }
      }

      return typePaths;
    },

    /**
     * Removes any illegal characters from the host name to make it a valid file
     * name.
     */
    getSanitizedHost(host) {
      if (host.startsWith("about:")) {
        host = "moz-safe-" + host;
      }
      return host.replace(ILLEGAL_CHAR_REGEX, "+");
    },

    /**
     * Retrieves the proper indexed db database name from the provided .sqlite
     * file location.
     */
    async getNameFromDatabaseFile(path) {
      let connection = null;
      let retryCount = 0;

      // Content pages might be having an open transaction for the same indexed db
      // which this sqlite file belongs to. In that case, sqlite.openConnection
      // will throw. Thus we retry for some time to see if lock is removed.
      while (!connection && retryCount++ < 25) {
        try {
          connection = await lazy.Sqlite.openConnection({ path });
        } catch (ex) {
          // Continuously retrying is overkill. Waiting for 100ms before next try
          await sleep(100);
        }
      }

      if (!connection) {
        return null;
      }

      const rows = await connection.execute("SELECT name FROM database");
      if (rows.length != 1) {
        return null;
      }

      const name = rows[0].getResultByName("name");

      await connection.close();

      return name;
    },

    async getValuesForHost(
      host,
      name = "null",
      options,
      hostVsStores,
      principal
    ) {
      name = JSON.parse(name);
      if (!name || !name.length) {
        // This means that details about the db in this particular host are
        // requested.
        const dbs = [];
        if (hostVsStores.has(host)) {
          for (let [, db] of hostVsStores.get(host)) {
            db = this.patchMetadataMapsAndProtos(db);
            dbs.push(db.toObject());
          }
        }
        return { dbs };
      }

      const [db2, objectStore, id] = name;
      if (!objectStore) {
        // This means that details about all the object stores in this db are
        // requested.
        const objectStores = [];
        if (hostVsStores.has(host) && hostVsStores.get(host).has(db2)) {
          let db = hostVsStores.get(host).get(db2);

          db = this.patchMetadataMapsAndProtos(db);

          const objectStores2 = db.objectStores;

          for (const objectStore2 of objectStores2) {
            objectStores.push(objectStore2[1].toObject());
          }
        }
        return {
          objectStores,
        };
      }
      // Get either all entries from the object store, or a particular id
      const storage = hostVsStores.get(host).get(db2).storage;
      const result = await this.getObjectStoreData(
        host,
        principal,
        db2,
        storage,
        {
          objectStore,
          id,
          index: options.index,
          offset: options.offset,
          size: options.size,
        }
      );
      return { result };
    },

    /**
     * Returns requested entries (or at most MAX_STORE_OBJECT_COUNT) from a particular
     * objectStore from the db in the given host.
     *
     * @param {string} host
     *        The given host.
     * @param {nsIPrincipal} principal
     *        The principal of the given document.
     * @param {string} dbName
     *        The name of the indexed db from the above host.
     * @param {String} storage
     *        Storage type, either "temporary", "default" or "persistent".
     * @param {Object} requestOptions
     *        An object in the following format:
     *        {
     *          objectStore: The name of the object store from the above db,
     *          id:          Id of the requested entry from the above object
     *                       store. null if all entries from the above object
     *                       store are requested,
     *          index:       Name of the IDBIndex to be iterated on while fetching
     *                       entries. null or "name" if no index is to be
     *                       iterated,
     *          offset:      offset of the entries to be fetched,
     *          size:        The intended size of the entries to be fetched
     *        }
     */
    getObjectStoreData(host, principal, dbName, storage, requestOptions) {
      const { name } = this.splitNameAndStorage(dbName);
      const request = this.openWithPrincipal(principal, name, storage);

      return new Promise((resolve, reject) => {
        let { objectStore, id, index, offset, size } = requestOptions;
        const data = [];
        let db;

        if (!size || size > MAX_STORE_OBJECT_COUNT) {
          size = MAX_STORE_OBJECT_COUNT;
        }

        request.onsuccess = event => {
          db = event.target.result;

          const transaction = db.transaction(objectStore, "readonly");
          let source = transaction.objectStore(objectStore);
          if (index && index != "name") {
            source = source.index(index);
          }

          source.count().onsuccess = event2 => {
            const objectsSize = [];
            const count = event2.target.result;
            objectsSize.push({
              key: host + dbName + objectStore + index,
              count,
            });

            if (!offset) {
              offset = 0;
            } else if (offset > count) {
              db.close();
              resolve([]);
              return;
            }

            if (id) {
              source.get(id).onsuccess = event3 => {
                db.close();
                resolve([{ name: id, value: event3.target.result }]);
              };
            } else {
              source.openCursor().onsuccess = event4 => {
                const cursor = event4.target.result;

                if (!cursor || data.length >= size) {
                  db.close();
                  resolve({
                    data,
                    objectsSize,
                  });
                  return;
                }
                if (offset-- <= 0) {
                  data.push({ name: cursor.key, value: cursor.value });
                }
                cursor.continue();
              };
            }
          };
        };

        request.onerror = () => {
          db.close();
          resolve([]);
        };
      });
    },

    /**
     * When indexedDB metadata is parsed to and from JSON then the object's
     * prototype is dropped and any Maps are changed to arrays of arrays. This
     * method is used to repair the prototypes and fix any broken Maps.
     */
    patchMetadataMapsAndProtos(metadata) {
      const md = Object.create(DatabaseMetadata.prototype);
      Object.assign(md, metadata);

      md._objectStores = new Map(metadata._objectStores);

      for (const [name, store] of md._objectStores) {
        const obj = Object.create(ObjectStoreMetadata.prototype);
        Object.assign(obj, store);

        md._objectStores.set(name, obj);

        if (typeof store._indexes.length !== "undefined") {
          obj._indexes = new Map(store._indexes);
        }

        for (const [name2, value] of obj._indexes) {
          const obj2 = Object.create(IndexMetadata.prototype);
          Object.assign(obj2, value);

          obj._indexes.set(name2, obj2);
        }
      }

      return md;
    },
  }
);

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
