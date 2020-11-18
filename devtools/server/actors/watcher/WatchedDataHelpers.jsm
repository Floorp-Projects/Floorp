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
  TARGETS: "targets",
  RESOURCES: "resources",
};

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
    for (const entry of entries) {
      if (watchedData[type].includes(entry)) {
        throw new Error(`'${type}:${entry} already exists`);
      }
    }

    for (const entry of entries) {
      watchedData[type].push(entry);
    }
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
   */
  removeWatchedDataEntry(watchedData, type, entries) {
    let includesAtLeastOne = false;
    for (const entry of entries) {
      const idx = watchedData[type].indexOf(entry);
      if (idx !== -1) {
        watchedData[type].splice(idx, 1);
        includesAtLeastOne = true;
      }
    }
    return includesAtLeastOne;
  },
};
