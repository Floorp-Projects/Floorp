/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
// @ts-check
"use strict";

/**
 * @typedef {import("./@types/perf").Library} Library
 * @typedef {import("./@types/perf").PerfFront} PerfFront
 * @typedef {import("./@types/perf").SymbolTableAsTuple} SymbolTableAsTuple
 * @typedef {import("./@types/perf").SymbolicationService} SymbolicationService
 * @typedef {import("./@types/perf").SymbolicationWorkerInitialMessage} SymbolicationWorkerInitialMessage
 */

/**
 * @template R
 * @typedef {import("./@types/perf").SymbolicationWorkerReplyData<R>} SymbolicationWorkerReplyData<R>
 */

ChromeUtils.defineModuleGetter(
  this,
  "setTimeout",
  "resource://gre/modules/Timer.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "clearTimeout",
  "resource://gre/modules/Timer.jsm"
);

/** @type {any} */
const global = this;

// This module obtains symbol tables for binaries.
// It does so with the help of a WASM module which gets pulled in from the
// internet on demand. We're doing this purely for the purposes of saving on
// code size. The contents of the WASM module are expected to be static, they
// are checked against the hash specified below.
// The WASM code is run on a ChromeWorker thread. It takes the raw byte
// contents of the to-be-dumped binary (and of an additional optional pdb file
// on Windows) as its input, and returns a set of typed arrays which make up
// the symbol table.

// Don't let the strange looking URLs and strings below scare you.
// The hash check ensures that the contents of the wasm module are what we
// expect them to be.
// The source code is at https://github.com/mstange/profiler-get-symbols/ .
// Documentation is at https://docs.rs/profiler-get-symbols/ .
// The sha384 sum can be computed with the following command (tested on macOS):
// shasum -b -a 384 profiler_get_symbols_wasm_bg.wasm | awk '{ print $1 }' | xxd -r -p | base64

// Generated from https://github.com/mstange/profiler-get-symbols/commit/c1dca28a2a506df40f0a6f32c12ba51ec54b02be
const WASM_MODULE_URL =
  "https://storage.googleapis.com/firefox-profiler-get-symbols/c1dca28a2a506df40f0a6f32c12ba51ec54b02be.wasm";
const WASM_MODULE_INTEGRITY =
  "sha384-ZWi2jwcKJr20rE2gmHjFQGHgsCF9CagkyPLsrIZfmf5QKD2oXgkLa8tMKHK6zPA1";

const EXPIRY_TIME_IN_MS = 5 * 60 * 1000; // 5 minutes

/** @type {Promise<WebAssembly.Module> | null} */
let gCachedWASMModulePromise = null;
let gCachedWASMModuleExpiryTimer = 0;

// Keep active workers alive (see bug 1592227).
const gActiveWorkers = new Set();

function clearCachedWASMModule() {
  gCachedWASMModulePromise = null;
  gCachedWASMModuleExpiryTimer = 0;
}

function getWASMProfilerGetSymbolsModule() {
  if (!gCachedWASMModulePromise) {
    gCachedWASMModulePromise = (async function() {
      const request = new Request(WASM_MODULE_URL, {
        integrity: WASM_MODULE_INTEGRITY,
        credentials: "omit",
      });
      return WebAssembly.compileStreaming(fetch(request));
    })();
  }

  // Reset expiry timer.
  clearTimeout(gCachedWASMModuleExpiryTimer);
  gCachedWASMModuleExpiryTimer = setTimeout(
    clearCachedWASMModule,
    EXPIRY_TIME_IN_MS
  );

  return gCachedWASMModulePromise;
}

/**
 * Handle the entire life cycle of a worker, and report its result.
 * This method creates a new worker, sends the initial message to it, handles
 * any errors, and accepts the result.
 * Returns a promise that resolves with the contents of the (singular) result
 * message or rejects with an error.
 *
 * @template M
 * @template R
 * @param {string} workerURL
 * @param {M} initialMessageToWorker
 * @returns {Promise<R>}
 */
async function getResultFromWorker(workerURL, initialMessageToWorker) {
  return new Promise((resolve, reject) => {
    const worker = new ChromeWorker(workerURL);
    gActiveWorkers.add(worker);

    /** @param {MessageEvent<SymbolicationWorkerReplyData<R>>} msg */
    worker.onmessage = msg => {
      gActiveWorkers.delete(worker);
      if ("error" in msg.data) {
        const error = msg.data.error;
        if (error.name) {
          // Turn the JSON error object into a real Error object.
          const { name, message, fileName, lineNumber } = error;
          const ErrorObjConstructor =
            name in global && Error.isPrototypeOf(global[name])
              ? global[name]
              : Error;
          const e = new ErrorObjConstructor(message, fileName, lineNumber);
          e.name = name;
          reject(e);
        } else {
          reject(error);
        }
        return;
      }
      resolve(msg.data.result);
    };

    // Handle uncaught errors from the worker script. onerror is called if
    // there's a syntax error in the worker script, for example, or when an
    // unhandled exception is thrown, but not for unhandled promise
    // rejections. Without this handler, mistakes during development such as
    // syntax errors can be hard to track down.
    worker.onerror = errorEvent => {
      gActiveWorkers.delete(worker);
      worker.terminate();
      if (errorEvent instanceof ErrorEvent) {
        const { message, filename, lineno } = errorEvent;
        const error = new Error(`${message} at ${filename}:${lineno}`);
        error.name = "WorkerError";
        reject(error);
      } else {
        reject(new Error("Error in worker"));
      }
    };

    // Handle errors from messages that cannot be deserialized. I'm not sure
    // how to get into such a state, but having this handler seems like a good
    // idea.
    worker.onmessageerror = errorEvent => {
      gActiveWorkers.delete(worker);
      worker.terminate();
      if (errorEvent instanceof ErrorEvent) {
        const { message, filename, lineno } = errorEvent;
        const error = new Error(`${message} at ${filename}:${lineno}`);
        error.name = "WorkerMessageError";
        reject(error);
      } else {
        reject(new Error("Error in worker"));
      }
    };

    worker.postMessage(initialMessageToWorker);
  });
}

/**
 * @param {PerfFront} perfFront
 * @param {string} path
 * @param {string} breakpadId
 * @returns {Promise<SymbolTableAsTuple>}
 */
async function getSymbolTableFromDebuggee(perfFront, path, breakpadId) {
  const [addresses, index, buffer] = await perfFront.getSymbolTable(
    path,
    breakpadId
  );
  // The protocol transmits these arrays as plain JavaScript arrays of
  // numbers, but we want to pass them on as typed arrays. Convert them now.
  return [
    new Uint32Array(addresses),
    new Uint32Array(index),
    new Uint8Array(buffer),
  ];
}

/**
 * Profiling through the DevTools remote debugging protocol supports multiple
 * different modes. This class is specialized to handle various profiling
 * modes such as:
 *
 *   1) Profiling the same browser on the same machine.
 *   2) Profiling a remote browser on the same machine.
 *   3) Profiling a remote browser on a different device.
 *
 * It's also built to handle symbolication requests for both Gecko libraries and
 * system libraries. However, it only handles cases where symbol information
 * can be found in a local file on this machine. There is one case that is not
 * covered by that restriction: Android system libraries. That case requires
 * the help of the perf actor and is implemented in
 * LocalSymbolicationServiceWithRemoteSymbolTableFallback.
 */
class LocalSymbolicationService {
  /**
   * @param {Library[]} sharedLibraries - Information about the shared libraries.
   *   This allows mapping (debugName, breakpadId) pairs to the absolute path of
   *   the binary and/or PDB file, and it ensures that these absolute paths come
   *   from a trusted source and not from the profiler UI.
   * @param {string[]} objdirs - An array of objdir paths
   *   on the host machine that should be searched for relevant build artifacts.
   */
  constructor(sharedLibraries, objdirs) {
    this._libInfoMap = new Map(
      sharedLibraries.map(lib => {
        const { debugName, breakpadId } = lib;
        const key = `${debugName}:${breakpadId}`;
        return [key, lib];
      })
    );
    this._objdirs = objdirs;
  }

  /**
   * @param {string} debugName
   * @param {string} breakpadId
   * @returns {Promise<SymbolTableAsTuple>}
   */
  async getSymbolTable(debugName, breakpadId) {
    const module = await getWASMProfilerGetSymbolsModule();
    /** @type {SymbolicationWorkerInitialMessage} */
    const initialMessage = {
      request: {
        type: "GET_SYMBOL_TABLE",
        debugName,
        breakpadId,
      },
      libInfoMap: this._libInfoMap,
      objdirs: this._objdirs,
      module,
    };
    return getResultFromWorker(
      "resource://devtools/client/performance-new/symbolication-worker.js",
      initialMessage
    );
  }

  /**
   * @param {string} path
   * @param {string} requestJson
   * @returns {Promise<string>}
   */
  async querySymbolicationApi(path, requestJson) {
    const module = await getWASMProfilerGetSymbolsModule();
    /** @type {SymbolicationWorkerInitialMessage} */
    const initialMessage = {
      request: {
        type: "QUERY_SYMBOLICATION_API",
        path,
        requestJson,
      },
      libInfoMap: this._libInfoMap,
      objdirs: this._objdirs,
      module,
    };
    return getResultFromWorker(
      "resource://devtools/client/performance-new/symbolication-worker.js",
      initialMessage
    );
  }
}

/**
 * An implementation of the SymbolicationService interface which also
 * covers the Android system library case.
 * We first try to get symbols from the wrapped SymbolicationService.
 * If that fails, we try to get the symbol table through the perf actor.
 */
class LocalSymbolicationServiceWithRemoteSymbolTableFallback {
  /**
   * @param {SymbolicationService} symbolicationService - The regular symbolication service.
   * @param {Library[]} sharedLibraries - Information about the shared libraries
   * @param {PerfFront} perfFront - A perf actor, to obtain symbol
   *   tables from remote targets
   */
  constructor(symbolicationService, sharedLibraries, perfFront) {
    this._symbolicationService = symbolicationService;
    this._libs = sharedLibraries;
    this._perfFront = perfFront;
  }

  /**
   * @param {string} debugName
   * @param {string} breakpadId
   * @returns {Promise<SymbolTableAsTuple>}
   */
  async getSymbolTable(debugName, breakpadId) {
    try {
      return await this._symbolicationService.getSymbolTable(
        debugName,
        breakpadId
      );
    } catch (errorFromLocalFiles) {
      // Try to obtain the symbol table on the debuggee. We get into this
      // branch in the following cases:
      //  - Android system libraries
      //  - Firefox binaries that have no matching equivalent on the host
      //    machine, for example because the user didn't point us at the
      //    corresponding objdir, or if the build was compiled somewhere
      //    else, or if the build on the device is outdated.
      // For now, the "debuggee" is never a Windows machine, which is why we don't
      // need to pass the library's debugPath. (path and debugPath are always the
      // same on non-Windows.)
      const lib = this._libs.find(
        l => l.debugName === debugName && l.breakpadId === breakpadId
      );
      if (!lib) {
        throw new Error(
          `Could not find the library for "${debugName}", "${breakpadId}" after falling ` +
            `back to remote symbol table querying because regular getSymbolTable failed ` +
            `with error: ${errorFromLocalFiles.message}.`
        );
      }
      return getSymbolTableFromDebuggee(this._perfFront, lib.path, breakpadId);
    }
  }

  /**
   * @param {string} path
   * @param {string} requestJson
   * @returns {Promise<string>}
   */
  async querySymbolicationApi(path, requestJson) {
    return this._symbolicationService.querySymbolicationApi(path, requestJson);
  }
}

/**
 * Return an object that implements the SymbolicationService interface.
 *
 * @param {Library[]} sharedLibraries - Information about the shared libraries
 * @param {string[]} objdirs - An array of objdir paths
 *   on the host machine that should be searched for relevant build artifacts.
 * @param {PerfFront} [perfFront] - An optional perf actor, to obtain symbol
 *   tables from remote targets
 * @return {SymbolicationService}
 */
function createLocalSymbolicationService(sharedLibraries, objdirs, perfFront) {
  const service = new LocalSymbolicationService(sharedLibraries, objdirs);
  if (perfFront) {
    return new LocalSymbolicationServiceWithRemoteSymbolTableFallback(
      service,
      sharedLibraries,
      perfFront
    );
  }
  return service;
}

// Provide an exports object for the JSM to be properly read by TypeScript.
/** @type {any} */ (this).module = {};

module.exports = {
  createLocalSymbolicationService,
};

// Object.keys() confuses the linting which expects a static array expression.
// eslint-disable-next-line
var EXPORTED_SYMBOLS = Object.keys(module.exports);
