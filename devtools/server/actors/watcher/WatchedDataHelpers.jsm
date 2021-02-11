/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Helper module alongside WatcherRegistry, which focus on updating the "watchedData" object.
 * This object is shared across processes and threads and have to be maintained in all these runtimes.
 */

var EXPORTED_SYMBOLS = ["WatchedDataHelpers"];

// List of all arrays stored in `watchedData`, which are replicated across processes and threads
const SUPPORTED_DATA = {
  BREAKPOINTS: "breakpoints",
  RESOURCES: "resources",
  TARGETS: "targets",
};

// Optional function, if data isn't a primitive data type in order to produce a key
// for the given data entry
const DATA_KEY_FUNCTION = {
  [SUPPORTED_DATA.BREAKPOINTS]: function({
    location: { sourceUrl, sourceId, line, column },
  }) {
    if (!sourceUrl && !sourceId) {
      throw new Error(
        `Breakpoints expect to have either a sourceUrl or a sourceId.`
      );
    }
    if (sourceUrl && typeof sourceUrl != "string") {
      throw new Error(
        `Breakpoints expect to have sourceUrl string, got ${typeof sourceUrl} instead.`
      );
    }
    // sourceId may be undefined for some sources keyed by URL
    if (sourceId && typeof sourceId != "string") {
      throw new Error(
        `Breakpoints expect to have sourceId string, got ${typeof sourceId} instead.`
      );
    }
    if (typeof line != "number") {
      throw new Error(
        `Breakpoints expect to have line number, got ${typeof line} instead.`
      );
    }
    if (typeof column != "number") {
      throw new Error(
        `Breakpoints expect to have column number, got ${typeof column} instead.`
      );
    }
    return `${sourceUrl}:${sourceId}:${line}:${column}`;
  },
};

function idFunction(v) {
  if (typeof v != "string") {
    throw new Error(
      `Expect data entry values to be string, or be using custom data key functions. Got ${typeof v} type instead.`
    );
  }
  return v;
}

const WatchedDataHelpers = {
  SUPPORTED_DATA,

  /**
   * Add new values to the shared "watchedData" object.
   *
   * @param Object watchedData
   *               The data object to update.
   * @param string type
   *               The type of data to be added
   * @param Array<Object> entries
   *               The values to be added to this type of data
   */
  addWatchedDataEntry(watchedData, type, entries) {
    const toBeAdded = [];
    const keyFunction = DATA_KEY_FUNCTION[type] || idFunction;
    for (const entry of entries) {
      const alreadyExists = watchedData[type].some(existingEntry => {
        return keyFunction(existingEntry) === keyFunction(entry);
      });
      if (!alreadyExists) {
        toBeAdded.push(entry);
      }
    }
    watchedData[type].push(...toBeAdded);
  },

  /**
   * Remove values from the shared "watchedData" object.
   *
   * @param Object watchedData
   *               The data object to update.
   * @param string type
   *               The type of data to be remove
   * @param Array<Object> entries
   *               The values to be removed from this type of data
   * @return Boolean
   *               True, if at least one entries existed and has been removed.
   *               False, if none of the entries existed and none has been removed.
   */
  removeWatchedDataEntry(watchedData, type, entries) {
    let includesAtLeastOne = false;
    const keyFunction = DATA_KEY_FUNCTION[type] || idFunction;
    for (const entry of entries) {
      const idx = watchedData[type].findIndex(existingEntry => {
        return keyFunction(existingEntry) === keyFunction(entry);
      });
      if (idx !== -1) {
        watchedData[type].splice(idx, 1);
        includesAtLeastOne = true;
      }
    }
    if (!includesAtLeastOne) {
      return false;
    }

    return true;
  },
};
