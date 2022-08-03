/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Helper module alongside WatcherRegistry, which focus on updating the "sessionData" object.
 * This object is shared across processes and threads and have to be maintained in all these runtimes.
 */

var EXPORTED_SYMBOLS = ["SessionDataHelpers"];

const lazy = {};

if (typeof module == "object") {
  // Allow this JSM to also be loaded as a CommonJS module
  // Because this module is used from the worker thread,
  // (via target-actor-mixin), and workers can't load JSMs via ChromeUtils.import.
  loader.lazyRequireGetter(
    lazy,
    "validateBreakpointLocation",
    "devtools/shared/validate-breakpoint.jsm",
    true
  );

  loader.lazyRequireGetter(
    lazy,
    "validateEventBreakpoint",
    "devtools/server/actors/utils/event-breakpoints",
    true
  );
} else {
  const { XPCOMUtils } = ChromeUtils.importESModule(
    "resource://gre/modules/XPCOMUtils.sys.mjs"
  );
  // Ignore the "duplicate" definitions here as this are also defined
  // in the if block above.
  // eslint-disable-next-line mozilla/valid-lazy
  XPCOMUtils.defineLazyGetter(lazy, "validateBreakpointLocation", () => {
    return ChromeUtils.import(
      "resource://devtools/shared/validate-breakpoint.jsm"
    ).validateBreakpointLocation;
  });
  // eslint-disable-next-line mozilla/valid-lazy
  XPCOMUtils.defineLazyGetter(lazy, "validateEventBreakpoint", () => {
    const { loader } = ChromeUtils.import(
      "resource://devtools/shared/loader/Loader.jsm"
    );
    return loader.require("devtools/server/actors/utils/event-breakpoints")
      .validateEventBreakpoint;
  });
}

// List of all arrays stored in `sessionData`, which are replicated across processes and threads
const SUPPORTED_DATA = {
  BLACKBOXING: "blackboxing",
  BREAKPOINTS: "breakpoints",
  XHR_BREAKPOINTS: "xhr-breakpoints",
  EVENT_BREAKPOINTS: "event-breakpoints",
  RESOURCES: "resources",
  TARGET_CONFIGURATION: "target-configuration",
  THREAD_CONFIGURATION: "thread-configuration",
  TARGETS: "targets",
};

// Optional function, if data isn't a primitive data type in order to produce a key
// for the given data entry
const DATA_KEY_FUNCTION = {
  [SUPPORTED_DATA.BLACKBOXING]({ url, range }) {
    return (
      url +
      (range
        ? `:${range.start.line}:${range.start.column}-${range.end.line}:${range.end.column}`
        : "")
    );
  },
  [SUPPORTED_DATA.BREAKPOINTS]({ location }) {
    lazy.validateBreakpointLocation(location);
    const { sourceUrl, sourceId, line, column } = location;
    return `${sourceUrl}:${sourceId}:${line}:${column}`;
  },
  [SUPPORTED_DATA.TARGET_CONFIGURATION]({ key }) {
    // Configuration data entries are { key, value } objects, `key` can be used
    // as the unique identifier for the entry.
    return key;
  },
  [SUPPORTED_DATA.THREAD_CONFIGURATION]({ key }) {
    // See target configuration comment
    return key;
  },
  [SUPPORTED_DATA.XHR_BREAKPOINTS]({ path, method }) {
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
  [SUPPORTED_DATA.EVENT_BREAKPOINTS](id) {
    if (typeof id != "string") {
      throw new Error(
        `Event Breakpoints expect the id to be a string , got ${typeof id} instead.`
      );
    }
    if (!lazy.validateEventBreakpoint(id)) {
      throw new Error(
        `The id string should be a valid event breakpoint id, ${id} is not.`
      );
    }
    return id;
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

const SessionDataHelpers = {
  SUPPORTED_DATA,

  /**
   * Add new values to the shared "sessionData" object.
   *
   * @param Object sessionData
   *               The data object to update.
   * @param string type
   *               The type of data to be added
   * @param Array<Object> entries
   *               The values to be added to this type of data
   */
  addSessionDataEntry(sessionData, type, entries) {
    const toBeAdded = [];
    const keyFunction = DATA_KEY_FUNCTION[type] || idFunction;
    for (const entry of entries) {
      if (!sessionData[type]) {
        sessionData[type] = [];
      }
      const existingIndex = sessionData[type].findIndex(existingEntry => {
        return keyFunction(existingEntry) === keyFunction(entry);
      });
      if (existingIndex === -1) {
        // New entry.
        toBeAdded.push(entry);
      } else {
        // Existing entry, update the value. This is relevant if the data-entry
        // is not a primitive data-type, and the value can change for the same
        // key.
        sessionData[type][existingIndex] = entry;
      }
    }
    sessionData[type].push(...toBeAdded);
  },

  /**
   * Remove values from the shared "sessionData" object.
   *
   * @param Object sessionData
   *               The data object to update.
   * @param string type
   *               The type of data to be remove
   * @param Array<Object> entries
   *               The values to be removed from this type of data
   * @return Boolean
   *               True, if at least one entries existed and has been removed.
   *               False, if none of the entries existed and none has been removed.
   */
  removeSessionDataEntry(sessionData, type, entries) {
    let includesAtLeastOne = false;
    const keyFunction = DATA_KEY_FUNCTION[type] || idFunction;
    for (const entry of entries) {
      const idx = sessionData[type]
        ? sessionData[type].findIndex(existingEntry => {
            return keyFunction(existingEntry) === keyFunction(entry);
          })
        : -1;
      if (idx !== -1) {
        sessionData[type].splice(idx, 1);
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
  module.exports.SessionDataHelpers = SessionDataHelpers;
}
