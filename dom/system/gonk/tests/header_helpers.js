/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;


let subscriptLoader = Cc["@mozilla.org/moz/jssubscript-loader;1"]
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
    importScripts: function fakeImportScripts() {
      Array.slice(arguments).forEach(function (script) {
        subscriptLoader.loadSubScript("resource://gre/modules/" + script, this);
      }, this);
    },

    postRILMessage: function fakePostRILMessage(message) {
    },

    postMessage: function fakepostMessage(message) {
    },

    // Define these variables inside the worker scope so ES5 strict mode
    // doesn't flip out.
    onmessage: undefined,
    onerror: undefined,

    DEBUG: true
  };
  // The 'self' variable in a worker points to the worker's own namespace.
  worker_ns.self = worker_ns;

  // systemlibs.js utilizes ctypes for loading native libraries.
  Cu.import("resource://gre/modules/ctypes.jsm", worker_ns);

  // Copy the custom definitions over.
  for (let key in custom_ns) {
    worker_ns[key] = custom_ns[key];
  }

  // Load the RIL worker itself.
  worker_ns.importScripts("ril_worker.js");

  return worker_ns;
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

  function writeUint32(value) {
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

  writeUint32(response);
  writeUint32(request);

  // write parcel data
  for (let ii = 0; ii < data.length; ++ii) {
    writeUint8(data[ii]);
  }

  return bytes;
}

/**
 *
 */
function newRadioInterfaceLayer() {
  let ril_ns = {
    ChromeWorker: function ChromeWorker() {
      // Stub function
    },
  };

  subscriptLoader.loadSubScript("resource://gre/components/RadioInterfaceLayer.js", ril_ns);
  return new ril_ns.RadioInterfaceLayer();
}

/**
 * Test whether specified function throws exception with expected
 * result.
 *
 * @param func
 *        Function to be tested.
 * @param result
 *        Expected result. <code>null</code> for no throws.
 * @param stack
 *        Optional stack object to be printed. <code>null</code> for
 *        Components#stack#caller.
 */
function do_check_throws(func, result, stack)
{
  if (!stack)
    stack = Components.stack.caller;

  try {
    func();
  } catch (exc) {
    if (exc.result == result)
      return;
    do_throw("expected result " + result + ", caught " + exc, stack);
  }

  if (result) {
    do_throw("expected result " + result + ", none thrown", stack);
  }
}

