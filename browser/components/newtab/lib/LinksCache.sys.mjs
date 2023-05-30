/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This should be slightly less than SYSTEM_TICK_INTERVAL as timer
// comparisons are too exact while the async/await functionality will make the
// last recorded time a little bit later. This causes the comparasion to skip
// updates.
// It should be 10% less than SYSTEM_TICK to update at least once every 5 mins.
// https://github.com/mozilla/activity-stream/pull/3695#discussion_r144678214
const EXPIRATION_TIME = 4.5 * 60 * 1000; // 4.5 minutes

/**
 * Cache link results from a provided object property and refresh after some
 * amount of time has passed. Allows for migrating data from previously cached
 * links to the new links with the same url.
 */
export class LinksCache {
  /**
   * Create a links cache for a given object property.
   *
   * @param {object} linkObject Object containing the link property
   * @param {string} linkProperty Name of property on object to access
   * @param {array} properties Optional properties list to migrate to new links.
   * @param {function} shouldRefresh Optional callback receiving the old and new
   *                                 options to refresh even when not expired.
   */
  constructor(
    linkObject,
    linkProperty,
    properties = [],
    shouldRefresh = () => {}
  ) {
    this.clear();

    // Allow getting links from both methods and array properties
    this.linkGetter = options => {
      const ret = linkObject[linkProperty];
      return typeof ret === "function" ? ret.call(linkObject, options) : ret;
    };

    // Always migrate the shared cache data in addition to any custom properties
    this.migrateProperties = ["__sharedCache", ...properties];
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
    if (
      this.lastUpdate === undefined ||
      now > this.lastUpdate + EXPIRATION_TIME ||
      // Allow custom rules around refreshing based on options
      this.shouldRefresh(this.lastOptions, options)
    ) {
      // Update request state early so concurrent requests can refer to it
      this.lastOptions = options;
      this.lastUpdate = now;

      // Save a promise before awaits, so other requests wait for correct data
      // eslint-disable-next-line no-async-promise-executor
      this.cache = new Promise(async (resolve, reject) => {
        try {
          // Allow fast lookup of old links by url that might need to migrate
          const toMigrate = new Map();
          for (const oldLink of await this.cache) {
            if (oldLink) {
              toMigrate.set(oldLink.url, oldLink);
            }
          }

          // Update the cache with migrated links without modifying source objects
          resolve(
            (await this.linkGetter(options)).map(link => {
              // Keep original array hole positions
              if (!link) {
                return link;
              }

              // Migrate data to the new link copy if we have an old link
              const newLink = Object.assign({}, link);
              const oldLink = toMigrate.get(newLink.url);
              if (oldLink) {
                for (const property of this.migrateProperties) {
                  const oldValue = oldLink[property];
                  if (oldValue !== undefined) {
                    newLink[property] = oldValue;
                  }
                }
              } else {
                // Share data among link copies and new links from future requests
                newLink.__sharedCache = {};
              }
              // Provide a helper to update the cached link
              newLink.__sharedCache.updateLink = (property, value) => {
                newLink[property] = value;
              };

              return newLink;
            })
          );
        } catch (error) {
          reject(error);
        }
      });
    }

    // Provide a shallow copy of the cached link objects for callers to modify
    return (await this.cache).map(link => link && Object.assign({}, link));
  }
}
