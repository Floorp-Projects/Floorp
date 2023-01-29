/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  StorageActors,
  DEFAULT_VALUE,
} = require("resource://devtools/server/actors/resources/storage/index.js");
const {
  LongStringActor,
} = require("resource://devtools/server/actors/string.js");

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
exports.LocalStorageActor = StorageActors.createActor(
  {
    typeName: "localStorage",
    observationTopics: ["dom-storage2-changed", "dom-private-storage2-changed"],
  },
  getObjectForLocalOrSessionStorage("localStorage")
);

/**
 * The Session Storage actor and front.
 */
exports.SessionStorageActor = StorageActors.createActor(
  {
    typeName: "sessionStorage",
    observationTopics: ["dom-storage2-changed", "dom-private-storage2-changed"],
  },
  getObjectForLocalOrSessionStorage("sessionStorage")
);
