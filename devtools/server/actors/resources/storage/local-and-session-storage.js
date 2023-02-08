/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  BaseStorageActor,
  DEFAULT_VALUE,
} = require("resource://devtools/server/actors/resources/storage/index.js");
const {
  LongStringActor,
} = require("resource://devtools/server/actors/string.js");

class LocalOrSessionStorageActor extends BaseStorageActor {
  constructor(storageActor, typeName) {
    super(storageActor, typeName);

    Services.obs.addObserver(this, "dom-storage2-changed");
    Services.obs.addObserver(this, "dom-private-storage2-changed");
  }

  destroy() {
    if (this.isDestroyed()) {
      return;
    }
    Services.obs.removeObserver(this, "dom-storage2-changed");
    Services.obs.removeObserver(this, "dom-private-storage2-changed");

    super.destroy();
  }

  getNamesForHost(host) {
    const storage = this.hostVsStores.get(host);
    return storage ? Object.keys(storage) : [];
  }

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
  }

  // We need to override this method as populateStoresForHost expect the window object
  populateStoresForHosts() {
    this.hostVsStores = new Map();
    for (const window of this.windows) {
      const host = this.getHostName(window.location);
      if (host) {
        this.populateStoresForHost(host, window);
      }
    }
  }

  populateStoresForHost(host, window) {
    try {
      this.hostVsStores.set(host, window[this.typeName]);
    } catch (ex) {
      console.warn(
        `Failed to enumerate ${this.typeName} for host ${host}: ${ex}`
      );
    }
  }

  async getFields() {
    return [
      { name: "name", editable: true },
      { name: "value", editable: true },
    ];
  }

  async addItem(guid, host) {
    const storage = this.hostVsStores.get(host);
    if (!storage) {
      return;
    }
    storage.setItem(guid, DEFAULT_VALUE);
  }

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
  }

  async removeItem(host, name) {
    const storage = this.hostVsStores.get(host);
    if (!storage) {
      return;
    }
    storage.removeItem(name);
  }

  async removeAll(host) {
    const storage = this.hostVsStores.get(host);
    if (!storage) {
      return;
    }
    storage.clear();
  }

  observe(subject, topic, data) {
    if (
      (topic != "dom-storage2-changed" &&
        topic != "dom-private-storage2-changed") ||
      data != this.typeName
    ) {
      return null;
    }

    const host = this.getSchemaAndHost(subject.url);

    if (!this.hostVsStores.has(host)) {
      return null;
    }

    let action = "changed";
    if (subject.key == null) {
      return this.storageActor.update("cleared", this.typeName, [host]);
    } else if (subject.oldValue == null) {
      action = "added";
    } else if (subject.newValue == null) {
      action = "deleted";
    }
    const updateData = {};
    updateData[host] = [subject.key];
    return this.storageActor.update(action, this.typeName, updateData);
  }

  /**
   * Given a url, correctly determine its protocol + hostname part.
   */
  getSchemaAndHost(url) {
    const uri = Services.io.newURI(url);
    if (!uri.host) {
      return uri.spec;
    }
    return uri.scheme + "://" + uri.hostPort;
  }

  toStoreObject(item) {
    if (!item) {
      return null;
    }

    return {
      name: item.name,
      value: new LongStringActor(this.conn, item.value || ""),
    };
  }
}

class LocalStorageActor extends LocalOrSessionStorageActor {
  constructor(storageActor) {
    super(storageActor, "localStorage");
  }
}
exports.LocalStorageActor = LocalStorageActor;

class SessionStorageActor extends LocalOrSessionStorageActor {
  constructor(storageActor) {
    super(storageActor, "sessionStorage");
  }
}
exports.SessionStorageActor = SessionStorageActor;
