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
 * Retrieve a symbol table from a binary on the host machine, by looking up
 * relevant build artifacts in the specified objdirs.
 * This is needed if the debuggee is a build running on a remote machine that
 * was compiled by the developer on *this* machine (the "host machine"). In
 * that case, the objdir will contain the compiled binary with full symbol and
 * debug information, whereas the binary on the device may not exist in
 * uncompressed form or may have been stripped of debug information and some
 * symbol information.
 * An objdir, or "object directory", is a directory on the host machine that's
 * used to store build artifacts ("object files") from the compilation process.
 *
 * @param {string[]} objdirs An array of objdir paths on the host machine
 *   that should be searched for relevant build artifacts.
 * @param {string} filename The file name of the binary.
 * @param {string} breakpadId The breakpad ID of the binary.
 * @returns {Promise<SymbolTableAsTuple>} The symbol table of the first encountered binary with a
 *   matching breakpad ID, in SymbolTableAsTuple format. An exception is thrown (the
 *   promise is rejected) if nothing was found.
 */
async function getSymbolTableFromLocalBinary(objdirs, filename, breakpadId) {
  const { OS } = lazy.OS();
  const candidatePaths = [];
  for (const objdirPath of objdirs) {
    // Binaries are usually expected to exist at objdir/dist/bin/filename.
    candidatePaths.push(OS.Path.join(objdirPath, "dist", "bin", filename));
    // Also search in the "objdir" directory itself (not just in dist/bin).
    // If, for some unforeseen reason, the relevant binary is not inside the
    // objdirs dist/bin/ directory, this provides a way out because it lets the
    // user specify the actual location.
    candidatePaths.push(OS.Path.join(objdirPath, filename));
  }

  for (const path of candidatePaths) {
    if (await doesFileExistAtPath(path)) {
      const { ProfilerGetSymbols } = lazy.ProfilerGetSymbols();
      try {
        return await ProfilerGetSymbols.getSymbolTable(path, path, breakpadId);
      } catch (e) {
        // ProfilerGetSymbols.getSymbolTable was unsuccessful. So either the
        // file wasn't parseable or its contents didn't match the specified
        // breakpadId, or some other error occurred.
        // Advance to the next candidate path.
      }
    }
  }
  throw new Error("Could not find any matching binary.");
}

/**
 * Profiling through the DevTools remote debugging protocol supports multiple
 * different modes. This function is specialized to handle various profiling
 * modes such as:
 *
 *   1) Profiling the same browser on the same machine.
 *   2) Profiling a remote browser on the same machine.
 *   3) Profiling a remote browser on a different device.
 *
 * It's also built to handle symbolication requests for both Gecko libraries and
 * system libraries.
 *
 * @param {Library} lib - The library to get symbols for.
 * @param {string[]} objdirs - An array of objdir paths on the host machine that
 *   should be searched for relevant build artifacts.
 * @param {PerfFront | undefined} perfFront - The perfFront for a remote debugging
 *   connection, or undefined when profiling this browser.
 * @return {Promise<SymbolTableAsTuple>}
 */
async function getSymbolTableMultiModal(lib, objdirs, perfFront = undefined) {
  const { name, debugName, path, debugPath, breakpadId } = lib;
  try {
    // First, try to find a binary with a matching file name and breakpadId
    // in one of the manually specified objdirs. If the profiled build was
    // compiled locally, and matches the build artifacts in the objdir, then
    // this gives the best results because the objdir build always has full
    // symbol information.
    // This only works if the binary is one of the Gecko binaries and not
    // a system library.
    return await getSymbolTableFromLocalBinary(objdirs, name, breakpadId);
  } catch (errorWhenCheckingObjdirs) {
    // Couldn't find a matching build in one of the objdirs. Search elsewhere.
    if (await doesFileExistAtPath(path)) {
      const { ProfilerGetSymbols } = lazy.ProfilerGetSymbols();
      // This profile was probably obtained from this machine, and not from a
      // different device (e.g. an Android phone). Dump symbols from the file
      // on this machine directly.
      return ProfilerGetSymbols.getSymbolTable(path, debugPath, breakpadId);
    }
    // No file exists at the path on this machine, which probably indicates
    // that the profile was obtained on a different machine, i.e. the debuggee
    // is truly remote (e.g. on an Android phone).
    if (!perfFront) {
      // No remote connection - we really needed the file at path.
      throw new Error(
        `Could not obtain symbols for the library ${debugName} ${breakpadId} ` +
          `because there was no file at the given path "${path}". Furthermore, ` +
          `looking for symbols in the given objdirs failed: ${errorWhenCheckingObjdirs.message}`
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
    return getSymbolTableFromDebuggee(perfFront, path, breakpadId);
  }
}

// Provide an exports object for the JSM to be properly read by TypeScript.
/** @type {any} */ (this).module = {};

module.exports = {
  getSymbolTableFromDebuggee,
  getSymbolTableFromLocalBinary,
  getSymbolTableMultiModal,
};

// Object.keys() confuses the linting which expects a static array expression.
// eslint-disable-next-line
var EXPORTED_SYMBOLS = Object.keys(module.exports);
