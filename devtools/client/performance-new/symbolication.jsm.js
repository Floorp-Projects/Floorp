/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
// @ts-check
"use strict";

const { createLazyLoaders } = ChromeUtils.import(
  "resource://devtools/client/performance-new/typescript-lazy-load.jsm.js"
);

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

const lazy = createLazyLoaders({
  OS: () => ChromeUtils.import("resource://gre/modules/osfile.jsm"),
});

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

// Generated from https://github.com/mstange/profiler-get-symbols/commit/90ee39f1d18d2727f07dc57bd93cff6bc73ce8a0
const WASM_MODULE_URL =
  "https://zealous-rosalind-a98ce8.netlify.com/wasm/8f7ca2f70e1cd21b5a2dbe96545672752887bfbd4e7b3b9437e9fc7c3da0a3bedae4584ff734f0c9f08c642e6b66ffab.wasm";
const WASM_MODULE_INTEGRITY =
  "sha384-j3yi9w4c0htaLb6WVFZydSiHv71OezuUN+n8fD2go77a5FhP9zTwyfCMZC5rZv+r";

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
 * @param {string} path
 * @returns {Promise<boolean>}
 */
async function doesFileExistAtPath(path) {
  const { OS } = lazy.OS();
  try {
    const result = await OS.File.stat(path);
    return !result.isDir;
  } catch (e) {
    if (e instanceof OS.File.Error && e.becauseNoSuchFile) {
      return false;
    }
    throw e;
  }
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
    // First, enumerate the local paths at which we could find binaries (and, on
    // Windows, PDB files) which could contain symbol information.
    const candidatePaths = this._getCandidatePaths(debugName, breakpadId);

    // Iterate over all the paths and try to get symbols from each entry.
    const module = await getWASMProfilerGetSymbolsModule();
    const errors = [];
    for (const { path, debugPath } of candidatePaths) {
      if (await doesFileExistAtPath(path)) {
        try {
          /** @type {SymbolicationWorkerInitialMessage} */
          const initialMessage = {
            binaryPath: path,
            debugPath,
            breakpadId,
            module,
          };
          return await getResultFromWorker(
            "resource://devtools/client/performance-new/symbolication-worker.js",
            initialMessage
          );
        } catch (e) {
          // getSymbolTable was unsuccessful. So either the
          // file wasn't parseable or its contents didn't match the specified
          // breakpadId, or some other error occurred.
          // Advance to the next candidate path.
          errors.push(e);
        }
      }
    }

    throw new Error(
      `Could not obtain symbols for the library ${debugName} ${breakpadId} ` +
        `because there was no matching file at any of the candidate paths: ${JSON.stringify(
          candidatePaths
        )}. Errors: ${errors.map(e => e.message).join(", ")}`
    );
  }

  /**
   * Enumerate all paths at which we could find files with symbol information.
   *
   * @param {string} debugName
   * @param {string} breakpadId
   * @returns {Array<{ path: string, debugPath: string }>}
   */
  _getCandidatePaths(debugName, breakpadId) {
    const key = `${debugName}:${breakpadId}`;
    const lib = this._libInfoMap.get(key);
    if (!lib) {
      throw new Error(
        `Could not find the library for "${debugName}", "${breakpadId}".`
      );
    }

    const { name, path, debugPath } = lib;
    const { OS } = lazy.OS();
    const candidatePaths = [];

    // First, try to find a binary with a matching file name and breakpadId
    // in one of the manually specified objdirs.
    // This is needed if the debuggee is a build running on a remote machine that
    // was compiled by the developer on *this* machine (the "host machine"). In
    // that case, the objdir will contain the compiled binary with full symbol and
    // debug information, whereas the binary on the device may not exist in
    // uncompressed form or may have been stripped of debug information and some
    // symbol information.
    // An objdir, or "object directory", is a directory on the host machine that's
    // used to store build artifacts ("object files") from the compilation process.
    // This only works if the binary is one of the Gecko binaries and not
    // a system library.
    for (const objdirPath of this._objdirs) {
      // Binaries are usually expected to exist at objdir/dist/bin/filename.
      candidatePaths.push({
        path: OS.Path.join(objdirPath, "dist", "bin", name),
        debugPath: OS.Path.join(objdirPath, "dist", "bin", name),
      });
      // Also search in the "objdir" directory itself (not just in dist/bin).
      // If, for some unforeseen reason, the relevant binary is not inside the
      // objdirs dist/bin/ directory, this provides a way out because it lets the
      // user specify the actual location.
      candidatePaths.push({
        path: OS.Path.join(objdirPath, name),
        debugPath: OS.Path.join(objdirPath, name),
      });
    }

    // Check the absolute paths of the library file(s) last.
    // We do this after the objdir search because the library's path may point
    // to a stripped binary, which will have fewer symbols than the original
    // binaries in the objdir.
    candidatePaths.push({ path, debugPath });

    return candidatePaths;
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
