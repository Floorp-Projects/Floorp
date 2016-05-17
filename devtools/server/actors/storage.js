/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Cc, Ci} = require("chrome");
const events = require("sdk/event/core");
const protocol = require("devtools/shared/protocol");
const {LongStringActor} = require("devtools/server/actors/string");
const {DebuggerServer} = require("devtools/server/main");
const Services = require("Services");
const promise = require("promise");
const {isWindowIncluded} = require("devtools/shared/layout/utils");
const specs = require("devtools/shared/specs/storage");

loader.lazyImporter(this, "OS", "resource://gre/modules/osfile.jsm");
loader.lazyImporter(this, "Sqlite", "resource://gre/modules/Sqlite.jsm");
loader.lazyImporter(this, "Task", "resource://gre/modules/Task.jsm", "Task");

var gTrackedMessageManager = new Map();

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
 *   - getValuesForHost : Given a host (and optianally a name) get all known
 *                        store objects.
 *   - toStoreObject : Given a store object, convert it to the required format
 *                     so that it can be transferred over wire.
 *   - populateStoresForHost : Given a host, populate the map of all store
 *                             objects for it
 *
 * @param {string} typeName
 *        The typeName of the actor.
 * @param {string} observationTopic
 *        The topic which this actor listens to via Notification Observers.
 */
StorageActors.defaults = function(typeName, observationTopic) {
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
    getHostName: function(location) {
      return location.hostname || location.href;
    },

    initialize: function(storageActor) {
      protocol.Actor.prototype.initialize.call(this, null);

      this.storageActor = storageActor;

      this.populateStoresForHosts();
      if (observationTopic) {
        Services.obs.addObserver(this, observationTopic, false);
      }
      this.onWindowReady = this.onWindowReady.bind(this);
      this.onWindowDestroyed = this.onWindowDestroyed.bind(this);
      events.on(this.storageActor, "window-ready", this.onWindowReady);
      events.on(this.storageActor, "window-destroyed", this.onWindowDestroyed);
    },

    destroy: function() {
      if (observationTopic) {
        Services.obs.removeObserver(this, observationTopic, false);
      }
      events.off(this.storageActor, "window-ready", this.onWindowReady);
      events.off(this.storageActor, "window-destroyed", this.onWindowDestroyed);

      this.hostVsStores.clear();
      this.storageActor = null;
    },

    getNamesForHost: function(host) {
      return [...this.hostVsStores.get(host).keys()];
    },

    getValuesForHost: function(host, name) {
      if (name) {
        return [this.hostVsStores.get(host).get(name)];
      }
      return [...this.hostVsStores.get(host).values()];
    },

    getObjectsSize: function(host, names) {
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
    onWindowDestroyed: function(window) {
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

    form: function(form, detail) {
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
    populateStoresForHosts: function() {
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
 *         - observationTopic {string}
 *                            The topic which this actor listens to via
 *                            Notification Observers.
 * @param {object} overrides
 *        All the methods which you want to be different from the ones in
 *        StorageActors.defaults method plus the required ones described there.
 */
StorageActors.createActor = function(options = {}, overrides = {}) {
  let actorObject = StorageActors.defaults(
    options.typeName,
    options.observationTopic || null
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
  initialize: function(storageActor) {
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

  destroy: function() {
    this.hostVsStores.clear();

    // We need to remove the cookie listeners early in E10S mode so we need to
    // use a conditional here to ensure that we only attempt to remove them in
    // single process mode.
    if (!DebuggerServer.isInChildProcess) {
      this.removeCookieObservers();
    }

    events.off(this.storageActor, "window-ready", this.onWindowReady);
    events.off(this.storageActor, "window-destroyed", this.onWindowDestroyed);

    this._pendingResponse = this.storageActor = null;
  },

  /**
   * Given a cookie object, figure out all the matching hosts from the page that
   * the cookie belong to.
   */
  getMatchingHosts: function(cookies) {
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
  isCookieAtHost: function(cookie, host) {
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

  toStoreObject: function(cookie) {
    if (!cookie) {
      return null;
    }

    return {
      name: cookie.name,
      path: cookie.path || "",
      host: cookie.host || "",

      // because expires is in seconds
      expires: (cookie.expires || 0) * 1000,

      // because it is in micro seconds
      creationTime: cookie.creationTime / 1000,

      // - do -
      lastAccessed: cookie.lastAccessed / 1000,
      value: new LongStringActor(this.conn, cookie.value || ""),
      isDomain: cookie.isDomain,
      isSecure: cookie.isSecure,
      isHttpOnly: cookie.isHttpOnly
    };
  },

  populateStoresForHost: function(host) {
    this.hostVsStores.set(host, new Map());

    let cookies = this.getCookiesFromHost(host);

    for (let cookie of cookies) {
      if (this.isCookieAtHost(cookie, host)) {
        this.hostVsStores.get(host).set(cookie.name, cookie);
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
  onCookieChanged: function(subject, topic, action) {
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
            this.hostVsStores.get(host).set(subject.name, subject);
            data[host] = [subject.name];
          }
          this.storageActor.update(action, "cookies", data);
        }
        break;

      case "deleted":
        if (hosts.length) {
          for (let host of hosts) {
            this.hostVsStores.get(host).delete(subject.name);
            data[host] = [subject.name];
          }
          this.storageActor.update("deleted", "cookies", data);
        }
        break;

      case "batch-deleted":
        if (hosts.length) {
          for (let host of hosts) {
            let stores = [];
            for (let cookie of subject) {
              this.hostVsStores.get(host).delete(cookie.name);
              stores.push(cookie.name);
            }
            data[host] = stores;
          }
          this.storageActor.update("deleted", "cookies", data);
        }
        break;

      case "cleared":
        this.storageActor.update("cleared", "cookies", hosts);
        break;

      case "reload":
        this.storageActor.update("reloaded", "cookies", hosts);
        break;
    }
    return null;
  },

  /**
   * This method marks the table as editable.
   *
   * @return {Array}
   *         An array of column header ids.
   */
  getEditableFields: Task.async(function* () {
    return [
      "name",
      "path",
      "host",
      "expires",
      "value",
      "isSecure",
      "isHttpOnly"
    ];
  }),

  /**
   * Pass the editItem command from the content to the chrome process.
   *
   * @param {Object} data
   *        See editCookie() for format details.
   */
  editItem: Task.async(function* (data) {
    this.editCookie(data);
  }),

  removeItem: Task.async(function* (host, name) {
    this.removeCookie(host, name);
  }),

  removeAll: Task.async(function* (host, domain) {
    this.removeAllCookies(host, domain);
  }),

  maybeSetupChildProcess: function() {
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

    addMessageListener("storage:storage-cookie-request-child",
                       cookieHelpers.handleParentRequest);

    function callParentProcess(methodName, ...args) {
      let reply = sendSyncMessage("storage:storage-cookie-request-parent", {
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
  getCookiesFromHost: function(host) {
    // Local files have no host.
    if (host.startsWith("file:///")) {
      host = "";
    }

    let cookies = Services.cookies.getCookiesFromHost(host);
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
   *          key: "name",
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
  editCookie: function(data) {
    let {field, oldValue, newValue} = data;
    let origName = field === "name" ? oldValue : data.items.name;
    let origHost = field === "host" ? oldValue : data.items.host;
    let origPath = field === "path" ? oldValue : data.items.path;
    let cookie = null;

    let enumerator = Services.cookies.getCookiesFromHost(origHost);
    while (enumerator.hasMoreElements()) {
      let nsiCookie = enumerator.getNext().QueryInterface(Ci.nsICookie2);
      if (nsiCookie.name === origName && nsiCookie.host === origHost) {
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

    // If the date is expired set it for 1 minute in the future.
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
      cookie.isSession ? MAX_COOKIE_EXPIRY : cookie.expires
    );
  },

  _removeCookies: function(host, opts = {}) {
    function hostMatches(cookieHost, matchHost) {
      if (cookieHost == null) {
        return matchHost == null;
      }
      if (cookieHost.startsWith(".")) {
        return ("." + matchHost).endsWith(cookieHost);
      }
      return cookieHost == host;
    }

    let enumerator = Services.cookies.getCookiesFromHost(host);
    while (enumerator.hasMoreElements()) {
      let cookie = enumerator.getNext().QueryInterface(Ci.nsICookie2);
      if (hostMatches(cookie.host, host) &&
          (!opts.name || cookie.name === opts.name) &&
          (!opts.domain || cookie.host === opts.domain)) {
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

  removeCookie: function(host, name) {
    if (name !== undefined) {
      this._removeCookies(host, { name });
    }
  },

  removeAllCookies: function(host, domain) {
    this._removeCookies(host, { domain });
  },

  addCookieObservers: function() {
    Services.obs.addObserver(cookieHelpers, "cookie-changed", false);
    return null;
  },

  removeCookieObservers: function() {
    Services.obs.removeObserver(cookieHelpers, "cookie-changed", false);
    return null;
  },

  observe: function(subject, topic, data) {
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

  handleParentRequest: function(msg) {
    switch (msg.json.method) {
      case "onCookieChanged":
        let [cookie, topic, data] = msg.data.args;
        cookie = JSON.parse(cookie);
        cookieHelpers.onCookieChanged(cookie, topic, data);
        break;
    }
  },

  handleChildRequest: function(msg) {
    switch (msg.json.method) {
      case "getCookiesFromHost": {
        let host = msg.data.args[0];
        let cookies = cookieHelpers.getCookiesFromHost(host);
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
        return cookieHelpers.removeCookie(host, name);
      }
      case "removeAllCookies": {
        let host = msg.data.args[0];
        let domain = msg.data.args[1];
        return cookieHelpers.removeAllCookies(host, domain);
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

exports.setupParentProcessForCookies = function({mm, prefix}) {
  cookieHelpers.onCookieChanged =
    callChildProcess.bind(null, "onCookieChanged");

  // listen for director-script requests from the child process
  mm.addMessageListener("storage:storage-cookie-request-parent",
                        cookieHelpers.handleChildRequest);

  DebuggerServer.once("disconnected-from-child:" + prefix,
                      handleMessageManagerDisconnected);

  gTrackedMessageManager.set("cookies", mm);

  function handleMessageManagerDisconnected(evt, { mm: disconnectedMm }) {
    // filter out not subscribed message managers
    if (disconnectedMm !== mm || !gTrackedMessageManager.has("cookies")) {
      return;
    }

    // Although "disconnected-from-child" implies that the child is already
    // disconnected this is not the case. The disconnection takes place after
    // this method has finished. This gives us chance to clean up items within
    // the parent process e.g. observers.
    cookieHelpers.removeCookieObservers();

    gTrackedMessageManager.delete("cookies");

    // unregister for director-script requests handlers from the parent process
    // (if any)
    mm.removeMessageListener("storage:storage-cookie-request-parent",
                             cookieHelpers.handleChildRequest);
  }

  function callChildProcess(methodName, ...args) {
    if (methodName === "onCookieChanged") {
      args[0] = JSON.stringify(args[0]);
    }

    try {
      mm.sendAsyncMessage("storage:storage-cookie-request-child", {
        method: methodName,
        args: args
      });
    } catch (e) {
      // We may receive a NS_ERROR_NOT_INITIALIZED if the target window has
      // been closed. This can legitimately happen in between test runs.
    }
  }
};

/**
 * Helper method to create the overriden object required in
 * StorageActors.createActor for Local Storage and Session Storage.
 * This method exists as both Local Storage and Session Storage have almost
 * identical actors.
 */
function getObjectForLocalOrSessionStorage(type) {
  return {
    getNamesForHost: function(host) {
      let storage = this.hostVsStores.get(host);
      return Object.keys(storage);
    },

    getValuesForHost: function(host, name) {
      let storage = this.hostVsStores.get(host);
      if (name) {
        return [{name: name, value: storage.getItem(name)}];
      }
      return Object.keys(storage).map(key => {
        return {
          name: key,
          value: storage.getItem(key)
        };
      });
    },

    getHostName: function(location) {
      if (!location.host) {
        return location.href;
      }
      return location.protocol + "//" + location.host;
    },

    populateStoresForHost: function(host, window) {
      try {
        this.hostVsStores.set(host, window[type]);
      } catch (ex) {
        // Exceptions happen when local or session storage is inaccessible
      }
      return null;
    },

    populateStoresForHosts: function() {
      this.hostVsStores = new Map();
      try {
        for (let window of this.windows) {
          this.hostVsStores.set(this.getHostName(window.location),
                                window[type]);
        }
      } catch (ex) {
        // Exceptions happen when local or session storage is inaccessible
      }
      return null;
    },

    /**
     * This method marks the fields as editable.
     *
     * @return {Array}
     *         An array of field ids.
     */
    getEditableFields: Task.async(function* () {
      return [
        "name",
        "value"
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

      if (field === "name") {
        storage.removeItem(oldValue);
      }

      storage.setItem(items.name, items.value);
    }),

    removeItem: Task.async(function* (host, name) {
      let storage = this.hostVsStores.get(host);
      storage.removeItem(name);
    }),

    removeAll: Task.async(function* (host) {
      let storage = this.hostVsStores.get(host);
      storage.clear();
    }),

    observe: function(subject, topic, data) {
      if (topic != "dom-storage2-changed" || data != type) {
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
    getSchemaAndHost: function(url) {
      let uri = Services.io.newURI(url, null, null);
      if (!uri.host) {
        return uri.spec;
      }
      return uri.scheme + "://" + uri.hostPort;
    },

    toStoreObject: function(item) {
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
  observationTopic: "dom-storage2-changed"
}, getObjectForLocalOrSessionStorage("localStorage"));

/**
 * The Session Storage actor and front.
 */
StorageActors.createActor({
  typeName: "sessionStorage",
  observationTopic: "dom-storage2-changed"
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

  form: function(form, detail) {
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

  getNamesForHost: function(host) {
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

  getHostName: function(location) {
    if (!location.host) {
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
      console.error(`Failed to enumerate CacheStorage for host ${host}:`, ex);
    }
    this.hostVsStores.set(host, storeMap);
  }),

  /**
   * This method is overriden and left blank as for Cache Storage, this
   * operation cannot be performed synchronously. Thus, the preListStores
   * method exists to do the same task asynchronously.
   */
  populateStoresForHosts: function() {
    this.hostVsStores = new Map();
  },

  /**
   * Given a url, correctly determine its protocol + hostname part.
   */
  getSchemaAndHost: function(url) {
    let uri = Services.io.newURI(url, null, null);
    return uri.scheme + "://" + uri.hostPort;
  },

  toStoreObject: function(item) {
    return item;
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
  toObject: function() {
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
  toObject: function() {
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
 */
function DatabaseMetadata(origin, db) {
  this._origin = origin;
  this._name = db.name;
  this._version = db.version;
  this._objectStores = [];

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

  toObject: function() {
    return {
      name: this._name,
      origin: this._origin,
      version: this._version,
      objectStores: this._objectStores.size
    };
  }
};

StorageActors.createActor({
  typeName: "indexedDB"
}, {
  initialize: function(storageActor) {
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

  destroy: function() {
    this.hostVsStores.clear();
    this.objectsSize = null;

    events.off(this.storageActor, "window-ready", this.onWindowReady);
    events.off(this.storageActor, "window-destroyed", this.onWindowDestroyed);
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

  getHostName: function(location) {
    if (!location.host) {
      return location.href;
    }
    return location.protocol + "//" + location.host;
  },

  /**
   * This method is overriden and left blank as for indexedDB, this operation
   * cannot be performed synchronously. Thus, the preListStores method exists to
   * do the same task asynchronously.
   */
  populateStoresForHosts: function() {},

  getNamesForHost: function(host) {
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
  getObjectsSize: function(host, names, options) {
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

      for (let name of names) {
        let metadata = yield this.getDBMetaData(host, principal, name);

        metadata = indexedDBHelpers.patchMetadataMapsAndProtos(metadata);
        storeMap.set(name, metadata);
      }
    }

    this.hostVsStores.set(host, storeMap);
  }),

  /**
   * Returns the over-the-wire implementation of the indexed db entity.
   */
  toStoreObject: function(item) {
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
    // Indexed db entry
    return {
      name: item.name,
      value: new LongStringActor(this.conn, JSON.stringify(item.value))
    };
  },

  form: function(form, detail) {
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

  onDatabaseRemoved(host, name) {
    if (this.hostVsStores.has(host)) {
      this.hostVsStores.get(host).delete(name);
    }

    this.storageActor.update("deleted", "indexedDB", {
      [host]: [ JSON.stringify([name]) ]
    });
  },

  maybeSetupChildProcess: function() {
    if (!DebuggerServer.isInChildProcess) {
      this.backToChild = function(...args) {
        return args[1];
      };
      this.getDBMetaData = indexedDBHelpers.getDBMetaData;
      this.openWithPrincipal = indexedDBHelpers.openWithPrincipal;
      this.getDBNamesForHost = indexedDBHelpers.getDBNamesForHost;
      this.getSanitizedHost = indexedDBHelpers.getSanitizedHost;
      this.getNameFromDatabaseFile = indexedDBHelpers.getNameFromDatabaseFile;
      this.getValuesForHost = indexedDBHelpers.getValuesForHost;
      this.getObjectStoreData = indexedDBHelpers.getObjectStoreData;
      this.removeDB = indexedDBHelpers.removeDB;
      return;
    }

    const { sendAsyncMessage, addMessageListener } =
      this.conn.parentMessageManager;

    this.conn.setupInParent({
      module: "devtools/server/actors/storage",
      setupParent: "setupParentProcessForIndexedDB"
    });

    this.getDBMetaData =
      callParentProcessAsync.bind(null, "getDBMetaData");
    this.getDBNamesForHost =
      callParentProcessAsync.bind(null, "getDBNamesForHost");
    this.getValuesForHost =
      callParentProcessAsync.bind(null, "getValuesForHost");
    this.removeDB =
      callParentProcessAsync.bind(null, "removeDB");

    addMessageListener("storage:storage-indexedDB-request-child", msg => {
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
        case "onDatabaseRemoved": {
          let [host, name] = msg.json.args;
          this.onDatabaseRemoved(host, name);
        }
      }
    });

    let unresolvedPromises = new Map();
    function callParentProcessAsync(methodName, ...args) {
      let deferred = promise.defer();

      unresolvedPromises.set(methodName, deferred);

      sendAsyncMessage("storage:storage-indexedDB-request-parent", {
        method: methodName,
        args: args
      });

      return deferred.promise;
    }
  },
});

var indexedDBHelpers = {
  backToChild: function(...args) {
    let mm = Cc["@mozilla.org/globalmessagemanager;1"]
               .getService(Ci.nsIMessageListenerManager);

    mm.broadcastAsyncMessage("storage:storage-indexedDB-request-child", {
      method: "backToChild",
      args: args
    });
  },

  onDatabaseRemoved: function (host, name) {
    let mm = Cc["@mozilla.org/globalmessagemanager;1"]
               .getService(Ci.nsIMessageListenerManager);

    mm.broadcastAsyncMessage("storage:storage-indexedDB-request-child", {
      method: "onDatabaseRemoved",
      args: [ host, name ]
    });
  },

  /**
   * Fetches and stores all the metadata information for the given database
   * `name` for the given `host` with its `principal`. The stored metadata
   * information is of `DatabaseMetadata` type.
   */
  getDBMetaData: Task.async(function* (host, principal, name) {
    let request = this.openWithPrincipal(principal, name);
    let success = promise.defer();

    request.onsuccess = event => {
      let db = event.target.result;

      let dbData = new DatabaseMetadata(host, db);
      db.close();

      success.resolve(this.backToChild("getDBMetaData", dbData));
    };
    request.onerror = () => {
      console.error(
        `Error opening indexeddb database ${name} for host ${host}`);
      success.resolve(this.backToChild("getDBMetaData", null));
    };
    return success.promise;
  }),

  /**
   * Opens an indexed db connection for the given `principal` and
   * database `name`.
   */
  openWithPrincipal: function(principal, name) {
    return require("indexedDB").openForPrincipal(principal, name);
  },

  removeDB: Task.async(function* (host, principal, name) {
    let request = require("indexedDB").deleteForPrincipal(principal, name);

    let result = new promise(resolve => {
      request.onsuccess = () => {
        resolve({});
        this.onDatabaseRemoved(host, name);
      };

      request.onblocked = () => {
        console.error(
          `Deleting indexedDB database ${name} for host ${host} is blocked`);
        resolve({ blocked: true });
      };

      request.onerror = () => {
        console.error(
          `Error deleting indexedDB database ${name} for host ${host}`);
        resolve({ error: request.error });
      };

      // If the database is blocked repeatedly, the onblocked event will not
      // be fired again. To avoid waiting forever, report as blocked if nothing
      // else happens after 3 seconds.
      setTimeout(() => resolve({ blocked: true }), 3000);
    });

    return this.backToChild("removeDB", yield result);
  }),

  /**
   * Fetches all the databases and their metadata for the given `host`.
   */
  getDBNamesForHost: Task.async(function* (host) {
    let sanitizedHost = this.getSanitizedHost(host);
    let directory = OS.Path.join(OS.Constants.Path.profileDir, "storage",
                                 "default", sanitizedHost, "idb");

    let exists = yield OS.File.exists(directory);
    if (!exists && host.startsWith("about:")) {
      // try for moz-safe-about directory
      sanitizedHost = this.getSanitizedHost("moz-safe-" + host);
      directory = OS.Path.join(OS.Constants.Path.profileDir, "storage",
                               "permanent", sanitizedHost, "idb");
      exists = yield OS.File.exists(directory);
    }
    if (!exists) {
      return this.backToChild("getDBNamesForHost", {names: []});
    }

    let names = [];
    let dirIterator = new OS.File.DirectoryIterator(directory);
    try {
      yield dirIterator.forEach(file => {
        // Skip directories.
        if (file.isDir) {
          return null;
        }

        // Skip any non-sqlite files.
        if (!file.name.endsWith(".sqlite")) {
          return null;
        }

        return this.getNameFromDatabaseFile(file.path).then(name => {
          if (name) {
            names.push(name);
          }
          return null;
        });
      });
    } finally {
      dirIterator.close();
    }
    return this.backToChild("getDBNamesForHost", {names: names});
  }),

  /**
   * Removes any illegal characters from the host name to make it a valid file
   * name.
   */
  getSanitizedHost: function(host) {
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
    // will throw. Thus we retey for some time to see if lock is removed.
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
    let result = yield this.getObjectStoreData(host, principal, db2,
      objectStore, id, options.index, options.size);
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
   * @param {string} objectStore
   *        The name of the object store from the above db.
   * @param {string} id
   *        id of the requested entry from the above object store.
   *        null if all entries from the above object store are requested.
   * @param {string} index
   *        name of the IDBIndex to be iterated on while fetching entries.
   *        null or "name" if no index is to be iterated.
   * @param {number} offset
   *        ofsset of the entries to be fetched.
   * @param {number} size
   *        The intended size of the entries to be fetched.
   */
  getObjectStoreData:
  function(host, principal, dbName, objectStore, id, index, offset, size) {
    let request = this.openWithPrincipal(principal, dbName);
    let success = promise.defer();
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
  patchMetadataMapsAndProtos: function(metadata) {
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

  handleChildRequest: function(msg) {
    let args = msg.data.args;

    switch (msg.json.method) {
      case "getDBMetaData": {
        let [host, principal, name] = args;
        return indexedDBHelpers.getDBMetaData(host, principal, name);
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
        let [host, principal, name] = args;
        return indexedDBHelpers.removeDB(host, principal, name);
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

exports.setupParentProcessForIndexedDB = function({mm, prefix}) {
  // listen for director-script requests from the child process
  mm.addMessageListener("storage:storage-indexedDB-request-parent",
                        indexedDBHelpers.handleChildRequest);

  DebuggerServer.once("disconnected-from-child:" + prefix,
                      handleMessageManagerDisconnected);

  gTrackedMessageManager.set("indexedDB", mm);

  function handleMessageManagerDisconnected(evt, { mm: disconnectedMm }) {
    // filter out not subscribed message managers
    if (disconnectedMm !== mm || !gTrackedMessageManager.has("indexedDB")) {
      return;
    }

    gTrackedMessageManager.delete("indexedDB");

    // unregister for director-script requests handlers from the parent process
    // (if any)
    mm.removeMessageListener("storage:storage-indexedDB-request-parent",
                             indexedDBHelpers.handleChildRequest);
  }
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

  initialize: function(conn, tabActor) {
    protocol.Actor.prototype.initialize.call(this, null);

    this.conn = conn;
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

  destroy: function() {
    clearTimeout(this.batchTimer);
    this.batchTimer = null;
    // Remove observers
    Services.obs.removeObserver(this, "content-document-global-created", false);
    Services.obs.removeObserver(this, "inner-window-destroyed", false);
    this.destroyed = true;
    if (this.parentActor.browser) {
      this.parentActor.browser.removeEventListener(
        "pageshow", this.onPageChange, true);
      this.parentActor.browser.removeEventListener(
        "pagehide", this.onPageChange, true);
    }
    // Destroy the registered store types
    for (let actor of this.childActorPool.values()) {
      actor.destroy();
    }
    this.childActorPool.clear();
    this.childWindowPool.clear();
    this.childWindowPool = this.childActorPool = this.__poolMap = this.conn =
      this.parentActor = this.boundUpdate = this.registeredPool =
      this._pendingResponse = null;
  },

  /**
   * Given a docshell, recursively find out all the child windows from it.
   *
   * @param {nsIDocShell} item
   *        The docshell from which all inner windows need to be extracted.
   */
  fetchChildWindows: function(item) {
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

  isIncludedInTopLevelWindow: function(window) {
    return isWindowIncluded(this.window, window);
  },

  getWindowFromInnerWindowID: function(innerID) {
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

  getWindowFromHost: function(host) {
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
  observe: function(subject, topic) {
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
  onPageChange: function({target, type, persisted}) {
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
   *           [<store_namesX] is an array of the names of the changed store
   *           objects. Leave it empty if the host was completely removed.
   *        When the action is "reloaded" or "cleared", `data` is an array of
   *        hosts for which the stores were cleared or reloaded.
   */
  update: function(action, storeType, data) {
    if (action == "cleared" || action == "reloaded") {
      let toSend = {};
      toSend[storeType] = data;
      events.emit(this, "stores-" + action, toSend);
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
  removeNamesFromUpdateList: function(action, storeType, data) {
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
