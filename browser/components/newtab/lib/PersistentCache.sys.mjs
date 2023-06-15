/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * A file (disk) based persistent cache of a JSON serializable object.
 */
export class PersistentCache {
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
    return (
      this._cache ||
      // eslint-disable-next-line no-async-promise-executor
      (this._cache = new Promise(async (resolve, reject) => {
        let filepath;
        try {
          filepath = PathUtils.join(PathUtils.localProfileDir, this._filename);
        } catch (error) {
          reject(error);
          return;
        }

        let data = {};
        try {
          data = await IOUtils.readJSON(filepath);
        } catch (error) {
          if (
            // isInstance() is not available in node unit test. It should be safe to use instanceof as it's directly from IOUtils.
            // eslint-disable-next-line mozilla/use-isInstance
            !(error instanceof DOMException) ||
            error.name !== "NotFoundError"
          ) {
            console.error(`Failed to parse ${this._filename}:`, error.message);
          }
        }

        resolve(data);
      }))
    );
  }

  /**
   * Persist the cache to file.
   */
  async _persist(data) {
    const filepath = PathUtils.join(PathUtils.localProfileDir, this._filename);
    await IOUtils.writeJSON(filepath, data, {
      tmpPath: `${filepath}.tmp`,
    });
  }
}
