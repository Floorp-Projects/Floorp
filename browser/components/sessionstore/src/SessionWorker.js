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
 * {ok: result, id: custom_id} / {fail: serialized_form_of_OS.File.Error,
 *                                id: custom_id}
 */
self.onmessage = function (msg) {
  let data = msg.data;
  if (!(data.fun in Agent)) {
    throw new Error("Cannot find method " + data.fun);
  }

  let result;
  let id = data.id;

  try {
    result = Agent[data.fun].apply(Agent, data.args);
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
  self.postMessage({ok: result, id: id});
};

let Agent = {
  // The initial session string as read from disk.
  initialState: null,

  // Boolean that tells whether we already wrote
  // the loadState to disk once after startup.
  hasWrittenLoadStateOnce: false,

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
   * This method is only intended to be called by _SessionFile.syncRead() and
   * can be removed when we're not supporting synchronous SessionStore
   * initialization anymore. When sessionstore.js is read from disk
   * synchronously the state string must be supplied to the worker manually by
   * calling this method.
   */
  setInitialState: function (aState) {
    // _SessionFile.syncRead() should not be called after startup has finished.
    // Thus we also don't support any setInitialState() calls after we already
    // wrote the loadState to disk.
    if (this.hasWrittenLoadStateOnce) {
      throw new Error("writeLoadStateOnceAfterStartup() must only be called once.");
    }

    // Initial state might have been filled by read() already but yet we might
    // be called by _SessionFile.syncRead() before SessionStore.jsm had a chance
    // to call writeLoadStateOnceAfterStartup(). It's safe to ignore
    // setInitialState() calls if this happens.
    if (!this.initialState) {
      this.initialState = aState;
    }
  },

  /**
   * Read the session from disk.
   * In case sessionstore.js does not exist, attempt to read sessionstore.bak.
   */
  read: function () {
    for (let path of [this.path, this.backupPath]) {
      try {
        return this.initialState = Decoder.decode(File.read(path));
      } catch (ex if isNoSuchFileEx(ex)) {
        // Ignore exceptions about non-existent files.
      }
    }

    // No sessionstore data files found. Return an empty string.
    return "";
  },

  /**
   * Write the session to disk.
   */
  write: function (stateString, options) {
    if (!this.hasWrittenState) {
      if (options && options.backupOnFirstWrite) {
        try {
          File.move(this.path, this.backupPath);
        } catch (ex if isNoSuchFileEx(ex)) {
          // Ignore exceptions about non-existent files.
        }
      }

      this.hasWrittenState = true;
    }

    let bytes = Encoder.encode(stateString);
    return File.writeAtomic(this.path, bytes, {tmpPath: this.path + ".tmp"});
  },

  /**
   * Writes the session state to disk again but changes session.state to
   * 'running' before doing so. This is intended to be called only once, shortly
   * after startup so that we detect crashes on startup correctly.
   */
  writeLoadStateOnceAfterStartup: function (loadState) {
    if (this.hasWrittenLoadStateOnce) {
      throw new Error("writeLoadStateOnceAfterStartup() must only be called once.");
    }

    if (!this.initialState) {
      throw new Error("writeLoadStateOnceAfterStartup() must not be called " +
                      "without a valid session state or before it has been " +
                      "read from disk.");
    }

    // Make sure we can't call this function twice.
    this.hasWrittenLoadStateOnce = true;

    let state;
    try {
      state = JSON.parse(this.initialState);
    } finally {
      this.initialState = null;
    }

    state.session = state.session || {};
    state.session.state = loadState;
    let bytes = Encoder.encode(JSON.stringify(state));
    return File.writeAtomic(this.path, bytes, {tmpPath: this.path + ".tmp"});
  },

  /**
   * Creates a copy of sessionstore.js.
   */
  createBackupCopy: function (ext) {
    try {
      return File.copy(this.path, this.backupPath + ext);
    } catch (ex if isNoSuchFileEx(ex)) {
      // Ignore exceptions about non-existent files.
      return true;
    }
  },

  /**
   * Removes a backup copy.
   */
  removeBackupCopy: function (ext) {
    try {
      return File.remove(this.backupPath + ext);
    } catch (ex if isNoSuchFileEx(ex)) {
      // Ignore exceptions about non-existent files.
      return true;
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

    return true;
  }
};

function isNoSuchFileEx(aReason) {
  return aReason instanceof OS.File.Error && aReason.becauseNoSuchFile;
}
