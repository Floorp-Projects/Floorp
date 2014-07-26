/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["GlobalState"];

const EXPORTED_METHODS = ["getState", "clear", "get", "set", "delete", "setFromState"];
/**
 * Module that contains global session data.
 */
function GlobalState() {
  let internal = new GlobalStateInternal();
  let external = {};
  for (let method of EXPORTED_METHODS) {
    external[method] = internal[method].bind(internal);
  }
  return Object.freeze(external);
}

function GlobalStateInternal() {
  // Storage for global state.
  this.state = {};
}

GlobalStateInternal.prototype = {
  /**
   * Get all value from the global state.
   */
  getState: function() {
    return this.state;
  },

  /**
   * Clear all currently stored global state.
   */
  clear: function() {
    this.state = {};
  },

  /**
   * Retrieve a value from the global state.
   *
   * @param aKey
   *        A key the value is stored under.
   * @return The value stored at aKey, or an empty string if no value is set.
   */
  get: function(aKey) {
    return this.state[aKey] || "";
  },

  /**
   * Set a global value.
   *
   * @param aKey
   *        A key to store the value under.
   */
  set: function(aKey, aStringValue) {
    this.state[aKey] = aStringValue;
  },

  /**
   * Delete a global value.
   *
   * @param aKey
   *        A key to delete the value for.
   */
  delete: function(aKey) {
    delete this.state[aKey];
  },

  /**
   * Set the current global state from a state object. Any previous global
   * state will be removed, even if the new state does not contain a matching
   * key.
   *
   * @param aState
   *        A state object to extract global state from to be set.
   */
  setFromState: function (aState) {
    this.state = (aState && aState.global) || {};
  }
};
