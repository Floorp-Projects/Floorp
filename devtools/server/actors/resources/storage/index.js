/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Actor } = require("resource://devtools/shared/protocol.js");
const specs = require("resource://devtools/shared/specs/storage.js");

loader.lazyRequireGetter(
  this,
  "naturalSortCaseInsensitive",
  "resource://devtools/shared/natural-sort.js",
  true
);

// Maximum number of cookies/local storage key-value-pairs that can be sent
// over the wire to the client in one request.
const MAX_STORE_OBJECT_COUNT = 50;
exports.MAX_STORE_OBJECT_COUNT = MAX_STORE_OBJECT_COUNT;

const DEFAULT_VALUE = "value";
exports.DEFAULT_VALUE = DEFAULT_VALUE;

// GUID to be used as a separator in compound keys. This must match the same
// constant in devtools/client/storage/ui.js,
// devtools/client/storage/test/head.js and
// devtools/server/tests/browser/head.js
const SEPARATOR_GUID = "{9d414cc5-8319-0a04-0586-c0a6ae01670a}";
exports.SEPARATOR_GUID = SEPARATOR_GUID;

class BaseStorageActor extends Actor {
  /**
   * Base class with the common methods required by all storage actors.
   *
   * This base class is missing a couple of required methods that should be
   * implemented seperately for each actor. They are namely:
   *   - observe : Method which gets triggered on the notification of the watched
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
   */
  constructor(storageActor, typeName) {
    super(storageActor.conn, specs.childSpecs[typeName]);

    this.storageActor = storageActor;

    // Map keyed by host name whose values are nested Maps.
    // Nested maps are keyed by store names and values are store values.
    // Store values are specific to each sub class.
    // Map(host name => stores <Map(name => values )>)
    // Map(string => stores <Map(string => any )>)
    this.hostVsStores = new Map();

    this.onWindowReady = this.onWindowReady.bind(this);
    this.onWindowDestroyed = this.onWindowDestroyed.bind(this);
    this.storageActor.on("window-ready", this.onWindowReady);
    this.storageActor.on("window-destroyed", this.onWindowDestroyed);
  }

  destroy() {
    if (!this.storageActor) {
      return;
    }

    this.storageActor.off("window-ready", this.onWindowReady);
    this.storageActor.off("window-destroyed", this.onWindowDestroyed);

    this.hostVsStores.clear();

    super.destroy();

    this.storageActor = null;
  }

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
  }

  /**
   * Returns all the windows present on the page. Includes main window + inner
   * iframe windows.
   */
  get windows() {
    return this.storageActor.windows;
  }

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
  }

  /**
   * Populates a map of known hosts vs a map of stores vs value.
   */
  async populateStoresForHosts() {
    for (const host of this.hosts) {
      await this.populateStoresForHost(host);
    }
  }

  getNamesForHost(host) {
    return [...this.hostVsStores.get(host).keys()];
  }

  getValuesForHost(host, name) {
    if (name) {
      return [this.hostVsStores.get(host).get(name)];
    }
    return [...this.hostVsStores.get(host).values()];
  }

  getObjectsSize(host, names) {
    return names.length;
  }

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
      this.storageActor.update("added", this.typeName, data);
    }
  }

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
      this.storageActor.update("deleted", this.typeName, data);
    }
  }

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
  }

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
  }

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
  }

  getPrincipal(win) {
    if (win) {
      return win.document.effectiveStoragePrincipal;
    }
    // We are running in the browser toolbox and viewing system DBs so we
    // need to use system principal.
    return Cc["@mozilla.org/systemprincipal;1"].createInstance(Ci.nsIPrincipal);
  }
}
exports.BaseStorageActor = BaseStorageActor;
