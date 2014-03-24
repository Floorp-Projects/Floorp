/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * A worker dedicated to handle I/O for Session Store.
 */

"use strict";

importScripts("resource://gre/modules/osfile.jsm");

let File = OS.File;
let Encoder = new TextEncoder();
let Decoder = new TextDecoder();

/**
 * Communications with the controller.
 *
 * Accepts messages:
 * {fun:function_name, args:array_of_arguments_or_null, id: custom_id}
 *
 * Sends messages:
 * {ok: result, id: custom_id, telemetry: {}} /
 * {fail: serialized_form_of_OS.File.Error, id: custom_id}
 */
self.onmessage = function (msg) {
  let data = msg.data;
  if (!(data.fun in Agent)) {
    throw new Error("Cannot find method " + data.fun);
  }

  let result;
  let id = data.id;

  try {
    result = Agent[data.fun].apply(Agent, data.args) || {};
  } catch (ex if ex instanceof OS.File.Error) {
    // Instances of OS.File.Error know how to serialize themselves
    // (deserialization ensures that we end up with OS-specific
    // instances of |OS.File.Error|)
    self.postMessage({fail: OS.File.Error.toMsg(ex), id: id});
    return;
  }

  // Other exceptions do not, and should be propagated through DOM's
  // built-in mechanism for uncaught errors, although this mechanism
  // may lose interesting information.
  self.postMessage({
    ok: result.result,
    id: id,
    telemetry: result.telemetry || {}
  });
};

let Agent = {
  // Boolean that tells whether we already made a
  // call to write(). We will only attempt to move
  // sessionstore.js to sessionstore.bak on the
  // first write.
  hasWrittenState: false,

  // The path to sessionstore.js
  path: OS.Path.join(OS.Constants.Path.profileDir, "sessionstore.js"),

  // The path to sessionstore.bak
  backupPath: OS.Path.join(OS.Constants.Path.profileDir, "sessionstore.bak"),

  /**
   * NO-OP to start the worker.
   */
  init: function () {
    return {result: true};
  },

  /**
   * Write the session to disk.
   */
  write: function (stateString) {
    let exn;
    let telemetry = {};

    if (!this.hasWrittenState) {
      try {
        let startMs = Date.now();
        File.move(this.path, this.backupPath);
        telemetry.FX_SESSION_RESTORE_BACKUP_FILE_MS = Date.now() - startMs;
      } catch (ex if isNoSuchFileEx(ex)) {
        // Ignore exceptions about non-existent files.
      } catch (ex) {
        // Throw the exception after we wrote the state to disk
        // so that the backup can't interfere with the actual write.
        exn = ex;
      }

      this.hasWrittenState = true;
    }

    let ret = this._write(stateString, telemetry);

    if (exn) {
      throw exn;
    }

    return ret;
  },

  /**
   * Extract all sorts of useful statistics from a state string,
   * for use with Telemetry.
   *
   * @return {object}
   */
  gatherTelemetry: function (stateString) {
    return Statistics.collect(stateString);
  },

  /**
   * Write a stateString to disk
   */
  _write: function (stateString, telemetry = {}) {
    let bytes = Encoder.encode(stateString);
    let startMs = Date.now();
    let result = File.writeAtomic(this.path, bytes, {tmpPath: this.path + ".tmp"});
    telemetry.FX_SESSION_RESTORE_WRITE_FILE_MS = Date.now() - startMs;
    telemetry.FX_SESSION_RESTORE_FILE_SIZE_BYTES = bytes.byteLength;
    return {result: result, telemetry: telemetry};
  },

  /**
   * Creates a copy of sessionstore.js.
   */
  createBackupCopy: function (ext) {
    try {
      return {result: File.copy(this.path, this.backupPath + ext)};
    } catch (ex if isNoSuchFileEx(ex)) {
      // Ignore exceptions about non-existent files.
      return {result: true};
    }
  },

  /**
   * Removes a backup copy.
   */
  removeBackupCopy: function (ext) {
    try {
      return {result: File.remove(this.backupPath + ext)};
    } catch (ex if isNoSuchFileEx(ex)) {
      // Ignore exceptions about non-existent files.
      return {result: true};
    }
  },

  /**
   * Wipes all files holding session data from disk.
   */
  wipe: function () {
    let exn;

    // Erase session state file
    try {
      File.remove(this.path);
    } catch (ex if isNoSuchFileEx(ex)) {
      // Ignore exceptions about non-existent files.
    } catch (ex) {
      // Don't stop immediately.
      exn = ex;
    }

    // Erase any backup, any file named "sessionstore.bak[-buildID]".
    let iter = new File.DirectoryIterator(OS.Constants.Path.profileDir);
    for (let entry in iter) {
      if (!entry.isDir && entry.path.startsWith(this.backupPath)) {
        try {
          File.remove(entry.path);
        } catch (ex) {
          // Don't stop immediately.
          exn = exn || ex;
        }
      }
    }

    if (exn) {
      throw exn;
    }

    return {result: true};
  }
};

function isNoSuchFileEx(aReason) {
  return aReason instanceof OS.File.Error && aReason.becauseNoSuchFile;
}

/**
 * Estimate the number of bytes that a data structure will use on disk
 * once serialized.
 */
function getByteLength(str) {
  return Encoder.encode(JSON.stringify(str)).byteLength;
}

/**
 * Tools for gathering statistics on a state string.
 */
let Statistics = {
  collect: function(stateString) {
    let start = Date.now();
    let TOTAL_PREFIX = "FX_SESSION_RESTORE_TOTAL_";
    let INDIVIDUAL_PREFIX = "FX_SESSION_RESTORE_INDIVIDUAL_";
    let SIZE_SUFFIX = "_SIZE_BYTES";

    let state = JSON.parse(stateString);

    // Gather all data
    let subsets = {};
    this.gatherSimpleData(state, subsets);
    this.gatherComplexData(state, subsets);

    // Extract telemetry
    let telemetry = {};
    for (let k of Object.keys(subsets)) {
      let obj = subsets[k];
      telemetry[TOTAL_PREFIX + k + SIZE_SUFFIX] = getByteLength(obj);

      if (Array.isArray(obj)) {
        let size = obj.map(getByteLength);
        telemetry[INDIVIDUAL_PREFIX + k + SIZE_SUFFIX] = size;
      }
    }

    let stop = Date.now();
    telemetry["FX_SESSION_RESTORE_EXTRACTING_STATISTICS_DURATION_MS"] = stop - start;
    return {
      telemetry: telemetry
    };
  },

  /**
   * Collect data that doesn't require a recursive walk through the
   * data structure.
   */
  gatherSimpleData: function(state, subsets) {
    // The subset of sessionstore.js dealing with open windows
    subsets.OPEN_WINDOWS = state.windows;

    // The subset of sessionstore.js dealing with closed windows
    subsets.CLOSED_WINDOWS = state._closedWindows;

    // The subset of sessionstore.js dealing with closed tabs
    // in open windows
    subsets.CLOSED_TABS_IN_OPEN_WINDOWS = [];

    // The subset of sessionstore.js dealing with cookies
    // in both open and closed windows
    subsets.COOKIES = [];

    for (let winData of state.windows) {
      let closedTabs = winData._closedTabs || [];
      subsets.CLOSED_TABS_IN_OPEN_WINDOWS.push(...closedTabs);

      let cookies = winData.cookies || [];
      subsets.COOKIES.push(...cookies);
    }

    for (let winData of state._closedWindows) {
      let cookies = winData.cookies || [];
      subsets.COOKIES.push(...cookies);
    }
  },

  /**
   * Walk through a data structure, recursively.
   *
   * @param {object} root The object from which to start walking.
   * @param {function(key, value)} cb Callback, called for each
   * item except the root. Returns |true| to walk the subtree rooted
   * at |value|, |false| otherwise   */
  walk: function(root, cb) {
    if (!root || typeof root !== "object") {
      return;
    }
    for (let k of Object.keys(root)) {
      let obj = root[k];
      let stepIn = cb(k, obj);
      if (stepIn) {
        this.walk(obj, cb);
      }
    }
  },

  /**
   * Collect data that requires walking through the data structure
   */
  gatherComplexData: function(state, subsets) {
    // The subset of sessionstore.js dealing with DOM storage
    subsets.DOM_STORAGE = [];
    // The subset of sessionstore.js storing form data
    subsets.FORMDATA = [];
    // The subset of sessionstore.js storing history
    subsets.HISTORY = [];


    this.walk(state, function(k, value) {
      let dest;
      switch (k) {
        case "entries":
          subsets.HISTORY.push(value);
          return true;
        case "storage":
          subsets.DOM_STORAGE.push(value);
          // Never visit storage, it's full of weird stuff
          return false;
        case "formdata":
          subsets.FORMDATA.push(value);
          // Never visit formdata, it's full of weird stuff
          return false;
        case "cookies": // Don't visit these places, they are full of weird stuff
        case "extData":
          return false;
        default:
          return true;
      }
    });

    return subsets;
  },

};
