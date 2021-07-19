/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Helper module alongside WatcherRegistry, which focus on updating the "watchedData" object.
 * This object is shared across processes and threads and have to be maintained in all these runtimes.
 */

var EXPORTED_SYMBOLS = ["WatchedDataHelpers"];

// Allow this JSM to also be loaded as a CommonJS module
// Because this module is used from the worker thread,
// (via target-actor-mixin), and workers can't load JSMs via ChromeUtils.import.
const { validateBreakpointLocation } =
  typeof module == "object"
    ? require("devtools/shared/validate-breakpoint.jsm")
    : ChromeUtils.import("resource://devtools/shared/validate-breakpoint.jsm");

// List of all arrays stored in `watchedData`, which are replicated across processes and threads
const SUPPORTED_DATA = {
  BREAKPOINTS: "breakpoints",
  XHR_BREAKPOINTS: "xhr-breakpoints",
  RESOURCES: "resources",
  TARGET_CONFIGURATION: "target-configuration",
  THREAD_CONFIGURATION: "thread-configuration",
  TARGETS: "targets",
};

// Optional function, if data isn't a primitive data type in order to produce a key
// for the given data entry
const DATA_KEY_FUNCTION = {
  [SUPPORTED_DATA.BREAKPOINTS]: function({ location }) {
    validateBreakpointLocation(location);
    const { sourceUrl, sourceId, line, column } = location;
    return `${sourceUrl}:${sourceId}:${line}:${column}`;
  },
  [SUPPORTED_DATA.TARGET_CONFIGURATION]: function({ key }) {
    // Configuration data entries are { key, value } objects, `key` can be used
    // as the unique identifier for the entry.
    return key;
  },
  [SUPPORTED_DATA.THREAD_CONFIGURATION]: function({ key }) {
    // See target configuration comment
    return key;
  },
  [SUPPORTED_DATA.XHR_BREAKPOINTS]: function({ path, method }) {
    if (typeof path != "string") {
      throw new Error(
        `XHR Breakpoints expect to have path string, got ${typeof path} instead.`
      );
    }
    if (typeof method != "string") {
      throw new Error(
        `XHR Breakpoints expect to have method string, got ${typeof method} instead.`
      );
    }
    return `${path}:${method}`;
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
      const existingIndex = watchedData[type].findIndex(existingEntry => {
        return keyFunction(existingEntry) === keyFunction(entry);
      });
      if (existingIndex === -1) {
        // New entry.
        toBeAdded.push(entry);
      } else {
        // Existing entry, update the value. This is relevant if the data-entry
        // is not a primitive data-type, and the value can change for the same
        // key.
        watchedData[type][existingIndex] = entry;
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

// Allow this JSM to also be loaded as a CommonJS module
// Because this module is used from the worker thread,
// (via target-actor-mixin), and workers can't load JSMs.
if (typeof module == "object") {
  module.exports.WatchedDataHelpers = WatchedDataHelpers;
}
