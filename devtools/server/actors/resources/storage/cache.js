/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  BaseStorageActor,
} = require("resource://devtools/server/actors/resources/storage/index.js");

class CacheStorageActor extends BaseStorageActor {
  constructor(storageActor) {
    super(storageActor, "Cache");
  }

  async populateStoresForHost(host) {
    const storeMap = new Map();
    const caches = await this.getCachesForHost(host);
    try {
      for (const name of await caches.keys()) {
        storeMap.set(name, await caches.open(name));
      }
    } catch (ex) {
      console.warn(`Failed to enumerate CacheStorage for host ${host}: ${ex}`);
    }
    this.hostVsStores.set(host, storeMap);
  }

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
  }

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
  }

  getNamesForHost(host) {
    // UI code expect each name to be a JSON string of an array :/
    return [...this.hostVsStores.get(host).keys()].map(a => {
      return JSON.stringify([a]);
    });
  }

  async getValuesForHost(host, name) {
    if (!name) {
      // if we get here, we most likely clicked on the refresh button
      // which called getStoreObjects, itself calling this method,
      // all that, without having selected any particular cache name.
      //
      // Try to detect if a new cache has been added and notify the client
      // asynchronously, via a RDP event.
      const previousCaches = [...this.hostVsStores.get(host).keys()];
      await this.populateStoresForHosts();
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
  }

  async processEntry(request, response) {
    return {
      url: String(request.url),
      status: String(response.statusText),
    };
  }

  async getFields() {
    return [
      { name: "url", editable: false },
      { name: "status", editable: false },
    ];
  }

  /**
   * Given a url, correctly determine its protocol + hostname part.
   */
  getSchemaAndHost(url) {
    const uri = Services.io.newURI(url);
    return uri.scheme + "://" + uri.hostPort;
  }

  toStoreObject(item) {
    return item;
  }

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
  }

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
  }

  /**
   * CacheStorage API doesn't support any notifications, we must fake them
   */
  onItemUpdated(action, host, path) {
    this.storageActor.update(action, "Cache", {
      [host]: [JSON.stringify(path)],
    });
  }
}
exports.CacheStorageActor = CacheStorageActor;
