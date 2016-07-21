/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const SOURCE_TOKEN = "<:>";

function LocationStore (store) {
  this._store = store || new Map();
}

/**
 * Method to get a promised location from the Store.
 * @param location
 * @returns Promise<Object>
 */
LocationStore.prototype.get = function (location) {
  this._safeAccessInit(location.url);
  return this._store.get(location.url).get(location);
};

/**
 * Method to set a promised location to the Store
 * @param location
 * @param promisedLocation
 */
LocationStore.prototype.set = function (location, promisedLocation = null) {
  this._safeAccessInit(location.url);
  this._store.get(location.url).set(serialize(location), promisedLocation);
};

/**
 * Utility method to verify if key exists in Store before accessing it.
 * If not, initializing it.
 * @param url
 * @private
 */
LocationStore.prototype._safeAccessInit = function (url) {
  if (!this._store.has(url)) {
    this._store.set(url, new Map());
  }
};

/**
 * Utility proxy method to Map.clear() method
 */
LocationStore.prototype.clear = function () {
  this._store.clear();
};

/**
 * Retrieves an object containing all locations to be resolved when `source-updated`
 * event is triggered.
 * @param url
 * @returns {Array<String>}
 */
LocationStore.prototype.getByURL = function (url){
  if (this._store.has(url)) {
    return [...this._store.get(url).keys()];
  }
  return [];
};

/**
 * Invalidates the stale location promises from the store when `source-updated`
 * event is triggered, and when FrameView unsubscribes from a location.
 * @param url
 */
LocationStore.prototype.clearByURL = function (url) {
  this._safeAccessInit(url);
  this._store.set(url, new Map());
};

exports.LocationStore = LocationStore;
exports.serialize = serialize;
exports.deserialize = deserialize;

/**
 * Utility method to serialize the source
 * @param source
 * @returns {string}
 */
function serialize(source) {
  let { url, line, column } = source;
  line = line || 0;
  column = column || 0;
  return `${url}${SOURCE_TOKEN}${line}${SOURCE_TOKEN}${column}`;
};

/**
 * Utility method to serialize the source
 * @param source
 * @returns Object
 */
function deserialize(source) {
  let [ url, line, column ] = source.split(SOURCE_TOKEN);
  line = parseInt(line);
  column = parseInt(column);
  if (column === 0) {
    return { url, line };
  }
  return { url, line, column };
};
