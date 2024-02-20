/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { PromiseWorker } from "resource://gre/modules/workers/PromiseWorker.mjs";

/**
 * Boilerplate to connect with the main thread PromiseWorker.
 */
const worker = new PromiseWorker.AbstractWorker();
worker.dispatch = function (method, args = []) {
  return agent[method](...args);
};
worker.postMessage = function (message, ...transfers) {
  self.postMessage(message, ...transfers);
};
worker.close = function () {
  self.close();
};

self.addEventListener("message", msg => worker.handleMessage(msg));
self.addEventListener("unhandledrejection", function (error) {
  throw error.reason;
});

/**
 * Stores and manages the Domain-to-Categories Map.
 */
class Agent {
  /**
   * @type {Map<string, Array<number>>} Hashes mapped to categories and values.
   */
  #map = new Map();

  /**
   * Converts data from the array directly into a Map.
   *
   * @param {Array<ArrayBuffer>} fileContents Files
   * @returns {boolean} Returns whether the Map contains results.
   */
  populateMap(fileContents) {
    this.#map.clear();

    for (let fileContent of fileContents) {
      let obj;
      try {
        obj = JSON.parse(new TextDecoder().decode(fileContent));
      } catch (ex) {
        return false;
      }
      for (let objKey in obj) {
        if (Object.hasOwn(obj, objKey)) {
          this.#map.set(objKey, obj[objKey]);
        }
      }
    }
    return this.#map.size > 0;
  }

  /**
   * Retrieves scores for the hash from the map.
   *
   * @param {string} hash Key to look up in the map.
   * @returns {Array<number>}
   */
  getScores(hash) {
    if (this.#map.has(hash)) {
      return this.#map.get(hash);
    }
    return [];
  }

  /**
   * Empties the internal map.
   *
   * @returns {boolean}
   */
  emptyMap() {
    this.#map.clear();
    return true;
  }

  /**
   * Test only function to allow the map to contain information without
   * having to go through Remote Settings.
   *
   * @param {object} obj The data to directly import into the Map.
   * @returns {boolean} Whether the map contains values.
   */
  overrideMapForTests(obj) {
    this.#map.clear();
    for (let objKey in obj) {
      if (Object.hasOwn(obj, objKey)) {
        this.#map.set(objKey, obj[objKey]);
      }
    }
    return this.#map.size > 0;
  }
}

const agent = new Agent();
