/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {XPCOMUtils} = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

ChromeUtils.defineModuleGetter(this, "OS", "resource://gre/modules/osfile.jsm");
XPCOMUtils.defineLazyGlobalGetters(this, ["fetch"]);

/**
 * A file (disk) based persistent cache of a JSON serializable object.
 */
this.PersistentCache = class PersistentCache {
  /**
   * Create a cache object based on a name.
   *
   * @param {string} name Name of the cache. It will be used to create the filename.
   * @param {boolean} preload (optional). Whether the cache should be preloaded from file. Defaults to false.
   */
  constructor(name, preload = false) {
    this.name = name;
    this._filename = `activity-stream.${name}.json`;
    if (preload) {
      this._load();
    }
  }

  /**
   * Set a value to be cached with the specified key.
   *
   * @param {string} key The cache key.
   * @param {object} value The data to be cached.
   */
  async set(key, value) {
    const data = await this._load();
    data[key] = value;
    await this._persist(data);
  }

  /**
   * Get a value from the cache.
   *
   * @param {string} key (optional) The cache key. If not provided, we return the full cache.
   * @returns {object} The cached data.
   */
  async get(key) {
    const data = await this._load();
    return key ? data[key] : data;
  }

  /**
   * Load the cache into memory if it isn't already.
   */
  _load() {
    return this._cache || (this._cache = new Promise(async resolve => {
      let file;
      let data = {};
      const filepath = OS.Path.join(OS.Constants.Path.localProfileDir, this._filename);

      try {
        file = await fetch(`file://${filepath}`);
      } catch (error) {} // Cache file doesn't exist yet.

      if (file) {
        try {
          data = await file.json();
        } catch (error) {
          Cu.reportError(`Failed to parse ${this._filename}: ${error.message}`);
        }
      }

      resolve(data);
    }));
  }

  /**
   * Persist the cache to file.
   */
  _persist(data) {
    const filepath = OS.Path.join(OS.Constants.Path.localProfileDir, this._filename);
    return OS.File.writeAtomic(filepath, JSON.stringify(data), {tmpPath: `${filepath}.tmp`});
  }
};

const EXPORTED_SYMBOLS = ["PersistentCache"];
