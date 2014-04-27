/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

module.metadata = {
  "stability": "stable"
};

const { Cc, Ci, Cu } = require("chrome");
const file = require("./io/file");
const prefs = require("./preferences/service");
const jpSelf = require("./self");
const timer = require("./timers");
const unload = require("./system/unload");
const { emit, on, off } = require("./event/core");
const { defer } = require('./core/promise');

const { Task } = Cu.import("resource://gre/modules/Task.jsm", {});

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

function getHash(data) {
  let { promise, resolve } = defer();

  let crypto = Cc["@mozilla.org/security/hash;1"].createInstance(Ci.nsICryptoHash);
  crypto.init(crypto.MD5);

  let listener = {
    onStartRequest: function() { },

    onDataAvailable: function(request, context, inputStream, offset, count) {
      crypto.updateFromStream(inputStream, count);
    },

    onStopRequest: function(request, context, status) {
      resolve(crypto.finish(false));
    }
  };

  let converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"].
                  createInstance(Ci.nsIScriptableUnicodeConverter);
  converter.charset = "UTF-8";
  let stream = converter.convertToInputStream(data);
  let pump = Cc["@mozilla.org/network/input-stream-pump;1"].
             createInstance(Ci.nsIInputStreamPump);
  pump.init(stream, -1, -1, 0, 0, true);
  pump.asyncRead(listener, null);

  return promise;
}

function writeData(filename, data) {
  let { promise, resolve, reject } = defer();

  let stream = file.open(filename, "w");
  try {
    stream.writeAsync(data, err => {
      if (err)
        reject(err);
      else
        resolve();
    });
  }
  catch (err) {
    // writeAsync closes the stream after it's done, so only close on error.
    stream.close();
    reject(err);
  }

  return promise;
}

// A generic JSON store backed by a file on disk.  This should be isolated
// enough to move to its own module if need be...
function JsonStore(options) {
  this.filename = options.filename;
  this.quota = options.quota;
  this.writePeriod = options.writePeriod;
  this.onOverQuota = options.onOverQuota;
  this.onWrite = options.onWrite;
  this.hash = null;
  unload.ensure(this);
  this.startTimer();
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

  startTimer: function JsonStore_startTimer() {
    timer.setTimeout(() => {
      this.write().then(this.startTimer.bind(this));
    }, this.writePeriod);
  },

  // Removes the backing file and all empty subdirectories.
  purge: function JsonStore_purge() {
    try {
      // This'll throw if the file doesn't exist.
      file.remove(this.filename);
      this.hash = null;
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
      let self = this;
      getHash(str).then(hash => this.hash = hash);
    }
    catch (err) {
      this.root = {};
      this.hash = null;
    }
  },

  // Cleans up on unload.  If unloading because of uninstall, the store is
  // purged; otherwise it's written.
  unload: function JsonStore_unload(reason) {
    timer.clearTimeout(this.writeTimer);
    this.writeTimer = null;

    if (reason === "uninstall")
      this.purge();
    else
      this.write();
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
  write: Task.async(function JsonStore_write() {
    // Don't write if the root is uninitialized or if the store is empty and the
    // backing file doesn't yet exist.
    if (!this.isRootInited || (this._isEmpty && !file.exists(this.filename)))
      return;

    let data = JSON.stringify(this.root);

    // If the store is over quota, don't write.  The current under-quota state
    // should persist.
    if ((this.quota > 0) && (data.length > this.quota)) {
      this.onOverQuota(this);
      return;
    }

    // Hash the data to compare it to any previously written data
    let hash = yield getHash(data);

    if (hash == this.hash)
      return;

    // Finally, write.
    try {
      yield writeData(this.filename, data);

      this.hash = hash;
      if (this.onWrite)
        this.onWrite(this);
    }
    catch (err) {
      console.error("Error writing simple storage file: " + this.filename);
      console.error(err);
    }
  })
};


// This manages a JsonStore singleton and tailors its use to simple storage.
// The root of the JsonStore is lazy-loaded:  The backing file is only read the
// first time the root's gotten.
let manager = ({
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
