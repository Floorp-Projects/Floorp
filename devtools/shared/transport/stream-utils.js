/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Ci, Cc, Cu, Cr, CC } = require("chrome");
const Services = require("Services");
const DevToolsUtils = require("devtools/shared/DevToolsUtils");
const { dumpv } = DevToolsUtils;
const EventEmitter = require("devtools/shared/event-emitter");
const promise = require("promise");
const defer = require("devtools/shared/defer");

DevToolsUtils.defineLazyGetter(this, "IOUtil", () => {
  return Cc["@mozilla.org/io-util;1"].getService(Ci.nsIIOUtil);
});

DevToolsUtils.defineLazyGetter(this, "ScriptableInputStream", () => {
  return CC("@mozilla.org/scriptableinputstream;1",
            "nsIScriptableInputStream", "init");
});

const BUFFER_SIZE = 0x8000;

/**
 * This helper function (and its companion object) are used by bulk senders and
 * receivers to read and write data in and out of other streams.  Functions that
 * make use of this tool are passed to callers when it is time to read or write
 * bulk data.  It is highly recommended to use these copier functions instead of
 * the stream directly because the copier enforces the agreed upon length.
 * Since bulk mode reuses an existing stream, the sender and receiver must write
 * and read exactly the agreed upon amount of data, or else the entire transport
 * will be left in a invalid state.  Additionally, other methods of stream
 * copying (such as NetUtil.asyncCopy) close the streams involved, which would
 * terminate the debugging transport, and so it is avoided here.
 *
 * Overall, this *works*, but clearly the optimal solution would be able to just
 * use the streams directly.  If it were possible to fully implement
 * nsIInputStream / nsIOutputStream in JS, wrapper streams could be created to
 * enforce the length and avoid closing, and consumers could use familiar stream
 * utilities like NetUtil.asyncCopy.
 *
 * The function takes two async streams and copies a precise number of bytes
 * from one to the other.  Copying begins immediately, but may complete at some
 * future time depending on data size.  Use the returned promise to know when
 * it's complete.
 *
 * @param input nsIAsyncInputStream
 *        The stream to copy from.
 * @param output nsIAsyncOutputStream
 *        The stream to copy to.
 * @param length Integer
 *        The amount of data that needs to be copied.
 * @return Promise
 *         The promise is resolved when copying completes or rejected if any
 *         (unexpected) errors occur.
 */
function copyStream(input, output, length) {
  let copier = new StreamCopier(input, output, length);
  return copier.copy();
}

function StreamCopier(input, output, length) {
  EventEmitter.decorate(this);
  this._id = StreamCopier._nextId++;
  this.input = input;
  // Save off the base output stream, since we know it's async as we've required
  this.baseAsyncOutput = output;
  if (IOUtil.outputStreamIsBuffered(output)) {
    this.output = output;
  } else {
    this.output = Cc["@mozilla.org/network/buffered-output-stream;1"].
                  createInstance(Ci.nsIBufferedOutputStream);
    this.output.init(output, BUFFER_SIZE);
  }
  this._length = length;
  this._amountLeft = length;
  this._deferred = defer();

  this._copy = this._copy.bind(this);
  this._flush = this._flush.bind(this);
  this._destroy = this._destroy.bind(this);

  // Copy promise's then method up to this object.
  // Allows the copier to offer a promise interface for the simple succeed or
  // fail scenarios, but also emit events (due to the EventEmitter) for other
  // states, like progress.
  this.then = this._deferred.promise.then.bind(this._deferred.promise);
  this.then(this._destroy, this._destroy);

  // Stream ready callback starts as |_copy|, but may switch to |_flush| at end
  // if flushing would block the output stream.
  this._streamReadyCallback = this._copy;
}
StreamCopier._nextId = 0;

StreamCopier.prototype = {

  copy: function () {
    // Dispatch to the next tick so that it's possible to attach a progress
    // event listener, even for extremely fast copies (like when testing).
    Services.tm.currentThread.dispatch(() => {
      try {
        this._copy();
      } catch (e) {
        this._deferred.reject(e);
      }
    }, 0);
    return this;
  },

  _copy: function () {
    let bytesAvailable = this.input.available();
    let amountToCopy = Math.min(bytesAvailable, this._amountLeft);
    this._debug("Trying to copy: " + amountToCopy);

    let bytesCopied;
    try {
      bytesCopied = this.output.writeFrom(this.input, amountToCopy);
    } catch (e) {
      if (e.result == Cr.NS_BASE_STREAM_WOULD_BLOCK) {
        this._debug("Base stream would block, will retry");
        this._debug("Waiting for output stream");
        this.baseAsyncOutput.asyncWait(this, 0, 0, Services.tm.currentThread);
        return;
      } else {
        throw e;
      }
    }

    this._amountLeft -= bytesCopied;
    this._debug("Copied: " + bytesCopied +
                ", Left: " + this._amountLeft);
    this._emitProgress();

    if (this._amountLeft === 0) {
      this._debug("Copy done!");
      this._flush();
      return;
    }

    this._debug("Waiting for input stream");
    this.input.asyncWait(this, 0, 0, Services.tm.currentThread);
  },

  _emitProgress: function () {
    this.emit("progress", {
      bytesSent: this._length - this._amountLeft,
      totalBytes: this._length
    });
  },

  _flush: function () {
    try {
      this.output.flush();
    } catch (e) {
      if (e.result == Cr.NS_BASE_STREAM_WOULD_BLOCK ||
          e.result == Cr.NS_ERROR_FAILURE) {
        this._debug("Flush would block, will retry");
        this._streamReadyCallback = this._flush;
        this._debug("Waiting for output stream");
        this.baseAsyncOutput.asyncWait(this, 0, 0, Services.tm.currentThread);
        return;
      } else {
        throw e;
      }
    }
    this._deferred.resolve();
  },

  _destroy: function () {
    this._destroy = null;
    this._copy = null;
    this._flush = null;
    this.input = null;
    this.output = null;
  },

  // nsIInputStreamCallback
  onInputStreamReady: function () {
    this._streamReadyCallback();
  },

  // nsIOutputStreamCallback
  onOutputStreamReady: function () {
    this._streamReadyCallback();
  },

  _debug: function (msg) {
    // Prefix logs with the copier ID, which makes logs much easier to
    // understand when several copiers are running simultaneously
    dumpv("Copier: " + this._id + " " + msg);
  }

};

/**
 * Read from a stream, one byte at a time, up to the next |delimiter|
 * character, but stopping if we've read |count| without finding it.  Reading
 * also terminates early if there are less than |count| bytes available on the
 * stream.  In that case, we only read as many bytes as the stream currently has
 * to offer.
 * TODO: This implementation could be removed if bug 984651 is fixed, which
 *       provides a native version of the same idea.
 * @param stream nsIInputStream
 *        The input stream to read from.
 * @param delimiter string
 *        The character we're trying to find.
 * @param count integer
 *        The max number of characters to read while searching.
 * @return string
 *         The data collected.  If the delimiter was found, this string will
 *         end with it.
 */
function delimitedRead(stream, delimiter, count) {
  dumpv("Starting delimited read for " + delimiter + " up to " +
        count + " bytes");

  let scriptableStream;
  if (stream instanceof Ci.nsIScriptableInputStream) {
    scriptableStream = stream;
  } else {
    scriptableStream = new ScriptableInputStream(stream);
  }

  let data = "";

  // Don't exceed what's available on the stream
  count = Math.min(count, stream.available());

  if (count <= 0) {
    return data;
  }

  let char;
  while (char !== delimiter && count > 0) {
    char = scriptableStream.readBytes(1);
    count--;
    data += char;
  }

  return data;
}

module.exports = {
  copyStream: copyStream,
  delimitedRead: delimitedRead
};
