/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { storageTypePool } = require("devtools/server/actors/storage");
const EventEmitter = require("devtools/shared/event-emitter");
const { Ci } = require("chrome");

loader.lazyRequireGetter(
  this,
  "getAddonIdForWindowGlobal",
  "devtools/server/actors/watcher/browsing-context-helpers.sys.mjs",
  true
);

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

  return Object.keys(filteredUpdate).length ? filteredUpdate : null;
}

class ContentProcessStorage {
  constructor(storageKey, storageType) {
    this.storageKey = storageKey;
    this.storageType = storageType;

    this.onStoresUpdate = this.onStoresUpdate.bind(this);
    this.onStoresCleared = this.onStoresCleared.bind(this);
  }

  async watch(targetActor, { onAvailable }) {
    const ActorConstructor = storageTypePool.get(this.storageKey);
    const storageActor = new StorageActorMock(targetActor);
    this.storageActor = storageActor;
    this.actor = new ActorConstructor(storageActor);

    // Some storage types require to prelist their stores
    if (typeof this.actor.preListStores === "function") {
      await this.actor.preListStores();
    }

    // We have to manage the actor manually, because ResourceCommand doesn't
    // use the protocol.js specification.
    // resource-available-form is typed as "json"
    // So that we have to manually handle stuff that would normally be
    // automagically done by procotol.js
    // 1) Manage the actor in order to have an actorID on it
    targetActor.manage(this.actor);
    // 2) Convert to JSON "form"
    const form = this.actor.form();

    // NOTE: this is hoisted, so the `update` method above may use it.
    const storage = form;

    // All resources should have a resourceType, resourceId and resourceKey
    // attributes, so available/updated/destroyed callbacks work properly.
    storage.resourceType = this.storageType;
    storage.resourceId = this.storageType;
    storage.resourceKey = this.storageKey;

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
      this.storageActor.on("stores-update", this.onStoresUpdate);
      this.storageActor.on("stores-cleared", this.onStoresCleared);
      this.storageActor.destroy();
      this.storageActor = null;
    }
  }
}

module.exports = ContentProcessStorage;

// This class mocks what used to be implement in devtools/server/actors/storage.js: StorageActor
// But without being a protocol.js actor, nor implement any RDP method/event.
// An instance of this class is passed to each storage type actor and named `storageActor`.
// Once we implement all storage type in watcher classes, we can get rid of the original
// StorageActor in devtools/server/actors/storage.js
class StorageActorMock extends EventEmitter {
  constructor(targetActor) {
    super();
    // Storage classes fetch conn from storageActor
    this.conn = targetActor.conn;
    this.targetActor = targetActor;

    this.childWindowPool = new Set();

    // Fetch all the inner iframe windows in this tab.
    this.fetchChildWindows(this.targetActor.docShell);

    // Notifications that help us keep track of newly added windows and windows
    // that got removed
    Services.obs.addObserver(this, "content-document-global-created");
    Services.obs.addObserver(this, "inner-window-destroyed");
    this.onPageChange = this.onPageChange.bind(this);

    const handler = targetActor.chromeEventHandler;
    handler.addEventListener("pageshow", this.onPageChange, true);
    handler.addEventListener("pagehide", this.onPageChange, true);

    this.destroyed = false;
    this.boundUpdate = {};
  }

  destroy() {
    clearTimeout(this.batchTimer);
    this.batchTimer = null;
    // Remove observers
    Services.obs.removeObserver(this, "content-document-global-created");
    Services.obs.removeObserver(this, "inner-window-destroyed");
    this.destroyed = true;
    if (this.targetActor.browser) {
      this.targetActor.browser.removeEventListener(
        "pageshow",
        this.onPageChange,
        true
      );
      this.targetActor.browser.removeEventListener(
        "pagehide",
        this.onPageChange,
        true
      );
    }
    this.childWindowPool.clear();

    this.childWindowPool = null;
    this.targetActor = null;
    this.boundUpdate = null;
  }

  get window() {
    return this.targetActor.window;
  }

  get document() {
    return this.targetActor.window.document;
  }

  get windows() {
    return this.childWindowPool;
  }

  /**
   * Given a docshell, recursively find out all the child windows from it.
   *
   * @param {nsIDocShell} item
   *        The docshell from which all inner windows need to be extracted.
   */
  fetchChildWindows(item) {
    const docShell = item
      .QueryInterface(Ci.nsIDocShell)
      .QueryInterface(Ci.nsIDocShellTreeItem);
    if (!docShell.contentViewer) {
      return null;
    }
    const window = docShell.contentViewer.DOMDocument.defaultView;
    if (window.location.href == "about:blank") {
      // Skip out about:blank windows as Gecko creates them multiple times while
      // creating any global.
      return null;
    }
    if (!this.isIncludedInTopLevelWindow(window)) {
      return null;
    }
    this.childWindowPool.add(window);
    for (let i = 0; i < docShell.childCount; i++) {
      const child = docShell.getChildAt(i);
      this.fetchChildWindows(child);
    }
    return null;
  }

  isIncludedInTargetExtension(subject) {
    const addonId = getAddonIdForWindowGlobal(subject.windowGlobalChild);
    return addonId && addonId === this.targetActor.addonId;
  }

  isIncludedInTopLevelWindow(window) {
    return this.targetActor.windows.includes(window);
  }

  getWindowFromInnerWindowID(innerID) {
    innerID = innerID.QueryInterface(Ci.nsISupportsPRUint64).data;
    for (const win of this.childWindowPool.values()) {
      const id = win.windowGlobalChild.innerWindowId;
      if (id == innerID) {
        return win;
      }
    }
    return null;
  }

  getWindowFromHost(host) {
    for (const win of this.childWindowPool.values()) {
      const origin = win.document.nodePrincipal.originNoSuffix;
      const url = win.document.URL;
      if (origin === host || url === host) {
        return win;
      }
    }
    return null;
  }

  /**
   * Event handler for any docshell update. This lets us figure out whenever
   * any new window is added, or an existing window is removed.
   */
  observe(subject, topic) {
    if (
      subject.location &&
      (!subject.location.href || subject.location.href == "about:blank")
    ) {
      return null;
    }

    // We don't want to try to find a top level window for an extension page, as
    // in many cases (e.g. background page), it is not loaded in a tab, and
    // 'isIncludedInTopLevelWindow' throws an error
    if (
      topic == "content-document-global-created" &&
      (this.isIncludedInTargetExtension(subject) ||
        this.isIncludedInTopLevelWindow(subject))
    ) {
      this.childWindowPool.add(subject);
      this.emit("window-ready", subject);
    } else if (topic == "inner-window-destroyed") {
      const window = this.getWindowFromInnerWindowID(subject);
      if (window) {
        this.childWindowPool.delete(window);
        this.emit("window-destroyed", window);
      }
    }
    return null;
  }

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
  onPageChange({ target, type, persisted }) {
    if (this.destroyed) {
      return;
    }

    const window = target.defaultView;

    if (type == "pagehide" && this.childWindowPool.delete(window)) {
      this.emit("window-destroyed", window);
    } else if (
      type == "pageshow" &&
      persisted &&
      window.location.href &&
      window.location.href != "about:blank" &&
      this.isIncludedInTopLevelWindow(window)
    ) {
      this.childWindowPool.add(window);
      this.emit("window-ready", window);
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
          !data[host].length &&
          this.boundUpdate.added &&
          this.boundUpdate.added[storeType] &&
          this.boundUpdate.added[storeType][host]
        ) {
          delete this.boundUpdate.added[storeType][host];
        }
        if (
          !data[host].length &&
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
        for (const name in data[host]) {
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
