/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * WeakMapMap is a weakmap collection dual-keyed using an object and a string.
 * This is useful for keeping data compartmentalized e.g. grouped by tab.
 *
 * It's name comes from the internal structure which maps a WeakMap to a map,
 * which contains the target data.
 *
 * Usage:
 *   const myWeakMapMap = new WeakMapMap();
 *   const key = { randomObject: true };
 *   myWeakMapMap.set(key, "text1", "Some value1");
 *   myWeakMapMap.set(key, "text2", "Some value2");
 *   myWeakMapMap.get(key, "text1"); // Returns "Some value1"
 *   myWeakMapMap.get(key, "text2"); // Returns "Some value2"
 *   myWeakMapMap.has(key, "text1"); // Returns true
 *   myWeakMapMap.has(key, "notakey"); // Returns false
 */

"use strict";

class WeakMapMap {
  constructor() {
    this.clear();
  }

  /**
   * Returns the value associated to the key and nestedKey, or undefined if
   * there is none.
   *
   * @param {Object} key
   *        The key associated with the desired value.
   * @param {String} nestedKey
   *        The nested key associated with the desired value.
   */
  get(key, nestedKey) {
    if (!this.has(key, nestedKey)) {
      return undefined;
    }

    return this.store.get(key).get(nestedKey);
  }

  /**
   * Returns the value associated to the key and nestedKey, or undefined if
   * there is none.
   *
   * @param {Object} key
   *        The key associated with the desired value.
   * @param {String} nestedKey
   *        The nested key associated with the desired value.
   */
  has(key, nestedKey) {
    const hasKey = this.store.has(key);

    return hasKey && this.store.get(key).has(nestedKey);
  }

  /**
   *
   * @param {Object} key
   *        The key associated with the value.
   * @param {String} nestedKey
   *        The nested key associated with the value.
   * @param {any} value
   *        The value to add.
   */
  set(key, nestedKey, value) {
    if (!this.store.has(key)) {
      this.store.set(key, new Map());
    }

    const innerMap = this.store.get(key);
    innerMap.set(nestedKey, value);
  }

  /**
   * Removes the value associated to the key and nestedKey.
   *
   * @param {Object} key
   *        The key associated with the desired value.
   * @param {String} nestedKey
   *        The nested key associated with the desired value.
   *
   * @returns True if an element in the store has been removed successfully.
   *          False if the key is not found in the store.
   */
  delete(key, nestedKey) {
    if (!this.store.has(key)) {
      return false;
    }

    return this.store.get(key).delete(nestedKey);
  }

  /**
   * Clear the store.
   */
  clear() {
    this.store = new WeakMap();
  }
}

module.exports = WeakMapMap;
