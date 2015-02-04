/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

module.metadata = {
  "stability": "experimental"
};

const { CC, Cc, Ci, Cu, Cr, components } = require("chrome");
const { EventTarget } = require("../event/target");
const { emit } = require("../event/core");
const { Buffer } = require("./buffer");
const { Class } = require("../core/heritage");
const { setTimeout } = require("../timers");


const MultiplexInputStream = CC("@mozilla.org/io/multiplex-input-stream;1",
                                "nsIMultiplexInputStream");
const AsyncStreamCopier = CC("@mozilla.org/network/async-stream-copier;1",
                             "nsIAsyncStreamCopier", "init");
const StringInputStream = CC("@mozilla.org/io/string-input-stream;1",
                             "nsIStringInputStream");
const ArrayBufferInputStream = CC("@mozilla.org/io/arraybuffer-input-stream;1",
                                  "nsIArrayBufferInputStream");

const BinaryInputStream = CC("@mozilla.org/binaryinputstream;1",
                             "nsIBinaryInputStream", "setInputStream");
const InputStreamPump = CC("@mozilla.org/network/input-stream-pump;1",
                           "nsIInputStreamPump", "init");

const threadManager = Cc["@mozilla.org/thread-manager;1"].
                      getService(Ci.nsIThreadManager);

const eventTarget = Cc["@mozilla.org/network/socket-transport-service;1"].
                    getService(Ci.nsIEventTarget);

let isFunction = value => typeof(value) === "function"

function accessor() {
  let map = new WeakMap();
  return function(target, value) {
    if (value)
      map.set(target, value);
    return map.get(target);
  }
}

const Stream = Class({
  extends: EventTarget,
  initialize: function() {
    this.readable = false;
    this.writable = false;
    this.encoding = null;
  },
  setEncoding: function setEncoding(encoding) {
    this.encoding = String(encoding).toUpperCase();
  },
  pipe: function pipe(target, options) {
    let source = this;
    function onData(chunk) {
      if (target.writable) {
        if (false === target.write(chunk))
          source.pause();
      }
    }
    function onDrain() {
      if (source.readable)
        source.resume();
    }
    function onEnd() {
      target.end();
    }
    function onPause() {
      source.pause();
    }
    function onResume() {
      if (source.readable)
        source.resume();
    }

    function cleanup() {
      source.removeListener("data", onData);
      target.removeListener("drain", onDrain);
      source.removeListener("end", onEnd);

      target.removeListener("pause", onPause);
      target.removeListener("resume", onResume);

      source.removeListener("end", cleanup);
      source.removeListener("close", cleanup);

      target.removeListener("end", cleanup);
      target.removeListener("close", cleanup);
    }

    if (!options || options.end !== false)
      target.on("end", onEnd);

    source.on("data", onData);
    target.on("drain", onDrain);
    target.on("resume", onResume);
    target.on("pause", onPause);

    source.on("end", cleanup);
    source.on("close", cleanup);

    target.on("end", cleanup);
    target.on("close", cleanup);

    emit(target, "pipe", source);
  },
  pause: function pause() {
    emit(this, "pause");
  },
  resume: function resume() {
    emit(this, "resume");
  },
  destroySoon: function destroySoon() {
    this.destroy();
  }
});
exports.Stream = Stream;


let nsIStreamListener = accessor();
let nsIInputStreamPump = accessor();
let nsIAsyncInputStream = accessor();
let nsIBinaryInputStream = accessor();

const StreamListener = Class({
  initialize: function(stream) {
    this.stream = stream;
  },

  // Next three methods are part of `nsIStreamListener` interface and are
  // invoked by `nsIInputStreamPump.asyncRead`.
  onDataAvailable: function(request, context, input, offset, count) {
    let stream = this.stream;
    let buffer = new ArrayBuffer(count);
    nsIBinaryInputStream(stream).readArrayBuffer(count, buffer);
    emit(stream, "data", new Buffer(buffer));
  },

  // Next two methods implement `nsIRequestObserver` interface and are invoked
  // by `nsIInputStreamPump.asyncRead`.
  onStartRequest: function() {},
  // Called to signify the end of an asynchronous request. We only care to
  // discover errors.
  onStopRequest: function(request, context, status) {
    let stream = this.stream;
    stream.readable = false;
    if (!components.isSuccessCode(status))
      emit(stream, "error", status);
    else
      emit(stream, "end");
  }
});


const InputStream = Class({
  extends: Stream,
  readable: false,
  paused: false,
  initialize: function initialize(options) {
    let { asyncInputStream } = options;

    this.readable = true;

    let binaryInputStream = new BinaryInputStream(asyncInputStream);
    let inputStreamPump = new InputStreamPump(asyncInputStream,
                                              -1, -1, 0, 0, false);
    let streamListener = new StreamListener(this);

    nsIAsyncInputStream(this, asyncInputStream);
    nsIInputStreamPump(this, inputStreamPump);
    nsIBinaryInputStream(this, binaryInputStream);
    nsIStreamListener(this, streamListener);

    this.asyncInputStream = asyncInputStream;
    this.inputStreamPump = inputStreamPump;
    this.binaryInputStream = binaryInputStream;
  },
  get status() nsIInputStreamPump(this).status,
  read: function() {
    nsIInputStreamPump(this).asyncRead(nsIStreamListener(this), null);
  },
  pause: function pause() {
    this.paused = true;
    nsIInputStreamPump(this).suspend();
    emit(this, "paused");
  },
  resume: function resume() {
    this.paused = false;
    if (nsIInputStreamPump(this).isPending()) {
        nsIInputStreamPump(this).resume();
        emit(this, "resume");
    }
  },
  close: function close() {
    this.readable = false;
    nsIInputStreamPump(this).cancel(Cr.NS_OK);
    nsIBinaryInputStream(this).close();
    nsIAsyncInputStream(this).close();
  },
  destroy: function destroy() {
    this.close();

    nsIInputStreamPump(this);
    nsIAsyncInputStream(this);
    nsIBinaryInputStream(this);
    nsIStreamListener(this);
  }
});
exports.InputStream = InputStream;



let nsIRequestObserver = accessor();
let nsIAsyncOutputStream = accessor();
let nsIAsyncStreamCopier = accessor();
let nsIMultiplexInputStream = accessor();

const RequestObserver = Class({
  initialize: function(stream) {
    this.stream = stream;
  },
  // Method is part of `nsIRequestObserver` interface that is
  // invoked by `nsIAsyncStreamCopier.asyncCopy`.
  onStartRequest: function() {},
  // Method is part of `nsIRequestObserver` interface that is
  // invoked by `nsIAsyncStreamCopier.asyncCopy`.
  onStopRequest: function(request, context, status) {
    let stream = this.stream;
    stream.drained = true;

    // Remove copied chunk.
    let multiplexInputStream = nsIMultiplexInputStream(stream);
    multiplexInputStream.removeStream(0);

    // If there was an error report.
    if (!components.isSuccessCode(status))
      emit(stream, "error", status);

    // If there more chunks in queue then flush them.
    else if (multiplexInputStream.count)
      stream.flush();

    // If stream is still writable notify that queue has drained.
    else if (stream.writable)
      emit(stream, "drain");

    // If stream is no longer writable close it.
    else {
      nsIAsyncStreamCopier(stream).cancel(Cr.NS_OK);
      nsIMultiplexInputStream(stream).close();
      nsIAsyncOutputStream(stream).close();
      nsIAsyncOutputStream(stream).flush();
    }
  }
});

const OutputStreamCallback = Class({
  initialize: function(stream) {
    this.stream = stream;
  },
  // Method is part of `nsIOutputStreamCallback` interface that
  // is invoked by `nsIAsyncOutputStream.asyncWait`. It is registered
  // with `WAIT_CLOSURE_ONLY` flag that overrides the default behavior,
  // causing the `onOutputStreamReady` notification to be suppressed until
  // the stream becomes closed.
  onOutputStreamReady: function(nsIAsyncOutputStream) {
    emit(this.stream, "finish");
  }
});

const OutputStream = Class({
  extends: Stream,
  writable: false,
  drained: true,
  get bufferSize() {
    let multiplexInputStream = nsIMultiplexInputStream(this);
    return multiplexInputStream && multiplexInputStream.available();
  },
  initialize: function initialize(options) {
    let { asyncOutputStream, output } = options;
    this.writable = true;

    // Ensure that `nsIAsyncOutputStream` was provided.
    asyncOutputStream.QueryInterface(Ci.nsIAsyncOutputStream);

    // Create a `nsIMultiplexInputStream` and `nsIAsyncStreamCopier`. Former
    // is used to queue written data chunks that `asyncStreamCopier` will
    // asynchronously drain into `asyncOutputStream`.
    let multiplexInputStream = MultiplexInputStream();
    let asyncStreamCopier = AsyncStreamCopier(multiplexInputStream,
                                              output || asyncOutputStream,
                                              eventTarget,
                                              // nsIMultiplexInputStream
                                              // implemnts .readSegments()
                                              true,
                                              // nsIOutputStream may or
                                              // may not implemnet
                                              // .writeSegments().
                                              false,
                                              // Use default buffer size.
                                              null,
                                              // Should not close an input.
                                              false,
                                              // Should not close an output.
                                              false);

    // Create `requestObserver` implementing `nsIRequestObserver` interface
    // in the constructor that's gonna be reused across several flushes.
    let requestObserver = RequestObserver(this);


    // Create observer that implements `nsIOutputStreamCallback` and register
    // using `WAIT_CLOSURE_ONLY` flag. That way it will be notfied once
    // `nsIAsyncOutputStream` is closed.
    asyncOutputStream.asyncWait(OutputStreamCallback(this),
                                asyncOutputStream.WAIT_CLOSURE_ONLY,
                                0,
                                threadManager.currentThread);

    nsIRequestObserver(this, requestObserver);
    nsIAsyncOutputStream(this, asyncOutputStream);
    nsIMultiplexInputStream(this, multiplexInputStream);
    nsIAsyncStreamCopier(this, asyncStreamCopier);

    this.asyncOutputStream = asyncOutputStream;
    this.multiplexInputStream = multiplexInputStream;
    this.asyncStreamCopier = asyncStreamCopier;
  },
  write: function write(content, encoding, callback) {
    if (isFunction(encoding)) {
      callback = encoding;
      encoding = callback;
    }

    // If stream is not writable we throw an error.
    if (!this.writable) throw Error("stream is not writable");

    let chunk = null;

    // If content is not a buffer then we create one out of it.
    if (Buffer.isBuffer(content)) {
      chunk = new ArrayBufferInputStream();
      chunk.setData(content.buffer, 0, content.length);
    }
    else {
      chunk = new StringInputStream();
      chunk.setData(content, content.length);
    }

    if (callback)
      this.once("drain", callback);

    // Queue up chunk to be copied to output sync.
    nsIMultiplexInputStream(this).appendStream(chunk);
    this.flush();

    return this.drained;
  },
  flush: function() {
    if (this.drained) {
      this.drained = false;
      nsIAsyncStreamCopier(this).asyncCopy(nsIRequestObserver(this), null);
    }
  },
  end: function end(content, encoding, callback) {
    if (isFunction(content)) {
      callback = content
      content = callback
    }
    if (isFunction(encoding)) {
      callback = encoding
      encoding = callback
    }

    // Setting a listener to "finish" event if passed.
    if (isFunction(callback))
      this.once("finish", callback);


    if (content)
      this.write(content, encoding);
    this.writable = false;

    // Close `asyncOutputStream` only if output has drained. If it's
    // not drained than `asyncStreamCopier` is busy writing, so let
    // it finish. Note that since `this.writable` is false copier will
    // close `asyncOutputStream` once output drains.
    if (this.drained)
      nsIAsyncOutputStream(this).close();
  },
  destroy: function destroy() {
    nsIAsyncOutputStream(this).close();
    nsIAsyncOutputStream(this);
    nsIMultiplexInputStream(this);
    nsIAsyncStreamCopier(this);
    nsIRequestObserver(this);
  }
});
exports.OutputStream = OutputStream;

const DuplexStream = Class({
  extends: Stream,
  implements: [InputStream, OutputStream],
  allowHalfOpen: true,
  initialize: function initialize(options) {
    options = options || {};
    let { readable, writable, allowHalfOpen } = options;

    InputStream.prototype.initialize.call(this, options);
    OutputStream.prototype.initialize.call(this, options);

    if (readable === false)
      this.readable = false;

    if (writable === false)
      this.writable = false;

    if (allowHalfOpen === false)
      this.allowHalfOpen = false;

    // If in a half open state and it's disabled enforce end.
    this.once("end", () => {
      if (!this.allowHalfOpen && (!this.readable || !this.writable))
        this.end();
    });
  },
  destroy: function destroy(error) {
    InputStream.prototype.destroy.call(this);
    OutputStream.prototype.destroy.call(this);
  }
});
exports.DuplexStream = DuplexStream;
