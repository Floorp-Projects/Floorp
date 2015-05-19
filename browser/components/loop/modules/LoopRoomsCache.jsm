/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Task.jsm");
const {MozLoopService, LOOP_SESSION_TYPE} =
  Cu.import("resource:///modules/loop/MozLoopService.jsm", {});
XPCOMUtils.defineLazyModuleGetter(this, "CommonUtils",
                                  "resource://services-common/utils.js");
XPCOMUtils.defineLazyModuleGetter(this, "OS", "resource://gre/modules/osfile.jsm");

this.EXPORTED_SYMBOLS = ["LoopRoomsCache"];

const LOOP_ROOMS_CACHE_FILENAME = "loopRoomsCache.json";

/**
 * RoomsCache is a cache for saving simple rooms data to the disk in case we
 * need it for back-up purposes, e.g. recording room keys for FxA if the user
 * changes their password.
 *
 * The format of the data is:
 *
 * {
 *   <sessionType>: {
 *     <roomToken>: {
 *       "key": <roomKey>
 *     }
 *   }
 * }
 *
 * It is intended to try and keep the data forward and backwards compatible in
 * a reasonable manner, hence why the structure is more complex than it needs
 * to be to store tokens and keys.
 *
 * @param {Object} options The options for the RoomsCache, containing:
 *   - {String} baseDir   The base directory in which to save the file.
 *   - {String} filename  The filename for the cache file.
 */
function LoopRoomsCache(options) {
  options = options || {};

  this.baseDir = options.baseDir || OS.Constants.Path.profileDir;
  this.path = OS.Path.join(
    this.baseDir,
    options.filename || LOOP_ROOMS_CACHE_FILENAME
  );
  this._cache = null;
}

LoopRoomsCache.prototype = {
  /**
   * Updates the local copy of the cache and saves it to disk.
   *
   * @param  {Object} contents An object to be saved in json format.
   * @return {Promise} A promise that is resolved once the save is complete.
   */
  _setCache: function(contents) {
    this._cache = contents;

    return OS.File.makeDir(this.baseDir, {ignoreExisting: true}).then(() => {
        return CommonUtils.writeJSON(contents, this.path);
      });
  },

  /**
   * Returns the local copy of the cache if there is one, otherwise it reads
   * it from the disk.
   *
   * @return {Promise} A promise that is resolved once the read is complete.
   */
  _getCache: Task.async(function* () {
    if (this._cache) {
      return this._cache;
    }

    try {
      return (this._cache = yield CommonUtils.readJSON(this.path));
    } catch(error) {
      if (!error.becauseNoSuchFile) {
        MozLoopService.log.debug("Error reading the cache:", error);
      }
      return (this._cache = {});
    }
  }),

  /**
   * Function for testability purposes. Clears the cache.
   *
   * @return {Promise} A promise that is resolved once the clear is complete.
   */
  clear: function() {
    this._cache = null;
    return OS.File.remove(this.path);
  },

  /**
   * Gets a room key from the cache.
   *
   * @param {LOOP_SESSION_TYPE} sessionType  The session type for the room.
   * @param {String}            roomToken    The token for the room.
   * @return {Promise} A promise that is resolved when the data has been read
   *                   with the value of the key, or null if it isn't present.
   */
  getKey: Task.async(function* (sessionType, roomToken) {
    if (sessionType != LOOP_SESSION_TYPE.FXA) {
      return null;
    }

    let sessionData = (yield this._getCache())[sessionType];

    if (!sessionData || !sessionData[roomToken]) {
      return null;
    }
    return sessionData[roomToken].key;
  }),

  /**
   * Stores a room key into the cache. Note, if the key has not changed,
   * the store will not be re-written.
   *
   * @param {LOOP_SESSION_TYPE} sessionType  The session type for the room.
   * @param {String}            roomToken    The token for the room.
   * @param {String}            roomKey      The encryption key for the room.
   * @return {Promise} A promise that is resolved when the data has been stored.
   */
  setKey: Task.async(function* (sessionType, roomToken, roomKey) {
    if (sessionType != LOOP_SESSION_TYPE.FXA) {
      return;
    }

    let cache = yield this._getCache();

    // Create these objects if they don't exist.
    // We aim to do this creation and setting of the room key in a
    // forwards-compatible way so that if new fields are added to rooms later
    // then we don't mess them up (if there's no keys).
    if (!cache[sessionType]) {
      cache[sessionType] = {};
    }

    if (!cache[sessionType][roomToken]) {
      cache[sessionType][roomToken] = {};
    }

    // Only save it if there's no key, or it is different.
    if (!cache[sessionType][roomToken].key ||
        cache[sessionType][roomToken].key != roomKey) {
      cache[sessionType][roomToken].key = roomKey;
      return yield this._setCache(cache);
    }
  })
};
