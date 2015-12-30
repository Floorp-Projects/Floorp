/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;


var subscriptLoader = Cc["@mozilla.org/moz/jssubscript-loader;1"]
                        .getService(Ci.mozIJSSubScriptLoader);

/**
 * Start a new RIL worker.
 *
 * @param custom_ns
 *        Namespace with symbols to be injected into the new worker
 *        namespace.
 *
 * @return an object that represents the worker's namespace.
 *
 * @note that this does not start an actual worker thread. The worker
 * is executed on the main thread, within a separate namespace object.
 */
function newWorker(custom_ns) {
  let worker_ns = {
    importScripts: function() {
      Array.slice(arguments).forEach(function(script) {
        if (!script.startsWith("resource:")) {
          script = "resource://gre/modules/" + script;
        }
        subscriptLoader.loadSubScript(script, this);
      }, this);
    },

    postRILMessage: function(message) {
    },

    postMessage: function(message) {
    },

    // Define these variables inside the worker scope so ES5 strict mode
    // doesn't flip out.
    onmessage: undefined,
    onerror: undefined,

    DEBUG: true
  };
  // The 'self' variable in a worker points to the worker's own namespace.
  worker_ns.self = worker_ns;

  // Copy the custom definitions over.
  for (let key in custom_ns) {
    worker_ns[key] = custom_ns[key];
  }

  // fake require() for toolkit/components/workerloader/require.js
  let require = (function() {
    return function require(script) {
      worker_ns.module = {};
      worker_ns.importScripts(script);
      return worker_ns;
    }
  })();

  Object.freeze(require);
  Object.defineProperty(worker_ns, "require", {
    value: require,
    enumerable: true,
    configurable: false
  });

  // Load the RIL worker itself.
  worker_ns.importScripts("ril_worker.js");

  // Register at least one client.
  worker_ns.ContextPool.registerClient({ clientId: 0 });

  return worker_ns;
}

/**
 * Create a buffered RIL worker.
 *
 * @return A worker object that stores sending octets in a internal buffer.
 */
function newUint8Worker() {
  let worker = newWorker();
  let index = 0; // index for read
  let buf = [];

  let context = worker.ContextPool._contexts[0];
  context.Buf.writeUint8 = function(value) {
    buf.push(value);
  };

  context.Buf.readUint8 = function() {
    return buf[index++];
  };

  context.Buf.seekIncoming = function(offset) {
    index += offset;
  };

  context.Buf.getReadAvailable = function() {
    return buf.length - index;
  };

  worker.debug = do_print;

  return worker;
}

/**
 * Create a worker that keeps posted chrome message.
 */
function newInterceptWorker() {
  let postedMessage;
  let worker = newWorker({
    postRILMessage: function(data) {
    },
    postMessage: function(message) {
      postedMessage = message;
    }
  });
  return {
    get postedMessage() {
      return postedMessage;
    },
    get worker() {
      return worker;
    }
  };
}

/**
 * Create a parcel suitable for postRILMessage().
 *
 * @param fakeParcelSize
 *        Value to be written to parcel size field for testing
 *        incorrect/incomplete parcel reading. Replaced with correct
 *        one determined length of data if negative.
 * @param response
 *        Response code of the incoming parcel.
 * @param request
 *        Request code of the incoming parcel.
 * @param data
 *        Extra data to be appended.
 *
 * @return an Uint8Array carrying all parcel data.
 */
function newIncomingParcel(fakeParcelSize, response, request, data) {
  const UINT32_SIZE = 4;
  const PARCEL_SIZE_SIZE = 4;

  let realParcelSize = data.length + 2 * UINT32_SIZE;
  let buffer = new ArrayBuffer(realParcelSize + PARCEL_SIZE_SIZE);
  let bytes = new Uint8Array(buffer);

  let writeIndex = 0;
  function writeUint8(value) {
    bytes[writeIndex] = value;
    ++writeIndex;
  }

  function writeInt32(value) {
    writeUint8(value & 0xff);
    writeUint8((value >> 8) & 0xff);
    writeUint8((value >> 16) & 0xff);
    writeUint8((value >> 24) & 0xff);
  }

  function writeParcelSize(value) {
    writeUint8((value >> 24) & 0xff);
    writeUint8((value >> 16) & 0xff);
    writeUint8((value >> 8) & 0xff);
    writeUint8(value & 0xff);
  }

  if (fakeParcelSize < 0) {
    fakeParcelSize = realParcelSize;
  }
  writeParcelSize(fakeParcelSize);

  writeInt32(response);
  writeInt32(request);

  // write parcel data
  for (let ii = 0; ii < data.length; ++ii) {
    writeUint8(data[ii]);
  }

  return bytes;
}

/**
 * Create a parcel buffer which represents the hex string.
 *
 * @param hexString
 *        The HEX string to be converted.
 *
 * @return an Uint8Array carrying all parcel data.
 */
function hexStringToParcelByteArrayData(hexString) {
  let length = Math.ceil((hexString.length / 2));
  let bytes = new Uint8Array(4 + length);

  bytes[0] = length & 0xFF;
  bytes[1] = (length >>  8) & 0xFF;
  bytes[2] = (length >> 16) & 0xFF;
  bytes[3] = (length >> 24) & 0xFF;

  for (let i = 0; i < length; i ++) {
    bytes[i + 4] = Number.parseInt(hexString.substr(i * 2, 2), 16);
  }

  return bytes;
}
