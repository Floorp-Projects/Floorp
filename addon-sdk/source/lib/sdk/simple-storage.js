/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

module.metadata = {
  "stability": "stable"
};

const { Cc, Ci } = require("chrome");
const file = require("./io/file");
const prefs = require("./preferences/service");
const jpSelf = require("./self");
const timer = require("./timers");
const unload = require("./system/unload");
const { emit, on, off } = require("./event/core");

const WRITE_PERIOD_PREF = "extensions.addon-sdk.simple-storage.writePeriod";
const WRITE_PERIOD_DEFAULT = 300000; // 5 minutes

const QUOTA_PREF = "extensions.addon-sdk.simple-storage.quota";
const QUOTA_DEFAULT = 5242880; // 5 MiB

const JETPACK_DIR_BASENAME = "jetpack";

Object.defineProperties(exports, {
  storage: {
    enumerable: true,
    get: function() { return manager.root; },
    set: function(value) { manager.root = value; }
  },
  quotaUsage: {
    get: function() { return manager.quotaUsage; }
  }
});

// A generic JSON store backed by a file on disk.  This should be isolated
// enough to move to its own module if need be...
function JsonStore(options) {
  this.filename = options.filename;
  this.quota = options.quota;
  this.writePeriod = options.writePeriod;
  this.onOverQuota = options.onOverQuota;
  this.onWrite = options.onWrite;

  unload.ensure(this);

  this.writeTimer = timer.setInterval(this.write.bind(this),
                                      this.writePeriod);
}

JsonStore.prototype = {
  // The store's root.
  get root() {
    return this.isRootInited ? this._root : {};
  },

  // Performs some type checking.
  set root(val) {
    let types = ["array", "boolean", "null", "number", "object", "string"];
    if (types.indexOf(typeof(val)) < 0) {
      throw new Error("storage must be one of the following types: " +
                      types.join(", "));
    }
    this._root = val;
    return val;
  },

  // True if the root has ever been set (either via the root setter or by the
  // backing file's having been read).
  get isRootInited() {
    return this._root !== undefined;
  },

  // Percentage of quota used, as a number [0, Inf).  > 1 implies over quota.
  // Undefined if there is no quota.
  get quotaUsage() {
    return this.quota > 0 ?
           JSON.stringify(this.root).length / this.quota :
           undefined;
  },

  // Removes the backing file and all empty subdirectories.
  purge: function JsonStore_purge() {
    try {
      // This'll throw if the file doesn't exist.
      file.remove(this.filename);
      let parentPath = this.filename;
      do {
        parentPath = file.dirname(parentPath);
        // This'll throw if the dir isn't empty.
        file.rmdir(parentPath);
      } while (file.basename(parentPath) !== JETPACK_DIR_BASENAME);
    }
    catch (err) {}
  },

  // Initializes the root by reading the backing file.
  read: function JsonStore_read() {
    try {
      let str = file.read(this.filename);

      // Ideally we'd log the parse error with console.error(), but logged
      // errors cause tests to fail.  Supporting "known" errors in the test
      // harness appears to be non-trivial.  Maybe later.
      this.root = JSON.parse(str);
    }
    catch (err) {
      this.root = {};
    }
  },

  // If the store is under quota, writes the root to the backing file.
  // Otherwise quota observers are notified and nothing is written.
  write: function JsonStore_write() {
    if (this.quotaUsage > 1)
      this.onOverQuota(this);
    else
      this._write();
  },

  // Cleans up on unload.  If unloading because of uninstall, the store is
  // purged; otherwise it's written.
  unload: function JsonStore_unload(reason) {
    timer.clearInterval(this.writeTimer);
    this.writeTimer = null;

    if (reason === "uninstall")
      this.purge();
    else
      this._write();
  },

  // True if the root is an empty object.
  get _isEmpty() {
    if (this.root && typeof(this.root) === "object") {
      let empty = true;
      for (let key in this.root) {
        empty = false;
        break;
      }
      return empty;
    }
    return false;
  },

  // Writes the root to the backing file, notifying write observers when
  // complete.  If the store is over quota or if it's empty and the store has
  // never been written, nothing is written and write observers aren't notified.
  _write: function JsonStore__write() {
    // Don't write if the root is uninitialized or if the store is empty and the
    // backing file doesn't yet exist.
    if (!this.isRootInited || (this._isEmpty && !file.exists(this.filename)))
      return;

    // If the store is over quota, don't write.  The current under-quota state
    // should persist.
    if (this.quotaUsage > 1)
      return;

    // Finally, write.
    let stream = file.open(this.filename, "w");
    try {
      stream.writeAsync(JSON.stringify(this.root), function writeAsync(err) {
        if (err)
          console.error("Error writing simple storage file: " + this.filename);
        else if (this.onWrite)
          this.onWrite(this);
      }.bind(this));
    }
    catch (err) {
      // writeAsync closes the stream after it's done, so only close on error.
      stream.close();
    }
  }
};


// This manages a JsonStore singleton and tailors its use to simple storage.
// The root of the JsonStore is lazy-loaded:  The backing file is only read the
// first time the root's gotten.
var manager = ({
  jsonStore: null,

  // The filename of the store, based on the profile dir and extension ID.
  get filename() {
    let storeFile = Cc["@mozilla.org/file/directory_service;1"].
                    getService(Ci.nsIProperties).
                    get("ProfD", Ci.nsIFile);
    storeFile.append(JETPACK_DIR_BASENAME);
    storeFile.append(jpSelf.id);
    storeFile.append("simple-storage");
    file.mkpath(storeFile.path);
    storeFile.append("store.json");
    return storeFile.path;
  },

  get quotaUsage() {
    return this.jsonStore.quotaUsage;
  },

  get root() {
    if (!this.jsonStore.isRootInited)
      this.jsonStore.read();
    return this.jsonStore.root;
  },

  set root(val) {
    return this.jsonStore.root = val;
  },

  unload: function manager_unload() {
    off(this);
  },

  new: function manager_constructor() {
    let manager = Object.create(this);
    unload.ensure(manager);

    manager.jsonStore = new JsonStore({
      filename: manager.filename,
      writePeriod: prefs.get(WRITE_PERIOD_PREF, WRITE_PERIOD_DEFAULT),
      quota: prefs.get(QUOTA_PREF, QUOTA_DEFAULT),
      onOverQuota: emit.bind(null, exports, "OverQuota")
    });

    return manager;
  }
}).new();

exports.on = on.bind(null, exports);
exports.removeListener = function(type, listener) {
  off(exports, type, listener);
};
