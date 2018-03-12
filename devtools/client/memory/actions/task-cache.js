/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { assert } = require("devtools/shared/DevToolsUtils");

/**
 * The `TaskCache` allows for re-using active tasks when spawning a second task
 * would simply duplicate work and is unnecessary. It maps from a task's unique
 * key to the promise of its result.
 */
const TaskCache = module.exports = class TaskCache {
  constructor() {
    this._cache = new Map();
  }

  /**
   * Get the promise keyed by the given unique `key`, if one exists.
   *
   * @param {Any} key
   * @returns {Promise<Any> | undefined}
   */
  get(key) {
    return this._cache.get(key);
  }

  /**
   * Put the task result promise in the cache and associate it with the given
   * `key` which must not already have an entry in the cache.
   *
   * @param {Any} key
   * @param {Promise<Any>} promise
   */
  put(key, promise) {
    assert(!this._cache.has(key),
           "We should not override extant entries");

    this._cache.set(key, promise);
  }

  /**
   * Remove the cache entry with the given key.
   *
   * @param {Any} key
   */
  remove(key) {
    assert(this._cache.has(key),
           `Should have an extant entry for key = ${key}`);

    this._cache.delete(key);
  }
};

/**
 * Create a new action-orchestrating task that is automatically cached. The
 * tasks themselves are responsible from removing themselves from the cache.
 *
 * @param {Function(...args) -> Any} getCacheKey
 * @param {Generator(...args) -> Any} task
 *
 * @returns Cacheable, Action-Creating Task
 */
TaskCache.declareCacheableTask = function ({ getCacheKey, task }) {
  const cache = new TaskCache();

  return function (...args) {
    return function* (dispatch, getState) {
      const key = getCacheKey(...args);

      const extantResult = cache.get(key);
      if (extantResult) {
        return extantResult;
      }

      // Ensure that we have our new entry in the cache *before* dispatching the
      // task!
      let resolve;
      cache.put(key, new Promise(r => {
        resolve = r;
      }));

      resolve(dispatch(function* () {
        try {
          args.push(() => cache.remove(key), dispatch, getState);
          return yield* task(...args);
        } catch (error) {
          // Don't perma-cache errors.
          if (cache.get(key)) {
            cache.remove(key);
          }
          throw error;
        }
      }));

      return yield cache.get(key);
    };
  };
};
