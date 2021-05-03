/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { storageTypePool } = require("devtools/server/actors/storage");
const EventEmitter = require("devtools/shared/event-emitter");
const Services = require("Services");

// ms of delay to throttle updates
const BATCH_DELAY = 200;

// Filters "stores-update" response to only include events for
// the storage type we desire
function getFilteredStorageEvents(updates, storageType) {
  const filteredUpdate = Object.create(null);

  // updateType will be "added", "changed", or "deleted"
  for (const updateType in updates) {
    if (updates[updateType][storageType]) {
      if (!filteredUpdate[updateType]) {
        filteredUpdate[updateType] = {};
      }
      filteredUpdate[updateType][storageType] =
        updates[updateType][storageType];
    }
  }

  return Object.keys(filteredUpdate).length > 0 ? filteredUpdate : null;
}

class ParentProcessStorage {
  constructor(storageKey, storageType) {
    this.storageKey = storageKey;
    this.storageType = storageType;

    this.onStoresUpdate = this.onStoresUpdate.bind(this);
    this.onStoresCleared = this.onStoresCleared.bind(this);
  }

  async watch(watcherActor, { onAvailable, onUpdated, onDestroyed }) {
    const browsingContext = watcherActor.browserElement.browsingContext;

    const ActorConstructor = storageTypePool.get(this.storageKey);
    const storageActor = new StorageActorMock(watcherActor);
    this.storageActor = storageActor;
    this.actor = new ActorConstructor(storageActor);

    // Some storage types require to prelist their stores
    if (typeof this.actor.preListStores === "function") {
      await this.actor.preListStores();
    }

    // We have to manage the actor manually, because ResourceWatcher doesn't
    // use the protocol.js specification.
    // resource-available-form is typed as "json"
    // So that we have to manually handle stuff that would normally be
    // automagically done by procotol.js
    // 1) Manage the actor in order to have an actorID on it
    watcherActor.manage(this.actor);
    // 2) Convert to JSON "form"
    const storage = this.actor.form();

    // All resources should have a resourceType, resourceId and resourceKey
    // attributes, so available/updated/destroyed callbacks work properly.
    storage.resourceType = this.storageType;
    storage.resourceId = `${this.storageType}-${browsingContext.id}`;
    storage.resourceKey = this.storageKey;
    // NOTE: the resource watcher needs this attribute
    storage.browsingContextID = browsingContext.id;

    onAvailable([storage]);

    // Maps global events from `storageActor` shared for all storage-types,
    // down to storage-type's specific actor `storage`.
    storageActor.on("stores-update", this.onStoresUpdate);

    // When a store gets cleared
    storageActor.on("stores-cleared", this.onStoresCleared);
  }

  onStoresUpdate(response) {
    response = getFilteredStorageEvents(response, this.storageKey);
    if (!response) {
      return;
    }
    this.actor.emit("single-store-update", {
      changed: response.changed,
      added: response.added,
      deleted: response.deleted,
    });
  }

  onStoresCleared(response) {
    const cleared = response[this.storageKey];

    if (!cleared) {
      return;
    }

    this.actor.emit("single-store-cleared", {
      clearedHostsOrPaths: cleared,
    });
  }

  destroy() {
    this.actor?.destroy();
    this.actor = null;
    if (this.storageActor) {
      this.storageActor.off("stores-update", this.onStoresUpdate);
      this.storageActor.off("stores-cleared", this.onStoresCleared);
      this.storageActor.destroy();
      this.storageActor = null;
    }
  }
}

module.exports = ParentProcessStorage;

class StorageActorMock extends EventEmitter {
  constructor(watcherActor) {
    super();

    this.conn = watcherActor.conn;
    this.watcherActor = watcherActor;

    this.observe = this.observe.bind(this);
    // Notifications that help us keep track of newly added windows and windows
    // that got removed
    Services.obs.addObserver(this, "window-global-created");
    Services.obs.addObserver(this, "window-global-destroyed");

    this.boundUpdate = {};
  }

  destroy() {
    // Remove observers
    Services.obs.removeObserver(this, "window-global-created");
    Services.obs.removeObserver(this, "window-global-destroyed");

    // clear update throttle timeout
    clearTimeout(this.batchTimer);
    this.batchTimer = null;
  }

  get windows() {
    const browsingContext = this.watcherActor.browserElement.browsingContext;
    const contexts = browsingContext.getAllBrowsingContextsInSubtree();
    // NOTE: we are removing about:blank because we might get them for iframes
    // whose src attribute has not been set yet.
    return contexts
      .filter(x => !!x.currentWindowGlobal)
      .map(x => {
        const uri = x.currentWindowGlobal.documentURI;
        return { location: uri };
      })
      .filter(x => x.location.displaySpec !== "about:blank");
  }

  // NOTE: this uri argument is not a real window.Location, but the
  // `currentWindowGlobal.documentURI` object passed from `windows` getter.
  getHostName(uri) {
    switch (uri.scheme) {
      case "about":
      case "file":
      case "javascript":
      case "resource":
      case "moz-extension":
        return uri.displaySpec;
      case "http":
      case "https":
        return uri.prePath;
      default:
        // chrome: and data: do not support storage
        return null;
    }
  }

  getWindowFromHost(host) {
    const browsingContext = this.watcherActor.browserElement.browsingContext;
    const contexts = browsingContext
      .getAllBrowsingContextsInSubtree()
      .filter(x => !!x.currentWindowGlobal);
    const hostBrowsingContext = contexts.find(x => {
      const hostName = this.getHostName(x.currentWindowGlobal.documentURI);
      return hostName === host;
    });

    const principal = hostBrowsingContext.currentWindowGlobal.documentPrincipal;

    return { document: { effectiveStoragePrincipal: principal } };
  }

  get parentActor() {
    return { isRootActor: !this.watcherActor.browserId };
  }

  /**
   * Event handler for any docshell update. This lets us figure out whenever
   * any new window is added, or an existing window is removed.
   */
  observe(subject, topic) {
    // If the watcher is bound to one browser element (i.e. a tab), ignore
    // updates related to other browser elements
    if (
      this.watcherActor.browserId &&
      subject.browsingContext.browserId != this.watcherActor.browserId
    ) {
      return;
    }
    // ignore about:blank
    if (subject.documentURI.displaySpec === "about:blank") {
      return;
    }

    const windowMock = { location: subject.documentURI };
    if (topic === "window-global-created") {
      this.emit("window-ready", windowMock);
    } else if (topic === "window-global-destroyed") {
      this.emit("window-destroyed", windowMock);
    }
  }

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
  // eslint-disable-next-line complexity
  update(action, storeType, data) {
    if (action == "cleared") {
      this.emit("stores-cleared", { [storeType]: data });
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
    for (const host in data) {
      if (!this.boundUpdate[action][storeType][host]) {
        this.boundUpdate[action][storeType][host] = [];
      }
      for (const name of data[host]) {
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
    } else if (
      action == "changed" &&
      this.boundUpdate.added &&
      this.boundUpdate.added[storeType]
    ) {
      // If something got added and changed at the same time, then remove those
      // items from changed instead.
      this.removeNamesFromUpdateList(
        "changed",
        storeType,
        this.boundUpdate.added[storeType]
      );
    } else if (action == "deleted") {
      // If any item got delete, or a host got delete, no point in sending
      // added or changed update
      this.removeNamesFromUpdateList("added", storeType, data);
      this.removeNamesFromUpdateList("changed", storeType, data);

      for (const host in data) {
        if (
          data[host].length == 0 &&
          this.boundUpdate.added &&
          this.boundUpdate.added[storeType] &&
          this.boundUpdate.added[storeType][host]
        ) {
          delete this.boundUpdate.added[storeType][host];
        }
        if (
          data[host].length == 0 &&
          this.boundUpdate.changed &&
          this.boundUpdate.changed[storeType] &&
          this.boundUpdate.changed[storeType][host]
        ) {
          delete this.boundUpdate.changed[storeType][host];
        }
      }
    }

    this.batchTimer = setTimeout(() => {
      clearTimeout(this.batchTimer);
      this.emit("stores-update", this.boundUpdate);
      this.boundUpdate = {};
    }, BATCH_DELAY);

    return null;
  }

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
    for (const host in data) {
      if (
        this.boundUpdate[action] &&
        this.boundUpdate[action][storeType] &&
        this.boundUpdate[action][storeType][host]
      ) {
        for (const name of data[host]) {
          const index = this.boundUpdate[action][storeType][host].indexOf(name);
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
}
