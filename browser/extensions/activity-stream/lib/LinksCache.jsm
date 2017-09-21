/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

this.EXPORTED_SYMBOLS = ["LinksCache"];

const EXPIRATION_TIME = 5 * 60 * 1000; // 5 minutes

/**
 * Cache link results from a provided object property and refresh after some
 * amount of time has passed. Allows for migrating data from previously cached
 * links to the new links with the same url.
 */
this.LinksCache = class LinksCache {

  /**
   * Create a links cache for a given object property.
   *
   * @param {object} linkObject Object containing the link property
   * @param {string} linkProperty Name of property on object to access
   * @param {function} migrator Optional callback receiving the old and new link
   *                            to allow custom migrating data from old to new.
   * @param {function} shouldRefresh Optional callback receiving the old and new
   *                                 options to refresh even when not expired.
   */
  constructor(linkObject, linkProperty, migrator = () => {}, shouldRefresh = () => {}) {
    this.clear();
    // Allow getting links from both methods and array properties
    this.linkGetter = options => {
      const ret = linkObject[linkProperty];
      return typeof ret === "function" ? ret.call(linkObject, options) : ret;
    };
    this.migrator = migrator;
    this.shouldRefresh = shouldRefresh;
  }

  /**
   * Clear the cached data.
   */
  clear() {
    this.cache = Promise.resolve([]);
    this.lastOptions = {};
    this.expire();
  }

  /**
   * Force the next request to update the cache.
   */
  expire() {
    delete this.lastUpdate;
  }

  /**
   * Request data and update the cache if necessary.
   *
   * @param {object} options Optional data to pass to the underlying method.
   * @returns {promise(array)} Links array with objects that can be modified.
   */
  async request(options = {}) {
    // Update the cache if the data has been expired
    const now = Date.now();
    if (this.lastUpdate === undefined ||
        now > this.lastUpdate + EXPIRATION_TIME ||
        // Allow custom rules around refreshing based on options
        this.shouldRefresh(this.lastOptions, options)) {
      // Update request state early so concurrent requests can refer to it
      this.lastOptions = options;
      this.lastUpdate = now;

      // Save a promise before awaits, so other requests wait for correct data
      this.cache = new Promise(async resolve => {
        // Allow fast lookup of old links by url that might need to migrate
        const toMigrate = new Map();
        for (const oldLink of await this.cache) {
          if (oldLink) {
            toMigrate.set(oldLink.url, oldLink);
          }
        }

        // Make a shallow copy of each resulting link to allow direct edits
        const copied = (await this.linkGetter(options)).map(link => link &&
          Object.assign({}, link));

        // Migrate data to the new link if we have an old link
        for (const newLink of copied) {
          if (newLink) {
            const oldLink = toMigrate.get(newLink.url);
            if (oldLink) {
              this.migrator(oldLink, newLink);
            }

            // Add a method that can be copied to cloned links that will update
            // the original cached link's property with the current one
            newLink.__updateCache = function(prop) {
              const val = this[prop];
              if (val === undefined) {
                delete newLink[prop];
              } else {
                newLink[prop] = val;
              }
            };
          }
        }

        // Update cache with the copied links migrated
        resolve(copied);
      });
    }

    // Return the promise of the links array
    return this.cache;
  }
};
