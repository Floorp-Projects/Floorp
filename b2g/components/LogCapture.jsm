/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* jshint moz: true */
/* global Uint8Array, Components, dump */

"use strict";

const Cu = Components.utils;
const Ci = Components.interfaces;
const Cc = Components.classes;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Promise", "resource://gre/modules/Promise.jsm");

this.EXPORTED_SYMBOLS = ["LogCapture"];

const SYSTEM_PROPERTY_KEY_MAX = 32;
const SYSTEM_PROPERTY_VALUE_MAX = 92;

function debug(msg) {
  dump("LogCapture.jsm: " + msg + "\n");
}

let LogCapture = {
  ensureLoaded: function() {
    if (!this.ctypes) {
      this.load();
    }
  },

  load: function() {
    // load in everything on first use
    Cu.import("resource://gre/modules/ctypes.jsm", this);

    this.libc = this.ctypes.open(this.ctypes.libraryName("c"));

    this.read = this.libc.declare("read",
      this.ctypes.default_abi,
      this.ctypes.int,       // bytes read (out)
      this.ctypes.int,       // file descriptor (in)
      this.ctypes.voidptr_t, // buffer to read into (in)
      this.ctypes.size_t     // size_t size of buffer (in)
    );

    this.open = this.libc.declare("open",
      this.ctypes.default_abi,
      this.ctypes.int,      // file descriptor (returned)
      this.ctypes.char.ptr, // path
      this.ctypes.int       // flags
    );

    this.close = this.libc.declare("close",
      this.ctypes.default_abi,
      this.ctypes.int, // error code (returned)
      this.ctypes.int  // file descriptor
    );

    this.getpid = this.libc.declare("getpid",
      this.ctypes.default_abi,
      this.ctypes.int  // PID
    );

    this.property_find_nth =
      this.libc.declare("__system_property_find_nth",
                        this.ctypes.default_abi,
                        this.ctypes.voidptr_t,     // return value: nullable prop_info*
                        this.ctypes.unsigned_int); // n: the index of the property to return

    this.property_read =
      this.libc.declare("__system_property_read",
                        this.ctypes.default_abi,
                        this.ctypes.void_t,     // return: none
                        this.ctypes.voidptr_t,  // non-null prop_info*
                        this.ctypes.char.ptr,   // key
                        this.ctypes.char.ptr);  // value

    this.key_buf   = this.ctypes.char.array(SYSTEM_PROPERTY_KEY_MAX)();
    this.value_buf = this.ctypes.char.array(SYSTEM_PROPERTY_VALUE_MAX)();
  },

  cleanup: function() {
    this.libc.close();

    this.read = null;
    this.open = null;
    this.close = null;
    this.property_find_nth = null;
    this.property_read = null;
    this.key_buf = null;
    this.value_buf = null;

    this.libc = null;
    this.ctypes = null;
  },

  /**
   * readLogFile
   * Read in /dev/log/{{log}} in nonblocking mode, which will return -1 if
   * reading would block the thread.
   *
   * @param log {String} The log from which to read. Must be present in /dev/log
   * @return {Uint8Array} Raw log data
   */
  readLogFile: function(logLocation) {
    this.ensureLoaded();

    const O_READONLY = 0;
    const O_NONBLOCK = 1 << 11;

    const BUF_SIZE = 2048;

    let BufType = this.ctypes.ArrayType(this.ctypes.char);
    let buf = new BufType(BUF_SIZE);
    let logArray = [];

    let logFd = this.open(logLocation, O_READONLY | O_NONBLOCK);
    if (logFd === -1) {
      return null;
    }

    let readStart = Date.now();
    let readCount = 0;
    while (true) {
      let count = this.read(logFd, buf, BUF_SIZE);
      readCount += 1;

      if (count <= 0) {
        // log has return due to being nonblocking or running out of things
        break;
      }
      for(let i = 0; i < count; i++) {
        logArray.push(buf[i]);
      }
    }

    let logTypedArray = new Uint8Array(logArray);

    this.close(logFd);

    return logTypedArray;
  },

 /**
   * Get all system properties as a dict with keys mapping to values
   */
  readProperties: function() {
    this.ensureLoaded();
    let n = 0;
    let propertyDict = {};

    while(true) {
      let prop_info = this.property_find_nth(n);
      if(prop_info.isNull()) {
        break;
      }

      // read the prop_info into the key and value buffers
      this.property_read(prop_info, this.key_buf, this.value_buf);
      let key = this.key_buf.readString();;
      let value = this.value_buf.readString()

      propertyDict[key] = value;
      n++;
    }

    return propertyDict;
  },

  /**
   * Dumping about:memory to a file in /data/local/tmp/, returning a Promise.
   * Will be resolved with the dumped file name.
   */
  readAboutMemory: function() {
    this.ensureLoaded();
    let deferred = Promise.defer();

    // Perform the dump
    let dumper = Cc["@mozilla.org/memory-info-dumper;1"]
                    .getService(Ci.nsIMemoryInfoDumper);

    let file = "/data/local/tmp/logshake-about_memory-" + this.getpid() + ".json.gz";
    dumper.dumpMemoryReportsToNamedFile(file, function() {
      deferred.resolve(file);
    }, null, false);

    return deferred.promise;
  }
};

this.LogCapture = LogCapture;
