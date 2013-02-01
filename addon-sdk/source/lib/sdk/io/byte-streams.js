/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim:set ts=2 sw=2 sts=2 et filetype=javascript
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

module.metadata = {
  "stability": "experimental"
};

exports.ByteReader = ByteReader;
exports.ByteWriter = ByteWriter;

const {Cc, Ci} = require("chrome");

// This just controls the maximum number of bytes we read in at one time.
const BUFFER_BYTE_LEN = 0x8000;

function ByteReader(inputStream) {
  const self = this;

  let stream = Cc["@mozilla.org/binaryinputstream;1"].
               createInstance(Ci.nsIBinaryInputStream);
  stream.setInputStream(inputStream);

  let manager = new StreamManager(this, stream);

  this.read = function ByteReader_read(numBytes) {
    manager.ensureOpened();
    if (typeof(numBytes) !== "number")
      numBytes = Infinity;

    let data = "";
    let read = 0;
    try {
      while (true) {
        let avail = stream.available();
        let toRead = Math.min(numBytes - read, avail, BUFFER_BYTE_LEN);
        if (toRead <= 0)
          break;
        data += stream.readBytes(toRead);
        read += toRead;
      }
    }
    catch (err) {
      throw new Error("Error reading from stream: " + err);
    }

    return data;
  };
}

function ByteWriter(outputStream) {
  const self = this;

  let stream = Cc["@mozilla.org/binaryoutputstream;1"].
               createInstance(Ci.nsIBinaryOutputStream);
  stream.setOutputStream(outputStream);

  let manager = new StreamManager(this, stream);

  this.write = function ByteWriter_write(str) {
    manager.ensureOpened();
    try {
      stream.writeBytes(str, str.length);
    }
    catch (err) {
      throw new Error("Error writing to stream: " + err);
    }
  };
}


// This manages the lifetime of stream, a ByteReader or ByteWriter.  It defines
// closed and close() on stream and registers an unload listener that closes
// rawStream if it's still opened.  It also provides ensureOpened(), which
// throws an exception if the stream is closed.
function StreamManager(stream, rawStream) {
  const self = this;
  this.rawStream = rawStream;
  this.opened = true;

  stream.__defineGetter__("closed", function stream_closed() {
    return !self.opened;
  });

  stream.close = function stream_close() {
    self.ensureOpened();
    self.unload();
  };

  require("../system/unload").ensure(this);
}

StreamManager.prototype = {
  ensureOpened: function StreamManager_ensureOpened() {
    if (!this.opened)
      throw new Error("The stream is closed and cannot be used.");
  },
  unload: function StreamManager_unload() {
    this.rawStream.close();
    this.opened = false;
  }
};
