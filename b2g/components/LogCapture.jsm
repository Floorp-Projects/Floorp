/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* jshint moz: true */
/* global Uint8Array, Components, dump */

'use strict';

this.EXPORTED_SYMBOLS = ['LogCapture'];

/**
 * readLogFile
 * Read in /dev/log/{{log}} in nonblocking mode, which will return -1 if
 * reading would block the thread.
 *
 * @param log {String} The log from which to read. Must be present in /dev/log
 * @return {Uint8Array} Raw log data
 */
let readLogFile = function(logLocation) {
  if (!this.ctypes) {
    // load in everything on first use
    Components.utils.import('resource://gre/modules/ctypes.jsm', this);

    this.lib = this.ctypes.open(this.ctypes.libraryName('c'));

    this.read = this.lib.declare('read',
      this.ctypes.default_abi,
      this.ctypes.int,       // bytes read (out)
      this.ctypes.int,       // file descriptor (in)
      this.ctypes.voidptr_t, // buffer to read into (in)
      this.ctypes.size_t     // size_t size of buffer (in)
    );

    this.open = this.lib.declare('open',
      this.ctypes.default_abi,
      this.ctypes.int,      // file descriptor (returned)
      this.ctypes.char.ptr, // path
      this.ctypes.int       // flags
    );

    this.close = this.lib.declare('close',
      this.ctypes.default_abi,
      this.ctypes.int, // error code (returned)
      this.ctypes.int  // file descriptor
    );
  }

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
};

let cleanup = function() {
  this.lib.close();
  this.read = this.open = this.close = null;
  this.lib = null;
  this.ctypes = null;
};

this.LogCapture = { readLogFile: readLogFile, cleanup: cleanup };
