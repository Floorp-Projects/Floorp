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
 */

const lazy = createLazyLoaders({
  OS: () => ChromeUtils.import("resource://gre/modules/osfile.jsm"),
  ProfilerGetSymbols: () =>
    ChromeUtils.import("resource://gre/modules/ProfilerGetSymbols.jsm"),
});

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
 * Enumerate all paths at which we could find files with symbol information.
 *
 * @param {string[]} objdirs An array of objdir paths on the host machine
 *   that should be searched for relevant build artifacts.
 * @param {Library} lib Information about the library.
 * @returns {Array<{ path: string, debugPath: string }>}
 */
function getCandidatePaths(objdirs, lib) {
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
  for (const objdirPath of objdirs) {
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

/**
 * Returns a function getLibrary(debugName, breakpadId) => Library which
 * resolves a (debugName, breakpadId) pair to the library's information, which
 * contains the absolute paths on the file system where the binary and its
 * optional pdb file are stored.
 *
 * This is needed for the following reason:
 *  - In order to obtain a symbol table for a system library, we need to know
 *    the library's absolute path on the file system. On Windows, we
 *    additionally need to know the absolute path to the library's PDB file,
 *    which we call the binary's "debugPath".
 *  - Symbol tables are requested asynchronously, by the profiler UI, after the
 *    profile itself has been obtained.
 *  - When the symbol tables are requested, we don't want the profiler UI to
 *    pass us arbitrary absolute file paths, as an extra defense against
 *    potential information leaks.
 *  - Instead, when the UI requests symbol tables, it identifies the library
 *    with a (debugName, breakpadId) pair. We need to map that pair back to the
 *    absolute paths.
 *  - We get the "trusted" paths from the "libs" sections of the profile. We
 *    trust these paths because we just obtained the profile directly from
 *    Gecko.
 *  - This function builds the (debugName, breakpadId) => Library mapping and
 *    retains it on the returned closure so that it can be consulted after the
 *    profile has been passed to the UI.
 *
 * @param {Library[]} sharedLibraries
 * @returns {(debugName: string, breakpadId: string) => Library | undefined}
 */
function createLibraryMap(sharedLibraries) {
  const map = new Map(
    sharedLibraries.map(lib => {
      const { debugName, breakpadId } = lib;
      const key = [debugName, breakpadId].join(":");
      return [key, lib];
    })
  );

  return function getLibraryFor(debugName, breakpadId) {
    const key = [debugName, breakpadId].join(":");
    return map.get(key);
  };
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
 * system libraries.
 */
class LocalSymbolicationService {
  /**
   * @param {Library[]} sharedLibraries - Information about the shared libraries
   * @param {string[]} objdirs - An array of objdir paths
   *   on the host machine that should be searched for relevant build artifacts.
   * @param {PerfFront} [perfFront] - An optional perf actor, to obtain symbol
   *   tables from remote targets
   */
  constructor(sharedLibraries, objdirs, perfFront) {
    this._libraryGetter = createLibraryMap(sharedLibraries);
    this._objdirs = objdirs;
    this._perfFront = perfFront;
  }

  /**
   * @param {string} debugName
   * @param {string} breakpadId
   * @returns {Promise<SymbolTableAsTuple>}
   */
  async getSymbolTable(debugName, breakpadId) {
    const lib = this._libraryGetter(debugName, breakpadId);
    if (!lib) {
      throw new Error(
        `Could not find the library for "${debugName}", "${breakpadId}".`
      );
    }

    // First, enumerate the local paths at which we could find binaries (and, on
    // Windows, PDB files) which could contain symbol information.
    const candidatePaths = getCandidatePaths(this._objdirs, lib);

    // Iterate over all the paths and try to get symbols from each entry.
    const { ProfilerGetSymbols } = lazy.ProfilerGetSymbols();
    for (const { path, debugPath } of candidatePaths) {
      if (await doesFileExistAtPath(path)) {
        try {
          return await ProfilerGetSymbols.getSymbolTable(
            path,
            debugPath,
            breakpadId
          );
        } catch (e) {
          // ProfilerGetSymbols.getSymbolTable was unsuccessful. So either the
          // file wasn't parseable or its contents didn't match the specified
          // breakpadId, or some other error occurred.
          // Advance to the next candidate path.
        }
      }
    }

    // No local file was found to obtain symbols from.
    // This can happen if the  debuggee is truly remote (e.g. on an Android phone).
    if (!this._perfFront) {
      throw new Error(
        `Could not obtain symbols for the library ${debugName} ${breakpadId} ` +
          `because there was no matching file at any of the candidate paths: ${JSON.stringify(
            candidatePaths
          )}`
      );
    }

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
    return getSymbolTableFromDebuggee(this._perfFront, lib.path, breakpadId);
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
  return new LocalSymbolicationService(sharedLibraries, objdirs, perfFront);
}

// Provide an exports object for the JSM to be properly read by TypeScript.
/** @type {any} */ (this).module = {};

module.exports = {
  createLocalSymbolicationService,
};

// Object.keys() confuses the linting which expects a static array expression.
// eslint-disable-next-line
var EXPORTED_SYMBOLS = Object.keys(module.exports);
