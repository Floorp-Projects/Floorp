/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* globals StopIteration */

"use strict";

const {Cc, Ci, Cu, CC} = require("chrome");
const events = require("sdk/event/core");
const protocol = require("devtools/shared/protocol");
const {LongStringActor} = require("devtools/server/actors/string");
const {DebuggerServer} = require("devtools/server/main");
const Services = require("Services");
const promise = require("promise");
const {isWindowIncluded} = require("devtools/shared/layout/utils");
const specs = require("devtools/shared/specs/storage");
const { Task } = require("devtools/shared/task");

// GUID to be used as a separator in compound keys. This must match the same
// constant in devtools/client/storage/ui.js,
// devtools/client/storage/test/head.js and
// devtools/server/tests/browser/head.js
const SEPARATOR_GUID = "{9d414cc5-8319-0a04-0586-c0a6ae01670a}";

loader.lazyImporter(this, "OS", "resource://gre/modules/osfile.jsm");
loader.lazyImporter(this, "Sqlite", "resource://gre/modules/Sqlite.jsm");

// We give this a funny name to avoid confusion with the global
// indexedDB.
loader.lazyGetter(this, "indexedDBForStorage", () => {
  // On xpcshell, we can't instantiate indexedDB without crashing
  try {
    let sandbox
      = Cu.Sandbox(CC("@mozilla.org/systemprincipal;1", "nsIPrincipal")(),
                   {wantGlobalProperties: ["indexedDB"]});
    return sandbox.indexedDB;
  } catch (e) {
    return {};
  }
});

// Maximum number of cookies/local storage key-value-pairs that can be sent
// over the wire to the client in one request.
const MAX_STORE_OBJECT_COUNT = 50;
// Delay for the batch job that sends the accumulated update packets to the
// client (ms).
const BATCH_DELAY = 200;

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
  "/:*?\\\"<>|\\\\",
  "]"
].join("");
var ILLEGAL_CHAR_REGEX = new RegExp(illegalFileNameCharacters, "g");

// Holder for all the registered storage actors.
var storageTypePool = new Map();

/**
 * An async method equivalent to setTimeout but using Promises
 *
 * @param {number} time
 *        The wait time in milliseconds.
 */
function sleep(time) {
  let deferred = promise.defer();

  setTimeout(() => {
    deferred.resolve(null);
  }, time);

  return deferred.promise;
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
StorageActors.defaults = function (typeName, observationTopics) {
  return {
    typeName: typeName,

    get conn() {
      return this.storageActor.conn;
    },

    /**
     * Returns a list of currently knwon hosts for the target window. This list
     * contains unique hosts from the window + all inner windows.
     */
    get hosts() {
      let hosts = new Set();
      for (let {location} of this.storageActor.windows) {
        hosts.add(this.getHostName(location));
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
     * Converts the window.location object into host.
     */
    getHostName(location) {
      if (location.protocol === "chrome:") {
        return location.href;
      }
      return location.hostname || location.href;
    },

    initialize(storageActor) {
      protocol.Actor.prototype.initialize.call(this, null);

      this.storageActor = storageActor;

      this.populateStoresForHosts();
      if (observationTopics) {
        observationTopics.forEach((observationTopic) => {
          Services.obs.addObserver(this, observationTopic, false);
        });
      }
      this.onWindowReady = this.onWindowReady.bind(this);
      this.onWindowDestroyed = this.onWindowDestroyed.bind(this);
      events.on(this.storageActor, "window-ready", this.onWindowReady);
      events.on(this.storageActor, "window-destroyed", this.onWindowDestroyed);
    },

    destroy() {
      if (observationTopics) {
        observationTopics.forEach((observationTopic) => {
          Services.obs.removeObserver(this, observationTopic, false);
        });
      }
      events.off(this.storageActor, "window-ready", this.onWindowReady);
      events.off(this.storageActor, "window-destroyed", this.onWindowDestroyed);

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
    onWindowReady: Task.async(function* (window) {
      let host = this.getHostName(window.location);
      if (!this.hostVsStores.has(host)) {
        yield this.populateStoresForHost(host, window);
        let data = {};
        data[host] = this.getNamesForHost(host);
        this.storageActor.update("added", typeName, data);
      }
    }),

    /**
     * When a window is removed from the page. This generally means that an
     * iframe was removed, or the current window reload is triggered.
     *
     * @param {window} window
     *        The window which was removed.
     */
    onWindowDestroyed(window) {
      if (!window.location) {
        // Nothing can be done if location object is null
        return;
      }
      let host = this.getHostName(window.location);
      if (!this.hosts.has(host)) {
        this.hostVsStores.delete(host);
        let data = {};
        data[host] = [];
        this.storageActor.update("deleted", typeName, data);
      }
    },

    form(form, detail) {
      if (detail === "actorid") {
        return this.actorID;
      }

      let hosts = {};
      for (let host of this.hosts) {
        hosts[host] = [];
      }

      return {
        actor: this.actorID,
        hosts: hosts
      };
    },

    /**
     * Populates a map of known hosts vs a map of stores vs value.
     */
    populateStoresForHosts() {
      this.hostVsStores = new Map();
      for (let host of this.hosts) {
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
     *
     * @return {object} An object containing following properties:
     *          - offset - The actual offset of the returned array. This might
     *                     be different from the requested offset if that was
     *                     invalid
     *          - total - The total number of entries possible.
     *          - data - The requested values.
     */
    getStoreObjects: Task.async(function* (host, names, options = {}) {
      let offset = options.offset || 0;
      let size = options.size || MAX_STORE_OBJECT_COUNT;
      if (size > MAX_STORE_OBJECT_COUNT) {
        size = MAX_STORE_OBJECT_COUNT;
      }
      let sortOn = options.sortOn || "name";

      let toReturn = {
        offset: offset,
        total: 0,
        data: []
      };

      let principal = null;
      if (this.typeName === "indexedDB") {
        // We only acquire principal when the type of the storage is indexedDB
        // because the principal only matters the indexedDB.
        let win = this.storageActor.getWindowFromHost(host);
        if (win) {
          principal = win.document.nodePrincipal;
        }
      }

      if (names) {
        for (let name of names) {
          let values = yield this.getValuesForHost(host, name, options,
            this.hostVsStores, principal);

          let {result, objectStores} = values;

          if (result && typeof result.objectsSize !== "undefined") {
            for (let {key, count} of result.objectsSize) {
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
        toReturn.total = this.getObjectsSize(host, names, options);
        if (offset > toReturn.total) {
          // In this case, toReturn.data is an empty array.
          toReturn.offset = toReturn.total;
          toReturn.data = [];
        } else {
          toReturn.data = toReturn.data.sort((a, b) => {
            return a[sortOn] - b[sortOn];
          }).slice(offset, offset + size).map(a => this.toStoreObject(a));
        }
      } else {
        let obj = yield this.getValuesForHost(host, undefined, undefined,
                                              this.hostVsStores, principal);
        if (obj.dbs) {
          obj = obj.dbs;
        }

        toReturn.total = obj.length;
        if (offset > toReturn.total) {
          // In this case, toReturn.data is an empty array.
          toReturn.offset = offset = toReturn.total;
          toReturn.data = [];
        } else {
          toReturn.data = obj.sort((a, b) => {
            return a[sortOn] - b[sortOn];
          }).slice(offset, offset + size)
            .map(object => this.toStoreObject(object));
        }
      }

      return toReturn;
    })
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
StorageActors.createActor = function (options = {}, overrides = {}) {
  let actorObject = StorageActors.defaults(
    options.typeName,
    options.observationTopics || null
  );
  for (let key in overrides) {
    actorObject[key] = overrides[key];
  }

  let actorSpec = specs.childSpecs[options.typeName];
  let actor = protocol.ActorClassWithSpec(actorSpec, actorObject);
  storageTypePool.set(actorObject.typeName, actor);
};

/**
 * The Cookies actor and front.
 */
StorageActors.createActor({
  typeName: "cookies"
}, {
  initialize(storageActor) {
    protocol.Actor.prototype.initialize.call(this, null);

    this.storageActor = storageActor;

    this.maybeSetupChildProcess();
    this.populateStoresForHosts();
    this.addCookieObservers();

    this.onWindowReady = this.onWindowReady.bind(this);
    this.onWindowDestroyed = this.onWindowDestroyed.bind(this);
    events.on(this.storageActor, "window-ready", this.onWindowReady);
    events.on(this.storageActor, "window-destroyed", this.onWindowDestroyed);
  },

  destroy() {
    this.hostVsStores.clear();

    // We need to remove the cookie listeners early in E10S mode so we need to
    // use a conditional here to ensure that we only attempt to remove them in
    // single process mode.
    if (!DebuggerServer.isInChildProcess) {
      this.removeCookieObservers();
    }

    events.off(this.storageActor, "window-ready", this.onWindowReady);
    events.off(this.storageActor, "window-destroyed", this.onWindowDestroyed);

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
    let hosts = new Set();
    for (let host of this.hosts) {
      for (let cookie of cookies) {
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
      uniqueKey: `${cookie.name}${SEPARATOR_GUID}${cookie.host}` +
                 `${SEPARATOR_GUID}${cookie.path}`,
      name: cookie.name,
      host: cookie.host || "",
      path: cookie.path || "",

      // because expires is in seconds
      expires: (cookie.expires || 0) * 1000,

      // because creationTime is in micro seconds
      creationTime: cookie.creationTime / 1000,

      // - do -
      lastAccessed: cookie.lastAccessed / 1000,
      value: new LongStringActor(this.conn, cookie.value || ""),
      isDomain: cookie.isDomain,
      isSecure: cookie.isSecure,
      isHttpOnly: cookie.isHttpOnly
    };
  },

  populateStoresForHost(host) {
    this.hostVsStores.set(host, new Map());
    let doc = this.storageActor.document;

    let cookies = this.getCookiesFromHost(host, doc.nodePrincipal
                                                   .originAttributes);

    for (let cookie of cookies) {
      if (this.isCookieAtHost(cookie, host)) {
        let uniqueKey = `${cookie.name}${SEPARATOR_GUID}${cookie.host}` +
                        `${SEPARATOR_GUID}${cookie.path}`;

        this.hostVsStores.get(host).set(uniqueKey, cookie);
      }
    }
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
    if (topic !== "cookie-changed" ||
        !this.storageActor ||
        !this.storageActor.windows) {
      return null;
    }

    let hosts = this.getMatchingHosts(subject);
    let data = {};

    switch (action) {
      case "added":
      case "changed":
        if (hosts.length) {
          for (let host of hosts) {
            let uniqueKey = `${subject.name}${SEPARATOR_GUID}${subject.host}` +
                            `${SEPARATOR_GUID}${subject.path}`;

            this.hostVsStores.get(host).set(uniqueKey, subject);
            data[host] = [uniqueKey];
          }
          this.storageActor.update(action, "cookies", data);
        }
        break;

      case "deleted":
        if (hosts.length) {
          for (let host of hosts) {
            let uniqueKey = `${subject.name}${SEPARATOR_GUID}${subject.host}` +
                            `${SEPARATOR_GUID}${subject.path}`;

            this.hostVsStores.get(host).delete(uniqueKey);
            data[host] = [uniqueKey];
          }
          this.storageActor.update("deleted", "cookies", data);
        }
        break;

      case "batch-deleted":
        if (hosts.length) {
          for (let host of hosts) {
            let stores = [];
            for (let cookie of subject) {
              let uniqueKey = `${cookie.name}${SEPARATOR_GUID}${cookie.host}` +
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
          for (let host of hosts) {
            data[host] = [];
          }
          this.storageActor.update("cleared", "cookies", data);
        }
        break;
    }
    return null;
  },

  getFields: Task.async(function* () {
    return [
      { name: "uniqueKey", editable: false, private: true },
      { name: "name", editable: true, hidden: false },
      { name: "host", editable: true, hidden: false },
      { name: "path", editable: true, hidden: false },
      { name: "expires", editable: true, hidden: false },
      { name: "lastAccessed", editable: false, hidden: false },
      { name: "creationTime", editable: false, hidden: true },
      { name: "value", editable: true, hidden: false },
      { name: "isDomain", editable: false, hidden: true },
      { name: "isSecure", editable: true, hidden: true },
      { name: "isHttpOnly", editable: true, hidden: false }
    ];
  }),

  /**
   * Pass the editItem command from the content to the chrome process.
   *
   * @param {Object} data
   *        See editCookie() for format details.
   */
  editItem: Task.async(function* (data) {
    let doc = this.storageActor.document;
    data.originAttributes = doc.nodePrincipal
                               .originAttributes;
    this.editCookie(data);
  }),

  removeItem: Task.async(function* (host, name) {
    let doc = this.storageActor.document;
    this.removeCookie(host, name, doc.nodePrincipal
                                     .originAttributes);
  }),

  removeAll: Task.async(function* (host, domain) {
    let doc = this.storageActor.document;
    this.removeAllCookies(host, domain, doc.nodePrincipal
                                           .originAttributes);
  }),

  maybeSetupChildProcess() {
    cookieHelpers.onCookieChanged = this.onCookieChanged.bind(this);

    if (!DebuggerServer.isInChildProcess) {
      this.getCookiesFromHost =
        cookieHelpers.getCookiesFromHost.bind(cookieHelpers);
      this.addCookieObservers =
        cookieHelpers.addCookieObservers.bind(cookieHelpers);
      this.removeCookieObservers =
        cookieHelpers.removeCookieObservers.bind(cookieHelpers);
      this.editCookie =
        cookieHelpers.editCookie.bind(cookieHelpers);
      this.removeCookie =
        cookieHelpers.removeCookie.bind(cookieHelpers);
      this.removeAllCookies =
        cookieHelpers.removeAllCookies.bind(cookieHelpers);
      return;
    }

    const { sendSyncMessage, addMessageListener } =
      this.conn.parentMessageManager;

    this.conn.setupInParent({
      module: "devtools/server/actors/storage",
      setupParent: "setupParentProcessForCookies"
    });

    this.getCookiesFromHost =
      callParentProcess.bind(null, "getCookiesFromHost");
    this.addCookieObservers =
      callParentProcess.bind(null, "addCookieObservers");
    this.removeCookieObservers =
      callParentProcess.bind(null, "removeCookieObservers");
    this.editCookie =
      callParentProcess.bind(null, "editCookie");
    this.removeCookie =
      callParentProcess.bind(null, "removeCookie");
    this.removeAllCookies =
      callParentProcess.bind(null, "removeAllCookies");

    addMessageListener("debug:storage-cookie-request-child",
                       cookieHelpers.handleParentRequest);

    function callParentProcess(methodName, ...args) {
      let reply = sendSyncMessage("debug:storage-cookie-request-parent", {
        method: methodName,
        args: args
      });

      if (reply.length === 0) {
        console.error("ERR_DIRECTOR_CHILD_NO_REPLY from " + methodName);
      } else if (reply.length > 1) {
        console.error("ERR_DIRECTOR_CHILD_MULTIPLE_REPLIES from " + methodName);
      }

      let result = reply[0];

      if (methodName === "getCookiesFromHost") {
        return JSON.parse(result);
      }

      return result;
    }
  },
});

var cookieHelpers = {
  getCookiesFromHost(host, originAttributes) {
    // Local files have no host.
    if (host.startsWith("file:///")) {
      host = "";
    }

    let cookies = Services.cookies.getCookiesFromHost(host, originAttributes);
    let store = [];

    while (cookies.hasMoreElements()) {
      let cookie = cookies.getNext().QueryInterface(Ci.nsICookie2);

      store.push(cookie);
    }

    return store;
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
  editCookie(data) {
    let {field, oldValue, newValue} = data;
    let origName = field === "name" ? oldValue : data.items.name;
    let origHost = field === "host" ? oldValue : data.items.host;
    let origPath = field === "path" ? oldValue : data.items.path;
    let cookie = null;

    let enumerator =
      Services.cookies.getCookiesFromHost(origHost, data.originAttributes || {});

    while (enumerator.hasMoreElements()) {
      let nsiCookie = enumerator.getNext().QueryInterface(Ci.nsICookie2);
      if (nsiCookie.name === origName &&
          nsiCookie.host === origHost &&
          nsiCookie.path === origPath) {
        cookie = {
          host: nsiCookie.host,
          path: nsiCookie.path,
          name: nsiCookie.name,
          value: nsiCookie.value,
          isSecure: nsiCookie.isSecure,
          isHttpOnly: nsiCookie.isHttpOnly,
          isSession: nsiCookie.isSession,
          expires: nsiCookie.expires,
          originAttributes: nsiCookie.originAttributes
        };
        break;
      }
    }

    if (!cookie) {
      return;
    }

    // If the date is expired set it for 10 seconds in the future.
    let now = new Date();
    if (!cookie.isSession && (cookie.expires * 1000) <= now) {
      let tenSecondsFromNow = (now.getTime() + 10 * 1000) / 1000;

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
        Services.cookies.remove(origHost, origName, origPath,
                                false, cookie.originAttributes);
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
      cookie.originAttributes
    );
  },

  _removeCookies(host, opts = {}) {
    // We use a uniqueId to emulate compound keys for cookies. We need to
    // extract the cookie name to remove the correct cookie.
    if (opts.name) {
      let split = opts.name.split(SEPARATOR_GUID);

      opts.name = split[0];
      opts.path = split[2];
    }

    function hostMatches(cookieHost, matchHost) {
      if (cookieHost == null) {
        return matchHost == null;
      }
      if (cookieHost.startsWith(".")) {
        return ("." + matchHost).endsWith(cookieHost);
      }
      return cookieHost == host;
    }

    let enumerator =
      Services.cookies.getCookiesFromHost(host, opts.originAttributes || {});

    while (enumerator.hasMoreElements()) {
      let cookie = enumerator.getNext().QueryInterface(Ci.nsICookie2);
      if (hostMatches(cookie.host, host) &&
          (!opts.name || cookie.name === opts.name) &&
          (!opts.domain || cookie.host === opts.domain) &&
          (!opts.path || cookie.path === opts.path)) {
        Services.cookies.remove(
          cookie.host,
          cookie.name,
          cookie.path,
          false,
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
    Services.obs.addObserver(cookieHelpers, "cookie-changed", false);
    return null;
  },

  removeCookieObservers() {
    Services.obs.removeObserver(cookieHelpers, "cookie-changed", false);
    return null;
  },

  observe(subject, topic, data) {
    if (!subject) {
      return;
    }

    switch (topic) {
      case "cookie-changed":
        if (data === "batch-deleted") {
          let cookiesNoInterface = subject.QueryInterface(Ci.nsIArray);
          let cookies = [];

          for (let i = 0; i < cookiesNoInterface.length; i++) {
            let cookie = cookiesNoInterface.queryElementAt(i, Ci.nsICookie2);
            cookies.push(cookie);
          }
          cookieHelpers.onCookieChanged(cookies, topic, data);

          return;
        }

        let cookie = subject.QueryInterface(Ci.nsICookie2);
        cookieHelpers.onCookieChanged(cookie, topic, data);
        break;
    }
  },

  handleParentRequest(msg) {
    switch (msg.json.method) {
      case "onCookieChanged":
        let [cookie, topic, data] = msg.data.args;
        cookie = JSON.parse(cookie);
        cookieHelpers.onCookieChanged(cookie, topic, data);
        break;
    }
  },

  handleChildRequest(msg) {
    switch (msg.json.method) {
      case "getCookiesFromHost": {
        let host = msg.data.args[0];
        let originAttributes = msg.data.args[1];
        let cookies = cookieHelpers.getCookiesFromHost(host, originAttributes);
        return JSON.stringify(cookies);
      }
      case "addCookieObservers": {
        return cookieHelpers.addCookieObservers();
      }
      case "removeCookieObservers": {
        return cookieHelpers.removeCookieObservers();
      }
      case "editCookie": {
        let rowdata = msg.data.args[0];
        return cookieHelpers.editCookie(rowdata);
      }
      case "removeCookie": {
        let host = msg.data.args[0];
        let name = msg.data.args[1];
        let originAttributes = msg.data.args[2];
        return cookieHelpers.removeCookie(host, name, originAttributes);
      }
      case "removeAllCookies": {
        let host = msg.data.args[0];
        let domain = msg.data.args[1];
        let originAttributes = msg.data.args[2];
        return cookieHelpers.removeAllCookies(host, domain, originAttributes);
      }
      default:
        console.error("ERR_DIRECTOR_PARENT_UNKNOWN_METHOD", msg.json.method);
        throw new Error("ERR_DIRECTOR_PARENT_UNKNOWN_METHOD");
    }
  },
};

/**
 * E10S parent/child setup helpers
 */

exports.setupParentProcessForCookies = function ({ mm, prefix }) {
  cookieHelpers.onCookieChanged =
    callChildProcess.bind(null, "onCookieChanged");

  // listen for director-script requests from the child process
  setMessageManager(mm);

  function callChildProcess(methodName, ...args) {
    if (methodName === "onCookieChanged") {
      args[0] = JSON.stringify(args[0]);
    }

    try {
      mm.sendAsyncMessage("debug:storage-cookie-request-child", {
        method: methodName,
        args: args
      });
    } catch (e) {
      // We may receive a NS_ERROR_NOT_INITIALIZED if the target window has
      // been closed. This can legitimately happen in between test runs.
    }
  }

  function setMessageManager(newMM) {
    if (mm) {
      mm.removeMessageListener("debug:storage-cookie-request-parent",
                               cookieHelpers.handleChildRequest);
    }
    mm = newMM;
    if (mm) {
      mm.addMessageListener("debug:storage-cookie-request-parent",
                            cookieHelpers.handleChildRequest);
    }
  }

  return {
    onBrowserSwap: setMessageManager,
    onDisconnected: () => {
      // Although "disconnected-from-child" implies that the child is already
      // disconnected this is not the case. The disconnection takes place after
      // this method has finished. This gives us chance to clean up items within
      // the parent process e.g. observers.
      cookieHelpers.removeCookieObservers();
      setMessageManager(null);
    }
  };
};

/**
 * Helper method to create the overriden object required in
 * StorageActors.createActor for Local Storage and Session Storage.
 * This method exists as both Local Storage and Session Storage have almost
 * identical actors.
 */
function getObjectForLocalOrSessionStorage(type) {
  return {
    getNamesForHost(host) {
      let storage = this.hostVsStores.get(host);
      return storage ? Object.keys(storage) : [];
    },

    getValuesForHost(host, name) {
      let storage = this.hostVsStores.get(host);
      if (!storage) {
        return [];
      }
      if (name) {
        let value = storage ? storage.getItem(name) : null;
        return [{ name, value }];
      }
      if (!storage) {
        return [];
      }
      return Object.keys(storage).map(key => ({
        name: key,
        value: storage.getItem(key)
      }));
    },

    getHostName(location) {
      if (!location.host) {
        return location.href;
      }
      if (location.protocol === "chrome:") {
        return location.href;
      }
      return location.protocol + "//" + location.host;
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
      for (let window of this.windows) {
        this.populateStoresForHost(this.getHostName(window.location), window);
      }
    },

    getFields: Task.async(function* () {
      return [
        { name: "name", editable: true },
        { name: "value", editable: true }
      ];
    }),

    /**
     * Edit localStorage or sessionStorage fields.
     *
     * @param {Object} data
     *        See editCookie() for format details.
     */
    editItem: Task.async(function* ({host, field, oldValue, items}) {
      let storage = this.hostVsStores.get(host);
      if (!storage) {
        return;
      }

      if (field === "name") {
        storage.removeItem(oldValue);
      }

      storage.setItem(items.name, items.value);
    }),

    removeItem: Task.async(function* (host, name) {
      let storage = this.hostVsStores.get(host);
      if (!storage) {
        return;
      }
      storage.removeItem(name);
    }),

    removeAll: Task.async(function* (host) {
      let storage = this.hostVsStores.get(host);
      if (!storage) {
        return;
      }
      storage.clear();
    }),

    observe(subject, topic, data) {
      if ((topic != "dom-storage2-changed" &&
           topic != "dom-private-storage2-changed") ||
          data != type) {
        return null;
      }

      let host = this.getSchemaAndHost(subject.url);

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
      let updateData = {};
      updateData[host] = [subject.key];
      return this.storageActor.update(action, type, updateData);
    },

    /**
     * Given a url, correctly determine its protocol + hostname part.
     */
    getSchemaAndHost(url) {
      let uri = Services.io.newURI(url, null, null);
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
        value: new LongStringActor(this.conn, item.value || "")
      };
    },
  };
}

/**
 * The Local Storage actor and front.
 */
StorageActors.createActor({
  typeName: "localStorage",
  observationTopics: ["dom-storage2-changed", "dom-private-storage2-changed"]
}, getObjectForLocalOrSessionStorage("localStorage"));

/**
 * The Session Storage actor and front.
 */
StorageActors.createActor({
  typeName: "sessionStorage",
  observationTopics: ["dom-storage2-changed", "dom-private-storage2-changed"]
}, getObjectForLocalOrSessionStorage("sessionStorage"));

StorageActors.createActor({
  typeName: "Cache"
}, {
  getCachesForHost: Task.async(function* (host) {
    let uri = Services.io.newURI(host, null, null);
    let principal =
      Services.scriptSecurityManager.getNoAppCodebasePrincipal(uri);

    // The first argument tells if you want to get |content| cache or |chrome|
    // cache.
    // The |content| cache is the cache explicitely named by the web content
    // (service worker or web page).
    // The |chrome| cache is the cache implicitely cached by the platform,
    // hosting the source file of the service worker.
    let { CacheStorage } = this.storageActor.window;
    let cache = new CacheStorage("content", principal);
    return cache;
  }),

  preListStores: Task.async(function* () {
    for (let host of this.hosts) {
      yield this.populateStoresForHost(host);
    }
  }),

  form(form, detail) {
    if (detail === "actorid") {
      return this.actorID;
    }

    let hosts = {};
    for (let host of this.hosts) {
      hosts[host] = this.getNamesForHost(host);
    }

    return {
      actor: this.actorID,
      hosts: hosts
    };
  },

  getNamesForHost(host) {
    // UI code expect each name to be a JSON string of an array :/
    return [...this.hostVsStores.get(host).keys()].map(a => {
      return JSON.stringify([a]);
    });
  },

  getValuesForHost: Task.async(function* (host, name) {
    if (!name) {
      return [];
    }
    // UI is weird and expect a JSON stringified array... and pass it back :/
    name = JSON.parse(name)[0];

    let cache = this.hostVsStores.get(host).get(name);
    let requests = yield cache.keys();
    let results = [];
    for (let request of requests) {
      let response = yield cache.match(request);
      // Unwrap the response to get access to all its properties if the
      // response happen to be 'opaque', when it is a Cross Origin Request.
      response = response.cloneUnfiltered();
      results.push(yield this.processEntry(request, response));
    }
    return results;
  }),

  processEntry: Task.async(function* (request, response) {
    return {
      url: String(request.url),
      status: String(response.statusText),
    };
  }),

  getFields: Task.async(function* () {
    return [
      { name: "url", editable: false },
      { name: "status", editable: false }
    ];
  }),

  getHostName(location) {
    if (!location.host) {
      return location.href;
    }
    if (location.protocol === "chrome:") {
      return location.href;
    }
    return location.protocol + "//" + location.host;
  },

  populateStoresForHost: Task.async(function* (host) {
    let storeMap = new Map();
    let caches = yield this.getCachesForHost(host);
    try {
      for (let name of (yield caches.keys())) {
        storeMap.set(name, (yield caches.open(name)));
      }
    } catch (ex) {
      console.warn(`Failed to enumerate CacheStorage for host ${host}: ${ex}`);
    }
    this.hostVsStores.set(host, storeMap);
  }),

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
    let uri = Services.io.newURI(url, null, null);
    return uri.scheme + "://" + uri.hostPort;
  },

  toStoreObject(item) {
    return item;
  },

  removeItem: Task.async(function* (host, name) {
    const cacheMap = this.hostVsStores.get(host);
    if (!cacheMap) {
      return;
    }

    const parsedName = JSON.parse(name);

    if (parsedName.length == 1) {
      // Delete the whole Cache object
      const [ cacheName ] = parsedName;
      cacheMap.delete(cacheName);
      const cacheStorage = yield this.getCachesForHost(host);
      yield cacheStorage.delete(cacheName);
      this.onItemUpdated("deleted", host, [ cacheName ]);
    } else if (parsedName.length == 2) {
      // Delete one cached request
      const [ cacheName, url ] = parsedName;
      const cache = cacheMap.get(cacheName);
      if (cache) {
        yield cache.delete(url);
        this.onItemUpdated("deleted", host, [ cacheName, url ]);
      }
    }
  }),

  removeAll: Task.async(function* (host, name) {
    const cacheMap = this.hostVsStores.get(host);
    if (!cacheMap) {
      return;
    }

    const parsedName = JSON.parse(name);

    // Only a Cache object is a valid object to clear
    if (parsedName.length == 1) {
      const [ cacheName ] = parsedName;
      const cache = cacheMap.get(cacheName);
      if (cache) {
        let keys = yield cache.keys();
        yield promise.all(keys.map(key => cache.delete(key)));
        this.onItemUpdated("cleared", host, [ cacheName ]);
      }
    }
  }),

  /**
   * CacheStorage API doesn't support any notifications, we must fake them
   */
  onItemUpdated(action, host, path) {
    this.storageActor.update(action, "Cache", {
      [host]: [ JSON.stringify(path) ]
    });
  },
});

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
      multiEntry: this._multiEntry
    };
  }
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
    let index = objectStore.index(objectStore.indexNames[i]);

    let newIndex = {
      keypath: index.keyPath,
      multiEntry: index.multiEntry,
      name: index.name,
      objectStore: {
        autoIncrement: index.objectStore.autoIncrement,
        indexNames: [...index.objectStore.indexNames],
        keyPath: index.objectStore.keyPath,
        name: index.objectStore.name,
      }
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
      )
    };
  }
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
    let transaction = db.transaction(db.objectStoreNames, "readonly");

    for (let i = 0; i < transaction.objectStoreNames.length; i++) {
      let objectStore =
        transaction.objectStore(transaction.objectStoreNames[i]);
      this._objectStores.push([transaction.objectStoreNames[i],
                              new ObjectStoreMetadata(objectStore)]);
    }
  }
}
DatabaseMetadata.prototype = {
  get objectStores() {
    return this._objectStores;
  },

  toObject() {
    return {
      name: `${this._name} (${this.storage})`,
      origin: this._origin,
      version: this._version,
      objectStores: this._objectStores.size
    };
  }
};

StorageActors.createActor({
  typeName: "indexedDB"
}, {
  initialize(storageActor) {
    protocol.Actor.prototype.initialize.call(this, null);

    this.storageActor = storageActor;

    this.maybeSetupChildProcess();

    this.objectsSize = {};
    this.storageActor = storageActor;
    this.onWindowReady = this.onWindowReady.bind(this);
    this.onWindowDestroyed = this.onWindowDestroyed.bind(this);

    events.on(this.storageActor, "window-ready", this.onWindowReady);
    events.on(this.storageActor, "window-destroyed", this.onWindowDestroyed);
  },

  destroy() {
    this.hostVsStores.clear();
    this.objectsSize = null;

    events.off(this.storageActor, "window-ready", this.onWindowReady);
    events.off(this.storageActor, "window-destroyed", this.onWindowDestroyed);

    protocol.Actor.prototype.destroy.call(this);

    this.storageActor = null;
  },

  /**
   * Remove an indexedDB database from given host with a given name.
   */
  removeDatabase: Task.async(function* (host, name) {
    let win = this.storageActor.getWindowFromHost(host);
    if (!win) {
      return { error: `Window for host ${host} not found` };
    }

    let principal = win.document.nodePrincipal;
    return this.removeDB(host, principal, name);
  }),

  removeAll: Task.async(function* (host, name) {
    let [db, store] = JSON.parse(name);

    let win = this.storageActor.getWindowFromHost(host);
    if (!win) {
      return;
    }

    let principal = win.document.nodePrincipal;
    this.clearDBStore(host, principal, db, store);
  }),

  removeItem: Task.async(function* (host, name) {
    let [db, store, id] = JSON.parse(name);

    let win = this.storageActor.getWindowFromHost(host);
    if (!win) {
      return;
    }

    let principal = win.document.nodePrincipal;
    this.removeDBRecord(host, principal, db, store, id);
  }),

  getHostName(location) {
    if (!location.host) {
      return location.href;
    }
    if (location.protocol === "chrome:") {
      return location.href;
    }
    return location.protocol + "//" + location.host;
  },

  /**
   * This method is overriden and left blank as for indexedDB, this operation
   * cannot be performed synchronously. Thus, the preListStores method exists to
   * do the same task asynchronously.
   */
  populateStoresForHosts() {},

  getNamesForHost(host) {
    let names = [];

    for (let [dbName, {objectStores}] of this.hostVsStores.get(host)) {
      if (objectStores.size) {
        for (let objectStore of objectStores.keys()) {
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
    let name = names[0];
    let parsedName = JSON.parse(name);

    if (parsedName.length == 3) {
      // This is the case where specific entries from an object store were
      // requested
      return names.length;
    } else if (parsedName.length == 2) {
      // This is the case where all entries from an object store are requested.
      let index = options.index;
      let [db, objectStore] = parsedName;
      if (this.objectsSize[host + db + objectStore + index]) {
        return this.objectsSize[host + db + objectStore + index];
      }
    } else if (parsedName.length == 1) {
      // This is the case where details of all object stores in a db are
      // requested.
      if (this.hostVsStores.has(host) &&
          this.hostVsStores.get(host).has(parsedName[0])) {
        return this.hostVsStores.get(host).get(parsedName[0]).objectStores.size;
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
  preListStores: Task.async(function* () {
    this.hostVsStores = new Map();

    for (let host of this.hosts) {
      yield this.populateStoresForHost(host);
    }
  }),

  populateStoresForHost: Task.async(function* (host) {
    let storeMap = new Map();
    let {names} = yield this.getDBNamesForHost(host);

    let win = this.storageActor.getWindowFromHost(host);
    if (win) {
      let principal = win.document.nodePrincipal;

      for (let {name, storage} of names) {
        let metadata = yield this.getDBMetaData(host, principal, name, storage);

        metadata = indexedDBHelpers.patchMetadataMapsAndProtos(metadata);

        storeMap.set(`${name} (${storage})`, metadata);
      }
    }

    this.hostVsStores.set(host, storeMap);
  }),

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
        indexes: item.indexes
      };
    }
    if ("objectStores" in item) {
      // DB meta data
      return {
        db: item.name,
        origin: item.origin,
        version: item.version,
        objectStores: item.objectStores
      };
    }

    let value = JSON.stringify(item.value);

    // FIXME: Bug 1318029 - Due to a bug that is thrown whenever a
    // LongStringActor string reaches DebuggerServer.LONG_STRING_LENGTH we need
    // to trim the value. When the bug is fixed we should stop trimming the
    // string here.
    let maxLength = DebuggerServer.LONG_STRING_LENGTH - 1;
    if (value.length > maxLength) {
      value = value.substr(0, maxLength);
    }

    // Indexed db entry
    return {
      name: item.name,
      value: new LongStringActor(this.conn, value)
    };
  },

  form(form, detail) {
    if (detail === "actorid") {
      return this.actorID;
    }

    let hosts = {};
    for (let host of this.hosts) {
      hosts[host] = this.getNamesForHost(host);
    }

    return {
      actor: this.actorID,
      hosts: hosts
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
      [host]: [ JSON.stringify(path) ]
    });
  },

  maybeSetupChildProcess() {
    if (!DebuggerServer.isInChildProcess) {
      this.backToChild = (func, rv) => rv;
      this.clearDBStore = indexedDBHelpers.clearDBStore;
      this.gatherFilesOrFolders = indexedDBHelpers.gatherFilesOrFolders;
      this.getDBMetaData = indexedDBHelpers.getDBMetaData;
      this.getDBNamesForHost = indexedDBHelpers.getDBNamesForHost;
      this.getNameFromDatabaseFile = indexedDBHelpers.getNameFromDatabaseFile;
      this.getObjectStoreData = indexedDBHelpers.getObjectStoreData;
      this.getSanitizedHost = indexedDBHelpers.getSanitizedHost;
      this.getValuesForHost = indexedDBHelpers.getValuesForHost;
      this.openWithPrincipal = indexedDBHelpers.openWithPrincipal;
      this.removeDB = indexedDBHelpers.removeDB;
      this.removeDBRecord = indexedDBHelpers.removeDBRecord;
      this.splitNameAndStorage = indexedDBHelpers.splitNameAndStorage;
      return;
    }

    const { sendAsyncMessage, addMessageListener } =
      this.conn.parentMessageManager;

    this.conn.setupInParent({
      module: "devtools/server/actors/storage",
      setupParent: "setupParentProcessForIndexedDB"
    });

    this.getDBMetaData = callParentProcessAsync.bind(null, "getDBMetaData");
    this.splitNameAndStorage = callParentProcessAsync.bind(null, "splitNameAndStorage");
    this.getDBNamesForHost = callParentProcessAsync.bind(null, "getDBNamesForHost");
    this.getValuesForHost = callParentProcessAsync.bind(null, "getValuesForHost");
    this.removeDB = callParentProcessAsync.bind(null, "removeDB");
    this.removeDBRecord = callParentProcessAsync.bind(null, "removeDBRecord");
    this.clearDBStore = callParentProcessAsync.bind(null, "clearDBStore");

    addMessageListener("debug:storage-indexedDB-request-child", msg => {
      switch (msg.json.method) {
        case "backToChild": {
          let [func, rv] = msg.json.args;
          let deferred = unresolvedPromises.get(func);
          if (deferred) {
            unresolvedPromises.delete(func);
            deferred.resolve(rv);
          }
          break;
        }
        case "onItemUpdated": {
          let [action, host, path] = msg.json.args;
          this.onItemUpdated(action, host, path);
        }
      }
    });

    let unresolvedPromises = new Map();
    function callParentProcessAsync(methodName, ...args) {
      let deferred = promise.defer();

      unresolvedPromises.set(methodName, deferred);

      sendAsyncMessage("debug:storage-indexedDB-request-parent", {
        method: methodName,
        args: args
      });

      return deferred.promise;
    }
  },

  getFields: Task.async(function* (subType) {
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
          { name: "value", editable: false }
        ];

      // Detail of indexedDB for one origin
      default:
        return [
          { name: "db", editable: false },
          { name: "origin", editable: false },
          { name: "version", editable: false },
          { name: "objectStores", editable: false },
        ];
    }
  })
});

var indexedDBHelpers = {
  backToChild(...args) {
    let mm = Cc["@mozilla.org/globalmessagemanager;1"]
               .getService(Ci.nsIMessageListenerManager);

    mm.broadcastAsyncMessage("debug:storage-indexedDB-request-child", {
      method: "backToChild",
      args: args
    });
  },

  onItemUpdated(action, host, path) {
    let mm = Cc["@mozilla.org/globalmessagemanager;1"]
               .getService(Ci.nsIMessageListenerManager);

    mm.broadcastAsyncMessage("debug:storage-indexedDB-request-child", {
      method: "onItemUpdated",
      args: [ action, host, path ]
    });
  },

  /**
   * Fetches and stores all the metadata information for the given database
   * `name` for the given `host` with its `principal`. The stored metadata
   * information is of `DatabaseMetadata` type.
   */
  getDBMetaData: Task.async(function* (host, principal, name, storage) {
    let request = this.openWithPrincipal(principal, name, storage);
    let success = promise.defer();

    request.onsuccess = event => {
      let db = event.target.result;
      let dbData = new DatabaseMetadata(host, db, storage);
      db.close();

      success.resolve(this.backToChild("getDBMetaData", dbData));
    };
    request.onerror = ({target}) => {
      console.error(
        `Error opening indexeddb database ${name} for host ${host}`, target.error);
      success.resolve(this.backToChild("getDBMetaData", null));
    };
    return success.promise;
  }),

  splitNameAndStorage: function (name) {
    let lastOpenBracketIndex = name.lastIndexOf("(");
    let lastCloseBracketIndex = name.lastIndexOf(")");
    let delta = lastCloseBracketIndex - lastOpenBracketIndex - 1;

    let storage = name.substr(lastOpenBracketIndex + 1, delta);

    name = name.substr(0, lastOpenBracketIndex - 1);

    return { storage, name };
  },

  /**
   * Opens an indexed db connection for the given `principal` and
   * database `name`.
   */
  openWithPrincipal: function (principal, name, storage) {
    return indexedDBForStorage.openForPrincipal(principal, name,
                                                { storage: storage });
  },

  removeDB: Task.async(function* (host, principal, dbName) {
    let result = new promise(resolve => {
      let {name, storage} = this.splitNameAndStorage(dbName);
      let request =
        indexedDBForStorage.deleteForPrincipal(principal, name,
                                               { storage: storage });

      request.onsuccess = () => {
        resolve({});
        this.onItemUpdated("deleted", host, [dbName]);
      };

      request.onblocked = () => {
        console.warn(`Deleting indexedDB database ${name} for host ${host} is blocked`);
        resolve({ blocked: true });
      };

      request.onerror = () => {
        let { error } = request;
        console.warn(
          `Error deleting indexedDB database ${name} for host ${host}: ${error}`);
        resolve({ error: error.message });
      };

      // If the database is blocked repeatedly, the onblocked event will not
      // be fired again. To avoid waiting forever, report as blocked if nothing
      // else happens after 3 seconds.
      setTimeout(() => resolve({ blocked: true }), 3000);
    });

    return this.backToChild("removeDB", yield result);
  }),

  removeDBRecord: Task.async(function* (host, principal, dbName, storeName, id) {
    let db;
    let {name, storage} = this.splitNameAndStorage(dbName);

    try {
      db = yield new promise((resolve, reject) => {
        let request = this.openWithPrincipal(principal, name, storage);
        request.onsuccess = ev => resolve(ev.target.result);
        request.onerror = ev => reject(ev.target.error);
      });

      let transaction = db.transaction(storeName, "readwrite");
      let store = transaction.objectStore(storeName);

      yield new promise((resolve, reject) => {
        let request = store.delete(id);
        request.onsuccess = () => resolve();
        request.onerror = ev => reject(ev.target.error);
      });

      this.onItemUpdated("deleted", host, [dbName, storeName, id]);
    } catch (error) {
      let recordPath = [dbName, storeName, id].join("/");
      console.error(`Failed to delete indexedDB record: ${recordPath}: ${error}`);
    }

    if (db) {
      db.close();
    }

    return this.backToChild("removeDBRecord", null);
  }),

  clearDBStore: Task.async(function* (host, principal, dbName, storeName) {
    let db;
    let {name, storage} = this.splitNameAndStorage(dbName);

    try {
      db = yield new promise((resolve, reject) => {
        let request = this.openWithPrincipal(principal, name, storage);
        request.onsuccess = ev => resolve(ev.target.result);
        request.onerror = ev => reject(ev.target.error);
      });

      let transaction = db.transaction(storeName, "readwrite");
      let store = transaction.objectStore(storeName);

      yield new promise((resolve, reject) => {
        let request = store.clear();
        request.onsuccess = () => resolve();
        request.onerror = ev => reject(ev.target.error);
      });

      this.onItemUpdated("cleared", host, [dbName, storeName]);
    } catch (error) {
      let storePath = [dbName, storeName].join("/");
      console.error(`Failed to clear indexedDB store: ${storePath}: ${error}`);
    }

    if (db) {
      db.close();
    }

    return this.backToChild("clearDBStore", null);
  }),

  /**
   * Fetches all the databases and their metadata for the given `host`.
   */
  getDBNamesForHost: Task.async(function* (host) {
    let sanitizedHost = this.getSanitizedHost(host);
    let profileDir = OS.Constants.Path.profileDir;
    let files = [];
    let names = [];
    let storagePath = OS.Path.join(profileDir, "storage");

    // We expect sqlite DB paths to look something like this:
    // - PathToProfileDir/storage/default/http+++www.example.com/
    //   idb/1556056096MeysDaabta.sqlite
    // - PathToProfileDir/storage/permanent/http+++www.example.com/
    //   idb/1556056096MeysDaabta.sqlite
    // - PathToProfileDir/storage/temporary/http+++www.example.com/
    //   idb/1556056096MeysDaabta.sqlite
    //
    // The subdirectory inside the storage folder is determined by the storage
    // type:
    // - default:   { storage: "default" } or not specified.
    // - permanent: { storage: "persistent" }.
    // - temporary: { storage: "temporary" }.
    let sqliteFiles = yield this.gatherFilesOrFolders(storagePath, path => {
      if (path.endsWith(".sqlite")) {
        let { components } = OS.Path.split(path);
        let isIDB = components[components.length - 2] === "idb";

        return isIDB;
      }
      return false;
    });

    for (let file of sqliteFiles) {
      let splitPath = OS.Path.split(file).components;
      let idbIndex = splitPath.indexOf("idb");
      let name = splitPath[idbIndex - 1];
      let storage = splitPath[idbIndex - 2];
      let relative = file.substr(profileDir.length + 1);

      if (name.startsWith(sanitizedHost)) {
        files.push({
          file: relative,
          storage: storage === "permanent" ? "persistent" : storage
        });
      }
    }

    if (files.length > 0) {
      for (let {file, storage} of files) {
        let name = yield this.getNameFromDatabaseFile(file);
        if (name) {
          names.push({
            name,
            storage
          });
        }
      }
    }

    return this.backToChild("getDBNamesForHost", {names});
  }),

  /**
   * Gather together all of the files in path and pass each path through a
   * validation function.
   *
   * @param {String}
   *        Path in which to begin searching.
   * @param {Function}
   *        Validation function, which checks each file path. If this function
   *        Returns true the file path is kept.
   *
   * @returns {Array}
   *          An array of file paths.
   */
  gatherFilesOrFolders: Task.async(function* (path, validationFunc) {
    let files = [];
    let iterator;
    let paths = [path];

    while (paths.length > 0) {
      try {
        iterator = new OS.File.DirectoryIterator(paths.pop());

        for (let child in iterator) {
          child = yield child;

          path = child.path;

          if (child.isDir) {
            paths.push(path);
          } else if (validationFunc(path)) {
            files.push(path);
          }
        }
      } catch (ex) {
        // Ignore StopIteration to prevent exiting the loop.
        if (ex != StopIteration) {
          throw ex;
        }
      }
    }
    iterator.close();

    return files;
  }),

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
  getNameFromDatabaseFile: Task.async(function* (path) {
    let connection = null;
    let retryCount = 0;

    // Content pages might be having an open transaction for the same indexed db
    // which this sqlite file belongs to. In that case, sqlite.openConnection
    // will throw. Thus we retry for some time to see if lock is removed.
    while (!connection && retryCount++ < 25) {
      try {
        connection = yield Sqlite.openConnection({ path: path });
      } catch (ex) {
        // Continuously retrying is overkill. Waiting for 100ms before next try
        yield sleep(100);
      }
    }

    if (!connection) {
      return null;
    }

    let rows = yield connection.execute("SELECT name FROM database");
    if (rows.length != 1) {
      return null;
    }

    let name = rows[0].getResultByName("name");

    yield connection.close();

    return name;
  }),

  getValuesForHost: Task.async(function* (host, name = "null", options,
                                          hostVsStores, principal) {
    name = JSON.parse(name);
    if (!name || !name.length) {
      // This means that details about the db in this particular host are
      // requested.
      let dbs = [];
      if (hostVsStores.has(host)) {
        for (let [, db] of hostVsStores.get(host)) {
          db = indexedDBHelpers.patchMetadataMapsAndProtos(db);
          dbs.push(db.toObject());
        }
      }
      return this.backToChild("getValuesForHost", {dbs: dbs});
    }

    let [db2, objectStore, id] = name;
    if (!objectStore) {
      // This means that details about all the object stores in this db are
      // requested.
      let objectStores = [];
      if (hostVsStores.has(host) && hostVsStores.get(host).has(db2)) {
        let db = hostVsStores.get(host).get(db2);

        db = indexedDBHelpers.patchMetadataMapsAndProtos(db);

        let objectStores2 = db.objectStores;

        for (let objectStore2 of objectStores2) {
          objectStores.push(objectStore2[1].toObject());
        }
      }
      return this.backToChild("getValuesForHost", {objectStores: objectStores});
    }
    // Get either all entries from the object store, or a particular id
    let storage = hostVsStores.get(host).get(db2).storage;
    let result = yield this.getObjectStoreData(host, principal, db2, storage, {
      objectStore: objectStore,
      id: id,
      index: options.index,
      offset: 0,
      size: options.size
    });
    return this.backToChild("getValuesForHost", {result: result});
  }),

  /**
   * Returns all or requested entries from a particular objectStore from the db
   * in the given host.
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
    let {name} = this.splitNameAndStorage(dbName);
    let request = this.openWithPrincipal(principal, name, storage);
    let success = promise.defer();
    let {objectStore, id, index, offset, size} = requestOptions;
    let data = [];
    let db;

    if (!size || size > MAX_STORE_OBJECT_COUNT) {
      size = MAX_STORE_OBJECT_COUNT;
    }

    request.onsuccess = event => {
      db = event.target.result;

      let transaction = db.transaction(objectStore, "readonly");
      let source = transaction.objectStore(objectStore);
      if (index && index != "name") {
        source = source.index(index);
      }

      source.count().onsuccess = event2 => {
        let objectsSize = [];
        let count = event2.target.result;
        objectsSize.push({
          key: host + dbName + objectStore + index,
          count: count
        });

        if (!offset) {
          offset = 0;
        } else if (offset > count) {
          db.close();
          success.resolve([]);
          return;
        }

        if (id) {
          source.get(id).onsuccess = event3 => {
            db.close();
            success.resolve([{name: id, value: event3.target.result}]);
          };
        } else {
          source.openCursor().onsuccess = event4 => {
            let cursor = event4.target.result;

            if (!cursor || data.length >= size) {
              db.close();
              success.resolve({
                data: data,
                objectsSize: objectsSize
              });
              return;
            }
            if (offset-- <= 0) {
              data.push({name: cursor.key, value: cursor.value});
            }
            cursor.continue();
          };
        }
      };
    };
    request.onerror = () => {
      db.close();
      success.resolve([]);
    };
    return success.promise;
  },

  /**
   * When indexedDB metadata is parsed to and from JSON then the object's
   * prototype is dropped and any Maps are changed to arrays of arrays. This
   * method is used to repair the prototypes and fix any broken Maps.
   */
  patchMetadataMapsAndProtos(metadata) {
    let md = Object.create(DatabaseMetadata.prototype);
    Object.assign(md, metadata);

    md._objectStores = new Map(metadata._objectStores);

    for (let [name, store] of md._objectStores) {
      let obj = Object.create(ObjectStoreMetadata.prototype);
      Object.assign(obj, store);

      md._objectStores.set(name, obj);

      if (typeof store._indexes.length !== "undefined") {
        obj._indexes = new Map(store._indexes);
      }

      for (let [name2, value] of obj._indexes) {
        let obj2 = Object.create(IndexMetadata.prototype);
        Object.assign(obj2, value);

        obj._indexes.set(name2, obj2);
      }
    }

    return md;
  },

  handleChildRequest(msg) {
    let args = msg.data.args;

    switch (msg.json.method) {
      case "getDBMetaData": {
        let [host, principal, name, storage] = args;
        return indexedDBHelpers.getDBMetaData(host, principal, name, storage);
      }
      case "splitNameAndStorage": {
        let [name] = args;
        return indexedDBHelpers.splitNameAndStorage(name);
      }
      case "getDBNamesForHost": {
        let [host] = args;
        return indexedDBHelpers.getDBNamesForHost(host);
      }
      case "getValuesForHost": {
        let [host, name, options, hostVsStores, principal] = args;
        return indexedDBHelpers.getValuesForHost(host, name, options,
                                                 hostVsStores, principal);
      }
      case "removeDB": {
        let [host, principal, dbName] = args;
        return indexedDBHelpers.removeDB(host, principal, dbName);
      }
      case "removeDBRecord": {
        let [host, principal, db, store, id] = args;
        return indexedDBHelpers.removeDBRecord(host, principal, db, store, id);
      }
      case "clearDBStore": {
        let [host, principal, db, store] = args;
        return indexedDBHelpers.clearDBStore(host, principal, db, store);
      }
      default:
        console.error("ERR_DIRECTOR_PARENT_UNKNOWN_METHOD", msg.json.method);
        throw new Error("ERR_DIRECTOR_PARENT_UNKNOWN_METHOD");
    }
  }
};

/**
 * E10S parent/child setup helpers
 */

exports.setupParentProcessForIndexedDB = function ({ mm, prefix }) {
  // listen for director-script requests from the child process
  setMessageManager(mm);

  function setMessageManager(newMM) {
    if (mm) {
      mm.removeMessageListener("debug:storage-indexedDB-request-parent",
                               indexedDBHelpers.handleChildRequest);
    }
    mm = newMM;
    if (mm) {
      mm.addMessageListener("debug:storage-indexedDB-request-parent",
                            indexedDBHelpers.handleChildRequest);
    }
  }

  return {
    onBrowserSwap: setMessageManager,
    onDisconnected: () => setMessageManager(null),
  };
};

/**
 * The main Storage Actor.
 */
let StorageActor = protocol.ActorClassWithSpec(specs.storageSpec, {
  typeName: "storage",

  get window() {
    return this.parentActor.window;
  },

  get document() {
    return this.parentActor.window.document;
  },

  get windows() {
    return this.childWindowPool;
  },

  initialize(conn, tabActor) {
    protocol.Actor.prototype.initialize.call(this, conn);

    this.parentActor = tabActor;

    this.childActorPool = new Map();
    this.childWindowPool = new Set();

    // Fetch all the inner iframe windows in this tab.
    this.fetchChildWindows(this.parentActor.docShell);

    // Initialize the registered store types
    for (let [store, ActorConstructor] of storageTypePool) {
      this.childActorPool.set(store, new ActorConstructor(this));
    }

    // Notifications that help us keep track of newly added windows and windows
    // that got removed
    Services.obs.addObserver(this, "content-document-global-created", false);
    Services.obs.addObserver(this, "inner-window-destroyed", false);
    this.onPageChange = this.onPageChange.bind(this);

    let handler = tabActor.chromeEventHandler;
    handler.addEventListener("pageshow", this.onPageChange, true);
    handler.addEventListener("pagehide", this.onPageChange, true);

    this.destroyed = false;
    this.boundUpdate = {};
  },

  destroy() {
    clearTimeout(this.batchTimer);
    this.batchTimer = null;
    // Remove observers
    Services.obs.removeObserver(this, "content-document-global-created", false);
    Services.obs.removeObserver(this, "inner-window-destroyed", false);
    this.destroyed = true;
    if (this.parentActor.browser) {
      this.parentActor.browser.removeEventListener("pageshow", this.onPageChange, true);
      this.parentActor.browser.removeEventListener("pagehide", this.onPageChange, true);
    }
    // Destroy the registered store types
    for (let actor of this.childActorPool.values()) {
      actor.destroy();
    }
    this.childActorPool.clear();
    this.childWindowPool.clear();

    this.childActorPool = null;
    this.childWindowPool = null;
    this.parentActor = null;
    this.boundUpdate = null;
    this.registeredPool = null;
    this._pendingResponse = null;

    protocol.Actor.prototype.destroy.call(this);
  },

  /**
   * Given a docshell, recursively find out all the child windows from it.
   *
   * @param {nsIDocShell} item
   *        The docshell from which all inner windows need to be extracted.
   */
  fetchChildWindows(item) {
    let docShell = item.QueryInterface(Ci.nsIDocShell)
                       .QueryInterface(Ci.nsIDocShellTreeItem);
    if (!docShell.contentViewer) {
      return null;
    }
    let window = docShell.contentViewer.DOMDocument.defaultView;
    if (window.location.href == "about:blank") {
      // Skip out about:blank windows as Gecko creates them multiple times while
      // creating any global.
      return null;
    }
    this.childWindowPool.add(window);
    for (let i = 0; i < docShell.childCount; i++) {
      let child = docShell.getChildAt(i);
      this.fetchChildWindows(child);
    }
    return null;
  },

  isIncludedInTopLevelWindow(window) {
    return isWindowIncluded(this.window, window);
  },

  getWindowFromInnerWindowID(innerID) {
    innerID = innerID.QueryInterface(Ci.nsISupportsPRUint64).data;
    for (let win of this.childWindowPool.values()) {
      let id = win.QueryInterface(Ci.nsIInterfaceRequestor)
                   .getInterface(Ci.nsIDOMWindowUtils)
                   .currentInnerWindowID;
      if (id == innerID) {
        return win;
      }
    }
    return null;
  },

  getWindowFromHost(host) {
    for (let win of this.childWindowPool.values()) {
      let origin = win.document
                      .nodePrincipal
                      .originNoSuffix;
      let url = win.document.URL;
      if (origin === host || url === host) {
        return win;
      }
    }
    return null;
  },

  /**
   * Event handler for any docshell update. This lets us figure out whenever
   * any new window is added, or an existing window is removed.
   */
  observe(subject, topic) {
    if (subject.location &&
        (!subject.location.href || subject.location.href == "about:blank")) {
      return null;
    }

    if (topic == "content-document-global-created" &&
        this.isIncludedInTopLevelWindow(subject)) {
      this.childWindowPool.add(subject);
      events.emit(this, "window-ready", subject);
    } else if (topic == "inner-window-destroyed") {
      let window = this.getWindowFromInnerWindowID(subject);
      if (window) {
        this.childWindowPool.delete(window);
        events.emit(this, "window-destroyed", window);
      }
    }
    return null;
  },

  /**
   * Called on "pageshow" or "pagehide" event on the chromeEventHandler of
   * current tab.
   *
   * @param {event} The event object passed to the handler. We are using these
   *        three properties from the event:
   *         - target {document} The document corresponding to the event.
   *         - type {string} Name of the event - "pageshow" or "pagehide".
   *         - persisted {boolean} true if there was no
   *                     "content-document-global-created" notification along
   *                     this event.
   */
  onPageChange({target, type, persisted}) {
    if (this.destroyed) {
      return;
    }

    let window = target.defaultView;

    if (type == "pagehide" && this.childWindowPool.delete(window)) {
      events.emit(this, "window-destroyed", window);
    } else if (type == "pageshow" && persisted && window.location.href &&
               window.location.href != "about:blank" &&
               this.isIncludedInTopLevelWindow(window)) {
      this.childWindowPool.add(window);
      events.emit(this, "window-ready", window);
    }
  },

  /**
   * Lists the available hosts for all the registered storage types.
   *
   * @returns {object} An object containing with the following structure:
   *  - <storageType> : [{
   *      actor: <actorId>,
   *      host: <hostname>
   *    }]
   */
  listStores: Task.async(function* () {
    let toReturn = {};

    for (let [name, value] of this.childActorPool) {
      if (value.preListStores) {
        yield value.preListStores();
      }
      toReturn[name] = value;
    }

    return toReturn;
  }),

  /**
   * This method is called by the registered storage types so as to tell the
   * Storage Actor that there are some changes in the stores. Storage Actor then
   * notifies the client front about these changes at regular (BATCH_DELAY)
   * interval.
   *
   * @param {string} action
   *        The type of change. One of "added", "changed" or "deleted"
   * @param {string} storeType
   *        The storage actor in which this change has occurred.
   * @param {object} data
   *        The update object. This object is of the following format:
   *         - {
   *             <host1>: [<store_names1>, <store_name2>...],
   *             <host2>: [<store_names34>...],
   *           }
   *           Where host1, host2 are the host in which this change happened and
   *           [<store_namesX] is an array of the names of the changed store objects.
   *           Pass an empty array if the host itself was affected: either completely
   *           removed or cleared.
   */
  update(action, storeType, data) {
    if (action == "cleared") {
      events.emit(this, "stores-cleared", { [storeType]: data });
      return null;
    }

    if (this.batchTimer) {
      clearTimeout(this.batchTimer);
    }
    if (!this.boundUpdate[action]) {
      this.boundUpdate[action] = {};
    }
    if (!this.boundUpdate[action][storeType]) {
      this.boundUpdate[action][storeType] = {};
    }
    for (let host in data) {
      if (!this.boundUpdate[action][storeType][host]) {
        this.boundUpdate[action][storeType][host] = [];
      }
      for (let name of data[host]) {
        if (!this.boundUpdate[action][storeType][host].includes(name)) {
          this.boundUpdate[action][storeType][host].push(name);
        }
      }
    }
    if (action == "added") {
      // If the same store name was previously deleted or changed, but now is
      // added somehow, dont send the deleted or changed update.
      this.removeNamesFromUpdateList("deleted", storeType, data);
      this.removeNamesFromUpdateList("changed", storeType, data);
    } else if (action == "changed" && this.boundUpdate.added &&
             this.boundUpdate.added[storeType]) {
      // If something got added and changed at the same time, then remove those
      // items from changed instead.
      this.removeNamesFromUpdateList("changed", storeType,
                                     this.boundUpdate.added[storeType]);
    } else if (action == "deleted") {
      // If any item got delete, or a host got delete, no point in sending
      // added or changed update
      this.removeNamesFromUpdateList("added", storeType, data);
      this.removeNamesFromUpdateList("changed", storeType, data);

      for (let host in data) {
        if (data[host].length == 0 && this.boundUpdate.added &&
            this.boundUpdate.added[storeType] &&
            this.boundUpdate.added[storeType][host]) {
          delete this.boundUpdate.added[storeType][host];
        }
        if (data[host].length == 0 && this.boundUpdate.changed &&
            this.boundUpdate.changed[storeType] &&
            this.boundUpdate.changed[storeType][host]) {
          delete this.boundUpdate.changed[storeType][host];
        }
      }
    }

    this.batchTimer = setTimeout(() => {
      clearTimeout(this.batchTimer);
      events.emit(this, "stores-update", this.boundUpdate);
      this.boundUpdate = {};
    }, BATCH_DELAY);

    return null;
  },

  /**
   * This method removes data from the this.boundUpdate object in the same
   * manner like this.update() adds data to it.
   *
   * @param {string} action
   *        The type of change. One of "added", "changed" or "deleted"
   * @param {string} storeType
   *        The storage actor for which you want to remove the updates data.
   * @param {object} data
   *        The update object. This object is of the following format:
   *         - {
   *             <host1>: [<store_names1>, <store_name2>...],
   *             <host2>: [<store_names34>...],
   *           }
   *           Where host1, host2 are the hosts which you want to remove and
   *           [<store_namesX] is an array of the names of the store objects.
   */
  removeNamesFromUpdateList(action, storeType, data) {
    for (let host in data) {
      if (this.boundUpdate[action] && this.boundUpdate[action][storeType] &&
          this.boundUpdate[action][storeType][host]) {
        for (let name in data[host]) {
          let index = this.boundUpdate[action][storeType][host].indexOf(name);
          if (index > -1) {
            this.boundUpdate[action][storeType][host].splice(index, 1);
          }
        }
        if (!this.boundUpdate[action][storeType][host].length) {
          delete this.boundUpdate[action][storeType][host];
        }
      }
    }
    return null;
  }
});

exports.StorageActor = StorageActor;
